/* maccam_coeur.c — la machine de MAC•CAM, sans HC autour (maccam_coeur.h).
 *
 * Chaque fonction est la traduction d'une fonction de MACCAM, l'XFCN
 * d'origine, dont THINK C a gardé le nom : optscanmoore, sfVonn, fmargph,
 * copyBorders… Lu et désassemblé le 16 octobre (docs/mesures/xcmd.txt) ;
 * rien de cette machine n'a été EXÉCUTÉ dans HyperCard pour comparaison.
 *
 * Les tampons ont h + 2 lignes, comme ceux de l'original (NewPtrClear) :
 * ses balayages lisent la ligne qui suit la dernière. Ses bornes sont
 * reprises telles quelles là où elles changent le résultat (Margolus, dont
 * le découpage en blocs en dépend) ; ailleurs, on ne calcule que
 * l'intérieur, la bordure étant refaite ensuite — le même résultat.
 *
 * Écrit pour tourner dans un module de HC (le kit xcmd/) comme dans un
 * programme ordinaire : il n'emploie que ce que le kit fournit. */

#include <stdlib.h>
#include <string.h>

#include "maccam_coeur.h"

typedef unsigned char u8;

/* Les plans dans un pixel. */
#define P0(x) (((x) >> 7) & 1)
#define P1(x) (((x) >> 6) & 1)
#define P2(x) (((x) >> 5) & 1)
#define P3(x) (((x) >> 4) & 1)

int cam_ouvre(Cam *c, int w, int h)
{
    memset(c, 0, sizeof *c);
    if (w < 4 || h < 4 || w > 4096 || h > 4096) return -1;
    size_t n = (size_t)(h + 2) * (size_t)w;
    c->pix = calloc(n, 1);
    c->neuf = calloc(n, 1);
    if (!c->pix || !c->neuf) { cam_ferme(c); return -1; }
    c->w = w;
    c->h = h;
    return 0;
}

void cam_ferme(Cam *c)
{
    free(c->pix);
    free(c->neuf);
    c->pix = c->neuf = NULL;
}

/* ---- les tables ---- */

/* « pln » : « 0 » éteint le bit, tout autre caractère l'allume ; la lecture
 * s'arrête au zéro de fin. L'original ne comptait pas : un texte plus long
 * que la table débordait. On s'arrête à 4 096. */
void cam_pose_pln(Cam *c, int n, const char *texte)
{
    u8 *t;
    u8 masque;
    switch (n) {
    case 7: t = c->lutA; masque = 0x80; break;
    case 6: t = c->lutA; masque = 0x40; break;
    case 5: t = c->lutB; masque = 0x20; break;
    case 4: t = c->lutB; masque = 0x10; break;
    default: return;
    }
    for (int i = 0; i < CAM_TABLE && texte && texte[i]; i++) {
        if (texte[i] == '0') t[i] &= (u8)~masque;
        else t[i] |= masque;
    }
}

/* L'original (getpln) relisait l'octet entier et non son bit, et pour une
 * autre table que celle qu'il écrivait : aucun script ne s'en sert. On rend
 * le bit du plan. */
void cam_lit_pln(const Cam *c, int n, char *texte)
{
    const u8 *t = n >= 6 ? c->lutA : c->lutB;
    u8 masque = n == 7 ? 0x80 : n == 6 ? 0x40 : n == 5 ? 0x20 : 0x10;
    for (int i = 0; i < CAM_TABLE; i++)
        texte[i] = (n >= 4 && n <= 7 && (t[i] & masque)) ? '1' : '0';
    texte[CAM_TABLE] = '\0';
}

/* ---- le tore : copyBorders ---- */

static void bords(u8 *b, int w, int h)
{
    memcpy(b + (size_t)(h - 1) * w + 1, b + w + 1, (size_t)w - 2);
    memcpy(b + 1, b + (size_t)(h - 2) * w + 1, (size_t)w - 2);
    for (int r = 1; r <= h - 2; r++) {
        b[(size_t)r * w + w - 1] = b[(size_t)r * w + 1];
        b[(size_t)r * w] = b[(size_t)r * w + w - 2];
    }
    b[0] = b[(size_t)(h - 2) * w + w - 2];
    b[w - 1] = b[(size_t)(h - 2) * w + 1];
    b[(size_t)(h - 1) * w] = b[(size_t)w + w - 2];
    b[(size_t)(h - 1) * w + w - 1] = b[(size_t)w + 1];
}

/* ---- Moore : optscanmoore, optscanmooreph, optscanmoorehv ---- */

enum { MODE_ESPERLUETTE, MODE_PH, MODE_HV };

/* L'index de Moore, colonne par colonne — l'est, le centre, l'ouest —, et
 * dans chaque colonne le centre, le sud, le nord. `p` choisit le plan
 * (le bit 7 pour CAM-A, le bit 5 pour CAM-B). */
static unsigned moore9(const u8 *s, size_t x, int w, int p)
{
    #define BIT(o) ((s[(o)] >> p) & 1u)
    return BIT(x + 1)          | BIT(x + 1 + w) << 1 | BIT(x + 1 - w) << 2
         | BIT(x)        << 3  | BIT(x + w)     << 4 | BIT(x - w)     << 5
         | BIT(x - 1)    << 6  | BIT(x - 1 + w) << 7 | BIT(x - 1 - w) << 8;
    #undef BIT
}

static void balaye_moore(Cam *c, int mode)
{
    const int w = c->w, h = c->h;
    const u8 *s = c->pix;
    u8 *d = c->neuf;
    for (int r = 1; r <= h - 2; r++) {
        /* HV : la parité de la ligne en bit 10, celle de la colonne en bit
         * 11 ; avec « alternate », la phase A s'y ajoute (scanoptmoorehv). */
        int v = c->alternate ? c->phaseA + r - 1 : r - 1;
        int compteur = c->alternate ? c->phaseA : 0;
        for (int x = 1; x <= w - 2; x++, compteur++) {
            size_t o = (size_t)r * w + (size_t)x;
            u8 centre = s[o];
            unsigned a = moore9(s, o, w, 7), b = moore9(s, o, w, 5);
            a |= (unsigned)P1(centre) << 9;
            b |= (unsigned)P3(centre) << 9;
            if (mode == MODE_ESPERLUETTE) {
                a |= (unsigned)P3(centre) << 10 | (unsigned)P2(centre) << 11;
                b |= (unsigned)P1(centre) << 10 | (unsigned)P0(centre) << 11;
            } else if (mode == MODE_PH) {
                unsigned ph = (unsigned)(c->phaseA & 1) << 10
                            | (unsigned)(c->phaseB & 1) << 11;
                a |= ph;
                b |= ph;
            } else {
                unsigned hv = (unsigned)(v & 1) << 10 | (unsigned)(compteur & 1) << 11;
                a |= hv;
                b |= hv;
            }
            d[o] = (u8)(c->lutA[a] + c->lutB[b]);
        }
    }
}

/* ---- von Neumann : sfVonn, sfVonnph, sfVonnhv ---- */

/* Deux bits par cellule — le plan propre puis le « prime » —, dans l'ordre
 * ouest, nord, sud, centre, est : l'ouest en tête. */
static unsigned vonn10(const u8 *s, size_t x, int w, int haut)
{
    #define DEUX(o) ((((s[(o)] >> haut) & 1u) << 1) | ((s[(o)] >> (haut - 1)) & 1u))
    return DEUX(x - 1) << 8 | DEUX(x - w) << 6 | DEUX(x + w) << 4
         | DEUX(x) << 2 | DEUX(x + 1);
    #undef DEUX
}

static void balaye_vonn(Cam *c, int mode)
{
    const int w = c->w, h = c->h;
    const u8 *s = c->pix;
    u8 *d = c->neuf;
    for (int r = 1; r <= h - 2; r++) {
        int v = c->alternate ? c->phaseA + r - 1 : r - 1;
        int compteur = c->alternate ? c->phaseA : 0;
        for (int x = 1; x <= w - 2; x++, compteur++) {
            size_t o = (size_t)r * w + (size_t)x;
            u8 centre = s[o];
            unsigned a = vonn10(s, o, w, 7), b = vonn10(s, o, w, 5);
            if (mode == MODE_ESPERLUETTE) {
                a |= (unsigned)P2(centre) << 11 | (unsigned)P3(centre) << 10;
                b |= (unsigned)P0(centre) << 11 | (unsigned)P1(centre) << 10;
            } else if (mode == MODE_PH) {
                /* sfVonnph : chaque demi-machine a SA phase en bit 10, et
                 * celle de l'autre en bit 11. */
                if (c->phaseA) { a |= 0x400; b |= 0x800; }
                if (c->phaseB) { a |= 0x800; b |= 0x400; }
            } else {
                unsigned hv = (unsigned)(v & 1) << 10 | (unsigned)(compteur & 1) << 11;
                a |= hv;
                b |= hv;
            }
            d[o] = (u8)(c->lutA[a] + c->lutB[b]);
        }
    }
}

/* ---- Margolus : scanoptmarg et fmargph ---- */

static u8 rol2(u8 x) { return (u8)(x << 2 | x >> 6); }

/* Une rangée de blocs 2 × 2, à partir de `o` ; lue comme l'original, jusqu'à
 * o + w - phaseA, quitte à déborder sur la ligne suivante. */
static void fmargph(Cam *c, size_t o)
{
    const int w = c->w;
    const u8 *s = c->pix;
    u8 *d = c->neuf;
    const size_t fin = o + (size_t)w - (size_t)c->phaseA;
    const unsigned phA = c->phaseA ? 0x400 : 0, phB = c->phaseB ? 0x800 : 0;
    for (; o < fin; o += 2) {
        u8 hg = s[o], hd = s[o + 1], bg = s[o + w], bd = s[o + w + 1];
        /* L'anneau horaire : haut-gauche, haut-droite, bas-droite,
         * bas-gauche ; deux bits par cellule. */
        u8 anA = (u8)((hg & 0xC0) | (hd & 0xC0) >> 2 | (bd & 0xC0) >> 4 | (bg & 0xC0) >> 6);
        u8 anB = (u8)((hg & 0x30) << 2 | (hd & 0x30) | (bd & 0x30) >> 2 | (bg & 0x30) >> 4);
        unsigned eA = anA, eB = anB;        /* les « & », lus en glissant */
        size_t cible[4] = { o, o + 1, o + (size_t)w + 1, o + (size_t)w };
        for (int k = 0; k < 4; k++) {
            eB <<= 2;
            eA <<= 2;
            unsigned a = anA | (eB & 0x300) | phA;
            unsigned b = anB | (eA & 0x300) | phB;
            d[cible[k]] = (u8)(c->lutA[a] + c->lutB[b]);
            anA = rol2(anA);
            anB = rol2(anB);
        }
    }
}

static void balaye_margolus(Cam *c)
{
    const int w = c->w, h = c->h;
    if (c->alternate) {
        for (int r = c->phaseA; r < h - c->phaseA; r += 2)
            fmargph(c, (size_t)r * w + (size_t)c->phaseA);
    } else {
        for (int r = 1; r < h - 1; r += 2)
            fmargph(c, (size_t)r * w + 1);
    }
}

/* ---- le pas ---- */

void cam_pas(Cam *c, int voisinage)
{
    int v = voisinage == CAM_SELON_MAJOR ? c->major : voisinage;
    switch (v) {
    case 0: balaye_moore(c, MODE_ESPERLUETTE); break;
    case 1: balaye_vonn(c, MODE_ESPERLUETTE); break;
    case 2: case 3: balaye_margolus(c); break;       /* le même pour PH et HV */
    case 4: balaye_moore(c, MODE_PH); break;
    case 5: balaye_moore(c, MODE_HV); break;
    case 6: balaye_vonn(c, MODE_PH); break;
    case 7: balaye_vonn(c, MODE_HV); break;
    default: return;                                  /* l'original ne fait rien */
    }
    bords(c->neuf, c->w, c->h);
    memcpy(c->pix, c->neuf, (size_t)c->w * (size_t)c->h);
    c->phaseA = !c->phaseA;
    c->phaseB = !c->phaseB;
}

/* ---- remplir, effacer, poser une image ---- */

/* fi : toutes les lignes sauf la dernière, toutes les colonnes. */
void cam_remplit(Cam *c, int bit)
{
    if (bit < 0 || bit > 7) return;
    for (size_t i = 0; i < (size_t)(c->h - 1) * (size_t)c->w; i++)
        c->pix[i] |= (u8)(1u << bit);
}

void cam_efface(Cam *c)
{
    memset(c->pix, 0, (size_t)c->w * (size_t)c->h);
}

/* erase1, erase2 : les lignes 0 à h - 2, les colonnes 1 à w - 1. */
void cam_efface_bit(Cam *c, int bit)
{
    if (bit < 0 || bit > 7) return;
    for (int r = 0; r <= c->h - 2; r++)
        for (int x = 1; x <= c->w - 1; x++)
            c->pix[(size_t)r * c->w + (size_t)x] &= (u8)~(1u << bit);
}

/* image, puis copyToPlane. Le dessin est copié dans le SECOND tampon, en
 * haut à gauche, sans changer d'échelle (noir 0xFF, blanc 0) ; puis chaque
 * octet non nul de ce tampon — lignes 0 à h - 2 — allume le bit dans
 * l'image. C'est l'original, bizarrerie comprise : hors du dessin, le
 * second tampon garde l'état du dernier pas, et ses cellules allumées
 * allument aussi le bit. Puis les bords. */
void cam_image(Cam *c, int bit, const unsigned char *noir, int rw, int rh)
{
    if (bit < 0 || bit > 7 || !noir || rw <= 0 || rh <= 0) return;
    int lw = rw < c->w ? rw : c->w, lh = rh < c->h ? rh : c->h;
    for (int r = 0; r < lh; r++)
        for (int x = 0; x < lw; x++)
            c->neuf[(size_t)r * c->w + (size_t)x] =
                noir[(size_t)r * (size_t)rw + (size_t)x] ? 0xFF : 0;
    for (size_t i = 0; i < (size_t)(c->h - 1) * (size_t)c->w; i++)
        if (c->neuf[i]) c->pix[i] |= (u8)(1u << bit);
    bords(c->pix, c->w, c->h);
}

void cam_couleur(unsigned char x, unsigned char rgb[3])
{
    static const unsigned char C[6][3] = {
        { 255, 255, 255 }, { 0, 200, 255 }, { 220, 210, 0 },
        { 255, 140, 0 }, { 0, 210, 90 }, { 0, 0, 0 },
    };
    int k = P0(x) ? 0 : P1(x) ? 1 : (P2(x) && P3(x)) ? 2 : P3(x) ? 3
          : P2(x) ? 4 : 5;
    memcpy(rgb, C[k], 3);
}
