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

    printf("=== go next (bouton) ===\n");
    hc_set_script(btn, "on mouseUp\n  go next\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte apres 'go next': %s (attendu Deux)\n\n", hc_current_card()->name);

    printf("=== go prev (bouton) ===\n");
    hc_set_script(btn, "on mouseUp\n  go prev\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte apres 'go prev': %s (attendu Une)\n\n", hc_current_card()->name);

    printf("=== go next card (forme complete) ===\n");
    hc_set_script(btn, "on mouseUp\n  go next card\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte apres 'go next card': %s (attendu Deux)\n\n", hc_current_card()->name);

    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
