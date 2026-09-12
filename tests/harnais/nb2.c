#include "hc_core.h"
#include <stdio.h>
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *c1    = hc_new_card(stack, bg, "Une");
    hc_new_card(stack, bg, "Deux");
    hc_new_background(stack, "Fond2");
    Object *btn = hc_new_button(c1, "B1");
    hc_new_button(c1, "B2");
    hc_new_button(bg, "BgB1");
    hc_new_field(c1, "F1");
    hc_new_field(bg, "BgF1");
    hc_new_field(bg, "BgF2");
    hc_set_current_card(c1);
    hc_set_script(btn,
      "on mouseUp\n"
      "  put \"cartes=\" & the number of cards\n"
      "  put \"fonds=\" & the number of backgrounds\n"
      "  put \"boutons=\" & the number of buttons\n"
      "  put \"cd boutons=\" & the number of card buttons\n"
      "  put \"bg boutons=\" & the number of bg buttons\n"
      "  put \"champs=\" & the number of fields\n"
      "  put \"cd champs=\" & the number of card fields\n"
      "  put \"bg champs=\" & the number of bg fields\n"
      "  put \"parts=\" & the number of parts\n"
      "  put \"rang de F1=\" & the number of card field \"F1\"\n"
      "end mouseUp\n");
    hc_send(btn, "mouseUp");
    hc_free(stack);
    return 0;
}
