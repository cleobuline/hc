/* astro — ASTRO-PROUT, UN ASTEROIDS MAISON EN HYPERTALK, MONTÉ PAR LE CODE.
 *
 * Demandé par l'utilisatrice le 25 octobre, après Gobe-Prout (« j'ai
 * vachement joué à Asteroids ! »). Du jeu d'origine, l'esprit seulement.
 * Tout est en boutons polygones tracés au trait blanc, fond transparent :
 * les rochers, le vaisseau, sa flamme, la soucoupe, les débris. Tout le jeu
 * vit dans le script de la pile, astro_pile.txt.
 *
 *   astro pile.txt                        le test
 *   astro pile.txt Astro.stack            écrit la pile
 *   astro pile.txt --maquette scene.txt   les sommets d'une scène de jeu,
 *                                         pour en tirer une image
 *
 * Le test rejoue les règles une à une — tourner, pousser et glisser,
 * l'espace qui se replie, tirer d'une frappe, un rocher qui se casse en
 * deux puis en quatre, le vaisseau qui explose et revient, l'hyperespace,
 * la soucoupe, la vie gagnée, la vague suivante —, puis un ROBOT joue, et
 * l'on vérifie à chaque image que les boutons disent ce que dit l'état.
 *
 * NON VÉRIFIÉ ICI : le rendu, les sons, la vitesse réelle — à jouer DANS HC
 * (l'application). */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* ─── l'hôte ─────────────────────────────────────────────────────────── */

static char g_dernier[512];
static int  g_erreurs = 0;
static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) snprintf(g_dernier, sizeof g_dernier, "%s", t ? t : "");
    else if (k == HC_ERR) { printf("   [ERR] %s\n", t); g_erreurs++; }
}

static char g_sons[2048];
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
    snprintf(s, sizeof s, "%s(%d)", nom, n);
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

/* ─── la pile ────────────────────────────────────────────────────────── */

static Object *g_carte;
static Object *g_roc[40], *g_tir[4], *g_utir[2], *g_eclat[8];
static Object *g_vaisseau, *g_flamme, *g_soucoupe;

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

static void colore(const char *nom, const char *fond, const char *trait)
{
    char s[160];
    snprintf(s, sizeof s, "set the backColor of card button \"%s\" to \"%s\"", nom, fond);
    hc_do(s);
    snprintf(s, sizeof s, "set the foreColor of card button \"%s\" to \"%s\"", nom, trait);
    hc_do(s);
}

/* Un polygone au trait blanc, au fond transparent, caché. */
static Object *trait(const char *nom)
{
    Object *b = pose_bouton(nom, "polygon", 0, 0, 10, 10);
    colore(nom, "0,0,0,0", "255,255,255");
    b->visible = 0;
    return b;
}

static Object *monte(const char *script)
{
    Object *st = hc_new_stack("Astro-Prout");
    st->w = 640; st->h = 480 + 44;
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    g_carte = hc_new_card(st, bg, "Espace");
    hc_set_current_card(g_carte);

    pose_bouton("Cadre", "rectangle", 0, 0, 640, 480);
    colore("Cadre", "0,0,0", "0,0,0");
    char nom[16];
    for (int i = 0; i < 40; i++) { snprintf(nom, sizeof nom, "a%d", i + 1); g_roc[i] = trait(nom); }
    for (int i = 0; i < 8; i++) { snprintf(nom, sizeof nom, "e%d", i + 1); g_eclat[i] = trait(nom); }
    for (int i = 0; i < 4; i++) {
        snprintf(nom, sizeof nom, "t%d", i + 1);
        g_tir[i] = pose_bouton(nom, "oval", 0, 0, 4, 4);
        colore(nom, "255,255,255", "255,255,255");
        g_tir[i]->visible = 0;
    }
    for (int i = 0; i < 2; i++) {
        snprintf(nom, sizeof nom, "u%d", i + 1);
        g_utir[i] = pose_bouton(nom, "oval", 0, 0, 4, 4);
        colore(nom, "255,200,200", "255,200,200");
        g_utir[i]->visible = 0;
    }
    g_soucoupe = trait("soucoupe");
    g_flamme = trait("flamme");
    g_vaisseau = trait("vaisseau");
    g_vaisseau->visible = 1;

    int y = 484;
    hc_set_field_text(pose_champ("Titre", 8, y, 132, 22, 14, HC_BOLD), "ASTRO-PROUT");
    hc_set_field_text(pose_champ("lScore", 144, y + 2, 46, 20, 10, 0), "SCORE");
    pose_champ("score", 190, y, 70, 22, 14, HC_BOLD);
    hc_set_field_text(pose_champ("lBest", 264, y + 2, 38, 20, 10, 0), "BEST");
    hc_set_field_text(pose_champ("best", 302, y, 70, 22, 14, HC_BOLD), "0");
    hc_set_field_text(pose_champ("lVies", 376, y + 2, 40, 20, 10, 0), "SHIPS");
    pose_champ("vies", 416, y, 28, 22, 14, HC_BOLD);
    hc_set_field_text(pose_champ("lVague", 448, y + 2, 40, 20, 10, 0), "WAVE");
    pose_champ("vague", 488, y, 28, 22, 14, HC_BOLD);
    pose_champ("etat", 8, y + 22, 500, 18, 10, HC_ITALIC);
    Object *n = pose_bouton("New game", "roundRect", 640 - 116, y + 2, 108, 26);
    n->showname = 1;
    hc_set_script(n, "on mouseUp\n  nouvellePartie\nend mouseUp\n");

    hc_set_script(st, script);
    return st;
}

/* ─── lire l'état ────────────────────────────────────────────────────── */

static char g_val[16][4096];
static int  g_tour = 0;
static const char *G(const char *nom)
{
    char *v = hc_module_globale(nom);
    char *b = g_val[g_tour++ % 16];
    snprintf(b, sizeof g_val[0], "%s", v ? v : "");
    free(v);
    return b;
}
static double GD(const char *nom) { return atof(G(nom)); }
static void pose(const char *nom, const char *v) { hc_module_pose_globale(nom, v); }

static double item_d(const char *liste, int i)
{
    const char *p = liste;
    for (int k = 1; k < i && p; k++) { p = strchr(p, ','); if (p) p++; }
    return p ? atof(p) : 0;
}

static int cx(const Object *o) { return o->x + o->w / 2; }
static int cy(const Object *o) { return o->y + o->h / 2; }

/* Ce que l'état dit, et ce que les boutons montrent : 0, ou le premier
 * désaccord. */
static int g_detail;
static long g_score_avant = 0;
static int controle(void)
{
    const char *ax = G("gAx"), *ay = G("gAy"), *at = G("gAt");
    for (int i = 1; i <= 40; i++) {
        int t = (int)item_d(at, i);
        Object *b = g_roc[i - 1];
        if (b->visible != (t > 0)) { g_detail = i; return 1; }
        if (t > 0 && (abs(cx(b) - (int)lround(item_d(ax, i))) > 1 || abs(cy(b) - (int)lround(item_d(ay, i))) > 1)) {
            g_detail = i; return 2;
        }
    }
    const char *tx = G("gTx"), *ty = G("gTy"), *tv = G("gTvie");
    for (int i = 1; i <= 4; i++) {
        int v = (int)item_d(tv, i);
        Object *b = g_tir[i - 1];
        if (b->visible != (v > 0)) { g_detail = i; return 3; }
        if (v > 0 && (abs(cx(b) - (int)lround(item_d(tx, i))) > 1 || abs(cy(b) - (int)lround(item_d(ty, i))) > 1)) {
            g_detail = i; return 4;
        }
    }
    if (g_soucoupe->visible != (atoi(G("gSvie")) > 0)) return 5;
    const char *viv = G("gVivant");
    int inv = atoi(G("gInvul"));
    if (!strcmp(viv, "true")) {
        int vis = inv == 0 || (inv / 3) % 2 == 0;
        if (g_vaisseau->visible != vis) return 6;
        if (abs(cx(g_vaisseau) - (int)lround(GD("gX"))) > 7 || abs(cy(g_vaisseau) - (int)lround(GD("gY"))) > 7) return 7;
    } else if (g_vaisseau->visible) return 8;
    long sc = atol(G("gScore"));
    if (sc < g_score_avant || sc % 10) return 9;
    g_score_avant = sc;
    return 0;
}

static int g_defauts = 0;
static void une_image(const char *touches)
{
    g_tenues = touches;
    hc_send(g_carte, "image");
    g_tenues = "";
    int r = controle();
    if (r) { printf("   DÉFAUT %d (%d)\n", r, g_detail); g_defauts++; }
}
static void images(int n, const char *touches) { for (int i = 0; i < n; i++) une_image(touches); }

static int nb_rochers(int taille)
{
    const char *at = G("gAt");
    int n = 0;
    for (int i = 1; i <= 40; i++) {
        int t = (int)item_d(at, i);
        if (taille ? t == taille : t > 0) n++;
    }
    return n;
}

static void etat_vaisseau(const char *quoi)
{
    printf("   %-38s x %.1f y %.1f · vx %.2f vy %.2f · angle %s\n", quoi,
           GD("gX"), GD("gY"), GD("gVx"), GD("gVy"), G("gAng"));
}

/* Une partie neuve sans rochers, ni soucoupe, ni attente. */
static void espace_vide(void)
{
    hc_do("global gScore, gVies, gVague, gBonus\nput 0 into gScore\nput 3 into gVies\n"
          "put 1 into gVague\nput 10000 into gBonus");
    pose("gAlea", "2025");
    hc_send(g_carte, "videTout");
    hc_send(g_carte, "remetVaisseau");
    pose("gInvul", "0");
    pose("gSprochaine", "100000");
    pose("gIssue", "");
    pose("gFeuTenu", "false");
    hc_send(g_carte, "dessine");
    g_score_avant = 0;
    g_sons[0] = 0;
}

/* Un rocher posé à la main, immobile. */
static int roc(double x, double y, int t)
{
    char cmd[128];
    snprintf(cmd, sizeof cmd, "put nouveauRocher(%.1f, %.1f, %d, 0)", x, y, t);
    g_dernier[0] = 0;
    hc_do(cmd);
    int i = atoi(g_dernier);
    hc_send(g_carte, "dessine");
    return i;
}

/* ─── le robot ───────────────────────────────────────────────────────── */

/* Il vise le rocher le plus proche — en tenant compte de l'espace replié —,
 * tire quand il est en face, et saute dans l'hyperespace quand un rocher
 * le frôle. */
static const char *robot(int t)
{
    static char k[64];
    double x = GD("gX"), y = GD("gY"), ang = GD("gAng");
    const char *ax = G("gAx"), *ay = G("gAy"), *at = G("gAt");
    double best = 1e9, bdx = 0, bdy = 0, br = 0;
    for (int i = 1; i <= 40; i++) {
        int ti = (int)item_d(at, i);
        if (!ti) continue;
        double dx = item_d(ax, i) - x, dy = item_d(ay, i) - y;
        if (dx > 320) dx -= 640;
        if (dx < -320) dx += 640;
        if (dy > 240) dy -= 480;
        if (dy < -240) dy += 480;
        double d = dx * dx + dy * dy;
        if (d < best) { best = d; bdx = dx; bdy = dy; br = ti == 3 ? 36 : ti == 2 ? 18 : 9; }
    }
    k[0] = 0;
    if (best > 1e8) return k;
    if (sqrt(best) < br + 22) { strcpy(k, "65364"); return k; }
    double cible = atan2(bdx, -bdy) * 180 / M_PI;
    double ecart = fmod(cible - ang + 540.0, 360.0) - 180.0;
    if (ecart > 6) strcat(k, "65363,");
    else if (ecart < -6) strcat(k, "65361,");
    if (fabs(ecart) < 14 && t % 2 == 0) strcat(k, "32,");
    if (sqrt(best) > 260 && t % 5 == 0) strcat(k, "65362,");
    size_t n = strlen(k);
    if (n && k[n - 1] == ',') k[n - 1] = 0;
    return k;
}

/* ─── le programme ───────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s pile.txt [Astro.stack]\n", argv[0]); return 2; }
    char *script = lire(argv[1]);
    if (!script) return 2;
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.play_sound = mon_son; h.play_notes = mes_notes;
    h.global_get = mon_global;
    hc_set_host(&h);
    Object *st = monte(script);

    /* LA MAQUETTE : une scène de jeu, les sommets de chaque polygone
     * visible en coordonnées de carte, pour en tirer une image. */
    if (argc > 3 && !strcmp(argv[2], "--maquette")) {
        hc_send(g_carte, "openCard");
        pose("gInvul", "0");
        pose("gSprochaine", "200");
        for (int t = 0; t < 260; t++) {
            g_tenues = robot(t);
            hc_send(g_carte, "image");
        }
        g_tenues = "";
        FILE *f = fopen(argv[3], "w");
        if (!f) return 1;
        for (int i = 0; i < g_carte->nparts; i++) {
            Object *o = g_carte->parts[i];
            if (o->type != OBJ_BUTTON || !o->visible || !o->style) continue;
            if (!strcmp(o->style, "oval")) { fprintf(f, "rond %d %d\n", cx(o), cy(o)); continue; }
            if (strcmp(o->style, "polygon")) continue;
            int xy[256];
            int n = hc_bouton_sommets(o, xy, 256);
            fprintf(f, "poly");
            for (int k = 0; k < n; k++) fprintf(f, " %d,%d", xy[2 * k], xy[2 * k + 1]);
            fprintf(f, "\n");
        }
        fclose(f);
        hc_free(st); free(script);
        return 0;
    }

    if (argc > 2) {
        hc_send(g_carte, "openCard");
        if (g_erreurs) { fprintf(stderr, "des erreurs : pile non écrite\n"); return 1; }
        if (hc_save(st, argv[2]) != 0) { fprintf(stderr, "impossible d'écrire %s\n", argv[2]); return 1; }
        printf("pile écrite : %s\n", argv[2]);
        hc_free(st); free(script);
        return 0;
    }

    puts("== 1. à l'ouverture, la première vague dérive ==");
    hc_send(g_carte, "openCard");
    printf("   %d rochers (%d gros) · vaisseau en %d,%d, %d sommets · contrôle %d\n",
           nb_rochers(0), nb_rochers(3), cx(g_vaisseau), cy(g_vaisseau), g_vaisseau->npoints, controle());
    printf("   un rocher : %d sommets, %d × %d\n", g_roc[0]->npoints, g_roc[0]->w, g_roc[0]->h);

    puts("\n== 2. tourner : neuf degrés par image ==");
    espace_vide();
    images(10, "65363");
    etat_vaisseau("dix images à droite");
    printf("   %-38s le vaisseau fait %d × %d\n", "couché sur le flanc", g_vaisseau->w, g_vaisseau->h);
    images(20, "113");
    etat_vaisseau("vingt à gauche (Q)");

    puts("\n== 3. pousser, glisser sur son élan, l'espace qui se replie ==");
    espace_vide();
    images(10, "65362");
    etat_vaisseau("dix images de poussée");
    images(40, "65362");
    etat_vaisseau("encore quarante : plafonné à 9");
    images(30, "");
    etat_vaisseau("trente sans pousser : il ralentit");
    printf("   %-38s flamme visible %d\n", "", g_flamme->visible);

    puts("\n== 4. tirer : une frappe, un tir ; quatre au plus ; ils s'éteignent ==");
    espace_vide();
    images(5, "32");
    printf("   %-38s tirs %s · sons %s\n", "espace tenu cinq images", G("gTvie"), g_sons);
    for (int i = 0; i < 5; i++) { une_image(""); une_image("32"); }
    printf("   %-38s tirs %s\n", "cinq frappes de plus", G("gTvie"));
    images(30, "");
    printf("   %-38s tirs %s\n", "trente images après", G("gTvie"));
    /* une frappe entre deux images compte */
    g_tenues = "32"; hc_send(g_carte, "retiens"); g_tenues = "";
    une_image("");
    printf("   %-38s tirs %s\n", "frappe brève entre deux images", G("gTvie"));

    puts("\n== 5. un rocher touché se casse : gros, moyens, petits ==");
    espace_vide();
    roc(320, 150, 3);
    {
        int n = 0;
        une_image("32");
        while (nb_rochers(3) && n < 30) { une_image(""); n++; }
        printf("   %-38s rochers %d (moyens %d) · score %s · %s\n", "le gros", nb_rochers(0), nb_rochers(2),
               G("gScore"), g_sons);
        hc_send(g_carte, "videTout");
        roc(320, 190, 2);
        une_image("32");
        n = 0;
        while (nb_rochers(2) && n < 30) { une_image(""); n++; }
        printf("   %-38s rochers %d (petits %d) · score %s\n", "un moyen", nb_rochers(0), nb_rochers(1), G("gScore"));
        hc_send(g_carte, "videTout");
        roc(320, 200, 1);
        une_image("32");
        n = 0;
        while (nb_rochers(1) && n < 30) { une_image(""); n++; }
        printf("   %-38s rochers %d · score %s\n", "un petit", nb_rochers(0), G("gScore"));
    }

    puts("\n== 6. un rocher sur le vaisseau : il explose, puis revient ==");
    espace_vide();
    roc(320, 240, 2);
    une_image("");
    printf("   %-38s vivant %s · vaisseaux %s · éclats %s · %s\n", "le choc", G("gVivant"), G("gVies"),
           G("gEvie"), g_sons);
    {
        int n = 0;
        while (strcmp(G("gVivant"), "true") && n < 200) { une_image(""); n++; }
        printf("   %-38s après %d images, en %.0f,%.0f, intouchable %s\n", "il revient", n, GD("gX"), GD("gY"),
               G("gInvul"));
    }

    puts("\n== 7. l'hyperespace ==");
    espace_vide();
    une_image("65364");
    etat_vaisseau("bas : ailleurs, à l'arrêt");
    images(3, "65364");
    etat_vaisseau("bas tenu : un seul saut");

    puts("\n== 8. la soucoupe passe, tire, et se fait abattre ==");
    espace_vide();
    pose("gSprochaine", "1");
    une_image("");
    printf("   %-38s en %.0f,%.0f, vers %s\n", "elle arrive", GD("gSx"), GD("gSy"), G("gSvx"));
    images(30, "");
    printf("   %-38s tirs de la soucoupe %s\n", "trente images plus tard", G("gUvie"));
    /* on la met devant le nez */
    pose("gSx", "320"); pose("gSy", "170"); pose("gSvx", "0"); pose("gSvy", "0");
    pose("gUvie", "0,0");
    hc_send(g_carte, "dessine");
    une_image("32");
    {
        int n = 0;
        while (atoi(G("gSvie")) && n < 20) { une_image(""); n++; }
        printf("   %-38s soucoupe %s · score %s\n", "abattue", G("gSvie"), G("gScore"));
    }

    puts("\n== 9. dix mille points : un vaisseau de plus ==");
    espace_vide();
    pose("gScore", "9990");
    g_score_avant = 9990;
    roc(320, 200, 1);
    une_image("32");
    { int n = 0; while (nb_rochers(1) && n < 30) { une_image(""); n++; } }
    printf("   %-38s score %s · vaisseaux %s · prochain bonus %s\n", "un petit rocher", G("gScore"), G("gVies"),
           G("gBonus"));

    puts("\n== 10. plus un rocher : la vague suivante ==");
    espace_vide();
    {
        int n = 0;
        while (nb_rochers(0) == 0 && n < 100) { une_image(""); n++; }
        printf("   %-38s après %d images : vague %s, %d gros rochers\n", "l'espace vide", n, G("gVague"), nb_rochers(3));
    }

    /* LE ROBOT : deux parties, 1 500 images au plus — sous ASan, 3 000
     * prenaient trois quarts de minute. À chaque image, le contrôle. */
    puts("\n== 11. un robot joue, et l'écran suit l'état ==");
    {
        int images_tot = 0, parties = 0, vagues = 0, morts = 0;
        for (int p = 0; p < 2 && images_tot < 1500; p++) {
            espace_vide();
            char a[16];
            snprintf(a, sizeof a, "%d", 3000 + 77 * p);
            pose("gAlea", a);
            pose("gSprochaine", "400");
            hc_do("nouvelleVague 1");
            hc_send(g_carte, "dessine");
            int vies = 3, vague = 1;
            while (images_tot < 1500 && !*G("gIssue")) {
                une_image(robot(images_tot));
                images_tot++;
                int v = atoi(G("gVies"));
                if (v < vies) morts++;
                vies = v;
                int w = atoi(G("gVague"));
                if (w > vague) { vagues++; vague = w; }
            }
            parties++;
        }
        printf("   %d partie(s), %d images, %d vague(s) passée(s), %d vaisseau(x) perdu(s) · défauts %d\n",
               parties, images_tot, vagues, morts, g_defauts);
    }

    puts("\n== 12. la vraie boucle : Échap arrête la partie ==");
    {
        g_horloge = 1;
        clock_gettime(CLOCK_MONOTONIC, &g_t0);
        g_lectures = 0; g_echap_a = 30; g_filet = 20000;
        hc_send(g_carte, "nouvellePartie");
        g_dernier[0] = 0;
        hc_do("put card field \"etat\"");
        printf("   %s · %s\n", g_lectures < g_filet ? "arrêtée par Échap" : "Échap SANS EFFET", g_dernier);
        g_echap_a = 0; g_filet = 0; g_horloge = 0;
    }

    printf("\n== bilan : %d défaut(s), %d erreur(s) de script ==\n", g_defauts, g_erreurs);
    hc_free(st); free(script);
    return g_defauts || g_erreurs ? 1 : 0;
}
