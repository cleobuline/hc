#include "hc_core.h"
#include <stdio.h>
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *btn = hc_new_button(card1, "GoBtn");
    hc_set_current_card(card1);

    hc_set_script(btn,
        "on mouseUp\n"
        "  put \"2026,12,1,0,0,0,3\" into theDateItems\n"
        "  put theDateItems into avant\n"
        "  put 1 into item 3 of theDateItems\n"
        "  put avant into fld1temp\n"
        "  put \"avant: \" & avant\n"
        "  put \"apres: \" & theDateItems\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");
    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
