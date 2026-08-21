#ifndef HCview_h
#define HCview_h

#import <Cocoa/Cocoa.h>
#import "hc_core.h"
#import "HCglobals.h"   /* declare gView, une bonne fois pour toutes */

@interface HCView : NSView <NSTextViewDelegate>
- (void)installMessageBox;
/* La minuterie d'« idle ». Une seule pour toute l'application : elle envoie le
 * message à la carte de la fenêtre active, jamais aux autres. */
- (void)startIdleTimer;
- (void)stopIdleTimer;
- (void)idleTick:(NSTimer *)t;
/* L'état de document de cette vue, et la carte qu'elle affiche. Chaque fenêtre
 * a les siens ; HCDocument désigne l'actif par hc_set_active_doc. */
- (void *)docState;
- (Object *)documentCard;
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
- (void)showDrawColorPanel:(BOOL)ink;
@end

#endif /* HCview_h */
