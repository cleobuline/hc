#ifndef HCvisual_h
#define HCvisual_h

#import <Cocoa/Cocoa.h>
#import "hc_core.h"
#import "HCview.h"  /* pour @class HCView */

/* ═══ Effets de transition ═══════════════════════════════════════════════════
 *
 * Tout l'etat de l'animation — image de depart, image d'arrivee, nom de
 * l'effet, etape courante, drapeaux — reste prive a HCvisual.m. Deux choses
 * seulement en sortent. */

/* Ce que le noyau appelle sur « visual effect ... ». Branche sur
 * host.visual_effect au demarrage. */
void cocoa_visual_effect(const char *effect, const char *speed,
                         const char *image);

/* Une transition attend-elle d'etre jouee ? Consulte par drawRect:. */
BOOL visual_pending(void);

/* Les deux methodes qui jouent l'animation vivent avec leur etat.
 * runVisualTransition est deja declaree dans HCview.h ; drawVisualStep ne
 * l'etait pas, drawRect: etant son seul appelant. */
@interface HCView (Visual)
- (void)runVisualTransition;
- (BOOL)drawVisualStep;
@end

#endif /* HCvisual_h */
