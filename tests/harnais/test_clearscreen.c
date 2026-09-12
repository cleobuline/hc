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
        "on clearScreen tl\n"
        "  choose select tool\n"
        "  doMenu \"select all\"\n"
        "  doMenu \"Clear picture\"\n"
        "  if tl <> empty then choose tl tool\n"
        "end clearScreen\n"
        "on mouseUp\n"
        "  debug raz\n"
        "  clearScreen \"browse\"\n"
        "  debug bilan\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    hc_free(stack);
    return 0;
}
