/* noms.c — un module aux noms d'externes longs et nombreux : la liste que
 * hc_module_externes en fait doit tenir dans le tampon qu'on lui donne,
 * des noms ENTIERS ou rien, sans jamais pointer au-delà. */
#include "hc_xcmd.h"

#define EXTERNE(n) HC_XCMD(n) { (void)paramPtr; }

EXTERNE(un_externe_au_nom_assez_long_pour_deborder_01)
EXTERNE(un_externe_au_nom_assez_long_pour_deborder_02)
EXTERNE(un_externe_au_nom_assez_long_pour_deborder_03)
EXTERNE(un_externe_au_nom_assez_long_pour_deborder_04)
EXTERNE(un_externe_au_nom_assez_long_pour_deborder_05)
EXTERNE(un_externe_au_nom_assez_long_pour_deborder_06)
EXTERNE(un_externe_au_nom_assez_long_pour_deborder_07)
EXTERNE(un_externe_au_nom_assez_long_pour_deborder_08)
