#import "HCview.h"
#import "hc_core.h"
#import <objc/runtime.h>   // pour associer un bitmap à un Object
#include <stdlib.h>        // getenv, pour la trace HC_RUNS_DEBUG
#include <strings.h>       // strcasecmp, pour les noms de proprietes globales
#include <float.h>         // FLT_MAX, pour la taille libre de l'editeur de champ
#import <QuartzCore/QuartzCore.h>  // CATransaction, pour pousser les pixels a l ecran
#import "icons.h"
#import "HCglobals.h"
#import "HCtext.h"
#import "HCvisual.h"
#import "HCprint.h"
#import "HCpalettes.h"
#import "HCicons.h"
#import "HCiconedit.h"
#import "graphics.h"
#import "hc_file.h"   /* hc_save, pour « save stack ... as ... » */
#import "HCdialogs.h"
#import "HCpaint.h"
#import "hct_verif.h"

extern void hc_sync_size_field(Object *o);  // definie dans HCdialogs.m
extern Object *cocoa_open_stack(const char *nom);      // definies dans AppDelegate.m
extern Object *cocoa_load_stack(const char *nom);
extern void    cocoa_stack_changed(Object *stack);
static NSFont *text_font(void);

typedef struct {
    /* édition d'un champ */
    Object       *editingField;
    NSTextView   *fieldEditor;
    NSScrollView *fieldScroll;

    /* édition d'un script */
    Object       *editTarget;
    NSTextView   *editView;
    NSPanel      *editPanel;

    /* sélection et interaction */
    Object       *pressed;        /* objet sous le bouton de la souris */
    Object       *popupTarget;    /* menu popup ouvert */
    NSArray<NSString *> *popupItems;
    NSArray<NSNumber *> *popupItemLines; /* lignes HC, base 1 */
    NSRect        popupRect;
    CGFloat       popupRowHeight;
    NSInteger     popupKeyboardRow;
    NSInteger     popupChosenRow;
    BOOL          popupFlashInverted;
    NSInteger     popupFlashToggles;
    NSTimer      *popupFlashTimer;
    NSTimeInterval popupOpenedAt;
    Object       *scrollField;    /* champ dont on glisse la poignée */
    Object       *clickField;     /* champ du dernier clic */
    NSPoint       clickPoint;

    /* couche affichée */
    BOOL          editBackground; /* NO = carte, YES = fond */

    /* collage de peinture en attente de dépôt */
    BOOL          floating;
    NSPoint       floatPos;

    /* saisie de l'outil texte */
    BOOL             textActive;
    NSPoint          textPos;
    NSMutableString *textBuf;
    NSBitmapImageRep *paintUndo;
    Object           *paintUndoLayer;

    Object       *card;

    /* numérotation des cartes créées */
    int           newCount;
} HCDoc;

static HCDoc  gDoc0;
static HCDoc *gDoc = &gDoc0;
static NSTimer *gStillDownTimer = nil;
void hc_set_active_doc(void *d) { gDoc = d ? (HCDoc *)d : &gDoc0; }

#define gEditingField    (gDoc->editingField)
#define gFieldEditor     (gDoc->fieldEditor)
#define gFieldScroll     (gDoc->fieldScroll)
#define gEditTarget      (gDoc->editTarget)
#define gEditView        (gDoc->editView)
#define gEditPanel       (gDoc->editPanel)
#define gPressed         (gDoc->pressed)
#define gPopupTarget     (gDoc->popupTarget)
#define gPopupItems      (gDoc->popupItems)
#define gPopupItemLines  (gDoc->popupItemLines)
#define gPopupRect       (gDoc->popupRect)
#define gPopupRowHeight  (gDoc->popupRowHeight)
#define gPopupKeyboardRow (gDoc->popupKeyboardRow)
#define gPopupChosenRow  (gDoc->popupChosenRow)
#define gPopupFlashInverted (gDoc->popupFlashInverted)
#define gPopupFlashToggles (gDoc->popupFlashToggles)
#define gPopupFlashTimer (gDoc->popupFlashTimer)
#define gPopupOpenedAt   (gDoc->popupOpenedAt)
#define gScrollField     (gDoc->scrollField)
#define gClickField      (gDoc->clickField)
#define gClickPoint      (gDoc->clickPoint)
#define gEditBackground  (gDoc->editBackground)
#define gFloating        (gDoc->floating)
#define gFloatPos        (gDoc->floatPos)
#define gTextActive      (gDoc->textActive)
#define gTextPos         (gDoc->textPos)
#define gTextBuf         (gDoc->textBuf)
#define gNewCount        (gDoc->newCount)
#define gDocCard         (gDoc->card)
#define gPaintUndo       (gDoc->paintUndo)
#define gPaintUndoLayer  (gDoc->paintUndoLayer)

static NSTextField *gMsgBox = nil;
static NSPanel *gMsgPanel = nil;

static NSPoint gDragStart;
static NSRect  gDragRect;
static BOOL    gDragging = NO;
static BOOL    gMoving = NO;
static NSPoint gMoveStart;
static int     gObjStartX, gObjStartY;
static int     gResizeHandle = 0;
static int     gObjStartW, gObjStartH;

static NSPoint gPenLast;
static BOOL    gPenDrawing = NO;

typedef enum { AXIS_NONE, AXIS_HORIZONTAL, AXIS_VERTICAL } HCAxisLock;
static HCAxisLock gLockedAxis = AXIS_NONE;

static NSPoint gShapeStart;
static NSPoint gShapeEnd;
static BOOL    gShapeDrawing = NO;
static int       gTextHeight = 0;
static NSString *gTextStyleName = nil;
static NSString *gTextAlign = nil;
 
static NSPoint gLassoPts[4096];
static int gLassoCount = 0;
static BOOL gLassoDrawing = NO;
static BOOL gLassoActive = NO;

static NSPoint gSelStart, gSelEnd;
static BOOL gSelRectDrawing = NO;
static BOOL gSelRectActive = NO;

static NSPanel *gPatternPanel = nil;
static NSPanel *gToolPanel = nil;
static NSPanel *gWidthPanel = nil;
static NSPanel *gBrushPanel = nil;

static BOOL gTextUnderline = NO;

static CGFloat gScrollGrab, gScrollGH, gScrollKH, gScrollGY, gScrollMax;

#define NUM_PATTERNS 38

static NSPoint gFreePts[4096];
static int gFreeCount = 0;
static BOOL gFreeDrawing = NO;

static BOOL gFloatDragging = NO;
static NSPoint gFloatGrab;
static NSFont *gTextFont = nil;

/* Fourmis de feu */
static NSTimer *gAntsTimer = nil;
static CGFloat  gAntsPhase = 0.0;

@interface HCView ()
- (void)popupFlashTick:(NSTimer *)timer;
- (void)startAntsTimer;
- (void)stopAntsTimer;
@end

/* Une partie mérite-t-elle d'être redessinée ? La marge couvre le cadre
 * d'édition et le liseré de sélection, qui débordent un peu. */
static inline BOOL part_touche(Object *o, NSRect sale) {
    if (!o) return NO;
    return NSIntersectsRect(sale, NSMakeRect(o->x - 8, o->y - 8,
                                             o->w + 16, o->h + 16));
}
static NSPoint constrain_to_axis(NSPoint start, NSPoint current) {
    CGFloat dx = fabs(current.x - start.x);
    CGFloat dy = fabs(current.y - start.y);
    if (dx > dy) {
        return NSMakePoint(current.x, start.y);
    } else {
        return NSMakePoint(start.x, current.y);
    }
}

static NSRect compute_shape_rect(NSPoint start, NSPoint end, BOOL centered) {
    if (centered) {
        CGFloat dx = fabs(end.x - start.x);
        CGFloat dy = fabs(end.y - start.y);
        return NSMakeRect(start.x - dx, start.y - dy, dx * 2, dy * 2);
    } else {
        return NSMakeRect(MIN(start.x, end.x), MIN(start.y, end.y),
                          fabs(end.x - start.x), fabs(end.y - start.y));
    }
}

static void radio_exclusive(Object *card, Object *keep) {
    if (!card) return;
    for (int i = 0; i < card->nparts; i++) {
        Object *o = card->parts[i];
        if (o->type == OBJ_BUTTON && o != keep && o->style &&
            (strcmp(o->style, "radioButton") == 0 || strcmp(o->style, "radiobutton") == 0))
            hc_set_hilite(o, card, 0);
    }
    if (card->bg)
        for (int i = 0; i < card->bg->nparts; i++) {
            Object *o = card->bg->parts[i];
            if (o->type == OBJ_BUTTON && o != keep && o->style &&
                (strcmp(o->style, "radioButton") == 0 || strcmp(o->style, "radiobutton") == 0))
                hc_set_hilite(o, card, 0);
        }
}

/* La couleur du nom d'un bouton.
 *
 * Un bouton désactivé écrit en gris, comme les contrôles grisés du Toolbox que
 * HyperCard a repris : c'est le seul signe visible qu'il est inerte, puisqu'il
 * garde sa place et son cadre. Le gris l'emporte sur l'allumage — un bouton
 * désactivé n'a de toute façon aucune raison d'être allumé. */
static NSColor *btn_label_color(Object *o, NSColor *normale) {
    if (o->type == OBJ_BUTTON && !o->enabled)
        return [NSColor colorWithWhite:0.55 alpha:1.0];
    return normale;
}

static void draw_btn_label(Object *o, NSString *s, NSRect r, BOOL on, CGFloat defSize) {
    if (!o->showname) return;
    CGFloat fs = o->textsize > 0 ? o->textsize : defSize;
    NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
    [ps setAlignment:NSTextAlignmentCenter];

    NSMutableDictionary *attrs =
        [obj_attrs(o, defSize, btn_label_color(o, on ? [NSColor whiteColor]
                                                     : [NSColor blackColor])) mutableCopy];
    attrs[NSParagraphStyleAttributeName] = ps;

    NSRect tr = NSInsetRect(r, 4, 0);
    tr.origin.y += (r.size.height - fs * 1.2) / 2;
    [s drawInRect:tr withAttributes:attrs];
}

static void draw_edit_outline(NSRect r) {
    if (gTool != TOOL_BUTTON && gTool != TOOL_FIELD) return;
    [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
    NSBezierPath *outline = [NSBezierPath bezierPathWithRect:r];
    [outline setLineWidth:1];
    CGFloat dash[] = {3, 2};
    [outline setLineDash:dash count:2 phase:0];
    [outline stroke];
}

static void draw_btn_frame(Object *o, NSRect r, BOOL on) {
    const char *st = o->style ? o->style : "rectangle";

    if (strcmp(st, "transparent") == 0) {
        /* Noir franc, et non un voile gris : HyperCard INVERSAIT la zone d'un
         * bouton transparent allumé, le noir passant au blanc et
         * réciproquement. Le reste du code suit déjà cette logique — l'icône
         * et le nom se dessinent en blanc quand `on`.
         *
         * Mais un bouton à ICÔNE ne s'inverse pas du tout par son fond : seule
         * l'encre de l'icône passe au blanc, ce dont draw_part se charge. On
         * ne touche donc à rien ici quand une icône est posée. */
        if (on && o->icon == 0) {
            [[NSColor blackColor] setFill];
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
        [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
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
    [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
    NSRectFill(r);
    [[NSColor blackColor] setFill];
    NSFrameRect(r);
}

static void close_popup_menu(void) {
    [gPopupFlashTimer invalidate];
    gPopupFlashTimer = nil;
    gPopupTarget = NULL;
    gPopupItems = nil;
    gPopupItemLines = nil;
    gPopupRect = NSZeroRect;
    gPopupRowHeight = 0;
    gPopupKeyboardRow = -1;
    gPopupChosenRow = -1;
    gPopupFlashInverted = NO;
    gPopupFlashToggles = 0;
}

static BOOL popup_row_is_enabled(NSInteger row) {
    return row >= 0 && row < (NSInteger)gPopupItems.count;
}

static NSInteger popup_row_at_point(NSPoint p) {
    if (!NSPointInRect(p, gPopupRect) || gPopupRowHeight <= 0) return -1;
    NSInteger row = (NSInteger)floor((p.y - gPopupRect.origin.y - 1) /
                                     gPopupRowHeight);
    return popup_row_is_enabled(row) ? row : -1;
}

static void open_popup_menu(Object *o, HCView *view) {
    if (!o->contents || !*o->contents) return;
    NSArray<NSString *> *raw = [[NSString stringWithUTF8String:o->contents]
                                componentsSeparatedByString:@"\n"];
    NSMutableArray<NSString *> *items = [NSMutableArray array];
    NSMutableArray<NSNumber *> *lines = [NSMutableArray array];
    for (NSUInteger i = 0; i < raw.count; i++) {
        if (raw[i].length == 0) continue;
        [items addObject:raw[i]];
        [lines addObject:@(i + 1)];
    }
    if (items.count == 0) return;

    NSDictionary *attrs = obj_attrs(o, 12, nil);
    CGFloat width = o->w;
    for (NSString *item in items)
        width = MAX(width, ceil([item sizeWithAttributes:attrs].width) + 24);
    gPopupRowHeight = MAX(16, ceil([@"Ag" sizeWithAttributes:attrs].height) + 4);
    CGFloat height = gPopupRowHeight * items.count;
    NSRect bounds = view.bounds;
    CGFloat x = MIN(MAX(0, o->x), MAX(0, bounds.size.width - width - 3));
    CGFloat y = o->y + o->h;
    if (y + height + 3 > bounds.size.height) y = MAX(0, o->y - height);

    gPopupTarget = o;
    gPopupItems = items.copy;
    gPopupItemLines = lines.copy;
    gPopupRect = NSMakeRect(x, y, width, height);
    gPopupOpenedAt = [NSDate timeIntervalSinceReferenceDate];
    NSUInteger selected = [gPopupItemLines indexOfObject:@(o->selectedline)];
    gPopupKeyboardRow = selected == NSNotFound ? 0 : (NSInteger)selected;
    [view.window makeFirstResponder:view];
    [view.window setAcceptsMouseMovedEvents:YES];
    [view setNeedsDisplay:YES];
}

static void draw_popup_menu(void) {
    if (!gPopupTarget || gPopupItems.count == 0) return;
    [[NSColor blackColor] setFill];
    NSRectFill(NSOffsetRect(gPopupRect, 3, 3));
    [[NSColor whiteColor] setFill];
    NSRectFill(gPopupRect);
    [[NSColor blackColor] setStroke];
    [[NSBezierPath bezierPathWithRect:NSInsetRect(gPopupRect, .5, .5)] stroke];

    NSDictionary *attrs = obj_attrs(gPopupTarget, 12, nil);
    for (NSUInteger i = 0; i < gPopupItems.count; i++) {
        NSRect row = NSMakeRect(gPopupRect.origin.x + 1,
                                gPopupRect.origin.y + i * gPopupRowHeight + 1,
                                gPopupRect.size.width - 2, gPopupRowHeight);
        BOOL selected = (NSInteger)i == gPopupKeyboardRow && !gPopupFlashInverted;
        if (selected) { [[NSColor blackColor] setFill]; NSRectFill(row); }
        NSMutableDictionary *rowAttrs = attrs.mutableCopy;
        rowAttrs[NSForegroundColorAttributeName] = selected ? NSColor.whiteColor : NSColor.blackColor;
        NSString *title = gPopupItems[i];
        CGFloat textHeight = [title sizeWithAttributes:rowAttrs].height;
        [title drawAtPoint:NSMakePoint(row.origin.x + 14, row.origin.y + floor((row.size.height - textHeight) / 2))
            withAttributes:rowAttrs];
        if (gPopupItemLines[i].integerValue == gPopupTarget->selectedline) {
            [(selected ? NSColor.whiteColor : NSColor.blackColor) setStroke];
            NSBezierPath *check = NSBezierPath.bezierPath;
            CGFloat cx = row.origin.x + 2, cy = row.origin.y + row.size.height / 2;
            [check moveToPoint:NSMakePoint(cx, cy)];
            [check lineToPoint:NSMakePoint(cx + 2, cy + 3)];
            [check lineToPoint:NSMakePoint(cx + 6, cy - 3)];
            check.lineWidth = 1.5;
            [check stroke];
        }
    }
}

static void choose_popup_row(HCView *view, NSInteger row) {
    if (!gPopupTarget || !popup_row_is_enabled(row)) return;
    gPopupTarget->selectedline = gPopupItemLines[row].intValue;
    hc_send(gPopupTarget, "mouseUp");
    close_popup_menu();
    [view setNeedsDisplay:YES];
}

static void flash_popup_selection(HCView *view, NSInteger row) {
    if (!gPopupTarget || !popup_row_is_enabled(row)) return;
    gPopupKeyboardRow = row;
    gPopupChosenRow = row;
    gPopupFlashInverted = NO;
    gPopupFlashToggles = 6;
    [gPopupFlashTimer invalidate];
    gPopupFlashTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 15.0
                                                         target:view
                                                       selector:@selector(popupFlashTick:)
                                                       userInfo:nil repeats:YES];
    [view setNeedsDisplay:YES];
}

static void draw_part(Object *o) {
    if (!o->visible) return;

    NSRect r = NSMakeRect(o->x, o->y, o->w, o->h);

    if (o->type == OBJ_BUTTON) {
        const char *st = o->style ? o->style : "rectangle";
        BOOL isCheck  = (strcmp(st, "checkBox") == 0    || strcmp(st, "checkbox") == 0);
        BOOL isRadio  = (strcmp(st, "radioButton") == 0 || strcmp(st, "radiobutton") == 0);
        BOOL isTransp = (strcmp(st, "transparent") == 0);
        BOOL isPopup  = (strcmp(st, "popup") == 0);

        const char *nm = o->name ? o->name : "";
        NSString *s = [NSString stringWithUTF8String:nm];
        /* Pour un bouton de fond non partagé, l'allumage vit dans la carte :
         * lire o->hilite donnerait le même état sur toutes les cartes du fond. */
        BOOL on = hc_hilite_of(o, hc_current_card());

        const HCIcon *ic = (o->icon ? hcicon_find(o->icon) : NULL);

        if (ic) {
            /* L'icône se centre dans la place qui lui revient.
             *
             * Quand le nom s'affiche, il occupe la bande du bas : l'icône se
             * centre alors dans les 36 points du haut, ce qui la remet à
             * o->y + 2. Sans nom, elle se centre dans TOUT le bouton — la
             * caler à o->y + 2 la décalait de deux points vers le bas sur un
             * bouton de 32 ou 34 de haut, soit la taille habituelle d'un
             * bouton à icône. */
            BOOL withName = (o->showname && o->h > 36);
            CGFloat iarea = withName ? 36 : o->h;
            CGFloat iy    = o->y + floor((iarea - 32) / 2.0);
            NSRect ir = NSMakeRect(floor(o->x + (o->w - 32)/2.0), floor(iy), 32, 32);

            draw_btn_frame(o, r, on);

            /* Bouton transparent allumé : l'inversion se limite à la FORME de
             * l'icône — l'encre passe au blanc, le blanc enclos passe au noir,
             * et la carte autour n'est pas touchée. Ni carré noir, ni icône
             * qui disparaît sur fond blanc. */
            if (on && isTransp) {
                hcicon_draw_inverted(ic, ir, 1.0);
            } else {
                [(on ? [NSColor whiteColor] : [NSColor blackColor]) setFill];
                hcicon_draw(ic, ir, 1.0);
            }
            draw_edit_outline(r);

            if (withName) {
                NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                [ps setAlignment:NSTextAlignmentCenter];
                NSMutableDictionary *bat = [obj_attrs(o, 11,
                    btn_label_color(o, on ? [NSColor whiteColor]
                                          : [NSColor blackColor])) mutableCopy];
                bat[NSParagraphStyleAttributeName] = ps;
                /* Le texte ne suit plus iy : la bande du bas est fixe, elle
                 * commence sous les 36 points réservés à l'icône. */
                NSRect btr = NSMakeRect(o->x, o->y + 36, o->w, o->h - 36);

                /* Bouton transparent allumé : le nom s'inverse lui aussi.
                 *
                 * Pour du texte, inverser ne peut vouloir dire qu'une chose —
                 * fond noir, lettres blanches. On noircit donc DERRIÈRE, mais
                 * serré sur le texte plutôt qu'en bande large sur toute la
                 * largeur du bouton, dans le même esprit que l'icône dont
                 * l'inversion épouse la forme.
                 *
                 * Les autres styles ont déjà noirci tout leur corps dans
                 * draw_btn_frame : il n'y a rien à poser sous le texte. */
                if (on && isTransp) {
                    NSSize sz = [s sizeWithAttributes:bat];
                    NSRect tb = NSMakeRect(
                        floor(btr.origin.x + (btr.size.width - sz.width) / 2.0) - 1,
                        btr.origin.y,
                        ceil(sz.width) + 2,
                        ceil(sz.height));
                    [[NSColor blackColor] setFill];
                    NSRectFill(tb);
                }

                [s drawInRect:btr withAttributes:bat];
            }
        }
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
                [s drawAtPoint:NSMakePoint(o->x + box + 8, o->y + o->h/2 - fs*0.6)
                withAttributes:obj_attrs(o, 13, btn_label_color(o, nil))];
            }
        }
        else if (isTransp) {
            draw_btn_frame(o, r, on);
            draw_edit_outline(r);
            if (o->showname) {
                CGFloat fs = o->textsize > 0 ? o->textsize : 16;
                NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                [ps setAlignment:NSTextAlignmentCenter];
                NSMutableDictionary *bat = [obj_attrs(o, 16,
                    btn_label_color(o, [NSColor blackColor])) mutableCopy];
                bat[NSParagraphStyleAttributeName] = ps;
                NSRect btr = NSInsetRect(r, 2, 0);
                btr.origin.y += (r.size.height - fs * 1.2) / 2;
                [s drawInRect:btr withAttributes:bat];
            }
        }
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

            CGFloat cx = body.origin.x + body.size.width - 12;
            CGFloat cy = body.origin.y + body.size.height/2.0;
            [[NSColor blackColor] setFill];
            NSBezierPath *ar = [NSBezierPath bezierPath];
            [ar moveToPoint:NSMakePoint(cx - 4, cy - 2)];
            [ar lineToPoint:NSMakePoint(cx + 4, cy - 2)];
            [ar lineToPoint:NSMakePoint(cx,     cy + 3)];
            [ar closePath];
            [ar fill];

            NSString *label = s;
            if (o->contents && *o->contents) {
                NSArray *lines = [[NSString stringWithUTF8String:o->contents]
                                  componentsSeparatedByString:@"\n"];
                int sel = o->selectedline > 0 ? o->selectedline : 1;
                if (sel <= (int)[lines count] && [lines[sel-1] length] > 0)
                    label = lines[sel-1];
            }
            CGFloat fs = o->textsize > 0 ? o->textsize : 12;
            [label drawAtPoint:NSMakePoint(body.origin.x + 6,
                                           body.origin.y + (body.size.height - fs*1.3)/2)
                withAttributes:obj_attrs(o, 12, btn_label_color(o, nil))];
        }
        else {
            draw_btn_frame(o, r, on);
            NSRect lr = r;
            if (strcmp(st, "shadow") == 0)
                lr = NSMakeRect(r.origin.x, r.origin.y,
                                r.size.width - 3, r.size.height - 3);
            draw_btn_label(o, s, lr, on, 13);
        }

        if (gTool == TOOL_BUTTON) {
            draw_edit_outline(r);
        }
    }
    else if (o->type == OBJ_FIELD) {
        const char *st = o->style ? o->style : "rectangle";
        BOOL isTransp = (strcmp(st, "transparent") == 0);
        BOOL isOpaque = (strcmp(st, "opaque") == 0);
        BOOL isShadow = (strcmp(st, "shadow") == 0);
        BOOL isScroll = (strcmp(st, "scrolling") == 0);

        NSRect body = r;

        if (isTransp) {
        }
        else if (isOpaque) {
            [[NSColor whiteColor] setFill];
            NSRectFill(r);
        }
        else if (isShadow) {
            body = NSMakeRect(r.origin.x, r.origin.y,
                              r.size.width - 3, r.size.height - 3);
            NSRect sh = NSMakeRect(r.origin.x + 3, r.origin.y + 3,
                                   r.size.width - 3, r.size.height - 3);
            [[NSColor blackColor] setFill];
            NSRectFill(sh);
            [[NSColor whiteColor] setFill];
            NSRectFill(body);
            [[NSColor blackColor] setStroke];
            NSBezierPath *bp = [NSBezierPath bezierPathWithRect:NSInsetRect(body, 0.5, 0.5)];
            [bp setLineWidth:1];
            [bp stroke];
        }
        else if (isScroll) {
            field_clamp_scroll(o);

            [[NSColor whiteColor] setFill];
            NSRectFill(r);
            [[NSColor blackColor] setFill];
            NSFrameRect(r);

            CGFloat bw = 16;
            NSRect bar = NSMakeRect(r.origin.x + r.size.width - bw, r.origin.y,
                                    bw, r.size.height);
            [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
            NSRectFill(bar);
            [[NSColor blackColor] setFill];
            NSFrameRect(bar);

            [[NSColor blackColor] setFill];
            CGFloat cx = bar.origin.x + bw/2;
            NSBezierPath *up = [NSBezierPath bezierPath];
            [up moveToPoint:NSMakePoint(cx - 4, bar.origin.y + 11)];
            [up lineToPoint:NSMakePoint(cx + 4, bar.origin.y + 11)];
            [up lineToPoint:NSMakePoint(cx,     bar.origin.y + 5)];
            [up closePath]; [up fill];
            NSBezierPath *dn = [NSBezierPath bezierPath];
            CGFloat by = bar.origin.y + bar.size.height;
            [dn moveToPoint:NSMakePoint(cx - 4, by - 11)];
            [dn lineToPoint:NSMakePoint(cx + 4, by - 11)];
            [dn lineToPoint:NSMakePoint(cx,     by - 5)];
            [dn closePath]; [dn fill];

            NSRect tr0 = field_text_rect(o);
            CGFloat th = field_text_height(o, tr0);
            CGFloat vh = tr0.size.height;
            CGFloat gy = bar.origin.y + 16;
            CGFloat gh = bar.size.height - 32;
            if (th > vh && gh > 8) {
                CGFloat kh = gh * (vh / th);
                if (kh < 12) kh = 12;
                if (kh > gh) kh = gh;
                CGFloat maxs = th - vh;
                CGFloat pos = (maxs > 0) ? (o->scroll / maxs) : 0;
                if (pos < 0) pos = 0;
                if (pos > 1) pos = 1;
                NSRect knob = NSMakeRect(bar.origin.x + 1, gy + pos * (gh - kh),
                                         bw - 2, kh);
                [[NSColor colorWithWhite:0.75 alpha:1.0] setFill];
                NSRectFill(knob);
                [[NSColor blackColor] setFill];
                NSFrameRect(knob);
            }

            body = NSMakeRect(r.origin.x, r.origin.y, r.size.width - bw, r.size.height);
        }
        else {
            [[NSColor whiteColor] setFill];
            NSRectFill(r);
            [[NSColor blackColor] setFill];
            NSFrameRect(r);
        }

        NSDictionary *at = obj_attrs(o, 12, [NSColor blackColor]);
        NSRect tr = field_text_rect(o);

        if (o->show_lines) {
            CGFloat lh;
            if (o->fixed_lh) {
                lh = hc_text_height(o);
            } else {
                NSFont *fb = obj_base_font(o, 12);
                NSAttributedString *une =
                    [[NSAttributedString alloc] initWithString:@"Mg"
                                                    attributes:@{NSFontAttributeName: fb}];
                lh = [une boundingRectWithSize:NSMakeSize(10000, CGFLOAT_MAX)
                                       options:NSStringDrawingUsesLineFragmentOrigin
                      ].size.height;
            }
            if (lh < 4) lh = 12;
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            for (CGFloat y = tr.origin.y + lh - o->scroll;
                 y < body.origin.y + body.size.height - 2; y += lh) {
                if (y < body.origin.y) continue;
                NSBezierPath *ln = [NSBezierPath bezierPath];
                [ln moveToPoint:NSMakePoint(tr.origin.x, floor(y) + 0.5)];
                [ln lineToPoint:NSMakePoint(tr.origin.x + tr.size.width, floor(y) + 0.5)];
                [ln setLineWidth:1];
                [ln stroke];
            }
        }

        const char *tx = hc_field_text(o);
        NSString *s = [NSString stringWithUTF8String:tx];

        NSAttributedString *as = field_attr_string(o, s, at);

        if (o != gEditingField) {
            /* On ne trace QUE la plage visible.
             *
             * -drawInRect: mettait en page tout le texte, quarante-huit
             * kilo-octets pour une vingtaine de lignes affichées : mesuré à
             * plus de quatre cents millisecondes par image sur un champ
             * chargé. NSLayoutManager, lui, compose paresseusement — en ne
             * lui demandant que les glyphes du rectangle visible, il ne
             * touchera jamais aux lignes du dessous. La mise en page est en
             * plus conservée d'une image à l'autre par field_layout.
             *
             * Le décalage vertical porte le défilement : le conteneur a son
             * origine au début du texte, et l'on pose cette origine plus haut
             * que le cadre pour montrer le passage voulu. */
            NSTextContainer *tc = nil;
            NSLayoutManager *lm = field_layout(o, s, at, tr.size.width, &tc);

            CGFloat dy = isScroll ? o->scroll : 0;
            NSRect visible = NSMakeRect(0, dy, tr.size.width, tr.size.height);
            NSRange plage = [lm glyphRangeForBoundingRect:visible
                                          inTextContainer:tc];
            NSPoint origine = NSMakePoint(tr.origin.x, tr.origin.y - dy);

            [NSGraphicsContext saveGraphicsState];
            NSRectClip(tr);
            [lm drawBackgroundForGlyphRange:plage atPoint:origine];
            [lm drawGlyphsForGlyphRange:plage atPoint:origine];
            [NSGraphicsContext restoreGraphicsState];
        }

        int fstart = 0, flen = 0;
        if (o != gEditingField &&
            hc_found_range(o, &fstart, &flen) && flen > 0 &&
            fstart + flen <= (int)[s length]) {

            /* La mise en page du TRACÉ, comme pour le test de clic.
             *
             * Celle qu'on montait ici gardait l'interligne de police par
             * défaut, alors que le tracé l'a désactivé : la boîte noire du
             * texte trouvé se décalait donc vers le bas à mesure qu'on
             * descendait dans le champ. Personne ne l'avait encore signalé,
             * mais c'est le même défaut que celui de la ligne surlignée, et
             * il se corrige au même endroit. */
            NSTextContainer *tc = nil;
            NSLayoutManager *lm = field_layout(o, s, at, tr.size.width, &tc);

            NSRange glyphs = [lm glyphRangeForCharacterRange:NSMakeRange(fstart, flen)
                                       actualCharacterRange:NULL];
            NSRect box = [lm boundingRectForGlyphRange:glyphs inTextContainer:tc];
            box.origin.x += tr.origin.x;
            box.origin.y += tr.origin.y - (isScroll ? o->scroll : 0);

            [NSGraphicsContext saveGraphicsState];
            NSRectClip(tr);
            [[NSColor blackColor] setFill];
            NSRectFill(box);

            NSMutableAttributedString *sub =
                [[as attributedSubstringFromRange:NSMakeRange(fstart, flen)] mutableCopy];
            [sub addAttribute:NSForegroundColorAttributeName
                        value:[NSColor whiteColor]
                        range:NSMakeRange(0, [sub length])];
            [sub drawAtPoint:box.origin];
            [NSGraphicsContext restoreGraphicsState];
        }

        if (gTool == TOOL_FIELD || isTransp) draw_edit_outline(r);
    }
}

static int handle_at(Object *o, NSPoint p) {
    if (!o) return 0;
    CGFloat s = 8;
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

/* Un bouton désactivé est-il transparent au clic ?
 *
 * Seulement en mode Browse. Avec l'outil Bouton on doit pouvoir le
 * sélectionner, le déplacer et rouvrir son Info — sans quoi on ne pourrait
 * plus jamais le réactiver autrement que par script. */
static BOOL part_inerte(Object *o) {
    return (gTool == TOOL_BROWSE && o->type == OBJ_BUTTON && !o->enabled);
}

static Object *part_at(Object *card, NSPoint p) {
    if (!card) return NULL;

    if (gTool != TOOL_BROWSE) {
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) return NULL;
        for (int i = layer->nparts - 1; i >= 0; i--) {
            Object *o = layer->parts[i];
            if (o->visible && !part_inerte(o) &&
                p.x >= o->x && p.x <= o->x + o->w &&
                p.y >= o->y && p.y <= o->y + o->h)
                return o;
        }
        return NULL;
    }

    for (int i = card->nparts - 1; i >= 0; i--) {
        Object *o = card->parts[i];
        if (o->visible && !part_inerte(o) &&
            p.x >= o->x && p.x <= o->x + o->w &&
            p.y >= o->y && p.y <= o->y + o->h)
            return o;
    }
    if (card->bg)
        for (int i = card->bg->nparts - 1; i >= 0; i--) {
            Object *o = card->bg->parts[i];
            if (o->visible && !part_inerte(o) &&
                p.x >= o->x && p.x <= o->x + o->w &&
                p.y >= o->y && p.y <= o->y + o->h)
                return o;
        }
    return NULL;
}
static char gDlgBuf[512];
static char gFileBuf[2048];

static int cocoa_save_stack(Object *stack, const char *path) {
    if (!stack || !path || !*path) return 0;
    [gView flushPaintToKernel];
    return hc_save(stack, path) == 0;
}

static const char *cocoa_answer_file(const char *prompt) {
    NSOpenPanel *p = [NSOpenPanel openPanel];
    [p setCanChooseFiles:YES];
    [p setCanChooseDirectories:NO];
    [p setAllowsMultipleSelection:NO];
    if (prompt && *prompt)
        [p setMessage:[NSString stringWithUTF8String:prompt]];
    if ([p runModal] != NSModalResponseOK) return NULL;
    NSString *chemin = [[p URL] path];
    if (!chemin) return NULL;
    snprintf(gFileBuf, sizeof gFileBuf, "%s", [chemin UTF8String]);
    return gFileBuf;
}

static const char *cocoa_ask_file(const char *prompt, const char *deflt) {
    NSSavePanel *p = [NSSavePanel savePanel];
    if (prompt && *prompt)
        [p setMessage:[NSString stringWithUTF8String:prompt]];
    if (deflt && *deflt)
        [p setNameFieldStringValue:[NSString stringWithUTF8String:deflt]];
    if ([p runModal] != NSModalResponseOK) return NULL;
    NSString *chemin = [[p URL] path];
    if (!chemin) return NULL;
    snprintf(gFileBuf, sizeof gFileBuf, "%s", [chemin UTF8String]);
    return gFileBuf;
}

static const char *cocoa_ask(const char *prompt, const char *deflt) {
    NSAlert *a = [[NSAlert alloc] init];
    [a setMessageText:[NSString stringWithUTF8String:prompt ? prompt : ""]];
    [a addButtonWithTitle:@"OK"];
    [a addButtonWithTitle:@"Annuler"];

    NSTextField *tf = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 260, 24)];
    [tf setStringValue:[NSString stringWithUTF8String:deflt ? deflt : ""]];
    [a setAccessoryView:tf];
    [[a window] setInitialFirstResponder:tf];

    if ([a runModal] != NSAlertFirstButtonReturn) return NULL;
    snprintf(gDlgBuf, sizeof gDlgBuf, "%s", [[tf stringValue] UTF8String]);
    return gDlgBuf;
}

static const char *cocoa_answer(const char *prompt, const char *b1,
                                const char *b2, const char *b3) {
    NSAlert *a = [[NSAlert alloc] init];
    [a setMessageText:[NSString stringWithUTF8String:prompt ? prompt : ""]];
    const char *order[3] = { b3, b2, b1 };
    for (int i = 0; i < 3; i++)
        if (order[i]) [a addButtonWithTitle:[NSString stringWithUTF8String:order[i]]];

    NSModalResponse rep = [a runModal];
    int idx = (int)(rep - NSAlertFirstButtonReturn);
    const char *chosen = b1;
    int k = 0;
    for (int i = 0; i < 3; i++)
        if (order[i]) { if (k == idx) { chosen = order[i]; break; } k++; }
    snprintf(gDlgBuf, sizeof gDlgBuf, "%s", chosen ? chosen : "OK");
    return gDlgBuf;
}

static void cocoa_line(HcLineKind kind, int depth, const char *text) {
    (void)depth;
    if (kind == HC_MSG && gMsgBox) {
        if (gMsgPanel && ![gMsgPanel isVisible])
            [gMsgPanel orderFront:nil];
        [gMsgBox setStringValue:[NSString stringWithUTF8String:text]];
        return;
    }
    if (kind == HC_ERR || getenv("HC_TRACE")) NSLog(@"%s", text);
}

static BOOL gMouseClicked = NO;

static void cocoa_choose_tool(const char *name) {
    if (!name) return;
    struct { const char *nom; HCTool t; } table[] = {
        { "browse",         TOOL_BROWSE   },
        { "button",         TOOL_BUTTON   },
        { "field",          TOOL_FIELD    },
        { "select",         TOOL_SELRECT  },
        { "lasso",          TOOL_LASSO    },
        { "pencil",         TOOL_PENCIL   },
        { "brush",          TOOL_BRUSH    },
        { "eraser",         TOOL_ERASER   },
        { "line",           TOOL_LINE     },
        { "spray",          TOOL_SPRAY    },
        { "spray can",      TOOL_SPRAY    },
        { "rectangle",      TOOL_RECT     },
        { "rect",           TOOL_RECT     },
        { "bucket",         TOOL_FILL     },
        { "oval",           TOOL_OVAL     },
        { "curve",          TOOL_FREEFORM },
        { "text",           TOOL_TEXT     },
        { "polygon",        TOOL_FREEFORM },
    };
    for (unsigned i = 0; i < sizeof table / sizeof *table; i++) {
        if (strcasecmp(name, table[i].nom) == 0) {
            [gView dropFloating];

            /* Sortie immédiate du mode édition et retrait du focus de l'éditeur Cocoa */
            if (gEditingField) {
                [gView endFieldEdit];
                [[gView window] makeFirstResponder:gView];
            }
            /* Changer pour un outil qui ne sélectionne pas abandonne la
             * sélection de peinture, comme dans HyperCard : on ne garde pas
             * son lasso en prenant la main ou le crayon.
             *
             * gSelected, juste en dessous, est la sélection d'un OBJET —
             * bouton ou champ. Ce sont deux choses distinctes, et seule la
             * première était remise à zéro : le rectangle en pointillés
             * survivait au changement d'outil, fourmis comprises. */
            HCTool neuf = table[i].t;
            if (neuf != TOOL_SELRECT && neuf != TOOL_LASSO) {
                gSelRectActive  = NO;
                gSelRectDrawing = NO;
                gLassoActive    = NO;
                gLassoDrawing   = NO;
                gLassoCount     = 0;
                [gView stopAntsTimer];
            }

            gTool = neuf;
            gTool = table[i].t;
            gSelected = NULL;
            [gView stopSprayTimer];
            [gView setNeedsDisplay:YES];
            return;
        }
    }
    NSLog(@"choose : outil inconnu « %s »", name);
}

static BOOL mods_has(const char *mods, const char *k) {
    if (!mods || !k || !*k) return NO;
    size_t n = strlen(k);
    for (const char *p = mods; *p; p++)
        if (strncasecmp(p, k, n) == 0) return YES;
    return NO;
}
/* ═══ VERROU D'ÉCRAN, CÔTÉ PEINTURE ═════════════════════════════════════
 *
 * « lock screen » ne verrouillait rien du tout pour le dessin. Le noyau
 * prévient pourtant l'hôte par host_global_set("lockScreen", …), mais
 * personne ici n'écoutait : chaque segment tracé invalidait sa zone, et un
 * tracé de surface en produit plus de deux mille. Entre un « lock screen »
 * et son « unlock screen », l'écran se recomposait donc deux mille fois —
 * exactement ce que la commande existe pour empêcher.
 *
 * Le noyau fait déjà cela pour les CHAMPS : notify_field met de côté ceux
 * qui changent pendant le verrou, et verrou_reveille les réveille au
 * déverrouillage. Il manquait le pendant pour la peinture, qui ne passe pas
 * par le noyau mais va droit à l'hôte.
 *
 * On accumule donc l'union des zones sales et l'on n'invalide qu'une fois,
 * au déverrouillage. Le noyau déverrouille de lui-même en retombant au
 * repos, si bien qu'un script qui oublie son « unlock screen » ne laisse pas
 * l'écran figé. */
static BOOL   gLockScreen = NO;
static BOOL   gSaleTout   = NO;   /* une invalidation totale est en attente */
static BOOL   gSaleUnPeu  = NO;   /* gSaleRect porte une zone en attente    */
static NSRect gSaleRect;

static void hcv_invalide(NSRect r)
{
    if (!gLockScreen) { [gView setNeedsDisplayInRect:r]; return; }
    gSaleRect  = gSaleUnPeu ? NSUnionRect(gSaleRect, r) : r;
    gSaleUnPeu = YES;
}

static void hcv_invalide_tout(void)
{
    if (!gLockScreen) { [gView setNeedsDisplay:YES]; return; }
    gSaleTout = YES;
}

static void hcv_verrou_ecran(BOOL ferme)
{
    if (ferme == gLockScreen) return;
    gLockScreen = ferme;
    if (ferme) return;                    /* on ferme : rien à faire */

    if (gSaleTout)       [gView setNeedsDisplay:YES];
    else if (gSaleUnPeu) [gView setNeedsDisplayInRect:gSaleRect];
    gSaleTout = NO;
    gSaleUnPeu = NO;
}

static void cocoa_drag(int x1, int y1, int x2, int y2, const char *mods) {
    Object *card = hc_current_card();
    if (!card || !gView) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSRect b = [gView bounds];
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)b.size.width, (int)b.size.height);
    NSPoint a = NSMakePoint(x1, y1), z = NSMakePoint(x2, y2);
    if (mods_has(mods, "shift")) {
        z = constrain_to_axis(a, z);
    }
    switch (gTool) {
        case TOOL_PENCIL: paint_stroke(rep, a, z, [NSColor blackColor], gLineWidth); break;
        case TOOL_BRUSH:  brush_stroke(rep, a, z); break;
        case TOOL_ERASER: erase_stroke(rep, a, z, 16); break;
        case TOOL_SPRAY:  spray_stroke(rep, a, z, gSprayRadius, gSprayDensity); break;
        case TOOL_LINE:   paint_shape(rep, TOOL_LINE, a, z, [NSColor blackColor], gLineWidth); break;
        case TOOL_RECT: case TOOL_OVAL: case TOOL_FREEFORM:
            if (gShapeFilled) fill_shape(rep, gTool, a, z);
            else paint_shape(rep, gTool, a, z, [NSColor blackColor], gLineWidth);
            break;

        case TOOL_SELRECT:
            /* Au lasso rectangulaire, un glissement ne dessine pas : il
             * SÉLECTIONNE. On pose directement l'état d'arrivée d'une
             * sélection faite à la souris, sans passer par gSelRectDrawing.
             *
             * Sans ce cas, le « drag from 0,0 to 169,341 » des scripts tombait
             * dans default, aucune sélection n'était établie, et le
             * « doMenu "Clear Picture" » qui suit effaçait TOUT le calque au
             * lieu de la seule bande visée — la courbe qu'on venait de tracer
             * disparaissait avec. */
            gSelStart = a;
            gSelEnd   = z;
            gSelRectDrawing = NO;
            gSelRectActive  = YES;
            [gView startAntsTimer];
            break;

        default:
            if (gSelected) {
                gSelected->x += x2 - x1;
                gSelected->y += y2 - y1;
            }
            break;
    }

    /* Ne rafraîchir que la zone touchée.
     *
     * Un tracé de script appelle cette fonction une fois par segment, et
     * v3_respire rend la main à AppKit à chaque tour de boucle : marquer TOUTE
     * la vue sale, c'était recomposer la carte entière — calque, champs,
     * boutons — une fois par segment. Sur une courbe de deux mille points, ça
     * se voit. HyperCard ne rafraîchissait que les pixels touchés, et c'est
     * pour cela que la pile d'origine paraît plus vive sous Basilisk.
     *
     * Deux exceptions gardent le rafraîchissement complet : la sélection, dont
     * le rectangle en pointillés bouge sur toute sa surface, et le déplacement
     * d'un objet, qui laisse un trou à son ancienne place. */
    if (gTool == TOOL_SELRECT || (gSelected && gTool != TOOL_BROWSE)) {
        hcv_invalide_tout();
        return;
    }

    NSRect sale = NSMakeRect(MIN(a.x, z.x), MIN(a.y, z.y),
                             fabs(z.x - a.x), fabs(z.y - a.y));

    /* De quoi couvrir l'épaisseur du trait, la largeur du pinceau et la
     * dispersion de l'aérographe. Large plutôt que juste : une marge de trop
     * ne coûte que quelques pixels, une marge manquante laisse une traînée. */
    CGFloat marge = gLineWidth + 2;
    if (gTool == TOOL_BRUSH)  marge = 24;
    if (gTool == TOOL_ERASER) marge = 20;
    if (gTool == TOOL_SPRAY)  marge = gSprayRadius + 4;

    hcv_invalide(NSInsetRect(sale, -marge, -marge));
}
static void cocoa_drag_old(int x1, int y1, int x2, int y2, const char *mods) {
    Object *card = hc_current_card();
    if (!card || !gView) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSRect b = [gView bounds];
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)b.size.width, (int)b.size.height);
    NSPoint a = NSMakePoint(x1, y1), z = NSMakePoint(x2, y2);
    if (mods_has(mods, "shift")) {
        z = constrain_to_axis(a, z);
    }
    switch (gTool) {
        case TOOL_PENCIL: paint_stroke(rep, a, z, [NSColor blackColor], gLineWidth); break;
        case TOOL_BRUSH:  brush_stroke(rep, a, z); break;
        case TOOL_ERASER: erase_stroke(rep, a, z, 16); break;
        case TOOL_SPRAY:  spray_stroke(rep, a, z, gSprayRadius, gSprayDensity); break;
        case TOOL_LINE:   paint_shape(rep, TOOL_LINE, a, z, [NSColor blackColor], gLineWidth); break;
        case TOOL_RECT: case TOOL_OVAL: case TOOL_FREEFORM:
            if (gShapeFilled) fill_shape(rep, gTool, a, z);
            else paint_shape(rep, gTool, a, z, [NSColor blackColor], gLineWidth);
            break;

        case TOOL_SELRECT:
            /* Au lasso rectangulaire, un glissement ne dessine pas : il
             * SÉLECTIONNE. On pose directement l'état d'arrivée d'une
             * sélection faite à la souris, sans passer par gSelRectDrawing.
             *
             * Sans ce cas, le « drag from 0,0 to 169,341 » des scripts tombait
             * dans default, aucune sélection n'était établie, et le
             * « doMenu "Clear Picture" » qui suit effaçait TOUT le calque au
             * lieu de la seule bande visée — la courbe qu'on venait de tracer
             * disparaissait avec. */
            gSelStart = a;
            gSelEnd   = z;
            gSelRectDrawing = NO;
            gSelRectActive  = YES;
            [gView startAntsTimer];
            break;

        default:
            if (gSelected) {
                gSelected->x += x2 - x1;
                gSelected->y += y2 - y1;
            }
            break;
    }
    [gView setNeedsDisplay:YES];
}

static void cocoa_click_at(int x, int y, const char *mods) {
    (void)mods;
    if (!gView) return;
    NSPoint p = NSMakePoint(x, y);

    Object *hit = part_at(hc_current_card(), p);
    gClickPoint = p;
    gClickField = (hit && hit->type == OBJ_FIELD) ? hit : NULL;
    gMouseClicked = YES;

    if (gTool == TOOL_TEXT) {
        [gView commitText];
        gTextPos = p;
        gTextBuf = [NSMutableString string];
        gTextActive = YES;
        [gView setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_FILL) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer,
                                    (int)[gView bounds].size.width,
                                    (int)[gView bounds].size.height);
        flood_fill(rep, (int)p.x, (int)p.y);
        hcv_invalide_tout();           /* le remplissage peut aller partout */
        return;
    }

    /* Un clic avec un outil de peinture POSE UN POINT.
     *
     * C'est la primitive de tracé point par point d'HyperCard : les scripts
     * qui dessinent sans relier leurs points — case « Connect » décochée dans
     * la pile des équations paramétriques — n'emploient que « click at ».
     * Sans ce cas, ils ne dessinaient rien du tout, en silence. */
    if (gTool == TOOL_PENCIL || gTool == TOOL_BRUSH  ||
        gTool == TOOL_ERASER || gTool == TOOL_SPRAY  ||
        gTool == TOOL_LINE   || gTool == TOOL_RECT   ||
        gTool == TOOL_OVAL   || gTool == TOOL_FREEFORM) {

        Object *card = hc_current_card();
        if (!card) return;
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer,
                                    (int)[gView bounds].size.width,
                                    (int)[gView bounds].size.height);

        CGFloat marge;
        switch (gTool) {
            case TOOL_BRUSH:
                brush_stroke(rep, p, p);
                marge = 24;
                break;
            case TOOL_ERASER:
                erase_stroke(rep, p, p, 16);
                marge = 20;
                break;
            case TOOL_SPRAY:
                spray_stroke(rep, p, p, gSprayRadius, gSprayDensity);
                marge = gSprayRadius + 4;
                break;
            default:
                /* Crayon, ligne et formes. On passe par paint_stroke plutôt
                 * que de peindre à la main : elle travaille dans le repère de
                 * la CARTE, y vers le bas, alors qu'un contexte bitmap a son
                 * origine en bas à gauche — un NSRectFill direct posait le
                 * point à la hauteur inverse.
                 *
                 * Un trait d'un pixel de long tient lieu de point, et évite le
                 * cas dégénéré d'un segment de longueur nulle, dont le rendu
                 * dépend du moteur. */
                paint_stroke(rep, p, NSMakePoint(p.x + 1, p.y),
                             [NSColor blackColor], gLineWidth);
                marge = gLineWidth + 2;
                break;
        }

        /* Ne rafraîchir que le point posé. Marquer toute la vue sale
         * recomposait la carte entière — calque, champs, boutons — une fois
         * par point, et un maillage de surface en compte des milliers. */
        hcv_invalide(NSMakeRect(p.x - marge, p.y - marge, 2 * marge, 2 * marge));
        return;
    }

    if (hit && gTool == TOOL_BROWSE) {
        hc_send(hit, "mouseDown");
        hc_send(hit, "mouseUp");
    }
    [gView setNeedsDisplay:YES];
}

static void cocoa_do_menu(const char *item) {
    if (!item || !gView) return;
 
    if (strcasecmp(item, "Clear Picture") == 0 ||
        strcasecmp(item, "Clear") == 0) {
        /* La sélection d'abord, tout le calque seulement s'il n'y en a pas. */
        if (gTool == TOOL_SELRECT && gSelRectActive) {
            Object *card = hc_current_card();
            if (!card) return;
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep =
                paint_bitmap(layer, (int)[gView bounds].size.width,
                                    (int)[gView bounds].size.height);
            [gView beginPaintUndo];
            erase_rect(rep, gSelStart, gSelEnd);
            gSelRectActive = NO;
            [gView stopAntsTimer];
            [gView setNeedsDisplay:YES];
            return;
        }
        [gView eraseAll];
        return;
    }

    if (strcasecmp(item, "Select All") == 0) {
        NSRect b = [gView bounds];
        gSelStart = NSMakePoint(0, 0);
        gSelEnd   = NSMakePoint(b.size.width, b.size.height);
        gSelRectActive = YES;
        [gView startAntsTimer];
        [gView setNeedsDisplay:YES];
        return;
    }

 
}

static void cocoa_type_text(const char *text, const char *mods) {
    (void)mods;
    if (!text || !gView) return;
    NSString *s = [NSString stringWithUTF8String:text];
    if (!s) return;

    if (gTool == TOOL_TEXT && gTextActive) {
        if (!gTextBuf) gTextBuf = [NSMutableString string];
        [gTextBuf appendString:s];
        [gView commitText];
        gView.needsDisplay = YES;
        return;
    }

    if (gEditingField && gFieldEditor) {
        [gFieldEditor insertText:s replacementRange:[gFieldEditor selectedRange]];
    } else if (gMsgBox) {
        [gMsgBox setStringValue:
            [[gMsgBox stringValue] stringByAppendingString:s]];
    }
    [gView setNeedsDisplay:YES];
}

static void cocoa_field_changed(Object *field)
{
    if (field && field == gEditingField && gFieldEditor) {
        const char *tx  = hc_field_text(field);
        NSString *noyau = [NSString stringWithUTF8String:tx ? tx : ""];

        if (![noyau isEqualToString:[gFieldEditor string]]) {
            NSRange sel = [gFieldEditor selectedRange];
            NSPoint org = [[gFieldScroll contentView] bounds].origin;

            NSDictionary *base = obj_attrs(field, 12, [NSColor blackColor]);
            gForEditor = YES;
            [[gFieldEditor textStorage]
                setAttributedString:field_attr_string(field, noyau, base)];
            gForEditor = NO;

            NSUInteger n = [[gFieldEditor string] length];
            if (sel.location > n) sel.location = n;
            if (sel.location + sel.length > n) sel.length = n - sel.location;
            [gFieldEditor setSelectedRange:sel];

            [[gFieldScroll contentView] scrollToPoint:org];
            [gFieldScroll reflectScrolledClipView:[gFieldScroll contentView]];
        }
    }
    [gView setNeedsDisplay:YES];
}

static BOOL gApplyingSelection = NO;

static void cocoa_selection_changed(Object *field, int start, int len) {
    if (gApplyingSelection) return;
    if (!gView) return;

    if (!field) {
        [gView setNeedsDisplay:YES];
        return;
    }

    gApplyingSelection = YES;

    if (field->locktext) {
        if (gEditingField && gEditingField != field) [gView endFieldEdit];
        gApplyingSelection = NO;
        [gView setNeedsDisplay:YES];
        return;
    }

    if (gEditingField != field) {
        [gView endFieldEdit];
        [gView beginFieldEdit:field];
    }

    if (gFieldEditor) {
        NSUInteger n = [[gFieldEditor string] length];
        NSUInteger s = (NSUInteger)(start < 0 ? 0 : start);
        NSUInteger l = (NSUInteger)(len   < 0 ? 0 : len);
        if (s > n)     s = n;
        if (s + l > n) l = n - s;
        [gFieldEditor setSelectedRange:NSMakeRange(s, l)];
        [gFieldEditor scrollRangeToVisible:NSMakeRange(s, l)];
    }

    gApplyingSelection = NO;
    [gView setNeedsDisplay:YES];
}

static char gGlobBuf[64];

static BOOL gSyncingEditorScroll = NO;

static CGFloat editor_course(void) {
    if (!gFieldScroll) return 0;
    CGFloat c = NSHeight([[gFieldScroll documentView] frame])
              - NSHeight([[gFieldScroll contentView] bounds]);
    return c > 0 ? c : 0;
}

static CGFloat editor_fraction(void) {
    CGFloat c = editor_course();
    if (c <= 0) return 0;
    CGFloat f = [[gFieldScroll contentView] bounds].origin.y / c;
    return f < 0 ? 0 : (f > 1 ? 1 : f);
}

static void sync_editor_scroll(Object *o) {
    if (!o || o != gEditingField || !gFieldScroll) return;

    NSView *doc = [gFieldScroll documentView];
    NSRect df = [doc frame];
    if (df.origin.y != 0 || df.origin.x != 0) {
        df.origin.x = 0;
        df.origin.y = 0;
        [doc setFrame:df];
    }

    CGFloat maxs = field_max_scroll(o);
    CGFloat pos  = maxs > 0 ? o->scroll / maxs : 0;
    if (pos < 0) pos = 0;
    if (pos > 1) pos = 1;

    gSyncingEditorScroll = YES;
    [[gFieldScroll contentView]
        scrollToPoint:NSMakePoint(0, floor(pos * editor_course()))];
    [gFieldScroll reflectScrolledClipView:[gFieldScroll contentView]];
    gSyncingEditorScroll = NO;
}

static int click_word_range(Object *f, NSPoint p, int *start, int *end) {
    if (!f || f->type != OBJ_FIELD) return 0;
    const char *tx = hc_field_text(f);
    if (!tx || !*tx) return 0;
    NSString *s = [NSString stringWithUTF8String:tx];
    if (!s) return 0;

    NSRect tr  = field_text_rect(f);
    NSRect off = field_text_draw_rect(f);

    /* Même mise en page que le tracé — voir click_line_number. */
    NSTextContainer *tc = nil;
    NSLayoutManager *lm = field_layout(f, s,
                                       obj_attrs(f, 12, [NSColor blackColor]),
                                       tr.size.width, &tc);

    NSPoint q = NSMakePoint(p.x - NSMinX(off), p.y - NSMinY(off));
    if (q.y < 0) return 0;
    NSRect used = [lm usedRectForTextContainer:tc];
    if (q.y > NSMaxY(used)) return 0;

    CGFloat frac = 0;
    NSUInteger gi = [lm glyphIndexForPoint:q inTextContainer:tc
                    fractionOfDistanceThroughGlyph:&frac];
    NSUInteger ci = [lm characterIndexForGlyphAtIndex:gi];
    NSUInteger n = [s length];
    if (ci >= n) ci = n ? n - 1 : 0;

    NSCharacterSet *blancs = [NSCharacterSet whitespaceAndNewlineCharacterSet];
    if ([blancs characterIsMember:[s characterAtIndex:ci]]) return 0;

    NSUInteger a = ci, b = ci;
    while (a > 0 && ![blancs characterIsMember:[s characterAtIndex:a - 1]]) a--;
    while (b + 1 < n && ![blancs characterIsMember:[s characterAtIndex:b + 1]]) b++;

    *start = (int)[[s substringToIndex:a] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    *end   = (int)[[s substringToIndex:b + 1] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    return 1;
}

static int click_line_number(Object *f, NSPoint p) {
    if (!f || f->type != OBJ_FIELD) return 0;

    const char *tx = hc_field_text(f);
    if (!tx || !*tx) return 0;
    NSString *s = [NSString stringWithUTF8String:tx];
    if (!s) return 0;

    NSRect tr  = field_text_rect(f);
    NSRect off = field_text_draw_rect(f);

    /* La MÊME mise en page que le tracé, et non une seconde montée pour
     * l'occasion.
     *
     * Deux raisons, et la seconde est celle qui se voit. La première : cette
     * fonction est appelée dans « repeat while the mouse is down », donc à
     * chaque tour de boucle, et remonter la mise en page d'un champ de
     * quarante-huit kilo-octets à chaque fois coûtait des centaines de
     * millisecondes par tour. La seconde : le tracé et le clic DOIVENT
     * s'accorder au pixel près, et deux mises en page montées séparément
     * finissent toujours par diverger — l'oubli de setUsesFontLeading:NO dans
     * field_layout a suffi à décaler la ligne surlignée. Une seule mise en
     * page, et la question ne se pose plus. */
    NSTextContainer *tc = nil;
    NSLayoutManager *lm = field_layout(f, s,
                                       obj_attrs(f, 12, [NSColor blackColor]),
                                       tr.size.width, &tc);

    NSPoint q = NSMakePoint(p.x - NSMinX(off), p.y - NSMinY(off));
    if (q.y < 0) return 0;

    CGFloat frac = 0;
    NSUInteger gi = [lm glyphIndexForPoint:q inTextContainer:tc
                    fractionOfDistanceThroughGlyph:&frac];
    NSUInteger ci = [lm characterIndexForGlyphAtIndex:gi];
    if (ci > [s length]) ci = [s length];

    NSRect used = [lm usedRectForTextContainer:tc];
    if (q.y > NSMaxY(used)) return 0;

    int line = 1;
    for (NSUInteger i = 0; i < ci; i++)
        if ([s characterAtIndex:i] == '\n') line++;
    return line;
}

static const char *cocoa_global_get(const char *name) {
    if (strcasecmp(name, "mouse") == 0)
        return ([NSEvent pressedMouseButtons] & 1) ? "down" : "up";

    if (strcasecmp(name, "optionKey") == 0)
        return ([NSEvent modifierFlags] & NSEventModifierFlagOption) ? "down" : "up";
    if (strcasecmp(name, "commandKey") == 0)
        return ([NSEvent modifierFlags] & NSEventModifierFlagCommand) ? "down" : "up";
    if (strcasecmp(name, "shiftKey") == 0)
        return ([NSEvent modifierFlags] & NSEventModifierFlagShift) ? "down" : "up";

    if (strcasecmp(name, "mouseH") == 0 || strcasecmp(name, "mouseV") == 0) {
        NSPoint s = [NSEvent mouseLocation];
        NSRect  w = [[gView window] convertRectFromScreen:
                        NSMakeRect(s.x, s.y, 0, 0)];
        NSPoint v = [gView convertPoint:w.origin fromView:nil];
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d",
                 (name[5] == 'H' || name[5] == 'h') ? (int)v.x : (int)v.y);
        return gGlobBuf;
    }

    if (strcasecmp(name, "screenRect") == 0) {
        NSRect r = [[NSScreen mainScreen] frame];
        snprintf(gGlobBuf, sizeof gGlobBuf, "0,0,%d,%d",
                 (int)r.size.width, (int)r.size.height);
        return gGlobBuf;
    }

    if (strcasecmp(name, "mouseClick") == 0) {
        BOOL eu = gMouseClicked;
        gMouseClicked = NO;
        return eu ? "true" : "false";
    }

    if (strcasecmp(name, "clickChunk") == 0) {
        static char buf[192];
        buf[0] = '\0';
        if (gClickField) {
            int s = 0, e = 0;
            if (click_word_range(gClickField, gClickPoint, &s, &e) && e > s) {
                char d[96];
                hc_describe(gClickField, d, sizeof d);
                snprintf(buf, sizeof buf, "char %d to %d of %s%s",
                         s + 1, e,
                         hc_owner_is_bg(gClickField) ? "bg " : "card ", d);
            }
        }
        return buf;
    }

    if (strcasecmp(name, "mouseLoc") == 0) {
        NSPoint s = [NSEvent mouseLocation];
        NSRect  w = [[gView window] convertRectFromScreen:
                        NSMakeRect(s.x, s.y, 0, 0)];
        NSPoint v = [gView convertPoint:w.origin fromView:nil];
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d,%d", (int)v.x, (int)v.y);
        return gGlobBuf;
    }

    if (strcasecmp(name, "ticks") == 0) {
        static NSTimeInterval t0 = 0;
        NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
        if (t0 == 0) t0 = now;
        snprintf(gGlobBuf, sizeof gGlobBuf, "%ld", (long)((now - t0) * 60.0));
        return gGlobBuf;
    }

    if (strcasecmp(name, "textHeight") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d",
                 gTextHeight > 0 ? gTextHeight
                                 : (int)[text_font() pointSize] + 3);
        return gGlobBuf;
    }
    if (strcasecmp(name, "textSize") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", (int)[text_font() pointSize]);
        return gGlobBuf;
    }
    if (strcasecmp(name, "textFont") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%s",
                 [[text_font() familyName] UTF8String]);
        return gGlobBuf;
    }
    if (strcasecmp(name, "textStyle") == 0)
        return gTextStyleName ? [gTextStyleName UTF8String] : "plain";
    if (strcasecmp(name, "textAlign") == 0)
        return gTextAlign ? [gTextAlign UTF8String] : "left";
    if (strcasecmp(name, "filled") == 0)
        return gShapeFilled ? "true" : "false";
    if (strcasecmp(name, "lineSize") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", gLineWidth);
        return gGlobBuf;
    }
    if (strcasecmp(name, "pattern") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", gPattern + 1);
        return gGlobBuf;
    }
    if (strcasecmp(name, "brush") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", gBrush + 1);
        return gGlobBuf;
    }

    if (strcasecmp(name, "tool") == 0) {
        const char *n = "browse";
        switch (gTool) {
            case TOOL_BROWSE:   n = "browse";    break;
            case TOOL_BUTTON:   n = "button";    break;
            case TOOL_FIELD:    n = "field";     break;
            case TOOL_SELRECT:  n = "select";    break;
            case TOOL_LASSO:    n = "lasso";     break;
            case TOOL_PENCIL:   n = "pencil";    break;
            case TOOL_BRUSH:    n = "brush";     break;
            case TOOL_ERASER:   n = "eraser";    break;
            case TOOL_LINE:     n = "line";      break;
            case TOOL_SPRAY:    n = "spray";     break;
            case TOOL_RECT:     n = "rectangle"; break;
            case TOOL_FILL:     n = "bucket";    break;
            case TOOL_OVAL:     n = "oval";      break;
            case TOOL_FREEFORM: n = "curve";     break;
            case TOOL_TEXT:     n = "text";      break;
                
            default: break;
        }
        snprintf(gGlobBuf, sizeof gGlobBuf, "%s tool", n);
        return gGlobBuf;
    }

    if (strcasecmp(name, "clickLoc") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d,%d",
                 (int)gClickPoint.x, (int)gClickPoint.y);
        return gGlobBuf;
    }
    if (strcasecmp(name, "clickH") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", (int)gClickPoint.x);
        return gGlobBuf;
    }
    if (strcasecmp(name, "clickV") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", (int)gClickPoint.y);
        return gGlobBuf;
    }
    if (strcasecmp(name, "clickLine") == 0) {
        static char buf[160];
        buf[0] = '\0';
        int line = click_line_number(gClickField, gClickPoint);
        if (gClickField && line > 0) {
            char desc[96];
            hc_describe(gClickField, desc, sizeof desc);
            snprintf(buf, sizeof buf, "line %d of %s%s", line,
                     hc_owner_is_bg(gClickField) ? "bg " : "card ", desc);
        }
        return buf;
    }
    if (strcasecmp(name, "mouseLine") == 0) {
        static char buf[160];
        buf[0] = '\0';
        NSPoint w = [[gView window] mouseLocationOutsideOfEventStream];
        NSPoint p = [gView convertPoint:w fromView:nil];
        Object *f = part_at(hc_current_card(), p);
        if (f && f->type == OBJ_FIELD) {
            int line = click_line_number(f, p);
            if (line > 0) {
                char desc[96];
                hc_describe(f, desc, sizeof desc);
                snprintf(buf, sizeof buf, "line %d of %s%s", line,
                         hc_owner_is_bg(f) ? "bg " : "card ", desc);
            }
        }
        return buf;
    }

    if (strcasecmp(name, "clickText") == 0) {
        static char buf[512];
        buf[0] = '\0';
        int line = click_line_number(gClickField, gClickPoint);
        if (gClickField && line > 0) {
            const char *t = hc_field_text(gClickField);
            int n = 1;
            const char *deb = t;
            while (n < line && (deb = strchr(deb, '\n'))) { deb++; n++; }
            if (deb) {
                const char *fin = strchr(deb, '\n');
                int len = fin ? (int)(fin - deb) : (int)strlen(deb);
                if (len > (int)sizeof buf - 1) len = (int)sizeof buf - 1;
                snprintf(buf, sizeof buf, "%.*s", len, deb);
            }
        }
        return buf;
    }
    return NULL;
}

static BOOL gCursorHidden = NO;

static void cocoa_global_set(const char *name, const char *value) {
    int vrai = (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0);

    /* Le verrou d'écran du noyau. Sans cette ligne, « lock screen » ne
     * retenait rien du dessin — voir hcv_verrou_ecran. */
    if (strcasecmp(name, "lockScreen") == 0) {
        hcv_verrou_ecran(vrai ? YES : NO);
        return;
    }

    if (strcasecmp(name, "filled") == 0) {
        gShapeFilled = vrai ? YES : NO;
        [gView setNeedsDisplay:YES];
        return;
    }
    if (strcasecmp(name, "lineSize") == 0) {
        int v = atoi(value);
        if (v < 1) v = 1;
        if (v > 8) v = 8;
        gLineWidth = v;
        [gView setNeedsDisplay:YES];
        return;
    }
    if (strcasecmp(name, "pattern") == 0) {
        int v = atoi(value);
        if (v < 1) v = 1;
        if (v > NUM_PATTERNS) v = NUM_PATTERNS;
        gPattern = v - 1;
        [gView setNeedsDisplay:YES];
        return;
    }
    if (strcasecmp(name, "brush") == 0) {
        int v = atoi(value);
        if (v < 1) v = 1;
        if (v > NUM_BRUSHES) v = NUM_BRUSHES;
        gBrush = v - 1;
        [gView setNeedsDisplay:YES];
        return;
    }
    if (strcasecmp(name, "textFont") == 0) {
        NSString *nom = [NSString stringWithUTF8String:value];
        NSFont *f = [NSFont fontWithName:nom size:gTextSize];
        if (f) gTextFont = f;
        return;
    }
    if (strcasecmp(name, "textSize") == 0) {
        int v = atoi(value);
        if (v < 4)  v = 4;
        if (v > 96) v = 96;
        gTextSize = v;
        NSFont *f = [NSFont fontWithName:[text_font() fontName] size:v];
        gTextFont = f ? f : [NSFont systemFontOfSize:v];
        return;
    }
    if (strcasecmp(name, "textHeight") == 0) {
        int v = atoi(value);
        if (v > 0) gTextHeight = v;
        return;
    }
    if (strcasecmp(name, "textStyle") == 0) {
        gTextStyleName = [NSString stringWithUTF8String:value];
        return;
    }
    if (strcasecmp(name, "textAlign") == 0) {
        gTextAlign = [NSString stringWithUTF8String:value];
        return;
    }
    if (strcasecmp(name, "cursor") == 0) {
        if (strcasecmp(value, "none") == 0) {
            if (!gCursorHidden) { [NSCursor hide]; gCursorHidden = YES; }
        } else {
            if (gCursorHidden) { [NSCursor unhide]; gCursorHidden = NO; }
            if (strcasecmp(value, "watch") == 0 || strcasecmp(value, "busy") == 0)
                [[NSCursor operationNotAllowedCursor] set];
            else if (strcasecmp(value, "ibeam") == 0)
                [[NSCursor IBeamCursor] set];
            else
                [[NSCursor arrowCursor] set];
        }
    }
}

void hc_restore_cursor(void) {
    if (gCursorHidden) { [NSCursor unhide]; gCursorHidden = NO; }
}

static NSMutableArray *gPlaying = nil;

@interface HCSoundKeeper : NSObject <NSSoundDelegate>
@end
@implementation HCSoundKeeper
- (void)sound:(NSSound *)s didFinishPlaying:(BOOL)ok {
    (void)ok;
    [gPlaying removeObject:s];
}
@end
static HCSoundKeeper *gSoundKeeper = nil;

static void cocoa_play(const char *name) {
    NSString *n = [NSString stringWithUTF8String:name ? name : ""];

    NSSound *s = [NSSound soundNamed:n];
    if (!s) {
        for (NSString *e in @[@"aiff", @"aif", @"wav"]) {
            NSString *p = [[NSBundle mainBundle] pathForResource:n ofType:e];
            if (p) { s = [[NSSound alloc] initWithContentsOfFile:p byReference:YES]; break; }
        }
    }
    if (!s) {
        NSFileManager *fm = [NSFileManager defaultManager];
        NSArray *dirs = @[[[NSBundle mainBundle] resourcePath],
                          [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Sounds"],
                          @"/Library/Sounds",
                          @"/System/Library/Sounds"];
        for (NSString *d in dirs) {
            if (!d) continue;
            for (NSString *f in [fm contentsOfDirectoryAtPath:d error:NULL]) {
                if ([[f stringByDeletingPathExtension] caseInsensitiveCompare:n] != NSOrderedSame)
                    continue;
                s = [[NSSound alloc] initWithContentsOfFile:
                        [d stringByAppendingPathComponent:f] byReference:YES];
                if (s) break;
            }
            if (s) break;
        }
    }
    if (!s) { NSBeep(); return; }

    if (!gPlaying)     gPlaying = [[NSMutableArray alloc] init];
    if (!gSoundKeeper) gSoundKeeper = [[HCSoundKeeper alloc] init];

    s = [s copy];
    [s setDelegate:gSoundKeeper];
    [gPlaying addObject:s];
    [s play];
}

static void cocoa_idle(void) {
    /* Le drapeau du noyau s'efface à la lecture : on le reporte tout de suite
     * sur la vue, sinon l'étranglement plus bas le consommerait sans rien
     * montrer. */
    if (hc_take_visual_dirty()) hcv_invalide_tout();

    /* Au plus soixante fois par seconde. Ce qui n'est pas montré maintenant
     * reste marqué sale et partira à l'image suivante : rien ne se perd, et la
     * boucle de script n'est pas ralentie par l'affichage. */
    static CFTimeInterval prochaine = 0;
    CFTimeInterval maintenant = CACurrentMediaTime();
    if (maintenant < prochaine) return;
    /* On avance d'un pas fixe, sans dériver ; si on a pris du retard, on
     * repart de maintenant plutôt que de rattraper en rafale. */
    prochaine += 1.0 / 60.0;
    if (prochaine < maintenant) prochaine = maintenant + 1.0 / 60.0;;

    /* Un tour de boucle d'événements, et non un simple display.
     *
     * Une partie de ce qui est visible ne vit pas dans drawRect: — le
     * surlignage d'un champ est dessiné par gFieldEditor, une NSTextView
     * adossée à un CALQUE. Marquer la vue sale ne suffit pas : le contenu d'un
     * calque n'atteint l'écran qu'à la validation de la transaction, que seule
     * la boucle d'événements déclenche.
     *
     * C'est aussi ce qui rend la fenêtre réactive pendant un long script :
     * les clics et les touches sont traités au passage. beforeDate:[NSDate
     * date] veut dire « ne dors pas si la file est vide ». */
    [[gView window] displayIfNeeded];
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate date]];
}

static NSFont *text_font(void) {
    if (!gTextFont) {
        gTextFont = [NSFont fontWithName:@"Helvetica" size:gTextSize];
        if (!gTextFont) gTextFont = [NSFont systemFontOfSize:gTextSize];
    }
 
    if (!gTextStyleName || [gTextStyleName length] == 0) return gTextFont;

    NSFontTraitMask traits = 0;
    NSString *s = [gTextStyleName lowercaseString];
    if ([s rangeOfString:@"bold"].location   != NSNotFound) traits |= NSBoldFontMask;
    if ([s rangeOfString:@"italic"].location != NSNotFound) traits |= NSItalicFontMask;

    if (traits) {
        NSFont *f = [[NSFontManager sharedFontManager]
                        convertFont:gTextFont toHaveTrait:traits];
        if (f) return f;
    }
    return gTextFont;
}

static NSDictionary *text_attrs(void) {
    NSColor *c = gInkColor;
    if (!c) c = (gInk == INK_WHITE) ? [NSColor whiteColor] : [NSColor blackColor];
    NSMutableDictionary *a = [NSMutableDictionary dictionary];
    a[NSFontAttributeName] = text_font();
    a[NSForegroundColorAttributeName] = c;
    if (gTextUnderline) a[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);
    return a;
}

static void stamp_text(NSBitmapImageRep *rep, NSString *s, NSPoint pos) {
    if (!rep || [s length] == 0) return;
    NSGraphicsContext *base = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!base) return;
    CGContextRef cg = [base CGContext];
    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithCGContext:cg flipped:YES];

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:YES];

    CGFloat H = [rep pixelsHigh];
    CGContextTranslateCTM(cg, 0, H);
    CGContextScaleCTM(cg, 1, -1);

    NSDictionary *attrs = text_attrs();
    NSFont *f = [attrs objectForKey:NSFontAttributeName];
    CGFloat montee = f ? [f ascender] : 0;

    CGFloat largeur = [s sizeWithAttributes:attrs].width;
    CGFloat x = pos.x;
    if (gTextAlign) {
        NSString *al = [gTextAlign lowercaseString];
        if      ([al isEqualToString:@"center"]) x -= largeur / 2;
        else if ([al isEqualToString:@"right"])  x -= largeur;
    }

    NSPoint haut = NSMakePoint(x, pos.y - montee);
    [s drawAtPoint:haut withAttributes:attrs];

    if (gTextStyleName &&
        [[gTextStyleName lowercaseString] rangeOfString:@"bold"].location
            != NSNotFound) {
        NSFont *f2 = [attrs objectForKey:NSFontAttributeName];
        NSString *nom = f2 ? [f2 fontName] : @"";
        if ([nom rangeOfString:@"Bold"].location == NSNotFound)
            [s drawAtPoint:NSMakePoint(haut.x + 1, haut.y) withAttributes:attrs];
    }
    [NSGraphicsContext restoreGraphicsState];
}

typedef struct { const char *glyph; int kind; int value; } ToolCell;

@interface SprayPreview : NSView
@end

@implementation SprayPreview
- (void)drawRect:(NSRect)dirty {
    [[NSColor whiteColor] setFill];
    NSRectFill([self bounds]);

    int w = (int)[self bounds].size.width, h = (int)[self bounds].size.height;
    if (w < 1 || h < 1) return;

    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL pixelsWide:w pixelsHigh:h
                    bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
                   colorSpaceName:NSCalibratedRGBColorSpace
                      bytesPerRow:0 bitsPerPixel:0];
    memset([rep bitmapData], 0, (size_t)[rep bytesPerRow] * h);

    for (int pass = 0; pass < 3; pass++) {
        NSPoint a = NSMakePoint(gSprayRadius + 4, h/2);
        NSPoint b = NSMakePoint(w - gSprayRadius - 4, h/2);
        spray_stroke(rep, a, b, gSprayRadius, gSprayDensity);
    }

    [rep drawInRect:[self bounds]];
    [[NSColor grayColor] setStroke];
    NSFrameRect([self bounds]);
}
@end

@implementation HCView {
    HCDoc _doc;
}

- (void *)docState { return &_doc; }

- (Object *)rememberedCard { return _doc.card; }

- (Object *)documentCard {
    if (gDoc == &_doc) {
        Object *c = hc_current_card();
        if (c) _doc.card = c;
        return c;
    }
    return _doc.card;
}

- (void)startAntsTimer {
    if (gAntsTimer) return;
    gAntsTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/15.0
                                                  target:self
                                                selector:@selector(antsTick:)
                                                userInfo:nil
                                                 repeats:YES];
}

- (void)stopAntsTimer {
    if (gAntsTimer) { [gAntsTimer invalidate]; gAntsTimer = nil; }
}

- (void)antsTick:(NSTimer *)t {
    if (!gSelRectActive && !gLassoActive) {
        [self stopAntsTimer];
        return;
    }
    gAntsPhase += 1.0;
    if (gAntsPhase >= 8.0) gAntsPhase = 0.0;
    [self setNeedsDisplay:YES];
}

- (void)updateWindowTitle {
    Object *card = [self documentCard];
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
    [gView setNeedsDisplay:YES];
}

- (BOOL)acceptsFirstResponder { return YES; }

- (void)showPopupMenuFor:(Object *)o atPoint:(NSPoint)p {
    if (!o->contents || !*o->contents) return;
    NSArray *lines = [[NSString stringWithUTF8String:o->contents]
                      componentsSeparatedByString:@"\n"];

    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

    NSMutableDictionary *iat = [obj_attrs(o, 12, nil) mutableCopy];
    [iat removeObjectForKey:NSForegroundColorAttributeName];

    for (NSUInteger i = 0; i < [lines count]; i++) {
        NSString *t = lines[i];
        if ([t length] == 0) continue;
        NSMenuItem *it = [[NSMenuItem alloc] initWithTitle:t
                                                    action:@selector(popupChosen:)
                                             keyEquivalent:@""];
        [it setAttributedTitle:
            [[NSAttributedString alloc] initWithString:t attributes:iat]];
        [it setTarget:self];
        [it setTag:(NSInteger)(i + 1)];
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
        hc_send(gPopupTarget, "mouseUp");
    }
    gPopupTarget = NULL;
    [self setNeedsDisplay:YES];
}

static BOOL object_selection_active(void)
{
    return (gSelected != NULL && gTool != TOOL_BROWSE) ? YES : NO;
}

static BOOL paint_selection_active(void)
{
    return ((gTool == TOOL_SELRECT && gSelRectActive) ||
            (gTool == TOOL_LASSO   && gLassoActive) ||
            gFloating) ? YES : NO;
}

- (void)copy:(id)sender {
    if (object_selection_active()) {
        if (hc_copy_part(gSelected)) return;
    }

    if (gFloating && gClipboard) {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        NSImage *img = [[NSImage alloc] initWithCGImage:[gClipboard CGImage] size:NSMakeSize(gClipW, gClipH)];
        [pb writeObjects:@[img]];
        return;
    }

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
    if (object_selection_active()) {
        if (gSelected == gEditingField) [self endFieldEdit];
        if (hc_cut_part(gSelected)) {
            gSelected = NULL;
            [gView setNeedsDisplay:YES];
            return;
        }
    }

    if (gFloating && gClipboard) {
        [self copy:sender];
        gFloating = NO;
        gFloatDragging = NO;
        [gView setNeedsDisplay:YES];
        return;
    }

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
    [gView setNeedsDisplay:YES];
}

/* ==================== cartes : couper, copier, dupliquer ====================
 *
 * Articles de menu DISTINCTS de Couper et Copier, comme dans HyperCard. Les
 * partager rendrait Cmd-C imprévisible : copy: a déjà trois sens selon le
 * contexte — objet sélectionné, sélection flottante, région de peinture — et
 * la carte en ferait un quatrième dans l'état le plus courant, outil Browse et
 * rien de sélectionné.
 *
 * Coller, lui, reste commun : il se contente de poser ce que le presse-papiers
 * contient. */

- (void)copyCard:(id)sender {
    (void)sender;
    Object *card = hc_current_card();
    if (!card) return;
    /* Même raison : on copie ce qui est à l'écran, pas ce qui dormait dans le
     * noyau depuis le dernier enregistrement. */
    [self flushPaintToKernel];
    hc_copy_card(card);
}

- (void)cutCard:(id)sender {
    (void)sender;
    Object *card = hc_current_card();
    if (!card) return;

    /* Lâcher ce que la vue retient de cette carte AVANT qu'elle soit libérée :
     * champ en édition, objet sélectionné, ET le cache de peinture.
     *
     * Ce dernier est indexé par POINTEUR d'objet, et flushPaintToKernel appelle
     * hc_set_paint sur chacune de ses clés. Une carte coupée mais laissée dans
     * le cache faisait donc appeler free sur un objet rendu — plantage dans
     * free, loin de sa cause.
     *
     * On vide APRÈS le flush : ce que la carte avait de peint doit d'abord
     * redescendre dans le noyau, faute de quoi la copie au presse-papiers
     * emporterait un calque périmé. */
    if (gEditingField) [self endFieldEdit];
    gSelected = NULL;
    [self flushPaintToKernel];
    [self clearPaintCache];

    if (!hc_cut_card(card)) {
        /* hc_delete_card refuse la dernière carte d'une pile. */
        NSBeep();
        return;
    }
    [gView setNeedsDisplay:YES];
}

- (void)duplicateCard:(id)sender {
    (void)sender;
    Object *card = hc_current_card();
    if (!card) return;

    /* Redescendre la peinture avant de cloner : sans cela le clone emporterait
     * le calque tel qu'il était au dernier enregistrement, et non ce que l'on
     * voit à l'écran. */
    [self flushPaintToKernel];

    Object *nouvelle = hc_duplicate_card(card);
    if (!nouvelle) { NSBeep(); return; }

    hc_set_current_card(nouvelle);
    gSelected = NULL;
    hc_send(nouvelle, "newCard");
    [gView setNeedsDisplay:YES];
}

- (void)paste:(id)sender {
    /* Une carte d'abord : c'est la nature du presse-papiers qui décide, pas
     * l'outil courant. Déduire du contexte se tromperait dès qu'une carte a
     * été copiée puis l'outil Bouton choisi. */
    if (hc_clipboard_has_card()) {
        Object *card = hc_current_card();
        Object *stack = card ? card->owner : NULL;
        Object *nouvelle = stack ? hc_paste_card(stack) : NULL;
        if (nouvelle) {
            /* Coller a pu apporter des icônes de l'autre pile. Le catalogue de
             * travail doit être refait ici : hcicon_edit_bind se contente de
             * comparer les pointeurs, et cette pile étant déjà liée, il ne
             * verrait rien changer — les boutons resteraient vides jusqu'au
             * prochain changement de fenêtre. */
            hcicon_edit_sync(stack);

            hc_set_current_card(nouvelle);
            gSelected = NULL;
            hc_send(nouvelle, "newCard");
            [gView setNeedsDisplay:YES];
        } else {
            /* Seul échec possible ici : le fond de la carte copiée n'existe
             * pas dans cette pile. On le dit, plutôt que de ne rien faire. */
            NSAlert *a = [[NSAlert alloc] init];
            [a setMessageText:@"Impossible de coller cette carte"];
            [a setInformativeText:@"Son fond n'existe pas dans cette pile."];
            [a runModal];
        }
        return;
    }

    if ((gTool == TOOL_BUTTON || gTool == TOOL_FIELD) && hc_clipboard_has_part()) {
        Object *card = hc_current_card();
        if (card) {
            Object *owner = (gEditBackground && card->bg) ? card->bg : card;
            Object *p = hc_paste_part(owner);
            if (p) {
                gSelected = p;
                hc_send(p, p->type == OBJ_BUTTON ? "newButton" : "newField");
                [gView setNeedsDisplay:YES];
                return;
            }
        }
    }

    if (gFloating) {
        [self dropFloating];
    }

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
        [gView setNeedsDisplay:YES];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    SEL a = [item action];

    if (a == @selector(togglePalette:)) {
        [item setState:[self paletteVisibleForTag:[item tag]]
                        ? NSControlStateValueOn : NSControlStateValueOff];
        return YES;
    }

    if (a == @selector(copy:) || a == @selector(cut:))
        return object_selection_active() || paint_selection_active();
    /* « Icône… » reste TOUJOURS disponible, même sans bouton sélectionné : le
     * panneau gère les icônes de la pile, qui sont des ressources. On peut donc
     * en créer et en dessiner avec n'importe quel outil ; faute de bouton, OK
     * referme sans rien attribuer. */
    if (a == @selector(paste:)) {
        /* Une carte se colle quel que soit l'outil : c'est la nature du
         * presse-papiers qui commande, comme dans paste: lui-même. Sans cette
         * branche, l'article restait grisé après « Copier la carte » — la
         * validation n'autorisait que les outils Bouton et Champ. */
        if (hc_clipboard_has_card())
            return YES;
        if ((gTool == TOOL_BUTTON || gTool == TOOL_FIELD) && hc_clipboard_has_part())
            return YES;
        return gClipboard != nil ||
               [[NSPasteboard generalPasteboard] canReadObjectForClasses:@[[NSImage class]] options:nil];
    }
    return YES;
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self setNeedsDisplay:YES];
}

- (void)print:(id)sender {
    (void)sender;
    
    Object *card = [self documentCard];
     
    if (!card) return;
    Object *tab[1] = { card };
    cocoa_print_cards(tab, 1);
}

- (void)changeColor:(id)sender {
     
    NSColorPanel *panneau = (NSColorPanel *)sender;
    if (![panneau respondsToSelector:@selector(color)]) return;
    NSColor *c = [panneau color];

    if (gColorTarget) {
        if (gColorTarget == 1) gInkColor  = c;
        else                   gBackColor = c;
        [(NSView *)[gToolPanel contentView] display];
        [gView setNeedsDisplay:YES];
        return;
    }

    if (gEditingField && gFieldEditor) {
        NSRange r = [gFieldEditor selectedRange];
        if (r.length > 0) {
            [[gFieldEditor textStorage]
                addAttribute:NSForegroundColorAttributeName value:c range:r];
        } else {
            NSMutableDictionary *ta =
                [[gFieldEditor typingAttributes] mutableCopy];
            ta[NSForegroundColorAttributeName] = c;
            [gFieldEditor setTypingAttributes:ta];
        }
        [gView setNeedsDisplay:YES];
        return;
    }

    gInkColor = c;
    [gView setNeedsDisplay:YES];
}

- (void)underline:(id)sender {
    gTextUnderline = !gTextUnderline;
    [gView setNeedsDisplay:YES];
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
    Object *tgt = gFontTarget ? gFontTarget
                    : (gEditingField ? gEditingField
                    : ((gSelected && gTool != TOOL_BROWSE) ? gSelected : NULL));

    NSFont *nf = [sender convertFont:(tgt ? obj_font(tgt, 12) : text_font())];

    if (tgt) {
            NSFontManager *fm = [NSFontManager sharedFontManager];
            NSFontTraitMask tr = [fm traitsOfFont:nf];

            NSFontDescriptor *pd =
                [[nf fontDescriptor] fontDescriptorWithSymbolicTraits:
                    [[nf fontDescriptor] symbolicTraits] &
                    ~(NSFontDescriptorTraitBold | NSFontDescriptorTraitItalic)];
            NSFont *plain = [NSFont fontWithDescriptor:pd size:[nf pointSize]];
            if (!plain) {
                plain = nf;
                if (tr & NSBoldFontMask)
                    plain = [fm convertFont:plain toNotHaveTrait:NSBoldFontMask];
                if (tr & NSItalicFontMask)
                    plain = [fm convertFont:plain toNotHaveTrait:NSItalicFontMask];
            }
            if (!plain) plain = nf;

            free(tgt->textfont);
            NSString *fam = [plain familyName];
            if (!fam || [fam hasPrefix:@"."]) {
                tgt->textfont = NULL;
            } else {
                tgt->textfont = strdup([fam UTF8String]);
            }
            tgt->textsize = (int)[nf pointSize];

            int st = tgt->textstyle & ~(HC_BOLD | HC_ITALIC);
            if (tr & NSBoldFontMask)   st |= HC_BOLD;
            if (tr & NSItalicFontMask) st |= HC_ITALIC;
            tgt->textstyle = st;

            if (tgt == gEditingField && gFieldEditor)
                [gFieldEditor setFont:obj_font(tgt, 12)];

            [fm setSelectedFont:obj_font(tgt, 12) isMultiple:NO];
            hc_sync_size_field(tgt);
        } else {
        gTextFont = nf;
        gTextSize = (int)[nf pointSize];
    }
    [[NSFontManager sharedFontManager] setSelectedFont:nf isMultiple:NO];
    [gView setNeedsDisplay:YES];
}

static int gColorTarget = 0;

- (void)showDrawColorPanel:(BOOL)ink {
    gColorTarget = ink ? 1 : 2;
    NSColorPanel *p = [NSColorPanel sharedColorPanel];
    [p setColor:(ink ? gInkColor : gBackColor)];
    [[self window] makeFirstResponder:self];
    [p orderFront:nil];
}

- (void)changeAttributes:(id)sender {
    [gView setNeedsDisplay:YES];
}

- (NSFontPanelModeMask)validModesForFontPanel:(NSFontPanel *)fontPanel {
    return NSFontPanelModesMaskStandardModes;
}

- (void)ditherSelection:(id)sender {
    if (gFloating && gClipboard) {
        dither_region(gClipboard, 0, 0, gClipW-1, gClipH-1, NULL, 0);
        [gView setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (gSelRectActive) {
        dither_region(rep, (int)MIN(gSelStart.x,gSelEnd.x), (int)MIN(gSelStart.y,gSelEnd.y),
                           (int)MAX(gSelStart.x,gSelEnd.x), (int)MAX(gSelStart.y,gSelEnd.y),
                           NULL, 0);
        [gView setNeedsDisplay:YES];
        return;
    }
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
        [gView setNeedsDisplay:YES];
        return;
    }
    dither_region(rep, 0, 0, (int)[rep pixelsWide]-1, (int)[rep pixelsHigh]-1, NULL, 0);
    [gView setNeedsDisplay:YES];
}

- (void)popupFlashTick:(NSTimer *)timer {
    if (!gPopupTarget || gPopupChosenRow < 0) { close_popup_menu(); return; }
    gPopupFlashInverted = !gPopupFlashInverted;
    if (--gPopupFlashToggles <= 0) {
        NSInteger row = gPopupChosenRow;
        [timer invalidate];
        gPopupFlashTimer = nil;
        choose_popup_row(self, row);
        return;
    }
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)event {
    unichar key = [[event charactersIgnoringModifiers] characterAtIndex:0];
    NSUInteger mods = [event modifierFlags];
    BOOL cmd = (mods & NSEventModifierFlagCommand) != 0;

    if (gPopupTarget && !cmd) {
        if (gPopupFlashTimer) return;
        NSInteger count = gPopupItems.count;
        if (key == 27) { close_popup_menu(); [self setNeedsDisplay:YES]; return; }
        if (key == NSUpArrowFunctionKey) {
            NSInteger row = gPopupKeyboardRow < 0 ? count - 1 : gPopupKeyboardRow - 1;
            while (row >= 0 && !popup_row_is_enabled(row)) row--;
            if (row >= 0) { gPopupKeyboardRow = row; [self setNeedsDisplay:YES]; }
            return;
        }
        if (key == NSDownArrowFunctionKey) {
            NSInteger row = gPopupKeyboardRow < 0 ? 0 : gPopupKeyboardRow + 1;
            while (row < count && !popup_row_is_enabled(row)) row++;
            if (row < count) { gPopupKeyboardRow = row; [self setNeedsDisplay:YES]; }
            return;
        }
        if ((key == NSEnterCharacter || key == NSCarriageReturnCharacter) &&
            popup_row_is_enabled(gPopupKeyboardRow)) {
            flash_popup_selection(self, gPopupKeyboardRow); return;
        }
        return;
    }

    if (gFloating && !cmd) {
        if (key == NSEnterCharacter || key == NSCarriageReturnCharacter) {
            [self dropFloating];
            return;
        }
        /* Échap ou Suppression abandonnent la zone flottante.
         *
         * Elle n'est ni gSelRectActive ni gLassoActive — ces drapeaux ont été
         * baissés au soulèvement — donc les cas de suppression plus bas ne la
         * voyaient pas, et la touche restait sans effet.
         *
         * Le geste sert surtout après un Option-glisser : on duplique, on
         * change d'avis, et l'on jette la copie sans avoir à la déposer puis
         * à la resélectionner. Après un glissement ordinaire, le calque a
         * déjà été effacé au soulèvement : abandonner supprime alors vraiment
         * la zone, ce qui est bien ce qu'on attend de Suppression. */
        if (key == 27 || key == NSDeleteCharacter || key == NSDeleteFunctionKey) {
            gFloating = NO;
            gFloatDragging = NO;
            [self stopAntsTimer];
            [self setNeedsDisplay:YES];
            return;
        }
    }

    if (gTool == TOOL_TEXT && gTextActive && !cmd) {
        if (key == 27) {
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
        [self stopAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gTool == TOOL_SELRECT && gSelRectActive) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                    (int)[self bounds].size.height);
        erase_rect(rep, gSelStart, gSelEnd);
        gSelRectActive = NO;
        [self stopAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

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
     
}

- (BOOL)isFlipped { return YES; }

- (void)applyStackSize {
    Object *card = [self documentCard];
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;
    int w = stack->w > 0 ? stack->w : 512;
    int h = stack->h > 0 ? stack->h : 342;

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
    int cols = 4, rows = 3;
    CGFloat cell = 40, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;
    gWidthPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(200, 150, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered defer:NO];
    [gWidthPanel setTitle:@"Épaisseur"];
    [gWidthPanel setFloatingPanel:YES];
    [gWidthPanel setBecomesKeyOnlyIfNeeded:YES];
    [gWidthPanel setHidesOnDeactivate:YES];
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
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered defer:NO];
    [gBrushPanel setTitle:@"Pinceaux"];
    [gBrushPanel setFloatingPanel:YES];
    [gBrushPanel setBecomesKeyOnlyIfNeeded:YES];
    [gBrushPanel setHidesOnDeactivate:YES];
    [gBrushPanel setReleasedWhenClosed:NO];
    BrushPalette *grid = [[BrushPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gBrushPanel setContentView:grid];
    [gBrushPanel makeKeyAndOrderFront:nil];
}

- (void)eraseAll {
    Object *card = [self documentCard];
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;

    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (!rep) return;

    [self beginPaintUndo];

    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    CGContextClearRect([ctx CGContext],
                       CGRectMake(0, 0, [rep pixelsWide], [rep pixelsHigh]));
    [NSGraphicsContext restoreGraphicsState];

    [self setNeedsDisplay:YES];
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
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [gPatternPanel setTitle:@"Motifs"];
    [gPatternPanel setFloatingPanel:YES];
    [gPatternPanel setBecomesKeyOnlyIfNeeded:YES];
    [gPatternPanel setHidesOnDeactivate:YES];
    PatternPalette *grid = [[PatternPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gPatternPanel setContentView:grid];
    [gPatternPanel makeKeyAndOrderFront:nil];
}

/* Ne composer que la portion du calque qui recoupe la zone sale.
 *
 * Composer le calque ENTIER à chaque redessin coûtait une vingtaine de
 * millisecondes, deux fois — celui du fond et celui de la carte. Sur un tracé
 * de plusieurs milliers de segments, c'est là que passait tout le temps, et
 * c'est pourquoi la pile d'origine paraissait plus vive sous Basilisk :
 * QuickDraw ne rafraîchissait que les pixels touchés.
 *
 * La vue est retournée, origine en haut ; un NSBitmapImageRep ne l'est pas.
 * Le rectangle source se déduit donc par symétrie verticale. */
static void draw_layer_dirty(NSBitmapImageRep *rep, NSRect sale) {
    if (!rep) return;
    CGFloat W = [rep pixelsWide], H = [rep pixelsHigh];
    NSRect dest = NSIntersectionRect(NSIntegralRect(sale), NSMakeRect(0, 0, W, H));
    if (NSIsEmptyRect(dest)) return;
    NSRect src = NSMakeRect(dest.origin.x,
                            H - (dest.origin.y + dest.size.height),
                            dest.size.width, dest.size.height);
    [rep drawInRect:dest fromRect:src
          operation:NSCompositingOperationSourceOver fraction:1.0
     respectFlipped:YES hints:nil];
}

- (void)drawRect:(NSRect)dirtyRect {
    /* Lier le catalogue d'icônes à la pile de CETTE fenêtre, avant tout dessin.
     *
     * HCicons ne retient qu'une copie de travail, alors que plusieurs piles
     * peuvent être ouvertes en même temps : c'est donc la fenêtre en train de
     * se dessiner qui doit imposer la sienne. Lier une fois au chargement ne
     * suffirait pas — la seconde pile ouverte écraserait le catalogue de la
     * première, qui afficherait alors de mauvaises icônes.
     *
     * Sans travail quand c'est déjà la bonne pile, donc gratuit au redessin. */
    {
        Object *dc = [self documentCard];
        hcicon_edit_bind(dc ? dc->owner : NULL);
    }

    if (visual_pending()) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self runVisualTransition];
        });
    }

    if ([self drawVisualStep]) return;

    [[NSColor whiteColor] setFill];
    NSRectFill(dirtyRect);

    Object *card = [self documentCard];
    if (!card) return;

    NSRect b = [self bounds];

    if (card->bg) {
        draw_layer_dirty(paint_bitmap(card->bg, (int)b.size.width, (int)b.size.height),
                         dirtyRect);
        for (int i = 0; i < card->bg->nparts; i++)
            if (part_touche(card->bg->parts[i], dirtyRect))
                draw_part(card->bg->parts[i]);
    }

    if (!gEditBackground) {
        draw_layer_dirty(paint_bitmap(card, (int)b.size.width, (int)b.size.height),
                         dirtyRect);
        for (int i = 0; i < card->nparts; i++)
            if (part_touche(card->parts[i], dirtyRect))
                draw_part(card->parts[i]);
    }

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

    if (gDragging) {
        [[NSColor blueColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:gDragRect];
        [path setLineWidth:1];
        CGFloat dash[] = {4, 3};
        [path setLineDash:dash count:2 phase:0];
        [path stroke];
    }

    if (gEditBackground) {
        [[NSColor colorWithRed:0.6 green:0.4 blue:0.2 alpha:1.0] setStroke];
        NSBezierPath *frame = [NSBezierPath bezierPathWithRect:NSInsetRect(b, 4, 4)];
        [frame setLineWidth:4];
        CGFloat dash[] = {10, 5};
        [frame setLineDash:dash count:2 phase:0];
        [frame stroke];
    }

    /* Aperçu dynamique des formes géométriques */
    if (gShapeDrawing) {
        BOOL optionDown = ([NSEvent modifierFlags] & NSEventModifierFlagOption) != 0;
        NSRect box = compute_shape_rect(gShapeStart, gShapeEnd, optionDown);

        [[NSColor blueColor] setStroke];
        NSBezierPath *preview = nil;
        if (gTool == TOOL_LINE) {
            preview = [NSBezierPath bezierPath];
            if (optionDown) {
                NSPoint opposite = NSMakePoint(2 * gShapeStart.x - gShapeEnd.x,
                                               2 * gShapeStart.y - gShapeEnd.y);
                [preview moveToPoint:opposite];
            } else {
                [preview moveToPoint:gShapeStart];
            }
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

    /* Lasso avec animation fourmis de feu */
    if ((gLassoDrawing || gLassoActive) && gLassoCount > 1) {
        [[NSColor blackColor] setStroke];
        NSBezierPath *pv = [NSBezierPath bezierPath];
        [pv moveToPoint:gLassoPts[0]];
        for (int i = 1; i < gLassoCount; i++) [pv lineToPoint:gLassoPts[i]];
        if (gLassoActive) [pv closePath];
        [pv setLineWidth:1];
        CGFloat dash[] = {4, 4};
        [pv setLineDash:dash count:2 phase:gAntsPhase];
        [pv stroke];
    }

    /* Sélection rectangulaire avec animation fourmis de feu */
    if (gSelRectDrawing || gSelRectActive) {
        NSRect sel = NSMakeRect(MIN(gSelStart.x,gSelEnd.x), MIN(gSelStart.y,gSelEnd.y),
                                fabs(gSelEnd.x-gSelStart.x), fabs(gSelEnd.y-gSelStart.y));
        [[NSColor blackColor] setStroke];
        NSBezierPath *pv = [NSBezierPath bezierPathWithRect:sel];
        [pv setLineWidth:1];
        CGFloat dash[] = {4, 4};
        [pv setLineDash:dash count:2 phase:gAntsPhase];
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
        CGFloat dash[] = {4, 4};
        [fp setLineDash:dash count:2 phase:gAntsPhase];
        [fp stroke];
    }

    if (gTextActive && gTextBuf) {
        NSDictionary *at = text_attrs();

        /* L'aperçu doit se placer comme la GRAVURE : à la ligne de base.
         * stamp_text a été corrigée pour poser le texte sur gTextPos, cet
         * aperçu non — d'où un texte affiché une ascendante trop haut pendant
         * la frappe, qui redescendait d'un coup à la validation. */
        NSFont *fp = [at objectForKey:NSFontAttributeName];
        CGFloat montee = fp ? [fp ascender] : 0;
        NSPoint haut = NSMakePoint(gTextPos.x, gTextPos.y - montee);

        [gTextBuf drawAtPoint:haut withAttributes:at];

        NSArray *lines = [gTextBuf componentsSeparatedByString:@"\n"];
        NSString *last = [lines lastObject];
        NSSize lastSz = [last sizeWithAttributes:at];
        NSSize oneLine = [@"Ag" sizeWithAttributes:at];

        /* Le curseur suit le même décalage, sans quoi il flotterait au-dessus
         * du texte qu'il est censé suivre. */
        CGFloat cy = haut.y + ([lines count] - 1) * oneLine.height;

        [[NSColor blackColor] setStroke];
        NSBezierPath *caret = [NSBezierPath bezierPath];
        [caret moveToPoint:NSMakePoint(gTextPos.x + lastSz.width, cy)];
        [caret lineToPoint:NSMakePoint(gTextPos.x + lastSz.width, cy + oneLine.height)];
        [caret setLineWidth:1];
        [caret stroke];
    }

    draw_popup_menu();
}

- (void)toggleBackground:(id)sender {
    gEditBackground = !gEditBackground;
    gSelected = NULL;
    [gView endFieldEdit];
    [gView setNeedsDisplay:YES];
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

- (Object *)paintLayer {
    Object *card = [self documentCard];
    if (!card) return NULL;
    Object *layer = gEditBackground ? card->bg : card;
    return layer ? layer : card;
}

- (void)beginPaintUndo {
    Object *layer = [self paintLayer];
    if (!layer) return;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (!rep) return;
    gPaintUndo      = paint_copy(rep);
    gPaintUndoLayer = layer;
}

- (void)undo:(id)sender {
    (void)sender;
    if (!gPaintUndo || !gPaintUndoLayer) { NSBeep(); return; }
    NSBitmapImageRep *rep = paint_bitmap(gPaintUndoLayer,
                                         (int)[self bounds].size.width,
                                         (int)[self bounds].size.height);
    if (!rep) return;
    paint_swap(rep, gPaintUndo);
    [self setNeedsDisplay:YES];
}

- (void)clearPaintCache {
    [gPaintCache removeAllObjects];
}

- (void)resetForNewStack {
    if (gEditingField) [self endFieldEdit];
    [self dropFloating];
    close_popup_menu();
    hc_set_selection(NULL, 0, 0);
    [self stopSprayTimer];
    [self stopAntsTimer];

    HCDoc vide = {0};
    *gDoc = vide;

    gSelected       = NULL;
    gResizeHandle   = 0;
    gDragging       = NO;
    gPenDrawing     = NO;
    gLockedAxis     = AXIS_NONE;
    gSelRectActive  = NO;
    gSelRectDrawing = NO;
    gLassoActive    = NO;
    gLassoCount     = 0;
    gFloatDragging  = NO;
}

- (void)flushPaintToKernel {
    if (!gPaintCache) return;

    NSMutableArray *mortes = [NSMutableArray array];

    for (NSValue *key in gPaintCache) {
        Object *o = [key pointerValue];

        /* Le cache est indexé par POINTEUR et rien ne l'invalide : une carte
         * coupée, une pile fermée, et l'adresse y reste. hc_set_paint ferait
         * alors free sur de la mémoire rendue — d'où un plantage à
         * l'enregistrement, très loin de sa cause, puisque le flush n'a lieu
         * qu'à ce moment-là.
         *
         * On demande donc au noyau, qui tient le registre des piles ouvertes.
         * Il ne déréférence pas l'adresse douteuse, il la compare aux objets
         * vivants. */
        if (!hc_layer_is_live(o)) {
            NSLog(@"[flush] o=%p calque mort, ignore", o);
            [mortes addObject:key];
            continue;
        }

        NSBitmapImageRep *rep = [gPaintCache objectForKey:key];
        if (!rep) continue;

        NSInteger w = [rep pixelsWide], h = [rep pixelsHigh];

        /* On recopie les octets tels quels : `fresh` doit donc porter le meme
           espace colorimetrique et le meme format alpha que `rep`. Figer
           NSCalibratedRGBColorSpace ici re-etiquetait les pixels sans les
           convertir, alors que le chargement, lui, convertissait pour de bon.
           C'est cette asymetrie qui rongeait les couleurs cycle apres cycle.
           Le calque vit desormais en sRGB (cf. paint_bitmap) et l'on grave en
           sRGB : l'aller-retour est l'identite au bit pres. */
        NSBitmapImageRep *fresh = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL
                          pixelsWide:w pixelsHigh:h
                       bitsPerSample:8 samplesPerPixel:4
                            hasAlpha:YES isPlanar:NO
                      colorSpaceName:[rep colorSpaceName]
                        bitmapFormat:([rep bitmapFormat]
                                      & NSBitmapFormatAlphaNonpremultiplied)
                         bytesPerRow:w * 4 bitsPerPixel:32];

        unsigned char *src = [rep bitmapData];
        unsigned char *dst = [fresh bitmapData];
        NSInteger sbpr = [rep bytesPerRow], dbpr = [fresh bytesPerRow];
        NSInteger spp  = [rep samplesPerPixel];
        /* Pas en octets : samplesPerPixel n'est pas le pas des lors qu'une rep
           a 3 echantillons alignes sur 32 bits. */
        NSInteger sstride = [rep bitsPerPixel] / 8;
        NSInteger dstride = [fresh bitsPerPixel] / 8;
        for (NSInteger y = 0; y < h; y++) {
            for (NSInteger x = 0; x < w; x++) {
                unsigned char *sp = src + y*sbpr + x*sstride;
                unsigned char *dp = dst + y*dbpr + x*dstride;
                dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2];
                dp[3] = (spp >= 4) ? sp[3] : 255;
            }
        }

        /* colorSpaceName ne transporte qu'un nom generique : on rattache le
           profil ICC exact de la source. Retagging = on change l'etiquette,
           jamais les valeurs. A faire une fois les pixels ecrits, la methode
           renvoyant une nouvelle rep. */
        NSColorSpace *cs = [rep colorSpace];
        if (!cs) cs = [NSColorSpace sRGBColorSpace];
        NSBitmapImageRep *tagged =
            [fresh bitmapImageRepByRetaggingWithColorSpace:cs];
        if (tagged) fresh = tagged;

        NSString *b64 = hcp_encode(fresh);
        if (!b64) { NSLog(@"[flush] o=%p ECHEC encodage", o); continue; }
        NSLog(@"[flush] o=%p grave %ldx%ld, %lu car.", o, (long)w, (long)h,
              (unsigned long)[b64 length]);
        hc_set_paint(o, [b64 UTF8String]);
    }

    /* Hors de la boucle : on ne modifie pas une table qu'on parcourt. */
    for (NSValue *key in mortes) [gPaintCache removeObjectForKey:key];
}

- (void)mouseMoved:(NSEvent *)event {
    if (!gPopupTarget || gPopupFlashTimer) return;
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    NSInteger row = popup_row_at_point(p);
    if (row == gPopupKeyboardRow) return;
    gPopupKeyboardRow = row;
    [self setNeedsDisplay:YES];
}

- (NSView *)hitTest:(NSPoint)point {
    if (gTool != TOOL_BROWSE && gEditingField) {
        [self endFieldEdit];
        [[self window] makeFirstResponder:self];
    }
    return [super hitTest:point];
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    /* 1. Menus Popup */
    if (gPopupTarget) {
        if (gPopupFlashTimer) return;
        NSInteger row = popup_row_at_point(p);
        if (row >= 0) {
            flash_popup_selection(self, row);
        } else {
            close_popup_menu();
            [self setNeedsDisplay:YES];
        }
        return;
    }

    Object *hit = part_at(hc_current_card(), p);

    gClickPoint = p;
    gClickField = (hit && hit->type == OBJ_FIELD) ? hit : NULL;
    gMouseClicked = YES;

    /* 2. Fermeture prioritaire du mode édition de texte si l'outil n'est plus Browse */
    if (gTool != TOOL_BROWSE && gEditingField) {
        [self endFieldEdit];
    }

    /* 3. Double-clic sur un objet (Édition des infos ou du script) */
    if (gTool != TOOL_BROWSE && hit && [event clickCount] == 2) {
        if (hit->type == OBJ_BUTTON)      [self showButtonInfo:hit];
        else if (hit->type == OBJ_FIELD)  [self showFieldInfo:hit];
        else                              [self editScriptOf:hit];
        return;
    }

    /* 4. Priorité aux outils FIELD et BUTTON (sélection et déplacement direct) */
    if (gTool == TOOL_FIELD || gTool == TOOL_BUTTON) {
        if (hit && ((gTool == TOOL_FIELD  && hit->type == OBJ_FIELD) ||
                    (gTool == TOOL_BUTTON && hit->type == OBJ_BUTTON))) {
            
            gSelected = hit;
            gMoving = YES;
            [[self window] makeFirstResponder:self];
            gMoveStart = p;
            gObjStartX = hit->x;
            gObjStartY = hit->y;
            [self setNeedsDisplay:YES];
            return;
        }
    }

    /* 5. Gestion des sélections de peinture actives (Rectangulaire & Lasso) */
    if (gTool == TOOL_SELRECT && gSelRectActive) {
        NSRect selRect = NSMakeRect(MIN(gSelStart.x, gSelEnd.x), MIN(gSelStart.y, gSelEnd.y),
                                    fabs(gSelEnd.x - gSelStart.x), fabs(gSelEnd.y - gSelStart.y));
        if (NSPointInRect(p, selRect)) {
            [self beginPaintUndo];
            Object *layer = [self paintLayer];
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            
            copy_rect(rep, gSelStart, gSelEnd);
            
            BOOL optionDown = ([event modifierFlags] & NSEventModifierFlagOption) != 0;
            if (!optionDown) {
                erase_rect(rep, gSelStart, gSelEnd);
            }

            gFloating = YES;
            gFloatDragging = YES;
            gFloatPos = selRect.origin;
            gFloatGrab = NSMakePoint(p.x - gFloatPos.x, p.y - gFloatPos.y);
            gSelRectActive = NO;
            [self startAntsTimer];
            [self setNeedsDisplay:YES];
            return;
        } else if (gFloating) {
            [self dropFloating];
        }
    }

    if (gTool == TOOL_LASSO && gLassoActive && gLassoCount >= 3) {
        NSBezierPath *lassoPath = [NSBezierPath bezierPath];
        [lassoPath moveToPoint:gLassoPts[0]];
        for (int i = 1; i < gLassoCount; i++) [lassoPath lineToPoint:gLassoPts[i]];
        [lassoPath closePath];

        if ([lassoPath containsPoint:p]) {
            [self beginPaintUndo];
            Object *layer = [self paintLayer];
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            
            copy_freeform(rep, gLassoPts, gLassoCount);
            
            BOOL optionDown = ([event modifierFlags] & NSEventModifierFlagOption) != 0;
            if (!optionDown) {
                erase_freeform(rep, gLassoPts, gLassoCount);
            }
            
            double minx = gLassoPts[0].x, miny = gLassoPts[0].y;
            for (int i = 1; i < gLassoCount; i++) {
                if (gLassoPts[i].x < minx) minx = gLassoPts[i].x;
                if (gLassoPts[i].y < miny) miny = gLassoPts[i].y;
            }
            gFloating = YES;
            gFloatDragging = YES;
            gFloatPos = NSMakePoint(minx, miny);
            gFloatGrab = NSMakePoint(p.x - gFloatPos.x, p.y - gFloatPos.y);
            gLassoActive = NO;
            [self startAntsTimer];
            [self setNeedsDisplay:YES];
            return;
        } else if (gFloating) {
            [self dropFloating];
        }
    }

    if (gFloating) {
        NSRect fr = NSMakeRect(gFloatPos.x, gFloatPos.y, gClipW, gClipH);
        if (NSPointInRect(p, fr)) {
            gFloatDragging = YES;
            gFloatGrab = NSMakePoint(p.x - gFloatPos.x, p.y - gFloatPos.y);
        } else {
            [self dropFloating];
        }
        return;
    }

    /* 6. Préparation Undo pour les outils de dessin */
    if (gTool == TOOL_PENCIL || gTool == TOOL_BRUSH || gTool == TOOL_ERASER ||
        gTool == TOOL_SPRAY  || gTool == TOOL_LINE  || gTool == TOOL_RECT   ||
        gTool == TOOL_OVAL   || gTool == TOOL_FILL  || gTool == TOOL_FREEFORM)
        [self beginPaintUndo];

    /* 7. Traitement des outils continus */
    if (gTool == TOOL_PENCIL || gTool == TOOL_BRUSH || gTool == TOOL_ERASER ||
        gTool == TOOL_SPRAY) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        if (layer) {
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                        (int)[self bounds].size.height);
            gPenLast = p;
            gPenDrawing = YES;
            gLockedAxis = AXIS_NONE;
            if (gTool == TOOL_PENCIL)      paint_stroke(rep, p, p, [NSColor blackColor], gLineWidth);
            else if (gTool == TOOL_BRUSH)  brush_stroke(rep, p, p);
            else if (gTool == TOOL_SPRAY) {
                spray_stamp(rep, (int)lround(p.x), (int)lround(p.y),
                            gSprayRadius, gSprayDensity);
                [self startSprayTimer];
            }
            else                           erase_stroke(rep, p, p, 16);
            [self setNeedsDisplay:YES];
        }
        return;
    }

    if (gTool == TOOL_LINE || gTool == TOOL_RECT || gTool == TOOL_OVAL) {
        gShapeStart = p;
        gShapeEnd = p;
        gShapeDrawing = YES;
        [self setNeedsDisplay:YES];
        return;
    }

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

    if (gTool == TOOL_FREEFORM) {
        gFreeCount = 0;
        gFreePts[gFreeCount++] = p;
        gFreeDrawing = YES;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_LASSO) {
        [[self window] makeFirstResponder:self];
        gLassoCount = 0;
        gLassoPts[gLassoCount++] = p;
        gLassoDrawing = YES;
        gLassoActive = NO;
        [self stopAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_SELRECT) {
        [[self window] makeFirstResponder:self];
        gSelStart = p; gSelEnd = p;
        gSelRectDrawing = YES;
        gSelRectActive = NO;
        [self stopAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_TEXT) {
        [self commitText];
        gTextPos = p;
        gTextBuf = [NSMutableString string];
        gTextActive = YES;
        [[self window] makeFirstResponder:self];
        [[NSFontManager sharedFontManager] setSelectedFont:text_font() isMultiple:NO];
        [self setNeedsDisplay:YES];
        return;
    }

    /* 8. Outil BROWSE (Interaction avec la carte) */
    if (gTool == TOOL_BROWSE) {
        /* Refermer l'édition en cours avant tout AUTRE clic, sinon un champ
         * en cours d'édition la garde ouverte pendant qu'un bouton clique à
         * côté exécute son script.
         *
         * C'est un vrai problème quand ce script change de carte (« go
         * next », par exemple) : gEditingField et sa zone de texte flottante
         * pointaient toujours sur le champ de l'ANCIENNE carte, et
         * continuaient de s'afficher par-dessus la nouvelle — en
         * surimpression, comme si les deux cartes se superposaient. Chaque
         * branche ci-dessous appelait déjà endFieldEdit pour SON propre cas
         * (changer de champ, cliquer la carte nue), sauf celle-ci — cliquer
         * un bouton, un champ verrouillé ou un menu popup pendant qu'un AUTRE
         * champ s'édite — qui ne l'appelait jamais.
         *
         * On épargne le champ qu'on re-clique lui-même : y cliquer ne fait
         * que replacer le curseur, pas recommencer l'édition, et lui
         * appliquer endFieldEdit ici la fermerait pour rien. */
        if (gEditingField && hit != gEditingField) [self endFieldEdit];

        if (hit && hit->type == OBJ_FIELD && hit->style &&
            strcmp(hit->style, "scrolling") == 0) {
            CGFloat bw = 16;
            NSRect bar = NSMakeRect(hit->x + hit->w - bw, hit->y, bw, hit->h);
            if (NSPointInRect(p, bar)) {
                NSRect tr0 = field_text_rect(hit);
                CGFloat th = field_text_height(hit, tr0);
                CGFloat vh = tr0.size.height;
                CGFloat gy = bar.origin.y + 16, gh = bar.size.height - 32;
                CGFloat lh = [@"Ag" sizeWithAttributes:
                    @{NSFontAttributeName: obj_font(hit, 12)}].height;
                if (lh < 4) lh = 12;

                if (p.y < bar.origin.y + 16) {
                    hit->scroll -= (int)lh;
                } else if (p.y > bar.origin.y + hit->h - 16) {
                    hit->scroll += (int)lh;
                } else if (th > vh && gh > 8) {
                    CGFloat kh = gh * (vh / th);
                    if (kh < 12) kh = 12;
                    if (kh > gh) kh = gh;
                    CGFloat maxs = th - vh;
                    CGFloat pos = (maxs > 0) ? (hit->scroll / maxs) : 0;
                    if (pos < 0) pos = 0;
                    if (pos > 1) pos = 1;
                    CGFloat ky = gy + pos * (gh - kh);
                    if (p.y >= ky && p.y <= ky + kh) {
                        gScrollField = hit;
                        gScrollGrab  = p.y - ky;
                        gScrollGH    = gh;
                        gScrollKH    = kh;
                        gScrollGY    = gy;
                        gScrollMax   = maxs;
                        return;
                    }
                    if (p.y < ky) hit->scroll -= hit->h;
                    else          hit->scroll += hit->h;
                }
                field_clamp_scroll(hit);
                sync_editor_scroll(hit);
                [self setNeedsDisplay:YES];
                return;
            }
        }

        if (hit && hit->type == OBJ_BUTTON && hit->style &&
            strcmp(hit->style, "popup") == 0) {
            open_popup_menu(hit, self);
            return;
        }

        if (hit && hit->type == OBJ_FIELD) {
            if (hit->locktext) {
                if (hit->auto_select) {
                    int ligne = click_line_number(hit, p);
                    if (ligne > 0) {
                        const char *tx = hc_field_text(hit);
                        int deb = 0, n = 1;
                        while (n < ligne && tx[deb]) {
                            if (tx[deb] == '\n') n++;
                            deb++;
                        }
                        int fin = deb;
                        while (tx[fin] && tx[fin] != '\n') fin++;
                        hc_set_selection(hit, deb, fin - deb);
                    }
                }
                gPressed = hit;
                hc_send(hit, "mouseDown");
                /* mouseStillDown part en continu tant que le bouton reste
                 * enfoncé, même immobile — c'est ce qui fait marcher les
                 * boutons à répétition et les flèches de défilement. Un
                 * minuteur plutôt que mouseDragged:, qui ne se déclenche
                 * qu'au mouvement. */
                [self startStillDownTimer];
                [self setNeedsDisplay:YES];
            } else {
                [self beginFieldEdit:hit];
            }
            return;
        }

        if (hit) {
            gPressed = hit;
            if (hit->type == OBJ_BUTTON && hit->autohilite &&
                (!hit->style ||
                 (strcmp(hit->style, "checkBox") != 0 && strcmp(hit->style, "checkbox") != 0 &&
                  strcmp(hit->style, "radioButton") != 0 && strcmp(hit->style, "radiobutton") != 0))) {
                hc_set_hilite(hit, hc_current_card(), 1);
            }
            hc_send(hit, "mouseDown");
            [self startStillDownTimer];
            [self setNeedsDisplay:YES];
        } else {
            /* Clic sur la carte nue : HyperCard lui envoie quand même
             * mouseDown, d'où il remonte au fond puis à la pile. C'est ce qui
             * permet à un script de carte de réagir à un clic n'importe où —
             * les piles de navigation s'en servent beaucoup. endFieldEdit
             * déjà fait en entrée du bloc TOOL_BROWSE, inutile de le refaire
             * ici. */
            gPressed = hc_current_card();
            hc_send(gPressed, "mouseDown");
            [self startStillDownTimer];
        }
        return;
    }

    /* 9. Redimensionnement via poignées */
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

    /* 10. Sélection générale ou tracé du rectangle de création */
    if (hit) {
        gSelected = hit;
        gMoving = YES;
        [[self window] makeFirstResponder:self];
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
    BOOL shiftDown = ([event modifierFlags] & NSEventModifierFlagShift) != 0;

    if (gPopupTarget && !gPopupFlashTimer) {
        NSInteger row = popup_row_at_point(p);
        if (row != gPopupKeyboardRow) {
            gPopupKeyboardRow = row;
            [self setNeedsDisplay:YES];
        }
        return;
    }

    if (gPressed && gPressed->type == OBJ_FIELD && gPressed->auto_select) {
        int ligne = click_line_number(gPressed, p);
        if (ligne > 0) {
            const char *tx = hc_field_text(gPressed);
            int deb = 0, n = 1;
            while (n < ligne && tx[deb]) {
                if (tx[deb] == '\n') n++;
                deb++;
            }
            int fin = deb;
            while (tx[fin] && tx[fin] != '\n') fin++;
            hc_set_selection(gPressed, deb, fin - deb);
            [self setNeedsDisplay:YES];
        }
        return;
    }

    if (gScrollField) {
        CGFloat travel = gScrollGH - gScrollKH;
        if (travel > 0) {
            CGFloat pos = (p.y - gScrollGrab - gScrollGY) / travel;
            if (pos < 0) pos = 0;
            if (pos > 1) pos = 1;
            gScrollField->scroll = (int)(pos * gScrollMax);
            sync_editor_scroll(gScrollField);
        }
        [self setNeedsDisplay:YES];
        return;
    }

    if (gFloating && gFloatDragging) {
        NSPoint targetP = shiftDown ? constrain_to_axis(gMoveStart, p) : p;
        gFloatPos = NSMakePoint(targetP.x - gFloatGrab.x, targetP.y - gFloatGrab.y);
        [self setNeedsDisplay:YES];
        return;
    }

    /* Outils de dessin continu (Crayon, Pinceau, Aérographe, Gomme) avec verrou d'axe */
    if ((gTool == TOOL_PENCIL || gTool == TOOL_BRUSH || gTool == TOOL_ERASER || gTool == TOOL_SPRAY) && gPenDrawing) {
        if (shiftDown) {
            if (gLockedAxis == AXIS_NONE) {
                CGFloat dx = fabs(p.x - gPenLast.x);
                CGFloat dy = fabs(p.y - gPenLast.y);
                if (dx > 2 || dy > 2) {
                    gLockedAxis = (dx > dy) ? AXIS_HORIZONTAL : AXIS_VERTICAL;
                }
            }

            if (gLockedAxis == AXIS_HORIZONTAL) {
                p.y = gPenLast.y;
            } else if (gLockedAxis == AXIS_VERTICAL) {
                p.x = gPenLast.x;
            }
        } else {
            gLockedAxis = AXIS_NONE;
        }

        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);

        if (gTool == TOOL_PENCIL) {
            paint_stroke(rep, gPenLast, p, [NSColor blackColor], gLineWidth);
        } else if (gTool == TOOL_BRUSH) {
            brush_stroke(rep, gPenLast, p);
        } else if (gTool == TOOL_SPRAY) {
            spray_stroke(rep, gPenLast, p, gSprayRadius, gSprayDensity);
        } else if (gTool == TOOL_ERASER) {
            erase_stroke(rep, gPenLast, p, 16);
        }

        gPenLast = p;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_FREEFORM && gFreeDrawing) {
        if (shiftDown) p = constrain_to_axis(gFreePts[0], p);
        if (gFreeCount < 4096) gFreePts[gFreeCount++] = p;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_SELRECT && gSelRectDrawing) {
        gSelEnd = shiftDown ? constrain_to_axis(gSelStart, p) : p;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gShapeDrawing) {
        gShapeEnd = shiftDown ? constrain_to_axis(gShapeStart, p) : p;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gResizeHandle && gSelected) {
        NSPoint currentP = shiftDown ? constrain_to_axis(gMoveStart, p) : p;
        int dx = (int)(currentP.x - gMoveStart.x);
        int dy = (int)(currentP.y - gMoveStart.y);
        int x = gObjStartX, y = gObjStartY, w = gObjStartW, h = gObjStartH;
        switch (gResizeHandle) {
            case 1: x += dx; y += dy; w -= dx; h -= dy; break;
            case 2: y += dy; w += dx; h -= dy; break;
            case 3: x += dx; w -= dx; h += dy; break;
            case 4: w += dx; h += dy; break;
        }
        if (w < 8) w = 8;
        if (h < 8) h = 8;
        gSelected->x = x; gSelected->y = y;
        gSelected->w = w; gSelected->h = h;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_LASSO && gLassoDrawing) {
        if (gLassoCount < 4096) gLassoPts[gLassoCount++] = p;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gMoving && gSelected) {
        NSPoint currentP = shiftDown ? constrain_to_axis(gMoveStart, p) : p;
        int dx = (int)(currentP.x - gMoveStart.x);
        int dy = (int)(currentP.y - gMoveStart.y);
        gSelected->x = gObjStartX + dx;
        gSelected->y = gObjStartY + dy;
        [self setNeedsDisplay:YES];
        return;
    }

    if (!gDragging) return;
    NSPoint currentP = shiftDown ? constrain_to_axis(gDragStart, p) : p;
    CGFloat x = MIN(gDragStart.x, currentP.x);
    CGFloat y = MIN(gDragStart.y, currentP.y);
    CGFloat w = fabs(currentP.x - gDragStart.x);
    CGFloat h = fabs(currentP.y - gDragStart.y);
    gDragRect = NSMakeRect(x, y, w, h);
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event {
    if (gPopupTarget && !gPopupFlashTimer) {
        NSPoint pp = [self convertPoint:[event locationInWindow] fromView:nil];
        NSInteger row = popup_row_at_point(pp);
        if (row >= 0 && popup_row_is_enabled(row)) {
            flash_popup_selection(self, row);
        } else {
            if ([NSDate timeIntervalSinceReferenceDate] - gPopupOpenedAt > 0.25) {
                close_popup_menu();
                [self setNeedsDisplay:YES];
            }
        }
        return;
    }

    if (gScrollField) { gScrollField = NULL; return; }
    if (gFloating) {
        gFloatDragging = NO;
        return;
    }
    if (gResizeHandle) {
        gResizeHandle = 0;
        [self setNeedsDisplay:YES];
        return;
    }
    if (gTool == TOOL_PENCIL || gTool == TOOL_ERASER ||
        gTool == TOOL_BRUSH  || gTool == TOOL_SPRAY) {
        gPenDrawing = NO;
        gLockedAxis = AXIS_NONE;
        [self stopSprayTimer];
        return;
    }
    if (gTool == TOOL_SELRECT) {
        gSelRectDrawing = NO;
        gSelRectActive = (fabs(gSelEnd.x-gSelStart.x) > 3 && fabs(gSelEnd.y-gSelStart.y) > 3);
        if (gSelRectActive) [self startAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }
    if (gTool == TOOL_LASSO) {
        gLassoDrawing = NO;
        gLassoActive = (gLassoCount >= 3);
        if (gLassoActive) [self startAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

    /* Gravure des formes géométriques */
    if (gShapeDrawing) {
        gShapeDrawing = NO;
        BOOL optionDown = ([event modifierFlags] & NSEventModifierFlagOption) != 0;
        NSRect box = compute_shape_rect(gShapeStart, gShapeEnd, optionDown);

        NSPoint finalStart = box.origin;
        NSPoint finalEnd = NSMakePoint(box.origin.x + box.size.width, box.origin.y + box.size.height);

        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);

        if (gTool == TOOL_LINE) {
            if (optionDown) {
                NSPoint opposite = NSMakePoint(2 * gShapeStart.x - gShapeEnd.x,
                                               2 * gShapeStart.y - gShapeEnd.y);
                paint_shape(rep, TOOL_LINE, opposite, gShapeEnd, [NSColor blackColor], gLineWidth);
            } else {
                paint_shape(rep, TOOL_LINE, gShapeStart, gShapeEnd, [NSColor blackColor], gLineWidth);
            }
        } else {
            if (gShapeFilled)
                fill_shape(rep, gTool, finalStart, finalEnd);
            paint_shape(rep, gTool, finalStart, finalEnd, [NSColor blackColor], gLineWidth);
        }

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

            /* Pour la carte elle-même, part_at rend NULL : on compare
             * donc à la carte courante plutôt qu'au résultat du test. */
            if (hit == gPressed ||
                (!hit && gPressed == hc_current_card()))
                hc_send(gPressed, "mouseUp");

            if (gPressed->type == OBJ_BUTTON && gPressed->style) {
                const char *st = gPressed->style;
                if (strcmp(st, "checkBox") == 0 || strcmp(st, "checkbox") == 0) {
                    hc_set_hilite(gPressed, hc_current_card(),
                                  !hc_hilite_of(gPressed, hc_current_card()));
                }
                else if (strcmp(st, "radioButton") == 0 || strcmp(st, "radiobutton") == 0) {
                    hc_set_hilite(gPressed, hc_current_card(), 1);
                    radio_exclusive(hc_current_card(), gPressed);
                }
                else if (gPressed->autohilite) {
                    hc_set_hilite(gPressed, hc_current_card(), 0);
                }
            } else if (gPressed->type == OBJ_BUTTON && gPressed->autohilite) {
                hc_set_hilite(gPressed, hc_current_card(), 0);
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

    if (!gDragging) return;
    gDragging = NO;
    if (gDragRect.size.width < 8 || gDragRect.size.height < 8) {
        [self setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
    if (!card) return;
    Object *owner = gEditBackground ? card->bg : card;
    if (!owner) owner = card;
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

- (void)findInStack:(id)sender {
    if (!gMsgBox) return;

    NSString *amorce = @"find \"\"";
    [gMsgBox setStringValue:amorce];
    [[self window] makeFirstResponder:gMsgBox];

    NSText *ed = [[self window] fieldEditor:YES forObject:gMsgBox];
    if (ed) {
        NSUInteger pos = [amorce length] - 1;
        [ed setSelectedRange:NSMakeRange(pos, 0)];
    }
}

- (void)dropFloating {
    if (!gFloating || !gClipboard) { gFloating = NO; return; }
    Object *card = hc_current_card();
    if (!card) { gFloating = NO; return; }
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    stamp_clipboard(rep, gFloatPos);

    /* La sélection suit la zone déposée.
     *
     * Sans cela, gSelStart/gSelEnd et gLassoPts gardent leur position
     * d'origine : le tracé pointillé réapparaissait là d'où le dessin venait,
     * alors qu'il est maintenant ailleurs. La reprendre au même endroit
     * permet aussi de la redéplacer aussitôt, comme dans HyperCard. */
    if (gTool == TOOL_SELRECT) {
        gSelStart = gFloatPos;
        gSelEnd   = NSMakePoint(gFloatPos.x + gClipW, gFloatPos.y + gClipH);
        gSelRectActive = YES;
    } else if (gTool == TOOL_LASSO && gClipPtsCount >= 3) {
        for (int i = 0; i < gClipPtsCount && i < 4096; i++)
            gLassoPts[i] = NSMakePoint(gFloatPos.x + gClipPts[i].x,
                                       gFloatPos.y + gClipPts[i].y);
        gLassoCount = gClipPtsCount;
        gLassoActive = YES;
    }

    gFloating = NO;
    gFloatDragging = NO;
    [self setNeedsDisplay:YES];
}

static NSTimer *gIdleTimer = nil;
static BOOL     gInIdle = NO;

- (void)startIdleTimer {
    if (gIdleTimer) return;
    gIdleTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/10.0
                                                  target:self
                                                selector:@selector(idleTick:)
                                                userInfo:nil
                                                 repeats:YES];
}

- (void)stopIdleTimer {
    if (gIdleTimer) { [gIdleTimer invalidate]; gIdleTimer = nil; }
}

- (void)idleTick:(NSTimer *)t {
    (void)t;
    if (gInIdle || hc_is_running()) return;

    HCView *v = gView;
    if (!v) return;
    Object *card = [v documentCard];
    if (!card) return;

    gInIdle = YES;
    hc_send(card, "idle");
    gInIdle = NO;
}

- (void)installMessageBox {
    gView = self;

    [self startIdleTimer];

    CGFloat w = 480, h = 30;
    gMsgPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(120, 120, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow |
                             NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered defer:NO];
    [gMsgPanel setTitle:@"Message"];
    [gMsgPanel setFloatingPanel:YES];
    [gMsgPanel setBecomesKeyOnlyIfNeeded:YES];
    [gMsgPanel setHidesOnDeactivate:YES];
    [gMsgPanel setReleasedWhenClosed:NO];

    gMsgBox = [[NSTextField alloc] initWithFrame:NSMakeRect(4, 3, w - 8, 24)];
    [gMsgBox setEditable:YES];
    [gMsgBox setSelectable:YES];
    [gMsgBox setBezeled:YES];
    [gMsgBox setDrawsBackground:YES];
    /* Fond et texte imposés ENSEMBLE.
     *
     * Le fond était figé à un blanc cassé, mais la couleur du texte restait
     * celle du système : en mode sombre elle passait au blanc, et la boîte
     * message devenait blanc sur blanc.
     *
     * La boîte message de HyperCard était noire sur blanc, et l'est restée
     * ici : c'est une fenêtre de PILE, pas un élément de l'interface de
     * macOS, et elle doit s'accorder à la carte qu'elle commande — laquelle
     * est blanche dans les deux modes. Imposer les deux couleurs plutôt que
     * suivre l'apparence est donc voulu. */
    [gMsgBox setBackgroundColor:[NSColor colorWithWhite:0.96 alpha:1.0]];
    [gMsgBox setTextColor:[NSColor blackColor]];
    [gMsgBox setAutoresizingMask:NSViewWidthSizable];
    [gMsgBox setStringValue:@""];
    [gMsgBox setTarget:self];
    [gMsgBox setAction:@selector(messageBoxEntered:)];
    [[gMsgPanel contentView] addSubview:gMsgBox];
    [gMsgPanel makeKeyAndOrderFront:nil];

    static HcHost host;
    host.line          = cocoa_line;
    host.field_changed = cocoa_field_changed;
    host.selection_changed = cocoa_selection_changed;
    host.ask           = cocoa_ask;
    host.answer_file   = cocoa_answer_file;
    host.ask_file      = cocoa_ask_file;
    host.save_stack    = cocoa_save_stack;
    host.open_stack    = cocoa_open_stack;
    host.load_stack    = cocoa_load_stack;
    hc_colors_init();
    host.print_cards   = cocoa_print_cards;
    host.stack_changed = cocoa_stack_changed;
    host.answer        = cocoa_answer;
    host.global_get    = cocoa_global_get;
    host.global_set    = cocoa_global_set;
    host.play_sound    = cocoa_play;
    host.choose_tool   = cocoa_choose_tool;
    host.drag          = cocoa_drag;
    host.click_at      = cocoa_click_at;
    host.type_text     = cocoa_type_text;
    host.visual_effect = cocoa_visual_effect;
    host.idle          = cocoa_idle;
    host.do_menu       = cocoa_do_menu;
    hc_set_host(&host);
}

- (void)messageBoxEntered:(id)sender {
    NSString *cmd = [gMsgBox stringValue];
    if ([cmd length] == 0) return;
    hc_do([cmd UTF8String]);

    [gView applyStackSize];
    [gView updateWindowTitle];
    [gView setNeedsDisplay:YES];
    [gMsgBox selectText:nil];
}

static NSTimer *gSprayTimer = nil;
static NSView       *gSprayPreview = nil;
static NSTextField  *gSprayRadiusLabel = nil;
static NSTextField  *gSprayDensityLabel = nil;

- (void)startSprayTimer {
    [self stopSprayTimer];
    gSprayTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/24.0
                                                   target:self
                                                 selector:@selector(sprayTick:)
                                                 userInfo:nil
                                                  repeats:YES];
}

- (void)stopSprayTimer {
    if (gSprayTimer) { [gSprayTimer invalidate]; gSprayTimer = nil; }
}

- (void)sprayTick:(NSTimer *)t {
    if (!gPenDrawing || gTool != TOOL_SPRAY) { [self stopSprayTimer]; return; }
    Object *card = hc_current_card();
    if (!card) { [self stopSprayTimer]; return; }
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    spray_stamp(rep, (int)lround(gPenLast.x), (int)lround(gPenLast.y),
                gSprayRadius, gSprayDensity);
    [self setNeedsDisplay:YES];
}

- (void)showSprayPalette {
    NSAlert *a = [[NSAlert alloc] init];
    [a setMessageText:@"Aérographe"];
    [a setInformativeText:@"Taille du nuage et nombre de points par passe."];
    [a addButtonWithTitle:@"OK"];
    [a addButtonWithTitle:@"Annuler"];

    NSView *c = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 280, 142)];

    SprayPreview *prev = [[SprayPreview alloc] initWithFrame:NSMakeRect(0, 72, 280, 62)];
    [c addSubview:prev];
    gSprayPreview = prev;

    NSTextField *rl = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 42, 62, 18)];
    [rl setStringValue:@"Rayon"];
    [rl setBezeled:NO]; [rl setDrawsBackground:NO]; [rl setEditable:NO];
    [c addSubview:rl];

    NSSlider *rs = [[NSSlider alloc] initWithFrame:NSMakeRect(62, 40, 176, 20)];
    [rs setMinValue:1]; [rs setMaxValue:32];
    [rs setNumberOfTickMarks:32];
    [rs setAllowsTickMarkValuesOnly:YES];
    [rs setIntValue:gSprayRadius];
    [rs setContinuous:YES];
    [rs setTarget:self]; [rs setAction:@selector(sprayRadiusChanged:)];
    [c addSubview:rs];

    gSprayRadiusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(244, 42, 36, 18)];
    [gSprayRadiusLabel setBezeled:NO]; [gSprayRadiusLabel setDrawsBackground:NO];
    [gSprayRadiusLabel setEditable:NO];
    [gSprayRadiusLabel setAlignment:NSTextAlignmentRight];
    [c addSubview:gSprayRadiusLabel];

    NSTextField *dl = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 10, 62, 18)];
    [dl setStringValue:@"Densité"];
    [dl setBezeled:NO]; [dl setDrawsBackground:NO]; [dl setEditable:NO];
    [c addSubview:dl];

    NSSlider *ds = [[NSSlider alloc] initWithFrame:NSMakeRect(62, 8, 176, 20)];
    [ds setMinValue:1]; [ds setMaxValue:120];
    [ds setIntValue:gSprayDensity];
    [ds setContinuous:YES];
    [ds setTarget:self]; [ds setAction:@selector(sprayDensityChanged:)];
    [c addSubview:ds];

    gSprayDensityLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(244, 10, 36, 18)];
    [gSprayDensityLabel setBezeled:NO]; [gSprayDensityLabel setDrawsBackground:NO];
    [gSprayDensityLabel setEditable:NO];
    [gSprayDensityLabel setAlignment:NSTextAlignmentRight];
    [c addSubview:gSprayDensityLabel];

    [a setAccessoryView:c];
    [self updateSprayLabels];

    int oldR = gSprayRadius, oldD = gSprayDensity;

    if ([a runModal] != NSAlertFirstButtonReturn) {
        gSprayRadius  = oldR;
        gSprayDensity = oldD;
    }

    gSprayPreview       = nil;
    gSprayRadiusLabel   = nil;
    gSprayDensityLabel  = nil;
}

- (void)updateSprayLabels {
    [gSprayRadiusLabel  setStringValue:[NSString stringWithFormat:@"%d", gSprayRadius]];
    [gSprayDensityLabel setStringValue:[NSString stringWithFormat:@"%d", gSprayDensity]];
    [gSprayPreview setNeedsDisplay:YES];
}

- (void)sprayRadiusChanged:(id)sender {
    gSprayRadius = [sender intValue];
    [self updateSprayLabels];
}

- (void)sprayDensityChanged:(id)sender {
    gSprayDensity = [sender intValue];
    [self updateSprayLabels];
}

- (void)installToolPalette {
    int cols = 4, rows = (NUM_TOOLCELLS + cols - 1) / cols;
    CGFloat cell = 38, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;

    gToolPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(560, 350, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow |
                             NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered defer:NO];
    [gToolPanel setTitle:@"Outils"];
    [gToolPanel setFloatingPanel:YES];
    [gToolPanel setBecomesKeyOnlyIfNeeded:YES];
    [gToolPanel setHidesOnDeactivate:YES];
    [gToolPanel setReleasedWhenClosed:NO];

    ToolPalette *grid = [[ToolPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gToolPanel setContentView:grid];
    [gToolPanel makeKeyAndOrderFront:nil];
}

- (void)showToolPalette {
    if (!gToolPanel) [self installToolPalette];
    else [gToolPanel makeKeyAndOrderFront:nil];
}

- (void)togglePalette:(id)sender {
    NSInteger tag = [sender tag];
    NSPanel *p = nil;
    switch (tag) {
        case 1: if (!gToolPanel)    { [self installToolPalette];    return; } p = gToolPanel;    break;
        case 2: if (!gPatternPanel) { [self installPatternPalette]; return; } p = gPatternPanel; break;
        case 3: if (!gWidthPanel)   { [self installWidthPalette];   return; } p = gWidthPanel;   break;
        case 4: if (!gBrushPanel)   { [self installBrushPalette];   return; } p = gBrushPanel;   break;
        case 5: if (!gMsgPanel)     { [self installMessageBox];     return; } p = gMsgPanel;     break;
        default: return;
    }
    if ([p isVisible]) [p orderOut:nil];
    else               [p makeKeyAndOrderFront:nil];
}

- (BOOL)paletteVisibleForTag:(NSInteger)tag {
    switch (tag) {
        case 1: return gToolPanel    && [gToolPanel isVisible];
        case 2: return gPatternPanel && [gPatternPanel isVisible];
        case 3: return gWidthPanel   && [gWidthPanel isVisible];
        case 4: return gBrushPanel   && [gBrushPanel isVisible];
        case 5: return gMsgPanel     && [gMsgPanel isVisible];
    }
    return NO;
}

- (void)toggleFilled:(id)sender {
    gShapeFilled = !gShapeFilled;
     
}

- (void)toolChosen:(id)sender {
    [self dropFloating];
    
    HCTool newTool = (HCTool)[sender tag];
    
    if (gEditingField) {
        [self endFieldEdit];
    }
    
    [[self window] makeFirstResponder:self];
    
    gTool = newTool;
    gSelected = NULL;
    [self setNeedsDisplay:YES];
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

    NSButton *verif = [[NSButton alloc] initWithFrame:NSMakeRect(290, 8, 90, 30)];
    [verif setTitle:@"Vérifier"];
    [verif setBezelStyle:NSBezelStyleRounded];
    [verif setTarget:self];
    [verif setAction:@selector(checkScript:)];
    [verif setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];
    [[panel contentView] addSubview:verif];

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

- (void)selectLine:(int)ligne inTextView:(NSTextView *)tv {
    NSString *s = [tv string];
    NSUInteger deb = 0, len = [s length];
    int courante = 1;

    if (len == 0) return;

    while (deb < len && courante < ligne) {
        NSRange r = [s lineRangeForRange:NSMakeRange(deb, 0)];
        deb = NSMaxRange(r);
        courante++;
    }
    if (deb >= len) deb = len - 1;

    NSRange r = [s lineRangeForRange:NSMakeRange(deb, 0)];
    while (r.length > 0) {
        unichar c = [s characterAtIndex:NSMaxRange(r) - 1];
        if (c != '\n' && c != '\r') break;
        r.length--;
    }

    [tv setSelectedRange:r];
    [tv scrollRangeToVisible:r];
    [[tv window] makeFirstResponder:tv];
}

- (void)checkScript:(id)sender {
    (void)sender;
    if (!gEditView) return;

    const char *src = [[gEditView string] UTF8String];
    HctRapport rap;
    hct_verifie(src ? src : "", &rap, YES);

    if (rap.n == 0) {
        NSAlert *a = [[NSAlert alloc] init];
        [a setMessageText:@"Script correct"];
        [a setInformativeText:@"Aucune faute détectée."];
        [a runModal];
        hct_rapport_libere(&rap);
        return;
    }

    NSMutableString *txt = [NSMutableString string];
    for (int i = 0; i < rap.n; i++) {
        HctSignalement *s = &rap.liste[i];
        [txt appendFormat:@"%@ ligne %d : %s",
             s->niveau == HCT_V_ERREUR ? @"Erreur" : @"Attention",
             s->ligne, s->message];
        if (s->extrait[0]) [txt appendFormat:@"  [%s]", s->extrait];
        [txt appendString:@"\n"];
    }

    const HctSignalement *p = hct_premier(&rap);
    if (p) [self selectLine:p->ligne inTextView:gEditView];

    NSAlert *a = [[NSAlert alloc] init];
    [a setMessageText:rap.nerreurs
        ? [NSString stringWithFormat:@"%d faute%@ de syntaxe", rap.nerreurs,
                                     rap.nerreurs > 1 ? @"s" : @""]
        : @"Script correct, avec des remarques"];
    [a setInformativeText:txt];
    [a runModal];

    hct_rapport_libere(&rap);
}

- (void)saveScript:(id)sender {
    if (gEditTarget && gEditView) {
        hc_set_script(gEditTarget, [[gEditView string] UTF8String]);
    }
    [gEditPanel close];
    gEditPanel = nil; gEditView = nil; gEditTarget = nil;
    [gView setNeedsDisplay:YES];
}

- (void)beginFieldEdit:(Object *)field {
    if (!field) return;

    /* Interdiction d'éditer si l'outil n'est pas BROWSE ou si le champ est verrouillé */
    if (gTool != TOOL_BROWSE || field->locktext) {
        if (gEditingField) [self endFieldEdit];
        [[self window] makeFirstResponder:self];
        [self setNeedsDisplay:YES];
        return;
    }

    [self endFieldEdit];
    gEditingField = field;

    NSRect r = field_text_rect(field);
    BOOL isScroll = (field->style && strcmp(field->style, "scrolling") == 0);

    /* 1. Conteneur ScrollView ajusté et verrouillé au cadre */
    gFieldScroll = [[NSScrollView alloc] initWithFrame:r];
    [gFieldScroll setHasVerticalScroller:NO];
    [gFieldScroll setHasHorizontalScroller:NO];
    [gFieldScroll setAutohidesScrollers:YES];
    [gFieldScroll setBorderType:NSNoBorder];
    [gFieldScroll setDrawsBackground:NO];
    [gFieldScroll setWantsLayer:YES];
    [[gFieldScroll layer] setMasksToBounds:YES];

    NSSize sz = [gFieldScroll contentSize];

    /* 2. Instanciation et configuration du NSTextView */
    gFieldEditor = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, sz.width, sz.height)];
    [gFieldEditor setMinSize:NSMakeSize(0, 0)];
    [gFieldEditor setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
    [gFieldEditor setVerticallyResizable:YES];
    [gFieldEditor setHorizontallyResizable:NO];
    [gFieldEditor setAutoresizingMask:NSViewWidthSizable];

    /* 3. Métriques et suppression des marges internes */
    NSTextContainer *container = [gFieldEditor textContainer];
    [container setContainerSize:NSMakeSize(sz.width, FLT_MAX)];
    [container setWidthTracksTextView:YES];
    [container setLineFragmentPadding:0];

    [gFieldEditor setTextContainerInset:NSMakeSize(0, 0)];
    [gFieldEditor setDrawsBackground:NO];
    [gFieldEditor setDelegate:self];

    /* Alignement typographique classique Cocoa */
    NSLayoutManager *lm = [gFieldEditor layoutManager];
    [lm setUsesFontLeading:YES];
    [lm setTypesetterBehavior:NSTypesetterBehavior_10_2_WithCompatibility];

    /* Annule les variations de baseline de la première ligne selon les polices (Geneva vs Times) */
        [gFieldEditor setDisplaysLinkToolTips:NO];
        [lm setBackgroundLayoutEnabled:NO];
    
    [gFieldEditor setRichText:YES];
    [gFieldEditor setImportsGraphics:NO];
    [gFieldEditor setUsesFontPanel:YES];
    [gFieldEditor setAllowsUndo:YES];

    /* 4. Chargement du texte et synchronisation de l'interligne statique */
    const char *tx = hc_field_text(field);
    NSString *str = [NSString stringWithUTF8String:tx ? tx : ""];
    NSDictionary *base = obj_attrs(field, 12, [NSColor blackColor]);

    [gFieldEditor setEditable:!field->locktext];
    [gFieldEditor setSelectable:YES];

    gForEditor = YES;
    [[gFieldEditor textStorage] setAttributedString:field_attr_string(field, str, base)];
    gForEditor = NO;

    /* Application stricte de la hauteur de ligne pour correspondre au mode Browse */
    NSMutableDictionary *tattr = [base mutableCopy];
    NSMutableParagraphStyle *ps = [[tattr objectForKey:NSParagraphStyleAttributeName] mutableCopy];
    if (!ps) ps = [[NSMutableParagraphStyle alloc] init];

    CGFloat lh = hc_text_height(field);
    if (lh > 0) {
        [ps setMinimumLineHeight:lh];
        [ps setMaximumLineHeight:lh];
    }
    tattr[NSParagraphStyleAttributeName] = ps;
    [gFieldEditor setTypingAttributes:tattr];

    [gFieldScroll setDocumentView:gFieldEditor];

    /* Réinitialisation de l'origine du document */
    NSRect df = [gFieldEditor frame];
    if (df.origin.x != 0 || df.origin.y != 0) {
        df.origin = NSZeroPoint;
        [gFieldEditor setFrame:df];
    }

    [self addSubview:gFieldScroll];

    /* 5. Prise en charge du défilement pour les champs scrolling */
    if (isScroll) {
        field_clamp_scroll(field);
        if (field->scroll > 0) {
            [[gFieldEditor layoutManager] ensureLayoutForTextContainer:container];
            [[gFieldScroll contentView] scrollToPoint:NSMakePoint(0, field->scroll)];
            [gFieldScroll reflectScrolledClipView:[gFieldScroll contentView]];
        }

        [[gFieldScroll contentView] setPostsBoundsChangedNotifications:YES];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(fieldEditorDidScroll:)
                                                     name:NSViewBoundsDidChangeNotification
                                                   object:[gFieldScroll contentView]];
    }

    [[self window] makeFirstResponder:gFieldEditor];
}

- (void)scrollWheel:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    Object *hit = part_at(hc_current_card(), p);
    if (!hit || hit->type != OBJ_FIELD ||
        !(hit->style && strcmp(hit->style, "scrolling") == 0)) {
        [super scrollWheel:event];
        return;
    }

    CGFloat dy = [event scrollingDeltaY];
    if ([event hasPreciseScrollingDeltas]) hit->scroll -= (int)lround(dy);
    else                                   hit->scroll -= (int)lround(dy * 16);

    field_clamp_scroll(hit);
    sync_editor_scroll(hit);
    [self setNeedsDisplay:YES];
}

- (void)fieldEditorDidScroll:(NSNotification *)note {
    if (gSyncingEditorScroll) return;
    if (!gEditingField || !gFieldScroll) return;
    gEditingField->scroll =
        (int)lround(editor_fraction() * field_max_scroll(gEditingField));
    [self setNeedsDisplay:YES];
}

- (void)textViewDidChangeSelection:(NSNotification *)note {
    if (gApplyingSelection) return;
    if (!gEditingField || !gFieldEditor) return;
    if ([note object] != gFieldEditor) return;

    NSRange r = [gFieldEditor selectedRange];
    gApplyingSelection = YES;
    hc_set_selection(gEditingField, (int)r.location, (int)r.length);
    gApplyingSelection = NO;
}

- (void)endFieldEdit {
    if (gFieldEditor && gEditingField) {
        if ([[self window] firstResponder] == gFieldEditor) {
            [[self window] makeFirstResponder:self];
        }

        NSString *str = [gFieldEditor string];

        hc_set_field_text(gEditingField, [str UTF8String]);
        hc_runs_clear(gEditingField);

        NSTextStorage *ts = [gFieldEditor textStorage];

        NSFont *fbase = obj_base_font(gEditingField, 12);
        NSString *fbaseName = [fbase familyName];
        if (!fbaseName) fbaseName = [fbase fontName];
        int fbaseSize = (int)lround([fbase pointSize]);
        int fbaseStyle = gEditingField->textstyle;

        NSUInteger len = [ts length], i = 0;
        while (i < len) {
            NSRange eff;
            NSDictionary *a = [ts attributesAtIndex:i
                              longestEffectiveRange:&eff
                                            inRange:NSMakeRange(i, len - i)];
            int st = style_bits_from_attrs(a);

            NSFont *rf = a[NSFontAttributeName];
            const char *fname = NULL;
            int fsize = 0;
            if (rf) {
                NSFontDescriptor *fd = [[rf fontDescriptor]
                    fontDescriptorWithSymbolicTraits:
                        [[rf fontDescriptor] symbolicTraits]
                        & ~(NSFontDescriptorTraitBold | NSFontDescriptorTraitItalic)];
                NSFont *plain = [NSFont fontWithDescriptor:fd size:[rf pointSize]];
                NSFont *src = plain ? plain : rf;
                NSString *nm = [src familyName];
                if (!nm) nm = [src fontName];
                int sz = (int)lround([rf pointSize]);

                if (![nm isEqualToString:fbaseName]) fname = [nm UTF8String];
                if (sz != fbaseSize)                 fsize = sz;
            }

            int fcolor = HC_COLOR_INHERIT;
            {
                /* La couleur récoltée dans l'éditeur Cocoa.
                 *
                 * On rejetait ici le noir ET le blanc, pour ne pas enregistrer
                 * une couleur là où l'utilisateur n'en avait posé aucune. Mais
                 * le blanc est une couleur qu'on choisit délibérément — sur un
                 * bandeau sombre, c'est même la seule qui convienne — et le
                 * noir en est une aussi dès qu'un champ a une autre couleur par
                 * défaut. Les deux se perdaient donc en refermant l'éditeur,
                 * alors que « set the textColor to white » les gardait.
                 *
                 * On ne rejette plus rien : c'est le TEXTE NON COLORÉ qui doit
                 * se reconnaître, et l'absence d'attribut s'en charge déjà,
                 * `rc` valant alors nil. */
                NSColor *rc = a[NSForegroundColorAttributeName];
                if (rc) {
                    NSColor *rgb = [rc colorUsingColorSpace:
                                        [NSColorSpace sRGBColorSpace]];
                    if (rgb) {
                        CGFloat rc0 = [rgb redComponent];
                        CGFloat gc0 = [rgb greenComponent];
                        CGFloat bc0 = [rgb blueComponent];
                        if (rc0 < 0) rc0 = 0;
                        if (rc0 > 1) rc0 = 1;
                        if (gc0 < 0) gc0 = 0;
                        if (gc0 > 1) gc0 = 1;
                        if (bc0 < 0) bc0 = 0;
                        if (bc0 > 1) bc0 = 1;
                        int r = (int)lround(rc0 * 255);
                        int g = (int)lround(gc0 * 255);
                        int b = (int)lround(bc0 * 255);
                        fcolor = (r << 16) | (g << 8) | b;
                    }
                }
            }

            if (st == fbaseStyle && !fname && fsize == 0 &&
                fcolor == HC_COLOR_INHERIT) {
                i = NSMaxRange(eff);
                if (eff.length == 0) break;
                continue;
            }

            int b0 = byte_from_utf16(str, eff.location);
            int b1 = byte_from_utf16(str, eff.location + eff.length);
            hc_run_add_color(gEditingField, b0, b1 - b0, st, fsize, fname, fcolor);
            i = NSMaxRange(eff);
            if (eff.length == 0) break;
        }
    }
    if (gEditingField && gFieldScroll &&
        gEditingField->style && strcmp(gEditingField->style, "scrolling") == 0) {
        gEditingField->scroll =
            (int)lround(editor_fraction() * field_max_scroll(gEditingField));
        field_clamp_scroll(gEditingField);
    }

    if (gFieldScroll)
        [[NSNotificationCenter defaultCenter]
            removeObserver:self
                      name:NSViewBoundsDidChangeNotification
                    object:[gFieldScroll contentView]];

    [gFieldEditor setDelegate:nil];

    [gFieldScroll removeFromSuperview];
    gFieldScroll = nil;
    gFieldEditor = nil;
    gEditingField = NULL;
    [self setNeedsDisplay:YES];
}
- (void)startStillDownTimer {
    [self stopStillDownTimer];
    gStillDownTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0
                                                       target:self
                                                     selector:@selector(stillDownTick:)
                                                     userInfo:nil
                                                      repeats:YES];

    /* Le mode « common » fait continuer le minuteur pendant qu'un menu est
     * ouvert ou qu'on redimensionne la fenêtre ; sans lui il se fige, et le
     * script cesse de recevoir mouseStillDown au pire moment.
     *
     * Tolérance nulle : macOS regroupe volontiers les déclenchements pour
     * économiser l'énergie, ce qui produit exactement les à-coups qu'on
     * cherche à supprimer. */
    [[NSRunLoop currentRunLoop] addTimer:gStillDownTimer
                                 forMode:NSRunLoopCommonModes];
    [gStillDownTimer setTolerance:0];
}

- (void)stopStillDownTimer {
    [gStillDownTimer invalidate];
    gStillDownTimer = nil;
}

- (void)stillDownTick:(NSTimer *)t {
    (void)t;
       if (!gPressed || !([NSEvent pressedMouseButtons] & 1)) {
        [self stopStillDownTimer];
        return;
    }
    hc_send(gPressed, "mouseStillDown");
}
@end
