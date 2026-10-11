/* hypertexte — UNE PILE DE LIENS « GROUP », MONTÉE PAR LE CODE, pour le DMG.
 *
 * Demandée par l'utilisatrice le 11 octobre, le jour du menu Style : une
 * petite pile qui présente HC et montre l'hypertexte d'HyperCard, des mots au
 * style Group et un script de champ qui suit le clic — exactement comme les
 * piles d'aide d'Apple.
 *
 * Bilingue : sept cartes en français, puis les mêmes en anglais, jumelles
 * deux à deux. Le texte des cartes est dans tests/donnees/hypertexte_cartes.txt,
 * le script de la pile dans tests/donnees/hypertexte_pile.txt.
 *
 * DEUX USAGES POUR UN SEUL PROGRAMME, comme le pendu :
 *
 *   hypertexte pile.txt cartes.txt                    le test
 *   hypertexte pile.txt cartes.txt Hypertexte.stack   écrit la pile, pour le DMG
 *
 * La pile écrite n'est pas versionnée — ses sources le sont (docs/livraison.md).
 *
 * LE TEST CLIQUE CHAQUE LIEN POUR DE BON. Le faux hôte rend the clickChunk
 * et the clickText comme HCview.m après un clic sur un mot groupé, puis
 * mouseUp part au champ, dont le script doit mener à la carte qui porte le
 * texte du lien. Un lien qui ne mène nulle part, ou ailleurs, se voit.
 *
 * NON VÉRIFIÉ ICI : la mise en page. Le nombre de lignes d'un champ est
 * ESTIMÉ (une ligne pour 62 caractères dans un champ large, Geneva 12), pour
 * décider s'il défile ; c'est l'écran de l'application qui tranche. */
#include "hc_core.h"
#include "hc_file.h"
#include "hct_chunk.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("   [msg] %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

/* ---- le faux hôte : ce que HCview.m répond après un clic ---- */

static char g_chunk[192] = "";
static char g_clic[512]  = "";
static int  g_groupes    = -1;          /* dernier « show / hide groups » */

static const char *glob_lit(const char *n)
{
    if (!strcasecmp(n, "clickChunk")) return g_chunk;
    if (!strcasecmp(n, "clickText"))  return g_clic;
    return NULL;
}

static void glob_pose(const char *n, const char *v)
{
    if (!strcasecmp(n, "showGroups"))
        g_groupes = (!strcasecmp(v, "true") || !strcmp(v, "1")) ? 1 : 0;
}

static char *lire(const char *chemin)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) { perror(chemin); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *t = malloc((size_t)n + 1);
    if (!t) { fclose(f); return NULL; }
    if (fread(t, 1, (size_t)n, f) != (size_t)n) { free(t); fclose(f); return NULL; }
    t[n] = 0; fclose(f);
    return t;
}

/* ---- le texte des cartes ---- */

#define MAX_CARTES 32
#define MAX_PLAGES 48

typedef struct { int deb, lon, code; } Plage;   /* en OCTETS, comme le noyau */

typedef struct {
    char   nom[64], titre[96];
    int    etoile;
    char  *brut;                 /* le texte tel qu'écrit, marques comprises */
    char  *texte;                /* le texte du champ, marques ôtées         */
    Plage  pl[MAX_PLAGES];
    int    npl;
} Carte;

static Carte g_c[MAX_CARTES];
static int   g_nc;

static int ajoute_plage(Carte *c, int deb, int lon, int code)
{
    if (c->npl >= MAX_PLAGES) {
        fprintf(stderr, "%s : trop de plages\n", c->nom);
        return 0;
    }
    c->pl[c->npl].deb = deb; c->pl[c->npl].lon = lon; c->pl[c->npl].code = code;
    c->npl++;
    return 1;
}

/* [[mots]] devient un lien, « >ligne » une ligne de code décalée de trois
 * espaces. Les lignes vides de la fin tombent. */
static int convertit(Carte *c)
{
    size_t cap = 2 * strlen(c->brut) + 16, n = 0;
    char *o = malloc(cap);
    if (!o) return 0;
    const char *q = c->brut;
    while (*q) {
        const char *fin = strchr(q, '\n');
        size_t l = fin ? (size_t)(fin - q) : strlen(q);
        if (*q == '>') {
            int deb = (int)n;
            memcpy(o + n, "   ", 3); n += 3;
            memcpy(o + n, q + 1, l - 1); n += l - 1;
            if (!ajoute_plage(c, deb, (int)n - deb, 1)) { free(o); return 0; }
        } else {
            size_t i = 0;
            while (i < l) {
                if (i + 1 < l && q[i] == '[' && q[i + 1] == '[') {
                    const char *f = strstr(q + i + 2, "]]");
                    if (!f || f > q + l) {
                        fprintf(stderr, "%s : « [[ » sans « ]] »\n", c->nom);
                        free(o); return 0;
                    }
                    size_t k = (size_t)(f - (q + i + 2));
                    int deb = (int)n;
                    memcpy(o + n, q + i + 2, k); n += k;
                    if (!ajoute_plage(c, deb, (int)k, 0)) { free(o); return 0; }
                    i = (size_t)(f + 2 - q);
                } else {
                    o[n++] = q[i++];
                }
            }
        }
        if (fin) o[n++] = '\n';
        q = fin ? fin + 1 : q + l;
    }
    while (n > 0 && o[n - 1] == '\n') n--;
    o[n] = 0;
    c->texte = o;
    return 1;
}

static int lit_cartes(const char *src)
{
    Carte *c = NULL;
    int corps = 0;
    size_t cap = 0, n = 0;
    const char *p = src;
    while (*p) {
        const char *fin = strchr(p, '\n');
        size_t l = fin ? (size_t)(fin - p) : strlen(p);
        char ligne[1024];
        snprintf(ligne, sizeof ligne, "%.*s", (int)l, p);
        p = fin ? fin + 1 : p + l;

        if (!strncmp(ligne, "=== ", 4)) {
            if (g_nc >= MAX_CARTES) { fprintf(stderr, "trop de cartes\n"); return 0; }
            c = &g_c[g_nc++];
            snprintf(c->nom, sizeof c->nom, "%s", ligne + 4);
            cap = 4096; n = 0;
            c->brut = malloc(cap);
            if (!c->brut) return 0;
            c->brut[0] = 0;
            corps = 0;
            continue;
        }
        if (!c) continue;                      /* l'en-tête du fichier */
        if (!corps) {
            if (!ligne[0])                          corps = 1;
            else if (!strncmp(ligne, "titre: ", 7)) snprintf(c->titre, sizeof c->titre, "%s", ligne + 7);
            else if (!strcmp(ligne, "etoile"))      c->etoile = 1;
            else { fprintf(stderr, "%s : en-tête inconnu « %s »\n", c->nom, ligne); return 0; }
            continue;
        }
        if (n + l + 2 > cap) {
            cap = 2 * (n + l + 2);
            char *t = realloc(c->brut, cap);
            if (!t) return 0;
            c->brut = t;
        }
        memcpy(c->brut + n, ligne, l); n += l;
        c->brut[n++] = '\n';
        c->brut[n] = 0;
    }
    for (int i = 0; i < g_nc; i++)
        if (!convertit(&g_c[i])) return 0;
    return g_nc > 0;
}

/* ---- la pile ---- */

static Object *g_carte[MAX_CARTES];
static Object *g_champ[MAX_CARTES];

static Object *champ(Object *ou, const char *nom, int x, int y, int w, int h,
                     const char *police, int taille, int style, int centre)
{
    Object *f = hc_new_field(ou, nom);
    if (!f) return NULL;
    f->x = x; f->y = y; f->w = w; f->h = h;
    f->locktext = 1;            /* verrouillé : le clic part au script */
    if (police) { free(f->textfont); f->textfont = strdup(police); }
    if (taille) f->textsize = taille;
    f->textstyle = style;
    f->text_align = centre;
    return f;
}

static Object *bouton(Object *ou, const char *nom, int x, int y, int w, int h,
                      const char *script)
{
    Object *b = hc_new_button(ou, nom);
    if (!b) return NULL;
    b->x = x; b->y = y; b->w = w; b->h = h;
    free(b->style); b->style = strdup("roundRect");
    hc_set_script(b, script);
    return b;
}

/* Le script de chaque champ « Texte » : celui des piles d'aide d'Apple, et
 * celui qu'enseigne la carte « Faire un lien ». */
static const char SCRIPT_LIEN[] =
    "-- Un lien : des mots au style Group, et ce script.\n"
    "-- A link: words in the Group style, and this script.\n"
    "on mouseUp\n"
    "  if \"group\" is in the textStyle of the clickChunk then\n"
    "    go card (the clickText)\n"
    "  end if\n"
    "end mouseUp\n";

static const char SCRIPT_ETOILE[] =
    "-- Une couleur au hasard. / A colour at random.\n"
    "on mouseUp\n"
    "  set the backColor of me to any item of "
    "\"red,orange,gold,lime,turquoise,blue,purple,pink\"\n"
    "end mouseUp\n";

/* Lignes estimées : une par saut de ligne, plus les retours à la ligne
 * qu'imposent les lignes longues. 62 caractères pour un champ large, 44 pour
 * celui qui laisse la place à l'étoile. UNE ESTIMATION, voir l'en-tête. */
static int lignes_estimees(const char *t, int large)
{
    int par_ligne = large ? 62 : 44, total = 0;
    const char *p = t;
    for (;;) {
        const char *fin = strchr(p, '\n');
        int octets = fin ? (int)(fin - p) : (int)strlen(p);
        int car = hct_utf8_compte_prefixe(p, octets);
        total += car == 0 ? 1 : (car + par_ligne - 1) / par_ligne;
        if (!fin) break;
        p = fin + 1;
    }
    return total;
}

#define LIGNES_VISIBLES 15     /* 250 points de champ, 16 par ligne en corps 12 */

static Object *monte(const char *script_pile)
{
    Object *st = hc_new_stack("Hypertexte");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Page");

    bouton(bg, "◀", 20, 308, 56, 26, "on mouseUp\n  pagePrecedente\nend mouseUp\n");
    bouton(bg, "▶", 84, 308, 56, 26, "on mouseUp\n  pageSuivante\nend mouseUp\n");
    bouton(bg, "Français ⇄ English", 312, 308, 180, 26,
           "on mouseUp\n  autreLangue\nend mouseUp\n");

    for (int i = 0; i < g_nc; i++) {
        Carte *c = &g_c[i];
        Object *cd = hc_new_card(st, bg, c->nom);
        g_carte[i] = cd;

        Object *t = champ(cd, "Titre", 20, 10, 472, 32, NULL, 20, HC_BOLD, 1);
        if (t) hc_set_field_text(t, c->titre);

        int w = c->etoile ? 330 : 472;
        Object *f = champ(cd, "Texte", 20, 50, w, 250, "Geneva", 12, 0, 0);
        g_champ[i] = f;
        if (!f) continue;
        hc_set_field_text(f, c->texte);
        hc_runs_clear(f);
        for (int k = 0; k < c->npl; k++) {
            if (c->pl[k].code) hc_run_add_full(f, c->pl[k].deb, c->pl[k].lon, 0, 0, "Monaco");
            else               hc_run_add(f, c->pl[k].deb, c->pl[k].lon, HC_GROUP);
        }
        hc_set_script(f, SCRIPT_LIEN);
        if (lignes_estimees(c->texte, !c->etoile) > LIGNES_VISIBLES) {
            free(f->style); f->style = strdup("scrolling");
        }

        if (c->etoile) {
            /* Une étoile à cinq branches, en polygone : dix sommets, en
             * coordonnées de carte, autour de (431, 160). */
            Object *e = bouton(cd, "Etoile", 380, 110, 100, 100, SCRIPT_ETOILE);
            if (e) {
                char pts[512] = "";
                size_t n = 0;
                for (int k = 0; k < 10; k++) {
                    double r = (k % 2) ? 21.0 : 52.0;
                    double a = -M_PI / 2 + k * M_PI / 5;
                    n += (size_t)snprintf(pts + n, sizeof pts - n, "%s%ld,%ld",
                                          k ? "\n" : "",
                                          lround(431 + r * cos(a)),
                                          lround(160 + r * sin(a)));
                }
                free(e->style); e->style = strdup("polygon");
                hc_pose_sommets(e, pts);
                e->showname = 0;
            }
        }
    }

    hc_set_script(st, script_pile);

    /* Les couleurs, par HyperTalk : le titre en bleu marine, l'étoile en or,
     * et le titre des deux cartes « couleur » en arc-en-ciel, lettre à
     * lettre. textColor se pose sur un MORCEAU de texte — mesuré le
     * 11 octobre : sur le champ entier, HC répond « propriété inconnue ». */
    for (int i = 0; i < g_nc; i++) {
        hc_set_current_card(g_carte[i]);
        hc_do("set the textColor of char 1 to (the number of chars of "
              "card field \"Titre\") of card field \"Titre\" to navy");
        if (g_c[i].etoile) {
            hc_do("set the backColor of card button \"Etoile\" to gold");
            hc_do("set the foreColor of card button \"Etoile\" to orange");
            hc_do("put \"red,orange,gold,green,turquoise,blue,purple\" into arc");
            hc_do("repeat with k = 1 to the number of chars of card field \"Titre\"\n"
                  "  set the textColor of char k of card field \"Titre\" to "
                  "item ((k - 1) mod 7 + 1) of arc\n"
                  "end repeat");
        }
    }
    hc_set_current_card(g_carte[0]);
    return st;
}

/* ---- les essais ---- */

static int index_carte(Object *cd)
{
    for (int i = 0; i < g_nc; i++) if (g_carte[i] == cd) return i;
    return -1;
}

static const char *ici(void)
{
    int i = index_carte(hc_current_card());
    return i >= 0 ? g_c[i].nom : "(hors de la pile)";
}

/* Un clic sur les octets [deb, deb + lon[ du champ : ce que rend HCview.m. */
static void clique(Object *f, int deb, int lon)
{
    const char *tx = hc_field_text(f);
    char d[96];
    hc_describe(f, d, sizeof d);
    snprintf(g_chunk, sizeof g_chunk, "char %d to %d of card %s",
             hct_utf8_compte_prefixe(tx, deb) + 1,
             hct_utf8_compte_prefixe(tx, deb + lon), d);
    snprintf(g_clic, sizeof g_clic, "%.*s", lon, tx + deb);
    hc_send(f, "mouseUp");
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s pile.txt cartes.txt [Hypertexte.stack]\n", argv[0]);
        return 2;
    }
    char *script = lire(argv[1]);
    char *cartes = lire(argv[2]);
    if (!script || !cartes) return 2;
    if (!lit_cartes(cartes)) return 2;

    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    h.global_get = glob_lit;
    h.global_set = glob_pose;
    hc_set_host(&h);

    Object *st = monte(script);

    if (argc > 3) {
        if (hc_save(st, argv[3]) != 0) {
            fprintf(stderr, "impossible d'écrire %s\n", argv[3]);
            return 1;
        }
        printf("pile écrite : %s\n", argv[3]);
        hc_unregister_stack(st);
        hc_free(st);
        for (int i = 0; i < g_nc; i++) { free(g_c[i].brut); free(g_c[i].texte); }
        free(script); free(cartes);
        return 0;
    }

    puts("== 1. les cartes, leurs liens, leurs lignes estimées ==");
    for (int i = 0; i < g_nc; i++) {
        int liens = 0;
        for (int k = 0; k < g_c[i].npl; k++) if (!g_c[i].pl[k].code) liens++;
        printf("   %2d %-20s %d lien(s), ~%d lignes%s\n", i + 1, g_c[i].nom, liens,
               lignes_estimees(g_c[i].texte, !g_c[i].etoile),
               (g_champ[i] && g_champ[i]->style && !strcmp(g_champ[i]->style, "scrolling"))
                   ? ", défile" : "");
    }

    hc_set_current_card(g_carte[0]);
    hc_do("put \"   titre de l'accueil : \" & the textColor of char 1 of card field \"Titre\"");
    hc_set_current_card(g_carte[4]);
    hc_do("put \"   titre de la couleur : \" & the textColor of char 1 of card field \"Titre\""
          " && the textColor of char 2 of card field \"Titre\"");

    puts("\n== 2. chaque lien, cliqué, mène à la carte de son nom ==");
    int rates = 0;
    for (int i = 0; i < g_nc; i++) {
        for (int k = 0; k < g_c[i].npl; k++) {
            if (g_c[i].pl[k].code) continue;
            hc_set_current_card(g_carte[i]);
            clique(g_champ[i], g_c[i].pl[k].deb, g_c[i].pl[k].lon);
            const char *arrivee = ici();
            int bon = !strcasecmp(arrivee, g_clic);
            if (!bon) rates++;
            printf("   %-20s « %s » -> %s%s\n", g_c[i].nom, g_clic, arrivee,
                   bon ? "" : "   <-- RATÉ");
        }
    }
    printf("   liens ratés : %d\n", rates);

    puts("\n== 3. toutes les cartes s'atteignent par les liens, depuis l'accueil ==");
    for (int depart = 0; depart < g_nc; depart += g_nc / 2) {
        int vu[MAX_CARTES] = {0}, pile[MAX_CARTES], np = 0, nvu = 1;
        vu[depart] = 1; pile[np++] = depart;
        while (np > 0) {
            int i = pile[--np];
            for (int k = 0; k < g_c[i].npl; k++) {
                if (g_c[i].pl[k].code) continue;
                char cible[96];
                snprintf(cible, sizeof cible, "%.*s", g_c[i].pl[k].lon,
                         g_c[i].texte + g_c[i].pl[k].deb);
                for (int j = 0; j < g_nc; j++)
                    if (!vu[j] && !strcasecmp(g_c[j].nom, cible)) {
                        vu[j] = 1; nvu++; pile[np++] = j;
                    }
            }
        }
        printf("   depuis « %s » : %d carte(s) sur %d", g_c[depart].nom, nvu, g_nc / 2);
        for (int j = 0; j < g_nc; j++)
            if (!vu[j] && (j < g_nc / 2) == (depart < g_nc / 2))
                printf(" ; manque « %s »", g_c[j].nom);
        printf("\n");
    }

    puts("\n== 4. un clic hors d'un lien ne mène nulle part ==");
    hc_set_current_card(g_carte[0]);
    clique(g_champ[0], 0, 2);                       /* « HC », un mot ordinaire */
    printf("   sur « %s » -> %s\n", g_clic, ici());
    g_chunk[0] = 0; g_clic[0] = 0;                  /* sous le texte, dans le vide */
    hc_send(g_champ[0], "mouseUp");
    printf("   dans le vide -> %s\n", ici());

    puts("\n== 5. les flèches tournent les pages sans changer de langue ==");
    Object *suiv = hc_resolve("bg button \"▶\"");
    Object *prec = hc_resolve("bg button \"◀\"");
    Object *lang = hc_resolve("bg button \"Français ⇄ English\"");
    printf("   boutons trouvés : %s %s %s\n", suiv ? "▶" : "-", prec ? "◀" : "-",
           lang ? "langue" : "-");
    if (suiv && prec && lang) {
        hc_set_current_card(g_carte[0]);
        printf("   %s", ici());
        for (int k = 0; k < g_nc / 2; k++) { hc_send(suiv, "mouseUp"); printf(" > %s", ici()); }
        printf("\n   %s", ici());
        hc_send(prec, "mouseUp"); printf(" < %s", ici());
        hc_send(prec, "mouseUp"); printf(" < %s\n", ici());
        hc_set_current_card(g_carte[g_nc / 2]);
        printf("   %s", ici());
        hc_send(prec, "mouseUp"); printf(" < %s", ici());
        hc_send(suiv, "mouseUp"); printf(" > %s\n", ici());

        puts("\n== 6. le bouton de langue mène à la jumelle ==");
        for (int i = 0; i < g_nc; i++) {
            hc_set_current_card(g_carte[i]);
            hc_send(lang, "mouseUp");
            int j = index_carte(hc_current_card());
            int bon = j == (i + g_nc / 2) % g_nc;
            printf("   %-20s -> %s%s\n", g_c[i].nom, ici(), bon ? "" : "   <-- RATÉ");
        }
    }

    puts("\n== 7. les exemples de la carte HyperTalk, tapés tels quels ==");
    hc_set_current_card(g_carte[0]);
    hc_do("go to card \"Les jeux\"");
    printf("   go to card \"Les jeux\" -> %s\n", ici());
    hc_do("go to card \"Games\"");
    printf("   go to card \"Games\" -> %s\n", ici());

    puts("\n== 8. l'étoile change de couleur ==");
    for (int i = 0; i < g_nc; i++) {
        if (!g_c[i].etoile) continue;
        hc_set_current_card(g_carte[i]);
        Object *e = hc_resolve("card button \"Etoile\"");
        if (!e) { printf("   %s : pas d'étoile\n", g_c[i].nom); continue; }
        int xy[2 * HC_SOMMETS_MAX];
        int ns = hc_bouton_sommets(e, xy, HC_SOMMETS_MAX);
        printf("   %s : %d sommets, rectangle %d,%d %dx%d, couleur posée : %s\n",
               g_c[i].nom, ns, e->x, e->y, e->w, e->h,
               e->backcolor ? "oui" : "non");
        int change = 0;
        for (int k = 0; k < 20 && !change; k++) {
            int avant = e->backcolor;
            hc_send(e, "mouseUp");
            change = e->backcolor != avant;
        }
        printf("   un clic sur l'étoile change sa couleur : %s\n", change ? "oui" : "NON");
    }

    puts("\n== 9. show groups à l'ouverture, hide groups au départ ==");
    hc_set_current_card(g_carte[0]);
    hc_send(st, "openStack");
    printf("   openStack    -> showGroups %d\n", g_groupes);
    hc_send(st, "suspendStack");
    printf("   suspendStack -> showGroups %d\n", g_groupes);
    hc_send(st, "resumeStack");
    printf("   resumeStack  -> showGroups %d\n", g_groupes);
    hc_send(st, "closeStack");
    printf("   closeStack   -> showGroups %d\n", g_groupes);

    /* LE FICHIER, puisque c'est lui qui part dans le DMG : écrit, relu, et
     * les liens retrouvés au même endroit, au style Group. */
    puts("\n== 10. la pile écrite puis relue garde ses liens ==");
    char chemin[] = "/tmp/hypertexte_XXXXXX";
    int fd = mkstemp(chemin);
    if (fd < 0) { perror("mkstemp"); return 1; }
    close(fd);
    int ecrit = hc_save(st, chemin) == 0;
    Object *relue = ecrit ? hc_load(chemin) : NULL;
    unlink(chemin);
    printf("   écrite : %s, relue : %s\n", ecrit ? "oui" : "NON", relue ? "oui" : "NON");
    if (relue) {
        int cartes_relues = 0, liens_relus = 0, liens_attendus = 0, ecarts = 0;
        for (int i = 0; i < g_nc; i++)
            for (int k = 0; k < g_c[i].npl; k++)
                if (!g_c[i].pl[k].code) liens_attendus++;
        for (int i = 0, j = 0; i < relue->nparts; i++) {
            Object *cd = relue->parts[i];
            if (cd->type != OBJ_CARD || j >= g_nc) continue;
            cartes_relues++;
            Object *f = NULL;
            for (int k = 0; k < cd->nparts; k++)
                if (cd->parts[k]->type == OBJ_FIELD && cd->parts[k]->name &&
                    !strcmp(cd->parts[k]->name, "Texte")) f = cd->parts[k];
            if (!f) { ecarts++; j++; continue; }
            for (int r = 0; r < hc_run_count(f); r++) {
                int deb = 0, lon = 0, style = 0;
                if (!hc_run_at(f, r, &deb, &lon, &style) || !(style & HC_GROUP)) continue;
                liens_relus++;
                int trouve = 0;
                for (int k = 0; k < g_c[j].npl; k++)
                    if (!g_c[j].pl[k].code && g_c[j].pl[k].deb == deb && g_c[j].pl[k].lon == lon)
                        trouve = 1;
                if (!trouve) ecarts++;
            }
            j++;
        }
        printf("   cartes : %d, liens au style Group : %d sur %d, écarts : %d\n",
               cartes_relues, liens_relus, liens_attendus, ecarts);
        hc_free(relue);
    }

    hc_unregister_stack(st);
    hc_free(st);
    for (int i = 0; i < g_nc; i++) { free(g_c[i].brut); free(g_c[i].texte); }
    free(script); free(cartes);
    return 0;
}
