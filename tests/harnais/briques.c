/* briques — UN CASSE-BRIQUES EN HYPERTALK, MONTÉ PAR LE CODE, pour le DMG.
 *
 * Demandé par l'utilisatrice le 9 octobre, après le Space Invaders. Les
 * soixante briques sont des boutons rectangles colorés ; la raquette et la
 * balle, des boutons polygones. Le clavier passe par « the keysDown ». Tout
 * le jeu vit dans le script de la pile, tests/donnees/briques_pile.txt.
 *
 *   briques pile.txt                    le test
 *   briques pile.txt Briques.stack      écrit la pile, pour le DMG
 *
 * Le jeu n'a pas de hasard : le test pose la balle à la main et regarde
 * chaque rebond — murs, plafond, raquette au centre et aux bords, le côté
 * d'une brique. Puis un ROBOT suit la balle pendant des milliers d'images,
 * avec ses distractions tirées d'un hasard fixé, et l'on vérifie à chaque
 * image que l'écran, l'état et le score disent la même chose.
 *
 * NON VÉRIFIÉ ICI : le rendu, les sons, la vitesse réelle — à jouer DANS HC
 * (l'application). */
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
    b->showname = 0;
    free(b->style); b->style = strdup(style);
    return b;
}

/* La carte fait 512 × 342. Le terrain, noir, de 10 à 370 ; les briques
 * depuis 11, 40, tous les 36 points en largeur et 14 en hauteur. */
static Object *monte(const char *script)
{
    Object *st = hc_new_stack("Casse-briques");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    g_carte = hc_new_card(st, bg, "Briques");

    pose_bouton("Cadre", "rectangle", 8, 8, 364, 324);
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 10; c++) {
            char nom[8];
            snprintf(nom, sizeof nom, "B%d", r * 10 + c + 1);
            pose_bouton(nom, "rectangle", 11 + c * 36, 40 + r * 14, 34, 12);
        }
    pose_bouton("Raquette", "polygon", 0, 0, 50, 8);
    pose_bouton("Balle", "polygon", 0, 0, 8, 8);

    Object *t = pose_champ("Titre", 382, 14, 124, 56, 18, HC_BOLD);
    hc_set_field_text(t, "CASSE\nBRIQUES");
    hc_set_field_text(pose_champ("LScore", 382, 80, 50, 20, 12, 0), "Score");
    pose_champ("Score", 434, 80, 72, 20, 12, HC_BOLD);
    hc_set_field_text(pose_champ("LVies", 382, 102, 50, 20, 12, 0), "Vies");
    pose_champ("Vies", 434, 102, 72, 20, 12, HC_BOLD);
    hc_set_field_text(pose_champ("LNiveau", 382, 124, 50, 20, 12, 0), "Niveau");
    pose_champ("Niveau", 434, 124, 72, 20, 12, HC_BOLD);
    pose_champ("Etat", 382, 150, 124, 48, 11, HC_ITALIC);
    hc_set_field_text(pose_champ("Aide", 382, 202, 124, 80, 10, 0),
        "Q et D : bouger\nZ ou espace : lancer\nEchap : arreter\n"
        "(les fleches marchent aussi)");
    Object *n = pose_bouton("Nouvelle partie", "roundRect", 382, 296, 124, 26);
    n->showname = 1;
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

static void rapport(void)
{
    g_dernier[0] = 0;
    hc_send(g_carte, "rapport");
    printf("   %s\n", g_dernier);
}

static Object *bouton(const char *nom)
{
    for (int i = 0; i < g_carte->nparts; i++)
        if (g_carte->parts[i]->type == OBJ_BUTTON && !strcmp(g_carte->parts[i]->name, nom))
            return g_carte->parts[i];
    return NULL;
}

static void envoie(const char *msg, const char *arg) { hc_send_arg(g_carte, msg, arg); }

/* Fait avancer la balle jusqu'à ce que la condition change, au plus n fois. */
static int jusqua(const char *globale, const char *valeur_initiale, int n)
{
    int i = 0;
    while (i < n && !strcmp(global_txt(globale), valeur_initiale)) { hc_send(g_carte, "avanceBalle"); i++; }
    return i;
}

static int signe(const char *v) { double d = atof(v); return d > 0 ? 1 : d < 0 ? -1 : 0; }

/* L'écran, l'état et le score disent-ils la même chose ? 0, ou le numéro
 * du premier désaccord. */
static int controle(void)
{
    char br[128];
    snprintf(br, sizeof br, "%s", global_txt("gBriques"));
    if (strlen(br) != 60) return 1;
    int vivantes = 0, points = 0;
    for (int k = 0; k < 60; k++) {
        char nom[8]; snprintf(nom, sizeof nom, "B%d", k + 1);
        int vivante = br[k] == '1';
        vivantes += vivante;
        if (!vivante) points += 10 * (6 - k / 10);
        if (bouton(nom)->visible != vivante) return 2;
    }
    if (vivantes != atoi(global_txt("gReste"))) return 3;
    int niveau = atoi(global_txt("gNiveau"));
    if (atoi(global_txt("gScore")) != (niveau - 1) * 2100 + points) return 4;
    if (strcmp(global_txt("gFini"), "true")) {
        double x = atof(global_txt("gBx")), y = atof(global_txt("gBy"));
        if (x < 10 || x > 370 || y < 10 || y > 340) return 5;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s pile.txt [Briques.stack]\n", argv[0]); return 2; }
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
    {
        static const char *noms[] = { "B1", "B10", "B60", "Raquette", "Balle" };
        for (size_t i = 0; i < sizeof noms / sizeof *noms; i++) {
            Object *b = bouton(noms[i]);
            printf("   %-8s rect %d,%d %dx%d, %d sommets\n", noms[i], b->x, b->y, b->w, b->h, b->npoints);
        }
    }
    printf("   contrôle : %d\n", controle());

    puts("\n== 2. la raquette s'arrête aux murs, et la balle posée la suit ==");
    envoie("nouvellePartie", "essai");
    envoie("raquetteEn", "0");
    rapport();
    envoie("raquetteEn", "999");
    rapport();

    puts("\n== 3. lancée, la balle monte et casse une brique ==");
    envoie("nouvellePartie", "essai");
    hc_send(g_carte, "lance");
    printf("   %d images jusqu'à la première brique\n", jusqua("gReste", "60", 400));
    rapport();
    printf("   briques %s\n", global_txt("gBriques"));

    puts("\n== 4. les murs et le plafond renvoient la balle ==");
    hc_do("balleEn 20, 200, -3, -1");
    for (int i = 0; i < 5; i++) hc_send(g_carte, "avanceBalle");
    rapport();
    hc_do("balleEn 200, 20, 1, -3");
    for (int i = 0; i < 5; i++) hc_send(g_carte, "avanceBalle");
    rapport();
    hc_do("balleEn 360, 200, 3, 1");
    for (int i = 0; i < 5; i++) hc_send(g_carte, "avanceBalle");
    rapport();

    puts("\n== 5. l'angle du rebond dépend de l'endroit de la raquette ==");
    envoie("nouvellePartie", "essai");
    envoie("raquetteEn", "190");
    static const struct { const char *x, *quoi; } impacts[] = {
        { "190", "au centre : droit" },
        { "204", "à mi-chemin à droite" },
        { "218", "au bord droit : soixante degrés" },
        { "170", "à gauche" },
    };
    for (size_t i = 0; i < sizeof impacts / sizeof *impacts; i++) {
        char s[96];
        snprintf(s, sizeof s, "balleEn %s, 290, 0, 3", impacts[i].x);
        hc_do(s);
        int n = 0;
        while (signe(global_txt("gVy")) > 0 && n < 20) { hc_send(g_carte, "avanceBalle"); n++; }
        printf("   %-34s ", impacts[i].quoi);
        rapport();
    }

    puts("\n== 6. le côté d'une brique renvoie la balle de côté ==");
    envoie("nouvellePartie", "essai");
    envoie("casse", "52");                     /* la 2e de la rangée du bas */
    hc_do("balleEn 64, 116, -3, 0.2");         /* dans son trou, vers la 51 */
    {
        int n = 0;
        while (!strcmp(global_txt("gReste"), "59") && n < 20) { hc_send(g_carte, "avanceBalle"); n++; }
    }
    rapport();
    printf("   briques %s\n", global_txt("gBriques"));

    puts("\n== 7. trois balles perdues, et c'est fini ==");
    envoie("nouvellePartie", "essai");
    for (int i = 0; i < 3; i++) {
        hc_do("balleEn 300, 328, 0, 3");      /* loin de la raquette */
        hc_send(g_carte, "avanceBalle");
        rapport();
    }
    g_dernier[0] = 0; hc_do("put card field \"Etat\""); printf("   champ Etat : %s\n", g_dernier);

    puts("\n== 8. le mur abattu, le niveau suivant, un peu plus vite ==");
    envoie("nouvellePartie", "essai");
    for (int k = 1; k <= 60; k++) {
        char n[8]; snprintf(n, sizeof n, "%d", k);
        envoie("casse", n);
    }
    rapport();
    printf("   vitesse %s · contrôle %d\n", global_txt("gVit"), controle());

    puts("\n== 9. le clavier : Q D Z, espace, ou les flèches ==");
    h.global_get = glob_lit; hc_set_host(&h);
    envoie("nouvellePartie", "essai");
    static const struct { const char *tenues, *quoi; } tours[] = {
        { "",       "rien de tenu" },
        { "113",    "q : à gauche" },
        { "68",     "D majuscule : à droite" },
        { "65361",  "flèche gauche" },
        { "32",     "espace : lance" },
    };
    for (size_t i = 0; i < sizeof tours / sizeof *tours; i++) {
        g_tenues = tours[i].tenues;
        hc_send(g_carte, "clavier");
        g_tenues = "";
        char px[16];
        snprintf(px, sizeof px, "%s", global_txt("gPx"));
        printf("   %s : raquette %s, posée %s\n", tours[i].quoi, px, global_txt("gColle"));
    }

    /* LE ROBOT : il suit la balle, en visant un peu à côté du centre pour
     * varier les angles, et se laisse distraire de temps en temps — d'où
     * quelques balles perdues. Le contrôle à chaque image. */
    puts("\n== 10. un robot joue, et l'écran suit l'état ==");
    {
        unsigned graine = 777;
        int images = 0, defaut = 0, parties = 0, niveaux = 0, perdues = 0, cassees = 0;
        while (parties < 3) {
            envoie("nouvellePartie", "essai");
            int decalage = 0, distrait = 0, attente = 30;
            for (int t = 0; t < 12000; t++) {
                graine = graine * 1103515245u + 12345u;
                if (t % 200 == 0) decalage = (int)((graine >> 16) % 41) - 20;
                if (distrait > 0) distrait--;
                else if ((graine >> 8) % 2500 == 0) distrait = 90;
                char px_s[32], bx_s[32];
                snprintf(px_s, sizeof px_s, "%s", global_txt("gPx"));
                snprintf(bx_s, sizeof bx_s, "%s", global_txt("gBx"));
                int px = atoi(px_s), bx = (int)atof(bx_s);
                if (!strcmp(global_txt("gColle"), "true")) {
                    g_tenues = --attente <= 0 ? "32" : "";
                    if (attente <= 0) attente = 30;
                } else if (distrait) {
                    g_tenues = "";
                } else {
                    int cible = bx + decalage;
                    g_tenues = cible < px - 3 ? "113" : cible > px + 3 ? "100" : "";
                }
                int vies_avant = atoi(global_txt("gVies"));
                hc_send(g_carte, "tour");
                images++;
                if (atoi(global_txt("gVies")) < vies_avant) perdues++;
                int r = controle();
                if (r) { printf("   DÉFAUT %d à l'image %d\n", r, t); defaut++; break; }
                if (!strcmp(global_txt("gFini"), "true")) break;
            }
            g_tenues = "";
            int niveau = atoi(global_txt("gNiveau"));
            niveaux += niveau - 1;
            cassees += (niveau - 1) * 60 + 60 - atoi(global_txt("gReste"));
            parties++;
        }
        printf("   %d parties, %d images, %d briques cassées, %d mur(s) abattu(s), "
               "%d balle(s) perdue(s), %s\n", parties, images, cassees, niveaux, perdues,
               defaut ? "DES DÉFAUTS" : "aucun défaut");
    }

    /* ÉCHAP ARRÊTE LA PARTIE, DANS LA VRAIE BOUCLE. La pile attendait 27,
     * charToNum du caractère ; HC donne 65307, le code de la touche. Échap
     * n'arrêtait donc rien, et seul Cmd-. sortait d'une partie. Trouvé le 9
     * octobre en cherchant pourquoi le Space Invaders et le casse-briques ne
     * répondaient plus aux touches DANS HC (l'application) ; le même défaut
     * était dans les trois jeux. Les sections précédentes jouent les tours à
     * la main et ne passaient jamais par la boucle : ici elle tourne pour de
     * bon, avec ses « wait 1 tick ». */
    puts("\n== 11. Échap arrête la partie, dans la vraie boucle ==");
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
