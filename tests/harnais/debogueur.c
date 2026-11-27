/* Le débogueur (la 1.1) : points d'arrêt, « debug checkpoint », pas à pas.
 *
 * L'hôte d'essai joue l'utilisatrice : à chaque arrêt, il écrit où il est
 * — l'objet, le gestionnaire, la ligne — et rend le geste suivant d'une
 * liste. Le script :
 *
 *    1  on mouseUp
 *    2    global a, c
 *    3    sousProgramme
 *    4    put 3 into c
 *    5  end mouseUp
 *    6  on sousProgramme
 *    7    global a
 *    8    put 20 into a
 *    9  end sousProgramme
 *
 * Les globales a et c laissent un second bouton, « outil », lire ce que le
 * script a fait.
 *
 * Ce qu'il tient :
 *   1. Step (PAS) reste dans le gestionnaire : 2, 3, 4 — sousProgramme
 *      s'exécute sans s'arrêter ;
 *   2. Step Into (PAS_DEDANS) y entre : 2, 3, 7, 8, puis 4 au retour ;
 *   3. Go (CONTINUE) ne s'arrête plus qu'aux points ;
 *   4. Abort (ABANDON) arrête tout le script, et le suivant repart libre ;
 *   5. « debug checkpoint » s'arrête sur sa propre ligne ;
 *   5 bis. à l'arrêt, hc_debug_variables rend les locales du gestionnaire,
 *      puis les globales ;
 *   6. un point s'ôte, se compte, et meurt avec son objet ;
 *   7. sans rappel debug_pause, rien ne s'arrête ;
 *   8. après une erreur, hc_debug_erreur_variables rend les variables du
 *      gestionnaire arrêté, figées à l'instant de l'erreur — pour le bouton
 *      « Debug » du dialogue, qui paraît quand le script est fini. Une
 *      faute de commande, dite dans un sous-gestionnaire, fige les locales
 *      de CELUI-LÀ, pas de l'appelant. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("      %s\n", t);
    else if (k == HC_ERR) printf("      [ERR] %s\n", t);
}

static const int *g_gestes = NULL;
static int g_ngestes = 0, g_i = 0;
static const char *nom_geste[] = { "continuer", "pas", "pas dedans", "abandon",
                                   "trace", "trace dedans" };

static void une_var(void *u, const char *nom, const char *val, int globale)
{
    (void)u;
    printf("         %s %s = [%s]\n", globale ? "globale" : "locale ", nom, val);
}

static int g_montre_vars = 0;

static int g_ecrit = 0;

static void trace(Object *o, const char *message, int l)
{
    if (!o) { printf("      [TRACE] fin\n"); return; }
    char qui[64];
    hc_describe(o, qui, sizeof qui);
    printf("      [TRACE] %s.%s ligne %d\n", qui, message, l);
}

static int pause(Object *o, const char *message, int l)
{
    char qui[64];
    hc_describe(o, qui, sizeof qui);
    int g = g_i < g_ngestes ? g_gestes[g_i++] : HC_DEBUG_CONTINUE;
    printf("      [ARRÊT] %s.%s ligne %d -> %s\n", qui, message, l, nom_geste[g]);
    if (g_montre_vars) hc_debug_variables(une_var, NULL);
    if (g_ecrit) {
        printf("      écrire x = 99 (locale) : %d\n", hc_debug_variable_pose("x", 0, "99"));
        printf("      écrire g = vert (globale) : %d\n", hc_debug_variable_pose("g", 1, "vert"));
        printf("      écrire absente (locale) : %d\n", hc_debug_variable_pose("absente", 0, "1"));
        printf("      écrire absente (globale) : %d\n", hc_debug_variable_pose("absente", 1, "1"));
        printf("      écrire g comme locale : %d\n", hc_debug_variable_pose("g", 0, "1"));
    }
    return g;
}

static Object *B, *T;
static void clic(const int *gestes, int n)
{
    g_gestes = gestes; g_ngestes = n; g_i = 0;
    hc_send(B, "mouseUp");
}
static void montre(const char *quoi)
{
    char s[256];
    snprintf(s, sizeof s, "on t\nglobal a, c\nput \"%s : a=\" & a & \" c=\" & c\nend t\n", quoi);
    hc_set_script(T, s);
    hc_send(T, "t");
}
static void remise(void)
{
    hc_set_script(T, "on t\nglobal a, c\nput empty into a\nput empty into c\nend t\n");
    hc_send(T, "t");
}

int main(void)
{
    static HcHost h;
    h.line = ligne;
    h.debug_pause = pause;
    hc_set_host(&h);

    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "C");
    B = hc_new_button(c, "B");
    T = hc_new_button(c, "outil");
    hc_set_current_card(c);
    hc_set_script(B,
        "on mouseUp\n"
        "  global a, c\n"
        "  sousProgramme\n"
        "  put 3 into c\n"
        "end mouseUp\n"
        "on sousProgramme\n"
        "  global a\n"
        "  put 20 into a\n"
        "end sousProgramme\n");

    puts("== 1. Step : ligne à ligne, sans entrer dans sousProgramme ==");
    hc_debug_point_pose(B, 2, 1);
    { int g[] = { HC_DEBUG_PAS, HC_DEBUG_PAS, HC_DEBUG_PAS }; clic(g, 3); }
    montre("après");

    puts("\n== 2. Step Into : dans sousProgramme, puis retour ==");
    remise();
    { int g[] = { HC_DEBUG_PAS, HC_DEBUG_PAS_DEDANS, HC_DEBUG_PAS, HC_DEBUG_PAS, HC_DEBUG_PAS };
      clic(g, 5); }
    montre("après");

    puts("\n== 3. Go : un seul arrêt, au point ==");
    remise();
    { int g[] = { HC_DEBUG_CONTINUE }; clic(g, 1); }
    montre("après");

    puts("\n== 4. Abort : tout s'arrête, c n'est pas posé ==");
    remise();
    { int g[] = { HC_DEBUG_PAS, HC_DEBUG_ABANDON }; clic(g, 2); }
    montre("après l'abandon");
    puts("   -- le clic suivant repart libre jusqu'au point, sans pas à pas hérité");
    { int g[] = { HC_DEBUG_CONTINUE }; clic(g, 1); }
    montre("après");

    puts("\n== 5. debug checkpoint s'arrête sur sa ligne ==");
    hc_debug_point_pose(B, 2, 0);
    hc_set_script(T, "on t\n  put 1 into z\n  debug checkpoint\n  put 2 into z\n  put z\nend t\n");
    { int g[] = { HC_DEBUG_PAS, HC_DEBUG_PAS }; g_gestes = g; g_ngestes = 2; g_i = 0;
      hc_send(T, "t"); }

    puts("\n== 5 bis. le Variable Watcher : locales, puis globales ==");
    hc_set_script(T, "on t\n  global a\n  put \"ici\" into loc\n  put 7 into a\n  debug checkpoint\nend t\n");
    g_montre_vars = 1;
    { int g[] = { HC_DEBUG_CONTINUE }; g_gestes = g; g_ngestes = 1; g_i = 0;
      hc_send(T, "t"); }
    g_montre_vars = 0;

    puts("\n== 6. les points : poser, compter, ôter, mourir avec l'objet ==");
    hc_debug_point_pose(B, 3, 1);
    hc_debug_point_pose(B, 4, 1);
    hc_debug_point_pose(B, 4, 1);                 /* deux fois : un seul point */
    printf("   points de B : %d\n", hc_debug_points(B, NULL, 0));
    { int l[4]; int n = hc_debug_points(B, l, 4);
      printf("   lignes :"); for (int i = 0; i < n; i++) printf(" %d", l[i]); puts(""); }
    hc_debug_point_pose(B, 3, 0);
    printf("   après en avoir ôté un : %d\n", hc_debug_points(B, NULL, 0));
    hc_debug_point_pose(B, 0, 1);                 /* ligne 0 : refusée */
    hc_debug_point_pose(NULL, 3, 1);              /* sans objet : refusé */
    printf("   après deux refus : %d\n", hc_debug_points(B, NULL, 0));
    {
        Object *autre = hc_new_button(c, "autre");
        hc_debug_point_pose(autre, 1, 1);
        printf("   points de autre : %d\n", hc_debug_points(autre, NULL, 0));
        hc_free(st);                              /* tout meurt */
        printf("   points de B après la mort de la pile : %d\n",
               hc_debug_points(B, NULL, 0));
    }

    puts("\n== 7. sans rappel debug_pause, rien ne s'arrête ==");
    st = hc_new_stack("T2");
    bg = hc_new_background(st, "F");
    c  = hc_new_card(st, bg, "C");
    B  = hc_new_button(c, "B");
    hc_set_current_card(c);
    h.debug_pause = NULL;
    hc_set_script(B, "on mouseUp\n  put 1 into q\n  debug checkpoint\n  put \"fini sans arrêt\"\nend mouseUp\n");
    hc_debug_point_pose(B, 2, 1);
    hc_send(B, "mouseUp");
    hc_free(st);

    puts("\n== 8. l'erreur fige les variables, pour le bouton Debug ==");
    st = hc_new_stack("T3");
    bg = hc_new_background(st, "F");
    c  = hc_new_card(st, bg, "C");
    B  = hc_new_button(c, "B");
    hc_set_current_card(c);
    hc_set_script(B, "on mouseUp\n  global g\n  put 5 into g\n  put 7 into x\n"
                     "  put \"ici\" into y\n  zorglub\n  put 8 into x\nend mouseUp\n");
    hc_send(B, "mouseUp");
    { const char *m = hc_debug_erreur_variables(une_var, NULL);
      printf("   gestionnaire figé : [%s]\n", m); }
    puts("   -- une faute de commande dans un sous-gestionnaire");
    hc_set_script(B, "on mouseUp\n  put 1 into haut\n  bas\nend mouseUp\n"
                     "on bas\n  put 2 into dedans\n  hide button \"Absent\"\nend bas\n");
    hc_send(B, "mouseUp");
    { const char *m = hc_debug_erreur_variables(une_var, NULL);
      printf("   gestionnaire figé : [%s]\n", m); }

    puts("\n== 9. écrire une variable pendant la pause ==");
    hc_set_script(B, "on mouseUp\n  global g\n  put 1 into x\n  put \"rouge\" into g\n"
                     "  debug checkpoint\n  put \"x=\" & x & \" g=\" & g\nend mouseUp\n");
    h.debug_pause = pause;              /* ôté au cas 7 */
    g_ecrit = 1;
    { int g[] = { HC_DEBUG_CONTINUE }; g_gestes = g; g_ngestes = 1; g_i = 0;
      hc_send(B, "mouseUp"); }
    g_ecrit = 0;
    printf("   hors pause, une locale : %d\n", hc_debug_variable_pose("x", 0, "5"));
    hc_free(st);

    puts("\n== 10. Trace, Trace Into ==");
    h.debug_trace = trace;
    st = hc_new_stack("T4");
    bg = hc_new_background(st, "F");
    c  = hc_new_card(st, bg, "C");
    B  = hc_new_button(c, "B");
    hc_set_current_card(c);
    hc_set_script(B,
        "on mouseUp\n"
        "  put 1 into x\n"
        "  sousProgramme\n"
        "  put 3 into x\n"
        "  put \"fini, x=\" & x\n"
        "end mouseUp\n"
        "on sousProgramme\n"
        "  put 20 into y\n"
        "end sousProgramme\n");
    hc_debug_point_pose(B, 2, 1);
    puts("   -- Trace depuis la ligne 2 : 3, 4, 5, sans entrer");
    { int g[] = { HC_DEBUG_TRACE }; clic(g, 1); }
    puts("   -- Trace Into : 3, 8, 4, 5");
    { int g[] = { HC_DEBUG_TRACE_DEDANS }; clic(g, 1); }
    puts("   -- un point en ligne 4 arrête la trace, Go repart libre");
    hc_debug_point_pose(B, 4, 1);
    { int g[] = { HC_DEBUG_TRACE, HC_DEBUG_CONTINUE }; clic(g, 2); }
    hc_debug_point_pose(B, 4, 0);
    puts("   -- traceDelay 2 : la trace l'attend, et finit pareil");
    T = hc_new_button(c, "outil");
    hc_set_script(T, "on t\nset the traceDelay to 2\nput the traceDelay\nend t\n");
    hc_send(T, "t");
    { int g[] = { HC_DEBUG_TRACE }; clic(g, 1); }
    h.debug_trace = NULL;
    hc_free(st);
    return 0;
}
