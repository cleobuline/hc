#ifndef graphics_h
#define graphics_h

#import <Cocoa/Cocoa.h>
#import "HCglobals.h"

#define NUM_PATTERNS 38

int pattern_bit(int pat, int x, int y);

NSBitmapImageRep *paint_bitmap(Object *o, int w, int h);

void paint_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, NSColor *color, CGFloat width);
void erase_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, CGFloat width);
void brush_stamp(NSBitmapImageRep *rep, int cx, int cy);
void brush_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to);

void paint_shape(NSBitmapImageRep *rep, HCTool tool, NSPoint a, NSPoint b, NSColor *color, CGFloat width);
void fill_shape(NSBitmapImageRep *rep, HCTool tool, NSPoint a, NSPoint b);
void flood_fill(NSBitmapImageRep *rep, int sx, int sy);

void paint_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n, CGFloat width);
void fill_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n);
void erase_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n);
void erase_rect(NSBitmapImageRep *rep, NSPoint a, NSPoint b);

void copy_rect(NSBitmapImageRep *rep, NSPoint a, NSPoint b);
void copy_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n);
void stamp_clipboard(NSBitmapImageRep *rep, NSPoint pos);

void dither_region(NSBitmapImageRep *rep, int x0, int y0, int x1, int y1,
                   NSPoint *poly, int npoly);

#endif /* graphics_h */
