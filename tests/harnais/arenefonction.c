/* L'ARÈNE SE REND APRÈS UN APPEL DE FONCTION.
 *
 * v3_fonction_pile prenait dans l'arène une ligne de HC_VAL par argument
 * sans noter le sommet : elle n'était rendue qu'à la fin du gestionnaire
 * tout entier. Une boucle qui appelle une fonction de la pile finissait par
 * saturer le gigaoctet de l'arène — mesuré le 2 octobre entre 16 000 et
 * 20 000 appels de « get f(1) », dans HC 0.7.6 comme après. La boucle d'un
 * jeu à trente images par seconde y arrivait en deux minutes. Trouvé par le
 * joueur automatique du flipper de l'utilisatrice.
 *
 * Même défaut, même soir, dans intersect() et within(), qui venaient d'être
 * écrites : resolve puise dans l'arène, et rien ne la rendait.
 *
 * Chaque boucle fait 25 000 tours dans UN SEUL gestionnaire — l'arène se
 * vide à la fin de chaque message, c'est pourquoi une boucle par message ne
 * voyait rien. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static int g_fautes = 0;
static char g_premiere[256];
static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_ERR && g_fautes++ == 0)
        snprintf(g_premiere, sizeof g_premiere, "%s", t);
    if (k == HC_MSG) printf("      %s\n", t);
}

static Object *B;
static void boucle(const char *corps)
{
    char s[1024];
    g_fautes = 0;
    g_premiere[0] = '\0';
    snprintf(s, sizeof s,
             "on t\n  put 0 into n\n  repeat 25000 times\n%s\n    add 1 to n\n"
             "  end repeat\n  put n\nend t\n"
             "function f a\n  return a + 1\nend f\n", corps);
    hc_set_script(B, s);
    printf("   %s\n", corps);
    hc_send(B, "t");
    if (g_fautes) printf("      *** %d fautes, la première : %s\n", g_fautes, g_premiere);
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "C");
    hc_new_button(c, "p");
    hc_new_button(c, "q");
    B = hc_new_button(c, "s");
    hc_set_current_card(c);

    puts("== 25 000 appels dans un seul gestionnaire ==");
    boucle("    get f(1)");
    boucle("    put f(n) into v");
    boucle("    get intersect(button \"p\", button \"q\")");
    boucle("    get within(button \"p\", \"1,1\")");
    puts("\n== témoins, qui ne fuyaient pas ==");
    boucle("    get abs(-1)");
    boucle("    get the points of button \"p\"");

    hc_free(st);
    return 0;
}
