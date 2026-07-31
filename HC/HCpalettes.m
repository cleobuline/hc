//
//  HCpalettes.m
//  HC
//
//  Created by Patricia Benedetto on 31/07/2026.
//

#import "HCpalettes.h"
#import "icons.h"

@interface HCView : NSView
- (void)showPatternPalette;
- (void)showWidthPalette;
@end

// #import "HCview.h"
// ==================== palette d'épaisseur de trait (vue custom) ====================
 
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
    {"⌫", 0, TOOL_ERASER},
    {"╱", 0, TOOL_LINE},
    {"▭", 0, TOOL_RECT},
    {"○", 0, TOOL_OVAL},
    {"💧", 0, TOOL_FILL},
    {"✎", 0, TOOL_FREEFORM},
    {"⬚", 0, TOOL_LASSO},
    {"◰", 0, TOOL_SELRECT},     // ou ⬛⃞ ou ◰ — un rectangle de sélection
    {"⬛", 1, INK_BLACK},
    {"⬜", 1, INK_WHITE},
    {"◌", 1, INK_ERASE},
    {"▣", 2, 0},
};


#define NUM_TOOLCELLS (int)(sizeof(TOOLCELLS)/sizeof(TOOLCELLS[0]))

 
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
                gTool = (HCTool)tc->value;
                gSelected = NULL;
                if (dbl) {
                    // double-clic : ouvrir la palette de réglage associée
                    if (tc->value == TOOL_FILL)
                        [(HCView *)gView showPatternPalette];
                    else if (tc->value == TOOL_PENCIL || tc->value == TOOL_ERASER ||
                             tc->value == TOOL_LINE || tc->value == TOOL_RECT ||
                             tc->value == TOOL_OVAL || tc->value == TOOL_FREEFORM)
                        [(HCView *)gView showWidthPalette];
                }
            }
            else if (tc->kind == 1) { gInk = (HCInk)tc->value; }
            else if (tc->kind == 2) { gShapeFilled = !gShapeFilled; }
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
