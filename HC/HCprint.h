#ifndef HCprint_h
#define HCprint_h

#import <Cocoa/Cocoa.h>
#import "hc_core.h"

/* ═══ Impression ═════════════════════════════════════════════════════════════
 *
 * Une seule chose franchit la frontiere. La vue jetable qui dessine les pages
 * (HCPrintView) reste privee a HCprint.m : elle n'a qu'un appelant, et
 * l'exporter n'inviterait qu'a s'en servir ailleurs. */

/* Imprime les cartes donnees, une par page. Branchee sur host.print_cards au
 * demarrage, et appelee directement par le print: de la vue.
 *
 * `decoupe` vaut NULL pour la carte entiere, ou pointe quatre entiers en
 * coordonnees carte — gauche, haut, droite, bas — pour « print card from x,y to
 * x,y », qui n'imprime qu'une partie de la carte. */
void cocoa_print_cards(Object **cards, int n, const int *decoupe);

#endif /* HCprint_h */
