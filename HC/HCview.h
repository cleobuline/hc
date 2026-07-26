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
@end

extern HCView *gView;
