#ifndef HCdialogs_h
#define HCdialogs_h

#import "HCview.h"

@interface HCView (Dialogs)

- (void)showButtonInfo:(Object *)obj;
- (void)showCardInfo;
- (void)showBackgroundInfo;
- (void)showStackInfo;

- (void)infoOK:(id)sender;
- (void)infoCancel:(id)sender;
- (void)infoScript:(id)sender;
- (void)infoIcon:(id)sender;
- (void)infoContents:(id)sender;

- (void)cardOK:(id)sender;
- (void)cardCancel:(id)sender;
- (void)cardScript:(id)sender;

- (void)bgOK:(id)sender;
- (void)bgCancel:(id)sender;
- (void)bgScript:(id)sender;

- (void)stackOK:(id)sender;
- (void)stackCancel:(id)sender;
- (void)stackScript:(id)sender;

- (void)iconOK:(id)sender;
- (void)iconNone:(id)sender;
- (void)iconCancel:(id)sender;

- (void)contentsOK:(id)sender;
- (void)contentsCancel:(id)sender;

@end

#endif /* HCdialogs_h */
