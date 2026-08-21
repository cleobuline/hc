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
 * demarrage, et appelee directement par le print: de la vue. */
void cocoa_print_cards(Object **cards, int n);

#endif /* HCprint_h */
