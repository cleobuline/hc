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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Un écrivain, uniquement pour le test                                */
/* ------------------------------------------------------------------ */

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
 * et le script. La taille se réécrit à la fin, quand on la connaît. */
static void pose_part(Tampon *t, int id, int bouton, int h, int g, int b, int d,
                      const char *nom, const char *script)
{
    size_t deb = t->n;
    pose16(t, 0);                               /* taille, réécrite plus bas */
    pose16(t, (unsigned)id);
    pose16(t, bouton ? 0x0100u : 0x0000u);      /* bit 8 : bouton */
    pose16(t, (unsigned)h); pose16(t, (unsigned)g);
    pose16(t, (unsigned)b); pose16(t, (unsigned)d);
    pose_zeros(t, 0x1E - 0x0E);                 /* styles, polices, hauteur de ligne */
    pose_texte(t, nom);
    if (script) { pose8(t, 0); pose_texte(t, script); }
    if ((t->n - deb) % 2) pose8(t, 0);          /* calage sur 16 bits */
    size_t taille = t->n - deb;
    t->o[deb] = (unsigned char)((taille >> 8) & 0xFF);
    t->o[deb+1] = (unsigned char)(taille & 0xFF);
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

static const char SCRIPT_BOUTON[] = "on mouseUp\r  beep\rend mouseUp";
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
    pose_zeros(t, 0x4C - 0x30);
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
    unsigned ncont  = (unsigned)avec_contenu;

    /* Les parts et les contenus se montent à part, pour connaître leur taille
     * totale avant d'écrire les en-têtes qui l'annoncent. */
    Tampon parts = {0,0,0}, conts = {0,0,0};
    if (avec_champ)  pose_part(&parts, 1, 0, 54, 20, 80, 250, "A", NULL);
    if (avec_bouton) pose_part(&parts, 2, 1, 302, 20, 324, 110, "TORTURE", SCRIPT_BOUTON);
    if (avec_contenu) pose_contenu(&conts, -1, "du texte");

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
    pose_couche(&t, "BKGD", 2000, 0, 0, 1, 0, "Fond", SCRIPT_FOND);
    pose_couche(&t, "CARD", 3000, 2000, 1, 1, 1, "Atelier", SCRIPT_CARTE);
    pose_couche(&t, "CARD", 3001, 2000, 0, 0, 0, "", NULL);
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
    printf(", nom « %s », %d part%s\n", k->nom ? k->nom : "(nul)",
           k->nparts, k->nparts == 1 ? "" : "s");
    dis_script("script", k->script);
    for (int i = 0; i < k->nparts; i++) {
        const HcOrigPart *p = &k->parts[i];
        printf("  %s id %d « %s » (%d,%d,%d,%d)\n",
               p->genre == HC_ORIG_BOUTON ? "bouton" : "champ ",
               p->id, p->nom ? p->nom : "(nul)", p->haut, p->gauche, p->bas, p->droite);
        dis_script("  script", p->script);
    }
}

static void lis_et_dis(const char *titre, const unsigned char *o, size_t n, int tout)
{
    HcOrigPile pile;
    char pourquoi[160];
    int r = hc_origine_lit(o, n, &pile, pourquoi, sizeof pourquoi);
    printf("--- %s : ", titre);
    if (r != 0) { printf("REFUSE — %s\n", pourquoi); hc_origine_libere(&pile); return; }
    printf("lu\n");

    printf("format %lu, %lu fond(s), %lu carte(s), %dx%d, somme %s, %d bloc%s, "
           "chaine %s la fin, TAIL %s, LIST %s, %d anomalie(s)\n",
           pile.format, pile.nfonds, pile.ncartes, pile.largeur, pile.hauteur,
           pile.somme_juste ? "juste" : "FAUSSE",
           pile.nblocs, pile.nblocs == 1 ? "" : "s",
           pile.chaine_atteint_la_fin ? "atteint" : "N'ATTEINT PAS",
           pile.tail_vu ? "vu" : "absent",
           pile.liste_vue ? "vu" : "ABSENT (l'ordre des cartes n'est donc pas connu)",
           pile.anomalies);

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
