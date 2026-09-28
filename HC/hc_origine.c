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

/* ------------------------------------------------------------------ */
/* Les parts                                                           */
/* ------------------------------------------------------------------ */

/* Le genre d'une part : bit 8 de l'entier 16 bits à 0x4, 0 pour un champ et 1
 * pour un bouton.
 *
 * C'EST LE SEUL CHAMP DE CE FICHIER QUE NOS DEUX SOURCES NE CORROBORENT PAS.
 * L'une le décrit ainsi ; l'autre ne lit jamais le genre d'une part, donc elle
 * ne confirme ni ne contredit. Écrit ici comme non mesuré : une vraie pile
 * d'origine tranchera, et le harnais porte le cas des deux valeurs pour que le
 * jour où ça change, on voie quoi. */
static int genre_de(unsigned drapeaux) { return (drapeaux & 0x0100u) ? HC_ORIG_BOUTON : HC_ORIG_CHAMP; }

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
        pt->id     = (int)u16(v, p + 0x02);
        pt->genre  = genre_de(u16(v, p + 0x04));
        pt->haut   = (int)u16(v, p + 0x06);
        pt->gauche = (int)u16(v, p + 0x08);
        pt->bas    = (int)u16(v, p + 0x0A);
        pt->droite = (int)u16(v, p + 0x0C);

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
        k->nparts = (int)(i + 1);
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

    /* Les contenus : un id (2), une taille (2) qui ne se compte pas elle-même,
     * puis la donnée ; et un octet de calage pour retomber sur un multiple de
     * deux. */
    for (unsigned i = 0; i < ncontenus; i++) {
        unsigned taille = u16(v, p + 2);
        if (p + 4 + taille > bloc_fin) {
            motif(pourquoi, npourquoi, "contenu de part hors du bloc", (unsigned long)p);
            return 0;
        }
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
/* Le fichier entier                                                   */
/* ------------------------------------------------------------------ */

#define ENTETE     0x10      /* taille(4) type(4) id(4) calage(4) */
#define BLOCS_MAX  200000    /* une pile de 200 000 blocs n'existe pas */

int hc_origine_lit(const unsigned char *octets, size_t n,
                   HcOrigPile *pile, char *pourquoi, size_t npourquoi)
{
    if (pourquoi && npourquoi) pourquoi[0] = '\0';
    if (!pile) return -1;
    memset(pile, 0, sizeof *pile);
    if (!octets || n < ENTETE) { motif(pourquoi, npourquoi, "fichier trop court pour un en-tete", 0); return -1; }

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

    /* --- 3. les fonds et les cartes --- */

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
            k = &pile->cartes[pile->ncartes_lues];
        } else {
            if ((unsigned long)pile->nfonds_lus >= pile->nfonds) continue;
            k = &pile->fonds[pile->nfonds_lus];
        }
        k->id = b->id;

        int r;
        if (est_carte) {
            k->fond = (int)s32(v, bloc + 0x24);
            r = lit_la_couche(v, k, pile, bloc, fin, 0x28, 0x2C, 0x30, 0x36, pourquoi, npourquoi);
        } else {
            r = lit_la_couche(v, k, pile, bloc, fin, 0x24, 0x28, 0x2C, 0x32, pourquoi, npourquoi);
        }
        if (r != 0) return -1;

        if (est_carte) pile->ncartes_lues++; else pile->nfonds_lus++;
    }

    if (v->debord) { motif(pourquoi, npourquoi, "lecture hors du fichier", 0); return -1; }
    return 0;
}

void hc_origine_libere(HcOrigPile *pile)
{
    if (!pile) return;
    free(pile->blocs);
    free(pile->script);
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
            }
            free(tab[i].parts);
        }
        free(tab);
    }
    memset(pile, 0, sizeof *pile);
}
