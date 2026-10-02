/* « the keysDown » : les touches tenues — la seconde extension de HC
 * (CLAUDE.md), accordée le 2 octobre pour les batteurs du flipper.
 *
 * Le mot et ses valeurs sont ceux de LiveCode : une touche par item, un
 * caractère par son code (« w » = 119 = charToNum("w")). L'hôte tient la
 * liste ; le noyau la demande, et ne la laisse pas poser. Un hôte qui ne la
 * sert pas lève « fonction inconnue », comme pour les autres questions qu'on
 * lui pose ; la console, elle, répond vide.
 *
 * « is among the items of », que LiveCode emploie avec elle, N'EST PAS
 * HyperTalk : HyperCard ne l'a jamais eu, et l'ajouter serait une troisième
 * extension. La tournure d'HyperTalk est « contains », virgules autour —
 * sans elles, « 11 » serait trouvé dans « 119 ». */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("      %s\n", t);
    else if (k == HC_ERR) printf("      [ERR] %s\n", t);
}

static const char *g_tenues = "110,119";
static const char *glob_lit(const char *nom)
{
    return strcasecmp(nom, "keysDown") == 0 ? g_tenues : NULL;
}

static Object *B;
static void joue(const char *s)
{
    char b[1024];
    printf("   %s\n", s);
    snprintf(b, sizeof b, "on t\n%s\nend t\n"
             "function tenue c\n"
             "  return (\",\" & the keysDown & \",\") contains (\",\" & c & \",\")\n"
             "end tenue\n", s);
    hc_set_script(B, b);
    hc_send(B, "t");
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
    B = hc_new_button(c, "s");
    hc_set_current_card(c);

    puts("== 1. un hôte qui ne sert pas keysDown ==");
    joue("put \"[\" & the keysDown & \"]\"");

    puts("\n== 2. W et N tenues ==");
    h.global_get = glob_lit;
    hc_set_host(&h);
    joue("put the keysDown");
    joue("put charToNum(\"w\")");
    joue("put tenue(119)");
    joue("put tenue(charToNum(\"w\"))");
    joue("put tenue(charToNum(\"q\"))");
    joue("put tenue(11)");
    joue("put keysDown()");

    puts("\n== 3. aucune touche ==");
    g_tenues = "";
    joue("put \"[\" & the keysDown & \"]\"");
    joue("put tenue(119)");

    puts("\n== 4. elle se lit, elle ne se pose pas ==");
    joue("set the keysDown to 119");

    puts("\n== 5. le batteur du flipper ==");
    g_tenues = "119";
    joue("if tenue(charToNum(\"w\")) then put \"batteur gauche levé\"\n"
         "if tenue(charToNum(\"n\")) then put \"batteur droit levé\"");

    hc_free(st);
    return 0;
}
