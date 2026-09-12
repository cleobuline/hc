#include "hc_core.h"
#include <stdio.h>
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *fld = hc_new_field(card1, "calendar");
    fld->x=10; fld->y=10; fld->w=200; fld->h=100;
    hc_set_current_card(card1);

    hc_set_script(fld,
        "function calData\n"
        "  return \"2026,12,1\"\n"
        "end calData\n"
        "on drawIt theArg\n"
        "  put \"drawIt a recu: \" & theArg\n"
        "end drawIt\n"
        "on updateCalendar\n"
        "  convert the date to dateItems\n"
        "  if item 1 to 2 of calData() <> item 1 to 2 of it then drawIt it\n"
        "  else\n"
        "    put \"branche else\"\n"
        "  end if\n"
        "end updateCalendar\n");

    hc_set_script(card1,
        "on openCard\n"
        "  send \"updateCalendar\" to card field \"calendar\"\n"
        "  pass openCard\n"
        "end openCard\n");
    hc_send(card1, "openCard");
    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
