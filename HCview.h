#import <Cocoa/Cocoa.h>
#import "hc_core.h"

@interface HCView : NSView
- (void)installMessageBox;
- (void)messageBoxEntered:(id)sender;
- (void)installToolPalette;
- (void)toolChosen:(id)sender;
- (void)editScriptOf:(Object *)obj;
- (void)saveScript:(id)sender;
@end

extern HCView *gView;
