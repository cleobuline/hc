#include "hc_core.h"
#include <stdio.h>
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *btn = hc_new_button(card1, "Bouton 1");
    hc_set_current_card(card1);

    hc_set_script(btn,
        "on mouseUp\n"
        "  set icon of me to (2100 + random(6))\n"
        "  put the icon of me\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");
    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
