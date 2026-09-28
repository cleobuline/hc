/* origine.c — Lecture d'une pile HyperCard au format D'ORIGINE.
 *
 * CE QUE CE HARNAIS PROUVE, ET CE QU'IL NE PROUVE PAS. À lire avant de croire
 * son verdict.
 *
 * Il MONTE une pile au format binaire d'Apple, puis la relit avec
 * hc_origine_lit. Pris seul, cela ne prouverait presque rien : mon écrivain et
 * mon lecteur seraient d'accord parce qu'ils partagent la même croyance, et si
 * cette croyance est fausse ils se tromperaient ensemble, en silence, pendant
 * que le test passerait au vert. C'est le défaut que ce dépôt punit le plus
 * souvent — l'instrument qui mesure autre chose que ce qu'il annonce.
 *
 * Trois contrepoids, qui ne viennent pas de moi :
 *
 * 1. LA SOMME DE CONTRÔLE DU BLOC STAK. Le format exige que les 384 entiers de
 *    32 bits allant de 0 à 0x600 totalisent zéro. L'écrivain ci-dessous doit la
 *    satisfaire ; c'est une règle du dehors, que je n'ai pas inventée et que je
 *    ne peux pas assouplir pour me faire plaisir.
 *
 * 2. UN LECTEUR TIERS. Le fichier monté ici s'écrit sur le disque si on donne
 *    un chemin en argument, et un lecteur écrit par quelqu'un d'autre, dans un
 *    autre langage, le lit et doit en tirer les mêmes scripts. Voir
 *    docs/mesures/pile_origine.txt : c'est l'arbitre, le rôle que Basilisk
 *    joue pour le langage.
 *
 * 3. LA TABLE MACROMAN. Les 256 conversions sont affichées, et elles ont été
 *    comparées une par une au codec « mac_roman » de Python. La référence de ce
 *    harnais les fige : un doigt qui glisse dans la table casse un test, au
 *    lieu de casser un accent qu'on découvrirait dans dix ans.
 *
 * CE QU'IL NE PROUVE TOUJOURS PAS : qu'une VRAIE pile de 1993 se lise. Aucune
 * n'est encore passée ici — ni le réseau du conteneur ni le dépôt n'en porte.
 * Ce qui manque est écrit dans docs/mesures/pile_origine.txt, et ce harnais
 * dira la vérité le jour où on lui en donnera une.
 */
#include "hc_origine.h"
#include "hc_importe.h"
#include "hc_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Un écrivain, uniquement pour le test                                */
/* ------------------------------------------------------------------ */

#define NHASH   4                       /* entiers de hachage par référence */
#define REFSZ   (4 + 4 * NHASH)         /* 20, comme la vraie pile mesurée */
#define ID_LIST 4803
#define ID_PAGE 4804

typedef struct { unsigned char *o; size_t n, cap; } Tampon;

static void grossis(Tampon *t, size_t combien)
{
    if (t->n + combien <= t->cap) return;
    size_t neuf = t->cap ? t->cap : 256;
    while (neuf < t->n + combien) neuf *= 2;
    unsigned char *d = realloc(t->o, neuf);
    if (!d) { fprintf(stderr, "memoire\n"); exit(1); }
    t->o = d; t->cap = neuf;
}
static void pose8(Tampon *t, unsigned v)  { grossis(t, 1); t->o[t->n++] = (unsigned char)v; }
static void pose16(Tampon *t, unsigned v) { pose8(t, (v >> 8) & 0xFF); pose8(t, v & 0xFF); }
static void pose32(Tampon *t, unsigned long v)
{
    pose8(t, (unsigned)((v >> 24) & 0xFF)); pose8(t, (unsigned)((v >> 16) & 0xFF));
    pose8(t, (unsigned)((v >> 8) & 0xFF));  pose8(t, (unsigned)(v & 0xFF));
}
static void pose_zeros(Tampon *t, size_t combien) { while (combien--) pose8(t, 0); }
static void pose_texte(Tampon *t, const char *s) { while (*s) pose8(t, (unsigned char)*s++); pose8(t, 0); }
static void ecris32(Tampon *t, size_t ou, unsigned long v)
{
    t->o[ou] = (unsigned char)((v >> 24) & 0xFF); t->o[ou+1] = (unsigned char)((v >> 16) & 0xFF);
    t->o[ou+2] = (unsigned char)((v >> 8) & 0xFF); t->o[ou+3] = (unsigned char)(v & 0xFF);
}

/* Une part : en-tête de 0x1E octets, le nom, puis (s'il y a un script) un zéro
 * et le script. La taille se réécrit à la fin, quand on la connaît.
 *
 * LES DRAPEAUX SONT POSÉS AVEC DES VALEURS DISTINCTES D'UNE PART À L'AUTRE, et
 * ce n'est pas de la décoration : quatre bits du format sont INVERSÉS — le bit
 * allumé signifie faux — et quatre autres CHANGENT DE SENS selon le genre de la
 * part. Un jeu de drapeaux tous à zéro, ou tous les mêmes, ne distinguerait ni
 * une inversion oubliée ni deux familles mélangées. La référence de la suite
 * porte donc un bouton et un champ dont AUCUN drapeau ne coïncide. */
static void pose_part(Tampon *t, int id, int bouton, int h, int g, int b, int d,
                      unsigned drapeaux, unsigned drapeaux2, unsigned style,
                      int police, int corps, const char *nom, const char *script)
{
    size_t deb = t->n;
    pose16(t, 0);                               /* taille, réécrite plus bas */
    pose16(t, (unsigned)id);
    pose16(t, (bouton ? 0x0100u : 0x0000u) | drapeaux);
    pose16(t, (unsigned)h); pose16(t, (unsigned)g);
    pose16(t, (unsigned)b); pose16(t, (unsigned)d);
    pose8(t, drapeaux2);                        /* 0x0E */
    pose8(t, style);                            /* 0x0F */
    pose16(t, bouton ? 42u : 3u);               /* 0x10 largeur du titre / derniere ligne */
    pose16(t, bouton ? 7u : 1u);                /* 0x12 icone / premiere ligne */
    pose16(t, bouton ? 1u : 0xFFFFu);           /* 0x14 alignement : centre / droite */
    pose16(t, (unsigned)police);                /* 0x16 identifiant de police */
    pose16(t, (unsigned)corps);                 /* 0x18 corps */
    pose8(t, 0x03);                             /* 0x1A gras + italique */
    pose8(t, 0);                                /* 0x1B calage */
    pose16(t, 18);                              /* 0x1C hauteur de ligne */
    pose_texte(t, nom);
    if (script) { pose8(t, 0); pose_texte(t, script); }
    if ((t->n - deb) % 2) pose8(t, 0);          /* calage sur 16 bits */
    size_t taille = t->n - deb;
    t->o[deb] = (unsigned char)((taille >> 8) & 0xFF);
    t->o[deb+1] = (unsigned char)(taille & 0xFF);
}

/* La table des polices. Deux entrées, dont une à identifiant ÉLEVÉ : les vraies
 * piles en portent (16383 pour Chicago dans l'une, 2002 pour Charcoal), et c'est
 * précisément pourquoi HyperCard rangeait les NOMS. */
static void pose_ftbl(Tampon *t)
{
    size_t deb = t->n;
    pose32(t, 0);
    for (const char *s = "FTBL"; *s; s++) pose8(t, (unsigned char)*s);
    pose32(t, 6000);
    pose32(t, 0);
    pose32(t, 2);                               /* 0x10 deux polices */
    pose32(t, 0);                               /* 0x14 */
    pose16(t, 3);   pose_texte(t, "Geneva");    /* 0x18 */
    if ((t->n - deb) % 2) pose8(t, 0);
    pose16(t, 16383); pose_texte(t, "Chicago");
    if ((t->n - deb) % 2) pose8(t, 0);
    while ((t->n - deb) % 32) pose8(t, 0);
    ecris32(t, deb, (unsigned long)(t->n - deb));
}

/* Un contenu de part : id, taille, marqueur de texte nu, la donnée. */
static void pose_contenu(Tampon *t, int id, const char *texte)
{
    size_t deb = t->n;
    pose16(t, (unsigned)(id & 0xFFFF));
    pose16(t, 0);                               /* taille, réécrite */
    pose8(t, 0);                                /* texte nu */
    for (const char *s = texte; *s; s++) pose8(t, (unsigned char)*s);
    size_t taille = t->n - deb - 4;
    t->o[deb+2] = (unsigned char)((taille >> 8) & 0xFF);
    t->o[deb+3] = (unsigned char)(taille & 0xFF);
    if ((t->n - deb) % 2) pose8(t, 0);
}

/* ------------------------------------------------------------------ */
/* La pile de démonstration                                            */
/* ------------------------------------------------------------------ */

/* Des accents en MACROMAN, un octet chacun : é=0x8E, à=0x88, ë=0x91, ç=0x8D.
 * C'est tout l'intérêt — un script d'origine les porte ainsi, et le noyau
 * n'avait aucune table pour les rendre en UTF-8. Les fins de ligne sont des
 * « \r », comme dans toutes les piles d'HyperCard. */
#define E_AIGU  "\x8E"
#define A_GRAVE "\x88"
#define E_TREMA "\x91"
#define C_CEDIL "\x8D"
#define E_GRAVE "\x8F"
#define E_CIRC  "\x90"

static const char SCRIPT_PILE[] =
    "on openStack\r"
    "  put \"pr" E_CIRC "t\" into card field 1\r"
    "end openStack";

static const char SCRIPT_CARTE[] =
    "on mouseUp\r"
    "  put \"" E_AIGU "l" E_GRAVE "ve " A_GRAVE " No" E_TREMA "l\" into card field 1\r"
    "  " C_CEDIL "a marche\r"
    "end mouseUp";

/* CE SCRIPT PORTE LES QUATRE CARACTÈRES QUE hc_set_script NORMALISE, et c'est
 * délibéré : « ≤ » (0xB2 en MacRoman) devient « <= », et « ¬ » (0xAC) est une
 * continuation de ligne qui aboute la ligne suivante.
 *
 * Sans eux, le tour complet croyait comparer des scripts identiques et ne
 * testait rien de cette transformation — qui n'avait aucun témoin dans ce
 * harnais. Les vraies piles en sont pleines : dix des scripts de « 3D Parametric
 * Equations » et de la pile d'Apple les portent, et c'est ce qui a fait croire
 * un instant à dix pertes. */
#define INFEG   "\xB2"                  /* ≤ */
/* LE « ¬ » DE MACROMAN EST 0xC2, PAS 0xAC. J'avais écrit 0xAC, qui est « ¨ » —
 * le tréma — si bien que la continuation n'était pas exercée du tout : le harnais
 * et la normalisation étaient d'accord pour ne rien faire, et le test passait en
 * ne testant rien. hc_script.c le dit noir sur blanc depuis toujours : « ¬ 0xC2
 * / 0xC2 0xAC ». Vérifié par mutation des DEUX branches, cette fois. */
#define CONTIN  "\xC2"                  /* ¬ */

static const char SCRIPT_BOUTON[] =
    "on mouseUp\r"
    "  if the number of cards " INFEG " 3 then beep\r"
    "  put \"une ligne\" " CONTIN "\r"
    "     & \" coupee en deux\"\r"
    "end mouseUp";
static const char SCRIPT_FOND[]   = "on openCard\r  pass openCard\rend openCard";

static void pose_stak(Tampon *t, unsigned long format, unsigned long nfonds,
                      unsigned long ncartes, unsigned protection)
{
    size_t deb = t->n;                          /* 0, toujours : STAK est premier */
    pose32(t, 0);                               /* taille, réécrite */
    pose_texte(t, "STAK"); t->n--;              /* le type n'est pas terminé par zéro */
    pose32(t, 0xFFFFFFFFUL);                    /* id = -1 */
    pose32(t, 0);                               /* calage */
    pose32(t, format);                          /* 0x10 */
    pose_zeros(t, 0x24 - 0x14);
    pose32(t, nfonds);                          /* 0x24 */
    pose32(t, 0);                               /* 0x28 premier fond */
    pose32(t, ncartes);                         /* 0x2C */
    pose32(t, 0);                               /* 0x30 premiere carte */
    pose32(t, (unsigned long)ID_LIST);          /* 0x34 le bloc LIST */
    pose_zeros(t, 0x4C - 0x38);
    pose16(t, protection);                      /* 0x4C */
    pose_zeros(t, 0x1B8 - 0x4E);
    pose16(t, 342);                             /* 0x1B8 hauteur (Quickdraw) */
    pose16(t, 512);                             /* 0x1BA largeur */
    pose_zeros(t, 0x600 - (t->n - deb));
    for (const char *s = SCRIPT_PILE; *s; s++) pose8(t, (unsigned char)*s);
    pose8(t, 0);
    while ((t->n - deb) % 32) pose8(t, 0);      /* les blocs sont calés sur 32 */

    /* LA SOMME DE CONTRÔLE, POSÉE EN DERNIER. Les 384 entiers de 0 à 0x600
     * doivent totaliser zéro ; le champ de 0x70 existe exactement pour ça. On
     * additionne tout le reste et on y écrit l'opposé. */
    ecris32(t, deb, (unsigned long)(t->n - deb));   /* la taille, enfin connue */
    unsigned long somme = 0;
    for (size_t i = 0; i < 0x600; i += 4) {
        if (i == 0x70) continue;
        somme = (somme + (((unsigned long)t->o[deb+i] << 24) | ((unsigned long)t->o[deb+i+1] << 16)
               | ((unsigned long)t->o[deb+i+2] << 8) | t->o[deb+i+3])) & 0xFFFFFFFFUL;
    }
    ecris32(t, deb + 0x70, (0x100000000ULL - somme) & 0xFFFFFFFFUL);
}

/* La queue commune : parts, contenus, nom, script. `poseur` décrit ce qu'on
 * met, pour ne pas écrire deux fois le même code pour CARD et BKGD. */
static void pose_couche(Tampon *t, const char *type, int id, int fond,
                        int avec_bouton, int avec_champ, int avec_contenu,
                        const char *nom, const char *script)
{
    size_t deb = t->n;
    pose32(t, 0);                               /* taille, réécrite */
    for (const char *s = type; *s; s++) pose8(t, (unsigned char)*s);
    pose32(t, (unsigned long)id);
    pose32(t, 0);                               /* calage */
    pose32(t, 0);                               /* 0x10 bloc BMAP : aucun */

    int carte = (strcmp(type, "CARD") == 0);
    unsigned nparts = (unsigned)(avec_bouton + avec_champ);
    unsigned ncont  = (unsigned)(avec_contenu ? 3 : 0);

    /* Les parts et les contenus se montent à part, pour connaître leur taille
     * totale avant d'écrire les en-têtes qui l'annoncent. */
    Tampon parts = {0,0,0}, conts = {0,0,0};
    /* AUCUN DRAPEAU NE COÏNCIDE ENTRE LES DEUX, pour que la référence distingue
     * une inversion oubliée d'un mélange de familles.
     *
     * Le champ : 0x0020 dontWrap, 0x0010 dontSearch, 0x0001 lockText — et
     * 0x0004 ABSENT, donc fixedLineHeight VRAI, puisque ce bit-là est inversé.
     * Son octet de 0xE vaut 0x50 : showLines et multipleLines, ni autoSelect ni
     * wideMargins. Style 7 : scrolling, qui n'existe que pour un champ.
     *
     * Le bouton : 0x0080 donc INVISIBLE, 0x0004 donc fixedLineHeight FAUX,
     * 0x0001 donc DÉSACTIVÉ, 0x0002 autoTab. Son octet de 0xE vaut 0xA5 :
     * showName et autoHilite allumés, hilite éteint, le bit 4 éteint donc
     * sharedHilite VRAI, et la famille 5. Style 5 : checkbox, qui n'existe que
     * pour un bouton. */
    if (avec_champ)  pose_part(&parts, 1, 0, 54, 20, 80, 250,
                               0x0031u, 0x50u, 7, 3, 12, "A", NULL);
    if (avec_bouton) pose_part(&parts, 2, 1, 302, 20, 324, 110,
                               0x0087u, 0xA5u, 5, 16383, 9, "TORTURE", SCRIPT_BOUTON);
    /* DEUX CONTENUS, ET LE SIGNE DE L'IDENTIFIANT EST TOUT L'OBJET : -1 désigne
     * la part 1 DE CETTE COUCHE, tandis que +1 désigne le champ 1 DU FOND dont
     * cette carte-là porte son propre texte. Les confondre ferait afficher le
     * même texte sur toutes les cartes du fond. */
    if (avec_contenu) {
        pose_contenu(&conts, -1, "du texte a la carte");
        pose_contenu(&conts,  1, "du texte au fond");
        /* UN ORPHELIN, exprès : la part 99 n'existe nulle part. Les vraies piles
         * en portent — cinq dans « 3D Parametric Equations » — et sans ce témoin
         * le cas n'aurait aucune trace dans la référence. */
        pose_contenu(&conts, -99, "un texte sans part");
    }

    if (carte) {
        pose16(t, 0); pose16(t, 0);             /* 0x14 drapeaux, 0x16 calage */
        pose_zeros(t, 8);                       /* 0x18 */
        pose32(t, 0);                           /* 0x20 bloc PAGE */
        pose32(t, (unsigned long)fond);         /* 0x24 le fond */
        pose16(t, nparts);                      /* 0x28 */
        pose16(t, 3);                           /* 0x2A id de part suivant */
        pose32(t, (unsigned long)parts.n);      /* 0x2C taille de la liste */
        pose16(t, ncont);                       /* 0x30 */
        pose32(t, (unsigned long)conts.n);      /* 0x32 */
    } else {
        pose16(t, 0); pose16(t, 0);             /* 0x14, 0x16 */
        pose32(t, 2);                           /* 0x18 nombre de cartes */
        pose32(t, 0); pose32(t, 0);             /* 0x1C, 0x20 fonds voisins */
        pose16(t, nparts);                      /* 0x24 */
        pose16(t, 3);                           /* 0x26 */
        pose32(t, (unsigned long)parts.n);      /* 0x28 */
        pose16(t, ncont);                       /* 0x2C */
        pose32(t, (unsigned long)conts.n);      /* 0x2E */
    }
    /* LE TEST SUR LA LONGUEUR N'EST PAS DU ZÈLE. Une carte sans parts ni
     * contenus laisse ces deux tampons vides, donc leur pointeur nul, et
     * « memcpy(dst, NULL, 0) » est un comportement indéfini — même pour zéro
     * octet. Rien ne se voit à l'exécution ; UBSan le dit, et c'est la seule
     * raison pour laquelle on le sait. */
    if (parts.n) { grossis(t, parts.n); memcpy(t->o + t->n, parts.o, parts.n); t->n += parts.n; }
    if (conts.n) { grossis(t, conts.n); memcpy(t->o + t->n, conts.o, conts.n); t->n += conts.n; }
    free(parts.o); free(conts.o);

    pose_texte(t, nom);
    if (script) for (const char *s = script; *s; s++) pose8(t, (unsigned char)*s);
    pose8(t, 0);
    while ((t->n - deb) % 32) pose8(t, 0);
    ecris32(t, deb, (unsigned long)(t->n - deb));
}

/* ------------------------------------------------------------------ */
/* La liste des cartes                                                 */
/* ------------------------------------------------------------------ */

/* Les deux sommes de contrôle de la liste, qui viennent de l'assembleur
 * d'HyperCard et pas de nous : une rotation de trois bits vers la droite entre
 * chaque terme. Si l'écrivain d'ici les calcule mal, le lecteur refuse l'ordre
 * — c'est une règle du dehors, et c'est tout son intérêt. */
static unsigned long rot3(unsigned long x)
{
    x &= 0xFFFFFFFFUL;
    return ((x >> 3) | (x << 29)) & 0xFFFFFFFFUL;
}

/* L'ORDRE DE LA LISTE EST L'INVERSE DE CELUI DU FICHIER, et c'est tout l'objet
 * du test. Le fichier porte CARD 3000 puis CARD 3001 ; la liste dit 3001 puis
 * 3000. Un lecteur qui se contenterait de l'ordre du fichier rendrait donc
 * l'inverse de ce qu'on attend, et la référence le verrait. Une liste qui
 * répéterait l'ordre du fichier ne distinguerait rien. */
static const int ORDRE[2]  = { 3001, 3000 };
/* bit 6 : début d'un fond — la première carte de l'ordre, les deux partageant
 * le même fond. bit 7 : porte un nom — 3000 s'appelle « Atelier », 3001 non.
 * bit 5 : porte du texte, sur les deux. */
static const unsigned FLAGS[2] = { 0x20 | 0x40, 0x20 | 0x80 };

static void pose_page(Tampon *t)
{
    size_t deb = t->n;
    pose32(t, 0);
    for (const char *s = "PAGE"; *s; s++) pose8(t, (unsigned char)*s);
    pose32(t, (unsigned long)ID_PAGE);
    pose32(t, 0);
    pose32(t, (unsigned long)ID_LIST);          /* 0x10 */
    size_t ou_somme = t->n;
    pose32(t, 0);                               /* 0x14 somme, réécrite */
    unsigned long somme = 0;
    for (int i = 0; i < 2; i++) {               /* 0x18 les références */
        pose32(t, (unsigned long)ORDRE[i]);
        pose8(t, FLAGS[i]);
        pose_zeros(t, REFSZ - 5);               /* le hachage de recherche */
        somme = (somme + (unsigned long)ORDRE[i]) & 0xFFFFFFFFUL;
        somme = rot3(somme);
    }
    ecris32(t, ou_somme, somme);
    while ((t->n - deb) % 32) pose8(t, 0);
    ecris32(t, deb, (unsigned long)(t->n - deb));
}

static void pose_liste(Tampon *t)
{
    size_t deb = t->n;
    pose32(t, 0);
    for (const char *s = "LIST"; *s; s++) pose8(t, (unsigned char)*s);
    pose32(t, (unsigned long)ID_LIST);
    pose32(t, 0);
    pose32(t, 1);                               /* 0x10 une page */
    pose32(t, 0x800);                           /* 0x14 taille d'une page */
    pose32(t, 2);                               /* 0x18 nombre de cartes */
    pose16(t, REFSZ);                           /* 0x1C */
    pose16(t, 2);                               /* 0x1E toujours 2 */
    pose16(t, NHASH);                           /* 0x20 */
    pose16(t, 0);                               /* 0x22 */
    size_t ou_somme = t->n;
    pose32(t, 0);                               /* 0x24 somme, réécrite */
    pose32(t, 2);                               /* 0x28 le nombre, une 2e fois */
    pose_zeros(t, 4);                           /* 0x2C */
    unsigned long somme = 0;                    /* 0x30 références de pages */
    pose32(t, (unsigned long)ID_PAGE);
    pose16(t, 2);
    somme = (somme + (unsigned long)ID_PAGE) & 0xFFFFFFFFUL;
    somme = rot3(somme);
    somme = (somme + 2) & 0xFFFFFFFFUL;
    ecris32(t, ou_somme, somme);
    while ((t->n - deb) % 32) pose8(t, 0);
    ecris32(t, deb, (unsigned long)(t->n - deb));
}

static void pose_tail(Tampon *t)
{
    size_t deb = t->n;
    pose32(t, 0);
    for (const char *s = "TAIL"; *s; s++) pose8(t, (unsigned char)*s);
    pose32(t, 0xFFFFFFFFUL);
    pose32(t, 0);
    pose8(t, 15); for (const char *s = "Nu \x8Ar det slut"; *s; s++) pose8(t, (unsigned char)*s);
    while ((t->n - deb) % 32) pose8(t, 0);
    ecris32(t, deb, (unsigned long)(t->n - deb));
}

static Tampon monte_la_pile(unsigned long format, unsigned protection)
{
    Tampon t = {0,0,0};
    pose_stak(&t, format, 1, 2, protection);
    pose_ftbl(&t);
    pose_couche(&t, "BKGD", 2000, 0, 0, 1, 0, "Fond", SCRIPT_FOND);
    pose_couche(&t, "CARD", 3000, 2000, 1, 1, 1, "Atelier", SCRIPT_CARTE);
    pose_couche(&t, "CARD", 3001, 2000, 0, 0, 0, "", NULL);
    pose_liste(&t);
    pose_page(&t);
    pose_tail(&t);
    return t;
}

/* ------------------------------------------------------------------ */
/* Ce qu'on affiche                                                    */
/* ------------------------------------------------------------------ */

static void dis_script(const char *quoi, const char *s)
{
    if (!s) { printf("  %s : aucun\n", quoi); return; }
    printf("  %s :\n", quoi);
    const char *d = s;
    while (*d) {
        const char *f = strchr(d, '\n');
        int n = f ? (int)(f - d) : (int)strlen(d);
        printf("    | %.*s\n", n, d);
        if (!f) break;
        d = f + 1;
    }
}

static void dis_couche(const char *quoi, const HcOrigCouche *k)
{
    printf("%s id %d", quoi, k->id);
    if (k->fond) printf(", fond %d", k->fond);
    if (k->debut_de_fond) printf(", DEBUT DE FOND");
    if (k->marque) printf(", MARQUEE");
    printf(", nom « %s », %d part%s\n", k->nom ? k->nom : "(nul)",
           k->nparts, k->nparts == 1 ? "" : "s");
    dis_script("script", k->script);
    for (int i = 0; i < k->nparts; i++) {
        const HcOrigPart *p = &k->parts[i];
        printf("  %s id %d « %s » (%d,%d,%d,%d) %s\n",
               p->genre == HC_ORIG_BOUTON ? "bouton" : "champ ",
               p->id, p->nom ? p->nom : "(nul)", p->haut, p->gauche, p->bas, p->droite,
               p->style);
        /* TOUS LES DRAPEAUX SONT ÉCRITS, y compris les faux : c'est ce qui fait
         * qu'une inversion oubliée se voit dans le diff de la référence. Un
         * affichage qui ne montrerait que les vrais laisserait passer un bit
         * inversé dans le mauvais sens sur une part qui ne le porte pas. */
        printf("    %s  police %s corps %d style %d hauteur %d alignement %d famille %d\n",
               p->visible ? "visible" : "INVISIBLE",
               p->police ? p->police : "(aucune)",
               p->textsize, p->textstyle, p->textheight, p->text_align, p->family);
        printf("    dontWrap %d dontSearch %d sharedText %d fixedLineHeight %d autoTab %d\n",
               p->dont_wrap, p->dont_search, p->shared_text, p->fixed_lh, p->auto_tab);
        if (p->genre == HC_ORIG_BOUTON)
            printf("    bouton : enabled %d showName %d hilite %d autoHilite %d "
                   "sharedHilite %d largeurTitre %d icone %d\n",
                   p->enabled, p->showname, p->hilite, p->autohilite,
                   p->shared_hilite, p->titlewidth, p->icon);
        else
            printf("    champ : lockText %d showLines %d wideMargins %d "
                   "multipleLines %d autoSelect %d lignes %d..%d\n",
                   p->locktext, p->show_lines, p->wide_margins,
                   p->multiple_lines, p->auto_select,
                   p->premiere_ligne, p->derniere_ligne);
        dis_script("  script", p->script);
    }
    /* LE SIGNE DE L'IDENTIFIANT EST LA CHOSE À VOIR : « propre » contre
     * « du fond », c'est-à-dire le texte de la part de cette couche contre le
     * texte qu'une CARTE porte pour un champ du FOND. */
    for (int i = 0; i < k->ncontenus; i++)
        printf("  contenu %s part %d : « %s »%s\n",
               k->contenus[i].du_fond ? "DU FOND," : "propre,  ",
               k->contenus[i].id_part, k->contenus[i].texte,
               k->contenus[i].decore ? "   [décoré : styles non lus]" : "");
}

static void lis_et_dis(const char *titre, const unsigned char *o, size_t n, int tout)
{
    HcOrigPile pile;
    char pourquoi[160];
    int r = hc_origine_lit(o, n, &pile, pourquoi, sizeof pourquoi);
    printf("--- %s : ", titre);
    if (r != 0) { printf("REFUSE — %s\n", pourquoi); hc_origine_libere(&pile); return; }
    printf("lu\n");

    printf("ordre : %s, %d page(s)\n",
           pile.ordre_lu ? "LU ET VERIFIE (sommes de la liste et des pages)"
                         : "NON LU — les cartes restent dans l'ordre du FICHIER",
           pile.npages);
    printf("format %lu, %lu fond(s), %lu carte(s), %dx%d, somme %s, %d bloc%s, "
           "chaine %s la fin, TAIL %s, LIST %s, %d anomalie(s), "
           "%d police(s), %d contenu(s) décoré(s)\n",
           pile.format, pile.nfonds, pile.ncartes, pile.largeur, pile.hauteur,
           pile.somme_juste ? "juste" : "FAUSSE",
           pile.nblocs, pile.nblocs == 1 ? "" : "s",
           pile.chaine_atteint_la_fin ? "atteint" : "N'ATTEINT PAS",
           pile.tail_vu ? "vu" : "absent",
           pile.liste_vue ? "vu" : "ABSENT (l'ordre des cartes n'est donc pas connu)",
           pile.anomalies, pile.npolices, pile.contenus_decores);

    if (tout) {
        for (int i = 0; i < pile.nblocs; i++)
            printf("  bloc %-4s id %6d taille %5lu offset 0x%04lX\n",
                   pile.blocs[i].type, pile.blocs[i].id,
                   pile.blocs[i].taille, pile.blocs[i].offset);
        dis_script("script de la pile", pile.script);
        for (int i = 0; i < pile.nfonds_lus; i++)  dis_couche("fond ", &pile.fonds[i]);
        for (int i = 0; i < pile.ncartes_lues; i++) dis_couche("carte", &pile.cartes[i]);
    }
    hc_origine_libere(&pile);
}

/* ------------------------------------------------------------------ */

static void la_table(void)
{
    puts("=== MacRoman -> UTF-8, les 256 octets ===");
    puts("(comparés un par un au codec « mac_roman » de Python : voir");
    puts(" docs/mesures/pile_origine.txt. Cette référence les fige.)");
    for (int base = 0; base < 256; base += 8) {
        printf(" ");
        for (int b = base; b < base + 8; b++) {
            unsigned char o = (unsigned char)b;
            char *u = hc_origine_utf8(&o, 1);
            printf("  %02X=", b);
            if (!u) { printf("nul"); continue; }
            for (unsigned char *w = (unsigned char *)u; *w; w++) printf("%02X", *w);
            if (!*u) printf("--");     /* \r rendu \n : jamais vide, mais on le dit */
            free(u);
        }
        printf("\n");
    }
    /* Les fins de ligne, à part : ce sont les seuls octets dont la conversion
     * n'est pas un caractère mais une RÈGLE. */
    char *a = hc_origine_utf8((const unsigned char *)"a\rb", 3);
    char *c = hc_origine_utf8((const unsigned char *)"a\r\nb", 4);
    printf(" « a\\rb » -> « %s » (%d octets)\n", a ? a : "nul", a ? (int)strlen(a) : -1);
    printf(" « a\\r\\nb » -> « %s » (%d octets)\n", c ? c : "nul", c ? (int)strlen(c) : -1);
    free(a); free(c);
}

/* ------------------------------------------------------------------ */
/* LE TOUR COMPLET, QUI S'ARBITRE LUI-MÊME                             */
/* ------------------------------------------------------------------ */

/* Lire, convertir, sauver dans NOTRE format, relire, et comparer à ce que le
 * lecteur d'origine avait annoncé.
 *
 * C'EST LE SEUL ORACLE QUI NE DEMANDE NI HYPERCARD NI PERSONNE. Toute propriété
 * qui se perd en chemin — dans la traduction, dans l'écriture de notre format ou
 * dans sa relecture — se dénonce ici, à chaque passage de la suite. Une
 * propriété traduite et jamais écrite est exactement le genre de manque qui ne
 * se voit pas : la pile s'ouvre, elle a l'air juste, et un champ n'a pas ses
 * marges.
 *
 * LES PARTS SE COMPARENT PAR POSITION, ET C'EST UN AVEU. Les identifiants que
 * hc_new_field et hc_new_button attribuent sont les NÔTRES : l'identifiant
 * d'origine est perdu. Un script qui dit « card field id 5 » ne retrouvera donc
 * pas son champ. C'est noté dans docs/mesures/pile_origine.txt comme le
 * prochain point à régler ; ici on compare dans l'ordre de création, qui est
 * celui du fichier d'origine. */
static int compares, perdus, orphelins;

static void egal_int(const char *quoi, int attendu, int obtenu)
{
    compares++;
    if (attendu == obtenu) return;
    perdus++;
    printf("  PERDU  %-34s attendu %d, obtenu %d\n", quoi, attendu, obtenu);
}

static void egal_txt(const char *quoi, const char *attendu, const char *obtenu)
{
    compares++;
    if (!attendu) attendu = "";
    if (!obtenu)  obtenu  = "";
    if (!strcmp(attendu, obtenu)) return;
    perdus++;
    printf("  PERDU  %-34s attendu « %s », obtenu « %s »\n", quoi, attendu, obtenu);
}

/* LA NORMALISATION DE hc_set_script, RÉÉCRITE ICI EXPRÈS.
 *
 * hc_set_script ne garde pas le script tel quel : dup_script (hc_script.c) y
 * remplace « ≠ ≤ ≥ » par « <> <= >= » et traite « ¬ » en fin de ligne comme une
 * continuation, la ligne suivante étant aboutée après un espace. C'est voulu et
 * documenté ; comparer le texte BRUT au texte stocké faisait donc apparaître des
 * pertes qui n'en sont pas.
 *
 * La règle est réécrite ici plutôt qu'appelée : dup_script est statique, et même
 * s'il ne l'était pas, comparer une transformation à elle-même ne testerait
 * rien. Deux expressions indépendantes de la même règle, c'est ce qui fait un
 * test — et celui-ci n'existait pas.
 *
 * Rend une chaîne à libérer. Les formes UTF-8 sont celles que hc_origine_utf8
 * produit depuis MacRoman. */
static char *comme_hc(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = malloc(2 * n + 2);
    if (!d) return NULL;
    char *w = d;
    for (const char *p = s; *p; ) {
        if (!memcmp(p, "\xE2\x89\xA0", 3)) { *w++='<'; *w++='>'; p += 3; continue; }  /* ≠ */
        if (!memcmp(p, "\xE2\x89\xA4", 3)) { *w++='<'; *w++='='; p += 3; continue; }  /* ≤ */
        if (!memcmp(p, "\xE2\x89\xA5", 3)) { *w++='>'; *w++='='; p += 3; continue; }  /* ≥ */
        if (!memcmp(p, "\xC2\xAC", 2)) {                                            /* ¬ */
            const char *q = p + 2;
            while (*q == ' ' || *q == '\t') q++;
            /* UNE ESPACE, ET L'INDENTATION DE LA LIGNE SUIVANTE RESTE.
             *
             * J'avais écrit le contraire — les blancs de tête de la ligne
             * suivante avalés eux aussi — et le tour complet l'a dit aussitôt :
             * « put "une ligne"  & … » contre « put "une ligne"       & … ».
             * dup_script (hc_script.c) n'avale que les blancs AVANT la fin de
             * ligne, puis la fin de ligne, et pose une espace. C'est la règle du
             * noyau qui a corrigé ma réécriture, et c'est exactement ce qu'on
             * attend de deux expressions indépendantes de la même règle. */
            if (*q == '\r' || *q == '\n') {
                if (*q == '\r' && q[1] == '\n') q++;
                q++;
                *w++ = ' ';
                p = q;
                continue;
            }
            *w++ = p[0]; *w++ = p[1]; p += 2; continue;
        }
        *w++ = *p++;
    }
    *w = '\0';
    return d;
}

static void egal_script(const char *quoi, const char *attendu, const char *obtenu)
{
    char *norme = comme_hc(attendu);
    egal_txt(quoi, norme ? norme : attendu, obtenu);
    free(norme);
}

static void compare_part(const char *ou, const HcOrigPart *q, Object *o)
{
    char quoi[128];
    #define Q(champ) (snprintf(quoi, sizeof quoi, "%s %s", ou, champ), quoi)
    egal_int(Q("genre"), q->genre == HC_ORIG_BOUTON ? OBJ_BUTTON : OBJ_FIELD, (int)o->type);
    egal_txt(Q("nom"), q->nom, o->name);
    egal_int(Q("x"), q->gauche, o->x);
    egal_int(Q("y"), q->haut, o->y);
    egal_int(Q("largeur"), q->droite - q->gauche, o->w);
    egal_int(Q("hauteur"), q->bas - q->haut, o->h);
    egal_txt(Q("style"), q->style, o->style);
    egal_script(Q("script"), q->script, o->script);
    egal_int(Q("visible"), q->visible, o->visible);
    egal_int(Q("dontWrap"), q->dont_wrap, o->dont_wrap);
    egal_int(Q("dontSearch"), q->dont_search, o->dont_search);
    egal_int(Q("sharedText"), q->shared_text, o->shared_text);
    egal_int(Q("fixedLineHeight"), q->fixed_lh, o->fixed_lh);
    egal_int(Q("autoTab"), q->auto_tab, o->auto_tab);
    egal_int(Q("famille"), q->family, o->family);
    /* LA RÈGLE EST RÉÉCRITE ICI, EXPRÈS, et non appelée depuis hc_importe.c :
     * partager la fonction ferait qu'une traduction fausse serait comparée à
     * elle-même et passerait. Deux expressions indépendantes de la même règle,
     * c'est ce qui donne un test. */
    egal_int(Q("alignement"),
             q->text_align == 1 ? 1 : (q->text_align == -1 ? 2 : 0),
             o->text_align);
    egal_int(Q("corps"), q->textsize, o->textsize);
    egal_int(Q("styleDuTexte"), q->textstyle, o->textstyle);
    egal_int(Q("hauteurDeLigne"), q->textheight, o->textheight);
    egal_txt(Q("police"), q->police, o->textfont);
    if (q->genre == HC_ORIG_BOUTON) {
        egal_int(Q("enabled"), q->enabled, o->enabled);
        egal_int(Q("showName"), q->showname, o->showname);
        egal_int(Q("hilite"), q->hilite, o->hilite);
        egal_int(Q("autoHilite"), q->autohilite, o->autohilite);
        egal_int(Q("sharedHilite"), q->shared_hilite, o->shared_hilite);
        egal_int(Q("largeurTitre"), q->titlewidth, o->titlewidth);
        egal_int(Q("icone"), q->icon, o->icon);
    } else {
        egal_int(Q("lockText"), q->locktext, o->locktext);
        egal_int(Q("showLines"), q->show_lines, o->show_lines);
        egal_int(Q("wideMargins"), q->wide_margins, o->wide_margins);
        egal_int(Q("multipleLines"), q->multiple_lines, o->multiple_lines);
        egal_int(Q("autoSelect"), q->auto_select, o->auto_select);
    }
    #undef Q
}

static Object *nieme(Object *couche, int rang)
{
    return (couche && rang < couche->nparts) ? couche->parts[rang] : NULL;
}

/* LA PART VISÉE PAR UN CONTENU, par POSITION et non par identifiant — les
 * identifiants de nos parts sont les nôtres. Réécrit ici plutôt qu'appelé depuis
 * hc_importe.c, pour la même raison que la normalisation des scripts. */
static Object *visee(Object *couche, const HcOrigCouche *k, int id_origine)
{
    if (!couche || !k) return NULL;
    for (int j = 0; j < k->nparts && j < couche->nparts; j++)
        if (k->parts[j].id == id_origine) return couche->parts[j];
    return NULL;
}

/* LE TEXTE DES CHAMPS, ET C'EST LE TROU QUI A LAISSÉ PASSER UN VRAI DÉFAUT.
 *
 * Le tour complet comparait les propriétés et les scripts — 3711 valeurs sur
 * trois vraies piles — et JAMAIS le texte. Pendant ce temps hc_importe.c
 * appariait les contenus par identifiant, donc n'en trouvait aucun, et le
 * fichier converti ne portait pas une seule ligne « contents » : tous les champs
 * de toutes les piles importées arrivaient vides. C'est l'autrice qui l'a vu,
 * dans l'application.
 *
 * Un instrument qui compare beaucoup de choses n'est pas un instrument qui
 * compare les bonnes. Celui-ci vérifie désormais ce qu'un utilisateur REGARDE.
 *
 * La carte courante compte : le texte d'un champ de fond non partagé vit dans
 * les bgtexts de la CARTE, et hc_field_text le cherche là. */
static void compare_textes(Object *couche, const HcOrigCouche *k,
                           Object *fond, const HcOrigCouche *kfond,
                           int couche_est_fond, const char *ou)
{
    for (int i = 0; i < k->ncontenus; i++) {
        const HcOrigContenu *ct = &k->contenus[i];
        Object *p = (couche_est_fond || !ct->du_fond)
                  ? visee(couche, k, ct->id_part)
                  : visee(fond, kfond, ct->id_part);
        char quoi[120];
        snprintf(quoi, sizeof quoi, "%s texte %s part %d", ou,
                 ct->du_fond ? "du fond," : "propre, ", ct->id_part);
        /* UN CONTENU ORPHELIN N'EST PAS UNE PERTE, et la nuance a été mesurée.
         *
         * « 3D Parametric Equations » porte cinq textes dont la part n'existe
         * pas dans la couche qui les porte : la part 25 y est réclamée par des
         * cartes du fond 2815, où elle n'existe pas — elle est dans le fond
         * 7925. Un texte laissé derrière par une part supprimée, ou par une
         * carte qui a changé de fond. HyperCard en laisse, et lui aussi les
         * ignore : il n'y a aucune part pour les afficher.
         *
         * On les COMPTE donc, séparément, plutôt que de les appeler pertes — il
         * n'y a rien à perdre — et séparément plutôt que de les taire, parce
         * qu'un jour la cause pourrait être un vrai défaut d'appariement. */
        if (!p) {
            printf("  orphelin  %s : aucune part de ce rang\n", quoi);
            orphelins++;
            continue;
        }
        egal_txt(quoi, ct->texte, hc_field_text(p));
    }
}

static void le_tour_complet(const unsigned char *octets, size_t n)
{
    puts("=== le tour complet : lire, convertir, sauver, relire, comparer ===");
    puts("(le seul oracle qui ne demande ni HyperCard ni personne : toute");
    puts(" propriété qui se perd en chemin se dénonce ici)");

    HcOrigPile pile;
    char pourquoi[160];
    if (hc_origine_lit(octets, n, &pile, pourquoi, sizeof pourquoi) != 0) {
        printf("la lecture a refusé : %s\n", pourquoi);
        return;
    }

    Object *st = hc_importe_pile(&pile, "Tour");
    if (!st) { puts("la conversion a échoué"); hc_origine_libere(&pile); return; }

    const char *chemin = "/tmp/hc_tour.stack";
    int r = hc_save(st, chemin);
    printf("hc_save : %s\n", r == 0 ? "écrit" : "ÉCHEC");
    hc_free(st);

    Object *st2 = hc_load(chemin);
    if (!st2) {
        printf("hc_load a refusé : %s\n", hc_load_erreur() ? hc_load_erreur() : "(sans raison)");
        remove(chemin); hc_origine_libere(&pile); return;
    }

    compares = perdus = orphelins = 0;
    egal_int("largeur de la pile", pile.largeur, st2->w);
    egal_int("hauteur de la pile", pile.hauteur, st2->h);
    egal_script("script de la pile", pile.script, st2->script);

    /* Les fonds et les cartes, dans l'ordre de leur création — celui du
     * fichier d'origine pour les fonds, celui de la LISTE pour les cartes. */
    int ifond = 0, icarte = 0;
    for (int i = 0; i < st2->nparts; i++) {
        Object *o = st2->parts[i];
        char ou[64];
        if (o->type == OBJ_BACKGROUND && ifond < pile.nfonds_lus) {
            const HcOrigCouche *k = &pile.fonds[ifond];
            snprintf(ou, sizeof ou, "fond %d", ifond);
            egal_txt("nom du fond", k->nom, o->name);
            egal_script("script du fond", k->script, o->script);
            egal_int("dontSearch du fond", k->dont_search, o->dont_search);
            for (int j = 0; j < k->nparts; j++) {
                char q[80]; snprintf(q, sizeof q, "%s part %d", ou, j);
                Object *p = nieme(o, j);
                if (!p) { printf("  PERDU  %s : absente\n", q); perdus++; compares++; continue; }
                compare_part(q, &k->parts[j], p);
            }
            /* Le texte par DÉFAUT d'un champ de fond : sans carte courante. */
            hc_set_current_card(NULL);
            compare_textes(o, k, NULL, NULL, 1, ou);
            ifond++;
        } else if (o->type == OBJ_CARD && icarte < pile.ncartes_lues) {
            const HcOrigCouche *k = &pile.cartes[icarte];
            snprintf(ou, sizeof ou, "carte %d", icarte);
            egal_txt("nom de la carte", k->nom, o->name);
            egal_script("script de la carte", k->script, o->script);
            egal_int("marked de la carte", k->marque, o->marked);
            for (int j = 0; j < k->nparts; j++) {
                char q[80]; snprintf(q, sizeof q, "%s part %d", ou, j);
                Object *p = nieme(o, j);
                if (!p) { printf("  PERDU  %s : absente\n", q); perdus++; compares++; continue; }
                compare_part(q, &k->parts[j], p);
            }
            /* Le texte de CETTE carte, y compris pour les champs du fond.
             *
             * ET IL FAUT LE BON FOND, pas le premier. J'avais pris le premier
             * OBJ_BACKGROUND venu, ce qui marche par accident tant qu'il n'y en a
             * qu'un — la pile de ce harnais — et donne 78 fausses pertes sur une
             * pile à trois fonds. Quatrième fois que mon instrument mesure autre
             * chose que ce qu'il annonce ; le convertisseur, lui, apparie par
             * identifiant et avait raison.
             *
             * Les fonds sont créés dans l'ordre de `orig->fonds`, donc le j-ième
             * OBJ_BACKGROUND de la pile relue est le j-ième du fichier. */
            Object *bg = NULL; const HcOrigCouche *kbg = NULL;
            for (int j = 0; j < pile.nfonds_lus; j++)
                if (pile.fonds[j].id == k->fond) {
                    kbg = &pile.fonds[j];
                    int vus = 0;
                    for (int q2 = 0; q2 < st2->nparts; q2++)
                        if (st2->parts[q2]->type == OBJ_BACKGROUND && vus++ == j) {
                            bg = st2->parts[q2]; break;
                        }
                    break;
                }
            hc_set_current_card(o);
            compare_textes(o, k, bg, kbg, 0, ou);
            icarte++;
        }
    }

    printf("%d valeurs comparées, %d perdue%s, %d contenu%s orphelin%s\n",
           compares, perdus, perdus == 1 ? "" : "s",
           orphelins, orphelins == 1 ? "" : "s", orphelins == 1 ? "" : "s");

    hc_free(st2);
    remove(chemin);
    hc_origine_libere(&pile);
}

int main(int argc, char **argv)
{
    la_table();
    puts("");

    Tampon t = monte_la_pile(10, 0);
    puts("=== la pile montée au format d'origine ===");
    printf("%lu octets\n", (unsigned long)t.n);
    lis_et_dis("pile HyperCard 2.x", t.o, t.n, 1);

    /* Le fichier s'écrit si on donne un chemin : c'est ce qu'un lecteur TIERS
     * relira, et son accord est le seul contrepoids à « mon écrivain et mon
     * lecteur partagent la même croyance ». */
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        if (!f) { fprintf(stderr, "ecriture impossible : %s\n", argv[1]); return 1; }
        fwrite(t.o, 1, t.n, f);
        fclose(f);
        fprintf(stderr, "ecrit : %s (%lu octets)\n", argv[1], (unsigned long)t.n);
    }

    puts("");
    le_tour_complet(t.o, t.n);

    puts("");
    puts("=== les témoins négatifs : ce qui doit être REFUSÉ ===");

    /* Chacun part de la pile SAINE et n'en abîme qu'une chose : c'est ce qui
     * rend le refus attribuable. Un témoin qui casse deux choses à la fois ne
     * dit pas laquelle a mordu. */
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        c[0] = c[1] = c[2] = c[3] = 0;                   /* taille de bloc nulle */
        lis_et_dis("taille du premier bloc nulle", c, t.n, 0);
        free(c);
    }
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        c[0] = 0x7F;                                     /* taille au-dela de la fin */
        lis_et_dis("taille de bloc au-dela du fichier", c, t.n, 0);
        free(c);
    }
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        memcpy(c + 4, "ZORG", 4);                        /* premier bloc pas STAK */
        lis_et_dis("premier bloc pas STAK", c, t.n, 0);
        free(c);
    }
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        c[0x4C] = 0x20;                                  /* bit 13 : acces prive */
        lis_et_dis("pile a acces prive", c, t.n, 0);
        free(c);
    }
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        c[0x13] = 8;                                     /* format 8 : HyperCard 1.x */
        lis_et_dis("pile HyperCard 1.x", c, t.n, 0);
        free(c);
    }
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        c[0x2E] = 0x10;                                  /* 4096 cartes annoncees */
        lis_et_dis("plus de cartes que de blocs", c, t.n, 0);
        free(c);
    }
    /* LA TAILLE DE LA LISTE DES PARTS, abîmée d'un seul octet. C'est le
     * recoupement que les deux lecteurs publics ne font pas : sans lui, ce
     * fichier se lirait, et rendrait un nom de carte pris un octet à côté. */
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        unsigned long off = 0;
        for (size_t i = 0; i + 8 <= t.n; ) {
            unsigned long taille = ((unsigned long)c[i] << 24) | ((unsigned long)c[i+1] << 16)
                                 | ((unsigned long)c[i+2] << 8) | c[i+3];
            if (memcmp(c + i + 4, "CARD", 4) == 0) { off = (unsigned long)i; break; }
            if (taille < 16) break;
            i += taille;
        }
        if (off) { c[off + 0x2F] = (unsigned char)(c[off + 0x2F] + 1); }
        lis_et_dis("taille de la liste des parts fausse d'un octet", c, t.n, 0);
        free(c);
    }
    /* L'ORDRE SE REFUSE SANS QUE LE FICHIER SE REFUSE, et c'est le point.
     *
     * Perdre les scripts d'une pile parce que la somme d'une page est fausse
     * serait un mauvais échange : l'ordre n'est pas nécessaire pour lire un
     * script en sûreté. Mais se rabattre en SILENCE sur l'ordre du fichier
     * serait malhonnête. Donc « ordre NON LU », une anomalie comptée, et les
     * cartes reviennent dans l'ordre du fichier — Atelier d'abord, et non
     * l'inverse comme la liste le voulait. La référence montre les deux ordres,
     * c'est ce qui rend ces témoins lisibles. */
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        for (size_t i = 0; i + 8 <= t.n; ) {
            unsigned long taille = ((unsigned long)c[i] << 24) | ((unsigned long)c[i+1] << 16)
                                 | ((unsigned long)c[i+2] << 8) | c[i+3];
            if (memcmp(c + i + 4, "LIST", 4) == 0) { c[i + 0x27] ^= 0x01; break; }
            if (taille < 16) break;
            i += taille;
        }
        lis_et_dis("somme de la LISTE fausse d'un bit", c, t.n, 1);
        free(c);
    }
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        for (size_t i = 0; i + 8 <= t.n; ) {
            unsigned long taille = ((unsigned long)c[i] << 24) | ((unsigned long)c[i+1] << 16)
                                 | ((unsigned long)c[i+2] << 8) | c[i+3];
            if (memcmp(c + i + 4, "PAGE", 4) == 0) { c[i + 0x17] ^= 0x01; break; }
            if (taille < 16) break;
            i += taille;
        }
        lis_et_dis("somme d'une PAGE fausse d'un bit", c, t.n, 1);
        free(c);
    }
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        for (size_t i = 0; i + 8 <= t.n; ) {
            unsigned long taille = ((unsigned long)c[i] << 24) | ((unsigned long)c[i+1] << 16)
                                 | ((unsigned long)c[i+2] << 8) | c[i+3];
            if (memcmp(c + i + 4, "PAGE", 4) == 0) { memcpy(c + i + 4, "ZORG", 4); break; }
            if (taille < 16) break;
            i += taille;
        }
        lis_et_dis("la PAGE que la liste nomme est introuvable", c, t.n, 1);
        free(c);
    }
    /* Le nombre total de cartes est écrit DEUX FOIS dans la liste, à 0x18 et à
     * 0x28. Les désaccorder doit se voir : c'est un recoupement que le format
     * nous offre et que les deux lecteurs publics ne font pas. */
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        for (size_t i = 0; i + 8 <= t.n; ) {
            unsigned long taille = ((unsigned long)c[i] << 24) | ((unsigned long)c[i+1] << 16)
                                 | ((unsigned long)c[i+2] << 8) | c[i+3];
            if (memcmp(c + i + 4, "LIST", 4) == 0) { c[i + 0x2B] = 9; break; }
            if (taille < 16) break;
            i += taille;
        }
        lis_et_dis("les deux nombres de cartes de la liste se contredisent", c, t.n, 1);
        free(c);
    }

    /* UN STYLE QUI NE VA PAS AU GENRE, et c'est le seul recoupement qu'on ait
     * sur l'octet de style — aucun lecteur tiers ne lit les propriétés. Un
     * « scrolling » n'existe que pour un champ, un « checkbox » que pour un
     * bouton : en trouver un du mauvais côté voudrait dire qu'on lit le mauvais
     * octet. Compté, jamais fatal.
     *
     * Vérifié sur les trois vraies piles avant d'y croire : sur environ 180
     * parts, checkbox, radio, standard et shadow n'apparaissent QUE sur des
     * boutons, et scrolling QUE sur des champs. Pas une violation. */
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        /* le champ « A » porte le style 7 (scrolling) : on le passe à 5
         * (checkbox), qui n'a pas de sens pour un champ */
        for (size_t i = 0; i + 8 < t.n; i++)
            if (memcmp(c + i, "A", 1) == 0 && c[i+1] == 0 && i > 0x1E && c[i - 0x1E + 0x0F] == 7) {
                c[i - 0x1E + 0x0F] = 5; break;
            }
        lis_et_dis("un « checkbox » sur un CHAMP (le style ne va pas au genre)", c, t.n, 1);
        free(c);
    }

    /* Une faute LOCALE, qui ne doit PAS faire refuser le fichier : l'octet nul
     * qui précède le script d'une part. On perd ce script, on compte
     * l'anomalie, et les autres scripts restent lisibles. */
    {
        unsigned char *c = malloc(t.n); memcpy(c, t.o, t.n);
        /* le bouton « TORTURE » de la carte : son nom, puis le zero */
        for (size_t i = 0; i + 8 < t.n; i++)
            if (memcmp(c + i, "TORTURE", 7) == 0 && c[i+7] == 0 && c[i+8] == 0) { c[i+8] = 'X'; break; }
        lis_et_dis("marqueur de script d'une part abime (faute LOCALE)", c, t.n, 1);
        free(c);
    }

    free(t.o);
    return 0;
}
