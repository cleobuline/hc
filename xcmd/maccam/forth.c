/* forth.c — l'XFCN FORTH de MAC•CAM Workshop, en module de HC.
 *
 *     global pln0, pln1, pln2, pln3
 *     put FORTH(bg fld "forth") into bg fld "pile"
 *
 * Comme celle d'origine : elle compile le programme Forth qu'on lui donne,
 * range chaque table qu'une MAKE-TABLE a remplie dans la globale pln0 à
 * pln3 — 4 096 caractères « 0 » ou « 1 » —, et rend ce qui reste sur sa
 * pile, ou la faute qui l'a arrêtée. Le compilateur lui-même est
 * forth_coeur.c ; ce fichier n'est que son passage dans HC.
 *
 * UNE FAUTE N'EFFACE PAS les tables déjà faites : elles partent dans leurs
 * globales, et la fonction rend la faute. MESURÉ sur la carte « Fourmis »,
 * dont le texte finit par une phrase hors parenthèses (« avec la propriété
 * que… ») : l'original s'y arrêtait, et ses tables étaient pourtant là. */
#include <stdio.h>
#include "hc_xcmd.h"
#include "forth_coeur.h"

HC_XFCN(FORTH)
{
    static ForthSortie s;
    const char *source = paramPtr->paramCount > 0 ? paramPtr->params[0] : "";
    forth_compile(source, &s);
    for (int k = 0; k < FORTH_PLANS; k++) {
        if (!(s.ecrits & (1 << k))) continue;
        char nom[8];
        snprintf(nom, sizeof nom, "pln%d", k);
        SetGlobal(nom, s.plans[k]);
    }
    paramPtr->returnValue = s.rendu;
}
