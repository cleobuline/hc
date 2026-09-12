#include "hc_core.h"
#include <stdio.h>

static void run(Object *btn, const char *label, const char *body) {
    char script[2048];
    snprintf(script, sizeof script, "on mouseUp\n  %s\nend mouseUp\n", body);
    hc_set_script(btn, script);
    printf(">>> %s\n", label);
    hc_send(btn, "mouseUp");
}

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

    run(btn, "designateur arithmetique: card i+1",
        "put \"i\" ; put 1 into i\n  put the short name of card (i + 1)");
    run(btn, "designateur variable directe: card i sans parens",
        "put 2 into i\n  put the short name of card i");
    run(btn, "designateur numerique simple toujours ok",
        "put the short name of card 2");
    run(btn, "id designateur avec concat apres",
        "put the short name of card id 1 && \"suffixe\"");
    run(btn, "nom cite avec concat apres",
        "put the short name of card \"Une\" && \"suffixe\"");
    run(btn, "get + put it : chaine normale",
        "get the short name of card 1 & \"-\" & the short name of card 3\n  put it");

    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
