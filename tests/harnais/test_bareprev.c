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
    printf("=== depuis PREMIERE : go prev (court, sans 'to'/'card') ===\n");
    hc_set_script(btn, "on mouseUp\n  go prev\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Trois)\n\n", hc_current_card()->name);

    hc_set_current_card(card3);
    printf("=== depuis DERNIERE : go next (court) ===\n");
    hc_set_script(btn, "on mouseUp\n  go next\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Une)\n\n", hc_current_card()->name);

    hc_set_current_card(card1);
    printf("=== depuis PREMIERE : go previous (court) ===\n");
    hc_set_script(btn, "on mouseUp\n  go previous\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Trois)\n\n", hc_current_card()->name);

    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
