#ifndef HCpalettes_h
#define HCpalettes_h

#import "HCglobals.h"

#define NUM_PATTERNS 38
#define ICONGRID_COLS 6
#define ICONGRID_CELL 44

extern const int NUM_TOOLCELLS;

int pattern_bit(int pat, int x, int y);

@interface PatternPalette : NSView
@end

@interface ToolPalette : NSView
@end

@interface WidthPalette : NSView
@end
#define NUM_BRUSHES 12
int brush_bit(int brush, int x, int y);

@interface BrushPalette : NSView
@end

@interface IconGrid : NSView
@property (assign) int selected;
@end
#endif /* HCpalettes_h */
