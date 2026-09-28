/* hc_origine.c — Lecture d'une pile HyperCard d'origine. Voir hc_origine.h
 * pour ce que ce module fait, ce qu'il ne fait pas, et d'où viennent ses
 * offsets.
 *
 * LE PRINCIPE DE TOUT LE FICHIER : NE JAMAIS CROIRE UNE TAILLE.
 *
 * L'une des deux descriptions du format porte cet avertissement sur la taille
 * d'un bloc : « ne t'y fie pas trop, elle est parfois corrompue ». Un lecteur
 * qui avance en additionnant des tailles qu'il n'a pas vérifiées lit, sur un
 * fichier abîmé, des octets pris n'importe où — et il en sort des scripts
 * vraisemblables. C'est le pire des résultats possibles pour un travail de
 * préservation : faux et crédible.
 *
 * Donc chaque lecture passe par un accesseur qui connaît la borne, et chaque
 * structure est confrontée à une SECONDE source d'information sur sa propre
 * étendue quand le format en offre une. Le bloc CARD en offre deux : le nombre
 * de parts, et la taille totale de la liste des parts. Parcourir les parts une
 * à une par leur propre taille doit tomber PILE sur la fin annoncée. Les deux
 * lecteurs publics que nous avons consultés ne font pas ce recoupement ; il ne
 * coûte rien et il transforme une supposition en vérification.
 */
#include "hc_origine.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* MacRoman                                                            */
/* ------------------------------------------------------------------ */

/* Les 128 caractères hauts de MacRoman, en points de code Unicode.
 *
 * CETTE TABLE N'EST PAS ÉCRITE DE MÉMOIRE : elle a été produite par le codec
 * « mac_roman » de Python, octet par octet, et la référence de la suite
 * (attendu/origine.txt) porte les 256 conversions, si bien qu'un doigt qui
 * glisse ici casse un test au lieu de casser un accent dans dix ans.
 *
 * UNE SEULE ENTRÉE EST UN CHOIX ET NON UN FAIT : 0xDB. Le MacRoman d'origine y
 * avait le signe monétaire « ¤ » (U+00A4) ; Mac OS 8.5 l'a remplacé par l'euro
 * (U+20AC), et c'est ce que rend le codec. Une pile de 1993 voulait dire « ¤ »
 * — l'euro n'existait pas. On suit le codec quand même, pour que notre sortie
 * reste comparable octet pour octet à celle du lecteur qui nous sert d'arbitre,
 * et c'est écrit ici pour que ce soit une décision et pas un oubli. Une entrée
 * à changer, le jour où ça compte.
 */
static const unsigned short macroman_haut[128] = {
    0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1,   /* 80  Ä Å Ç É Ñ Ö Ü á */
    0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,   /* 88  à â ä ã å ç é è */
    0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3,   /* 90  ê ë í ì î ï ñ ó */
    0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC,   /* 98  ò ô ö õ ú ù û ü */
    0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF,   /* A0  † ° ¢ £ § • ¶ ß */
    0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,   /* A8  ® © ™ ´ ¨ ≠ Æ Ø */
    0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211,   /* B0  ∞ ± ≤ ≥ ¥ µ ∂ ∑ */
    0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,   /* B8  ∏ π ∫ ª º Ω æ ø */
    0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB,   /* C0  ¿ ¡ ¬ √ ƒ ≈ ∆ « */
    0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153,   /* C8  » …   À Ã Õ Œ œ */
    0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA,   /* D0  – — “ ” ‘ ’ ÷ ◊ */
    0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,   /* D8  ÿ Ÿ ⁄ € ‹ › ﬁ ﬂ */
    0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1,   /* E0  ‡ · ‚ „ ‰ Â Ê Á */
    0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,   /* E8  Ë È Í Î Ï Ì Ó Ô */
    0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC,   /* F0    Ò Ú Û Ù ı ˆ ˜ */
    0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7    /* F8  ¯ ˘ ˙ ˚ ¸ ˝ ˛ ˇ */
};

static char *pose_utf8(char *w, unsigned long cp)
{
    if (cp < 0x80) {
        *w++ = (char)cp;
    } else if (cp < 0x800) {
        *w++ = (char)(0xC0 | (cp >> 6));
        *w++ = (char)(0x80 | (cp & 0x3F));
    } else {
        *w++ = (char)(0xE0 | (cp >> 12));
        *w++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *w++ = (char)(0x80 | (cp & 0x3F));
    }
    return w;
}

char *hc_origine_utf8(const unsigned char *octets, size_t n)
{
    if (!octets) return NULL;
    /* Trois octets au plus par caractère : tous les points de code de la table
     * tiennent sous U+FFFF, y compris la pomme (U+F8FF). */
    char *d = malloc(3 * n + 1);
    if (!d) return NULL;
    char *w = d;

    for (size_t i = 0; i < n; i++) {
        unsigned char c = octets[i];
        /* Les fins de ligne du Mac classique. Tous les scripts d'origine sont
         * en « \r » seul ; « \r\n » ne s'y rencontre pas, mais un fichier qui
         * a transité par un outil moderne peut en porter, et deux sauts de
         * ligne au lieu d'un couperait un gestionnaire en deux. */
        if (c == '\r') {
            if (i + 1 < n && octets[i + 1] == '\n') i++;
            *w++ = '\n';
            continue;
        }
        if (c < 0x80) { *w++ = (char)c; continue; }
        w = pose_utf8(w, macroman_haut[c - 0x80]);
    }
    *w = '\0';
    return d;
}

/* ------------------------------------------------------------------ */
/* Lecture bornée                                                      */
/* ------------------------------------------------------------------ */

/* Un curseur qui connaît sa borne. `debord` se lève à la première lecture qui
 * sort, et ne se rabaisse jamais : on peut donc enchaîner vingt lectures et ne
 * tester qu'une fois à la fin, sans qu'aucune valeur douteuse ait servi —
 * celles qui sortent rendent zéro. */
typedef struct {
    const unsigned char *o;
    size_t n;
    int debord;
} Vue;

static unsigned long u32(Vue *v, size_t p)
{
    if (p + 4 > v->n) { v->debord = 1; return 0; }
    return ((unsigned long)v->o[p] << 24) | ((unsigned long)v->o[p+1] << 16)
         | ((unsigned long)v->o[p+2] << 8) | (unsigned long)v->o[p+3];
}

static long s32(Vue *v, size_t p)
{
    unsigned long u = u32(v, p);
    return (u & 0x80000000UL) ? (long)(u - 0x100000000ULL) : (long)u;
}

static unsigned u16(Vue *v, size_t p)
{
    if (p + 2 > v->n) { v->debord = 1; return 0; }
    return (unsigned)(v->o[p] << 8) | v->o[p+1];
}

static int s16(Vue *v, size_t p)
{
    unsigned u = u16(v, p);
    return (u & 0x8000u) ? (int)u - 0x10000 : (int)u;
}

/* La longueur d'une chaîne terminée par zéro, à partir de `p`, SANS sortir de
 * la vue. Rend -1 si le zéro n'y est pas : une chaîne sans sa fin est une
 * faute structurelle, parce que tout ce qui la suit dans le bloc — le script,
 * pour une carte — est alors introuvable. */
static long longueur_chaine(Vue *v, size_t p)
{
    size_t i = p;
    while (i < v->n && v->o[i]) i++;
    if (i >= v->n) return -1;
    return (long)(i - p);
}

static char *dit(Vue *v, size_t p, long len)
{
    if (len < 0 || p + (size_t)len > v->n) return NULL;
    return hc_origine_utf8(v->o + p, (size_t)len);
}

static void motif(char *ou, size_t combien, const char *quoi, unsigned long ou_ca)
{
    if (ou && combien) snprintf(ou, combien, "%s (offset 0x%lX)", quoi, ou_ca);
}

/* La rotation de trois bits vers la droite, sur 32 bits. Elle vient des deux
 * sommes de contrôle de la liste des cartes, et c'est la seule arithmétique
 * bizarre de ce fichier : elle est dans l'assembleur d'HyperCard, pas dans
 * notre tête. */
static unsigned long rotd3(unsigned long x)
{
    x &= 0xFFFFFFFFUL;
    return ((x >> 3) | (x << 29)) & 0xFFFFFFFFUL;
}

/* ------------------------------------------------------------------ */
/* WOBA — le compactage des dessins                                    */
/* ------------------------------------------------------------------ */

/* « Wrath of Bill Atkinson », ainsi baptisé par Rebecca Bettencourt qui l'a
 * retrouvé par rétro-ingénierie. Rien ici n'est copié de son code : ce sont les
 * OFFSETS et les CODES OPÉRATION qui sont documents, et c'est d'eux qu'on part.
 *
 * L'ORACLE EST DANS LE FORMAT LUI-MÊME, et il est plus fort qu'il n'y paraît : un
 * flot d'instructions doit remplir EXACTEMENT le nombre de lignes que son
 * rectangle annonce, et s'arrêter à la fin des données. Une seule instruction
 * mal dimensionnée décale tout ce qui suit et le compte de lignes tombe faux.
 * Mesuré sur les 22 plans des deux vraies piles : les 22 tombent juste.
 *
 * CE QUE CET ORACLE NE VOIT PAS, ce sont `dh` et `dv` — ils transforment une
 * ligne DÉJÀ remplie, donc ils ne changent pas un octet du décompte. Pour
 * ceux-là le juge est l'ŒIL : le fond de « Découvrir HyperCard » porte les
 * libellés de ses boutons PEINTS, et l'on y lit « Bienvenue » en clair. Une
 * transformation fausse étale le texte en diagonale ; celle-là se voit du
 * premier coup d'œil.
 *
 * DEUX CODES NE SONT PAS EXERCÉS PAR CES PILES, et c'est écrit plutôt que tu :
 * 0x82 (ligne noire) et 0x88 (dh=16). Le harnais leur fabrique un flot à la
 * main, faute de vraie pile qui les porte. */

typedef struct {
    const unsigned char *src;
    size_t n, i;
    unsigned char *dst;
    int rowbytes, hauteur;
    int y, x, dh, dv;
    unsigned char patron[8];
    unsigned char *tampon;      /* une ligne, pour la transformation */
    int faute;
} Woba;

/* Le décalage d'une ligne vers la DROITE, en bits, gros-boutiste : le bit 7 de
 * l'octet 0 est le pixel le plus à gauche, donc « vers la droite » va vers les
 * octets de poids fort en indice croissant. */
static void woba_decale(unsigned char *l, int n, int bits)
{
    int oct = bits >> 3, rest = bits & 7;
    if (oct) {
        for (int i = n - 1; i >= 0; i--) l[i] = (i - oct >= 0) ? l[i - oct] : 0;
    }
    if (rest) {
        unsigned char report = 0;
        for (int i = 0; i < n; i++) {
            unsigned char v = l[i];
            l[i] = (unsigned char)((v >> rest) | report);
            report = (unsigned char)(v << (8 - rest));
        }
    }
}

static int woba_tout_zero(const unsigned char *l, int n)
{
    for (int i = 0; i < n; i++) if (l[i]) return 0;
    return 1;
}

/* LA FIN D'UNE LIGNE BÂTIE PAR MORCEAUX, avec ses deux transformations.
 *
 * `dh` est une INTÉGRATION horizontale : la ligne devient le XOR d'elle-même et
 * de toutes ses copies décalées de dh, 2dh, 3dh... bits. La boucle s'arrête
 * quand la copie décalée est vide, c'est-à-dire quand tous les bits sont sortis.
 * `dv` est la même chose verticalement, mais d'un seul cran : la ligne est XORée
 * avec celle de dv lignes plus haut, déjà transformée.
 *
 * Les lignes bâties d'UNE SEULE instruction (0x80 à 0x87) n'y passent pas : la
 * spécification les en exclut nommément, et c'est ce qui permet à une ligne
 * blanche ou noire de rester blanche ou noire au milieu d'un dégradé. */
static void woba_fin_ligne(Woba *w)
{
    unsigned char *lg = w->dst + (size_t)w->y * w->rowbytes;
    if (w->dh) {
        memcpy(w->tampon, lg, (size_t)w->rowbytes);
        for (;;) {
            woba_decale(w->tampon, w->rowbytes, w->dh);
            if (woba_tout_zero(w->tampon, w->rowbytes)) break;
            for (int k = 0; k < w->rowbytes; k++) lg[k] ^= w->tampon[k];
        }
    }
    if (w->dv && w->y >= w->dv) {
        const unsigned char *av = w->dst + (size_t)(w->y - w->dv) * w->rowbytes;
        for (int k = 0; k < w->rowbytes; k++) lg[k] ^= av[k];
    }
    w->y++;
    w->x = 0;
}

/* Un octet de plus dans la ligne en cours. La ligne se termine d'elle-même
 * quand elle est pleine — c'est l'une des deux fins possibles que décrit la
 * spécification, l'autre étant un code de 0x80 à 0xBF. */
static void woba_pose(Woba *w, unsigned char octet)
{
    if (w->y >= w->hauteur) return;
    w->dst[(size_t)w->y * w->rowbytes + w->x] = octet;
    if (++w->x == w->rowbytes) woba_fin_ligne(w);
}

/* Une ligne entière d'un coup, SANS transformation. */
static void woba_ligne_entiere(Woba *w, const unsigned char *octets, int valeur)
{
    if (w->y >= w->hauteur) return;
    unsigned char *lg = w->dst + (size_t)w->y * w->rowbytes;
    if (octets) memcpy(lg, octets, (size_t)w->rowbytes);
    else        memset(lg, valeur, (size_t)w->rowbytes);
    w->y++;
    w->x = 0;
}

/* Décompresse `n` octets vers un plan de `rowbytes` × `hauteur`.
 *
 * Rend 0 si le flot remplit exactement le plan, -1 sinon, et `*reste` reçoit le
 * nombre d'octets non consommés — la taille annoncée dans le bloc est calée sur
 * quatre, donc 1 à 3 octets traînent souvent derrière. Mesuré : six des dix
 * plans de « Découvrir HyperCard » en laissent, et ces octets ne sont pas
 * toujours nuls — deux valent 0xFF. C'est donc bien du calage et non des
 * données, et on ne peut pas exiger qu'ils soient à zéro. */
static int woba_decompresse(const unsigned char *src, size_t n,
                            int rowbytes, int hauteur, unsigned char *dst,
                            int *reste, char *pourquoi, size_t npourquoi)
{
    Woba w;
    int repete = 1;

    if (rowbytes <= 0 || hauteur <= 0) { motif(pourquoi, npourquoi, "rectangle vide", 0); return -1; }

    memset(dst, 0, (size_t)rowbytes * (size_t)hauteur);
    memset(&w, 0, sizeof w);
    w.src = src; w.n = n; w.dst = dst;
    w.rowbytes = rowbytes; w.hauteur = hauteur;
    for (int k = 0; k < 8; k++) w.patron[k] = (k & 1) ? 0x55 : 0xAA;  /* le gris */
    w.tampon = malloc((size_t)rowbytes);
    if (!w.tampon) { motif(pourquoi, npourquoi, "mémoire", 0); return -1; }

    while (w.i < w.n && w.y < w.hauteur) {
        unsigned op = w.src[w.i++];

        if (op < 0x80) {                        /* dz : z zéros puis d données */
            int d = (int)(op >> 4), z = (int)(op & 0x0F);
            while (repete-- > 0) {
                for (int k = 0; k < z; k++) woba_pose(&w, 0);
                for (int k = 0; k < d; k++) {
                    if (w.i >= w.n) { w.faute = 1; break; }
                    woba_pose(&w, w.src[w.i++]);
                }
            }
        } else if (op >= 0xE0) {                 /* z*16 zéros */
            int z = (int)(op & 0x1F) * 16;
            while (repete-- > 0)
                for (int k = 0; k < z; k++) woba_pose(&w, 0);
        } else if (op >= 0xC0) {                 /* d*8 données */
            int d = (int)(op & 0x1F) * 8;
            while (repete-- > 0)
                for (int k = 0; k < d; k++) {
                    if (w.i >= w.n) { w.faute = 1; break; }
                    woba_pose(&w, w.src[w.i++]);
                }
        } else if (op >= 0xA0) {                 /* répéter l'instruction suivante */
            repete = (int)(op & 0x1F);
            continue;                            /* sans le remettre à un */
        } else switch (op) {                     /* 0x80 à 0x9F */
            case 0x80:                           /* une ligne brute */
                while (repete-- > 0) {
                    if (w.i + (size_t)rowbytes > w.n) { w.faute = 1; break; }
                    woba_ligne_entiere(&w, w.src + w.i, 0);
                    w.i += (size_t)rowbytes;
                }
                break;
            case 0x81:                           /* une ligne blanche */
                while (repete-- > 0) woba_ligne_entiere(&w, NULL, 0x00);
                break;
            case 0x82:                           /* une ligne noire */
                while (repete-- > 0) woba_ligne_entiere(&w, NULL, 0xFF);
                break;
            case 0x83: {                         /* un octet répété, et retenu */
                if (w.i >= w.n) { w.faute = 1; break; }
                unsigned char b = w.src[w.i++];
                while (repete-- > 0) {
                    w.patron[w.y & 7] = b;       /* l'indice est la LIGNE, modulo 8 */
                    woba_ligne_entiere(&w, NULL, b);
                }
                break;
            }
            case 0x84:                           /* l'octet retenu pour cette ligne */
                while (repete-- > 0) woba_ligne_entiere(&w, NULL, w.patron[w.y & 7]);
                break;
            case 0x85:                           /* copier la ligne précédente */
                while (repete-- > 0) {
                    if (w.y < 1) { w.faute = 1; break; }
                    woba_ligne_entiere(&w, w.dst + (size_t)(w.y - 1) * rowbytes, 0);
                }
                break;
            case 0x86:                           /* copier l'avant-dernière */
                while (repete-- > 0) {
                    if (w.y < 2) { w.faute = 1; break; }
                    woba_ligne_entiere(&w, w.dst + (size_t)(w.y - 2) * rowbytes, 0);
                }
                break;
            case 0x88: w.dh = 16; w.dv = 0; break;
            case 0x89: w.dh = 0;  w.dv = 0; break;
            case 0x8A: w.dh = 0;  w.dv = 1; break;
            case 0x8B: w.dh = 0;  w.dv = 2; break;
            case 0x8C: w.dh = 1;  w.dv = 0; break;
            case 0x8D: w.dh = 1;  w.dv = 1; break;
            case 0x8E: w.dh = 2;  w.dv = 2; break;
            case 0x8F: w.dh = 8;  w.dv = 0; break;
            default:                             /* 0x87 et 0x90..0x9F */
                w.faute = 1;
                break;
        }
        if (w.faute) break;
        repete = 1;
    }
    free(w.tampon);

    if (w.faute) {
        if (pourquoi && npourquoi)
            snprintf(pourquoi, npourquoi, "code opération refusé ou données tronquées "
                     "(octet %lu, ligne %d)", (unsigned long)w.i, w.y);
        return -1;
    }
    if (w.y != w.hauteur) {
        if (pourquoi && npourquoi)
            snprintf(pourquoi, npourquoi, "%d lignes remplies sur %d", w.y, w.hauteur);
        return -1;
    }
    /* Plus de trois octets de reste, ce n'est plus du calage. */
    if (w.n - w.i >= 4) {
        if (pourquoi && npourquoi)
            snprintf(pourquoi, npourquoi, "%lu octets de reste, le calage n'en fait que 3",
                     (unsigned long)(w.n - w.i));
        return -1;
    }
    *reste = (int)(w.n - w.i);
    return 0;
}

/* Poser un plan décompressé dans le plan de la CARTE, au bon endroit. Le
 * décalage horizontal est en OCTETS : les rectangles arrondis à 32 bits le
 * garantissent tant que le rectangle de la carte commence lui aussi sur un
 * multiple de huit, ce que l'appelant vérifie. */
static void dessin_pose(unsigned char *carte, int rb_carte, int h_carte,
                        const unsigned char *plan, int rb, int h,
                        int dx_octets, int dy)
{
    for (int y = 0; y < h; y++) {
        int yc = y + dy;
        if (yc < 0 || yc >= h_carte) continue;
        for (int x = 0; x < rb; x++) {
            int xc = x + dx_octets;
            if (xc < 0 || xc >= rb_carte) continue;
            carte[(size_t)yc * rb_carte + xc] = plan[(size_t)y * rb + x];
        }
    }
}

/* UN RECTANGLE SANS DONNÉES EST UN RECTANGLE PLEIN, et c'est la règle du format,
 * pas une invention : « if the content data is not present but the bounding
 * rectangle is not zero, the pixels in the bounding rectangle are 1 ». Le fond de
 * « Découvrir HyperCard » en dépend — son masque n'a aucune donnée et couvre la
 * carte entière, ce qui veut dire « toute la carte est opaque ». Sans cette
 * règle, sa peinture arriverait transparente et l'on ne verrait rien. */
static void dessin_remplit(unsigned char *carte, int rb_carte, int h_carte,
                           int t, int l, int b, int r, int ct, int cl)
{
    for (int y = t; y < b; y++) {
        int yc = y - ct;
        if (yc < 0 || yc >= h_carte) continue;
        for (int x = l; x < r; x++) {
            int xc = x - cl;
            if (xc < 0 || xc >= rb_carte * 8) continue;
            carte[(size_t)yc * rb_carte + (xc >> 3)] |= (unsigned char)(0x80u >> (xc & 7));
        }
    }
}

/* Le dessin d'une couche : son bloc BMAP, ses deux plans, et leur place.
 *
 * Une faute est LOCALE : une couche sans son dessin reste une couche, avec son
 * nom, ses parts et son script. On la compte en anomalie et l'on continue —
 * refuser la pile entière pour un dessin abîmé serait hors de proportion. */
static void lit_le_dessin(Vue *v, HcOrigPile *pile, HcOrigCouche *k)
{
    if (k->bloc_image == 0) return;

    size_t bloc = 0, fin = 0;
    int trouve = 0;
    for (int i = 0; i < pile->nblocs; i++) {
        if (pile->blocs[i].id != k->bloc_image) continue;
        if (memcmp(v->o + pile->blocs[i].offset + 4, "BMAP", 4) != 0) continue;
        bloc = (size_t)pile->blocs[i].offset;
        fin  = bloc + (size_t)pile->blocs[i].taille;
        trouve = 1;
        break;
    }
    if (!trouve) { pile->anomalies++; return; }     /* la couche annonce un dessin absent */

    /* DEUX CONSTANTES À VÉRIFIER PLUTÔT QU'À SAUTER : les deux mots de 0x10 et
     * 0x14 valent 0 et 0x10000 dans tous les blocs que la spécification décrit.
     * C'est un recoupement gratuit sur un bloc dont on va croire les six
     * rectangles suivants. */
    if (u32(v, bloc + 0x10) != 0 || u32(v, bloc + 0x14) != 0x10000UL) pile->anomalies++;

    int ct = s16(v, bloc + 0x18), cl = s16(v, bloc + 0x1A);
    int cb = s16(v, bloc + 0x1C), cr = s16(v, bloc + 0x1E);
    int mt = s16(v, bloc + 0x20), ml = s16(v, bloc + 0x22);
    int mb = s16(v, bloc + 0x24), mr = s16(v, bloc + 0x26);
    int it = s16(v, bloc + 0x28), il = s16(v, bloc + 0x2A);
    int ib = s16(v, bloc + 0x2C), ir = s16(v, bloc + 0x2E);
    unsigned long tm = u32(v, bloc + 0x38), ti = u32(v, bloc + 0x3C);
    if (v->debord) { pile->anomalies++; return; }

    int w = cr - cl, h = cb - ct;
    /* Les bornes sont celles d'une carte plausible : HyperCard n'allait pas
     * au-delà de 1280x1024, on laisse large sans laisser n'importe quoi. */
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) { pile->anomalies++; return; }
    if (cl % 8) { pile->anomalies++; return; }       /* on ne sait pas décaler d'un bit */

    /* Les données des deux plans doivent tenir DANS le bloc. */
    if (bloc + 0x40 + tm + ti > fin) { pile->anomalies++; return; }

    /* LES TAILLES ANNONCÉES SE NOTENT AVANT DE DÉCOMPRESSER, et `reste` reste à
     * -1 tant que le plan n'est pas lu : sinon un plan refusé s'affichait
     * « 0 octet », ce qui se lit « il n'y en avait pas » au lieu de « il y en
     * avait et je n'ai pas su ». */
    k->dessin.taille_masque = tm;
    k->dessin.taille_image  = ti;
    k->dessin.reste_masque  = -1;
    k->dessin.reste_image   = -1;

    int rb = (w + 31) / 32 * 4;
    unsigned char *image  = calloc((size_t)rb * (size_t)h, 1);
    unsigned char *masque = calloc((size_t)rb * (size_t)h, 1);
    if (!image || !masque) { free(image); free(masque); pile->anomalies++; return; }

    int pose = 0;
    for (int plan = 0; plan < 2; plan++) {          /* 0 le masque, 1 l'image */
        unsigned long taille = plan ? ti : tm;
        int t = plan ? it : mt, l = plan ? il : ml;
        int b = plan ? ib : mb, r = plan ? ir : mr;
        unsigned char *ou = plan ? image : masque;

        if (taille == 0) {
            if (plan) k->dessin.reste_image = 0; else k->dessin.reste_masque = 0;
            if (r > l && b > t) { dessin_remplit(ou, rb, h, t, l, b, r, ct, cl); pose = 1; }
            continue;
        }
        /* LES RECTANGLES S'ARRONDISSENT AVANT DE DÉCOMPRESSER, et c'est écrit
         * dans la spécification : le bord gauche descend au multiple de 32 bits,
         * le bord droit monte. Les pixels ainsi ajoutés sont blancs. Sans cet
         * arrondi, le nombre d'octets par ligne est faux et le compte de lignes
         * tombe à côté — ce qui se voit, au moins. */
        int L = l & ~31, R = (r + 31) & ~31;
        int rbp = (R - L) / 8, hp = b - t;
        if (rbp <= 0 || hp <= 0 || (L - cl) % 8) { pile->anomalies++; continue; }

        unsigned char *tampon = calloc((size_t)rbp * (size_t)hp, 1);
        if (!tampon) { pile->anomalies++; continue; }
        int reste = 0;
        char pq[120];
        if (woba_decompresse(v->o + bloc + 0x40 + (plan ? tm : 0), taille,
                             rbp, hp, tampon, &reste, pq, sizeof pq) != 0) {
            pile->anomalies++;
            free(tampon);
            continue;
        }
        if (plan) k->dessin.reste_image = reste; else k->dessin.reste_masque = reste;
        dessin_pose(ou, rb, h, tampon, rbp, hp, (L - cl) / 8, t - ct);
        pose = 1;
        free(tampon);
    }

    if (!pose) { free(image); free(masque); return; }
    k->dessin.present          = 1;
    k->dessin.largeur          = w;
    k->dessin.hauteur          = h;
    k->dessin.octets_par_ligne = rb;
    k->dessin.image            = image;
    k->dessin.masque           = masque;
}

/* ------------------------------------------------------------------ */
/* Les parts                                                           */
/* ------------------------------------------------------------------ */

/* Le genre d'une part : bit 8 de l'entier 16 bits à 0x4, 0 pour un champ et 1
 * pour un bouton.
 *
 * MESURÉ SUR UNE VRAIE PILE, et c'était le seul champ que nos deux sources ne
 * corroboraient pas — l'une le décrit, l'autre ne lit jamais le genre. « 3D
 * Parametric Equations » tranche : 40 boutons, 65 champs, et pas un classement
 * absurde. Les boutons sont des verbes — AddComment, Draw graph, Export — et
 * les champs des porteurs de données — MaxX, EndT, Increm. */
static int genre_de(unsigned drapeaux) { return (drapeaux & 0x0100u) ? HC_ORIG_BOUTON : HC_ORIG_CHAMP; }

/* Les douze styles de part, dans l'ordre du format. Les noms sont ceux de notre
 * propre format (hc_file.c les écrit tels quels), si bien qu'un importateur n'a
 * rien à traduire. */
/* LES NOMS SONT CEUX DU SÉLECTEUR DE STYLE DE L'APPLICATION, mot pour mot —
 * HCdialogs.m les liste pour le dialogue « Informations ». Je les avais inventés,
 * et « radio » n'existe nulle part : le noyau attend « radioButton »
 * (hc_core.c:2560) et le rendu aussi (HCview.m:6126). Résultat mesuré dans
 * l'application par l'autrice — le bouton radio d'une pile d'Apple se dessinait
 * en RECTANGLE.
 *
 * Deux autres passaient par chance, la comparaison acceptant la variante en
 * minuscules : « checkbox » et « roundrect ». Ils sont écrits en camelCase ici
 * quand même, parce que select_style compare exactement et que le dialogue
 * affichait sinon le mauvais choix. */
static const char *STYLES[] = {
    "transparent", "opaque", "rectangle", "roundRect", "shadow", "checkBox",
    "radioButton", "scrolling", "standard", "default", "oval", "popup"
};
#define NSTYLES ((int)(sizeof STYLES / sizeof STYLES[0]))

/* CERTAINS STYLES N'APPARTIENNENT QU'À UN GENRE, et ça donne un recoupement
 * gratuit sur l'octet de style — le seul qu'on ait, faute d'oracle qui lise les
 * propriétés. Un « scrolling » sur un bouton, ou un « checkbox » sur un champ,
 * voudrait dire qu'on lit le mauvais octet. Compté en anomalie, jamais fatal :
 * une pile étrange ne doit pas faire perdre ses scripts. */
static int style_va_au_genre(int istyle, int genre)
{
    switch (istyle) {
        case 5: case 6: case 8: case 9: case 11:   /* checkbox radio standard default popup */
        case 3: case 4: case 10:                   /* roundrect shadow oval */
            return genre == HC_ORIG_BOUTON;
        case 7:                                    /* scrolling */
            return genre == HC_ORIG_CHAMP;
        default:                                   /* transparent, opaque, rectangle */
            return 1;
    }
}

/* Le nom d'une police, par son identifiant, dans la table de la pile.
 *
 * Les identifiants de police n'étaient PAS les mêmes d'un Macintosh à l'autre :
 * HyperCard rangeait donc les noms dans le fichier. Et la spec avertit que
 * l'identifiant écrit dans une part peut être NÉGATIF, auquel cas le vrai
 * identifiant est -valeur-1. */
static const char *police_de(const HcOrigPile *pile, int id)
{
    if (id < 0) id = -id - 1;
    for (int i = 0; i < pile->npolices; i++)
        if (pile->polices[i].id == id) return pile->polices[i].nom;
    return NULL;
}

/* Lit la liste des parts, puis SAUTE la liste des contenus, et rend l'offset
 * du nom de la couche. Rend 0 en cas de faute structurelle.
 *
 * Les contenus des champs ne nous intéressent pas — on extrait des scripts —
 * mais il faut bien les TRAVERSER : le nom de la carte et son script sont
 * derrière eux, et leur longueur totale ne se devine pas autrement. */
static size_t lit_les_parts(Vue *v, HcOrigCouche *k, HcOrigPile *pile,
                            size_t bloc, size_t deb_parts, unsigned nparts,
                            unsigned long taille_liste,
                            unsigned ncontenus, size_t bloc_fin,
                            char *pourquoi, size_t npourquoi)
{
    if (nparts > 0) {
        k->parts = calloc(nparts, sizeof *k->parts);
        if (!k->parts) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)deb_parts); return 0; }
    }

    size_t p = deb_parts;
    for (unsigned i = 0; i < nparts; i++) {
        unsigned taille = u16(v, p);
        /* Une part de taille nulle ferait une boucle sans fin, et une part plus
         * courte que son propre en-tête n'a pas de nom où aller chercher. */
        if (taille < 0x1E + 1 || p + taille > bloc_fin) {
            motif(pourquoi, npourquoi, "taille de part impossible", (unsigned long)p);
            return 0;
        }
        HcOrigPart *pt = &k->parts[i];

        /* LE COMPTEUR MONTE AVANT LES ALLOCATIONS, ET C'EST UNE CORRECTION.
         *
         * Il montait à la FIN de la lecture d'une part, alors que son nom et le
         * nom de sa police sont alloués AVANT — si bien qu'un échec entre les
         * deux laissait deux chaînes que hc_origine_libere ne voyait pas : il
         * s'arrête à `nparts`. Trouvé par le fuzzing, deux fois sur 3866
         * fichiers abîmés, et invisible autrement.
         *
         * C'est la deuxième fois que ce chantier paie la même faute : une
         * allocation faite avant le compteur qui gouverne sa libération. Le
         * compteur dit désormais « emplacements ENTAMÉS » et non « parts
         * entièrement lues » — sur un succès c'est le même nombre, et sur un
         * échec la pile est vidée de toute façon, donc aucun appelant ne voit la
         * différence. */
        k->nparts = (int)(i + 1);

        unsigned dr  = u16(v, p + 0x04);
        unsigned dr2 = (p + 0x0E < v->n) ? v->o[p + 0x0E] : 0;
        unsigned ist = (p + 0x0F < v->n) ? v->o[p + 0x0F] : 0;

        pt->id     = (int)u16(v, p + 0x02);
        pt->genre  = genre_de(dr);
        pt->haut   = (int)u16(v, p + 0x06);
        pt->gauche = (int)u16(v, p + 0x08);
        pt->bas    = (int)u16(v, p + 0x0A);
        pt->droite = (int)u16(v, p + 0x0C);

        /* LES QUATRE DRAPEAUX INVERSÉS. Le bit allumé signifie FAUX, et la spec
         * les écrit entre parenthèses. Convertis ici une fois pour toutes, si
         * bien que plus rien en aval n'a à s'en souvenir. */
        pt->visible     = (dr & 0x0080u) ? 0 : 1;
        pt->fixed_lh    = (dr & 0x0004u) ? 0 : 1;
        pt->dont_wrap   = (dr & 0x0020u) ? 1 : 0;
        pt->dont_search = (dr & 0x0010u) ? 1 : 0;
        pt->shared_text = (dr & 0x0008u) ? 1 : 0;
        pt->auto_tab    = (dr & 0x0002u) ? 1 : 0;
        pt->family      = (int)(dr2 & 0x0Fu);

        /* LE BIT 0 N'A PAS LE MÊME SENS SELON LE GENRE, et il n'est inversé que
         * d'un côté : un bouton y lit « PAS actif », un champ y lit « texte
         * verrouillé », qui est déjà positif. */
        if (pt->genre == HC_ORIG_BOUTON) {
            pt->enabled  = (dr & 0x0001u) ? 0 : 1;
            pt->locktext = 0;
        } else {
            pt->enabled  = 1;
            pt->locktext = (dr & 0x0001u) ? 1 : 0;
        }

        /* LES MÊMES QUATRE BITS DE 0xE, DEUX FAMILLES DE SENS. Seuls ceux du bon
         * genre sont remplis : les mélanger donnerait des propriétés plausibles
         * et fausses, ce qui est le pire résultat possible. Et le bit 4 n'est
         * inversé que du côté du bouton. */
        if (pt->genre == HC_ORIG_BOUTON) {
            pt->showname      = (dr2 & 0x80u) ? 1 : 0;
            pt->hilite        = (dr2 & 0x40u) ? 1 : 0;
            pt->autohilite    = (dr2 & 0x20u) ? 1 : 0;
            pt->shared_hilite = (dr2 & 0x10u) ? 0 : 1;
            pt->titlewidth    = (int)u16(v, p + 0x10);
            pt->icon          = s16(v, p + 0x12);
        } else {
            pt->auto_select    = (dr2 & 0x80u) ? 1 : 0;
            pt->show_lines     = (dr2 & 0x40u) ? 1 : 0;
            pt->wide_margins   = (dr2 & 0x20u) ? 1 : 0;
            pt->multiple_lines = (dr2 & 0x10u) ? 1 : 0;
            pt->derniere_ligne = (int)u16(v, p + 0x10);
            pt->premiere_ligne = s16(v, p + 0x12);
        }

        pt->style = (ist < (unsigned)NSTYLES) ? STYLES[ist] : "transparent";
        if (ist >= (unsigned)NSTYLES || !style_va_au_genre((int)ist, pt->genre))
            pile->anomalies++;       /* un style qui ne va pas au genre : on lit mal */

        pt->text_align = s16(v, p + 0x14);
        pt->textsize   = (int)u16(v, p + 0x18);
        pt->textstyle  = (int)((p + 0x1A < v->n) ? v->o[p + 0x1A] : 0);
        pt->textheight = (int)u16(v, p + 0x1C);
        {
            const char *nom = police_de(pile, s16(v, p + 0x16));
            if (nom) {
                size_t ln = strlen(nom) + 1;
                pt->police = malloc(ln);
                if (!pt->police) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)p); return 0; }
                memcpy(pt->police, nom, ln);
            }
        }

        long ln = longueur_chaine(v, p + 0x1E);
        if (ln < 0 || p + 0x1E + (size_t)ln + 1 > p + taille) {
            motif(pourquoi, npourquoi, "nom de part sans fin", (unsigned long)(p + 0x1E));
            return 0;
        }
        pt->nom = dit(v, p + 0x1E, ln);
        if (!pt->nom) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)p); return 0; }

        /* Après le nom : rien du tout si la part n'a pas de script, sinon un
         * octet nul puis le script. Les deux sources le disent ainsi. */
        size_t apres = p + 0x1E + (size_t)ln + 1;
        if (apres < p + taille) {
            if (v->o[apres] != 0) {
                /* Faute LOCALE : on perd le script de cette part, pas les
                 * autres. Comptée, jamais tue. */
                pile->anomalies++;
            } else {
                size_t d = apres + 1;
                size_t f = p + taille;
                /* Le script s'arrête à son premier zéro : la fin de la part
                 * porte souvent du remplissage. Et un script dont le PREMIER
                 * octet est nul n'est pas du HyperTalk mais un script OSA
                 * (AppleScript), exécuté par le système et non par HyperCard :
                 * il ne nous concerne pas, et il rend ici un script vide. */
                size_t z = d;
                while (z < f && v->o[z]) z++;
                if (z > d) {
                    pt->script = dit(v, d, (long)(z - d));
                    if (!pt->script) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)d); return 0; }
                }
            }
        }
        p += taille;
    }

    /* LE RECOUPEMENT. La liste des parts annonce sa taille totale ; le
     * parcours des parts une à une doit tomber exactement là. Deux chemins
     * indépendants vers la même frontière, et c'est ce qui distingue une
     * lecture vérifiée d'une lecture crédule. */
    if (p != deb_parts + taille_liste) {
        motif(pourquoi, npourquoi, "la liste des parts ne finit pas ou sa taille l'annonce", (unsigned long)p);
        return 0;
    }

    /* LES CONTENUS : un id (2), une taille (2) qui ne se compte pas elle-même,
     * puis la donnée ; et un octet de calage pour retomber sur un multiple de
     * deux.
     *
     * L'IDENTIFIANT PORTE DEUX INFORMATIONS. Négatif, c'est une part de CETTE
     * couche, d'id -valeur. Positif, dans un bloc CARD, c'est un champ du FOND
     * dont cette carte-là porte son propre texte — ce que notre modèle appelle
     * un BgText. Confondre les deux ferait afficher le même texte sur toutes les
     * cartes d'un fond.
     *
     * ET SUR LE TEXTE DÉCORÉ, NOS DEUX SOURCES SE CONTREDISENT. La spec dit que
     * l'entier de 0x4 est une TAILLE en octets, la liste des plages comprise et
     * lui-même compris ; l'autre lecteur le traite comme un NOMBRE de plages de
     * deux octets. Les deux calculs ne peuvent pas être justes ensemble. On suit
     * la spec, qui est explicite et cohérente avec elle-même, ET on vérifie que
     * la longueur obtenue tient dans le contenu : sinon, anomalie comptée et
     * texte laissé vide plutôt que pris n'importe où. */
    if (ncontenus > 0) {
        k->contenus = calloc(ncontenus, sizeof *k->contenus);
        if (!k->contenus) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)p); return 0; }
    }
    for (unsigned i = 0; i < ncontenus; i++) {
        int      ident  = s16(v, p + 0);
        unsigned taille = u16(v, p + 2);
        if (p + 4 + taille > bloc_fin) {
            motif(pourquoi, npourquoi, "contenu de part hors du bloc", (unsigned long)p);
            return 0;
        }

        HcOrigContenu *ct = &k->contenus[i];
        k->ncontenus = (int)(i + 1);        /* avant l'allocation, comme au-dessus */
        ct->du_fond = (ident > 0);
        ct->id_part = ct->du_fond ? ident : -ident;

        size_t debut = 0;
        long   len   = -1;
        if (taille >= 1 && v->o[p + 4] == 0) {
            debut = p + 5;                       /* texte nu */
            len   = (long)taille - 1;
        } else if (taille >= 2) {
            unsigned octets_plages = u16(v, p + 4) & 0x7FFFu;
            ct->decore = 1;
            pile->contenus_decores++;
            if (octets_plages >= 2 && octets_plages <= taille) {
                debut = p + 4 + octets_plages;
                len   = (long)taille - (long)octets_plages;
            }
        }
        if (len < 0 || debut + (size_t)len > bloc_fin) { pile->anomalies++; len = 0; debut = p + 4; }

        ct->texte = dit(v, debut, len);
        if (!ct->texte) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)debut); return 0; }

        p += 4 + taille;
        /* LE CALAGE SE COMPTE DEPUIS LE DÉBUT DU BLOC, pas depuis celui du
         * fichier. Rien ne garantit qu'un bloc commence à une adresse paire —
         * seule sa TAILLE est d'ordinaire un multiple de 32 — et compter
         * depuis le fichier décalerait d'un octet tout ce qui suit les
         * contenus : le nom de la carte, et donc son script. */
        if ((p - bloc) % 2) p++;
    }

    if (v->debord) { motif(pourquoi, npourquoi, "lecture hors du fichier", (unsigned long)p); return 0; }
    return p;
}

/* La queue commune aux blocs CARD et BKGD : les parts, les contenus, le nom,
 * le script. Les deux blocs ne diffèrent que par leur en-tête — d'où un seul
 * corps ici, et pas deux qui divergeraient à la première correction. */
static int lit_la_couche(Vue *v, HcOrigCouche *k, HcOrigPile *pile,
                         size_t bloc, size_t bloc_fin,
                         size_t off_nparts, size_t off_taille_liste,
                         size_t off_ncontenus, size_t deb_parts,
                         char *pourquoi, size_t npourquoi)
{
    unsigned      nparts    = u16(v, bloc + off_nparts);
    unsigned long taille_l  = u32(v, bloc + off_taille_liste);
    unsigned      ncontenus = u16(v, bloc + off_ncontenus);

    if (bloc + deb_parts + taille_l > bloc_fin) {
        motif(pourquoi, npourquoi, "liste des parts plus longue que le bloc", (unsigned long)bloc);
        return -1;
    }

    size_t p = lit_les_parts(v, k, pile, bloc, bloc + deb_parts, nparts, taille_l,
                             ncontenus, bloc_fin, pourquoi, npourquoi);
    if (!p) return -1;

    long ln = longueur_chaine(v, p);
    if (ln < 0 || p + (size_t)ln + 1 > bloc_fin) {
        motif(pourquoi, npourquoi, "nom de couche sans fin", (unsigned long)p);
        return -1;
    }
    k->nom = dit(v, p, ln);
    if (!k->nom) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)p); return -1; }

    size_t d = p + (size_t)ln + 1;
    size_t z = d;
    while (z < bloc_fin && v->o[z]) z++;
    if (z > d) {
        k->script = dit(v, d, (long)(z - d));
        if (!k->script) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)d); return -1; }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* L'ordre des cartes                                                  */
/* ------------------------------------------------------------------ */

/* Lit la chaîne LIST -> PAGE -> références de cartes, et VÉRIFIE les deux
 * sommes de contrôle que le format y met. Remplit `ordre` (les identifiants,
 * dans l'ordre de la pile) et `drapeaux` (l'octet de drapeaux de chaque
 * référence). Rend 1 si l'ordre a été lu ET vérifié, 0 sinon.
 *
 * ELLE NE REFUSE JAMAIS LE FICHIER, et c'est un choix. L'ordre des cartes n'est
 * pas nécessaire pour lire des scripts en sûreté : perdre les 66 scripts d'une
 * pile parce que la somme d'une page est fausse serait un mauvais échange. Mais
 * se rabattre en silence sur l'ordre du fichier serait malhonnête — d'où
 * `ordre_lu` à zéro, et une anomalie comptée si la liste existait sans se
 * vérifier.
 *
 * TROIS RECOUPEMENTS EN PLUS DES SOMMES, tous gratuits : le nombre total de
 * cartes est écrit DEUX FOIS dans la liste (0x18 et 0x28, l'assembleur recopie
 * simplement le premier) ; il doit valoir celui que STAK annonce ; et la taille
 * d'une référence doit s'accorder avec le nombre d'entiers de hachage qu'elle
 * porte, refsz == 4 + 4 * nhash. */
static int lit_l_ordre(Vue *v, HcOrigPile *pile, const unsigned char *octets,
                       int *ordre, unsigned char *drapeaux)
{
    size_t liste = 0;
    int trouvee = 0;
    for (int i = 0; i < pile->nblocs && !trouvee; i++)
        if (memcmp(octets + pile->blocs[i].offset + 4, "LIST", 4) == 0) {
            liste = (size_t)pile->blocs[i].offset;
            trouvee = 1;
        }
    if (!trouvee) return 0;          /* pas de liste : rien à reprocher */

    unsigned long npages  = u32(v, liste + 0x10);
    unsigned long ntotal  = u32(v, liste + 0x18);
    unsigned      refsz   = u16(v, liste + 0x1C);
    unsigned      nhash   = u16(v, liste + 0x20);
    unsigned long attendu = u32(v, liste + 0x24);
    unsigned long ntotal2 = u32(v, liste + 0x28);
    pile->npages = (int)npages;

    if (v->debord) { pile->anomalies++; return 0; }
    if (ntotal != ntotal2 || ntotal != pile->ncartes) { pile->anomalies++; return 0; }
    if (refsz < 4 || refsz != 4 + 4 * nhash)          { pile->anomalies++; return 0; }
    if (npages > (unsigned long)pile->nblocs)         { pile->anomalies++; return 0; }

    /* La somme de la liste, sur ses références de pages. */
    unsigned long somme = 0;
    for (unsigned long p = 0; p < npages; p++) {
        size_t r = liste + 0x30 + 6 * (size_t)p;
        somme = (somme + u32(v, r)) & 0xFFFFFFFFUL;
        somme = rotd3(somme);
        somme = (somme + u16(v, r + 4)) & 0xFFFFFFFFUL;
    }
    if (v->debord || somme != attendu) { pile->anomalies++; return 0; }

    /* Puis chaque page, avec SA somme, et les identifiants qu'elle porte. */
    unsigned long rang = 0;
    for (unsigned long p = 0; p < npages; p++) {
        size_t r = liste + 0x30 + 6 * (size_t)p;
        long   idpage = s32(v, r);
        unsigned ncartes = u16(v, r + 4);

        size_t page = 0;
        int vue = 0;
        for (int i = 0; i < pile->nblocs && !vue; i++)
            if (memcmp(octets + pile->blocs[i].offset + 4, "PAGE", 4) == 0
                && pile->blocs[i].id == (int)idpage) {
                page = (size_t)pile->blocs[i].offset;
                vue = 1;
            }
        if (!vue) { pile->anomalies++; return 0; }

        unsigned long som_page = 0, att_page = u32(v, page + 0x14);
        for (unsigned k = 0; k < ncartes; k++) {
            size_t ref = page + 0x18 + (size_t)k * refsz;
            som_page = (som_page + u32(v, ref)) & 0xFFFFFFFFUL;
            som_page = rotd3(som_page);
            if (rang < ntotal) {
                ordre[rang]    = (int)s32(v, ref);
                drapeaux[rang] = (ref + 4 < v->n) ? octets[ref + 4] : 0;
                rang++;
            }
        }
        if (v->debord || som_page != att_page) { pile->anomalies++; return 0; }
    }

    if (rang != ntotal) { pile->anomalies++; return 0; }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Le fichier entier                                                   */
/* ------------------------------------------------------------------ */

#define ENTETE     0x10      /* taille(4) type(4) id(4) calage(4) */
#define BLOCS_MAX  200000    /* une pile de 200 000 blocs n'existe pas */

static int lit_interne(const unsigned char *octets, size_t n,
                      HcOrigPile *pile, char *pourquoi, size_t npourquoi)
{
    memset(pile, 0, sizeof *pile);
    if (!octets || n < ENTETE) { motif(pourquoi, npourquoi, "fichier trop court pour un en-tete", 0); return -1; }

    /* NOTRE PROPRE FORMAT SE RECONNAÎT ET SE NOMME.
     *
     * Un fichier du format maison donnait « taille de bloc impossible », ce qui
     * est vrai et inutile : on cherche le défaut dans la pile alors qu'on s'est
     * trompé de lecteur. Mesuré en essayant d'ouvrir Graph_Maker.stack, déjà
     * converti, avec ce module-ci. */
    if (n >= 8 && memcmp(octets, "-- pile ", 8) == 0) {
        motif(pourquoi, npourquoi, "c'est le format maison, pas une pile d'origine : hc_load", 0);
        return -1;
    }

    Vue vue = { octets, n, 0 };
    Vue *v = &vue;

    /* --- 1. le recensement, qui se valide seul --- */
    int cap = 64;
    pile->blocs = malloc((size_t)cap * sizeof *pile->blocs);
    if (!pile->blocs) { motif(pourquoi, npourquoi, "memoire epuisee", 0); return -1; }

    size_t p = 0;
    while (p + ENTETE <= n) {
        unsigned long taille = u32(v, p);
        /* Une taille nulle, ou plus petite que l'en-tête, boucle sans fin.
         * Une taille qui dépasse la fin lit des octets qui n'existent pas. Les
         * deux se refusent, et c'est ici que se joue la sûreté du module. */
        if (taille < ENTETE || taille > n - p) {
            motif(pourquoi, npourquoi, "taille de bloc impossible", (unsigned long)p);
            return -1;
        }
        if (pile->nblocs >= cap) {
            if (cap >= BLOCS_MAX) { motif(pourquoi, npourquoi, "trop de blocs", (unsigned long)p); return -1; }
            cap *= 2;
            HcOrigBloc *neuf = realloc(pile->blocs, (size_t)cap * sizeof *pile->blocs);
            if (!neuf) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)p); return -1; }
            pile->blocs = neuf;
        }
        HcOrigBloc *b = &pile->blocs[pile->nblocs++];
        memcpy(b->type, octets + p + 4, 4);
        b->type[4] = '\0';
        for (int k = 0; k < 4; k++)
            if (b->type[k] < 0x20 || (unsigned char)b->type[k] > 0x7E) b->type[k] = '?';
        b->id     = (int)s32(v, p + 8);
        b->taille = taille;
        b->offset = (unsigned long)p;
        if (memcmp(octets + p + 4, "TAIL", 4) == 0) pile->tail_vu = 1;
        if (memcmp(octets + p + 4, "LIST", 4) == 0) pile->liste_vue = 1;
        p += taille;
    }
    pile->chaine_atteint_la_fin = (p == n);

    /* --- 2. STAK --- */
    if (pile->nblocs == 0 || memcmp(octets + 4, "STAK", 4) != 0) {
        motif(pourquoi, npourquoi, "le premier bloc n'est pas STAK", 0);
        return -1;
    }
    unsigned long taille_stak = pile->blocs[0].taille;
    pile->format  = u32(v, 0x10);
    pile->nfonds  = u32(v, 0x24);
    pile->ncartes = u32(v, 0x2C);
    pile->protegee = (u16(v, 0x4C) & 0x2000u) ? 1 : 0;
    {
        unsigned h = u16(v, 0x1B8), l = u16(v, 0x1BA);   /* Quickdraw : hauteur d'abord */
        pile->hauteur = h ? (int)h : 342;
        pile->largeur = l ? (int)l : 512;
    }

    /* LA SOMME DE CONTRÔLE, et c'est un cadeau du format : les 384 entiers de
     * 32 bits qui vont de 0 à 0x600 totalisent zéro. Une règle que nous
     * n'avons pas inventée — donc un écrivain de test à nous ne peut pas être
     * d'accord avec ce lecteur par complaisance : il faut qu'il la satisfasse.
     * C'est le seul contrôle de ce fichier qui vienne entièrement du dehors. */
    if (taille_stak >= 0x600) {
        unsigned long somme = 0;
        for (size_t i = 0; i < 0x600; i += 4) somme = (somme + u32(v, i)) & 0xFFFFFFFFUL;
        pile->somme_juste = (somme == 0);
    }

    /* Le script de la pile est à un offset FIXE, 0x600, juste après les motifs
     * et la table des blocs libres. */
    if (taille_stak > 0x600) {
        size_t d = 0x600, f = taille_stak, z = d;
        while (z < f && octets[z]) z++;
        if (z > d) {
            pile->script = dit(v, d, (long)(z - d));
            if (!pile->script) { motif(pourquoi, npourquoi, "memoire epuisee", 0x600); return -1; }
        }
    }

    /* UNE PILE À ACCÈS PRIVÉ A SES BLOCS CHIFFRÉS, et rien de ce qui suit
     * n'aurait de sens. On le DIT plutôt que de rendre du charabia : le
     * recensement et l'en-tête de STAK, eux, restent lisibles et sont déjà
     * dans `pile`. */
    if (pile->protegee) {
        motif(pourquoi, npourquoi, "pile a acces prive : les blocs sont chiffres", 0x4C);
        return -1;
    }

    /* HYPERCARD 1.x SE REFUSE, ET C'EST UN REFUS ÉCRIT, PAS UN OUBLI.
     *
     * Ses en-têtes de bloc n'ont pas les quatre octets de calage à 0xC, si bien
     * que TOUT le contenu des blocs CARD et BKGD est décalé de quatre octets
     * vers la gauche. Le recensement, lui, marche des deux côtés — la taille et
     * le type sont au même endroit — et il est donc rendu quand même.
     *
     * Le décalage est documenté et tiendrait en vingt lignes. Il n'est pas
     * écrit parce qu'il n'est pas MESURÉ : aucune pile 1.x n'est passée ici. Le
     * jour où il y en a une, ce refus dira exactement quoi faire. */
    if (pile->format < 9) {
        motif(pourquoi, npourquoi, "pile HyperCard 1.x : blocs decales, lecture non ecrite", 0x10);
        return -1;
    }

    /* --- 3. la table des polices --- */

    /* LES IDENTIFIANTS DE POLICE N'ÉTAIENT PAS LES MÊMES D'UN MACINTOSH À
     * L'AUTRE : HyperCard rangeait donc les NOMS dans le fichier, dans un bloc
     * FTBL. Sans lui, « police 3 » ne veut rien dire — et une part rendrait une
     * police qui n'est pas la sienne sur une autre machine, ce qui est
     * précisément le défaut que ce bloc existe pour éviter.
     *
     * Le bloc est FACULTATIF : les piles d'HyperCard 1.x n'en ont pas, et une
     * pile qui n'a jamais changé de police non plus. Son absence n'est donc pas
     * une faute ; les parts rendront simplement une police nulle. */
    for (int i = 0; i < pile->nblocs; i++) {
        if (memcmp(octets + pile->blocs[i].offset + 4, "FTBL", 4) != 0) continue;
        size_t bloc = (size_t)pile->blocs[i].offset;
        size_t fin  = bloc + (size_t)pile->blocs[i].taille;
        unsigned long combien = u32(v, bloc + 0x10);
        if (v->debord || combien == 0) break;
        /* Une entrée fait au moins trois octets — un identifiant et un nom
         * vide — donc le bloc borne leur nombre sans qu'on ait à le croire. */
        if (combien > (unsigned long)(pile->blocs[i].taille) / 3) { pile->anomalies++; break; }

        pile->polices = calloc(combien, sizeof *pile->polices);
        if (!pile->polices) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)bloc); return -1; }

        size_t q = bloc + 0x18;
        for (unsigned long j = 0; j < combien; j++) {
            long ln = longueur_chaine(v, q + 2);
            if (ln < 0 || q + 2 + (size_t)ln + 1 > fin) { pile->anomalies++; break; }
            pile->polices[j].id  = s16(v, q);
            pile->polices[j].nom = dit(v, q + 2, ln);
            if (!pile->polices[j].nom) { motif(pourquoi, npourquoi, "memoire epuisee", (unsigned long)q); return -1; }
            pile->npolices = (int)(j + 1);
            q += 2 + (size_t)ln + 1;
            if ((q - bloc) % 2) q++;          /* calage sur 16 bits */
        }
        break;
    }

    /* --- 4. les fonds et les cartes --- */

    /* IL NE PEUT PAS Y AVOIR PLUS DE CARTES QUE DE BLOCS : chaque carte a le
     * sien. Le nombre annoncé par STAK est donc borné par le recensement, qui
     * vient du fichier ENTIER et pas d'un seul entier de quatre octets.
     * Sans cette borne, un « nombre de cartes » abîmé — ou forgé — faisait
     * demander quatre milliards d'entrées à calloc. */
    if (pile->nfonds > (unsigned long)pile->nblocs || pile->ncartes > (unsigned long)pile->nblocs) {
        motif(pourquoi, npourquoi, "plus de cartes ou de fonds que de blocs", 0x24);
        return -1;
    }

    if (pile->nfonds) {
        pile->fonds = calloc(pile->nfonds, sizeof *pile->fonds);
        if (!pile->fonds) { motif(pourquoi, npourquoi, "memoire epuisee", 0x24); return -1; }
    }
    if (pile->ncartes) {
        pile->cartes = calloc(pile->ncartes, sizeof *pile->cartes);
        if (!pile->cartes) { motif(pourquoi, npourquoi, "memoire epuisee", 0x2C); return -1; }
    }

    /* L'ORDRE D'ABORD, LES CARTES ENSUITE. Sans lui on ne saurait pas où ranger
     * une carte, et l'ordre du fichier n'est pas celui de la pile. */
    int           *ordre    = NULL;
    unsigned char *drapeaux = NULL;
    unsigned char *occupe   = NULL;
    if (pile->ncartes) {
        ordre    = calloc(pile->ncartes, sizeof *ordre);
        drapeaux = calloc(pile->ncartes, sizeof *drapeaux);
        occupe   = calloc(pile->ncartes, sizeof *occupe);
        if (!ordre || !drapeaux || !occupe) {
            free(ordre); free(drapeaux); free(occupe);
            motif(pourquoi, npourquoi, "memoire epuisee", 0x34); return -1;
        }
        pile->ordre_lu = lit_l_ordre(v, pile, octets, ordre, drapeaux);
    }

    for (int i = 0; i < pile->nblocs; i++) {
        HcOrigBloc *b = &pile->blocs[i];
        size_t bloc = (size_t)b->offset, fin = bloc + (size_t)b->taille;
        int est_carte = (memcmp(octets + bloc + 4, "CARD", 4) == 0);
        int est_fond  = (memcmp(octets + bloc + 4, "BKGD", 4) == 0);
        if (!est_carte && !est_fond) continue;

        /* Plus de blocs que STAK n'en annonce : on s'arrête d'en ranger et on
         * le dit par la différence entre `ncartes` et `ncartes_lues`, que le
         * harnais affiche. Silencieusement en perdre serait pire. */
        HcOrigCouche *k;
        if (est_carte) {
            if ((unsigned long)pile->ncartes_lues >= pile->ncartes) continue;
            /* LA PLACE VIENT DE LA LISTE, pas du rang dans le fichier. Une carte
             * que la liste ne nomme pas est comptée en anomalie et rangée à la
             * suite : elle existe, elle n'est simplement pas à sa place. */
            long place = -1;
            if (pile->ordre_lu) {
                for (unsigned long q = 0; q < pile->ncartes; q++)
                    if (ordre[q] == b->id) { place = (long)q; break; }
                if (place < 0) pile->anomalies++;
            }
            if (place < 0) {
                for (unsigned long q = 0; q < pile->ncartes; q++)
                    if (!occupe[q]) { place = (long)q; break; }
                if (place < 0) continue;
            }

            /* L'OCCUPATION SE SUIT À PART, ET C'EST UNE CORRECTION.
             *
             * Un emplacement était réputé libre quand son identifiant valait
             * zéro. Mais un bloc CARD peut PORTER l'identifiant zéro — une
             * mutation d'un seul octet suffit — et son emplacement restait alors
             * marqué libre après avoir été rempli : la carte suivante venait
             * écraser son nom et son script sans les libérer. De même, deux
             * blocs CARD de même identifiant visaient la même place.
             *
             * Le fuizzing l'a trouvé deux fois sur 6162 fichiers abîmés, et ma
             * première hypothèse — le compteur de parts monté trop tard — était
             * fausse : c'est la trace de LeakSanitizer qui a nommé le vrai
             * coupable, le NOM d'une couche. Deviner deux fois de suite sur un
             * défaut mémoire coûte plus cher que lire la trace une fois. */
            if (occupe[place]) continue;    /* déjà remplie : on n'écrase rien */
            occupe[place] = 1;

            k = &pile->cartes[place];
            if (pile->ordre_lu) {
                /* LA NUMÉROTATION DES BITS EST MESURÉE, pas supposée. Sur une
                 * vraie pile de 1990, le bit 7 est allumé pour l'unique carte
                 * qui porte un nom, et le bit 6 pour les trois premières
                 * cartes de chacun des trois fonds — aux positions 0, 4 et 10
                 * de l'ordre calculé ici. Deux témoins, dont un unique. Le
                 * bit 4 (« marked ») découle de la même numérotation ; aucune
                 * carte marquée n'est passée ici, ça reste à voir. */
                k->marque        = (drapeaux[place] & 0x10) ? 1 : 0;
                k->debut_de_fond = (drapeaux[place] & 0x40) ? 1 : 0;
            }
        } else {
            if ((unsigned long)pile->nfonds_lus >= pile->nfonds) continue;
            k = &pile->fonds[pile->nfonds_lus];
        }
        k->id = b->id;

        /* Les drapeaux de la COUCHE, à 0x14, les mêmes pour CARD et BKGD :
         * bit 11 dontSearch, bit 14 cantDelete. Et l'identifiant du bloc BMAP à
         * 0x10 — le dessin n'est pas lu, mais savoir qu'il existe évite de
         * prendre une couche illustrée pour une couche vide. */
        {
            unsigned f = u16(v, bloc + 0x14);
            k->dont_search  = (f & 0x0800u) ? 1 : 0;
            k->cant_delete  = (f & 0x4000u) ? 1 : 0;
            k->bloc_image   = (int)s32(v, bloc + 0x10);
        }

        int r;
        if (est_carte) {
            k->fond = (int)s32(v, bloc + 0x24);
            r = lit_la_couche(v, k, pile, bloc, fin, 0x28, 0x2C, 0x30, 0x36, pourquoi, npourquoi);
        } else {
            r = lit_la_couche(v, k, pile, bloc, fin, 0x24, 0x28, 0x2C, 0x32, pourquoi, npourquoi);
        }
        if (r != 0) { free(ordre); free(drapeaux); free(occupe); return -1; }

        /* LE DESSIN APRÈS LA COUCHE, et non avant : lit_la_couche peut refuser,
         * et l'on n'aura pas alloué deux plans pour rien. */
        lit_le_dessin(v, pile, k);

        /* DEUX SOURCES POUR LE MÊME FAIT, donc un recoupement gratuit : la
         * référence de la liste dit si la carte porte un nom (bit 7), et le
         * bloc CARD porte le nom lui-même. Elles doivent s'accorder. Si elles
         * ne s'accordent pas, c'est qu'on a apparié la mauvaise référence avec
         * le mauvais bloc — exactement la faute qu'un ordre mal lu produirait,
         * et qui autrement ne se verrait pas. */
        if (est_carte && pile->ordre_lu) {
            int annonce = (drapeaux[k - pile->cartes] & 0x80) ? 1 : 0;
            int reel    = (k->nom && k->nom[0]) ? 1 : 0;
            if (annonce != reel) pile->anomalies++;
        }

        if (est_carte) pile->ncartes_lues++; else pile->nfonds_lus++;
    }
    free(ordre); free(drapeaux); free(occupe);

    if (v->debord) { motif(pourquoi, npourquoi, "lecture hors du fichier", 0); return -1; }
    return 0;
}

/* UN REFUS NE LAISSE RIEN DERRIÈRE LUI, et ce n'est pas une politesse.
 *
 * La lecture alloue au fur et à mesure — le recensement des blocs, puis les
 * couches, puis les noms et les scripts — et elle peut refuser à n'importe
 * quel moment. Il faut donc que QUELQU'UN libère ce qui a déjà été pris.
 *
 * Ce quelqu'un était le CALLEUR, et c'était une mauvaise idée : le contrat
 * n'était écrit nulle part, et le premier consommateur écrit hors du harnais
 * l'a oublié. Un fuzzing de 2321 fichiers abîmés a signalé une fuite à CHACUN
 * des 431 refus.
 *
 * ET LA PROVENANCE EXACTE, parce qu'elle n'est pas celle que j'ai d'abord
 * annoncée : l'essentiel de ces fuites était le tampon du FICHIER, que ce
 * pilote-là ne libérait pas — un défaut d'appelant, pas du module. Ce qui
 * revenait bien au module était plus modeste : sous l'ancien contrat, rien ne
 * libérait le recensement des blocs sur un chemin de refus. Les deux étaient
 * dans le même appelant bâclé, et c'est l'argument : une API dont le bon usage
 * n'est pas évident finira toujours par être mal appelée.
 *
 * Donc c'est le module qui nettoie. Un refus rend une pile VIDE, et le
 * calleur n'a plus rien à savoir. On perd le recensement partiel d'un fichier
 * refusé — aucun appelant ne s'en servait, et une API qui ne peut pas fuir vaut
 * mieux qu'une API avec une mise en garde. */
int hc_origine_reconnait(const unsigned char *octets, size_t n)
{
    /* Seize octets, c'est la taille d'un en-tête de bloc : moins que ça et il
     * n'y a pas de premier bloc du tout. */
    if (!octets || n < 16) return 0;
    return memcmp(octets + 4, "STAK", 4) == 0;
}

int hc_origine_lit(const unsigned char *octets, size_t n,
                   HcOrigPile *pile, char *pourquoi, size_t npourquoi)
{
    if (pourquoi && npourquoi) pourquoi[0] = '\0';
    if (!pile) return -1;
    int r = lit_interne(octets, n, pile, pourquoi, npourquoi);
    if (r != 0) {
        /* `pourquoi` est déjà rempli, et hc_origine_libere remet la pile à
         * zéro : le motif du refus survit, la mémoire non. */
        hc_origine_libere(pile);
    }
    return r;
}

void hc_origine_libere(HcOrigPile *pile)
{
    if (!pile) return;
    free(pile->blocs);
    free(pile->script);
    for (int i = 0; i < pile->npolices; i++) free(pile->polices[i].nom);
    free(pile->polices);
    for (int quoi = 0; quoi < 2; quoi++) {
        HcOrigCouche *tab = quoi ? pile->cartes : pile->fonds;
        int combien = quoi ? (int)pile->ncartes : (int)pile->nfonds;
        if (!tab) continue;
        for (int i = 0; i < combien; i++) {
            free(tab[i].nom);
            free(tab[i].script);
            for (int j = 0; j < tab[i].nparts; j++) {
                free(tab[i].parts[j].nom);
                free(tab[i].parts[j].script);
                free(tab[i].parts[j].police);
            }
            free(tab[i].parts);
            for (int j = 0; j < tab[i].ncontenus; j++) free(tab[i].contenus[j].texte);
            free(tab[i].contenus);
            free(tab[i].dessin.image);
            free(tab[i].dessin.masque);
        }
        free(tab);
    }
    memset(pile, 0, sizeof *pile);
}
