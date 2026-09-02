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

/* ═══ Ecran gele (« lock screen ») ═══════════════════════════════════════════
 *
 * Le verrou ne suspend pas le dessin : il fige l'image montree, le script
 * peignant dessous. Meme machinerie de photographie que les transitions, d'ou
 * leur cohabitation dans ce fichier. */
void hcv_lock_screen(void);
void hcv_unlock_screen(void);
BOOL hcv_screen_locked(void);

/* Appelee en tete de drawRect:. Rend YES si l'image gelee a ete dessinee, et
 * le dessin normal de la carte est alors a sauter. */
BOOL hcv_draw_locked(NSView *v);

/* Les deux methodes qui jouent l'animation vivent avec leur etat.
 * runVisualTransition est deja declaree dans HCview.h ; drawVisualStep ne
 * l'etait pas, drawRect: etant son seul appelant. */
@interface HCView (Visual)
- (void)runVisualTransition;
- (BOOL)drawVisualStep;
@end

#endif /* HCvisual_h */
