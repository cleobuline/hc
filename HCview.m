#import "HCview.h"
#import "hc_core.h"

typedef enum { TOOL_BROWSE, TOOL_BUTTON, TOOL_FIELD } HCTool;
static HCTool gTool = TOOL_BROWSE;
static Object *gSelected = NULL;   // objet sélectionné en mode édition

HCView *gView = nil;               // la vue courante
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
static void draw_part(Object *o) {
    if (!o->visible) return;

    NSRect r = NSMakeRect(o->x, o->y, o->w, o->h);

    if (o->type == OBJ_BUTTON) {
            const char *st = o->style ? o->style : "rectangle";
            BOOL isCheck = (strcmp(st, "checkBox") == 0 || strcmp(st, "checkbox") == 0);
            BOOL isRadio = (strcmp(st, "radioButton") == 0 || strcmp(st, "radiobutton") == 0);

            const char *nm = o->name ? o->name : "";
            NSString *s = [NSString stringWithUTF8String:nm];

            if (isCheck || isRadio) {
                // case ou rond à gauche, nom à droite
                CGFloat box = 14;
                CGFloat cy = o->y + o->h/2.0 - box/2.0;
                NSRect mark = NSMakeRect(o->x + 2, cy, box, box);

                [[NSColor whiteColor] setFill];
                [[NSColor blackColor] setStroke];

                if (isRadio) {
                    NSBezierPath *circle = [NSBezierPath bezierPathWithOvalInRect:mark];
                    [circle fill];
                    [circle stroke];
                    if (o->hilite) {
                        NSRect dot = NSInsetRect(mark, 4, 4);
                        [[NSColor blackColor] setFill];
                        [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
                    }
                } else {
                                // case à cocher
                                [[NSColor whiteColor] setFill];
                                NSRectFill(mark);
                                [[NSColor blackColor] setStroke];
                                NSBezierPath *box_path = [NSBezierPath bezierPathWithRect:mark];
                                [box_path setLineWidth:1];
                                [box_path stroke];
                                if (o->hilite) {
                                    NSBezierPath *x = [NSBezierPath bezierPath];
                                    [x moveToPoint:NSMakePoint(mark.origin.x+2, mark.origin.y+2)];
                                    [x lineToPoint:NSMakePoint(mark.origin.x+box-2, mark.origin.y+box-2)];
                                    [x moveToPoint:NSMakePoint(mark.origin.x+box-2, mark.origin.y+2)];
                                    [x lineToPoint:NSMakePoint(mark.origin.x+2, mark.origin.y+box-2)];
                                    [x setLineWidth:1.5];
                                    [x stroke];
                                }
                            }

                // le nom, à droite de la case
                NSDictionary *attrs = @{ NSFontAttributeName: [NSFont systemFontOfSize:13] };
                [s drawAtPoint:NSMakePoint(o->x + box + 8, o->y + o->h/2 - 8) withAttributes:attrs];
            }
            else {
                // bouton rectangle classique, avec highlight vidéo inverse
                BOOL on = o->hilite;
                [(on ? [NSColor blackColor] : [NSColor colorWithWhite:0.9 alpha:1.0]) setFill];
                NSRectFill(r);
                [[NSColor blackColor] setStroke];
                NSFrameRect(r);

                NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                [ps setAlignment:NSTextAlignmentCenter];
                NSDictionary *attrs = @{
                    NSFontAttributeName: [NSFont boldSystemFontOfSize:13],
                    NSForegroundColorAttributeName: (on ? [NSColor whiteColor] : [NSColor blackColor]),
                    NSParagraphStyleAttributeName: ps
                };
                NSRect tr = NSInsetRect(r, 4, 0);
                tr.origin.y += (r.size.height - 16) / 2;
                [s drawInRect:tr withAttributes:attrs];
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
static Object *part_at(Object *card, NSPoint p) {
    if (!card) return NULL;
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

@implementation HCView

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor whiteColor] setFill];
    NSRectFill(dirtyRect);

    Object *card = hc_current_card();
    if (!card) return;

    if (card->bg)
        for (int i = 0; i < card->bg->nparts; i++)
            draw_part(card->bg->parts[i]);

    for (int i = 0; i < card->nparts; i++)
        draw_part(card->parts[i]);

    if (gSelected) {
            NSRect r = NSMakeRect(gSelected->x, gSelected->y, gSelected->w, gSelected->h);
            [[NSColor redColor] setStroke];
            NSBezierPath *path = [NSBezierPath bezierPathWithRect:NSInsetRect(r, -2, -2)];
            [path setLineWidth:2];
            [path stroke];

            // poignées aux quatre coins
            [[NSColor redColor] setFill];
            CGFloat s = 6;
            NSPoint corners[4] = {
                {r.origin.x, r.origin.y},                                  // haut-gauche
                {r.origin.x + r.size.width, r.origin.y},                   // haut-droit
                {r.origin.x, r.origin.y + r.size.height},                  // bas-gauche
                {r.origin.x + r.size.width, r.origin.y + r.size.height}    // bas-droit
            };
            for (int i = 0; i < 4; i++)
                NSRectFill(NSMakeRect(corners[i].x - s/2, corners[i].y - s/2, s, s));
        }

    if (gDragging) {
        [[NSColor blueColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:gDragRect];
        [path setLineWidth:1];
        CGFloat dash[] = {4, 3};
        [path setLineDash:dash count:2 phase:0];
        [path stroke];
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    Object *hit = part_at(hc_current_card(), p);
    if (gSelected) NSLog(@"clic p=%.0f,%.0f  handle=%d  obj=%d,%d,%d,%d",
            p.x, p.y, handle_at(gSelected, p),
            gSelected->x, gSelected->y, gSelected->w, gSelected->h);
    // double-clic en mode édition : éditer le script
    if (gTool != TOOL_BROWSE && hit && [event clickCount] == 2) {
        [self editScriptOf:hit];
        return;
    }

    if (gTool == TOOL_BROWSE) {
            if (hit && hit->type == OBJ_FIELD) {
                [self beginFieldEdit:hit];
                return;
            }
        if (hit) {
                    gPressed = hit;
                    // flash uniquement pour les boutons rectangle avec autohilite
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

 
    // mode bouton/champ
        // d'abord : saisit-on une poignée de l'objet déjà sélectionné ?
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
    if (gResizeHandle) {
            gResizeHandle = 0;
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
    char name[64];
    Object *o;
    if (gTool == TOOL_BUTTON) {
        snprintf(name, sizeof name, "Bouton %d", ++gNewCount);
        o = hc_new_button(card, name);
    } else {
        snprintf(name, sizeof name, "Champ %d", ++gNewCount);
        o = hc_new_field(card, name);
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
    [self setNeedsDisplay:YES];
    [gMsgBox selectText:nil];
}

- (void)installToolPalette {
    NSPanel *palette = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(520, 400, 120, 60)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [palette setTitle:@"Outils"];
    [palette setFloatingPanel:YES];

    NSView *content = [palette contentView];

    NSButton *(^mk)(NSString*, NSInteger, CGFloat) = ^NSButton*(NSString *title, NSInteger tag, CGFloat x) {
        NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(x, 10, 36, 36)];
        [b setTitle:title];
        [b setTag:tag];
        [b setTarget:self];
        [b setAction:@selector(toolChosen:)];
        [b setBezelStyle:NSBezelStyleRegularSquare];
        [content addSubview:b];
        return b;
    };
    mk(@"👆", TOOL_BROWSE, 4);
    mk(@"B", TOOL_BUTTON, 42);
    mk(@"F", TOOL_FIELD, 80);

    [palette makeKeyAndOrderFront:nil];
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
