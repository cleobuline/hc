/* boulder — BOULDER-PROUT, UN BOULDER DASH MAISON EN HYPERTALK, MONTÉ PAR LE CODE.
 *
 * Demandé par l'utilisatrice le 25 octobre : « on se fait un boulder dash
 * maison ? ». Une creuseuse à la place du mineur ; du jeu d'origine, rien
 * n'est repris que l'idée. Les 240 cases sont des boutons transparents qui
 * portent une icône en couleur (16 × 16, doublée) ; tout le jeu vit dans le
 * script de la pile, tests/donnees/boulder_pile.txt. Les icônes et les
 * cavernes sont dans boulder_icones.txt et boulder_niveaux.txt, en texte :
 * elles se retouchent à la main.
 *
 *   boulder pile.txt icones.txt niveaux.txt                  le test
 *   boulder pile.txt icones.txt niveaux.txt Boulder.stack    écrit la pile,
 *                                                            pour le DMG et le site
 *   … Boulder.stack boulder_banc.txt                         la même, avec le
 *                                                            bouton Bench
 *
 * C'est ce jeu qui a montré que HC dessinait ses icônes en couleur point par
 * point : « pas jouable » avec la 0.8.7, jouable avec la 0.8.8. Le banc du
 * bouton Bench est décrit dans docs/mesures/dessin_icones.txt.
 *
 * Le test rejoue des situations posées à la main — une chute, un
 * écrasement, un rocher sur la tête, une roulade, une poussée, la sortie, une
 * luciole et un papillon écrasés, une bête qui touche — puis un ROBOT creuse
 * les trois cavernes, et l'on vérifie à CHAQUE image que les 240 icônes, la
 * table des chutes, la place de la creuseuse et la liste des bêtes disent la
 * même chose que le plateau. Enfin la vraie boucle tourne, avec son horloge :
 * le temps qui s'épuise, et Échap.
 *
 * NON VÉRIFIÉ ICI : le rendu, les sons, la vitesse réelle — à jouer DANS HC
 * (l'application). */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

/* Le clavier et l'horloge. Sans horloge, « the ticks » reste au repli du
 * noyau ; avec, il suit le temps réel, comme dans l'application. Le FILET
 * fait ce que ferait Cmd-. si une boucle ne s'arrêtait pas. */
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

/* La ligne suivante de *p, sans son retour ; NULL à la fin. */
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

/* ─── les cavernes ───────────────────────────────────────────────────── */

typedef struct { int n, besoin, secondes; char texte[16 + 12 * 21 + 256]; } Niveau;
static Niveau g_niv[9];
static int g_nniv = 0;

static int charge_niveaux(const char *chemin)
{
    char *t = lire(chemin);
    if (!t) return -1;
    char *p = t, *l;
    while ((l = ligne_suivante(&p))) {
        if (!strncmp(l, "--", 2) || !*l) continue;
        if (!strncmp(l, "conseil ", 8) && g_nniv > 0) {
            Niveau *pr = &g_niv[g_nniv - 1];
            size_t k = strlen(pr->texte);
            snprintf(pr->texte + k, sizeof pr->texte - k, "\n%s", l + 8);
            continue;
        }
        Niveau *nv = &g_niv[g_nniv];
        if (g_nniv >= 9 || sscanf(l, "niveau %d %d %d", &nv->n, &nv->besoin, &nv->secondes) != 3) goto faute;
        int k = snprintf(nv->texte, sizeof nv->texte, "%d,%d", nv->besoin, nv->secondes);
        for (int r = 0; r < 12; r++) {
            char *a = ligne_suivante(&p);
            if (!a || strlen(a) != 20) goto faute;
            k += snprintf(nv->texte + k, sizeof nv->texte - (size_t)k, "\n%s", a);
        }
        g_nniv++;
    }
    free(t);
    return g_nniv;
faute:
    fprintf(stderr, "niveaux : ligne illisible « %s »\n", l ? l : "(fin)");
    free(t);
    return -1;
}

/* ─── la pile ────────────────────────────────────────────────────────── */

static Object *g_carte;
static Object *g_cases[240];
static char *g_banc = NULL;       /* le script du bouton Bench, s'il y en a un */

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
    free(b->style); b->style = strdup(style);
    return b;
}

/* La carte fait 640 × 460 : la caverne, 20 × 12 cases de 32 points, de 0 à
 * 384 ; dessous, la barre d'état. Le fond noir est le bouton 1, posé AVANT
 * les cases pour être dessiné dessous ; une case vide n'a pas d'icône. */
static Object *monte(const char *script)
{
    Object *st = hc_new_stack("Boulder-Prout");
    st->w = 640; st->h = 460;
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    g_carte = hc_new_card(st, bg, "Caverne");

    pose_bouton("Cadre", "rectangle", 0, 0, 640, 384);
    for (int k = 0; k < 240; k++) {
        char nom[16];
        snprintf(nom, sizeof nom, "c%d", k + 1);
        g_cases[k] = pose_bouton(nom, "transparent", (k % 20) * 32, (k / 20) * 32, 32, 32);
        g_cases[k]->autohilite = 0;
    }

    Object *t = pose_champ("Titre", 8, 390, 164, 22, 14, HC_BOLD);
    hc_set_field_text(t, "BOULDER-PROUT");
    pose_champ("niveau",   176, 392, 70, 20, 12, HC_BOLD);
    pose_champ("diamants", 250, 392, 120, 20, 12, 0);
    pose_champ("temps",    374, 392, 80, 20, 12, 0);
    pose_champ("vies",     458, 392, 70, 20, 12, 0);
    pose_champ("score",    532, 392, 100, 20, 12, HC_BOLD);
    pose_champ("etat",     8, 418, 500, 20, 12, HC_ITALIC);
    hc_set_field_text(pose_champ("aide", 8, 438, 500, 18, 10, 0),
        "A digger, boulders and diamonds. Falling boulders crush, resting ones don't.");
    for (int i = 0; i < g_nniv; i++) {
        char nom[16];
        snprintf(nom, sizeof nom, "level %d", g_niv[i].n);
        Object *f = pose_champ(nom, 8, 8 + 20 * i, 200, 200, 9, 0);
        f->visible = 0;
        hc_set_field_text(f, g_niv[i].texte);
    }
    Object *n = pose_bouton("New game", "roundRect", 516, 414, 116, 26);
    n->showname = 1;
    hc_set_script(n, "on mouseUp\n  nouvellePartie\nend mouseUp\n");
    if (g_banc) {
        Object *bb = pose_bouton("Bench", "roundRect", 516, 386, 116, 24);
        bb->showname = 1;
        hc_set_script(bb, g_banc);
    }

    hc_set_script(st, script);
    hc_set_current_card(g_carte);
    hc_do("set the backColor of card button 1 to \"0,0,0\"");
    hc_do("set the foreColor of card button 1 to \"0,0,0\"");
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

static int g_plat_ok;
static char g_plat[12][21];
static void lit_plat(void)
{
    const char *p = G("gPlat");
    g_plat_ok = 1;
    for (int r = 0; r < 12; r++) {
        const char *fin = strpbrk(p, "\r\n");
        size_t n = fin ? (size_t)(fin - p) : strlen(p);
        if (n != 20) { g_plat_ok = 0; return; }
        memcpy(g_plat[r], p, 20); g_plat[r][20] = 0;
        p = fin ? fin + 1 : p + n;
    }
}

static int icone_de(char ch, int ouvert, int phase)
{
    switch (ch) {
    case '#': return 2001;  case '=': return 2002;  case '.': return 2003;
    case 'O': return 2004;  case '*': return 2005;  case 'P': return 2006;
    case 'E': return !ouvert ? 2007 : phase == 0 ? 2008 : 2012;
    case 'F': return phase == 0 ? 2009 : 2013;
    case 'B': return phase == 0 ? 2010 : 2014;
    case 'x': return 2011;  case 'y': return 2015;
    default:  return 0;
    }
}

/* L'écran, la table des chutes, la creuseuse et les bêtes disent-ils la
 * même chose que le plateau ? 0, ou le numéro du premier désaccord. */
static int g_detail_r, g_detail_c;
static int controle(void)
{
    lit_plat();
    if (!g_plat_ok) return 1;
    int ouvert = !strcmp(G("gOuvert"), "true");
    int phase = atoi(G("gPhase"));
    for (int k = 0; k < 240; k++) {
        int r = k / 20, c = k % 20;
        if (g_cases[k]->icon != icone_de(g_plat[r][c], ouvert, phase)) {
            g_detail_r = r + 1; g_detail_c = c + 1;
            return 2;
        }
    }
    const char *ch = G("gChute");
    for (int r = 0, i = 0; r < 12; r++, i++) {
        for (int c = 0; c < 20; c++, i++) {
            char f = ch[i];
            if (f != '0' && f != '1') return 3;
            if (f == '1' && g_plat[r][c] != 'O' && g_plat[r][c] != '*') {
                g_detail_r = r + 1; g_detail_c = c + 1;
                return 3;
            }
        }
        if (r < 11 && ch[i] != '\n' && ch[i] != '\r') return 3;
    }
    int np = 0, pr = 0, pc = 0;
    for (int r = 0; r < 12; r++)
        for (int c = 0; c < 20; c++)
            if (g_plat[r][c] == 'P') { np++; pr = r + 1; pc = c + 1; }
    const char *j = G("gJoueur");
    if (*j) {
        if (np != 1 || atoi(j) != pr || atoi(strchr(j, ',') ? strchr(j, ',') + 1 : "0") != pc) return 4;
    } else if (np != 0) return 4;
    /* chaque bête du plateau est dans la liste, une fois, et rien d'autre */
    const char *e = G("gEnnemis");
    int vues[12][20] = { { 0 } }, nliste = 0;
    while (*e) {
        int r, c, d; char t;
        if (sscanf(e, "%d,%d,%d,%c", &r, &c, &d, &t) != 4) return 5;
        if (r < 1 || r > 12 || c < 1 || c > 20) return 5;
        if (g_plat[r - 1][c - 1] == t) { vues[r - 1][c - 1]++; nliste++; }
        e = strchr(e, '\n');
        if (!e) break;
        e++;
    }
    int nplat = 0;
    for (int r = 0; r < 12; r++)
        for (int c = 0; c < 20; c++)
            if (g_plat[r][c] == 'F' || g_plat[r][c] == 'B') {
                nplat++;
                if (vues[r][c] != 1) { g_detail_r = r + 1; g_detail_c = c + 1; return 5; }
            }
    if (nliste != nplat) return 5;
    return 0;
}

/* ─── jouer des images à la main ─────────────────────────────────────── */

static const char *code_de(char k)
{
    switch (k) {
    case 'h': return "65362";     /* haut   */
    case 'b': return "65364";     /* bas    */
    case 'g': return "65361";     /* gauche */
    case 'd': return "65363";     /* droite */
    case 'z': return "122";       case 'q': return "113";
    case 's': return "115";       case 'a': return "97";
    case 'w': return "119";
    case 'r': return "114";
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
    if (r) {
        printf("   DÉFAUT %d (case %d,%d)\n", r, g_detail_r, g_detail_c);
        g_defauts++;
    }
}

/* Installe un plateau de 12 lignes et remet l'issue à zéro. */
static void installe(const char *const lignes[12], int besoin)
{
    char plat[12 * 21 + 1] = "";
    for (int r = 0; r < 12; r++) {
        strcat(plat, lignes[r]);
        if (r < 11) strcat(plat, "\n");
    }
    char b[16];
    snprintf(b, sizeof b, "%d", besoin);
    hc_module_pose_globale("gBesoin", b);
    hc_module_pose_globale("gLimite", "99");
    hc_module_pose_globale("gIssue", "");
    hc_module_pose_globale("gScore", "0");
    hc_send_arg(g_carte, "installe", plat);
    g_sons[0] = 0;
    int r = controle();
    if (r) { printf("   DÉFAUT %d à l'installation\n", r); g_defauts++; }
}

/* Un libellé et ses espaces, sur `largeur` caractères — comptés en
 * caractères et non en octets : « départ » en fait six et sept octets, et
 * %-12s décalait d'un cran tout ce qui porte un accent. */
static void libelle(const char *s, int largeur)
{
    int n = 0;
    for (const char *p = s; *p; p++)
        if (((unsigned char)*p & 0xC0) != 0x80) n++;
    printf("%s%*s", s, largeur > n ? largeur - n : 0, "");
}

/* Les rangées r1..r2, colonnes c1..c2 : ce que montre le plateau. */
static void montre(const char *quoi, int r1, int r2, int c1, int c2)
{
    lit_plat();
    printf("   ");
    libelle(quoi, 12);
    for (int r = r1; r <= r2; r++) {
        printf("|");
        for (int c = c1; c <= c2; c++) putchar(g_plat[r - 1][c - 1] == ' ' ? '_' : g_plat[r - 1][c - 1]);
    }
    printf("|\n");
}

/* Plusieurs images, montrées chacune. */
static void joue_et_montre(const char *touches, int r1, int r2, int c1, int c2)
{
    montre("départ", r1, r2, c1, c2);
    for (const char *t = touches; *t; t++) {
        char quoi[16];
        une_image(*t);
        snprintf(quoi, sizeof quoi, "%c", *t == '.' ? '-' : *t);
        montre(quoi, r1, r2, c1, c2);
    }
}

#define BORD "####################"
#define TERRE "#..................#"

/* ─── le robot ───────────────────────────────────────────────────────── */

/* Il cherche le plus court chemin vers le diamant le plus proche — la sortie
 * quand elle est ouverte —, en évitant ce qui tue : les abords d'une bête,
 * une case sous un rocher qui tombe, et descendre sous un rocher posé (il
 * suivrait). Bloqué trop longtemps, il recommence le niveau (R). */
static int g_chute[12][20];
static void lit_chutes(void)
{
    const char *ch = G("gChute");
    for (int r = 0, i = 0; r < 12; r++, i++)
        for (int c = 0; c < 20; c++, i++)
            g_chute[r][c] = ch[i] == '1';
}

static int bete(int r, int c)
{
    return r >= 0 && r < 12 && c >= 0 && c < 20 && (g_plat[r][c] == 'F' || g_plat[r][c] == 'B');
}

static int dangereuse(int r, int c)
{
    static const int d4[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
    for (int k = 0; k < 4; k++) {
        int rr = r + d4[k][0], cc = c + d4[k][1];
        if (bete(rr, cc)) return 1;
        /* une case vide à côté, où une bête peut entrer à l'image suivante */
        if (rr >= 0 && rr < 12 && cc >= 0 && cc < 20 && g_plat[rr][cc] == ' ')
            for (int j = 0; j < 4; j++)
                if (bete(rr + d4[j][0], cc + d4[j][1])) return 1;
    }
    if (r > 0 && (g_plat[r - 1][c] == 'O' || g_plat[r - 1][c] == '*') && g_chute[r - 1][c]) return 1;
    if (r > 1 && g_plat[r - 1][c] == ' ' && (g_plat[r - 2][c] == 'O' || g_plat[r - 2][c] == '*')
        && g_chute[r - 2][c]) return 1;
    return 0;
}

static int g_vise_r = -1, g_vise_c = -1;     /* une case à atteindre, en 0..11, 0..19 */
static char robot_choisit(int ouvert, unsigned *graine, int *bloque)
{
    int pr = -1, pc = -1;
    for (int r = 0; r < 12; r++)
        for (int c = 0; c < 20; c++)
            if (g_plat[r][c] == 'P') { pr = r; pc = c; }
    if (pr < 0) return '.';
    static const int d4[4][2] = { { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 } };
    static const char tk[4] = { 'g', 'd', 'h', 'b' };
    int dist[12][20], prem[12][20];
    for (int r = 0; r < 12; r++) for (int c = 0; c < 20; c++) dist[r][c] = -1;
    int file[240][2], tete = 0, queue = 0;
    dist[pr][pc] = 0; prem[pr][pc] = -1;
    file[queue][0] = pr; file[queue][1] = pc; queue++;
    int cible = -1;
    while (tete < queue) {
        int r = file[tete][0], c = file[tete][1]; tete++;
        char ici = g_plat[r][c];
        int but = g_vise_r >= 0 ? (r == g_vise_r && c == g_vise_c)
                                : ((!ouvert && ici == '*') || (ouvert && ici == 'E'));
        if ((r != pr || c != pc) && but) {
            cible = prem[r][c];
            break;
        }
        for (int k = 0; k < 4; k++) {
            int rr = r + d4[k][0], cc = c + d4[k][1];
            if (rr < 0 || rr >= 12 || cc < 0 || cc >= 20 || dist[rr][cc] >= 0) continue;
            char v = g_plat[rr][cc];
            if (!(v == ' ' || v == '.' || v == '*' || (v == 'E' && ouvert))) continue;
            if (dangereuse(rr, cc)) continue;
            /* descendre sous un rocher posé : il suivrait */
            if (k == 3 && r > 0 && (g_plat[r - 1][c] == 'O' || g_plat[r - 1][c] == '*')) continue;
            dist[rr][cc] = dist[r][c] + 1;
            prem[rr][cc] = prem[r][c] < 0 ? k : prem[r][c];
            file[queue][0] = rr; file[queue][1] = cc; queue++;
        }
    }
    if (cible >= 0) { *bloque = 0; return tk[cible]; }
    /* Rien d'accessible : pousser un rocher, ou creuser au hasard, sans aller
     * là où ça tue. */
    (*bloque)++;
    *graine = *graine * 1103515245u + 12345u;
    int k = (int)((*graine >> 16) % 4);
    int rr = pr + d4[k][0], cc = pc + d4[k][1];
    if (dangereuse(rr, cc)) return '.';
    if (k == 3 && pr > 0 && (g_plat[pr - 1][pc] == 'O' || g_plat[pr - 1][pc] == '*')) return '.';
    return tk[k];
}

/* Le robot joue une caverne jusqu'à en sortir, au plus `essais_max` fois ;
 * rend 1 s'il est sorti. */
static int robot_caverne(int n, int essais_max, unsigned *graine, int *essais, int *morts, int *images)
{
    *essais = *morts = *images = 0;
    while (*essais < essais_max) {
        char num[8];
        snprintf(num, sizeof num, "%d", n);
        hc_send_arg(g_carte, "chargeNiveau", num);
        hc_module_pose_globale("gIssue", "");
        (*essais)++;
        int bloque = 0;
        for (int t = 0; t < 3000; t++) {
            lit_plat(); lit_chutes();
            int ouvert = !strcmp(G("gOuvert"), "true");
            char k = bloque > 60 ? 'r' : robot_choisit(ouvert, graine, &bloque);
            une_image(k);
            (*images)++;
            const char *issue = G("gIssue");
            if (!strcmp(issue, "won")) return 1;
            if (!strcmp(issue, "dead")) {
                (*morts)++;
                if (getenv("BOULDER_MORTS")) {
                    lit_plat();
                    printf("   — mort à l'image %d :\n", t);
                    for (int r = 0; r < 12; r++) printf("     %s\n", g_plat[r]);
                }
                break;
            }
        }
    }
    return 0;
}

/* ─── le programme ───────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s pile.txt icones.txt niveaux.txt [Boulder.stack [boulder_banc.txt]]\n", argv[0]);
        return 2;
    }
    char *script = lire(argv[1]);
    if (!script) return 2;
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.play_sound = mon_son; h.play_notes = mes_notes;
    h.global_get = mon_global;
    hc_set_host(&h);
    if (charge_niveaux(argv[3]) <= 0) return 2;
    if (argc > 5 && !(g_banc = lire(argv[5]))) return 2;
    Object *st = monte(script);
    int nic = charge_icones(st, argv[2]);
    if (nic <= 0) return 2;

    if (argc > 4) {
        hc_send(g_carte, "openCard");
        if (g_erreurs) { fprintf(stderr, "des erreurs : pile non écrite\n"); return 1; }
        if (hc_save(st, argv[4]) != 0) { fprintf(stderr, "impossible d'écrire %s\n", argv[4]); return 1; }
        hc_free(st); free(script);

        /* RELUE : la pile écrite se rouvre, ses icônes sont en couleur, et
         * le robot y sort de la première caverne. */
        Object *rl = hc_load(argv[4]);
        if (!rl) { fprintf(stderr, "relecture impossible : %s\n", hc_load_erreur()); return 1; }
        hc_register_stack(rl);
        Object *ca = NULL;
        for (int i = 0; i < rl->nparts && !ca; i++)
            if (rl->parts[i]->type == OBJ_CARD) ca = rl->parts[i];
        if (!ca) { fprintf(stderr, "relecture : pas de carte\n"); return 1; }
        int nb = 0;
        for (int i = 0; i < ca->nparts; i++)
            if (ca->parts[i]->type == OBJ_BUTTON) {
                if (nb >= 1 && nb <= 240) g_cases[nb - 1] = ca->parts[i];
                nb++;
            }
        g_carte = ca;
        int coul = 0;
        for (int i = 0; i < hc_icon_count(rl); i++)
            if (hc_icon_couleur(hc_icon_at(rl, i))) coul++;
        hc_set_current_card(ca);
        hc_send(ca, "openCard");
        int r = controle();
        unsigned graine = 7;
        int essais, morts, images;
        int gagne = robot_caverne(1, 3, &graine, &essais, &morts, &images);
        printf("pile écrite et relue : %s — %d × %d, %d boutons, %d icônes en couleur sur %d, "
               "contrôle %d, caverne 1 %s (%d images), défauts %d, erreurs %d\n",
               argv[4], rl->w, rl->h, nb, coul, hc_icon_count(rl), r,
               gagne ? "sortie" : "PAS SORTIE", images, g_defauts, g_erreurs);
        hc_free(rl);
        return (r || !gagne || g_defauts || g_erreurs) ? 1 : 0;
    }

    puts("== 1. à l'ouverture, la caverne 1 est dessinée ==");
    printf("   %d icônes, %d cavernes, %d boutons\n", nic, g_nniv, 242);
    hc_send(g_carte, "openCard");
    {
        int r = controle();
        printf("   contrôle : %d\n", r);
        int vus[16] = { 0 };
        for (int k = 0; k < 240; k++) {
            int ic = g_cases[k]->icon;
            vus[ic ? ic - 2000 : 0]++;
        }
        printf("   cases : vide %d, acier %d, briques %d, terre %d, rochers %d, diamants %d, "
               "creuseuse %d, sortie %d\n", vus[0], vus[1], vus[2], vus[3], vus[4], vus[5], vus[6], vus[7]);
        g_dernier[0] = 0;
        hc_do("put card field \"etat\" && \"|\" && card field \"niveau\" && \"|\" && "
              "card field \"diamants\" && \"|\" && card field \"temps\"");
        printf("   %s\n", g_dernier);
    }

    puts("\n== 2. un rocher tombe dans le vide, une case par image, et s'arrête sur la terre ==");
    {
        static const char *const p[12] = {
            BORD, "#....O.............#", "#.... .............#", "#.... .............#",
            "#.... .............#", TERRE, "#P.................#", TERRE, TERRE, TERRE, TERRE, BORD };
        installe(p, 9);
        joue_et_montre(".....", 2, 6, 6, 6);
        printf("   sons : %s · le rocher tombe encore : %c\n", g_sons, G("gChute")[21 * 4 + 5]);
    }

    puts("\n== 3. il écrase la creuseuse qui est dessous ==");
    {
        static const char *const p[12] = {
            BORD, "#....O.............#", "#.... .............#", "#.... .............#",
            "#.... .............#", "#....P.............#", TERRE, TERRE, TERRE, TERRE, TERRE, BORD };
        installe(p, 9);
        joue_et_montre("....", 2, 7, 6, 6);
        printf("   mort %s · sons %s\n", G("gMort"), g_sons);
        int n = 0;
        while (!*G("gIssue") && n < 30) { une_image('.'); n++; }
        printf("   issue « %s » après %d images de plus\n", G("gIssue"), n);
        montre("ensuite", 4, 7, 5, 7);
    }

    puts("\n== 4. posé sur sa tête, il ne lui fait rien ; qu'elle s'écarte, il tombe ==");
    {
        static const char *const q[12] = {
            BORD, TERRE, TERRE, "#....O.............#", "#....P ............#",
            "#..... ............#", TERRE, TERRE, TERRE, TERRE, TERRE, BORD };
        installe(q, 9);
        joue_et_montre("...d..", 4, 6, 6, 7);
        printf("   mort %s · joueur %s\n", G("gMort"), G("gJoueur"));
    }

    puts("\n== 5. sur un rocher, un rocher roule du côté libre, puis tombe ==");
    {
        static const char *const p[12] = {
            BORD, TERRE, "#... O.............#", "#... O.............#", "#... ..............#",
            TERRE, TERRE, "#P.................#", TERRE, TERRE, TERRE, BORD };
        installe(p, 9);
        joue_et_montre("....", 3, 6, 5, 6);
        static const char *const q[12] = {
            BORD, TERRE, "#....* ............#", "#....O ............#", "#..... ............#",
            TERRE, TERRE, "#P.................#", TERRE, TERRE, TERRE, BORD };
        puts("   — un diamant sur un rocher, libre à droite seulement :");
        installe(q, 9);
        joue_et_montre("....", 3, 6, 6, 7);
        static const char *const s[12] = {
            BORD, TERRE, "#... O.............#", "#....=.............#", TERRE,
            TERRE, TERRE, "#P.................#", TERRE, TERRE, TERRE, BORD };
        puts("   — sur des briques, mais la case d'à côté est pleine dessous : il reste");
        installe(s, 9);
        joue_et_montre("..", 3, 5, 5, 6);
    }

    puts("\n== 6. elle pousse un rocher de côté, s'il y a de la place, et pas vers le haut ==");
    {
        static const char *const p[12] = {
            BORD, TERRE, TERRE, TERRE, TERRE, "#.PO ..............#", TERRE, TERRE, TERRE, TERRE, TERRE, BORD };
        installe(p, 9);
        joue_et_montre("dd", 6, 6, 2, 7);
        static const char *const s[12] = {
            BORD, TERRE, TERRE, TERRE, "#..O...............#", "#..P...............#", TERRE, TERRE, TERRE, TERRE, TERRE, BORD };
        installe(s, 9);
        puts("   — vers le haut :");
        joue_et_montre("h", 5, 6, 4, 4);
    }

    puts("\n== 7. deux diamants ouvrent la sortie, et l'on sort ==");
    {
        static const char *const p[12] = {
            BORD, "#P**E..............#", TERRE, TERRE, TERRE, TERRE, TERRE, TERRE, TERRE, TERRE, TERRE, BORD };
        installe(p, 2);
        une_image('d');
        printf("   un diamant  : %s · ouverte %s · score %s\n", G("gDiams"), G("gOuvert"), G("gScore"));
        une_image('d');
        printf("   deux        : %s · ouverte %s · score %s\n", G("gDiams"), G("gOuvert"), G("gScore"));
        une_image('d');
        printf("   la sortie   : issue « %s » · gagné %s · sons %s\n", G("gIssue"), G("gGagne"), g_sons);
        g_dernier[0] = 0;
        hc_do("put card field \"etat\" && \"|\" && card field \"diamants\" && \"|\" && card field \"score\"");
        printf("   %s\n", g_dernier);
        static const char *const q[12] = {
            BORD, "#PE*...............#", TERRE, TERRE, TERRE, TERRE, TERRE, TERRE, TERRE, TERRE, TERRE, BORD };
        installe(q, 1);
        une_image('d');
        printf("   sortie fermée : joueur %s, issue « %s »\n", G("gJoueur"), G("gIssue"));
    }

    puts("\n== 8. un rocher qui tombe sur une luciole : elle explose, et laisse du vide ==");
    {
        static const char *const p[12] = {
            BORD, TERRE, TERRE, "#....O.............#", "#.... .............#", "#....F.............#",
            TERRE, TERRE, "#P.................#", TERRE, TERRE, BORD };
        installe(p, 9);
        joue_et_montre("......", 4, 7, 5, 7);
        printf("   bêtes « %s » · sons %s\n", G("gEnnemis"), g_sons);
    }

    puts("\n== 9. sur un papillon : il laisse neuf diamants ==");
    {
        static const char *const p[12] = {
            BORD, TERRE, TERRE, "#....O.............#", "#.... .............#", "#....B.............#",
            "#...   ............#", TERRE, "#P.................#", TERRE, TERRE, BORD };
        installe(p, 9);
        joue_et_montre("......", 4, 8, 5, 7);
    }

    puts("\n== 10. une luciole longe son tunnel, et explose en touchant la creuseuse ==");
    {
        static const char *const p[12] = {
            BORD, TERRE, TERRE, TERRE, "#......P...........#", "#.F        ........#",
            TERRE, TERRE, TERRE, TERRE, TERRE, BORD };
        installe(p, 9);
        joue_et_montre("........", 5, 6, 2, 11);
        printf("   mort %s · bêtes « %s »\n", G("gMort"), G("gEnnemis"));
    }

    puts("\n== 11. un papillon tourne à droite, une luciole à gauche : chacun longe son mur ==");
    {
        static const char *const p[12] = {
            BORD, TERRE, "#.   ....   .......#", "#. F .... B .......#", "#.   ....   .......#",
            TERRE, TERRE, TERRE, "#P.................#", TERRE, TERRE, BORD };
        installe(p, 9);
        joue_et_montre("........", 3, 5, 3, 12);
    }

    puts("\n== 12. R : elle renonce, et le niveau recommence ==");
    {
        static const char *const p[12] = {
            BORD, TERRE, TERRE, "#....P.............#", TERRE, TERRE, TERRE, TERRE, TERRE, TERRE, TERRE, BORD };
        installe(p, 9);
        une_image('r');
        montre("après R", 3, 5, 5, 7);
        printf("   mort %s · joueur « %s »\n", G("gMort"), G("gJoueur"));
        int n = 0;
        while (!*G("gIssue") && n < 30) { une_image('.'); n++; }
        printf("   issue « %s » après %d images ; les éclats : ", G("gIssue"), n);
        montre("", 3, 5, 5, 7);
    }

    puts("\n== 13. le clavier : flèches, ZQSD, WASD ==");
    {
        static const char *const p[12] = {
            BORD, TERRE, TERRE, TERRE, TERRE, "#........P.........#", TERRE, TERRE, TERRE, TERRE, TERRE, BORD };
        static const struct { char k; const char *quoi; } t[] = {
            { 'd', "flèche droite" }, { 'q', "q (gauche, français)" }, { 'a', "a (gauche, anglais)" },
            { 'z', "z (haut, français)" }, { 'w', "w (haut, anglais)" }, { 's', "s (bas)" },
            { 'g', "flèche gauche" }, { 'h', "flèche haut" }, { 'b', "flèche bas" },
        };
        installe(p, 9);
        for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
            une_image(t[i].k);
            printf("   ");
            libelle(t[i].quoi, 22);
            printf(" → %s\n", G("gJoueur"));
        }
        /* Une frappe brève, entre deux images, compte ; une touche tenue ne
         * fait pas un pas de trop une fois relâchée. */
        g_tenues = "65363"; hc_send(g_carte, "retiens"); g_tenues = "";
        une_image('.');
        printf("   frappe brève à droite entre deux images → %s\n", G("gJoueur"));
        g_tenues = "65363"; hc_send(g_carte, "clavier");
        hc_send(g_carte, "retiens"); g_tenues = "";
        une_image('.');
        printf("   droite tenue, relâchée avant l'image  → %s (pas de pas de trop)\n", G("gJoueur"));
    }

    /* LE ROBOT : il creuse les trois cavernes l'une après l'autre ; à chaque
     * image, le contrôle.
     *
     * Seul, il ne sort pas de la 3, et c'est voulu : ses dix diamants n'y
     * suffisent pas, il faut écraser le papillon, ce qu'il ne sait pas faire.
     * Ses « morts » y sont des R, appuyés quand plus rien n'est à sa portée.
     * La section 14 bis montre que la caverne se résout. */
    puts("\n== 14. un robot creuse les cavernes, et l'écran suit le plateau ==");
    {
        unsigned graine = 2510;
        for (int i = 0; i < g_nniv; i++) {
            int essais, morts, images;
            int gagne = robot_caverne(g_niv[i].n, 12, &graine, &essais, &morts, &images);
            printf("   caverne %d : %s en %d essai(s), %d mort(s), %d images, %s diamant(s)\n",
                   g_niv[i].n, gagne ? "SORTIE" : "pas sortie", essais, morts, images, G("gDiams"));
        }
        printf("   défauts : %d\n", g_defauts);
    }

    /* LA CAVERNE 3 SE RÉSOUT. Ses dix diamants n'y suffisent pas : il faut
     * écraser le papillon, qui va et vient dans son tunnel. On cherche le
     * moment : la creuseuse se poste à côté du rocher (4,9), attend w images,
     * descend, creuse sous le rocher — qui ne tombe pas sur une tête —,
     * s'écarte — il tombe —, et remonte s'abriter. Le rocher arrive sur le
     * tunnel. Puis le robot ramasse et sort.
     *
     * Par la gauche, ça marche. Par la droite, c'est un piège : la colonne
     * (·,10) porte un autre rocher, qui descend sur elle dès qu'elle quitte
     * (4,10). Les deux sont gardées : la seconde dit que la caverne a un
     * mauvais chemin, et qu'il se voit. */
    puts("\n== 14 bis. la caverne 3 se résout : un rocher lâché sur le papillon ==");
    {
        /* Deux façons : par la gauche du rocher, depuis (4,8), ou par sa
         * droite, depuis (4,10). */
        static const struct { int r, c; const char *gestes; } voies[] = {
            { 4, 8, "bdgh.." }, { 4, 10, "bgdh.." },
        };
        unsigned graine = 99;
        for (size_t v = 0; v < sizeof voies / sizeof *voies; v++) {
            int trouve = -1, essais = 0, total = 0, mortes = 0, vivant = 0;
            for (int w = 0; w < 32 && trouve < 0; w++) {
                essais++;
                hc_send_arg(g_carte, "chargeNiveau", "3");
                hc_module_pose_globale("gIssue", "");
                int t = 0, bloque = 0, arrivee = 0;
                g_vise_r = voies[v].r - 1; g_vise_c = voies[v].c - 1;
                for (; t < 300; t++) {
                    const char *j = G("gJoueur");
                    if (!*j) break;
                    if (atoi(j) == voies[v].r && atoi(strchr(j, ',') + 1) == voies[v].c) { arrivee = 1; break; }
                    lit_plat(); lit_chutes();
                    une_image(robot_choisit(0, &graine, &bloque));
                }
                g_vise_r = g_vise_c = -1;
                if (!arrivee) continue;
                for (int i = 0; i < w; i++) une_image('.');
                for (const char *k = voies[v].gestes; *k; k++) une_image(*k);
                if (!*G("gJoueur")) { mortes++; continue; }
                if (*G("gEnnemis")) { vivant++; continue; }
                int tt;
                bloque = 0;
                for (tt = 0; tt < 3000 && !*G("gIssue"); tt++) {
                    lit_plat(); lit_chutes();
                    une_image(robot_choisit(!strcmp(G("gOuvert"), "true"), &graine, &bloque));
                }
                if (!strcmp(G("gIssue"), "won")) { trouve = w; total = t + w + 6 + tt; }
            }
            printf("   depuis (%d,%d), « %s » : ", voies[v].r, voies[v].c, voies[v].gestes);
            if (trouve >= 0)
                printf("SORTIE en attendant %d image(s) ; %d essai(s) (%d morte(s), %d papillon(s) "
                       "manqué(s)), %d images, %s diamants\n", trouve, essais, mortes, vivant, total, G("gDiams"));
            else
                printf("aucun moment ne marche sur %d essais (%d morte(s), %d papillon(s) manqué(s))\n",
                       essais, mortes, vivant);
        }
    }

    /* LA VRAIE BOUCLE, avec son horloge. Le temps d'abord : une caverne d'une
     * seconde, sans toucher à rien, coûte une vie à chaque fois. */
    puts("\n== 15. la vraie boucle : le temps s'épuise, trois fois ==");
    {
        g_horloge = 1;
        clock_gettime(CLOCK_MONOTONIC, &g_t0);
        g_filet = 200000; g_lectures = 0; g_echap_a = 0;
        hc_do("put \"8,1\" into line 1 of card field \"level 1\"");
        g_sons[0] = 0;
        hc_send(g_carte, "nouvellePartie");
        g_dernier[0] = 0;
        hc_do("put card field \"etat\" && \"|\" && card field \"vies\" && \"|\" && card field \"temps\"");
        printf("   %s · sons : %s\n", g_dernier, g_sons);
        char l1[32];
        snprintf(l1, sizeof l1, "%d,%d", g_niv[0].besoin, g_niv[0].secondes);
        char cmd[96];
        snprintf(cmd, sizeof cmd, "put \"%s\" into line 1 of card field \"level 1\"", l1);
        hc_do(cmd);
    }

    puts("\n== 16. la vraie boucle : Échap arrête la partie ==");
    {
        g_lectures = 0; g_echap_a = 40; g_filet = 4000;
        hc_send(g_carte, "nouvellePartie");
        g_dernier[0] = 0;
        hc_do("put card field \"etat\"");
        printf("   %s · %s\n", g_lectures < g_filet ? "arrêtée par Échap" : "Échap SANS EFFET, arrêtée par le filet",
               g_dernier);
        g_echap_a = 0; g_filet = 0; g_horloge = 0;
    }

    /* D'UNE CAVERNE À L'AUTRE. Trois cavernes minuscules — un diamant, puis
     * la sortie —, la flèche droite tenue : la partie doit les enchaîner,
     * compter le bonus de temps, et finir sur la dernière. */
    puts("\n== 17. la vraie boucle : d'une caverne à l'autre, jusqu'au bout ==");
    {
        char mini[16 + 12 * 21 + 64];
        int k = snprintf(mini, sizeof mini, "1,30\n%s\n#P*E..............#", BORD);
        for (int r = 0; r < 9; r++) k += snprintf(mini + k, sizeof mini - (size_t)k, "\n%s", TERRE);
        snprintf(mini + k, sizeof mini - (size_t)k, "\n%s\nTiny.", BORD);
        for (int i = 0; i < g_nniv; i++) {
            char nom[16];
            snprintf(nom, sizeof nom, "level %d", g_niv[i].n);
            hc_module_pose_champ(1, nom, 0, 0, mini);
        }
        g_horloge = 1; g_tenues = "65363"; g_filet = 20000; g_lectures = 0;
        g_sons[0] = 0;
        hc_send(g_carte, "nouvellePartie");
        g_tenues = ""; g_filet = 0; g_horloge = 0;
        g_dernier[0] = 0;
        hc_do("put card field \"etat\" && \"|\" && card field \"niveau\" && \"|\" && "
              "card field \"score\" && \"|\" && card field \"aide\"");
        printf("   %s\n   sons : %s\n", g_dernier, g_sons);
        for (int i = 0; i < g_nniv; i++) {
            char nom[16];
            snprintf(nom, sizeof nom, "level %d", g_niv[i].n);
            hc_module_pose_champ(1, nom, 0, 0, g_niv[i].texte);
        }
    }

    printf("\n== bilan : %d défaut(s), %d erreur(s) de script ==\n", g_defauts, g_erreurs);
    hc_free(st); free(script);
    return g_defauts || g_erreurs ? 1 : 0;
}
