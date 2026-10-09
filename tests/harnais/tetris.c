/* tetris — UN TETRIS EN HYPERTALK, MONTÉ PAR LE CODE, pour le DMG.
 *
 * La pièce qui tombe est UN bouton polygone, « Piece » : sa forme vient de
 * « the points », sa couleur de « backColor » — les deux extensions des
 * boutons, comme le flipper. Les pièces posées sont deux cents boutons
 * carrés, « C1 » à « C200 ». Tout le jeu vit dans le script de la pile,
 * tests/donnees/tetris_pile.txt.
 *
 *   tetris pile.txt                le test
 *   tetris pile.txt Tetris.stack   écrit la pile, pour le DMG
 *
 * Le test ne lance pas la boucle de jeu — elle attend le clavier et les
 * ticks : il impose les pièces (« nouvellePartie "O,O,…" ») et joue les
 * gestes un par un. Puis un ROBOT pose des centaines de pièces au hasard
 * (un hasard fixé, pour un témoin stable) et l'on vérifie à chaque pièce
 * que rien ne se perd : vingt lignes de dix cases, et autant de cases
 * pleines que 4 × pièces posées − 10 × lignes faites.
 *
 * NON VÉRIFIÉ ICI : le rendu à l'écran, le clavier et la vitesse — à jouer
 * DANS HC (l'application). */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char g_dernier[512];
static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) { snprintf(g_dernier, sizeof g_dernier, "%s", t ? t : ""); }
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

/* Les touches tenues que l'hôte prétend voir, pour la section 6. */
static const char *g_tenues = "";
static const char *glob_lit(const char *nom)
{
    return strcmp(nom, "keysDown") == 0 ? g_tenues : NULL;
}

/* Le clavier de la vraie boucle : rien, puis Échap tenu, avec le code que
 * HC lui donne — 65307, celui de LiveCode (hcv_code_touche, HCview.m). À la
 * lecture FILET, le filet fait ce que ferait Cmd-. : si Échap n'arrête pas
 * la partie, le défaut se lit au lieu de bloquer la suite. */
#define FILET 400
static int g_lectures = 0;
static const char *glob_echap(const char *nom)
{
    if (strcmp(nom, "keysDown")) return NULL;
    if (++g_lectures == FILET) hc_interrompre();
    return g_lectures >= 20 ? "65307" : "";
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

static Object *g_carte;

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
    free(b->style); b->style = strdup(style);
    return b;
}

/* La carte fait 512 × 342. Le plateau : dix colonnes de 15 points depuis
 * x = 40, vingt rangées depuis y = 20, soit 150 × 300. À droite, le titre,
 * le score, les lignes, l'état et le bouton de partie. */
static Object *monte(const char *script)
{
    Object *st = hc_new_stack("Tetris");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    g_carte = hc_new_card(st, bg, "Tetris");

    Object *cadre = pose_bouton("Cadre", "rectangle", 38, 18, 154, 304);
    cadre->showname = 0;
    for (int l = 0; l < 20; l++)
        for (int c = 0; c < 10; c++) {
            char nom[8];
            snprintf(nom, sizeof nom, "C%d", l * 10 + c + 1);
            Object *b = pose_bouton(nom, "rectangle", 40 + c * 15, 20 + l * 15, 15, 15);
            b->showname = 0;
            b->visible = 0;
        }
    Object *p = pose_bouton("Piece", "polygon", 85, 20, 45, 30);
    p->showname = 0;

    Object *t = pose_champ("Titre", 230, 20, 250, 40, 28, HC_BOLD);
    hc_set_field_text(t, "TETRIS");
    hc_set_field_text(pose_champ("LScore", 230, 80, 80, 20, 12, 0), "Score");
    pose_champ("Score", 320, 80, 120, 20, 12, HC_BOLD);
    hc_set_field_text(pose_champ("LLignes", 230, 104, 80, 20, 12, 0), "Lignes");
    pose_champ("Lignes", 320, 104, 120, 20, 12, HC_BOLD);
    pose_champ("Etat", 230, 134, 250, 40, 12, HC_ITALIC);
    hc_set_field_text(pose_champ("Aide", 230, 180, 250, 80, 10, 0),
        "Q et D : bouger\nZ : tourner\nX : descendre\n"
        "Espace : lacher\nEchap : arreter\n(les fleches marchent aussi)");
    Object *n = pose_bouton("Nouvelle partie", "roundRect", 230, 280, 150, 26);
    hc_set_script(n, "on mouseUp\n  nouvellePartie\nend mouseUp\n");

    hc_set_script(st, script);
    hc_set_current_card(g_carte);
    return st;
}

static const char *global_txt(const char *nom)
{
    static char b[4096];
    char s[128];
    snprintf(s, sizeof s, "global %s\nput %s", nom, nom);
    g_dernier[0] = 0;
    hc_do(s);
    snprintf(b, sizeof b, "%s", g_dernier);
    return b;
}

static void grille(void)
{
    char g[512];
    snprintf(g, sizeof g, "%s", global_txt("gGrille"));
    char *l = strtok(g, "\n");
    while (l) { if (strchr(l, '#') == NULL && strspn(l, ".") != strlen(l)) printf("      |%s|\n", l); l = strtok(NULL, "\n"); }
}

static void rapport(void)
{
    g_dernier[0] = 0;
    hc_send(g_carte, "rapport");
    printf("   %s\n", g_dernier);
}

static void gestes(const char *suite)
{
    for (const char *p = suite; *p; p++) {
        const char *m = *p == 'g' ? "gauche" : *p == 'd' ? "droite" : *p == 't' ? "tourne"
                      : *p == 'b' ? "descend" : *p == 'c' ? "chute" : NULL;
        if (m) hc_send(g_carte, m);
    }
}

static Object *bouton(const char *nom)
{
    for (int i = 0; i < g_carte->nparts; i++)
        if (g_carte->parts[i]->type == OBJ_BUTTON && !strcmp(g_carte->parts[i]->name, nom))
            return g_carte->parts[i];
    return NULL;
}

/* La grille est-elle saine, et le compte des cases tient-il ? */
static int controle(int *lignes, int *posees)
{
    char g[512];
    snprintf(g, sizeof g, "%s", global_txt("gGrille"));
    int nl = 0, pleines = 0, bad = 0;
    for (char *l = strtok(g, "\n"); l; l = strtok(NULL, "\n")) {
        nl++;
        if (strlen(l) != 10) bad = 1;
        for (char *q = l; *q; q++) if (*q != '.') pleines++;
    }
    *lignes = atoi(global_txt("gLignes"));
    *posees = atoi(global_txt("gPosees"));
    /* Les cases montrées à l'écran doivent être exactement les pleines. */
    int montrees = 0;
    for (int i = 1; i <= 200; i++) {
        char nom[8]; snprintf(nom, sizeof nom, "C%d", i);
        Object *b = bouton(nom);
        if (b && b->visible) montrees++;
    }
    if (nl != 20 || bad) return -1;
    if (pleines != 4 * *posees - 10 * *lignes) return -2;
    if (montrees != pleines) return -3;
    return pleines;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s pile.txt [Tetris.stack]\n", argv[0]); return 2; }
    char *script = lire(argv[1]);
    if (!script) return 2;
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = monte(script);

    if (argc > 2) {
        hc_send(g_carte, "openCard");
        if (hc_save(st, argv[2]) != 0) { fprintf(stderr, "impossible d'écrire %s\n", argv[2]); return 1; }
        printf("pile écrite : %s\n", argv[2]);
        hc_free(st); free(script);
        return 0;
    }

    puts("== 1. à l'ouverture, une partie se prépare ==");
    hc_send(g_carte, "openCard");
    rapport();

    puts("\n== 2. cinq carrés côte à côte : deux lignes d'un coup ==");
    hc_send_arg(g_carte, "nouvellePartie", "O,O,O,O,O,T");
    gestes("gggc" "gc" "dc" "dddc" "dddddc");
    grille();
    rapport();

    puts("\n== 3. tourner quatre fois revient au départ ; les murs tiennent ==");
    hc_send_arg(g_carte, "nouvellePartie", "T,I");
    gestes("tttt");
    rapport();
    gestes("gggggggggggg");
    rapport();
    gestes("ddddddddddddd");
    rapport();
    puts("   -- le I debout contre le mur droit, puis tourné : il se décale");
    gestes("c" "t" "dddddddd" "t");
    rapport();

    puts("\n== 4. la forme du bouton polygone suit la pièce ==");
    hc_send_arg(g_carte, "nouvellePartie", "L");
    Object *p = bouton("Piece");
    for (int r = 0; r < 4; r++) {
        printf("   rotation %d : rect %d,%d %dx%d, %d sommets\n", r, p->x, p->y, p->w, p->h, p->npoints);
        gestes("t");
    }

    puts("\n== 5. un robot pose des pièces, et rien ne se perd ==");
    unsigned graine = 12345;
    int parties = 0, pieces = 0, lignes_tot = 0, defaut = 0;
    while (parties < 5) {
        char suite[2048] = "";
        for (int i = 0; i < 300; i++) {
            graine = graine * 1103515245u + 12345u;
            char t[3] = { "IOTSZJL"[(graine >> 16) % 7], ',', 0 };
            strcat(suite, t);
        }
        suite[strlen(suite) - 1] = 0;
        hc_send_arg(g_carte, "nouvellePartie", suite);
        for (int k = 0; k < 300; k++) {
            if (!strcmp(global_txt("gFini"), "true")) break;
            graine = graine * 1103515245u + 12345u;
            int rot = (graine >> 16) % 4, dx = (int)((graine >> 20) % 10) - 5;
            for (int i = 0; i < rot; i++) hc_send(g_carte, "tourne");
            for (int i = 0; i < abs(dx); i++) hc_send(g_carte, dx < 0 ? "gauche" : "droite");
            hc_send(g_carte, "chute");
            int lg, po;
            int r = controle(&lg, &po);
            if (r < 0) { printf("   DÉFAUT %d après %d pièces\n", r, po); defaut++; break; }
        }
        int lg, po;
        controle(&lg, &po);
        pieces += po; lignes_tot += lg; parties++;
    }
    printf("   %d parties, %d pièces posées, %d lignes faites, %s\n",
           parties, pieces, lignes_tot, defaut ? "DES DÉFAUTS" : "aucun défaut");

    /* La boucle de jeu ne tourne pas ici : on lui joue un tour de clavier,
     * avec les touches tenues que l'hôte annonce. Les lettres comptent comme
     * les flèches, en minuscule comme en majuscule. */
    puts("\n== 6. le clavier : Q D Z X, ou les flèches ==");
    h.global_get = glob_lit; hc_set_host(&h);
    static const struct { const char *tenues, *quoi; } tours[] = {
        { "",            "rien de tenu" },
        { "100",         "d : à droite" },
        { "90",          "Z majuscule : tourne" },
        { "113,120",     "q et x ensemble : à gauche et descend" },
        { "65363",       "flèche droite" },
    };
    hc_send_arg(g_carte, "nouvellePartie", "T");
    for (size_t i = 0; i < sizeof tours / sizeof *tours; i++) {
        g_tenues = tours[i].tenues;
        hc_send_arg(g_carte, "clavier", "0");
        g_tenues = "";
        hc_send_arg(g_carte, "clavier", "0");  /* relâchée : la suivante agit */
        printf("   %s :\n", tours[i].quoi);
        rapport();
    }

    /* ÉCHAP ARRÊTE LA PARTIE, DANS LA VRAIE BOUCLE. La pile attendait 27,
     * charToNum du caractère ; HC donne 65307, le code de la touche. Échap
     * n'arrêtait donc rien, et seul Cmd-. sortait d'une partie. Trouvé le 9
     * octobre en cherchant pourquoi le Space Invaders et le casse-briques ne
     * répondaient plus aux touches DANS HC (l'application) ; le même défaut
     * était dans les trois jeux. Les sections précédentes jouent les tours à
     * la main et ne passaient jamais par la boucle : ici elle tourne pour de
     * bon, avec ses « wait 1 tick ». */
    puts("\n== 7. Échap arrête la partie, dans la vraie boucle ==");
    h.global_get = glob_echap; hc_set_host(&h);
    g_lectures = 0;
    hc_send(g_carte, "nouvellePartie");
    {
        char etat[sizeof g_dernier];
        g_dernier[0] = 0;
        hc_do("put card field \"Etat\"");
        snprintf(etat, sizeof etat, "%s", g_dernier);
        printf("   %s · Etat « %s » · fini %s\n",
               g_lectures < FILET ? "arrêtée par Échap"
                                  : "Échap SANS EFFET, arrêtée par le filet",
               etat, global_txt("gFini"));
    }

    hc_free(st); free(script);
    return 0;
}
