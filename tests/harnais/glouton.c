/* glouton — GOBE-PROUT, UN PAC-MAN MAISON EN HYPERTALK, MONTÉ PAR LE CODE.
 *
 * Demandé par l'utilisatrice le 25 octobre, après Boulder-Prout : « on
 * attaque pac man ! ». Du jeu d'origine, l'esprit seulement : le
 * labyrinthe, les noms, la musique et les dessins sont les nôtres. Tout le
 * jeu vit dans le script de la pile, glouton_pile.txt ; les icônes et le
 * labyrinthe sont dans glouton_icones.txt et glouton_labyrinthe.txt, en
 * texte, à retoucher à la main.
 *
 *   glouton pile.txt icones.txt labyrinthe.txt                 le test
 *   glouton pile.txt icones.txt labyrinthe.txt Glouton.stack   écrit la pile
 *
 * LA CARTE fait 736 × 600 : le labyrinthe, 23 × 17 cases de 32 points, puis
 * la barre d'état. Le bouton 1 est le fond noir ; les boutons 2 à 392 sont
 * les pastilles, une par case (cachée là où il n'y en a pas) ; viennent
 * ensuite les murs, puis le fruit, les quatre fantômes et le glouton, posés
 * en dernier pour être dessinés dessus.
 *
 * Le test rejoue les règles une à une — avancer, buter, tourner au
 * carrefour, faire demi-tour, le tunnel, manger, la peur, les fantômes
 * mangés qui rentrent, la cible de chacun, la dispersion et la chasse, la
 * rencontre qui tue, le fruit, le niveau fini —, puis un ROBOT joue, et l'on
 * vérifie à CHAQUE image que les boutons disent ce que dit l'état.
 *
 * NON VÉRIFIÉ ICI : le rendu, les sons, la vitesse réelle — à jouer DANS HC
 * (l'application). */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define NR 17
#define NC 23

/* ─── l'hôte ─────────────────────────────────────────────────────────── */

static char g_dernier[512];
static int  g_erreurs = 0;
static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) snprintf(g_dernier, sizeof g_dernier, "%s", t ? t : "");
    else if (k == HC_ERR) { printf("   [ERR] %s\n", t); g_erreurs++; }
}

static char g_sons[1024];
static void note_son(const char *nom)
{
    size_t n = strlen(g_sons);
    if (n + strlen(nom) + 2 < sizeof g_sons)
        snprintf(g_sons + n, sizeof g_sons - n, "%s%s", n ? " " : "", nom);
}
static void mon_son(const char *nom) { note_son(nom); }
static void mes_notes(const char *nom, const HcNote *notes, int n)
{
    char s[64];
    (void)notes;
    snprintf(s, sizeof s, "%s(%d notes)", nom, n);
    note_son(s);
}

static const char *g_tenues = "";
static int g_horloge = 0;
static struct timespec g_t0;
static long g_lectures = 0, g_echap_a = 0, g_filet = 0;
static const char *mon_global(const char *nom)
{
    if (!strcmp(nom, "keysDown")) {
        g_lectures++;
        if (g_filet && g_lectures == g_filet) hc_interrompre();
        if (g_echap_a && g_lectures >= g_echap_a) return "65307";
        return g_tenues;
    }
    if (!strcmp(nom, "ticks") && g_horloge) {
        static char b[32];
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        double s = (double)(t.tv_sec - g_t0.tv_sec) + (double)(t.tv_nsec - g_t0.tv_nsec) / 1e9;
        snprintf(b, sizeof b, "%ld", 1000 + (long)(s * 60));
        return b;
    }
    return NULL;
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

static char *ligne_suivante(char **p)
{
    if (!*p || !**p) return NULL;
    char *l = *p, *fin = strchr(l, '\n');
    if (fin) { *fin = 0; *p = fin + 1; } else *p = l + strlen(l);
    size_t n = strlen(l);
    if (n && l[n - 1] == '\r') l[n - 1] = 0;
    return l;
}

/* ─── les icônes ─────────────────────────────────────────────────────── */

static struct { char k; unsigned char r, v, b; } g_pal[64];
static int g_npal = 0;

static int charge_icones(Object *st, const char *chemin)
{
    char *t = lire(chemin);
    if (!t) return -1;
    char *p = t, *l;
    int n = 0;
    while ((l = ligne_suivante(&p))) {
        if (!strncmp(l, "--", 2) || !*l) continue;
        if (!strncmp(l, "couleur ", 8)) {
            int r, v, b; char k;
            if (sscanf(l + 8, "%c %d %d %d", &k, &r, &v, &b) != 4 || g_npal >= 64) goto faute;
            g_pal[g_npal].k = k;
            g_pal[g_npal].r = (unsigned char)r; g_pal[g_npal].v = (unsigned char)v; g_pal[g_npal].b = (unsigned char)b;
            g_npal++;
            continue;
        }
        if (strncmp(l, "icone ", 6)) goto faute;
        int num; char nom[64] = "";
        if (sscanf(l + 6, "%d %63[^\n]", &num, nom) < 1) goto faute;
        struct StackIcon *ic = hc_icon_add(st, num, nom);
        if (!ic || !hc_icon_couleur_cree(ic)) goto faute;
        for (int y = 0; y < 16; y++) {
            char *a = ligne_suivante(&p);
            if (!a || strlen(a) != 16) goto faute;
            for (int x = 0; x < 16; x++) {
                int idx = 0;
                if (a[x] != ' ') {
                    int j = 0;
                    while (j < g_npal && g_pal[j].k != a[x]) j++;
                    if (j == g_npal) goto faute;
                    idx = hc_icon_palette_index(ic, g_pal[j].r, g_pal[j].v, g_pal[j].b);
                    if (!idx) goto faute;
                }
                for (int dy = 0; dy < 2; dy++)
                    for (int dx = 0; dx < 2; dx++)
                        hc_icon_pixel_pose(ic, 2 * x + dx, 2 * y + dy, idx);
            }
        }
        n++;
    }
    free(t);
    return n;
faute:
    fprintf(stderr, "icônes : ligne illisible « %s »\n", l ? l : "(fin)");
    free(t);
    return -1;
}

/* ─── le labyrinthe ──────────────────────────────────────────────────── */

static char g_modele[NR][NC + 1];       /* tel qu'écrit dans le fichier */

static int charge_labyrinthe(const char *chemin)
{
    char *t = lire(chemin);
    if (!t) return -1;
    char *p = t, *l;
    int r = 0;
    while ((l = ligne_suivante(&p)) && r < NR) {
        if (strlen(l) != NC) { fprintf(stderr, "labyrinthe : ligne %d de %zu caractères\n", r + 1, strlen(l)); free(t); return -1; }
        memcpy(g_modele[r], l, NC + 1);
        r++;
    }
    free(t);
    return r == NR ? 0 : -1;
}

static int est_mur(int r, int c)
{
    if (r < 0 || r >= NR || c < 0 || c >= NC) return 1;
    return g_modele[r][c] == '#' || g_modele[r][c] == 'x';
}

/* ─── la pile ────────────────────────────────────────────────────────── */

static Object *g_carte;
static Object *g_past[NR * NC];
static Object *g_glouton, *g_fant[4], *g_fruit;
static int g_nmurs = 0;

static Object *pose_champ(const char *nom, int x, int y, int w, int h, int taille, int style)
{
    Object *f = hc_new_field(g_carte, nom);
    f->x = x; f->y = y; f->w = w; f->h = h;
    f->locktext = 1;
    if (taille) f->textsize = taille;
    f->textstyle = style;
    return f;
}

static Object *pose_bouton(const char *nom, const char *style, int x, int y, int w, int h)
{
    Object *b = hc_new_button(g_carte, nom);
    b->x = x; b->y = y; b->w = w; b->h = h;
    b->showname = 0;
    b->autohilite = 0;
    free(b->style); b->style = strdup(style);
    return b;
}

/* La couleur d'un bouton, par le chemin d'HyperTalk : c'est celui que
 * prendrait une pile écrite à la main. */
static void colore(int numero, const char *quoi, const char *rvb)
{
    char s[128];
    snprintf(s, sizeof s, "set the %s of card button %d to \"%s\"", quoi, numero, rvb);
    hc_do(s);
}

static Object *monte(const char *script, const char *labyrinthe_texte)
{
    Object *st = hc_new_stack("Gobe-Prout");
    st->w = NC * 32; st->h = NR * 32 + 56;
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    g_carte = hc_new_card(st, bg, "Labyrinthe");
    hc_set_current_card(g_carte);
    int num = 0;

    pose_bouton("Cadre", "rectangle", 0, 0, NC * 32, NR * 32);
    num++;
    colore(num, "backColor", "0,0,0"); colore(num, "foreColor", "0,0,0");

    /* Les pastilles : une par case. Les grosses sont des ronds. */
    for (int r = 0; r < NR; r++)
        for (int c = 0; c < NC; c++) {
            char ch = g_modele[r][c];
            int gros = ch == 'o';
            int t = gros ? 16 : 6;
            g_past[r * NC + c] = pose_bouton("", gros ? "oval" : "rectangle",
                                             c * 32 + 16 - t / 2, r * 32 + 16 - t / 2, t, t);
            num++;
            colore(num, "backColor", "255,184,151"); colore(num, "foreColor", "255,184,151");
            g_past[r * NC + c]->visible = ch == '.' || ch == 'o';
        }

    /* Les murs : un rectangle par case, en retrait de 6 points du côté
     * d'un couloir, si bien que deux cases de mur voisines se touchent et
     * que les couloirs gardent une marge noire. La porte est une barre
     * rose. */
    for (int r = 0; r < NR; r++)
        for (int c = 0; c < NC; c++) {
            char ch = g_modele[r][c];
            if (ch == '-') {
                pose_bouton("", "rectangle", c * 32, r * 32 + 13, 32, 6);
                num++;
                colore(num, "backColor", "255,184,222"); colore(num, "foreColor", "255,184,222");
                continue;
            }
            if (ch != '#') continue;
            int g = est_mur(r, c - 1) ? 0 : 6, d = est_mur(r, c + 1) ? 0 : 6;
            int h = est_mur(r - 1, c) ? 0 : 6, b = est_mur(r + 1, c) ? 0 : 6;
            pose_bouton("", "rectangle", c * 32 + g, r * 32 + h, 32 - g - d, 32 - h - b);
            num++;
            colore(num, "backColor", "33,33,222"); colore(num, "foreColor", "33,33,222");
            g_nmurs++;
        }

    g_fruit = pose_bouton("fruit", "transparent", 0, 0, 32, 32);
    g_fruit->icon = 3170;
    g_fruit->visible = 0;
    for (int i = 0; i < 4; i++) {
        char nom[16];
        snprintf(nom, sizeof nom, "g%d", i + 1);
        g_fant[i] = pose_bouton(nom, "transparent", 0, 0, 32, 32);
    }
    g_glouton = pose_bouton("glouton", "transparent", 0, 0, 32, 32);

    int y = NR * 32 + 4;
    Object *t = pose_champ("Titre", 8, y, 132, 22, 14, HC_BOLD);
    hc_set_field_text(t, "GOBE-PROUT");
    hc_set_field_text(pose_champ("lScore", 144, y + 2, 46, 20, 10, 0), "SCORE");
    pose_champ("score", 190, y, 70, 22, 14, HC_BOLD);
    hc_set_field_text(pose_champ("lBest", 266, y + 2, 38, 20, 10, 0), "BEST");
    hc_set_field_text(pose_champ("best", 304, y, 70, 22, 14, HC_BOLD), "0");
    hc_set_field_text(pose_champ("lVies", 380, y + 2, 42, 20, 10, 0), "LIVES");
    pose_champ("vies", 422, y, 28, 22, 14, HC_BOLD);
    hc_set_field_text(pose_champ("lNiveau", 456, y + 2, 44, 20, 10, 0), "LEVEL");
    pose_champ("niveau", 500, y, 36, 22, 14, HC_BOLD);
    pose_champ("etat", 8, y + 26, 440, 20, 12, HC_ITALIC);
    hc_set_field_text(pose_champ("aide", 452, y + 28, 150, 18, 10, 0), "Arrows, ZQSD or WASD");
    Object *m = pose_champ("maze", 8, 8, 300, 300, 9, 0);
    m->visible = 0;
    hc_set_field_text(m, labyrinthe_texte);
    Object *n = pose_bouton("New game", "roundRect", NC * 32 - 128, y + 2, 120, 26);
    n->showname = 1;
    hc_set_script(n, "on mouseUp\n  nouvellePartie\nend mouseUp\n");

    hc_set_script(st, script);
    return st;
}

/* ─── lire l'état ────────────────────────────────────────────────────── */

static char g_val[8][4096];
static int  g_tour = 0;
static const char *G(const char *nom)
{
    char *v = hc_module_globale(nom);
    char *b = g_val[g_tour++ % 8];
    snprintf(b, sizeof g_val[0], "%s", v ? v : "");
    free(v);
    return b;
}
static int GI(const char *nom) { return atoi(G(nom)); }
static void pose(const char *nom, const char *v) { hc_module_pose_globale(nom, v); }

/* L'item i (1..) d'une liste à virgules. */
static int item_i(const char *liste, int i)
{
    const char *p = liste;
    for (int k = 1; k < i && p; k++) { p = strchr(p, ','); if (p) p++; }
    return p ? atoi(p) : 0;
}
static void item_s(const char *liste, int i, char *out, size_t n)
{
    const char *p = liste;
    for (int k = 1; k < i && p; k++) { p = strchr(p, ','); if (p) p++; }
    if (!p) { out[0] = 0; return; }
    size_t k = strcspn(p, ",");
    if (k >= n) k = n - 1;
    memcpy(out, p, k); out[k] = 0;
}

static char g_lab[NR][NC + 1];
static int lit_lab(void)
{
    const char *p = G("gLab");
    for (int r = 0; r < NR; r++) {
        const char *fin = strpbrk(p, "\r\n");
        size_t n = fin ? (size_t)(fin - p) : strlen(p);
        if (n != NC) return 0;
        memcpy(g_lab[r], p, NC); g_lab[r][NC] = 0;
        p = fin ? fin + 1 : p + n;
    }
    return 1;
}

static int ddx(int d) { return d == 2 ? 1 : d == 4 ? -1 : 0; }
static int ddy(int d) { return d == 3 ? 1 : d == 1 ? -1 : 0; }

/* Ce que l'état dit, et ce que les boutons montrent : 0, ou le premier
 * désaccord. */
static int g_detail;
static int controle(void)
{
    if (!lit_lab()) return 1;
    for (int r = 0; r < NR; r++)
        for (int c = 0; c < NC; c++) {
            char ch = g_lab[r][c];
            if (ch == 'o') continue;                 /* elles clignotent */
            if (g_past[r * NC + c]->visible != (ch == '.')) { g_detail = r * NC + c; return 2; }
        }
    int pr = GI("gPr"), pc = GI("gPc"), pd = GI("gPd"), pp = GI("gPp");
    if (pp % 4 || pp < 0 || pp >= 32) return 3;
    if (g_glouton->x + 16 != (pc - 1) * 32 + 16 + ddx(pd) * pp ||
        g_glouton->y + 16 != (pr - 1) * 32 + 16 + ddy(pd) * pp) return 4;
    char ch = g_lab[pr - 1][pc - 1];
    if (ch == '#' || ch == 'x' || ch == '-' || ch == 'h') return 5;
    const char *gr = G("gGr"), *gc = G("gGc"), *gd = G("gGd"), *gp = G("gGp"), *gm = G("gGm");
    for (int i = 1; i <= 4; i++) {
        int r = item_i(gr, i), c = item_i(gc, i), d = item_i(gd, i), p = item_i(gp, i);
        char m[16];
        item_s(gm, i, m, sizeof m);
        if (p % 4 || p < 0 || p >= 32) { g_detail = i; return 6; }
        if (g_fant[i - 1]->x + 16 != (c - 1) * 32 + 16 + ddx(d) * p ||
            g_fant[i - 1]->y + 16 != (r - 1) * 32 + 16 + ddy(d) * p) { g_detail = i; return 7; }
        char f = g_lab[r - 1][c - 1];
        if (f == '#' || f == 'x') { g_detail = i; return 8; }
        if ((f == '-' || f == 'h') && strcmp(m, "sortie") && strcmp(m, "yeux") && strcmp(m, "maison")) {
            g_detail = i; return 9;
        }
    }
    return 0;
}

/* ─── jouer des images à la main ─────────────────────────────────────── */

static const char *code_de(char k)
{
    switch (k) {
    case 'h': return "65362"; case 'b': return "65364";
    case 'g': return "65361"; case 'd': return "65363";
    case 'z': return "122";   case 'q': return "113";
    case 's': return "115";   case 'a': return "97";   case 'w': return "119";
    default:  return "";
    }
}

static int g_defauts = 0;
static void une_image(char touche)
{
    g_tenues = code_de(touche);
    hc_send(g_carte, "image");
    g_tenues = "";
    int r = controle();
    if (r) { printf("   DÉFAUT %d (%d)\n", r, g_detail); g_defauts++; }
}
static void images(int n, char touche) { for (int i = 0; i < n; i++) une_image(touche); }

static void etat_glouton(const char *quoi)
{
    printf("   %-40s glouton %s,%s dir %s pas %s · score %s\n", quoi,
           G("gPr"), G("gPc"), G("gPd"), G("gPp"), G("gScore"));
}

static void etat_fantome(const char *quoi, int i)
{
    char m[16];
    item_s(G("gGm"), i, m, sizeof m);
    printf("   %-40s fantôme %d en %d,%d dir %d · %s\n", quoi, i,
           item_i(G("gGr"), i), item_i(G("gGc"), i), item_i(G("gGd"), i), m);
}

/* Une partie neuve, sans la musique ni l'attente : ce que fait
 * nouvellePartie avant sa boucle. */
static void partie_neuve(void)
{
    hc_do("global gVies, gNiveau, gScore\nput 3 into gVies\nput 1 into gNiveau\nput 0 into gScore");
    hc_send(g_carte, "chargeLab");
    hc_send(g_carte, "remetPastilles");
    hc_send(g_carte, "placeActeurs");
    hc_send(g_carte, "dessine");
    pose("gIssue", "");
    pose("gAlea", "4242");
    g_sons[0] = 0;
}

/* Les fantômes hors jeu : tous à la maison, immobiles. */
static void fantomes_au_lit(void)
{
    pose("gGm", "maison,maison,maison,maison");
    pose("gGr", "10,10,10,10");
    pose("gGc", "10,11,12,13");
    pose("gGd", "0,0,0,0");
    pose("gGp", "0,0,0,0");
    pose("gT", "1000");          /* plus personne ne sort */
}

/* ─── le robot ───────────────────────────────────────────────────────── */

static int libre_g(int r, int c)
{
    if (r < 0 || r >= NR) return 0;
    c = (c + NC) % NC;
    char ch = g_lab[r][c];
    return !(ch == '#' || ch == 'x' || ch == '-' || ch == 'h');
}

/* Le plus court chemin vers la pastille la plus proche, en évitant les
 * abords des fantômes qui ne sont pas apeurés ; sans chemin, il fuit. */
static char robot(void)
{
    lit_lab();
    int pr = GI("gPr") - 1, pc = GI("gPc") - 1;
    const char *gr = G("gGr"), *gc = G("gGc"), *gm = G("gGm");
    int danger[NR][NC] = { { 0 } };
    for (int i = 1; i <= 4; i++) {
        char m[16];
        item_s(gm, i, m, sizeof m);
        if (strcmp(m, "normal") && strcmp(m, "sortie")) continue;
        int fr = item_i(gr, i) - 1, fc = item_i(gc, i) - 1;
        for (int r = 0; r < NR; r++)
            for (int c = 0; c < NC; c++)
                if (abs(r - fr) + abs(c - fc) <= 2) danger[r][c] = 1;
    }
    static const int d4[4] = { 1, 2, 3, 4 };
    int dist[NR][NC], prem[NR][NC], file[NR * NC][2], t = 0, q = 0;
    for (int r = 0; r < NR; r++) for (int c = 0; c < NC; c++) dist[r][c] = -1;
    dist[pr][pc] = 0; prem[pr][pc] = 0;
    file[q][0] = pr; file[q][1] = pc; q++;
    while (t < q) {
        int r = file[t][0], c = file[t][1]; t++;
        if ((r != pr || c != pc) && (g_lab[r][c] == '.' || g_lab[r][c] == 'o')) {
            static const char tk[5] = { '.', 'h', 'd', 'b', 'g' };
            return tk[prem[r][c]];
        }
        for (int k = 0; k < 4; k++) {
            int d = d4[k], rr = r + ddy(d), cc = (c + ddx(d) + NC) % NC;
            if (!libre_g(rr, cc) || dist[rr][cc] >= 0 || danger[rr][cc]) continue;
            dist[rr][cc] = dist[r][c] + 1;
            prem[rr][cc] = prem[r][c] ? prem[r][c] : d;
            file[q][0] = rr; file[q][1] = cc; q++;
        }
    }
    /* fuir : la case voisine la plus loin du fantôme le plus proche */
    int meilleur = 0, dmax = -1;
    for (int k = 0; k < 4; k++) {
        int d = d4[k], rr = pr + ddy(d), cc = (pc + ddx(d) + NC) % NC;
        if (!libre_g(rr, cc)) continue;
        int dm = 1000;
        for (int i = 1; i <= 4; i++) {
            int dd = abs(rr - (item_i(gr, i) - 1)) + abs(cc - (item_i(gc, i) - 1));
            if (dd < dm) dm = dd;
        }
        if (dm > dmax) { dmax = dm; meilleur = d; }
    }
    static const char tk[5] = { '.', 'h', 'd', 'b', 'g' };
    return tk[meilleur];
}

/* ─── le programme ───────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s pile.txt icones.txt labyrinthe.txt [Glouton.stack]\n", argv[0]);
        return 2;
    }
    char *script = lire(argv[1]);
    char *labtxt = lire(argv[3]);
    if (!script || !labtxt || charge_labyrinthe(argv[3]) != 0) return 2;
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.play_sound = mon_son; h.play_notes = mes_notes;
    h.global_get = mon_global;
    hc_set_host(&h);
    { size_t n = strlen(labtxt); while (n && (labtxt[n - 1] == '\n' || labtxt[n - 1] == '\r')) labtxt[--n] = 0; }
    Object *st = monte(script, labtxt);
    int nic = charge_icones(st, argv[2]);
    if (nic <= 0) return 2;

    if (argc > 4) {
        hc_send(g_carte, "openCard");
        if (g_erreurs) { fprintf(stderr, "des erreurs : pile non écrite\n"); return 1; }
        if (hc_save(st, argv[4]) != 0) { fprintf(stderr, "impossible d'écrire %s\n", argv[4]); return 1; }
        printf("pile écrite : %s (%d icônes, %d murs)\n", argv[4], nic, g_nmurs);
        hc_free(st); free(script); free(labtxt);
        return 0;
    }

    puts("== 1. à l'ouverture, le labyrinthe est dessiné ==");
    hc_send(g_carte, "openCard");
    {
        int vis = 0;
        for (int k = 0; k < NR * NC; k++) vis += g_past[k]->visible;
        printf("   %d icônes, %d murs, %d pastilles visibles, %s à manger\n", nic, g_nmurs, vis, G("gTotal"));
        printf("   départs : %s · fruit en %s · super-pastilles %s\n", G("gDepart"), G("gFruitCase"), G("gSupers"));
        printf("   contrôle : %d\n", controle());
        g_dernier[0] = 0;
        hc_do("put card field \"etat\"");
        printf("   %s\n", g_dernier);
    }

    puts("\n== 2. le glouton part à gauche, mange, et bute sur le mur ==");
    partie_neuve();
    fantomes_au_lit();
    etat_glouton("départ");
    images(4, '.');
    etat_glouton("4 images");
    images(40, '.');
    etat_glouton("au bout du couloir");
    printf("   sons : %s\n", g_sons);

    puts("\n== 3. tourner au carrefour, faire demi-tour entre deux cases ==");
    partie_neuve();
    fantomes_au_lit();
    images(1, '.');
    etat_glouton("parti à gauche");
    une_image('d');
    etat_glouton("droite : demi-tour tout de suite");
    images(3, '.');
    etat_glouton("puis il continue à droite");
    une_image('h');
    etat_glouton("haut demandé hors d'un carrefour");
    images(12, '.');
    etat_glouton("il a tourné au carrefour suivant");

    puts("\n== 4. le tunnel : on sort à gauche, on rentre à droite ==");
    partie_neuve();
    fantomes_au_lit();
    pose("gPr", "10"); pose("gPc", "2"); pose("gPd", "4"); pose("gPp", "0"); pose("gVeut", "4");
    for (int i = 0; i < 8; i++) {
        une_image('.');
        etat_glouton(i == 0 ? "dans le tunnel, vers la gauche" : "");
    }

    puts("\n== 5. une super-pastille : la peur, le demi-tour, deux fantômes mangés ==");
    partie_neuve();
    pose("gT", "1000");
    /* le glouton juste à droite de la super-pastille (3,2), deux fantômes
     * dehors qui arrivent */
    pose("gPr", "4"); pose("gPc", "2"); pose("gPd", "1"); pose("gPp", "0"); pose("gVeut", "1");
    pose("gGm", "normal,normal,maison,maison");
    pose("gGr", "2,4,10,10"); pose("gGc", "5,5,12,13"); pose("gGd", "4,4,0,0"); pose("gGp", "0,0,0,0");
    hc_send(g_carte, "dessine");
    etat_fantome("avant", 1);
    images(4, '.');
    etat_glouton("la super-pastille mangée");
    printf("   %-40s peur %s · modes %s · sons %s\n", "", G("gPeur"), G("gGm"), g_sons);
    etat_fantome("le rouge a fait demi-tour", 1);
    {
        int n = 0;
        while (n < 60 && strstr(G("gGm"), "peur")) {
            lit_lab();
            une_image(robot());
            n++;
        }
        printf("   %-40s modes %s · score %s · %d images\n", "le robot les chasse", G("gGm"), G("gScore"), n);
        n = 0;
        while (n < 400 && strstr(G("gGm"), "yeux")) { une_image('.'); n++; }
        printf("   %-40s modes %s · score %s · %d images de plus\n", "les yeux sont rentrés", G("gGm"), G("gScore"), n);
        n = 0;
        while (n < 200 && strstr(G("gGm"), "sortie")) { une_image('.'); n++; }
        printf("   %-40s modes %s · score %s · %d images de plus\n", "et ressortis", G("gGm"), G("gScore"), n);
    }

    puts("\n== 6. la cible de chacun, en chasse ==");
    partie_neuve();
    pose("gPhase", "chasse");
    pose("gPr", "14"); pose("gPc", "6"); pose("gPd", "2"); pose("gPp", "0");
    pose("gGm", "normal,normal,normal,normal");
    pose("gGr", "2,2,2,16"); pose("gGc", "2,22,12,8"); pose("gGd", "2,4,2,2"); pose("gGp", "0,0,0,0");
    for (int i = 1; i <= 4; i++) {
        char cmd[96];
        snprintf(cmd, sizeof cmd, "put cible(%d)", i);
        g_dernier[0] = 0;
        hc_do(cmd);
        static const char *qui[] = { "", "le rouge : le glouton", "la rose : quatre devant",
                                     "le bleu : en tenaille avec le rouge", "l'orange, de près : son coin" };
        printf("   %-40s %s\n", qui[i], g_dernier);
    }
    pose("gGr", "2,2,2,2"); pose("gGc", "2,22,12,2");
    g_dernier[0] = 0;
    hc_do("put cible(4)");
    printf("   %-40s %s\n", "l'orange, de loin : le glouton", g_dernier);
    pose("gPhase", "disperse");
    for (int i = 1; i <= 4; i++) {
        char cmd[96];
        snprintf(cmd, sizeof cmd, "put cible(%d)", i);
        g_dernier[0] = 0;
        hc_do(cmd);
        printf("   %-40s %s\n", i == 1 ? "en dispersion, chacun son coin" : "", g_dernier);
    }

    puts("\n== 7. dispersion puis chasse : tous font demi-tour ==");
    partie_neuve();
    pose("gT", "1000");
    pose("gPhaseT", "1");
    pose("gPr", "16"); pose("gPc", "2");
    etat_fantome("avant", 1);
    une_image('.');
    printf("   %-40s phase %s, pour %s images\n", "", G("gPhase"), G("gPhaseT"));
    etat_fantome("après", 1);

    puts("\n== 8. un fantôme qui n'a pas peur : la rencontre tue ==");
    partie_neuve();
    pose("gT", "1000");
    pose("gGm", "normal,maison,maison,maison");
    pose("gGr", "16,10,10,10"); pose("gGc", "8,11,12,13"); pose("gGd", "2,0,0,0"); pose("gGp", "0,0,0,0");
    {
        int n = 0;
        while (n < 20 && !*G("gIssue")) { une_image('.'); n++; }
        printf("   issue « %s » après %d images\n", G("gIssue"), n);
    }

    puts("\n== 9. quarante pastilles : la cerise paraît, et se mange ==");
    partie_neuve();
    fantomes_au_lit();
    pose("gManges", "39");
    pose("gPr", "12"); pose("gPc", "13"); pose("gPd", "4"); pose("gPp", "0"); pose("gVeut", "4");
    /* une pastille en (12,11), de l'autre côté de la place du fruit */
    hc_do("global gLab\nput \".\" into char 11 of line 12 of gLab\nshow card button (11 * 23 + 11 + 1)");
    images(8, '.');            /* une case, c'est quatre images */
    printf("   %-40s fruit %s images · visible %d · en %d,%d · manges %s\n", "la quarantième", G("gFruitT"),
           g_fruit->visible, g_fruit->x + 16, g_fruit->y + 16, G("gManges"));
    images(8, 'd');
    printf("   %-40s fruit %s · visible %d · score %s · %s\n", "il passe dessus", G("gFruitT"),
           g_fruit->visible, G("gScore"), g_sons);

    puts("\n== 10. la dernière pastille finit le niveau ==");
    partie_neuve();
    fantomes_au_lit();
    hc_do("global gLab, gReste\nrepeat with r = 1 to 17\n"
          "  put line r of gLab into l\n"
          "  repeat with c = 1 to 23\n"
          "    if char c of l is \".\" or char c of l is \"o\" then put \" \" into char c of l\n"
          "  end repeat\n"
          "  put l into line r of gLab\nend repeat\n"
          "put \".\" into char 10 of line 16 of gLab\nput 1 into gReste\n"
          "repeat with k = 1 to 391\n"
          "  if char ((k - 1) mod 23 + 1) of line ((k - 1) div 23 + 1) of gLab is not \".\" then hide card button (k + 1)\n"
          "end repeat");
    hc_send(g_carte, "dessine");
    {
        int n = 0;
        while (n < 20 && !*G("gIssue")) { une_image('.'); n++; }
        printf("   issue « %s » après %d images · reste %s\n", G("gIssue"), n, G("gReste"));
    }

    /* LE ROBOT : deux parties, ses vies comprises, et pas plus de 3 000
     * images en tout — sous ASan, chacune coûte ; trois parties et 7 000
     * images prenaient une minute. À chaque image, le contrôle. */
    puts("\n== 11. un robot joue, et l'écran suit l'état ==");
    {
        int parties = 0, vies_perdues = 0, niveaux = 0, images_tot = 0, mangés_fant = 0;
        for (int p = 0; p < 2 && images_tot < 3000; p++) {
            partie_neuve();
            char a[16];
            snprintf(a, sizeof a, "%d", 1000 + 37 * p);
            pose("gAlea", a);
            int vies = 3;
            while (vies > 0 && images_tot < 3000) {
                pose("gIssue", "");
                int n = 0;
                while (!*G("gIssue") && n < 3000 - images_tot) {
                    int avant = (int)(strstr(G("gGm"), "yeux") != NULL);
                    une_image(robot());
                    n++;
                    if (!avant && strstr(G("gGm"), "yeux")) mangés_fant++;
                }
                images_tot += n;
                const char *iss = G("gIssue");
                if (!strcmp(iss, "fini")) {
                    niveaux++;
                    hc_do("global gNiveau\nadd 1 to gNiveau");
                    hc_send(g_carte, "remetPastilles");
                    hc_send(g_carte, "placeActeurs");
                } else if (!strcmp(iss, "mort")) {
                    vies--; vies_perdues++;
                    hc_send(g_carte, "placeActeurs");
                } else break;
            }
            parties++;
        }
        printf("   %d parties, %d images, %d niveau(x) fini(s), %d vie(s) perdue(s), "
               "%d fantôme(s) mangé(s) · défauts %d\n",
               parties, images_tot, niveaux, vies_perdues, mangés_fant, g_defauts);
    }

    puts("\n== 12. la vraie boucle : Échap arrête la partie ==");
    {
        g_horloge = 1;
        clock_gettime(CLOCK_MONOTONIC, &g_t0);
        g_lectures = 0; g_echap_a = 30; g_filet = 20000;
        g_sons[0] = 0;
        hc_send(g_carte, "nouvellePartie");
        g_dernier[0] = 0;
        hc_do("put card field \"etat\"");
        printf("   %s · %s · sons : %s\n", g_lectures < g_filet ? "arrêtée par Échap" : "Échap SANS EFFET",
               g_dernier, g_sons);
        g_echap_a = 0; g_filet = 0; g_horloge = 0;
    }

    printf("\n== bilan : %d défaut(s), %d erreur(s) de script ==\n", g_defauts, g_erreurs);
    hc_free(st); free(script); free(labtxt);
    return g_defauts || g_erreurs ? 1 : 0;
}
