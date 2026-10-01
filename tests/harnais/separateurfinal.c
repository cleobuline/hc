/* UN SÉPARATEUR FINAL NE FAIT PAS UN MORCEAU DE PLUS — MAIS IL OUVRE UNE PLACE.
 *
 * MESURÉ DANS HYPERCARD (Basilisk II) le 1er octobre, pile Torture3 :
 *
 *     the number of items of "a,b,"   ->  2
 *
 * HC en comptait trois : hct_chunk.c affirmait que « le séparateur final crée
 * un item vide », « une dissymétrie de HyperCard, pas une inattention ». Ce
 * n'était mesuré nulle part. Les lignes, elles, ignoraient déjà le saut de
 * ligne final.
 *
 * L'AUTRE MOITIÉ, trouvée en corrigeant la première : écrire au-delà de la
 * fin remplissait d'après le COMPTE. Une fois « a,b, » réduit à deux items,
 * « put "x" into item 4 » posait deux virgules au lieu d'une — « a,b,,,x »,
 * cinq items. Et le défaut existait DÉJÀ pour les lignes : « put "X" into
 * line 4 » de « a » & return & « b » & return donnait cinq lignes, la
 * quatrième vide. Le remplissage se fait maintenant sur les EMPLACEMENTS,
 * séparateur final compris.
 *
 * Non mesurés dans HyperCard : tout ce qui suit le premier bloc. Ils suivent
 * la règle mesurée et celle des lignes. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

static void execute(const char *corps)
{
    char s[2048];
    printf("   %s\n", corps);
    snprintf(s, sizeof s, "on essaie\n%s\nend essaie\n", corps);
    hc_set_script(b, s);
    hc_send(b, "essaie");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    b = hc_new_button(c, "B");

    puts("== 1. le compte (mesuré : 2) ==");
    execute("  put the number of items of \"a,b,\"");

    puts("\n== 2. ce qui en découle à la lecture ==");
    execute("  put \"[\" & last item of \"a,b,\" & \"]\"");
    execute("  put \"[\" & item 3 of \"a,b,\" & \"]\"");
    execute("  put the number of items of \"a,b,,\"");
    execute("  put the number of items of \",\"");
    /* Le délimiteur est un réglage GLOBAL : on le rend dans le même cas,
     * sinon il fausse tous les suivants. */
    execute("  set the itemDelimiter to \";\"\n  put the number of items of \"a;b;\"\n"
            "  set the itemDelimiter to comma");
    execute("  put the number of lines of (\"a\" & return & \"b\" & return)");

    puts("\n== 3. écrire au-delà de la fin : UN séparateur, pas deux ==");
    execute("  put \"a,b,\" into v\n  put \"x\" into item 3 of v\n  put v");
    execute("  put \"a,b,\" into v\n  put \"x\" into item 4 of v\n"
            "  put v & \" -> \" & the number of items of v & \" items\"");
    execute("  put \"a,b\" into v\n  put \"x\" into item 4 of v\n"
            "  put v & \" -> \" & the number of items of v & \" items\"");
    execute("  put \"a\" & return & \"b\" & return into v\n"
            "  put \"X\" into line 4 of v\n"
            "  put the number of lines of v & \" lignes, la 4e : \" & line 4 of v");
    execute("  put \"a\" & return & \"b\" & return into v\n"
            "  put \"X\" into line 3 of v\n"
            "  put the number of lines of v & \" lignes, la 3e : \" & line 3 of v");

    hc_free(st);
    return 0;
}
