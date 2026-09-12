/* Charge un script, et dit ce que l'analyseur v3 en pense, ligne par ligne. */
#include "hct_lex.h"
#include "hct_expr.h"
#include "hct_arbre.h"
#include "hct_bloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fautes(const HctNoeud *n, const char *src)
{
    if (!n) return;
    if (n->genre == HCTN_ERREUR) {
        char t[64]; hct_texte(&n->jeton, t, sizeof t);
        printf("  ligne %-3d col %-3d : %s   (près de « %s »)\n",
               n->jeton.ligne, n->jeton.col, n->msg ? n->msg : "?", t);
        /* la ligne source */
        int l = 1; const char *p = src, *deb = src;
        while (*p && l < n->jeton.ligne) { if (*p=='\n') { l++; deb = p+1; } p++; }
        const char *fin = strchr(deb, '\n'); if (!fin) fin = deb + strlen(deb);
        printf("        %.*s\n", (int)(fin-deb), deb);
    }
    for (int i = 0; i < n->nfils; i++) fautes(n->fils[i], src);
}
int main(int argc, char **argv){
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb"); if(!f){perror(argv[1]);return 2;}
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    char *src = malloc((size_t)n+1); fread(src,1,(size_t)n,f); src[n]=0; fclose(f);
    HctLot lot; HctReserve r; memset(&r,0,sizeof r);
    int sain = hct_lex(src, &lot);
    HctAnalyseur an; hct_analyseur_init(&an, &lot, &r);
    HctNoeud *a = hct_bloc_script(&an);
    printf("=== %s ===\n", argv[1]);
    printf("lexeur   : %s\n", sain ? "propre" : "EN FAUTE");
    printf("erreurs  : %d\n", an.nerreurs);
    printf("racine   : %s, %d enfants\n",
           a ? hct_genre_noeud_nom(a->genre) : "(nulle)", a ? a->nfils : 0);
    int structure_ok = (a != NULL);
    for (int i = 0; a && i < a->nfils; i++) {
        const HctNoeud *f = a->fils[i];
        char t[64]; hct_texte(&f->jeton, t, sizeof t);
        if (f->genre != HCTN_GESTIONNAIRE) {
            structure_ok = 0;
            printf("  !! enfant %d : %s  ligne %d  « %s »\n",
                   i, hct_genre_noeud_nom(f->genre), f->jeton.ligne, t);
        }
    }
    printf("structure: %s\n", structure_ok ? "tient" : "CASSÉE");
    printf("verdict  : la v3 %s ce script\n",
           (sain && a && (an.nerreurs == 0 || structure_ok)) ? "ACCEPTE" : "REFUSE");
    fautes(a, src);
    /* Rendre ce qu'on a pris. Un harnais qui fuit apprend à ignorer les
     * rapports de fuite — et c'est justement l'angle mort qui vient de laisser
     * passer une vraie fuite du noyau pendant deux jours. */
    hct_reserve_libere(&r);
    hct_lot_libere(&lot);
    free(src);
    return 0;
}
