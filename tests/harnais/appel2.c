#include "hc_core.h"
#include <stdio.h>
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *c1    = hc_new_card(stack, bg, "Une");
    Object *btn   = hc_new_button(c1, "B1");
    hc_set_current_card(c1);
    hc_set_script(c1,
      "on drawChart\n"
      "  put \"avant\"\n"
      "  put the width of card field \"nExistePas\" into z\n"
      "  put \"apres\"\n"
      "end drawChart\n");
    hc_set_script(btn,
      "on mouseUp\n"
      "debug raz\n"
      "  drawChart\n"
      "debug bilan\n"
      "end mouseUp\n");
    hc_send(btn, "mouseUp");
    hc_free(stack);
    return 0;
}
