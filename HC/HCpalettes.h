#ifndef HCpalettes_h
#define HCpalettes_h

#import "HCglobals.h"

#define NUM_PATTERNS 38

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
#endif /* HCpalettes_h */
