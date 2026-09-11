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

    Object *f1 = hc_new_field(card1, "IncremU");
    hc_set_field_text(f1, "5");
    Object *bb = hc_new_button(bg, "\xe2\x80\x9cDepth\xe2\x80\x9d"); /* curly quotes UTF-8 */

    run(btn, "hilite of bg btn avec guillemets typographiques",
        "put hilite of bg btn \"\xe2\x80\x9cDepth\xe2\x80\x9d\" into dep\n  put dep");

    run(btn, "the value of fld", "put the value of fld \"IncremU\" into incU\n  put incU");

    run(btn, "fld tout court comme expression", "put fld \"IncremU\" into incU\n  put incU");

    run(btn, "bg btn ordinaire", "put the hilite of bg btn \"GoBtn2\" into x\n  put x");

    (void)bb;
    hc_free(stack);
    return 0;
}
