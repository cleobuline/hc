/* LES TRANSFORMATIONS DU MENU PAINT, MESURÉES.
 *
 * hc_pixels.h le demandait depuis le premier jour, dans son propre en-tête :
 * « un banc d'essai peut l'inclure à son tour et vérifier chaque
 * transformation sur des images de quelques pixels, ce qu'aucun test à
 * l'écran ne ferait aussi bien ». L'invitation était écrite ; personne ne
 * l'avait acceptée, et c'est un défaut de Trace Edges qui l'a fait ouvrir.
 *
 * SIGNALÉ : « si le trait est noir sur transparent, Trace Edges trace les
 * bords ; mais si c'est une trace noire sur du peint en blanc, Trace Edges
 * rend la sélection transparente ».
 *
 * La cause : « posé » ne regardait que l'alpha. Sur du blanc OPAQUE, tous
 * les pixels sont posés — bloc plein, aucun bord, tout se vide. Le même
 * défaut que celui du crayon, corrigé la veille, dont le jumeau était resté
 * ici.
 *
 * Les images font quelques pixels et se lisent à l'œil : c'est le seul
 * format où une erreur d'un pixel se VOIT.
 */
#include "hc_pixels.h"
#include <stdio.h>

#define W 8
#define H 6
#define SPP 4
#define BPR (W * SPP)

static unsigned char img[H * BPR];

static void efface(void)        { memset(img, 0, sizeof img); }
static unsigned char *px(int x, int y) { return HCP_PX(img, BPR, SPP, x, y); }

static void pose(int x, int y, unsigned char r, unsigned char g,
                 unsigned char b, unsigned char a)
{ unsigned char *p = px(x, y); p[0]=r; p[1]=g; p[2]=b; p[3]=a; }

/* '#' encre, '.' vide, 'o' fond opaque (blanc), '?' autre chose */
static void montre(const char *titre)
{
    printf("   %s\n", titre);
    for (int y = 0; y < H; y++) {
        printf("      ");
        for (int x = 0; x < W; x++) {
            unsigned char *p = px(x, y);
            char c;
            if (p[3] == 0)                              c = '.';
            else if (p[0]==0   && p[1]==0   && p[2]==0) c = '#';
            else if (p[0]==255 && p[1]==255 && p[2]==255) c = 'o';
            else                                        c = '?';
            putchar(c);
        }
        putchar('\n');
    }
}

/* Un carré plein de 4x4, en (2,1). */
static void carre_sur_vide(void)
{
    efface();
    for (int y = 1; y <= 4; y++)
        for (int x = 2; x <= 5; x++) pose(x, y, 0, 0, 0, 255);
}

/* Le même carré, mais posé sur un fond BLANC OPAQUE partout. */
static void carre_sur_blanc(void)
{
    efface();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) pose(x, y, 255, 255, 255, 255);
    for (int y = 1; y <= 4; y++)
        for (int x = 2; x <= 5; x++) pose(x, y, 0, 0, 0, 255);
}

int main(void)
{
    printf("=== 1. Trace Edges sur du NOIR SUR TRANSPARENT ===\n");
    printf("   (le cas qui marchait déjà)\n");
    hcp_fond_pose(0, 255, 255, 255);
    carre_sur_vide();
    montre("avant :");
    hcp_trace_edges(img, BPR, SPP, W, H, 0, 0, W-1, H-1, NULL, 0);
    montre("après : un contour creux");

    printf("=== 2. Trace Edges sur du NOIR SUR BLANC OPAQUE ===\n");
    printf("   (le cas signalé : tout devenait transparent)\n");
    hcp_fond_pose(1, 255, 255, 255);
    carre_sur_blanc();
    montre("avant :");
    hcp_trace_edges(img, BPR, SPP, W, H, 0, 0, W-1, H-1, NULL, 0);
    montre("après : le même contour, et l'intérieur repeint en fond");

    printf("=== 3. ce que « retirer » laisse, selon le mode ===\n");
    hcp_fond_pose(0, 255, 255, 255);
    carre_sur_vide();
    hcp_trace_edges(img, BPR, SPP, W, H, 0, 0, W-1, H-1, NULL, 0);
    montre("transparent : l'intérieur redevient vide");

    printf("=== 4. Invert : le vide noircit, l'encre s'efface ===\n");
    printf("   (inverser du blanc donne du noir, comme chez HyperCard)\n");
    hcp_fond_pose(0, 255, 255, 255);
    carre_sur_vide();
    hcp_invert(img, BPR, SPP, W, H, 2, 1, 5, 4, NULL, 0);
    montre("le carré inversé sur lui-même :");

    printf("=== 5. Flip horizontal : un coin marqué doit changer de côté ===\n");
    efface();
    pose(1, 1, 0, 0, 0, 255);
    hcp_flip(img, BPR, SPP, W, H, 0, 0, W-1, H-1, 1);
    montre("le point est passé à droite :");

    printf("=== 6. Flip vertical ===\n");
    efface();
    pose(1, 1, 0, 0, 0, 255);
    hcp_flip(img, BPR, SPP, W, H, 0, 0, W-1, H-1, 0);
    montre("le point est passé en bas :");

    printf("=== 7. Rotation : un rectangle 4x2 devient 2x4, même centre ===\n");
    efface();
    for (int y = 2; y <= 3; y++)
        for (int x = 2; x <= 5; x++) pose(x, y, 0, 0, 0, 255);
    montre("avant :");
    {
        int nx0, ny0, nx1, ny1;
        hcp_rotate(img, BPR, SPP, W, H, 2, 2, 5, 3, 1, &nx0, &ny0, &nx1, &ny1);
        montre("après :");
        printf("      la sélection devient %d,%d - %d,%d\n", nx0, ny0, nx1, ny1);
    }

    printf("=== 8. Darken et Lighten sont ALÉATOIRES ===\n");
    printf("   (un pixel sur huit, tiré au sort : on ne mesure donc que le\n");
    printf("    SENS — darken ajoute de l'encre, lighten en retire — et non\n");
    printf("    quels pixels. Un golden sur un tirage serait un golden sur\n");
    printf("    l'implémentation de rand, pas sur la nôtre.)\n");
    srand(1);
    efface();
    int avant = 0, apres = 0;
    hcp_darken(img, BPR, SPP, W, H, 0, 0, W-1, H-1, NULL, 0);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        if (hcp_pose(px(x, y), SPP)) apres++;
    printf("   darken sur une image vide : %d posés au départ, %d après\n",
           avant, apres);

    hcp_fond_pose(0, 255, 255, 255);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        pose(x, y, 0, 0, 0, 255);
    avant = W * H; apres = 0;
    hcp_lighten(img, BPR, SPP, W, H, 0, 0, W-1, H-1, NULL, 0);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        if (hcp_pose(px(x, y), SPP)) apres++;
    printf("   lighten sur une image pleine : %d posés au départ, %d après\n",
           avant, apres);

    printf("=== 9. « posé » : les trois réponses ===\n");
    hcp_fond_pose(1, 255, 255, 255);
    efface();
    pose(0, 0, 0, 0, 0, 255);          /* encre    */
    pose(1, 0, 255, 255, 255, 255);    /* fond     */
    pose(2, 0, 128, 0, 0, 255);        /* couleur  */
    printf("   noir opaque      : %d\n", hcp_pose(px(0,0), SPP));
    printf("   blanc opaque     : %d   (le fond n'est pas de l'encre)\n",
           hcp_pose(px(1,0), SPP));
    printf("   rouge sombre     : %d   (une couleur EST de l'encre)\n",
           hcp_pose(px(2,0), SPP));
    printf("   transparent      : %d\n", hcp_pose(px(3,0), SPP));

    printf("=== 10. le remplissage au motif ===\n");
    printf("   (touché ici parce que ce harnais inclut tout hc_pixels.h :\n");
    printf("    une fonction jamais appelée est une fonction jamais vue)\n");
    {
        static const unsigned char damier[8] =
            { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 };
        hcp_fond_pose(0, 255, 255, 255);
        efface();
        hcp_remplit(img, BPR, SPP, W, H, 1, 1, 6, 4, NULL, 0,
                    damier, 0, 0, 0, 255, 255, 255, 255, 1);
        montre("un damier, fond transparent :");
    }
    return 0;
}
