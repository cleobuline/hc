/* invaders — UN SPACE INVADERS EN HYPERTALK, MONTÉ PAR LE CODE, pour le DMG.
 *
 * Demandé par l'utilisatrice le 9 octobre, après le Tetris : « amusons-nous
 * plutôt à faire un Space Invaders ». Même construction : les quarante
 * envahisseurs, le canon et la soucoupe sont des boutons polygones — sommets
 * et couleurs, les deux extensions des boutons —, le clavier passe par « the
 * keysDown ». Tout le jeu vit dans le script de la pile,
 * tests/donnees/invaders_pile.txt.
 *
 *   invaders pile.txt                     le test
 *   invaders pile.txt Invaders.stack      écrit la pile, pour le DMG
 *
 * Le test ne lance pas la boucle — elle attend le clavier et les ticks : la
 * partie est préparée en mode « essai », sans hasard, et l'on joue les gestes
 * un à un. Puis un ROBOT joue des milliers de tours (un hasard fixé, côté C,
 * pour un témoin stable) et l'on vérifie à chaque tour que l'état et l'écran
 * disent la même chose.
 *
 * NON VÉRIFIÉ ICI : le rendu à l'écran, les sons, la vitesse réelle — à
 * jouer DANS HC (l'application). */
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

/* Les touches tenues que l'hôte prétend voir. */
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

/* La carte fait 512 × 342. Le terrain, noir, de 10 à 370 sur 10 à 330 ; à
 * droite, le titre, le score, les vies, la vague, l'état et le bouton. */
static Object *monte(const char *script)
{
    Object *st = hc_new_stack("Space Invaders");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    g_carte = hc_new_card(st, bg, "Invaders");

    pose_bouton("Cadre", "rectangle", 8, 8, 364, 324);
    for (int b = 0; b < 4; b++)                     /* les abris */
        for (int r = 0; r < 2; r++)
            for (int c = 0; c < 3; c++) {
                char nom[8];
                snprintf(nom, sizeof nom, "M%d", b * 6 + r * 3 + c + 1);
                pose_bouton(nom, "rectangle", 45 * (2 * b + 1) - 5 + c * 10, 262 + r * 8, 10, 8);
            }
    for (int i = 0; i < 40; i++) {                  /* les envahisseurs */
        char nom[8];
        snprintf(nom, sizeof nom, "E%d", i + 1);
        pose_bouton(nom, "polygon", 0, 0, 24, 16);
    }
    pose_bouton("Canon", "polygon", 0, 0, 26, 14);
    pose_bouton("Ovni", "polygon", 0, 0, 32, 12);
    pose_bouton("Tir", "rectangle", 0, 0, 2, 8);
    for (int k = 1; k <= 3; k++) {
        char nom[8];
        snprintf(nom, sizeof nom, "Bombe%d", k);
        pose_bouton(nom, "rectangle", 0, 0, 3, 8);
    }

    Object *t = pose_champ("Titre", 382, 14, 124, 56, 18, HC_BOLD);
    hc_set_field_text(t, "SPACE\nINVADERS");
    hc_set_field_text(pose_champ("LScore", 382, 80, 50, 20, 12, 0), "Score");
    pose_champ("Score", 434, 80, 72, 20, 12, HC_BOLD);
    hc_set_field_text(pose_champ("LVies", 382, 102, 50, 20, 12, 0), "Vies");
    pose_champ("Vies", 434, 102, 72, 20, 12, HC_BOLD);
    hc_set_field_text(pose_champ("LVague", 382, 124, 50, 20, 12, 0), "Vague");
    pose_champ("Vague", 434, 124, 72, 20, 12, HC_BOLD);
    pose_champ("Etat", 382, 150, 124, 48, 11, HC_ITALIC);
    hc_set_field_text(pose_champ("Aide", 382, 202, 124, 80, 10, 0),
        "Q et D : bouger\nZ ou espace : tirer\nEchap : arreter\n"
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

static void pose_globale(const char *nom, const char *val)
{
    char s[256];
    snprintf(s, sizeof s, "global %s\nput \"%s\" into %s", nom, val, nom);
    hc_do(s);
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

/* Le tir monte jusqu'à toucher ou sortir : au plus quarante tours. */
static int laisse_monter(void)
{
    int n = 0;
    while (!strcmp(global_txt("gTir"), "true") && n < 40) { hc_send(g_carte, "bougeTir"); n++; }
    return n;
}

/* L'état et l'écran disent-ils la même chose ? Rend 0, ou le numéro du
 * premier désaccord. */
static int controle(void)
{
    char etat[64], murs[64], bombes[256];
    snprintf(etat, sizeof etat, "%s", global_txt("gEtat"));
    snprintf(murs, sizeof murs, "%s", global_txt("gMurs"));
    snprintf(bombes, sizeof bombes, "%s", global_txt("gBombes"));
    if (strlen(etat) != 40) return 1;
    if (strlen(murs) != 24) return 2;
    int vivants = 0;
    for (int i = 0; i < 40; i++) {
        char nom[8]; snprintf(nom, sizeof nom, "E%d", i + 1);
        Object *b = bouton(nom);
        int vivant = etat[i] == '1';
        vivants += vivant;
        if (!b || b->visible != vivant) return 3;
    }
    if (vivants != atoi(global_txt("gVivants"))) return 4;
    for (int k = 0; k < 24; k++) {
        char nom[8]; snprintf(nom, sizeof nom, "M%d", k + 1);
        Object *b = bouton(nom);
        if (!b || b->visible != (murs[k] == '1')) return 5;
    }
    int tir = !strcmp(global_txt("gTir"), "true");
    if (bouton("Tir")->visible != tir) return 6;
    /* Une bombe montrée par ligne non vide de gBombes. */
    char *l = bombes;
    for (int k = 1; k <= 3; k++) {
        char nom[8]; snprintf(nom, sizeof nom, "Bombe%d", k);
        char *fin = l ? strchr(l, '\n') : NULL;
        int pleine = l && *l && *l != '\n';
        if (bouton(nom)->visible != pleine) return 7;
        l = fin ? fin + 1 : NULL;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s pile.txt [Invaders.stack]\n", argv[0]); return 2; }
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
        static const char *noms[] = { "E1", "E8", "E9", "E33", "E40", "Canon", "Ovni" };
        for (size_t i = 0; i < sizeof noms / sizeof *noms; i++) {
            Object *b = bouton(noms[i]);
            printf("   %-6s rect %d,%d %dx%d, %d sommets, %s\n", noms[i], b->x, b->y,
                   b->w, b->h, b->npoints, b->visible ? "visible" : "caché");
        }
    }
    printf("   contrôle : %d\n", controle());

    puts("\n== 2. la formation marche, touche le bord, descend et repart ==");
    envoie("nouvellePartie", "essai");
    {
        char avant[16] = "1";
        for (int pas = 1; pas <= 40; pas++) {
            hc_send(g_carte, "pasFormation");
            const char *sens = global_txt("gDir");
            if (strcmp(sens, avant)) {
                printf("   pas %2d : demi-tour, ", pas);
                rapport();
                snprintf(avant, sizeof avant, "%s", sens);
            }
        }
    }

    puts("\n== 3. le tir abat le plus bas de la colonne, puis celui d'au-dessus ==");
    envoie("nouvellePartie", "essai");
    envoie("canonEn", "32");                   /* sous la colonne 1 */
    hc_send(g_carte, "tire");
    printf("   %d tours de tir\n", laisse_monter());
    rapport();
    printf("   E33 %s, E25 %s\n", bouton("E33")->visible ? "visible" : "abattu",
           bouton("E25")->visible ? "visible" : "abattu");
    hc_send(g_carte, "tire");
    laisse_monter();
    rapport();
    printf("   E25 %s · contrôle %d\n", bouton("E25")->visible ? "visible" : "abattu", controle());
    puts("   -- à droite de la formation et entre deux abris : le tir sort du terrain");
    envoie("canonEn", "295");                  /* formation 20..282, abri 4 dès 310 */
    hc_send(g_carte, "tire");
    printf("   %d tours de tir\n", laisse_monter());
    rapport();
    printf("   murs %s\n", global_txt("gMurs"));

    puts("\n== 4. les abris arrêtent le tir et les bombes ==");
    envoie("nouvellePartie", "essai");
    envoie("canonEn", "145");                  /* sous l'abri 2 */
    hc_send(g_carte, "tire");
    laisse_monter();
    printf("   murs %s\n", global_txt("gMurs"));
    hc_send(g_carte, "tire");
    laisse_monter();
    printf("   murs %s  (la brique du dessus, à son tour)\n", global_txt("gMurs"));
    hc_send(g_carte, "tire");
    laisse_monter();
    printf("   murs %s  (la colonne est percée : le tir passe)\n", global_txt("gMurs"));
    hc_do("lacheBombe 55, 240");
    for (int i = 0; i < 20; i++) hc_send(g_carte, "bougeBombes");
    printf("   murs %s  (une bombe sur l'abri 1)\n", global_txt("gMurs"));
    printf("   contrôle %d\n", controle());

    puts("\n== 5. une bombe sur le canon coûte une vie ; trois, la partie ==");
    envoie("nouvellePartie", "essai");
    for (int vie = 0; vie < 3; vie++) {
        hc_do("global gCanonX\nlacheBombe gCanonX, 284");
        for (int i = 0; i < 15; i++) hc_send(g_carte, "bougeBombes");
        rapport();
    }
    g_dernier[0] = 0; hc_do("put card field \"Etat\""); printf("   champ Etat : %s\n", g_dernier);

    puts("\n== 6. la soucoupe ==");
    envoie("nouvellePartie", "essai");
    /* Elle part de 30 et avance de 2 : 130 tours la mènent en 290, à droite
     * de la formation et entre deux abris — le tir monte sans obstacle. */
    envoie("lanceOvni", "1");
    for (int i = 0; i < 130; i++) hc_send(g_carte, "bougeOvni");
    printf("   soucoupe en %s\n", global_txt("gOvni"));
    envoie("canonEn", global_txt("gOvni"));
    hc_send(g_carte, "tire");
    laisse_monter();
    rapport();
    g_dernier[0] = 0; hc_do("put card field \"Etat\""); printf("   champ Etat : %s\n", g_dernier);

    puts("\n== 7. la vague abattue, la suivante arrive plus bas ==");
    envoie("nouvellePartie", "essai");
    for (int i = 1; i <= 40; i++) {
        char n[8]; snprintf(n, sizeof n, "%d", i);
        envoie("tueEnvahisseur", n);
    }
    rapport();
    printf("   contrôle %d\n", controle());

    puts("\n== 8. personne ne tire : la formation descend jusqu'au canon ==");
    envoie("nouvellePartie", "essai");
    {
        int pas = 0;
        while (strcmp(global_txt("gFini"), "true") && pas < 500) { hc_send(g_carte, "pasFormation"); pas++; }
        printf("   %d pas\n", pas);
    }
    rapport();
    printf("   murs %s\n", global_txt("gMurs"));
    g_dernier[0] = 0; hc_do("put card field \"Etat\""); printf("   champ Etat : %s\n", g_dernier);

    /* La boucle de jeu ne tourne pas ici : on lui joue des tours de clavier,
     * avec les touches tenues que l'hôte annonce. */
    puts("\n== 9. le clavier : Q D Z, espace, ou les flèches ==");
    h.global_get = glob_lit; hc_set_host(&h);
    envoie("nouvellePartie", "essai");
    static const struct { const char *tenues, *quoi; } tours[] = {
        { "",       "rien de tenu" },
        { "113",    "q : à gauche" },
        { "68",     "D majuscule : à droite" },
        { "65361",  "flèche gauche" },
        { "32",     "espace : tire" },
        { "122",    "z, le tir déjà en l'air : rien de plus" },
    };
    for (size_t i = 0; i < sizeof tours / sizeof *tours; i++) {
        g_tenues = tours[i].tenues;
        hc_send(g_carte, "clavier");
        g_tenues = "";
        char canon[16];
        snprintf(canon, sizeof canon, "%s", global_txt("gCanonX"));
        printf("   %s : canon %s, tir %s\n", tours[i].quoi, canon, global_txt("gTir"));
    }

    /* LE ROBOT : des tours complets, avec le contrôle à chaque tour. Il VISE
     * — le plus bas des vivants, colonne par colonne — pour abattre des
     * vagues entières et faire passer le jeu par nouvelleVague ; une fois sur
     * cinq il appuie au hasard. Les bombes tombent d'une colonne au hasard ;
     * le hasard est tiré côté C, pour un témoin stable. */
    puts("\n== 10. un robot joue, et l'écran suit l'état ==");
    {
        unsigned graine = 4242;
        int tours_joues = 0, defaut = 0, parties = 0, vagues = 0, score = 0;
        while (parties < 3) {
            envoie("nouvellePartie", "essai");
            for (int t = 0; t < 6000; t++) {
                graine = graine * 1103515245u + 12345u;
                int k = (graine >> 16) % 6;
                if ((graine >> 12) % 5 == 0) {
                    g_tenues = k == 0 ? "113" : k == 1 ? "100" : k == 2 ? "32" : k == 3 ? "113,32" : k == 4 ? "100,32" : "";
                } else {
                    char etat[64];
                    snprintf(etat, sizeof etat, "%s", global_txt("gEtat"));
                    int fx = atoi(global_txt("gFx")), cx = atoi(global_txt("gCanonX"));
                    int cible = -1;
                    for (int i = 39; i >= 0 && cible < 0; i--) if (etat[i] == '1') cible = i;
                    int x = cible < 0 ? cx : fx + (cible % 8) * 34 + 12;
                    g_tenues = x < cx - 2 ? "113" : x > cx + 2 ? "100" : "32";
                }
                if ((graine >> 8) % 150 == 0) hc_send(g_carte, "bombeAuHasard");
                char tt[16]; snprintf(tt, sizeof tt, "%d", t);
                envoie("tour", tt);
                tours_joues++;
                int r = controle();
                if (r) { printf("   DÉFAUT %d au tour %d\n", r, t); defaut++; break; }
                if (!strcmp(global_txt("gFini"), "true")) break;
            }
            g_tenues = "";
            vagues += atoi(global_txt("gVague")) - 1;
            score += atoi(global_txt("gScore"));
            parties++;
        }
        printf("   %d parties, %d tours, %d vague(s) abattue(s), %s\n",
               parties, tours_joues, vagues, defaut ? "DES DÉFAUTS" : "aucun défaut");
        printf("   (score cumulé %d)\n", score);
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
