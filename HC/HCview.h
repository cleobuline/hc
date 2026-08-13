#ifndef HCview_h
#define HCview_h

#import <Cocoa/Cocoa.h>
#import "hc_core.h"
#import "HCglobals.h"   /* declare gView, une bonne fois pour toutes */

@interface HCView : NSView <NSTextViewDelegate>
- (void)installMessageBox;
- (void)dropFloating;
- (void)findInStack:(id)sender;
- (void)messageBoxEntered:(id)sender;
- (void)installToolPalette;
- (void)toolChosen:(id)sender;
- (void)editScriptOf:(Object *)obj;
- (void)saveScript:(id)sender;
- (void)beginFieldEdit:(Object *)field;
- (void)endFieldEdit;
- (void)commitText;
- (void)testScribble;
- (void)flushPaintToKernel;
- (void)clearPaintCache;
- (void)resetForNewStack;
- (void)toggleBackground:(id)sender;
- (void)installPatternPalette;
- (void)applyStackSize;
- (void)installWidthPalette;
- (void)installBrushPalette;
- (void)startSprayTimer;
- (void)stopSprayTimer;
- (void)sprayTick:(NSTimer *)t;
- (void)showSprayPalette;
- (void)updateSprayLabels;
- (void)sprayRadiusChanged:(id)sender;
- (void)sprayDensityChanged:(id)sender;
- (void)widthChosen:(id)sender;
- (void)toggleFilled:(id)sender;
- (void)ditherSelection:(id)sender;
- (void)showPatternPalette;
- (void)showToolPalette;
- (void)togglePalette:(id)sender;
- (BOOL)paletteVisibleForTag:(NSInteger)tag;
- (void)showWidthPalette;
- (void)showBrushPalette;
- (void)newBackground:(id)sender;
- (void)updateWindowTitle;
@end

#endif /* HCview_h */
