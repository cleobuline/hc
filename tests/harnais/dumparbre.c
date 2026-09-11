#include <stdio.h>
#include <string.h>
#include "hct_expr.h"
#include "hct_arbre.h"

static const char *G[] = {0};
static void aff(const HctNoeud *n, int p)
{
    if (!n) return;
    char t[64]; hct_texte(&n->jeton, t, sizeof t);
    for (int i=0;i<p;i++) printf("  ");
    printf("%s  jeton=«%s»", hct_genre_noeud_nom(n->genre), t);
    if (n->op) printf(" op=%s", n->op);
    if (n->article) printf(" THE");
    if (n->genre == HCTN_OBJET)
        printf(" typeobj=%d portee=%d des=%d rel=%d", n->typeobj, n->portee,
               n->designateur, n->relatif);
    printf(" nfils=%d\n", n->nfils);
    for (int i=0;i<n->nfils;i++) aff(n->fils[i], p+1);
}

int main(int argc, char **argv)
{
    (void)G;
    for (int a=1; a<argc; a++) {
        HctLot lot; HctReserve r; int nerr=0;
        memset(&r,0,sizeof r);
        printf("=== « %s » ===\n", argv[a]);
        HctNoeud *n = hct_analyse_texte(argv[a], &lot, &r, &nerr);
        printf("erreurs=%d\n", nerr);
        aff(n, 0);
        hct_reserve_libere(&r);
        printf("\n");
    }
    return 0;
}
