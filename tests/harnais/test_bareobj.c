#include "hc_core.h"
#include <stdio.h>
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *btn   = hc_new_button(card1, "GoBtn");
    hc_set_current_card(card1);
    Object *f = hc_new_field(card1, "pattern");
    hc_set_field_text(f, "TEXTE-DU-CHAMP");
    hc_set_script(btn, "on mouseUp\n  put pattern\n  put the pattern\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    hc_free(stack);
    return 0;
}
