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
        "function daysInMonth theDateItems\n"
        "  get item 2 of theDateItems\n"
        "  return it & \"-days\"\n"
        "end daysInMonth\n"
        "on drawCal theDateItems\n"
        "  put \"[0] \" & theDateItems\n"
        "  put item 1 of theDateItems into theYear\n"
        "  put \"[1] \" & theDateItems\n"
        "  put daysInMonth(theDateItems) into daysInThisMonth\n"
        "  put \"[2] \" & theDateItems\n"
        "  put 1 into item 3 of theDateItems\n"
        "  put \"[3] \" & theDateItems\n"
        "end drawCal\n"
        "on mouseUp\n"
        "  drawCal 2026,12,1,0,0,0,3\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");
    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
