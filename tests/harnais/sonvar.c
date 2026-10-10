/* sonvar — « play » LIT UNE VARIABLE QUAND LE NOM DU SON EN EST UNE.
 *
 * Le mot était lu tel quel : « put "cling" into son » puis « play son »
 * cherchait un son nommé « son », et l'application bipait. Trouvé le 10
 * octobre dans le flipper, en donnant à « bord » un son en paramètre.
 *
 * MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II) par l'utilisatrice, le même
 * jour : « put "boing" into s / play s » fait boing. Un mot que rien ne lie
 * reste lui-même : « play tik » joue tik, comme avant.
 *
 * Le harnais tient la place de l'hôte et dit quel nom lui parvient. Les
 * notes derrière le nom (« tempo 200 c4 e4 ») ne sont pas jouées : HC ne
 * retient que le nom, voir v3_cmd_play.
 *
 * NON MESURÉ : la variable vide (« global gRien » jamais remplie, puis
 * « play gRien ») — HC transmet le nom vide à l'hôte, qui ne joue rien. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static char g_joue[256];
static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_ERR) printf("   [ERR] %s\n", t);
}
static void joue(const char *nom) { snprintf(g_joue, sizeof g_joue, "%s", nom ? nom : ""); }

static Object *g_carte;
static void cas(const char *quoi, const char *msg, const char *arg)
{
    strcpy(g_joue, "(rien)");
    if (arg) hc_send_arg(g_carte, msg, arg);
    else hc_send(g_carte, msg);
    printf("   %-44s -> « %s »\n", quoi, g_joue);
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.play_sound = joue;
    hc_set_host(&h);
    Object *st = hc_new_stack("Sons");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    g_carte = hc_new_card(st, bg, "Carte");
    hc_set_current_card(g_carte);
    hc_set_script(st,
        "on motNu\n  play tik\nend motNu\n"
        "on chaine\n  play \"cling\"\nend chaine\n"
        "on locale\n  put \"cling\" into son\n  play son\nend locale\n"
        "on parametre son\n  play son\nend parametre\n"
        "on parentheses son\n  play (son)\nend parentheses\n"
        "on globale\n  global gSon\n  put \"canard\" into gSon\n  play gSon\nend globale\n"
        "on expression\n  put \"boing\" into x\n  play x & \"3\"\nend expression\n"
        "on avecNotes\n  put \"harpsichord\" into instrument\n"
        "  play instrument tempo 200 \"c4 e4 g4\"\nend avecNotes\n"
        "on masque\n  put \"coq\" into tik\n  play tik\nend masque\n");

    puts("== 1. un mot que rien ne lie reste lui-même ==");
    cas("play tik", "motNu", NULL);
    cas("play \"cling\"", "chaine", NULL);

    puts("\n== 2. un mot qui est une variable donne sa valeur ==");
    cas("put \"cling\" into son / play son", "locale", NULL);
    cas("on parametre son / play son (\"cling\")", "parametre", "cling");
    cas("play (son) (\"cling\")", "parentheses", "cling");
    cas("global gSon / play gSon", "globale", NULL);
    cas("play x & \"3\" (x = \"boing\")", "expression", NULL);
    cas("play instrument tempo 200 \"c4 e4 g4\"", "avecNotes", NULL);

    puts("\n== 3. la variable l'emporte sur le mot, comme en HyperTalk ==");
    cas("put \"coq\" into tik / play tik", "masque", NULL);

    hc_free(st);
    return 0;
}
