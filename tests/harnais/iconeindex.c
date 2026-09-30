/* Un pixel d'icône ne peut pas désigner une couleur que la palette n'a pas.
 *
 * Signalé par un audit extérieur : hc_icon_pixel_pose refusait un index hors
 * du TABLEAU (256) mais pas hors de la PALETTE. Avec quatre couleurs, poser
 * l'index 4 — ou 200 — était accepté. La silhouette comptait ce pixel comme
 * encre (il n'est pas nul), l'éditeur le dessinait transparent (il n'a pas de
 * couleur) : deux vérités, fabriquées par la primitive même qui garde
 * l'invariant.
 *
 * Et ce n'était pas théorique : l'éditeur garde la couleur choisie d'une icône
 * à l'autre. Choisir la couleur 7 d'une icône qui en a huit, passer à une
 * icône qui en a trois, peindre : l'index 7 était posé.
 *
 * LE SITE JUMEAU est le lecteur de fichiers. put_cicon écrit la palette AVANT
 * les pixels, et son commentaire en donnait la raison — « le relecteur peut
 * ainsi vérifier qu'un index désigne une couleur connue ». Le relecteur ne le
 * vérifiait pas. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define F1 "/tmp/hc_iconeindex_a.stack"
#define F2 "/tmp/hc_iconeindex_b.stack"

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)k; (void)d; (void)t; }

static int encres(const struct StackIcon *ic)
{
    int n = 0;
    for (int i = 0; i < HC_ICON_BYTES; i++)
        for (int b = 0; b < 8; b++) n += (ic->bits[i] >> b) & 1;
    return n;
}

static void pose(struct StackIcon *ic, int index)
{
    hc_icon_pixel_pose(ic, 5, 5, index);
    printf("   poser l'index %3d : pixel = %d, encres = %d\n",
           index, hc_icon_pixel_lu(ic, 5, 5), encres(ic));
}

/* Recopie F1 dans F2 en changeant, dans le bloc ciconres, la PREMIÈRE ligne
 * de pixels : son premier octet devient `hex`. */
static int abime_un_pixel(const char *hex)
{
    FILE *a = fopen(F1, "r"), *b = fopen(F2, "w");
    if (!a || !b) { if (a) fclose(a); if (b) fclose(b); return 0; }
    char l[4096];
    int dans = 0, fait = 0;
    while (fgets(l, sizeof l, a)) {
        if (strncmp(l, "ciconres ", 9) == 0) dans = 1;
        else if (strncmp(l, "end ciconres", 12) == 0) dans = 0;
        else if (dans && !fait && l[0] == '|' && l[1] == ' ' && l[2] != 'c') {
            l[2] = hex[0]; l[3] = hex[1];
            fait = 1;
        }
        fputs(l, b);
    }
    fclose(a); fclose(b);
    return fait;
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("S");
    Object *bg = hc_new_background(st, "F");
    hc_new_card(st, bg, "U");
    struct StackIcon *ic = hc_icon_add(st, 500, "quatre");
    memset(ic->bits, 0, HC_ICON_BYTES);
    hc_icon_couleur_depuis_bits(ic, 0, 0, 0);          /* palette : 0, 1 */
    hc_icon_palette_pose(ic, 2, 255, 0, 0);
    hc_icon_palette_pose(ic, 3, 0, 255, 0);            /* palette : 0..3 */
    printf("== la primitive : palette de %d couleurs ==\n", hc_icon_couleur(ic)->ncouleurs);
    pose(ic, 3);
    pose(ic, 4);
    pose(ic, 200);
    pose(ic, 0);

    puts("\n== le lecteur ==");
    pose(ic, 2);
    if (hc_save(st, F1) != 0) { puts("   écriture impossible"); return 1; }
    Object *t = hc_load(F1);
    printf("   la pile enregistrée : %s\n", t ? "ouverte" : hc_load_erreur());
    if (t) hc_free(t);

    if (!abime_un_pixel("03")) { puts("   pas de ligne de pixels"); return 1; }
    t = hc_load(F2);
    printf("   un pixel à l'index 3 (dans la palette) : %s\n", t ? "ouverte" : hc_load_erreur());
    if (t) hc_free(t);

    if (!abime_un_pixel("04")) { puts("   pas de ligne de pixels"); return 1; }
    t = hc_load(F2);
    printf("   un pixel à l'index 4 (hors palette)    : %s\n", t ? "OUVERTE, à tort" : hc_load_erreur());
    if (t) hc_free(t);

    if (!abime_un_pixel("FF")) { puts("   pas de ligne de pixels"); return 1; }
    t = hc_load(F2);
    printf("   un pixel à l'index 255                 : %s\n", t ? "OUVERTE, à tort" : hc_load_erreur());
    if (t) hc_free(t);

    hc_free(st);
    remove(F1);
    remove(F2);
    return 0;
}
