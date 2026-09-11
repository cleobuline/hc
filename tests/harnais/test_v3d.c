#include "hc_core.h"
#include <stdio.h>

static void run(Object *btn, const char *label, const char *body) {
    char script[2048];
    snprintf(script, sizeof script, "on mouseUp\n  put %s\nend mouseUp\n", body);
    hc_set_script(btn, script);
    printf(">>> %s : %s\n", label, body);
    hc_send(btn, "mouseUp");
}

int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *card2 = hc_new_card(stack, bg, "Deux");
    (void)card2;
    Object *btn = hc_new_button(card1, "GoBtn");
    hc_set_current_card(card1);

    run(btn, "basique",              "\"a\" && \"b\"");
    run(btn, "of a gauche",          "(the short name of card 1) && \"b\"");
    run(btn, "of a gauche sans par", "the short name of card 1 && \"b\"");
    run(btn, "of a droite",          "\"a\" && the short name of card 1");
    run(btn, "of des deux cotes",    "the short name of card 1 && the short name of card 2");
    run(btn, "avec & simple",        "the short name of card 1 & the short name of card 2");

    hc_free(stack);
    return 0;
}
