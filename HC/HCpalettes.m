//
//  HCpalettes.m
//  HC
//
//  Created by Patricia Benedetto on 31/07/2026.
//

#import "HCpalettes.h"
#import "icons.h"
#import "HCview.h"
#import "HCicons.h"
// #import "HCview.h"
// ==================== palette d'épaisseur de trait (vue custom) ====================
 
// 12 brosses 16x16 (bit a 1 = pixel peint)
static const unsigned short BRUSHES[12][16] = {
    /* 0 : point 1px */
    {0,0,0,0,0,0,0,0x0080,0,0,0,0,0,0,0,0},
    /* 1 : carre 2 */
    {0,0,0,0,0,0,0x00C0,0x00C0,0,0,0,0,0,0,0,0},
    /* 2 : carre 4 */
    {0,0,0,0,0,0x03C0,0x03C0,0x03C0,0x03C0,0,0,0,0,0,0,0},
    /* 3 : carre 8 */
    {0,0,0,0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0,0,0,0},
    /* 4 : rond 4 */
    {0,0,0,0,0,0x0180,0x03C0,0x03C0,0x03C0,0x0180,0,0,0,0,0,0},
    /* 5 : rond 8 */
    {0,0,0,0x0180,0x07E0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x07E0,0x0180,0,0,0},
    /* 6 : rond 12 */
    {0,0x0180,0x07E0,0x0FF0,0x1FF8,0x3FFC,0x3FFC,0x3FFC,0x3FFC,0x3FFC,0x3FFC,0x1FF8,0x0FF0,0x07E0,0x0180,0},
    /* 7 : barre horizontale */
    {0,0,0,0,0,0,0,0x3FFC,0x3FFC,0,0,0,0,0,0,0},
    /* 8 : barre verticale */
    {0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180},
    /* 9 : oblique \ */
        {0xC000,0xE000,0x7000,0x3800,0x1C00,0x0E00,0x0700,0x0380,0x01C0,0x00E0,0x0070,0x0038,0x001C,0x000E,0x0007,0x0003},
        /* 10 : oblique / */
        {0x0003,0x0007,0x000E,0x001C,0x0038,0x0070,0x00E0,0x01C0,0x0380,0x0700,0x0E00,0x1C00,0x3800,0x7000,0xE000,0xC000},
    /* 11 : croix */
        {0,0,0,0x0180,0x0180,0x0180,0x0180,0x0180,0x0FF0,0x0FF0,0x0180,0x0180,0x0180,0x0180,0x0180,0},
};

int brush_bit(int brush, int x, int y) {
    if (brush < 0 || brush >= NUM_BRUSHES) brush = 5;
    if (x < 0 || x > 15 || y < 0 || y > 15) return 0;
    return (BRUSHES[brush][y] >> (15 - x)) & 1;
}
@implementation WidthPalette

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
    NSRectFill(dirtyRect);

    int cols = 4;
    CGFloat cell = 40, gap = 3, margin = 6;

    for (int i = 0; i <= 10; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap),
                                margin + row*(cell+gap),
                                cell, cell);

        BOOL active = (gLineWidth == i);

        [(active ? [NSColor whiteColor] : [NSColor colorWithWhite:0.82 alpha:1.0]) setFill];
        NSRectFill(box);

        if (i == 0) {
            NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
            [ps setAlignment:NSTextAlignmentCenter];
            NSDictionary *attrs = @{
                NSFontAttributeName: [NSFont systemFontOfSize:14],
                NSParagraphStyleAttributeName: ps
            };
            NSRect tr = box; tr.origin.y += (box.size.height - 18)/2;
            [@"0" drawInRect:tr withAttributes:attrs];
        } else {
            [[NSColor blackColor] setStroke];
            NSBezierPath *line = [NSBezierPath bezierPath];
            CGFloat midY = box.origin.y + box.size.height/2;
            [line moveToPoint:NSMakePoint(box.origin.x + 6, midY)];
            [line lineToPoint:NSMakePoint(box.origin.x + box.size.width - 6, midY)];
            [line setLineWidth:i];
            [line stroke];
        }

        if (active) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr = [NSBezierPath bezierPathWithRect:NSInsetRect(box, 1, 1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    int cols = 4;
    CGFloat cell = 40, gap = 3, margin = 6;
    for (int i = 0; i <= 10; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap),
                                margin + row*(cell+gap),
                                cell, cell);
        if (NSPointInRect(p, box)) {
            gLineWidth = i;
            [self setNeedsDisplay:YES];
            break;
        }
    }
}

@end



// ==================== palette d'outils custom (grille + sélection encadrée) ====================
typedef struct { const char *glyph; int kind; int value; } ToolCell;

static const ToolCell TOOLCELLS[] = {
    {"👆", 0, TOOL_BROWSE},
    {"B",  0, TOOL_BUTTON},
    {"F",  0, TOOL_FIELD},
    {"✏", 0, TOOL_PENCIL},
    {"P", 0, TOOL_BRUSH},
    {"⌫", 0, TOOL_ERASER},
    {"╱", 0, TOOL_LINE},
    {"▭", 0, TOOL_RECT},
    {"○", 0, TOOL_OVAL},
    {"💧", 0, TOOL_FILL},
    {"✎", 0, TOOL_FREEFORM},
    {"⬚", 0, TOOL_LASSO},
    {"◰", 0, TOOL_SELRECT},
    {"A", 0, TOOL_TEXT},//  
    {"⬛", 1, INK_BLACK},
    {"⬜", 1, INK_WHITE},
    {"▣", 2, 0},
    {"◫", 3, 0},
};


const int NUM_TOOLCELLS = (int)(sizeof(TOOLCELLS)/sizeof(TOOLCELLS[0]));
@interface HCView (Palettes)
- (void)showPatternPalette;
- (void)showWidthPalette;
- (void)showBrushPalette;
- (void)commitText;
@end
 

#define ICONGRID_COLS 6
#define ICONGRID_CELL 44

@implementation IconGrid

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
    NSRectFill([self bounds]);

    for (int i = 0; i < NUM_HCICONS; i++) {
        int col = i % ICONGRID_COLS, row = i / ICONGRID_COLS;
        NSRect box = NSMakeRect(col*ICONGRID_CELL, row*ICONGRID_CELL,
                                ICONGRID_CELL, ICONGRID_CELL);
        if (!NSIntersectsRect(box, dirtyRect)) continue;

        BOOL active = (HCICONS[i].id == self.selected);
        [(active ? [NSColor whiteColor] : [NSColor colorWithWhite:0.82 alpha:1.0]) setFill];
        NSRectFill(box);

        [[NSColor blackColor] setFill];
        hcicon_draw(&HCICONS[i], box, 1.0);

        if (active) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr = [NSBezierPath bezierPathWithRect:NSInsetRect(box, 1, 1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    int col = (int)(p.x / ICONGRID_CELL);
    int row = (int)(p.y / ICONGRID_CELL);
    int i = row * ICONGRID_COLS + col;
    if (col < 0 || col >= ICONGRID_COLS || i < 0 || i >= NUM_HCICONS) return;
    self.selected = HCICONS[i].id;
    [self setNeedsDisplay:YES];
}

@end
@implementation ToolPalette

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
    NSRectFill(dirtyRect);

    int cols = 4;
    CGFloat cell = 38, gap = 3, margin = 6;

    for (int i = 0; i < NUM_TOOLCELLS; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap),
                                margin + row*(cell+gap),
                                cell, cell);
        const ToolCell *tc = &TOOLCELLS[i];

        BOOL active = NO;
        if (tc->kind == 0) active = (gTool == (HCTool)tc->value);
        else if (tc->kind == 1) active = (gInk == (HCInk)tc->value);
        else if (tc->kind == 2) active = gShapeFilled;
        else if (tc->kind == 3) active = gTransparentBg;
        [(active ? [NSColor whiteColor] : [NSColor colorWithWhite:0.82 alpha:1.0]) setFill];
        NSRectFill(box);

        // icône bitmap pour certains outils, glyphe pour les autres
        if (tc->kind == 0 && tc->value == TOOL_PENCIL) {
             draw_icon_ascii(ICON_PENCIL32, box);
         } else if  (tc->kind == 0 && tc->value == TOOL_FILL) {
                    draw_icon_ascii(ICON_BUCKET32, box);
                } else if (tc->kind == 0 && tc->value == TOOL_LASSO) {
                    draw_icon_ascii(ICON_LASSO32, box);
                } else if (tc->kind == 0 && tc->value == TOOL_ERASER) {
                    draw_icon_ascii(ICON_ERASER32, box);
                } else if (tc->kind == 0 && tc->value == TOOL_FREEFORM) {
                    draw_icon_ascii(ICON_FREEFORM32, box);
                } else if (tc->kind == 0 && tc->value == TOOL_SELRECT) {
                            draw_icon_ascii(ICON_SELRECT32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_BUTTON) {
                            draw_icon_ascii(ICON_BUTTON32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_FIELD) {
                   draw_icon_ascii(ICON_FIELD32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_RECT) {
                   draw_icon_ascii(ICON_RECT32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_LINE) {
                   draw_icon_ascii(ICON_LINE32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_OVAL) {
                   draw_icon_ascii(ICON_OVAL32, box);
               } else   if (tc->kind == 0 && tc->value == TOOL_BROWSE) {
                       draw_icon_ascii(ICON_HAND32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_BRUSH) {
                           draw_icon_ascii(ICON_BRUSH32, box);
                }else{
                    NSString *g = [NSString stringWithUTF8String:tc->glyph];
                    NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                    [ps setAlignment:NSTextAlignmentCenter];
                    NSDictionary *attrs = @{
                        NSFontAttributeName: [NSFont systemFontOfSize:18],
                        NSParagraphStyleAttributeName: ps
                    };
                    NSRect tr = box;
                    tr.origin.y += (box.size.height - 22) / 2;
                    [g drawInRect:tr withAttributes:attrs];
                }

        if (active) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr = [NSBezierPath bezierPathWithRect:NSInsetRect(box, 1, 1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    BOOL dbl = ([event clickCount] == 2);
    int cols = 4;
    CGFloat cell = 38, gap = 3, margin = 6;
    for (int i = 0; i < NUM_TOOLCELLS; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap), margin + row*(cell+gap), cell, cell);
        if (NSPointInRect(p, box)) {
            const ToolCell *tc = &TOOLCELLS[i];
            if (tc->kind == 0) {
                            // quitter l'outil texte : graver la saisie en cours
                            if (gTool == TOOL_TEXT && tc->value != TOOL_TEXT) {
                                #pragma clang diagnostic push
                                #pragma clang diagnostic ignored "-Warc-performSelector-leaks"
                                if ([gView respondsToSelector:@selector(commitText)])
                                    [gView performSelector:@selector(commitText)];
                                #pragma clang diagnostic pop
                            }
                            gTool = (HCTool)tc->value;
                            gSelected = NULL;
                            if (dbl) {
                                // double-clic : ouvrir la palette de réglage associée
                                SEL sel = NULL;
                                if (tc->value == TOOL_FILL)
                                    sel = @selector(showPatternPalette);
                                else if (tc->value == TOOL_BRUSH)
                                    sel = @selector(showBrushPalette);
                                else if (tc->value == TOOL_PENCIL || tc->value == TOOL_ERASER ||
                                         tc->value == TOOL_LINE || tc->value == TOOL_RECT ||
                                         tc->value == TOOL_OVAL || tc->value == TOOL_FREEFORM)
                                    sel = @selector(showWidthPalette);
                                if (sel && [gView respondsToSelector:sel]) {
                                    #pragma clang diagnostic push
                                    #pragma clang diagnostic ignored "-Warc-performSelector-leaks"
                                    [gView performSelector:sel];
                                    #pragma clang diagnostic pop
                                }
                            }
                        }
            else if (tc->kind == 1) { gInk = (HCInk)tc->value; }
            else if (tc->kind == 2) { gShapeFilled = !gShapeFilled; }
            else if (tc->kind == 3) { gTransparentBg = !gTransparentBg; }
            [self setNeedsDisplay:YES];
            [gView setNeedsDisplay:YES];
            break;
        }
    }
}

@end
// ---- petite vue-grille pour choisir un motif ----
 

@implementation PatternPalette

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithWhite:0.85 alpha:1.0] setFill];
    NSRectFill(dirtyRect);

    int cols = 4;
    CGFloat cell = 32, gap = 4, margin = 6;

    for (int i = 0; i < NUM_PATTERNS; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap),
                                margin + row*(cell+gap),
                                cell, cell);
        // fond blanc de la case
        [[NSColor whiteColor] setFill];
        NSRectFill(box);

        // dessiner le motif dans la case, pixel par pixel (agrandi)
        // dessiner le motif : 8x8 bits, chaque bit = 4x4 px -> remplit la case 32x32
        [[NSColor blackColor] setFill];
        CGFloat px = cell / 8.0;
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (pattern_bit(i, x, y)) {
                    NSRectFill(NSMakeRect(box.origin.x + x*px,
                                          box.origin.y + y*px,
                                          px, px));
                }
            }
        
        }

        // cadre : rouge épais si c'est le motif courant, gris sinon
        if (i == gPattern) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr = [NSBezierPath bezierPathWithRect:NSInsetRect(box, -1, -1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor grayColor] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    int cols = 4;
    CGFloat cell = 32, gap = 4, margin = 6;
    for (int i = 0; i < NUM_PATTERNS; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap),
                                margin + row*(cell+gap),
                                cell, cell);
        if (NSPointInRect(p, box)) {
            gPattern = i;
            [self setNeedsDisplay:YES];
            [gView setNeedsDisplay:YES];
            break;
        }
    }
}

@end
@implementation BrushPalette

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
    NSRectFill(dirtyRect);
    int cols = 4;
    CGFloat cell = 34, gap = 3, margin = 6;
    for (int i = 0; i < NUM_BRUSHES; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap), margin + row*(cell+gap), cell, cell);
        BOOL active = (gBrush == i);
        [(active ? [NSColor whiteColor] : [NSColor colorWithWhite:0.82 alpha:1.0]) setFill];
        NSRectFill(box);
        // apercu de la brosse, 1 bit = 2x2 px, centre
        [[NSColor blackColor] setFill];
        CGFloat px = 2.0;
        CGFloat ox = box.origin.x + (cell - 16*px)/2;
        CGFloat oy = box.origin.y + (cell - 16*px)/2;
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++)
                if (brush_bit(i, x, y))
                    NSRectFill(NSMakeRect(ox + x*px, oy + y*px, px, px));
        if (active) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr = [NSBezierPath bezierPathWithRect:NSInsetRect(box, 1, 1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    int cols = 4;
    CGFloat cell = 34, gap = 3, margin = 6;
    for (int i = 0; i < NUM_BRUSHES; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap), margin + row*(cell+gap), cell, cell);
        if (NSPointInRect(p, box)) {
            gBrush = i;
            [self setNeedsDisplay:YES];
            break;
        }
    }
}

@end
