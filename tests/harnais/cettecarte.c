/* cettecarte — « this card of stack "X" » EST LA CARTE QUE MONTRE X.
 *
 * Les articles du menu « Reference » de HyperCard Help font
 *
 *     go this card of stack "Help Extras" in a new window
 *
 * HC y lisait la carte courante de la pile COURANTE : le go restait sur
 * place, et la pile choisie ne venait jamais devant. Signalé le 10 octobre
 * DANS HC (l'application).
 *
 * La carte d'une autre pile est la dernière qu'on y a visitée — le noyau la
 * tient de son historique, alimenté par openCard — ou sa première carte si
 * l'on n'y est jamais allé.
 *
 * DÉDUIT DU CODE D'APPLE, qui ne ferait rien autrement. NON MESURÉ dans
 * HyperCard : la section 4, les relatifs (« next card of stack "X" »), qui
 * suivent la même carte. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      = %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static void fais(const char *l) { printf("   %s\n", l); hc_do(l); }

static void ou(void)
{
    char d[96]; hc_describe(hc_current_card(), d, sizeof d);
    printf("      -> %s de « %s »\n", d, hc_current_card()->owner->name);
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);

    Object *a = hc_new_stack("Aide"); hc_register_stack(a);
    Object *abg = hc_new_background(a, "F");
    Object *a1 = hc_new_card(a, abg, "A1");
    hc_new_card(a, abg, "A2");
    Object *t = hc_new_stack("Talk"); hc_register_stack(t);
    Object *tbg = hc_new_background(t, "G");
    hc_new_card(t, tbg, "B1");
    hc_new_card(t, tbg, "B2");
    hc_new_card(t, tbg, "B3");
    hc_set_current_card(a1);

    puts("== 1. une pile jamais visitée montre sa première carte ==");
    fais("put the short name of this card of stack \"Talk\"");
    fais("put the short name of this card of stack \"Aide\"");

    puts("\n== 2. puis la dernière qu'on y a visitée ==");
    fais("go card \"B2\" of stack \"Talk\"");
    fais("go card \"A2\" of stack \"Aide\"");
    fais("put the short name of this card of stack \"Talk\"");
    fais("put the short name of this card");

    puts("\n== 3. le go du menu Reference amène la pile choisie ==");
    fais("go this card of stack \"Talk\" in a new window");
    ou();
    fais("go this card of stack \"Aide\"");
    ou();

    puts("\n== 4. non mesuré : les relatifs suivent la même carte ==");
    fais("put the short name of next card of stack \"Talk\"");
    fais("put the number of cards of stack \"Talk\"");

    hc_unregister_stack(a); hc_free(a);
    hc_unregister_stack(t); hc_free(t);
    return 0;
}
