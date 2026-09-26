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

/* Le dessin, avec l'indexation de hcicon_draw : pixels[row*32+col]. */
static void dessine(const struct HcIconCouleur *c)
{
    static const char CAR[] = " 1234567890";
    for (int y = 0; y < HC_ICON_COTE; y++) {
        printf("   ");
        for (int x = 0; x < HC_ICON_COTE; x++) {
            int v = c->pixels[y * HC_ICON_COTE + x];
            putchar(v == 0 ? '.'
                    : (v > 0 && v < c->ncouleurs && v < (int)sizeof CAR - 1)
                      ? CAR[v] : '?');
        }
        putchar('\n');
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
            /* « . » pour la transparence, le chiffre de l'index sinon, et
             * « ? » RESERVE a un index hors palette : ce caractere-la ne doit
             * jamais apparaitre, et s'il apparait il dit quoi chercher. */
            dessine(c3);
            printf("   (. = transparent ; 1 rouge, cadre ; 2 vert, aplat ; 3 bleu, diagonale)\n");
        }
        if (r3) hc_free(r3);
    }

    printf("=== 10. METTRE EN COULEUR NE PERD PAS LE DESSIN ===\n");
    printf("   (le geste de l'editeur : une icone noir et blanc passe en\n");
    printf("    couleur, chaque pixel d'encre devenant l'index 1. Si elle se\n");
    printf("    vidait, on perdrait son travail au premier clic pour voir)\n");
    {
        Object *s3 = hc_new_stack("R");
        Object *b3 = hc_new_background(s3, "F");
        hc_new_card(s3, b3, "u");
        struct StackIcon *m = hc_icon_add(s3, 7, "damier");
        /* un damier de huit : dissymetrique verticalement, donc une rotation
         * du mauvais cote se verrait */
        for (int y = 0; y < HC_ICON_COTE; y++)
            for (int x = 0; x < HC_ICON_COTE; x++)
                if (y < 16 && ((x / 4 + y / 4) % 2) == 0)
                    m->bits[y * 4 + x / 8] |= (unsigned char)(0x80 >> (x % 8));
        int avant_bits = compte_encre(m->bits);
        hc_icon_couleur_depuis_bits(m, 0x20, 0x40, 0xC0);
        struct HcIconCouleur *cm = hc_icon_couleur(m);
        printf("   encre avant : %d   pixels peints apres : %d\n",
               avant_bits, compte_non_transparents(cm));
        printf("   palette : %d entrees\n", cm->ncouleurs);
        printf("   %s\n", avant_bits == compte_non_transparents(cm)
                            ? "le dessin est intact" : "ECART");
        printf("   et le geste est IDEMPOTENT (un double clic ne doit pas\n");
        printf("   reduire l'icone a deux couleurs) :\n");
        hc_icon_palette_pose(m, 2, 0xFF, 0x00, 0x00);
        hc_icon_couleur_depuis_bits(m, 0x00, 0xFF, 0x00);
        printf("   palette apres un second appel : %d entrees\n", cm->ncouleurs);

        printf("=== 11. LA ROTATION, ET SON SENS ===\n");
        printf("   (une icone tournee du MAUVAIS cote a exactement le meme\n");
        printf("    nombre d'encres : aucun compte ne peut le voir. Le dessin,\n");
        printf("    lui, le montre — et c'est comme ca qu'on a trouve que les\n");
        printf("    deux chemins, couleur et noir et blanc, tournaient en sens\n");
        printf("    OPPOSES a la premiere ecriture)\n");
        printf("   avant :\n");
        dessine(cm);
        hc_icon_tourne(m);
        printf("   apres un quart de tour :\n");
        dessine(cm);
        printf("   (le bloc du haut doit etre passe a DROITE)\n");

        printf("=== 12. LA PALETTE : poser, retrouver, deborder ===\n");
        struct StackIcon *q = hc_icon_add(s3, 8, "palette");
        struct HcIconCouleur *cq = hc_icon_couleur_cree(q);
        printf("   index 0 refuse (c'est la transparence) : %s\n",
               hc_icon_palette_pose(q, 0, 1, 2, 3) ? "ACCEPTE — defaut" : "refuse");
        printf("   rouge -> index %d\n", hc_icon_palette_index(q, 0xFF, 0, 0));
        printf("   vert  -> index %d\n", hc_icon_palette_index(q, 0, 0xFF, 0));
        printf("   rouge de nouveau -> index %d   (la meme, pas une nouvelle)\n",
               hc_icon_palette_index(q, 0xFF, 0, 0));
        printf("   palette : %d entrees\n", cq->ncouleurs);
        /* remplir jusqu'au bord */
        for (int i = cq->ncouleurs; i < HC_ICON_COULEURS_MAX; i++)
            hc_icon_palette_pose(q, i, (unsigned char)i, (unsigned char)i,
                                 (unsigned char)i);
        printf("   pleine a %d entrees ; une couleur de plus -> index %d\n",
               cq->ncouleurs, hc_icon_palette_index(q, 1, 2, 3));
        printf("   (0 veut dire « choisis la plus proche toi-meme » : le noyau\n");
        printf("    ne dessine pas, et « la plus proche » est une question de\n");
        printf("    perception, pas d'arithmetique)\n");

        printf("=== 13. UN PIXEL POSE MET LA SILHOUETTE A JOUR ===\n");
        printf("   (sinon `bits` serait faux entre deux enregistrements — et\n");
        printf("    c'est justement entre-temps que l'interface le lit pour\n");
        printf("    afficher le bouton)\n");
        struct StackIcon *z = hc_icon_add(s3, 9, "un pixel");
        hc_icon_couleur_cree(z);
        hc_icon_palette_pose(z, 1, 0xFF, 0xFF, 0x00);
        printf("   bits au depart : %d\n", compte_encre(z->bits));
        hc_icon_pixel_pose(z, 5, 5, 1);
        printf("   apres un pixel : %d   (lu : %d)\n",
               compte_encre(z->bits), hc_icon_pixel_lu(z, 5, 5));
        hc_icon_pixel_pose(z, 5, 5, 0);
        printf("   apres l'avoir efface : %d\n", compte_encre(z->bits));
        printf("   hors bornes ne deborde pas : ");
        hc_icon_pixel_pose(z, -1, 99, 1);
        printf("bits = %d, lu hors bornes = %d\n",
               compte_encre(z->bits), hc_icon_pixel_lu(z, 99, 99));

        hc_free(s3);
    }

    printf("=== 14. COLLER UNE IMAGE : peu de couleurs, donc SANS PERTE ===\n");
    printf("   (le cas courant — un bout de dessin, un logo, un aplat. Les\n");
    printf("    couleurs distinctes tiennent dans la palette, on les reprend\n");
    printf("    telles quelles, et aucun pixel ne change de teinte)\n");
    {
        Object *s4 = hc_new_stack("S");
        Object *b4 = hc_new_background(s4, "F");
        hc_new_card(s4, b4, "u");
        struct StackIcon *p1 = hc_icon_add(s4, 1, "quatre aplats");

        static unsigned char img[HC_ICON_PIXELS * 4];
        /* quatre quadrants francs, plus un coin transparent */
        for (int y = 0; y < HC_ICON_COTE; y++)
            for (int x = 0; x < HC_ICON_COTE; x++) {
                int i = (y * HC_ICON_COTE + x) * 4;
                int haut = y < 16, gauche = x < 16;
                img[i]   = haut ? (gauche ? 0xFF : 0x00) : (gauche ? 0x00 : 0xFF);
                img[i+1] = haut ? (gauche ? 0x00 : 0xFF) : (gauche ? 0x00 : 0xFF);
                img[i+2] = haut ? (gauche ? 0x00 : 0x00) : (gauche ? 0xFF : 0x00);
                img[i+3] = (x < 4 && y < 4) ? 0 : 255;      /* coin transparent */
            }
        int nc = hc_icon_colle_rvba(p1, img);
        struct HcIconCouleur *cp = hc_icon_couleur(p1);
        printf("   palette : %d entrees (4 couleurs + la transparence)\n", nc);

        /* verifier PIXEL PAR PIXEL que la teinte est la meme qu'a l'entree */
        int faux = 0, transp = 0;
        for (int i = 0; i < HC_ICON_PIXELS; i++) {
            int idx = cp->pixels[i];
            if (img[i*4+3] < 128) { if (idx != 0) faux++; else transp++; continue; }
            if (idx == 0) { faux++; continue; }
            if (cp->palette[idx][0] != img[i*4]   ||
                cp->palette[idx][1] != img[i*4+1] ||
                cp->palette[idx][2] != img[i*4+2]) faux++;
        }
        printf("   pixels dont la teinte a change : %d   (transparents : %d)\n",
               faux, transp);
        printf("   %s\n", faux == 0 ? "COLLAGE SANS PERTE" : "ECART");
        printf("   et le dessin :\n");
        dessine(cp);

        printf("=== 15. COLLER UNE IMAGE DE PLUS DE 255 COULEURS ===\n");
        printf("   (une photo, ou un degrade. La decoupe mediane construit\n");
        printf("    255 boites, chacune rendant la moyenne de ce qu'elle\n");
        printf("    contient. On coupe la boite la plus ETENDUE et non la plus\n");
        printf("    peuplee : une photo a des milliers de pixels de ciel et\n");
        printf("    quelques-uns de rouge vif, et couper par population\n");
        printf("    noierait le rouge)\n");
        struct StackIcon *p2 = hc_icon_add(s4, 2, "degrade");
        for (int y = 0; y < HC_ICON_COTE; y++)
            for (int x = 0; x < HC_ICON_COTE; x++) {
                int i = (y * HC_ICON_COTE + x) * 4;
                /* un degrade continu : 1024 couleurs toutes differentes */
                img[i]   = (unsigned char)(x * 8);
                img[i+1] = (unsigned char)(y * 8);
                img[i+2] = (unsigned char)((x + y) * 4);
                img[i+3] = 255;
            }
        int nc2 = hc_icon_colle_rvba(p2, img);
        struct HcIconCouleur *cq2 = hc_icon_couleur(p2);
        printf("   1024 couleurs distinctes en entree -> %d en palette\n", nc2);

        /* l'erreur maximale et moyenne, en distance sur un canal */
        long pire = 0, somme = 0;
        for (int i = 0; i < HC_ICON_PIXELS; i++) {
            int idx = cq2->pixels[i];
            long d = 0;
            for (int k = 0; k < 3; k++) {
                long e = (long)img[i*4+k] - cq2->palette[idx][k];
                if (e < 0) e = -e;
                if (e > d) d = e;
            }
            somme += d;
            if (d > pire) pire = d;
        }
        printf("   ecart maximal sur un canal : %ld sur 255\n", pire);
        printf("   ecart moyen                : %ld\n", somme / HC_ICON_PIXELS);
        printf("   (un ecart maximal de quelques unites veut dire que l'oeil\n");
        printf("    ne verra pas la difference ; c'est ce qu'on attend d'une\n");
        printf("    decoupe qui dispose de 255 couleurs pour 1024 pixels)\n");

        printf("=== 16. UNE IMAGE ENTIEREMENT TRANSPARENTE VIDE L'ICONE ===\n");
        printf("   (et c'est la bonne reponse : coller du vide donne du vide,\n");
        printf("    pas une icone noire ni un refus)\n");
        struct StackIcon *p3 = hc_icon_add(s4, 3, "rien");
        memset(img, 0, sizeof img);
        int nc3 = hc_icon_colle_rvba(p3, img);
        struct HcIconCouleur *cq3 = hc_icon_couleur(p3);
        printf("   palette : %d   pixels peints : %d   bits d'encre : %d\n",
               nc3, compte_non_transparents(cq3), compte_encre(p3->bits));

        printf("=== 17. LE COLLAGE SURVIT A L'ALLER-RETOUR ===\n");
        hc_save(s4, FIC);
        Object *r4 = hc_load(FIC);
        struct StackIcon *q2 = r4 ? hc_icon_get(r4, 2) : NULL;
        struct HcIconCouleur *cr = q2 ? hc_icon_couleur(q2) : NULL;
        if (!cr) printf("   [ERR] relecture\n");
        else {
            int dp = 0, dc = 0;
            for (int i = 0; i < HC_ICON_PIXELS; i++)
                if (cr->pixels[i] != cq2->pixels[i]) dp++;
            for (int i = 0; i < cq2->ncouleurs; i++)
                if (memcmp(cr->palette[i], cq2->palette[i], 3)) dc++;
            printf("   %d couleurs, %d differentes ; %d pixels differents\n",
                   cr->ncouleurs, dc, dp);
            printf("   %s\n", (!dp && !dc) ? "IDENTIQUE" : "ECART");
        }
        if (r4) hc_free(r4);
        hc_free(s4);
    }

    hc_free(rl); hc_free(st);
    remove(FIC);
    return 0;
}
