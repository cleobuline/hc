/* xcmd — LES MODULES : des XCMD et des XFCN en WebAssembly, rangés dans la
 * pile (hc_wasm.c ; décidé le 15 octobre, docs/mesures/xcmd.txt).
 *
 * Le premier maillon que l'utilisatrice a demandé : « une XCMD "hello
 * world" dans la message box, pour valider toute la chaîne de production ».
 * bonjour.wasm est l'exemple du kit (xcmd/exemples/bonjour.c), tel quel.
 * Les autres modules (donnees/modules/, refais.sh) se conduisent mal exprès :
 * chacun éprouve un garde-fou.
 *
 * Rien n'est mesuré dans HyperCard ici : il ne connaissait pas le
 * WebAssembly. Ce qui en vient — le bloc de paramètres, SendCardMessage,
 * passFlag — est noté dans docs/mesures/xcmd.txt. */
#include "hc_core.h"
#include "hc_file.h"
#include "hc_wasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if      (k == HC_ERR)  printf("      [ERR] %s\n", t);
    else if (k == HC_MSG)  printf("      = %s\n", t);
    else if (k == HC_INFO && strstr(t, "XCMD")) printf("      %s\n", t);
}

static void fais(const char *l);

/* CMD-. L'hôte de Cocoa appelle hc_interrompre quand il voit passer la
 * touche, pendant sa respiration (hcv_guette_cmd_point) : c'est le seul
 * moment où il a la main. Celui-ci appuie de lui-même, à la respiration
 * numéro `g_touche` après qu'on l'a armé. */
static int g_touche;
static int g_respirations;

static void respire(void)
{
    g_respirations++;
    if (g_touche && --g_touche == 0) hc_interrompre();
}

static double chrono(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

/* La grâce qu'un module qui guette Button a pour rendre la main (GRACE,
 * hc_wasm.c). */
#define GRACE_HC 1.0

/* Une ligne, avec Cmd-. au bout de `n` respirations ; un délai de cinq
 * secondes en filet, s'il n'arrêtait rien. */
static double cmd_point(const char *l, int n)
{
    hc_module_delai(5);
    g_touche = n;
    g_respirations = 0;
    double t0 = chrono();
    fais(l);
    double s = chrono() - t0;
    printf("   rendue en %s, après %s respirations ; un module en cours : %d\n",
           s < 2.0 ? "moins de deux secondes" : "PLUS DE DEUX SECONDES",
           g_respirations >= n ? "ses" : "MOINS DE", hc_module_actif());
    g_touche = 0;
    hc_module_delai(0);
    return s;
}

static void fais(const char *l)
{
    printf("   %s\n", l);
    hc_do(l);
}

/* LES FENÊTRES DES MODULES : l'hôte dit ce qu'on lui demande. Il n'en
 * montre aucune ; il tient leur rectangle, pour le relire. */
static int g_rect[4] = { 10, 20, 90, 60 };
static int g_essai, g_trois;     /* leurs numéros, pour les désigner d'ici */

static int xw_ouvre(int id, const char *titre, const int r[4], int visible,
                    int style, int flottante)
{
    printf("      hôte : ouvre %d « %s » %d,%d,%d,%d, visible %d, style %d, "
           "flottante %d\n", id, titre, r[0], r[1], r[2], r[3], visible,
           style, flottante);
    memcpy(g_rect, r, sizeof g_rect);
    if (!strcmp(titre, "Essai")) g_essai = id;
    if (!strcmp(titre, "Trois")) g_trois = id;
    return 1;
}

static void xw_ferme(int id)
{
    printf("      hôte : ferme %d\n", id);
}

static void xw_image(int id, const unsigned char *px, int l, int h,
                     const unsigned char *pal, int e)
{
    printf("      hôte : image de %d, %d × %d à l'échelle %d :", id, l, h, e);
    for (int i = 0; i < l * h; i++) printf(" %d", px[i]);
    printf(" ; couleur 2 = %d,%d,%d\n", pal[6], pal[7], pal[8]);
}

static int xw_fenetre(int id, const char *quoi, const char *prop,
                      const char *valeur, char *out, int outlen)
{
    printf("      hôte : %s %d%s%s%s%s\n", quoi, id, prop ? " " : "",
           prop ? prop : "", valeur ? " = " : "", valeur ? valeur : "");
    if (!strcmp(quoi, "lit") && prop && !strcmp(prop, "rect")) {
        snprintf(out, (size_t)outlen, "%d,%d,%d,%d",
                 g_rect[0], g_rect[1], g_rect[2], g_rect[3]);
        return 1;
    }
    if (!strcmp(quoi, "lit")) return -1;
    return 1;
}

/* Un damier : la peinture d'une couche, comme la lirait l'hôte. */
static int peinture(Object *couche, const int r[4], unsigned char *noir)
{
    printf("      hôte : la peinture de %s, %d,%d,%d,%d\n",
           couche && couche->type == OBJ_CARD ? "la carte" : "UNE AUTRE COUCHE",
           r[0], r[1], r[2], r[3]);
    int k = 0;
    for (int y = r[1]; y < r[3]; y++)
        for (int x = r[0]; x < r[2]; x++) noir[k++] = (unsigned char)((x + y) & 1);
    return 1;
}

/* Le bouton de la souris : enfoncé à la lecture numéro `g_souris`. */
static int g_souris;

static const char *globale(const char *nom)
{
    if (strcmp(nom, "mouse") != 0) return NULL;
    return g_souris && --g_souris == 0 ? "down" : "up";
}

static unsigned char *lis(const char *dossier, const char *nom, size_t *n)
{
    char chemin[512];
    snprintf(chemin, sizeof chemin, "%s/%s", dossier, nom);
    FILE *f = fopen(chemin, "rb");
    if (!f) { perror(chemin); exit(2); }
    fseek(f, 0, SEEK_END);
    long t = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)t + 1);
    if (!b || fread(b, 1, (size_t)t, f) != (size_t)t) { perror(chemin); exit(2); }
    fclose(f);
    *n = (size_t)t;
    return b;
}

static void ajoute(Object *st, const char *dossier, const char *fichier,
                   const char *nom)
{
    size_t n = 0;
    unsigned char *o = lis(dossier, fichier, &n);
    char pourquoi[300] = "";
    int r = hc_module_ajoute(st, nom, o, n, pourquoi, sizeof pourquoi);
    printf("   ajoute %s (%zu octets) : %s%s\n", fichier, n,
           r == 0 ? "accepté" : "REFUSÉ — ", r == 0 ? "" : pourquoi);
    free(o);
}

static void externes(Object *st)
{
    for (int i = 0; i < hc_module_nombre(st); i++) {
        char liste[1000];
        int k = hc_module_externes(st, i, liste, sizeof liste);
        printf("   module « %s » : %d externe(s)\n", hc_module_nom(st, i), k);
        for (char *p = strtok(liste, "\n"); p; p = strtok(NULL, "\n"))
            printf("      %s\n", p);
    }
}

/* Le texte d'un fichier, pour en montrer un bout ou le réécrire abîmé. */
static char *texte(const char *chemin)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long t = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)t + 1);
    if (!b || fread(b, 1, (size_t)t, f) != (size_t)t) { fclose(f); free(b); return NULL; }
    b[t] = '\0';
    fclose(f);
    return b;
}

static void ecris(const char *chemin, const char *t)
{
    FILE *f = fopen(chemin, "wb");
    if (!f) return;
    fputs(t, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage : xcmd <dossier des modules>\n"); return 2; }
    const char *D = argv[1];
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ligne;
    h.idle = respire;
    hc_set_host(&h);
    Object *st = hc_new_stack("S");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "C");
    hc_set_current_card(c);

    puts("== 1. bonjour, le premier module : la chaîne entière ==");
    ajoute(st, D, "bonjour.wasm", "bonjour");
    externes(st);
    fais("bonjour \"Patricia\"");
    fais("bonjour");
    fais("put bonjour(\"Patricia\")");
    fais("put bonjour() into x");
    fais("put x");

    puts("\n== 2. le nom ne distingue pas les capitales, comme HyperTalk ==");
    fais("BONJOUR \"en capitales\"");
    fais("put BonJour(\"aussi\")");

    puts("\n== 3. un script du même nom passe AVANT le module ==");
    hc_set_script(st, "on bonjour qui\n  put \"le script répond à \" & qui\nend bonjour");
    fais("bonjour \"Patricia\"");
    fais("put bonjour(\"la fonction\")");     /* la fonction, elle, reste au module */
    hc_set_script(st, "");
    fais("bonjour \"de nouveau le module\"");

    puts("\n== 4. refusés à l'ajout, et la pile n'en garde rien ==");
    {
        char pourquoi[300] = "";
        static const unsigned char pas_un[] = "on mouseUp beep end mouseUp";
        int r = hc_module_ajoute(st, "texte", pas_un, sizeof pas_un - 1,
                                 pourquoi, sizeof pourquoi);
        printf("   ajoute du texte : %s %s\n", r ? "REFUSÉ —" : "accepté", pourquoi);
        r = hc_module_ajoute(st, "vide", pas_un, 0, pourquoi, sizeof pourquoi);
        printf("   ajoute un module vide : %s %s\n", r ? "REFUSÉ —" : "accepté", pourquoi);
    }
    ajoute(st, D, "inconnu.wasm", "inconnu");
    ajoute(st, D, "sansboite.wasm", "sansboite");
    printf("   la pile garde %d module(s)\n", hc_module_nombre(st));

    puts("\n== 5. les épreuves : un module qui se conduit mal ==");
    ajoute(st, D, "epreuves.wasm", "epreuves");
    externes(st);
    fais("put compte(\"a\", \"b\", \"c\")");
    fais("put compte()");
    fais("faute");
    fais("put \"le script suivant tourne\"");
    fais("put compte(\"repart\")");            /* une instance neuve */
    fais("hors");
    fais("put hors()");
    fais("put \"[\" & the result & \"]\"");
    fais("resultat");
    fais("put \"[\" & the result & \"]\"");
    fais("reentre");
    fais("mauvaise");
    fais("passe");
    fais("put compte(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)");
    fais("put compte(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17)");
    fais("passe 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17");

    puts("\n== 6. la boucle sans fin : arrêtée au délai ==");
    hc_module_delai(0.3);
    {
        double t0 = chrono();
        fais("boucle");
        double s = chrono() - t0;
        printf("   rendue en %s\n", s < 2.0 ? "moins de deux secondes" : "PLUS DE DEUX SECONDES");
        printf("   un module en cours, après : %d\n", hc_module_actif());
    }
    hc_module_delai(0);
    fais("bonjour \"après la boucle\"");

    /* Le module calcule dans un fil à part, et le fil de HC respire comme
     * dans un « wait » : Cmd-. y arrive par le chemin de tous les scripts,
     * et l'arrêt ne se dit pas — il n'y a pas de faute. */
    puts("\n== 6 bis. Cmd-. arrête un module ==");
    puts("   -- tapé dans la boîte de messages :");
    cmd_point("boucle", 6);
    fais("put \"la boîte de messages répond\"");
    puts("   -- dans un gestionnaire : la suite ne tourne pas");
    hc_set_script(st, "on essai\n  boucle\n  put \"JAMAIS\"\nend essai");
    cmd_point("essai", 6);
    puts("   -- pendant un « wait » que le module a demandé :");
    cmd_point("patiente", 6);
    puts("   -- un module qui ne fait qu'envoyer des lignes :");
    cmd_point("bavarde", 6);
    puts("   -- cantAbort : la touche ne fait rien, le délai l'arrête");
    fais("set the cantAbort of this stack to true");
    {
        hc_module_delai(0.3);
        g_touche = 3;
        fais("boucle");
        g_touche = 0;
        hc_module_delai(0);
    }
    fais("set the cantAbort of this stack to false");
    hc_set_script(st, "");
    fais("bonjour \"après Cmd-.\"");

    puts("\n== 7. enregistrer, relire : le module voyage dans la pile ==");
    {
        char chemin[] = "/tmp/xcmd_XXXXXX";
        int fd = mkstemp(chemin);
        if (fd < 0) { perror("mkstemp"); return 1; }
        close(fd);
        printf("   hc_save : %d\n", hc_save(st, chemin));
        char *t = texte(chemin);
        const char *m = t ? strstr(t, "module \"bonjour\"") : NULL;
        if (m) {
            const char *fin = strchr(m, '\n');
            const char *l2 = fin ? strchr(fin + 1, '\n') : NULL;
            printf("   le fichier : %.*s\n", (int)(fin - m), m);
            if (fin && l2) printf("                %.24s…\n", fin + 1);
        } else {
            puts("   le fichier n'a PAS de bloc « module »");
        }
        Object *relue = hc_load(chemin);
        printf("   hc_load : %s, %d module(s)\n", relue ? "relue" : hc_load_erreur(),
               relue ? hc_module_nombre(relue) : 0);
        if (relue) {
            size_t n1 = 0, n2 = 0;
            const unsigned char *a = hc_module_octets(st, 0, &n1);
            const unsigned char *b = hc_module_octets(relue, 0, &n2);
            printf("   les octets de bonjour : %s\n",
                   n1 == n2 && !memcmp(a, b, n1) ? "identiques" : "DIFFÉRENTS");
            hc_unregister_stack(st);
            hc_register_stack(relue);
            Object *cr = NULL;
            for (int i = 0; i < relue->nparts && !cr; i++)
                if (relue->parts[i]->type == OBJ_CARD) cr = relue->parts[i];
            hc_set_current_card(cr);
            fais("bonjour \"depuis la pile relue\"");
            hc_unregister_stack(relue);
            hc_register_stack(st);
            hc_set_current_card(c);
            hc_free(relue);
        }

        /* Un chiffre hexadécimal abîmé : la pile est refusée, et le dit. */
        if (t && m) {
            char *z = strstr(t, "module \"bonjour\"");
            char *l = strchr(z, '\n');
            if (l && l[1] == '|' && l[3]) l[3] = 'Z';
            ecris(chemin, t);
            Object *x = hc_load(chemin);
            printf("   un octet abîmé : %s\n", x ? "RELUE" : hc_load_erreur());
            if (x) hc_free(x);
        }
        /* Un bloc sans sa fin : une troncature. */
        if (t) {
            char *z = strstr(t, "end module");
            if (z) {
                *z = '\0';
                ecris(chemin, t);
                Object *x = hc_load(chemin);
                printf("   un bloc sans sa fin : %s\n", x ? "RELUE" : hc_load_erreur());
                if (x) hc_free(x);
            }
        }
        free(t);
        unlink(chemin);
    }

    puts("\n== 8. retirer un module ==");
    printf("   hc_module_retire(bonjour) : %d\n", hc_module_retire(st, "bonjour"));
    fais("bonjour \"Patricia\"");
    printf("   la pile garde %d module(s)\n", hc_module_nombre(st));

    /* Les rappels jouent sur le fil de HC, comme la ligne de SendCardMessage,
     * et rendent une chaîne que le module fait copier chez lui. */
    puts("\n== 9. les rappels de HyperCard ==");
    {
        Object *nom = hc_new_field(c, "nom");
        Object *fond = hc_new_field(bg, "fond");
        hc_set_field_text(nom, "le texte du champ de carte");
        hc_set_field_text(fond, "le texte du champ de fond");
        ajoute(st, D, "rappels.wasm", "rappels");
        puts("   -- EvalExpr");
        fais("put evalue(\"1 + 2\")");
        fais("put evalue(\"the number of cards\")");
        fais("put evalue(\"1 +\")");
        fais("put evalue(\"zorglub(\")");
        fais("put evalue(\"evalue(1)\")");      /* le module est déjà en cours */
        hc_set_script(st, "on essai\n  put 41 into x\n"
                          "  put evalue(\"x + 1\")\nend essai\n"
                          "on lit\n  global g1\n  put g1\nend lit\n"
                          "on compile\n  global pln0, pln3\n  put \"\" into pln0\n"
                          "  put tables(\"Conway\") into p\n"
                          "  put p & \" ; \" & pln0 & \" ; \" & pln3\nend compile");
        fais("essai");                            /* une locale du gestionnaire */
        puts("   -- GetGlobal, SetGlobal");
        fais("poseGlobale \"g1\", \"valeur un\"");
        fais("put globale(\"g1\")");
        fais("lit");
        fais("put \"[\" & globale(\"inconnue\") & \"]\"");
        fais("compile");                          /* comme le FORTH de MAC•CAM */
        puts("   -- GetFieldBy…, SetFieldBy…");
        fais("put champ(1, \"nom\")");
        fais("put champ(0, \"fond\")");
        fais("put champ(1, \"fond\")");         /* pas sur la carte */
        fais("put champNum(1, 1)");
        char l[200];
        snprintf(l, sizeof l, "put champID(0, %d)", fond->id);
        fais(l);
        fais("put champNum(1, 2)");
        fais("poseChamp 1, \"nom\", \"écrit par le module\"");
        fais("put the result && fld \"nom\"");
        fais("poseChamp 1, \"absent\", \"x\"");
        fais("put the result");
        fais("poseChampNum 0, 1, \"le fond, par son numéro\"");
        fais("put bg fld \"fond\"");
        snprintf(l, sizeof l, "poseChampID 1, %d, \"la carte, par son id\"", nom->id);
        fais(l);
        fais("put fld \"nom\"");
        puts("   -- dix mille EvalExpr, rendus par free : pages gagnées");
        fais("put memoire()");
        hc_set_script(st, "");
    }

    /* Le premier module de MAC•CAM, appelé comme le fait la pile : les
     * globales vidées, puis « put FORTH(…) » ; les tables arrivent dans
     * pln0 à pln3. Le compilateur lui-même a son harnais (forth). */
    puts("\n== 10. le FORTH de MAC•CAM ==");
    ajoute(st, D, "forth.wasm", "FORTH");
    hc_set_script(st, "on compile f\n  global pln0, pln1\n"
                      "  put empty into pln0\n  put empty into pln1\n"
                      "  put FORTH(f) into p\n"
                      "  put \"[\" & p & \"] \" & length(pln0) && char 1 to 16 of pln0"
                      " && length(pln1)\nend compile");
    fais("compile \": R CENTER >PLN0 ; MAKE-TABLE R\"");
    fais("compile \"N/VONN : R CENTER' >PLN0 NORTH >PLN1 ; MAKE-TABLE R\"");
    fais("compile \"1 2 3\"");
    fais("compile \": R EAST >PLN0 ; MAKE-TABLE R ZORGLUB\"");   /* faute après la table */
    hc_set_script(st, "");

    /* LES FENÊTRES : l'XFCN qui l'ouvre en reçoit les événements, comme
     * MACCAM. Sans les rappels xw_ de l'hôte, aucune ne s'ouvre. */
    puts("\n== 11. les fenêtres d'un module ==");
    ajoute(st, D, "fenetres.wasm", "fenetres");
    fais("put fenetre(\"Essai\")");                   /* pas d'hôte : 0 */
    h.xw_ouvre = xw_ouvre;
    h.xw_ferme = xw_ferme;
    h.xw_image = xw_image;
    h.xw_fenetre = xw_fenetre;
    h.peinture = peinture;
    h.global_get = globale;
    puts("   -- ouverte : xOpenEvt arrive quand l'appel est fini");
    fais("put fenetre(\"Essai\")");
    fais("put there is a window \"essai\" && there is a window \"Absente\"");
    puts("   -- ses propriétés : le module d'abord, HC pour ce qu'il passe");
    fais("set the compte of window \"Essai\" to 42");
    fais("put the compte of window \"Essai\" && the ouvertures of window \"Essai\"");
    fais("put the rect of window \"Essai\"");
    fais("put the name of window \"Essai\"");
    fais("put the zorglub of window \"Essai\"");
    fais("hide window \"Essai\"");
    puts("   -- ses messages : chacun éprouve un rappel");
    fais("send \"dessine\" to window \"Essai\"");
    fais("send copie to window \"Essai\"");
    fais("send \"peinture\" to window \"Essai\"");
    fais("send \"largeur\" to window \"Essai\"");     /* HC sert : le module tourne */
    fais("send \"reentre\" to window \"Essai\"");
    fais("send \"inconnu\" to window \"Essai\"");      /* passé : rien */
    fais("send \"dessine\" to window \"Absente\"");
    g_souris = 3;
    fais("send \"guette\" to window \"Essai\"");      /* le clic l'arrête */
    puts("   -- un autre module ne dessine pas dans sa fenêtre");
    ajoute(st, D, "autre.wasm", "autre");
    {
        char l[64];
        snprintf(l, sizeof l, "dessineAutre %d", g_essai);
        fais(l);
    }
    fais("put there is a window \"Essai\"");
    puts("   -- renommée : le nom suit");
    fais("set the name of window \"Essai\" to \"CAM\"");
    fais("put there is a window \"Essai\" && there is a window \"CAM\"");
    fais("send \"dessine\" to window \"CAM\"");
    puts("   -- Cmd-. : un module qui guette Button s'arrête de lui-même, et la garde");
    printf("   une seconde de grâce consommée : %s\n",
           cmd_point("send \"guette\" to window \"CAM\"", 6) >= GRACE_HC
           ? "OUI" : "non, rendue avant");
    fais("put there is a window \"CAM\" && the compte of window \"CAM\"");
    puts("   -- Cmd-. : un module qui ne guette pas est arrêté, et la perd");
    cmd_point("send \"boucle\" to window \"CAM\"", 6);
    fais("put there is a window \"CAM\"");
    puts("   -- Cmd-. : un module qui guette sans s'arrêter l'est après sa grâce");
    fais("put fenetre(\"Sourd\")");
    printf("   une seconde de grâce consommée : %s\n",
           cmd_point("send \"sourd\" to window \"Sourd\"", 6) >= GRACE_HC
           ? "oui" : "NON");
    fais("put there is a window \"Sourd\"");
    puts("   -- une faute dans un événement la ferme aussi");
    fais("put fenetre(\"Essai\")");
    fais("send \"faute\" to window \"Essai\"");
    fais("put there is a window \"Essai\"");
    puts("   -- fermée par le module, par close window, par sa case");
    fais("put fenetre(\"Un\") && fenetre(\"Deux\") && fenetre(\"Trois\")");
    fais("send \"ferme\" to window \"Un\"");
    fais("close window \"Deux\"");
    printf("   la case de « Trois »\n");
    hc_xw_fermee(g_trois);
    fais("get fenetre(\"Quatre\")");
    fais("put there is a window \"Un\" && there is a window \"Deux\" "
         "&& there is a window \"Trois\" && there is a window \"Quatre\"");
    puts("   -- ouverte pendant un événement : xOpenEvt suit l'événement");
    fais("send \"fille\" to window \"Quatre\"");
    puts("   -- ouverte, puis un AUTRE module appelé dans le même appel :");
    puts("      l'autre tourne, et xOpenEvt attend la fin de l'appel");
    fais("put fenetre(\"Cinq\", \"dessineAutre 0\")");
    fais("put there is a window \"Fille\" && there is a window \"Cinq\"");
    puts("   -- le module remplacé : ses fenêtres partent avec son instance");
    ajoute(st, D, "fenetres.wasm", "fenetres");
    fais("put there is a window \"Quatre\"");
    h.global_get = NULL;

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
