#include "hc_core.h"
#include <stdio.h>
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *fld = hc_new_field(card1, "CalField");
    fld->x = 10; fld->y = 10; fld->w = 300; fld->h = 200;
    Object *btn = hc_new_button(card1, "GoBtn");
    hc_set_current_card(card1);

    hc_set_script(fld,
        "function monthNameData\n"
        "  return \"January,February,March,April,May,June,July,August,September,October,November,December\"\n"
        "end monthNameData\n"
        "on drawCal theDateItems\n"
        "  put item 2 of theDateItems into theMonth\n"
        "  put item theMonth of monthNameData() && \"2026\" & return into me\n"
        "end drawCal\n");

    hc_set_script(btn,
        "on mouseUp\n"
        "  send \"drawCal \" & quote & \"2026,12,1,0,0,0,3\" & quote to card field \"CalField\"\n"
        "  put card field \"CalField\"\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    hc_free(stack);
    return 0;
}
