#import "HCview.h"
#import "hc_core.h"
#import <objc/runtime.h>   // pour associer un bitmap à un Object
#import "icons.h"
#import "HCglobals.h"
#import "HCpalettes.h"
#import "HCicons.h"
#import "graphics.h"
#import "HCdialogs.h"
static NSTextField *gMsgBox = nil; // la message box

static NSPoint gDragStart;
static NSRect  gDragRect;
static BOOL    gDragging = NO;
static int     gNewCount = 0;
static Object *gEditTarget = NULL;
static NSTextView *gEditView = nil;
static NSPanel *gEditPanel = nil;
static Object *gPressed = NULL;
static BOOL    gMoving = NO;        // on déplace l'objet sélectionné
static NSPoint gMoveStart;         // point de départ du déplacement
static int     gObjStartX, gObjStartY;  // position de l'objet au départ
static int     gResizeHandle = 0;  // 0 = pas de resize, 1..4 = coin saisi
static int     gObjStartW, gObjStartH;
static NSTextView *gFieldEditor = nil;
static Object     *gEditingField = NULL;
static NSPoint gPenLast;
static BOOL    gPenDrawing = NO;
static BOOL gEditBackground = NO;   // NO = couche carte, YES = couche fond
// peint un segment de ligne dans le bitmap de la carte courante
static NSPoint gShapeStart;
static NSPoint gShapeEnd;
static BOOL    gShapeDrawing = NO;
 
 
static NSPoint gLassoPts[4096];
static int gLassoCount = 0;
static BOOL gLassoDrawing = NO;
static BOOL gLassoActive = NO;   // une sélection existe-t-elle ?


static NSPoint gSelStart, gSelEnd; // selection rectabgle
static BOOL gSelRectDrawing = NO;
static BOOL gSelRectActive = NO;

static NSPanel *gPatternPanel = nil;
static NSPanel *gWidthPanel = nil;
static NSPanel *gBrushPanel = nil;

BOOL gTransparentBg = NO;

static BOOL gTextActive = NO;
static NSPoint gTextPos;                 // coin haut-gauche de la saisie
static NSMutableString *gTextBuf = nil;

int gTextSize = 16;
static NSColor *gTextColor = nil;
static BOOL gTextUnderline = NO;

static Object *gPopupTarget = NULL;

 #define NUM_PATTERNS 38

// #define NUM_PATTERNS (int)(sizeof(PATTERNS)/sizeof(PATTERNS[0]))


 // int gPattern = 2;   // motif courant (1 = noir plein)
static NSPoint gFreePts[4096];
static int gFreeCount = 0;
static BOOL gFreeDrawing = NO;




static BOOL gFloating = NO;        // un collage flotte-t-il ?
static NSPoint gFloatPos;          // position (coin haut-gauche) du flottant
static BOOL gFloatDragging = NO;   // en train de le déplacer ?
static NSPoint gFloatGrab;         // décalage entre le clic et le coin
static NSFont *gTextFont = nil;
//static NSTextField *gInfoTextSize = nil;



//static NSPanel   *gIconPanel = nil;
//static IconGrid  *gIconGrid = nil;
//static NSTextField *gIconLabel = nil;



// récupère (ou crée) le bitmap de peinture d'une carte/fond
static NSMutableDictionary *gPaintCache = nil;  // clé = pointeur objet, valeur = NSBitmapImageRep

// éteint tous les radioButtons de la carte sauf 'keep'
static void radio_exclusive(Object *card, Object *keep) {
    if (!card) return;
    for (int i = 0; i < card->nparts; i++) {
        Object *o = card->parts[i];
        if (o->type == OBJ_BUTTON && o != keep && o->style &&
            (strcmp(o->style, "radioButton") == 0 || strcmp(o->style, "radiobutton") == 0))
            o->hilite = 0;
    }
    if (card->bg)
        for (int i = 0; i < card->bg->nparts; i++) {
            Object *o = card->bg->parts[i];
            if (o->type == OBJ_BUTTON && o != keep && o->style &&
                (strcmp(o->style, "radioButton") == 0 || strcmp(o->style, "radiobutton") == 0))
                o->hilite = 0;
        }
}


// dessine un objet (bouton ou champ) à son rectangle
/* ---- dessine le nom d'un bouton, centre dans le rect ---- */
static void draw_btn_label(Object *o, NSString *s, NSRect r, BOOL on, CGFloat defSize) {
    if (!o->showname) return;
    CGFloat fs = o->textsize > 0 ? o->textsize : defSize;
    NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
    [ps setAlignment:NSTextAlignmentCenter];
    NSDictionary *attrs = @{
        NSFontAttributeName: [NSFont boldSystemFontOfSize:fs],
        NSForegroundColorAttributeName: (on ? [NSColor whiteColor] : [NSColor blackColor]),
        NSParagraphStyleAttributeName: ps
    };
    NSRect tr = NSInsetRect(r, 4, 0);
    tr.origin.y += (r.size.height - fs * 1.2) / 2;
    [s drawInRect:tr withAttributes:attrs];
}

/* ---- contour pointille montre en mode edition ---- */
static void draw_edit_outline(NSRect r) {
    if (gTool != TOOL_BUTTON && gTool != TOOL_FIELD) return;
    [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
    NSBezierPath *outline = [NSBezierPath bezierPathWithRect:r];
    [outline setLineWidth:1];
    CGFloat dash[] = {3, 2};
    [outline setLineDash:dash count:2 phase:0];
    [outline stroke];
}
/* ---- dessine le fond et le cadre d'un bouton selon son style ---- */
static void draw_btn_frame(Object *o, NSRect r, BOOL on) {
    const char *st = o->style ? o->style : "rectangle";

    if (strcmp(st, "transparent") == 0) {
        if (on) {
            [[NSColor colorWithWhite:0.0 alpha:0.15] setFill];
            NSRectFill(r);
        }
        return;
    }
    if (strcmp(st, "shadow") == 0) {
        NSRect body = NSMakeRect(r.origin.x, r.origin.y,
                                 r.size.width - 3, r.size.height - 3);
        NSRect sh   = NSMakeRect(r.origin.x + 3, r.origin.y + 3,
                                 r.size.width - 3, r.size.height - 3);
        [[NSColor blackColor] setFill];
        NSRectFill(sh);
        [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
        NSRectFill(body);
        [[NSColor blackColor] setStroke];
        NSBezierPath *bp = [NSBezierPath bezierPathWithRect:NSInsetRect(body, 0.5, 0.5)];
        [bp setLineWidth:1];
        [bp stroke];
        return;
    }
    if (strcmp(st, "roundRect") == 0 || strcmp(st, "roundrect") == 0) {
        NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 0.5, 0.5)
                                                          xRadius:8 yRadius:8];
        [(on ? [NSColor blackColor] : [NSColor colorWithWhite:0.9 alpha:1.0]) setFill];
        [p fill];
        [[NSColor blackColor] setStroke];
        [p setLineWidth:1];
        [p stroke];
        return;
    }
    if (strcmp(st, "oval") == 0) {
        NSBezierPath *p = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(r, 0.5, 0.5)];
        [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
        [p fill];
        [[NSColor blackColor] setStroke];
        [p setLineWidth:1];
        [p stroke];
        return;
    }
    if (strcmp(st, "standard") == 0 || strcmp(st, "default") == 0) {
        NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 2.5, 2.5)
                                                          xRadius:6 yRadius:6];
        [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
        [p fill];
        [[NSColor blackColor] setStroke];
        [p setLineWidth:1];
        [p stroke];
        if (strcmp(st, "default") == 0) {
            NSBezierPath *o2 = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 1.5, 1.5)
                                                               xRadius:9 yRadius:9];
            [o2 setLineWidth:3];
            [o2 stroke];
        }
        return;
    }
    if (strcmp(st, "opaque") == 0) {
        [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
        NSRectFill(r);
        return;
    }
    // rectangle par defaut
    [(on ? [NSColor blackColor] : [NSColor colorWithWhite:0.9 alpha:1.0]) setFill];
    NSRectFill(r);
    [[NSColor blackColor] setStroke];
    NSFrameRect(r);
}
static void draw_part(Object *o) {
    if (!o->visible) return;

    NSRect r = NSMakeRect(o->x, o->y, o->w, o->h);

    if (o->type == OBJ_BUTTON) {
        const char *st = o->style ? o->style : "rectangle";
        BOOL isCheck   = (strcmp(st, "checkBox") == 0    || strcmp(st, "checkbox") == 0);
        BOOL isRadio   = (strcmp(st, "radioButton") == 0 || strcmp(st, "radiobutton") == 0);
        BOOL isTransp  = (strcmp(st, "transparent") == 0);
        BOOL isRound   = (strcmp(st, "roundRect") == 0   || strcmp(st, "roundrect") == 0);
        BOOL isOpaque  = (strcmp(st, "opaque") == 0);
        BOOL isShadow  = (strcmp(st, "shadow") == 0);
        BOOL isPopup   = (strcmp(st, "popup") == 0);
        BOOL isStd     = (strcmp(st, "standard") == 0);
        BOOL isDefault = (strcmp(st, "default") == 0);
        BOOL isOval    = (strcmp(st, "oval") == 0);

        const char *nm = o->name ? o->name : "";
        NSString *s = [NSString stringWithUTF8String:nm];
        BOOL on = o->hilite;

        const HCIcon *ic = (o->icon ? hcicon_find(o->icon) : NULL);


        /* ---------- bouton a icone : habillage du style + icone par-dessus ---------- */
                if (ic) {
                    CGFloat iy = o->y + 2;
                    NSRect ir = NSMakeRect(floor(o->x + (o->w - 32)/2.0), floor(iy), 32, 32);

                    draw_btn_frame(o, r, on);

                    [(on ? [NSColor whiteColor] : [NSColor blackColor]) setFill];
                    hcicon_draw(ic, ir, 1.0);
                    draw_edit_outline(r);

                    if (o->showname && o->h > 36) {
                        CGFloat fs = o->textsize > 0 ? o->textsize : 11;
                        NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                        [ps setAlignment:NSTextAlignmentCenter];
                        NSDictionary *attrs = @{
                            NSFontAttributeName: [NSFont systemFontOfSize:fs],
                            NSForegroundColorAttributeName: (on ? [NSColor whiteColor] : [NSColor blackColor]),
                            NSParagraphStyleAttributeName: ps
                        };
                        NSRect tr = NSMakeRect(o->x, iy + 34, o->w, o->h - 36);
                        [s drawInRect:tr withAttributes:attrs];
                    }
                }
        /* ---------- case a cocher / bouton radio ---------- */
        else if (isCheck || isRadio) {
            CGFloat box = 14;
            CGFloat cy = o->y + o->h/2.0 - box/2.0;
            NSRect mark = NSMakeRect(o->x + 2, cy, box, box);

            [[NSColor whiteColor] setFill];
            [[NSColor blackColor] setStroke];

            if (isRadio) {
                NSBezierPath *circle = [NSBezierPath bezierPathWithOvalInRect:mark];
                [circle fill];
                [circle stroke];
                if (on) {
                    NSRect dot = NSInsetRect(mark, 4, 4);
                    [[NSColor blackColor] setFill];
                    [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
                }
            } else {
                [[NSColor whiteColor] setFill];
                NSRectFill(mark);
                [[NSColor blackColor] setStroke];
                NSBezierPath *bp = [NSBezierPath bezierPathWithRect:mark];
                [bp setLineWidth:1];
                [bp stroke];
                if (on) {
                    NSBezierPath *x = [NSBezierPath bezierPath];
                    [x moveToPoint:NSMakePoint(mark.origin.x+2, mark.origin.y+2)];
                    [x lineToPoint:NSMakePoint(mark.origin.x+box-2, mark.origin.y+box-2)];
                    [x moveToPoint:NSMakePoint(mark.origin.x+box-2, mark.origin.y+2)];
                    [x lineToPoint:NSMakePoint(mark.origin.x+2, mark.origin.y+box-2)];
                    [x setLineWidth:1.5];
                    [x stroke];
                }
            }

            if (o->showname) {
                CGFloat fs = o->textsize > 0 ? o->textsize : 13;
                NSDictionary *attrs = @{ NSFontAttributeName: [NSFont systemFontOfSize:fs] };
                [s drawAtPoint:NSMakePoint(o->x + box + 8, o->y + o->h/2 - fs*0.6)
                withAttributes:attrs];
            }
        }
        /* ---------- transparent ---------- */
        else if (isTransp) {
            if (on) {
                [[NSColor colorWithWhite:0.0 alpha:0.15] setFill];
                NSRectFill(r);
            }
            draw_edit_outline(r);
            if (o->showname) {
                CGFloat fs = o->textsize > 0 ? o->textsize : 16;
                NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                [ps setAlignment:NSTextAlignmentCenter];
                NSDictionary *attrs = @{
                    NSFontAttributeName: [NSFont boldSystemFontOfSize:fs],
                    NSForegroundColorAttributeName: [NSColor blackColor],
                    NSParagraphStyleAttributeName: ps
                };
                NSRect tr = NSInsetRect(r, 2, 0);
                tr.origin.y += (r.size.height - fs * 1.2) / 2;
                [s drawInRect:tr withAttributes:attrs];
            }
        }
        /* ---------- popup : cadre + chevron + ligne courante ---------- */
        /* ---------- popup : rectangle ombre + chevron + ligne courante ---------- */
                else if (isPopup) {
                    NSRect body = NSMakeRect(r.origin.x, r.origin.y,
                                             r.size.width - 3, r.size.height - 3);
                    NSRect sh   = NSMakeRect(r.origin.x + 3, r.origin.y + 3,
                                             r.size.width - 3, r.size.height - 3);

                    [[NSColor blackColor] setFill];
                    NSRectFill(sh);
                    [[NSColor whiteColor] setFill];
                    NSRectFill(body);
                    [[NSColor blackColor] setStroke];
                    NSBezierPath *bp = [NSBezierPath bezierPathWithRect:NSInsetRect(body, 0.5, 0.5)];
                    [bp setLineWidth:1];
                    [bp stroke];

                    // chevron de deroulement, a droite
                    CGFloat cx = body.origin.x + body.size.width - 12;
                    CGFloat cy = body.origin.y + body.size.height/2.0;
                    [[NSColor blackColor] setFill];
                    NSBezierPath *ar = [NSBezierPath bezierPath];
                    [ar moveToPoint:NSMakePoint(cx - 4, cy - 2)];
                    [ar lineToPoint:NSMakePoint(cx + 4, cy - 2)];
                    [ar lineToPoint:NSMakePoint(cx,     cy + 3)];
                    [ar closePath];
                    [ar fill];

                    // la ligne choisie, ou le nom du bouton a defaut
                    NSString *label = s;
                    if (o->contents && *o->contents) {
                        NSArray *lines = [[NSString stringWithUTF8String:o->contents]
                                          componentsSeparatedByString:@"\n"];
                        int sel = o->selectedline > 0 ? o->selectedline : 1;
                        if (sel <= (int)[lines count] && [lines[sel-1] length] > 0)
                            label = lines[sel-1];
                    }
                    CGFloat fs = o->textsize > 0 ? o->textsize : 12;
                    NSDictionary *attrs = @{ NSFontAttributeName: [NSFont systemFontOfSize:fs] };
                    [label drawAtPoint:NSMakePoint(body.origin.x + 6,
                                                   body.origin.y + (body.size.height - fs*1.3)/2)
                        withAttributes:attrs];
                }
        /* ---------- standard / default : boutons systeme ---------- */
        else if (isStd || isDefault) {
            NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 2.5, 2.5)
                                                              xRadius:6 yRadius:6];
            [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
            [p fill];
            [[NSColor blackColor] setStroke];
            [p setLineWidth:1];
            [p stroke];
            if (isDefault) {
                NSBezierPath *o2 = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 1.5, 1.5)
                                                                   xRadius:9 yRadius:9];
                [o2 setLineWidth:3];
                [o2 stroke];
            }
            draw_btn_label(o, s, r, on, 13);
        }
        /* ---------- oval ---------- */
        else if (isOval) {
            NSBezierPath *p = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(r, 0.5, 0.5)];
            [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
            [p fill];
            [[NSColor blackColor] setStroke];
            [p setLineWidth:1];
            [p stroke];
            draw_btn_label(o, s, r, on, 13);
        }
        /* ---------- shadow ---------- */
        else if (isShadow) {
            NSRect body = NSMakeRect(r.origin.x, r.origin.y,
                                     r.size.width - 3, r.size.height - 3);
            NSRect sh   = NSMakeRect(r.origin.x + 3, r.origin.y + 3,
                                     r.size.width - 3, r.size.height - 3);
            [[NSColor blackColor] setFill];
            NSRectFill(sh);
            [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
            NSRectFill(body);
            [[NSColor blackColor] setStroke];
            NSBezierPath *bp = [NSBezierPath bezierPathWithRect:NSInsetRect(body, 0.5, 0.5)];
            [bp setLineWidth:1];
            [bp stroke];
            draw_btn_label(o, s, body, on, 13);
        }
        /* ---------- roundRect ---------- */
        else if (isRound) {
            NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 0.5, 0.5)
                                                              xRadius:8 yRadius:8];
            [(on ? [NSColor blackColor] : [NSColor colorWithWhite:0.9 alpha:1.0]) setFill];
            [p fill];
            [[NSColor blackColor] setStroke];
            [p setLineWidth:1];
            [p stroke];
            draw_btn_label(o, s, r, on, 13);
        }
        /* ---------- opaque : fond uni, pas de cadre ---------- */
        else if (isOpaque) {
            [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
            NSRectFill(r);
            draw_btn_label(o, s, r, on, 13);
        }
        /* ---------- rectangle (defaut) ---------- */
        else {
            [(on ? [NSColor blackColor] : [NSColor colorWithWhite:0.9 alpha:1.0]) setFill];
            NSRectFill(r);
            [[NSColor blackColor] setStroke];
            NSFrameRect(r);
            draw_btn_label(o, s, r, on, 13);
        }
    }
    else if (o->type == OBJ_FIELD) {
        [[NSColor colorWithWhite:0.97 alpha:1.0] setFill];
        NSRectFill(r);
        [[NSColor colorWithWhite:0.4 alpha:1.0] setStroke];
        NSFrameRect(r);
        const char *tx = o->contents ? o->contents : "";
        NSString *s = [NSString stringWithUTF8String:tx];
        [s drawInRect:NSInsetRect(r, 4, 4) withAttributes:nil];
    }
}
static int handle_at(Object *o, NSPoint p) {
    if (!o) return 0;
    CGFloat s = 8;  // tolérance de clic
    NSPoint corners[4] = {
        {o->x, o->y},
        {o->x + o->w, o->y},
        {o->x, o->y + o->h},
        {o->x + o->w, o->y + o->h}
    };
    for (int i = 0; i < 4; i++)
        if (fabs(p.x - corners[i].x) <= s && fabs(p.y - corners[i].y) <= s)
            return i + 1;
    return 0;
}
// trouve la part (bouton/champ) contenant le point, carte puis fond
// trouve la part contenant le point.
// En navigation (browse), cherche partout (carte + fond).
// En édition, ne cherche que dans la couche active.
static Object *part_at(Object *card, NSPoint p) {
    if (!card) return NULL;

    // en mode édition, restreindre à la couche active
    if (gTool != TOOL_BROWSE) {
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) return NULL;
        for (int i = layer->nparts - 1; i >= 0; i--) {
            Object *o = layer->parts[i];
            if (o->visible &&
                p.x >= o->x && p.x <= o->x + o->w &&
                p.y >= o->y && p.y <= o->y + o->h)
                return o;
        }
        return NULL;
    }

    // en navigation : carte d'abord, puis fond
    for (int i = card->nparts - 1; i >= 0; i--) {
        Object *o = card->parts[i];
        if (o->visible &&
            p.x >= o->x && p.x <= o->x + o->w &&
            p.y >= o->y && p.y <= o->y + o->h)
            return o;
    }
    if (card->bg)
        for (int i = card->bg->nparts - 1; i >= 0; i--) {
            Object *o = card->bg->parts[i];
            if (o->visible &&
                p.x >= o->x && p.x <= o->x + o->w &&
                p.y >= o->y && p.y <= o->y + o->h)
                return o;
        }
    return NULL;
}

// reçoit toute sortie du noyau
static void cocoa_line(HcLineKind kind, int depth, const char *text) {
    (void)depth;
    if (kind == HC_MSG && gMsgBox) {
        [gMsgBox setStringValue:[NSString stringWithUTF8String:text]];
    } else {
        NSLog(@"%s", text);
    }
}

static void cocoa_field_changed(Object *field) {
    (void)field;
    [gView setNeedsDisplay:YES];
}
static NSFont *text_font(void) {
    if (!gTextFont) {
        gTextFont = [NSFont fontWithName:@"Helvetica" size:gTextSize];
        if (!gTextFont) gTextFont = [NSFont systemFontOfSize:gTextSize];
    }
    return gTextFont;
}

static NSDictionary *text_attrs(void) {
    NSColor *c = gTextColor;
    if (!c) c = (gInk == INK_WHITE) ? [NSColor whiteColor] : [NSColor blackColor];
    NSMutableDictionary *a = [NSMutableDictionary dictionary];
    a[NSFontAttributeName] = text_font();
    a[NSForegroundColorAttributeName] = c;
    if (gTextUnderline) a[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);
    return a;
}
// grave le texte dans le bitmap a la position donnee
static void stamp_text(NSBitmapImageRep *rep, NSString *s, NSPoint pos) {
    if (!rep || [s length] == 0) return;
    NSGraphicsContext *base = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!base) return;
    CGContextRef cg = [base CGContext];
    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithCGContext:cg flipped:YES];

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:NO];

    CGFloat H = [rep pixelsHigh];
    CGContextTranslateCTM(cg, 0, H);
    CGContextScaleCTM(cg, 1, -1);

    [s drawAtPoint:pos withAttributes:text_attrs()];

    [NSGraphicsContext restoreGraphicsState];
}


// ==================== palette d'épaisseur de trait (vue custom) ====================




// ==================== palette d'outils custom (grille + sélection encadrée) ====================
typedef struct { const char *glyph; int kind; int value; } ToolCell;


@implementation HCView

// static NSPanel     *gStackPanel = nil;
//static Object      *gStackTarget = NULL;
//static NSTextField *gStackName = nil;

- (void)updateWindowTitle {
    Object *card = hc_current_card();
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;
    const char *nm = stack->name ? stack->name : "Sans titre";
    [[self window] setTitle:[NSString stringWithUTF8String:nm]];
}
- (void)newBackground:(id)sender {
    Object *card = hc_current_card();
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;

    char name[64];
    static int bgCount = 0;
    snprintf(name, sizeof name, "Fond %d", ++bgCount);
    Object *bg = hc_new_background(stack, name);
    Object *nc = hc_new_card(stack, bg, "");
    hc_set_current_card(nc);
    gSelected = NULL;
    [self setNeedsDisplay:YES];
}




  
- (BOOL)acceptsFirstResponder { return YES; }

- (void)showPopupMenuFor:(Object *)o atPoint:(NSPoint)p {
    if (!o->contents || !*o->contents) return;
    NSArray *lines = [[NSString stringWithUTF8String:o->contents]
                      componentsSeparatedByString:@"\n"];

    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    for (NSUInteger i = 0; i < [lines count]; i++) {
        NSString *t = lines[i];
        if ([t length] == 0) continue;
        NSMenuItem *it = [[NSMenuItem alloc] initWithTitle:t
                                                    action:@selector(popupChosen:)
                                             keyEquivalent:@""];
        [it setTarget:self];
        [it setTag:(NSInteger)(i + 1)];        // numero de ligne, base 1
        if ((int)(i + 1) == o->selectedline) [it setState:NSControlStateValueOn];
        [menu addItem:it];
    }
    gPopupTarget = o;
    NSPoint origin = NSMakePoint(o->x, o->y + o->h);
    [menu popUpMenuPositioningItem:nil
                        atLocation:origin
                            inView:self];
}

- (void)popupChosen:(id)sender {
    if (gPopupTarget) {
        gPopupTarget->selectedline = (int)[sender tag];
        hc_send(gPopupTarget, "mouseUp");     // le script reagit au choix
    }
    gPopupTarget = NULL;
    [self setNeedsDisplay:YES];
}








 

 
 

- (void)copy:(id)sender {
    Object *card = hc_current_card();
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (gTool == TOOL_SELRECT && gSelRectActive)
        copy_rect(rep, gSelStart, gSelEnd);
    else if (gTool == TOOL_LASSO && gLassoActive)
        copy_freeform(rep, gLassoPts, gLassoCount);
}

- (void)cut:(id)sender {
    Object *card = hc_current_card();
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (gTool == TOOL_SELRECT && gSelRectActive) {
        copy_rect(rep, gSelStart, gSelEnd);
        erase_rect(rep, gSelStart, gSelEnd);
        gSelRectActive = NO;
    } else if (gTool == TOOL_LASSO && gLassoActive) {
        copy_freeform(rep, gLassoPts, gLassoCount);
        erase_freeform(rep, gLassoPts, gLassoCount);
        gLassoActive = NO;
        gLassoCount = 0;
    }
    [self setNeedsDisplay:YES];
}

- (void)paste:(id)sender {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSArray *imgs = [pb readObjectsForClasses:@[[NSImage class]] options:nil];
    if (imgs.count > 0) {
        NSData *tiff = [imgs[0] TIFFRepresentation];
        NSBitmapImageRep *ext = [NSBitmapImageRep imageRepWithData:tiff];
        if (ext) {
            gClipboard = ext;
            gClipW = (int)[ext pixelsWide];
            gClipH = (int)[ext pixelsHigh];
            gClipPtsCount = 0;
        }
    }
    if (gClipboard) {
        gFloating = YES;
        NSRect b = [self bounds];
        gFloatPos = NSMakePoint((b.size.width - gClipW)/2, (b.size.height - gClipH)/2);
        [self setNeedsDisplay:YES];
    }
}
- (BOOL)validateMenuItem:(NSMenuItem *)item {
    SEL a = [item action];
    if (a == @selector(copy:) || a == @selector(cut:))
        return (gTool == TOOL_SELRECT && gSelRectActive) ||
               (gTool == TOOL_LASSO && gLassoActive);
    if (a == @selector(paste:))
        return gClipboard != nil ||
               [[NSPasteboard generalPasteboard] canReadObjectForClasses:@[[NSImage class]] options:nil];
    return YES;
}
- (void)setFrameSize:(NSSize)newSize {
    [self flushPaintToKernel];   // encoder les dessins avant que le cache ne soit invalide
    [super setFrameSize:newSize];
    [self clearPaintCache];
    [self setNeedsDisplay:YES];
}
- (void)changeColor:(id)sender {
    gTextColor = [sender color];
    [self setNeedsDisplay:YES];
}
- (void)underline:(id)sender {
    gTextUnderline = !gTextUnderline;
    [self setNeedsDisplay:YES];
}
- (void)commitText {
    if (!gTextActive) return;
    if ([gTextBuf length] > 0) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        if (layer) {
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                        (int)[self bounds].size.height);
            stamp_text(rep, gTextBuf, gTextPos);
        }
    }
    gTextActive = NO;
    gTextBuf = nil;
    [self setNeedsDisplay:YES];
}
- (void)changeFont:(id)sender {
    gTextFont = [sender convertFont:text_font()];
    gTextSize = (int)[gTextFont pointSize];
    [[NSFontManager sharedFontManager] setSelectedFont:gTextFont isMultiple:NO];
    [self setNeedsDisplay:YES];
}
// gras / italique / souligne passent par la
- (void)changeAttributes:(id)sender {
    [self setNeedsDisplay:YES];
}

- (NSFontPanelModeMask)validModesForFontPanel:(NSFontPanel *)fontPanel {
    return NSFontPanelModesMaskStandardModes;
}
- (void)ditherSelection:(id)sender {
    // collage flottant : tramer l'image qui flotte
    if (gFloating && gClipboard) {
        dither_region(gClipboard, 0, 0, gClipW-1, gClipH-1, NULL, 0);
        [self setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    // selection rectangulaire
    if (gSelRectActive) {
        dither_region(rep, (int)MIN(gSelStart.x,gSelEnd.x), (int)MIN(gSelStart.y,gSelEnd.y),
                           (int)MAX(gSelStart.x,gSelEnd.x), (int)MAX(gSelStart.y,gSelEnd.y),
                           NULL, 0);
        [self setNeedsDisplay:YES];
        return;
    }
    // selection lasso
    if (gLassoActive && gLassoCount >= 3) {
        double minx=gLassoPts[0].x, maxx=minx, miny=gLassoPts[0].y, maxy=miny;
        for (int i=1;i<gLassoCount;i++){
            if(gLassoPts[i].x<minx)minx=gLassoPts[i].x;
            if(gLassoPts[i].x>maxx)maxx=gLassoPts[i].x;
            if(gLassoPts[i].y<miny)miny=gLassoPts[i].y;
            if(gLassoPts[i].y>maxy)maxy=gLassoPts[i].y;
        }
        dither_region(rep, (int)floor(minx), (int)floor(miny),
                           (int)ceil(maxx),  (int)ceil(maxy),
                           gLassoPts, gLassoCount);
        [self setNeedsDisplay:YES];
        return;
    }
    // rien de selectionne : toute la couche
    dither_region(rep, 0, 0, (int)[rep pixelsWide]-1, (int)[rep pixelsHigh]-1, NULL, 0);
    [self setNeedsDisplay:YES];
}
- (void)keyDown:(NSEvent *)event {
    unichar key = [[event charactersIgnoringModifiers] characterAtIndex:0];
    NSUInteger mods = [event modifierFlags];
    BOOL cmd = (mods & NSEventModifierFlagCommand) != 0;

    // saisie de texte en cours : tout va au tampon
    if (gTool == TOOL_TEXT && gTextActive && !cmd) {
        if (key == 27) {                          // Echap : annuler
            gTextActive = NO;
            [self setNeedsDisplay:YES];
            return;
        }
        if (key == NSDeleteCharacter || key == NSBackspaceCharacter) {
            NSUInteger n = [gTextBuf length];
            if (n > 0) [gTextBuf deleteCharactersInRange:NSMakeRange(n-1, 1)];
            [self setNeedsDisplay:YES];
            return;
        }
        NSString *chars = [event characters];
        if ([chars length] > 0) {
            if ([chars isEqualToString:@"\r"]) chars = @"\n";
            [gTextBuf appendString:chars];
            [self setNeedsDisplay:YES];
        }
        return;
    }

    // Delete : lasso actif -> effacer la zone
    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gTool == TOOL_LASSO && gLassoActive) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                    (int)[self bounds].size.height);
        erase_freeform(rep, gLassoPts, gLassoCount);
        gLassoActive = NO;
        gLassoCount = 0;
        [self setNeedsDisplay:YES];
        return;
    }

    // Delete : selection rectangulaire -> effacer la zone
    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gTool == TOOL_SELRECT && gSelRectActive) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                    (int)[self bounds].size.height);
        erase_rect(rep, gSelStart, gSelEnd);
        gSelRectActive = NO;
        [self setNeedsDisplay:YES];
        return;
    }

    // Delete : objet selectionne -> le supprimer
    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gSelected && gTool != TOOL_BROWSE) {
        hc_delete_part(gSelected);
        gSelected = NULL;
        [self setNeedsDisplay:YES];
        return;
    }

    [super keyDown:event];
}
- (void)inkChosen:(id)sender {
    gInk = (HCInk)[sender tag];
    NSLog(@"encre : %d", (int)gInk);
}
- (BOOL)isFlipped { return YES; }
- (void)applyStackSize {
    // 1. D'ABORD encoder les dessins actuels (avant tout redimensionnement)
    [self flushPaintToKernel];

    // 2. Trouver la pile et sa taille
    Object *card = hc_current_card();
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;
    int w = stack->w > 0 ? stack->w : 512;
    int h = stack->h > 0 ? stack->h : 342;

    // 3. Vider le cache (les bitmaps seront recréés à la nouvelle taille, en rechargeant le PNG)
    [self clearPaintCache];

    // 4. Redimensionner la fenêtre
    NSWindow *win = [self window];
    if (!win) return;
    NSRect frame = [win frame];
    NSRect content = NSMakeRect(0, 0, w, h);
    NSRect newFrame = [win frameRectForContentRect:content];
    newFrame.origin = frame.origin;
    newFrame.origin.y = frame.origin.y + frame.size.height - newFrame.size.height;
    [win setFrame:newFrame display:YES animate:NO];
    [self updateWindowTitle];
    [self setNeedsDisplay:YES];
}
- (void)installWidthPalette {
    int cols = 4, rows = 3;   // 11 valeurs sur 3 rangées
    CGFloat cell = 40, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;
    gWidthPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(200, 150, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gWidthPanel setTitle:@"Épaisseur"];
    [gWidthPanel setFloatingPanel:YES];
    [gWidthPanel setReleasedWhenClosed:NO];
    WidthPalette *grid = [[WidthPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gWidthPanel setContentView:grid];
    [gWidthPanel makeKeyAndOrderFront:nil];
}
- (void)installBrushPalette {
    int cols = 4, rows = (NUM_BRUSHES + cols - 1) / cols;
    CGFloat cell = 34, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;
    gBrushPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(250, 300, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gBrushPanel setTitle:@"Pinceaux"];
    [gBrushPanel setFloatingPanel:YES];
    [gBrushPanel setReleasedWhenClosed:NO];
    BrushPalette *grid = [[BrushPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gBrushPanel setContentView:grid];
    [gBrushPanel makeKeyAndOrderFront:nil];
}

- (void)showBrushPalette {
    if (!gBrushPanel) [self installBrushPalette];
    else [gBrushPanel makeKeyAndOrderFront:nil];
}
- (void)widthChosen:(id)sender {
    gLineWidth = (int)[sender tag];
}
- (void)showPatternPalette {
   if (!gPatternPanel) [self installPatternPalette];
   else [gPatternPanel makeKeyAndOrderFront:nil];
    [gPatternPanel setReleasedWhenClosed:NO];
}
- (void)showWidthPalette {
    NSLog(@"showWidthPalette: gWidthPanel=%@", gWidthPanel);
   if (!gWidthPanel) [self installWidthPalette];
   else [gWidthPanel makeKeyAndOrderFront:nil];
    [gWidthPanel setReleasedWhenClosed:NO];
}
- (void)installPatternPalette {
    int cols = 4, rows = (NUM_PATTERNS + cols - 1) / cols;
    CGFloat cell = 32, gap = 4, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;
    gPatternPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(560, 200, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [gPatternPanel setTitle:@"Motifs"];
    [gPatternPanel setFloatingPanel:YES];
    PatternPalette *grid = [[PatternPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gPatternPanel setContentView:grid];
    [gPatternPanel makeKeyAndOrderFront:nil];
}
 

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor whiteColor] setFill];
    NSRectFill(dirtyRect);

    Object *card = hc_current_card();
    if (!card) return;

    NSRect b = [self bounds];

    // ===== empilement façon HyperCard, de bas en haut =====

    // 1. peinture du fond
    if (card->bg) {
        NSBitmapImageRep *bgpaint = paint_bitmap(card->bg, (int)b.size.width, (int)b.size.height);
        [bgpaint drawInRect:NSMakeRect(0, 0, [bgpaint pixelsWide], [bgpaint pixelsHigh])
                   fromRect:NSZeroRect
                  operation:NSCompositingOperationSourceOver fraction:1.0
             respectFlipped:YES hints:nil];
    }

    // 2. objets du fond
    if (card->bg)
        for (int i = 0; i < card->bg->nparts; i++)
            draw_part(card->bg->parts[i]);

    // 3. peinture de la carte (PAR-DESSUS les objets du fond)
    NSBitmapImageRep *cardpaint = paint_bitmap(card, (int)b.size.width, (int)b.size.height);
    [cardpaint drawInRect:NSMakeRect(0, 0, [cardpaint pixelsWide], [cardpaint pixelsHigh])
                 fromRect:NSZeroRect
                operation:NSCompositingOperationSourceOver fraction:1.0
           respectFlipped:YES hints:nil];

    // 4. objets de la carte (au-dessus de tout)
    for (int i = 0; i < card->nparts; i++)
        draw_part(card->parts[i]);

    // ===== surcouches d'édition (toujours au-dessus) =====

    // sélection
    if (gSelected) {
        NSRect r = NSMakeRect(gSelected->x, gSelected->y, gSelected->w, gSelected->h);
        [[NSColor redColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:NSInsetRect(r, -2, -2)];
        [path setLineWidth:2];
        [path stroke];
        [[NSColor redColor] setFill];
        CGFloat s = 6;
        NSPoint corners[4] = {
            {r.origin.x, r.origin.y},
            {r.origin.x + r.size.width, r.origin.y},
            {r.origin.x, r.origin.y + r.size.height},
            {r.origin.x + r.size.width, r.origin.y + r.size.height}
        };
        for (int i = 0; i < 4; i++)
            NSRectFill(NSMakeRect(corners[i].x - s/2, corners[i].y - s/2, s, s));
    }

    // aperçu de création d'objet (drag rectangle)
    if (gDragging) {
        [[NSColor blueColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:gDragRect];
        [path setLineWidth:1];
        CGFloat dash[] = {4, 3};
        [path setLineDash:dash count:2 phase:0];
        [path stroke];
    }

    // cadre marron : mode édition du fond
    if (gEditBackground) {
        [[NSColor colorWithRed:0.6 green:0.4 blue:0.2 alpha:1.0] setStroke];
        NSBezierPath *frame = [NSBezierPath bezierPathWithRect:NSInsetRect(b, 4, 4)];
        [frame setLineWidth:4];
        CGFloat dash[] = {10, 5};
        [frame setLineDash:dash count:2 phase:0];
        [frame stroke];
    }

    // aperçu élastique des formes (ligne / rect / ovale)
    if (gShapeDrawing) {
        [[NSColor blueColor] setStroke];
        NSBezierPath *preview = [NSBezierPath bezierPath];
        NSRect box = NSMakeRect(MIN(gShapeStart.x,gShapeEnd.x), MIN(gShapeStart.y,gShapeEnd.y),
                                fabs(gShapeEnd.x-gShapeStart.x), fabs(gShapeEnd.y-gShapeStart.y));
        if (gTool == TOOL_LINE) {
            [preview moveToPoint:gShapeStart];
            [preview lineToPoint:gShapeEnd];
        } else if (gTool == TOOL_RECT) {
            preview = [NSBezierPath bezierPathWithRect:box];
        } else if (gTool == TOOL_OVAL) {
            preview = [NSBezierPath bezierPathWithOvalInRect:box];
        }
        [preview setLineWidth:1];
        [preview stroke];
    }
    if (gFreeDrawing && gFreeCount > 1) {
            [[NSColor blueColor] setStroke];
            NSBezierPath *pv = [NSBezierPath bezierPath];
            [pv moveToPoint:gFreePts[0]];
            for (int i = 1; i < gFreeCount; i++) [pv lineToPoint:gFreePts[i]];
            [pv setLineWidth:1];
            [pv stroke];
        }
    if ((gLassoDrawing || gLassoActive) && gLassoCount > 1) {
            [[NSColor blackColor] setStroke];
            NSBezierPath *pv = [NSBezierPath bezierPath];
            [pv moveToPoint:gLassoPts[0]];
            for (int i = 1; i < gLassoCount; i++) [pv lineToPoint:gLassoPts[i]];
            if (gLassoActive) [pv closePath];
            [pv setLineWidth:1];
            CGFloat dash[] = {4, 3};
            [pv setLineDash:dash count:2 phase:0];
            [pv stroke];
        }
    if (gSelRectDrawing || gSelRectActive) {
            NSRect sel = NSMakeRect(MIN(gSelStart.x,gSelEnd.x), MIN(gSelStart.y,gSelEnd.y),
                                    fabs(gSelEnd.x-gSelStart.x), fabs(gSelEnd.y-gSelStart.y));
            [[NSColor blackColor] setStroke];
            NSBezierPath *pv = [NSBezierPath bezierPathWithRect:sel];
            [pv setLineWidth:1];
            CGFloat dash[] = {4, 3};
            [pv setLineDash:dash count:2 phase:0];
            [pv stroke];
        }
    if (gFloating && gClipboard) {
            NSRect fr = NSMakeRect(gFloatPos.x, gFloatPos.y, gClipW, gClipH);
            [gClipboard drawInRect:fr fromRect:NSZeroRect
                         operation:NSCompositingOperationSourceOver fraction:1.0
                    respectFlipped:YES hints:nil];

            [[NSColor blackColor] setStroke];
            NSBezierPath *fp = [NSBezierPath bezierPath];
            if (gClipPtsCount >= 3) {
                [fp moveToPoint:NSMakePoint(gFloatPos.x + gClipPts[0].x,
                                            gFloatPos.y + gClipPts[0].y)];
                for (int i = 1; i < gClipPtsCount; i++)
                    [fp lineToPoint:NSMakePoint(gFloatPos.x + gClipPts[i].x,
                                                gFloatPos.y + gClipPts[i].y)];
                [fp closePath];
            } else {
                fp = [NSBezierPath bezierPathWithRect:fr];
            }
            [fp setLineWidth:1];
            CGFloat dash[] = {4, 3};
            [fp setLineDash:dash count:2 phase:0];
            [fp stroke];
        }
    if (gTextActive && gTextBuf) {
            NSDictionary *at = text_attrs();
            [gTextBuf drawAtPoint:gTextPos withAttributes:at];

            NSArray *lines = [gTextBuf componentsSeparatedByString:@"\n"];
            NSString *last = [lines lastObject];
            NSSize lastSz = [last sizeWithAttributes:at];
            // hauteur d'une ligne telle que la mesure le moteur de rendu
            NSSize oneLine = [@"Ag" sizeWithAttributes:at];
            CGFloat cy = gTextPos.y + ([lines count] - 1) * oneLine.height;

            [[NSColor blackColor] setStroke];
            NSBezierPath *caret = [NSBezierPath bezierPath];
            [caret moveToPoint:NSMakePoint(gTextPos.x + lastSz.width, cy)];
            [caret lineToPoint:NSMakePoint(gTextPos.x + lastSz.width, cy + oneLine.height)];
            [caret setLineWidth:1];
            [caret stroke];
        }
}
- (void)toggleBackground:(id)sender {
    gEditBackground = !gEditBackground;
    gSelected = NULL;
    [self endFieldEdit];
    [self setNeedsDisplay:YES];
    NSLog(@"couche : %@", gEditBackground ? @"FOND" : @"CARTE");
}
- (void)testScribble {
    Object *card = hc_current_card();
    if (!card) return;
    NSBitmapImageRep *rep = paint_bitmap(card, 500, 400);

    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:NO];
    [[NSColor blackColor] setStroke];
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path moveToPoint:NSMakePoint(50, 50)];
    [path curveToPoint:NSMakePoint(250, 150)
         controlPoint1:NSMakePoint(100, 200)
         controlPoint2:NSMakePoint(200, 20)];
    [path setLineWidth:3];
    [path stroke];

    [NSGraphicsContext restoreGraphicsState];
    [self setNeedsDisplay:YES];
}
- (void)clearPaintCache {
    [gPaintCache removeAllObjects];
}
- (void)flushPaintToKernel {
    if (!gPaintCache) return;
    for (NSValue *key in gPaintCache) {
        Object *o = [key pointerValue];
        NSBitmapImageRep *rep = [gPaintCache objectForKey:key];
        NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        NSString *b64 = [png base64EncodedStringWithOptions:0];
        hc_set_paint(o, [b64 UTF8String]);
    }
}
/* a placer avec les autres globales de HCview.m */
// static Object *gPopupTarget = NULL;

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    Object *hit = part_at(hc_current_card(), p);

    /* ---------- collage flottant : deplacer ou scotcher ---------- */
    if (gFloating) {
        NSRect fr = NSMakeRect(gFloatPos.x, gFloatPos.y, gClipW, gClipH);
        if (NSPointInRect(p, fr)) {
            gFloatDragging = YES;
            gFloatGrab = NSMakePoint(p.x - gFloatPos.x, p.y - gFloatPos.y);
        } else {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                        (int)[self bounds].size.height);
            stamp_clipboard(rep, gFloatPos);
            gFloating = NO;
            [self setNeedsDisplay:YES];
        }
        return;
    }

    /* ---------- outils de trace libre ---------- */
    if (gTool == TOOL_PENCIL || gTool == TOOL_BRUSH || gTool == TOOL_ERASER) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        if (layer) {
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                        (int)[self bounds].size.height);
            gPenLast = p;
            gPenDrawing = YES;
            if (gTool == TOOL_PENCIL)      paint_stroke(rep, p, p, [NSColor blackColor], gLineWidth);
            else if (gTool == TOOL_BRUSH)  brush_stroke(rep, p, p);
            else                           erase_stroke(rep, p, p, 16);
            [self setNeedsDisplay:YES];
        }
        return;
    }

    /* ---------- formes elastiques ---------- */
    if (gTool == TOOL_LINE || gTool == TOOL_RECT || gTool == TOOL_OVAL) {
        gShapeStart = p;
        gShapeEnd = p;
        gShapeDrawing = YES;
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- pot de peinture ---------- */
    if (gTool == TOOL_FILL) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                    (int)[self bounds].size.height);
        flood_fill(rep, (int)p.x, (int)p.y);
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- forme libre ---------- */
    if (gTool == TOOL_FREEFORM) {
        gFreeCount = 0;
        gFreePts[gFreeCount++] = p;
        gFreeDrawing = YES;
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- lasso ---------- */
    if (gTool == TOOL_LASSO) {
        [[self window] makeFirstResponder:self];
        gLassoCount = 0;
        gLassoPts[gLassoCount++] = p;
        gLassoDrawing = YES;
        gLassoActive = NO;
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- selection rectangulaire ---------- */
    if (gTool == TOOL_SELRECT) {
        [[self window] makeFirstResponder:self];
        gSelStart = p; gSelEnd = p;
        gSelRectDrawing = YES;
        gSelRectActive = NO;
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- outil texte ---------- */
    if (gTool == TOOL_TEXT) {
        [self commitText];                 // graver la saisie precedente
        gTextPos = p;
        gTextBuf = [NSMutableString string];
        gTextActive = YES;
        [[self window] makeFirstResponder:self];
        [[NSFontManager sharedFontManager] setSelectedFont:text_font() isMultiple:NO];
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- double-clic en mode edition : Info ou script ---------- */
    if (gTool != TOOL_BROWSE && hit && [event clickCount] == 2) {
        if (hit->type == OBJ_BUTTON) [self showButtonInfo:hit];
        else                         [self editScriptOf:hit];
        return;
    }

    /* ---------- navigation ---------- */
    if (gTool == TOOL_BROWSE) {
        // bouton popup : derouler le menu
        if (hit && hit->type == OBJ_BUTTON && hit->style &&
            strcmp(hit->style, "popup") == 0) {
            [self showPopupMenuFor:hit atPoint:p];
            return;
        }
        if (hit && hit->type == OBJ_FIELD) {
            [self beginFieldEdit:hit];
            return;
        }
        if (hit) {
            gPressed = hit;
            // flash uniquement pour les boutons sans etat persistant
            if (hit->type == OBJ_BUTTON && hit->autohilite &&
                (!hit->style ||
                 (strcmp(hit->style, "checkBox") != 0 && strcmp(hit->style, "checkbox") != 0 &&
                  strcmp(hit->style, "radioButton") != 0 && strcmp(hit->style, "radiobutton") != 0))) {
                hit->hilite = 1;
            }
            hc_send(hit, "mouseDown");
            [self setNeedsDisplay:YES];
        } else {
            [self endFieldEdit];
        }
        return;
    }

    /* ---------- mode bouton / champ ---------- */
    // saisit-on une poignee de l'objet deja selectionne ?
    if (gSelected) {
        int h = handle_at(gSelected, p);
        if (h) {
            gResizeHandle = h;
            gMoveStart = p;
            gObjStartX = gSelected->x;
            gObjStartY = gSelected->y;
            gObjStartW = gSelected->w;
            gObjStartH = gSelected->h;
            gMoving = NO;
            gDragging = NO;
            return;
        }
    }

    if (hit) {
        gSelected = hit;
        gMoving = YES;
        [[self window] makeFirstResponder:self];   // focus clavier pour Delete
        gMoveStart = p;
        gObjStartX = hit->x;
        gObjStartY = hit->y;
        gDragging = NO;
    } else {
        gSelected = NULL;
        gDragStart = p;
        gDragRect = NSMakeRect(p.x, p.y, 0, 0);
        gDragging = YES;
    }
    [self setNeedsDisplay:YES];
}


- (void)mouseDragged:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    if (gFloating && gFloatDragging) {
            gFloatPos = NSMakePoint(p.x - gFloatGrab.x, p.y - gFloatGrab.y);
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_PENCIL && gPenDrawing) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;   // ← couche active
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
        paint_stroke(rep, gPenLast, p, [NSColor blackColor], gLineWidth);            gPenLast = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_BRUSH && gPenDrawing) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            brush_stroke(rep, gPenLast, p);
            gPenLast = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_ERASER && gPenDrawing) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            erase_stroke(rep, gPenLast, p, 16);
            gPenLast = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_FREEFORM && gFreeDrawing) {
            if (gFreeCount < 4096) gFreePts[gFreeCount++] = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_SELRECT && gSelRectDrawing) {
        gSelEnd = p;
        [self setNeedsDisplay:YES];
        return;
    }





    if (gShapeDrawing) {
            gShapeEnd = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gResizeHandle && gSelected) {
        int dx = (int)(p.x - gMoveStart.x);
        int dy = (int)(p.y - gMoveStart.y);
        int x = gObjStartX, y = gObjStartY, w = gObjStartW, h = gObjStartH;
        switch (gResizeHandle) {
            case 1: x += dx; y += dy; w -= dx; h -= dy; break;  // HG
            case 2: y += dy; w += dx; h -= dy; break;            // HD
            case 3: x += dx; w -= dx; h += dy; break;            // BG
            case 4: w += dx; h += dy; break;                     // BD
        }
        if (w < 8) w = 8;
        if (h < 8) h = 8;
        gSelected->x = x; gSelected->y = y;
        gSelected->w = w; gSelected->h = h;
        [self setNeedsDisplay:YES];
        return;
    }
    // mouseDragged
        if (gTool == TOOL_LASSO && gLassoDrawing) {
            if (gLassoCount < 4096) gLassoPts[gLassoCount++] = p;
            [self setNeedsDisplay:YES];
            return;
        }
    // déplacement d'un objet sélectionné
    if (gMoving && gSelected) {
        int dx = (int)(p.x - gMoveStart.x);
        int dy = (int)(p.y - gMoveStart.y);
        gSelected->x = gObjStartX + dx;
        gSelected->y = gObjStartY + dy;
        [self setNeedsDisplay:YES];
        return;
    }

    // création par tracé (inchangé)
    if (!gDragging) return;
    CGFloat x = MIN(gDragStart.x, p.x);
    CGFloat y = MIN(gDragStart.y, p.y);
    CGFloat w = fabs(p.x - gDragStart.x);
    CGFloat h = fabs(p.y - gDragStart.y);
    gDragRect = NSMakeRect(x, y, w, h);
    [self setNeedsDisplay:YES];
}
- (void)mouseUp:(NSEvent *)event {
    // --- mode flèche : envoyer mouseUp au script ---
    if (gFloating) {
            gFloatDragging = NO;
            return;
        }
    if (gResizeHandle) {
            gResizeHandle = 0;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_PENCIL || gTool == TOOL_ERASER) {
            gPenDrawing = NO;
            return;
        }
    if (gTool == TOOL_SELRECT) {
            gSelRectDrawing = NO;
            gSelRectActive = (fabs(gSelEnd.x-gSelStart.x) > 3 && fabs(gSelEnd.y-gSelStart.y) > 3);
            [self setNeedsDisplay:YES];
            return;
        }
    // tool lasso
        if (gTool == TOOL_LASSO) {
            gLassoDrawing = NO;
            gLassoActive = (gLassoCount >= 3);   // sélection valide si au moins un triangle
            [self setNeedsDisplay:YES];
            return;
        }
    if (gShapeDrawing) {
            gShapeDrawing = NO;
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
                    if (gShapeFilled && gTool != TOOL_LINE)
                        fill_shape(rep, gTool, gShapeStart, gShapeEnd);
                    paint_shape(rep, gTool, gShapeStart, gShapeEnd, [NSColor blackColor], gLineWidth);;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_FREEFORM) {
            gFreeDrawing = NO;
            if (gFreeCount >= 2) {
                Object *card = hc_current_card();
                Object *layer = gEditBackground ? card->bg : card;
                if (!layer) layer = card;
                NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
                if (gShapeFilled && gFreeCount >= 3)
                    fill_freeform(rep, gFreePts, gFreeCount);
                paint_freeform(rep, gFreePts, gFreeCount, gLineWidth);
            }
            gFreeCount = 0;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_BROWSE) {
            if (gPressed) {
                NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
                Object *hit = part_at(hc_current_card(), p);
                if (hit == gPressed)
                    hc_send(gPressed, "mouseUp");

                // comportement checkBox / radioButton : bascule persistante
                if (gPressed->type == OBJ_BUTTON && gPressed->style) {
                    const char *st = gPressed->style;
                    if (strcmp(st, "checkBox") == 0 || strcmp(st, "checkbox") == 0) {
                        gPressed->hilite = !gPressed->hilite;   // bascule et reste
                    }
                    else if (strcmp(st, "radioButton") == 0 || strcmp(st, "radiobutton") == 0) {
                        gPressed->hilite = 1;                    // allume
                        radio_exclusive(hc_current_card(), gPressed);  // éteint les autres
                    }
                    else if (gPressed->autohilite) {
                        gPressed->hilite = 0;   // bouton normal : éteindre le flash
                    }
                } else if (gPressed->type == OBJ_BUTTON && gPressed->autohilite) {
                    gPressed->hilite = 0;
                }

                gPressed = NULL;
                [self setNeedsDisplay:YES];
            }
            if (gMoving) {
                gMoving = NO;
                [self setNeedsDisplay:YES];
                return;
            }
            return;
        }

    // --- mode édition : finir la création par drag ---
    if (!gDragging) return;
    gDragging = NO;
    if (gDragRect.size.width < 8 || gDragRect.size.height < 8) {
        [self setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
        if (!card) return;
        Object *owner = gEditBackground ? card->bg : card;   // couche cible
        if (!owner) owner = card;                            // sécurité si pas de fond
        char name[64];
        Object *o;
        if (gTool == TOOL_BUTTON) {
            snprintf(name, sizeof name, "Bouton %d", ++gNewCount);
            o = hc_new_button(owner, name);
        } else {
            snprintf(name, sizeof name, "Champ %d", ++gNewCount);
            o = hc_new_field(owner, name);
        }
    o->x = (int)gDragRect.origin.x;
    o->y = (int)gDragRect.origin.y;
    o->w = (int)gDragRect.size.width;
    o->h = (int)gDragRect.size.height;
    gSelected = o;
    [self setNeedsDisplay:YES];
}
- (void)installMessageBox {
    gView = self;
    NSRect b = [self bounds];
    gMsgBox = [[NSTextField alloc] initWithFrame:NSMakeRect(10, b.size.height - 34, b.size.width - 20, 24)];
    [gMsgBox setEditable:YES];
    [gMsgBox setSelectable:YES];
    [gMsgBox setBezeled:YES];
    [gMsgBox setDrawsBackground:YES];
    [gMsgBox setBackgroundColor:[NSColor colorWithWhite:0.96 alpha:1.0]];
    [gMsgBox setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
    [gMsgBox setStringValue:@""];
    [gMsgBox setTarget:self];
    [gMsgBox setAction:@selector(messageBoxEntered:)];
    [self addSubview:gMsgBox];

    static HcHost host;
    host.line = cocoa_line;
    host.field_changed = cocoa_field_changed;
    hc_set_host(&host);
}

- (void)messageBoxEntered:(id)sender {
    NSString *cmd = [gMsgBox stringValue];
    if ([cmd length] == 0) return;
    hc_do([cmd UTF8String]);
    [self applyStackSize];          // ← applique un éventuel changement de taille
    [self setNeedsDisplay:YES];
    [gMsgBox selectText:nil];
}

- (void)installToolPalette {
    int cols = 4, rows = (NUM_TOOLCELLS + cols - 1) / cols;
    CGFloat cell = 38, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;

    NSPanel *palette = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(560, 350, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow)
                    backing:NSBackingStoreBuffered defer:NO];
    [palette setTitle:@"Outils"];
    [palette setFloatingPanel:YES];

    ToolPalette *grid = [[ToolPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [palette setContentView:grid];
    [palette makeKeyAndOrderFront:nil];
}
- (void)toggleFilled:(id)sender {
    gShapeFilled = !gShapeFilled;
    NSLog(@"formes : %@", gShapeFilled ? @"PLEINES" : @"VIDES");
}
- (void)toolChosen:(id)sender {
    gTool = (HCTool)[sender tag];
    gSelected = NULL;
    [self setNeedsDisplay:YES];
    NSLog(@"outil : %d", (int)gTool);
}
- (void)editScriptOf:(Object *)obj {
    gEditTarget = obj;

    NSPanel *panel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(300, 200, 480, 340)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    char d[64]; hc_describe(obj, d, sizeof d);
    [panel setTitle:[NSString stringWithFormat:@"Script de %s", d]];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(10, 44, 460, 286)];
    [scroll setHasVerticalScroller:YES];
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    NSTextView *tv = [[NSTextView alloc] initWithFrame:[[scroll contentView] bounds]];
    [tv setFont:[NSFont fontWithName:@"Monaco" size:12]];
    [tv setAutoresizingMask:NSViewWidthSizable];
    const char *cur = hc_script_of(obj);
    [tv setString:cur ? [NSString stringWithUTF8String:cur] : @""];
    [scroll setDocumentView:tv];
    [[panel contentView] addSubview:scroll];

    gEditView = tv;

    NSButton *ok = [[NSButton alloc] initWithFrame:NSMakeRect(390, 8, 80, 30)];
    [ok setTitle:@"OK"];
    [ok setBezelStyle:NSBezelStyleRounded];
    [ok setTarget:self];
    [ok setAction:@selector(saveScript:)];
    [ok setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];
    [[panel contentView] addSubview:ok];

    gEditPanel = panel;
    [panel makeKeyAndOrderFront:nil];
}

- (void)saveScript:(id)sender {
    if (gEditTarget && gEditView) {
        hc_set_script(gEditTarget, [[gEditView string] UTF8String]);
    }
    [gEditPanel close];
    gEditPanel = nil; gEditView = nil; gEditTarget = nil;
    [self setNeedsDisplay:YES];
}
- (void)beginFieldEdit:(Object *)field {
    [self endFieldEdit];
    gEditingField = field;
    NSRect r = NSMakeRect(field->x, field->y, field->w, field->h);
    gFieldEditor = [[NSTextView alloc] initWithFrame:NSInsetRect(r, 2, 2)];
    [gFieldEditor setFont:[NSFont systemFontOfSize:13]];
    const char *tx = field->contents ? field->contents : "";
    [gFieldEditor setString:[NSString stringWithUTF8String:tx]];
    [self addSubview:gFieldEditor];
    [[self window] makeFirstResponder:gFieldEditor];
}

- (void)endFieldEdit {
    if (gFieldEditor && gEditingField) {
        hc_set_field_text(gEditingField, [[gFieldEditor string] UTF8String]);
    }
    [gFieldEditor removeFromSuperview];
    gFieldEditor = nil;
    gEditingField = NULL;
    [self setNeedsDisplay:YES];
}
 
@end
