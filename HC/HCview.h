#import <Cocoa/Cocoa.h>
#import "hc_core.h"

@interface HCView : NSView
- (void)installMessageBox;
- (void)messageBoxEntered:(id)sender;
- (void)installToolPalette;
- (void)toolChosen:(id)sender;
- (void)editScriptOf:(Object *)obj;
- (void)saveScript:(id)sender;
- (void)beginFieldEdit:(Object *)field;
- (void)endFieldEdit;
- (void)testScribble;
- (void)flushPaintToKernel;
- (void)clearPaintCache;
- (void)toggleBackground:(id)sender;
- (void)installPatternPalette;
- (void)applyStackSize;
- (void)installWidthPalette;
- (void)widthChosen:(id)sender;
- (void)toggleFilled:(id)sender;
- (void)ditherSelection:(id)sender;
- (void)showPatternPalette;
- (void)showWidthPalette;
- (void)showBrushPalette;
- (void)showButtonInfo:(Object *)obj;
- (void)infoContents:(id)sender;
- (void)contentsOK:(id)sender;
- (void)contentsCancel:(id)sender;
- (void)showPopupMenuFor:(Object *)o atPoint:(NSPoint)p;
- (void)popupChosen:(id)sender;
- (void)newBackground:(id)sender;
- (void)showBackgroundInfo;
- (void)bgOK:(id)sender;
- (void)bgCancel:(id)sender;
- (void)bgScript:(id)sender;
- (void)showCardInfo;
- (void)cardOK:(id)sender;
- (void)cardCancel:(id)sender;
- (void)cardScript:(id)sender;
- (void)showStackInfo;
- (void)stackOK:(id)sender;
- (void)stackCancel:(id)sender;
- (void)stackScript:(id)sender;
- (void)updateWindowTitle;
@end

extern HCView *gView;
