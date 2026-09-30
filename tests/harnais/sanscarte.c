/* SANS CARTE COURANTE, RIEN NE PLANTE.
 *
 * « go first background », tapé avant qu'aucune carte ne soit posée,
 * déréférençait stack->nparts avec stack NULL : resolve_local tire la pile de
 * la carte courante. Signalé par un audit extérieur, qui l'avait trouvé avec
 * clang --analyze ; reproduit sous ASan avant d'être corrigé — SEGV à
 * hc_core.c:3779.
 *
 * On balaie les désignateurs qui passent par là, et leurs voisins : un
 * refus est attendu, un plantage non. Ce que le noyau DIT ici n'est pas
 * comparé à HyperCard : il n'existe pas d'HyperCard sans pile ouverte. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG || k == HC_ERR) printf("      %s\n", t);
}

static void essai(const char *ligne)
{
    printf("── %s\n", ligne);
    hc_do(ligne);
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    printf("carte courante : %s\n", hc_current_card() ? "oui" : "aucune");

    essai("go first background");
    essai("go last bkgnd");
    essai("go next background");
    essai("go prev bkgnd");
    essai("put the name of first background");
    essai("put the number of cards");
    essai("go first card");

    puts("fini sans planter");
    return 0;
}
