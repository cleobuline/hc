#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void mon_menu(const char *item) { printf("   [HÔTE] exécute « %s »\n", item); }
static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)d; printf("   [%d] %s\n", (int)k, t); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.do_menu = mon_menu;
    h.line    = ma_ligne;
    /* on garde l'hôte console pour le reste : on ne pose que do_menu */
    hc_set_host(&h);

    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c1 = hc_new_card(st, bg, "Une");
    hc_new_card(st, bg, "Deux");
    Object *b  = hc_new_button(c1, "B");
    hc_set_current_card(c1);

    hc_set_script(st,
      "on doMenu quoi\n"
      "  if quoi is \"Clear Picture\" then\n"
      "    put \"INTERCEPTÉ : \" & quoi\n"
      "  else\n"
      "    put \"laissé passer : \" & quoi\n"
      "    pass doMenu\n"
      "  end if\n"
      "end doMenu\n"
      "\n"
      "on deleteCard\n"
      "  put \"adieu \" & the name of this card\n"
      "end deleteCard\n");

    hc_set_script(b,
      "on mouseUp\n"
      "  doMenu \"Clear Picture\"\n"
      "  doMenu \"Select All\"\n"
      "  delete this card\n"
      "end mouseUp\n");

    printf("--- doMenu intercepté / passé ---\n");
    hc_send(b, "mouseUp");
    printf("--- suppression de carte ---\n");
    hc_delete_card(hc_current_card());
    hc_free(st);
    return 0;
}
