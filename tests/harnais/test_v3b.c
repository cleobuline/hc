#include "hc_core.h"
#include <stdio.h>

int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *card2 = hc_new_card(stack, bg, "Deux");
    Object *card3 = hc_new_card(stack, bg, "Trois");
    (void)card2; (void)card3;
    Object *btn = hc_new_button(card1, "GoBtn");
    hc_set_current_card(card1);

    printf("=== isole : the name of card 1 seul ===\n");
    hc_set_script(btn,
        "on mouseUp\n"
        "  put the name of card 1\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    printf("\n=== isole : concatenation de 3 noms ===\n");
    hc_set_script(btn,
        "on mouseUp\n"
        "  put the name of card 1 && the name of card 2 && the name of card 3\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    printf("\n=== isole : sans sort avant, juste la carte 1 ===\n");
    hc_set_script(btn,
        "on mouseUp\n"
        "  put card 1\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    hc_free(stack);
    return 0;
}
