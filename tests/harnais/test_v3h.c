#include "hc_core.h"
#include <stdio.h>
static void run(Object *btn, const char *label, const char *body) {
    char script[4096];
    snprintf(script, sizeof script, "on mouseUp\n  debug raz\n  %s\n  debug bilan\nend mouseUp\n", body);
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

    run(btn, "the result (vide)", "put the result");
    run(btn, "the date", "put the date");
    run(btn, "the long date", "put the long date");
    run(btn, "the short date", "put the short date");
    run(btn, "the abbreviated date", "put the abbreviated date");
    run(btn, "the time", "put the time");
    run(btn, "the long time", "put the long time");
    run(btn, "the short time", "put the short time");
    run(btn, "the seconds", "put the seconds");
    run(btn, "the secs", "put the secs");
    run(btn, "the ticks", "put the ticks");
    run(btn, "the paramCount", "put the paramCount");
    run(btn, "the params", "put the params");
    run(btn, "the selectedText (vide)", "put the selectedText");
    run(btn, "the selection (vide)", "put the selection");
    run(btn, "the foundText (vide)", "put the foundText");
    run(btn, "the stacksInUse", "put the stacksInUse");
    run(btn, "the itemDelimiter", "put the itemDelimiter");
    run(btn, "set puis get result", "set the result to \"coucou\"\n  put the result");

    hc_free(stack);
    return 0;
}
