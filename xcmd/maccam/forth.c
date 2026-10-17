/* forth.c — l'XFCN FORTH de MAC•CAM Workshop, en module de HC.
 *
 *     global pln0, pln1, pln2, pln3
 *     put FORTH(bg fld "forth") into bg fld "pile"
 *
 * Comme celle d'origine : elle compile et exécute le programme Forth qu'on
 * lui donne, range les quatre tables dans les globales pln0 à pln3 —
 * 4 096 caractères « 0 » ou « 1 » —, et rend sa pile, sommet en tête. Le
 * compilateur lui-même est forth_coeur.c ; ce fichier n'est que son passage
 * dans HC.
 *
 * LU DANS SON CODE 68000, et fait de même :
 *   · une faute est ENVOYÉE à la carte (SendCardMessage) — « COMPILE_ERR
 *     "Unknow'd word ", 13 » — pour les gestionnaires COMPILE_ERR, STACK_ERR,
 *     NEST_ERR et INTERPRET_ERR que les piles de l'utilisatrice portent ; la
 *     fonction rend la pile quand même ;
 *   · les quatre globales sont posées chaque fois, vides pour un plan
 *     qu'aucun >PLNk n'a rempli ;
 *   · BEEP sonne. L'original appelait SysBeep à chaque fois, et une règle qui
 *     sonne le faisait 4 096 fois : ici, dix au plus.
 *
 * UNE FAUTE N'EFFACE PAS les tables déjà faites. MESURÉ sur la carte
 * « Fourmis », dont le texte finit par une phrase hors parenthèses (« avec
 * la propriété que… ») : l'original s'y arrêtait, et ses tables étaient
 * pourtant là. */
#include <stdio.h>
#include "hc_xcmd.h"
#include "forth_coeur.h"

enum { BIPS_MAX = 10 };

HC_XFCN(FORTH)
{
    static ForthSortie s;
    const char *source = paramPtr->paramCount > 0 ? paramPtr->params[0] : "";
    forth_compile(source, &s);
    for (int b = 0; b < s.bips && b < BIPS_MAX; b++) SendCardMessage("beep");
    if (s.faute[0]) SendCardMessage(s.faute);
    for (int k = 0; k < FORTH_PLANS; k++) {
        char nom[8];
        snprintf(nom, sizeof nom, "pln%d", k);
        SetGlobal(nom, s.plans[k]);
    }
    paramPtr->returnValue = s.rendu;
}
