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

#endif /* HCpalettes_h */
