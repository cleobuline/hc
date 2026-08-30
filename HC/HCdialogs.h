#ifndef HCdialogs_h
#define HCdialogs_h

#import "HCview.h"

/* À appeler quand une pile se ferme, AVANT hc_free : referme le panneau Icônes
 * s'il montrait celle-là. NULL ferme dans tous les cas. */
void hcicon_panel_stack_closing(Object *stack);

@interface HCView (Dialogs)

- (void)showButtonInfo:(Object *)obj;
- (void)showCardInfo;
- (void)showBackgroundInfo;
- (void)showStackInfo;

- (void)infoOK:(id)sender;
- (void)infoCancel:(id)sender;
- (void)infoScript:(id)sender;
- (void)infoIcon:(id)sender;
- (void)editIcon:(id)sender;   /* Édition › Icône… */
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

/* Edition des icones, moitie droite du panneau Icones. */
- (void)iconRefresh;
- (void)iconCommitName;
- (void)iconSelect:(int)id;
- (void)iconPicked:(id)sender;
- (void)iconEdited:(id)sender;
- (void)iconNew:(id)sender;
- (void)iconDuplicate:(id)sender;
- (void)iconErase:(id)sender;
- (void)iconRotate:(id)sender;
- (void)iconDelete:(id)sender;
- (void)iconRename:(id)sender;

- (void)contentsOK:(id)sender;
- (void)contentsCancel:(id)sender;

- (void)showFieldInfo:(Object *)obj;

- (void)fldOK:(id)sender;
- (void)fldCancel:(id)sender;
- (void)fldScript:(id)sender;
- (void)fldTextStyle:(id)sender;

/* Panneau de styles, commun au bouton et au champ. */
- (void)infoTextStyle:(id)sender;
- (void)styleOK:(id)sender;
- (void)styleFont:(id)sender;
@end

#endif /* HCdialogs_h */
