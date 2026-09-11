#include "hc_core.h"
#include <stdio.h>
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *c1    = hc_new_card(stack, bg, "Une");
    Object *btn   = hc_new_button(c1, "B");
    Object *fc    = hc_new_field(c1, "surCarte");
    Object *fb    = hc_new_field(bg, "menu");        /* champ de FOND */
    hc_set_current_card(c1);
    hc_set_field_text(fc, "TEXTE-CARTE");
    hc_set_field_text(fb, "TEXTE-FOND");
    hc_set_script(btn,
      "on mouseUp\n"
      "  debug raz\n"
      "  put field \"surCarte\"\n"
      "  put card field \"surCarte\"\n"
      "  put field \"menu\"\n"
      "  put bg field \"menu\"\n"
      "  debug bilan\n"
      "end mouseUp\n");
    hc_send(btn, "mouseUp");
    hc_free(stack);
    return 0;
}
