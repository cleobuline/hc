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
        "  if (it is in \"1,3,5,7,8,10\") or (it = 12) then return 31\n"
        "  else if (it is in \"4,6,9,11\") then return 30\n"
        "  else\n"
        "    if (item 1 of theDateItems mod 400 = 0) or (item 1 of theDateItems mod 100 <> 0) and (item 1 of theDateItems mod 4 = 0) then return 29\n"
        "    else return 28\n"
        "  end if\n"
        "end daysInMonth\n"
        "function dayNameData\n"
        "  return \"Sun Mon Tue Wed Thu Fri Sat,4\"\n"
        "end dayNameData\n"
        "function spaces\n"
        "  return \"                                        \"\n"
        "end spaces\n"
        "on mouseUp\n"
        "  put \"2026,12,1,0,0,0,3\" into theDateItems\n"
        "  put \"[0] \" & theDateItems\n"
        "  put item 1 of theDateItems into theYear\n"
        "  put \"[1] \" & theDateItems\n"
        "  put item 2 of theDateItems into theMonth\n"
        "  put \"[2] \" & theDateItems\n"
        "  put item 3 of theDateItems into theDay\n"
        "  put \"[3] \" & theDateItems\n"
        "  put daysInMonth(theDateItems) into daysInThisMonth\n"
        "  put \"[4] \" & theDateItems\n"
        "  put last item of dayNameData() into colWidth\n"
        "  put \"[5] \" & theDateItems\n"
        "  put char 1 to colWidth of spaces() into emptyColumn\n"
        "  put \"[6] \" & theDateItems\n"
        "  put 1 into item 3 of theDateItems\n"
        "  put \"[7] \" & theDateItems\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");
    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
