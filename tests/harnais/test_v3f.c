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
    Object *btn = hc_new_button(card1, "GoBtn");
    hc_set_current_card(card1);
    printf("id de card1 = %d\n", card1->id);
    run(btn, "card id N seul (sans &&)", "put the short name of card id 1");
    run(btn, "juste card id N", "put card id 1");
    hc_free(stack);
    return 0;
}
