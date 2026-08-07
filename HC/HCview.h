#ifndef HCview_h
#define HCview_h

#import <Cocoa/Cocoa.h>
#import "hc_core.h"
#import "HCglobals.h"   /* declare gView, une bonne fois pour toutes */

@interface HCView : NSView
- (void)installMessageBox;
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
- (void)toggleBackground:(id)sender;
- (void)installPatternPalette;
- (void)applyStackSize;
- (void)installWidthPalette;
- (void)installBrushPalette;
- (void)widthChosen:(id)sender;
- (void)toggleFilled:(id)sender;
- (void)ditherSelection:(id)sender;
- (void)showPatternPalette;
- (void)showWidthPalette;
- (void)showBrushPalette;
- (void)newBackground:(id)sender;
- (void)updateWindowTitle;
@end

#endif /* HCview_h */
