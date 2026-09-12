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
        "function calData\n"
        "  return \"2026,12,1\"\n"
        "end calData\n"
        "on mouseUp\n"
        "  convert \"6/15/2024\" to dateItems\n"
        "  put \"apres convert: \" & it\n"
        "  put calData() into x\n"
        "  put \"apres appel calData(): \" & it\n"
        "  if item 1 to 2 of calData() <> item 1 to 2 of it then put \"different\"\n"
        "  else put \"pareil\"\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");
    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
