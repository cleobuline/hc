#include "hc_core.h"
#include <stdio.h>
static void run(Object *btn, const char *label, const char *body) {
    char script[4096];
    snprintf(script, sizeof script, "on mouseUp\n  debug raz\n%s\n  debug bilan\nend mouseUp\n", body);
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

    run(btn, "objet inexistant", "put button 99 into x\n  put x");
    run(btn, "on clearScreen tl (choose tl tool)",
        "clearScreen \"browse\"");
    hc_set_script(btn, "on clearScreen tl\n  if tl is not empty then choose tl tool\nend clearScreen\n"
                        "on mouseUp\n  debug raz\n  clearScreen \"browse\"\n  debug bilan\nend mouseUp\n");
    hc_send(btn, "mouseUp");

    hc_free(stack);
    return 0;
}
