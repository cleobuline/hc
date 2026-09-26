/* LES ICÔNES EN COULEUR : le format, son aller-retour, et sa silhouette.
 *
 * HyperCard n'avait que des ressources ICON — 32×32 en un bit. Les piles qui
 * voulaient de la couleur passaient par des extensions à ressources cicn, que
 * HC n'a jamais sues lire. Le format est donc le nôtre, et ce harnais est sa
 * spécification exécutable.
 *
 * UN OCTET PAR PIXEL, index dans la palette de l'icône, INDEX 0 TRANSPARENT.
 * C'est cette transparence par l'index qui remplace le masque d'une cicn :
 * une seule image à tenir au lieu de deux qui peuvent se contredire.
 *
 * POURQUOI INDEXÉ ET NON UN PNG EN BASE64, et c'est la raison d'être de ce
 * fichier : un PNG serait opaque au noyau, donc INTESTABLE. Tout le format
 * vivrait dans la couche Cocoa, la seule dont chaque note de version répète
 * qu'elle n'a aucun test automatique. Ici, les mille vingt-quatre pixels
 * s'écrivent, se relisent et se comparent un par un, sous Linux, à chaque
 * make test.
 *
 * LA SILHOUETTE EST LA CLÉ DE VOÛTE. `bits` — les cent vingt-huit octets du
 * noir et blanc — reste JUSTE quand une icône passe en couleur : tout pixel
 * non transparent y devient de l'encre. Sans ça il aurait fallu convertir un
 * par un les cent chemins de code qui lisent `bits` sans savoir qu'une
 * couleur existe à côté. Avec, ils continuent de marcher sans le savoir.
 *
 * ET LES DEUX BLOCS SONT ÉCRITS, iconres puis ciconres, pour la même raison
 * vue depuis le fichier : un binaire d'avant saute « ciconres » qu'il ne
 * connaît pas, mais lit « iconres » — il retrouve donc l'icône en noir et
 * blanc au lieu de ne rien retrouver. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

#define FIC "/tmp/hc_cicon.stack"

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

/* Un petit motif reconnaissable : un cadre rouge, un fond vert, une diagonale
 * bleue. Assez simple pour se décrire en une ligne, assez varié pour qu'une
 * erreur d'indice ou de ligne se voie. */
static void peins(struct HcIconCouleur *c)
{
    c->ncouleurs = 4;
    c->palette[1][0] = 0xFF; c->palette[1][1] = 0x00; c->palette[1][2] = 0x00;
    c->palette[2][0] = 0x00; c->palette[2][1] = 0x80; c->palette[2][2] = 0x00;
    c->palette[3][0] = 0x00; c->palette[3][1] = 0x00; c->palette[3][2] = 0xFF;
    for (int y = 0; y < HC_ICON_COTE; y++)
        for (int x = 0; x < HC_ICON_COTE; x++) {
            int i = y * HC_ICON_COTE + x;
            if (x == 0 || y == 0 || x == 31 || y == 31) c->pixels[i] = 1;
            else if (x == y)                            c->pixels[i] = 3;
            else if (x > 7 && x < 24 && y > 7 && y < 24) c->pixels[i] = 2;
            else                                         c->pixels[i] = 0;
        }
}

static int compte_encre(const unsigned char *bits)
{
    int n = 0;
    for (int i = 0; i < HC_ICON_BYTES; i++)
        for (int b = 0; b < 8; b++) if (bits[i] & (0x80 >> b)) n++;
    return n;
}

static int compte_non_transparents(const struct HcIconCouleur *c)
{
    int n = 0;
    for (int i = 0; i < HC_ICON_PIXELS; i++) if (c->pixels[i]) n++;
    return n;
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);

    printf("=== 1. une icône neuve est en NOIR ET BLANC ===\n");
    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *cd = hc_new_card(st, bg, "Une");
    hc_set_current_card(cd);
    struct StackIcon *ic = hc_icon_add(st, 20554, "Terminator");
    printf("   couleur : %s\n", hc_icon_couleur(ic) ? "OUI" : "non — c'est le défaut");

    printf("=== 2. la passer en couleur, et la peindre ===\n");
    struct HcIconCouleur *c = hc_icon_couleur_cree(ic);
    if (!c) { printf("   [ERR] création impossible\n"); return 1; }
    peins(c);
    hc_icon_silhouette(ic);
    printf("   couleurs de la palette : %d\n", c->ncouleurs);
    printf("   pixels non transparents : %d sur %d\n",
           compte_non_transparents(c), HC_ICON_PIXELS);

    printf("=== 3. LA SILHOUETTE se dérive : tout pixel peint est de l'encre ===\n");
    printf("   bits d'encre : %d   (doit égaler le compte ci-dessus)\n",
           compte_encre(ic->bits));
    printf("   %s\n", compte_encre(ic->bits) == compte_non_transparents(c)
                        ? "les deux comptes concordent" : "ECART");

    printf("=== 4. ALLER-RETOUR par le fichier, pixel par pixel ===\n");
    if (hc_save(st, FIC) != 0) { printf("   [ERR] écriture\n"); return 1; }
    Object *rl = hc_load(FIC);
    if (!rl) { printf("   [ERR] relecture : %s\n", hc_load_erreur()); return 1; }
    struct StackIcon *ic2 = hc_icon_get(rl, 20554);
    if (!ic2) { printf("   [ERR] icône absente après relecture\n"); return 1; }
    struct HcIconCouleur *c2 = hc_icon_couleur(ic2);
    printf("   nom          : « %s »\n", ic2->name ? ic2->name : "(nul)");
    printf("   couleur      : %s\n", c2 ? "retrouvée" : "PERDUE");
    if (c2) {
        int dpix = 0, dpal = 0;
        for (int i = 0; i < HC_ICON_PIXELS; i++)
            if (c->pixels[i] != c2->pixels[i]) dpix++;
        for (int i = 0; i < c->ncouleurs; i++)
            if (memcmp(c->palette[i], c2->palette[i], 3)) dpal++;
        printf("   palette      : %d entrées, %d différentes\n", c2->ncouleurs, dpal);
        printf("   pixels       : %d différents sur %d\n", dpix, HC_ICON_PIXELS);
        printf("   %s\n", (!dpix && !dpal && c2->ncouleurs == c->ncouleurs)
                            ? "IDENTIQUE au pixel près" : "ECART");
    }
    printf("   silhouette   : %d bits d'encre relus\n", compte_encre(ic2->bits));

    printf("=== 5. LES DEUX BLOCS sont dans le fichier ===\n");
    printf("   (iconres pour un binaire d'avant, ciconres pour la couleur :\n");
    printf("    une pile qui gagne de la couleur ne devient pas illisible)\n");
    {
        FILE *f = fopen(FIC, "r");
        int a_iconres = 0, a_ciconres = 0, lignes_pix = 0;
        char l[4096];
        while (f && fgets(l, sizeof l, f)) {
            if (!strncmp(l, "iconres ", 8))  a_iconres = 1;
            if (!strncmp(l, "ciconres ", 9)) a_ciconres = 1;
            if (a_ciconres && l[0] == '|' && l[2] != 'c') lignes_pix++;
        }
        if (f) fclose(f);
        printf("   iconres  : %s\n", a_iconres  ? "présent" : "ABSENT");
        printf("   ciconres : %s\n", a_ciconres ? "présent" : "ABSENT");
        printf("   lignes de pixels : %d   (une par ligne de l'icône)\n", lignes_pix);
    }

    printf("=== 6. RETIRER la couleur garde la silhouette ===\n");
    printf("   (l'effacer avec les couleurs ferait disparaître l'icône entière\n");
    printf("    pour qui voulait seulement lui retirer sa couleur)\n");
    int avant = compte_encre(ic2->bits);
    hc_icon_couleur_ote(ic2);
    printf("   couleur : %s\n", hc_icon_couleur(ic2) ? "encore là" : "retirée");
    printf("   bits    : %d avant, %d après\n", avant, compte_encre(ic2->bits));

    printf("=== 7. une icône NOIR ET BLANC n'écrit pas de bloc couleur ===\n");
    {
        Object *s2 = hc_new_stack("Q");
        Object *b2 = hc_new_background(s2, "F");
        hc_new_card(s2, b2, "u");
        struct StackIcon *m = hc_icon_add(s2, 3, "mono");
        for (int i = 0; i < HC_ICON_BYTES; i++) m->bits[i] = 0xAA;
        hc_save(s2, FIC);
        FILE *f = fopen(FIC, "r");
        int a_ciconres = 0; char l[4096];
        while (f && fgets(l, sizeof l, f))
            if (!strncmp(l, "ciconres ", 9)) a_ciconres = 1;
        if (f) fclose(f);
        printf("   ciconres dans le fichier : %s\n",
               a_ciconres ? "OUI — ce serait un défaut" : "non, comme il se doit");
        hc_free(s2);
    }

    printf("=== 8. UN BLOC ABÎMÉ REFUSE LE FICHIER ===\n");
    printf("   (une image complétée de zéros revient à moitié en silence ;\n");
    printf("    on a décidé une fois pour toutes que ça n'est pas acceptable)\n");
    {
        hc_save(st, FIC);
        /* couper une ligne de pixels du bloc couleur */
        FILE *f = fopen(FIC, "r");
        char tout[200000]; size_t n = f ? fread(tout, 1, sizeof tout - 1, f) : 0;
        if (f) fclose(f);
        tout[n] = 0;
        char *p = strstr(tout, "end ciconres");
        if (p) {
            /* reculer d'une ligne de pixels et la supprimer */
            char *q = p - 1;
            while (q > tout && *q != '\n') q--;
            if (q > tout) { char *r = q - 1; while (r > tout && *r != '\n') r--;
                            memmove(r, q, strlen(q) + 1); }
            f = fopen(FIC, "w");
            if (f) { fputs(tout, f); fclose(f); }
            Object *mauvais = hc_load(FIC);
            printf("   relecture : %s\n",
                   mauvais ? "ACCEPTEE — ce serait un défaut" : "refusée");
            printf("   raison    : %s\n", mauvais ? "" : hc_load_erreur());
            if (mauvais) hc_free(mauvais);
        }
    }

    printf("=== 9. L'ICONE, DESSINEE — la garde contre une transposition ===\n");
    printf("   (l'Objective-C ne se compile pas ici, donc hcicon_draw n'a aucun\n");
    printf("    test. Ce qu'on PEUT garder, c'est l'indexation : ce dessin\n");
    printf("    utilise pixels[row*32+col], exactement celle de hcicon_draw.\n");
    printf("    Une ligne et une colonne echangees se verraient ici d'un coup\n");
    printf("    d'oeil, alors qu'elles passeraient inapercues dans un compte\n");
    printf("    de pixels — le motif est expres dissymetrique : cadre, aplat\n");
    printf("    central, et UNE diagonale qui descend vers la droite)\n");
    {
        /* On relit depuis le fichier plutot que depuis la memoire : le dessin
         * garde ainsi l'aller-retour autant que l'indexation. */
        hc_save(st, FIC);
        Object *r3 = hc_load(FIC);
        struct StackIcon *i3 = r3 ? hc_icon_get(r3, 20554) : NULL;
        struct HcIconCouleur *c3 = i3 ? hc_icon_couleur(i3) : NULL;
        if (!c3) printf("   [ERR] relecture pour le dessin\n");
        else {
            static const char CAR[] = " 1234567890";
            for (int y = 0; y < HC_ICON_COTE; y++) {
                printf("   ");
                for (int x = 0; x < HC_ICON_COTE; x++) {
                    /* « . » pour la transparence, le chiffre de l'index
                     * sinon, et « ? » RESERVE a un index hors palette : ce
                     * caractere-la ne doit jamais apparaitre, et s'il
                     * apparait il dit tout de suite quoi chercher. */
                    int v = c3->pixels[y * HC_ICON_COTE + x];
                    putchar(v == 0 ? '.'
                            : (v > 0 && v < c3->ncouleurs && v < (int)sizeof CAR - 1)
                              ? CAR[v] : '?');
                }
                putchar('\n');
            }
            printf("   (. = transparent ; 1 rouge, cadre ; 2 vert, aplat ; 3 bleu, diagonale)\n");
        }
        if (r3) hc_free(r3);
    }

    hc_free(rl); hc_free(st);
    remove(FIC);
    return 0;
}
