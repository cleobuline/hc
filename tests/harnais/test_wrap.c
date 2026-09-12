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
    printf("=== depuis la PREMIERE, go to prev card (doit boucler sur Trois) ===\n");
    hc_set_script(btn, "on mouseUp\n  go to prev card\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Trois)\n\n", hc_current_card()->name);

    hc_set_current_card(card1);
    printf("=== go to prev card, encore, encore (Trois -> Deux -> Une) ===\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Trois)\n", hc_current_card()->name);
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Deux)\n", hc_current_card()->name);
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Une)\n\n", hc_current_card()->name);

    hc_set_current_card(card3);
    printf("=== depuis la DERNIERE, go to next card (doit boucler sur Une) ===\n");
    hc_set_script(btn, "on mouseUp\n  go to next card\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Une)\n\n", hc_current_card()->name);

    hc_free(stack);
    return 0;
}
