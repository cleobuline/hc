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
        "function monthNameData\n"
        "  return \"January,February,March,April,May,June,July,August,September,October,November,December\"\n"
        "end monthNameData\n"
        "on mouseUp\n"
        "  put 12 into theMonth\n"
        "  put item theMonth of monthNameData() && \"2026\" & return into me\n"
        "  put me\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    hc_free(stack);
    return 0;
}
