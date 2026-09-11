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

    run(btn, "write simple", "write \"bonjour\" to file \"/tmp/hc_test_rw.txt\"");
    run(btn, "open + write + close", "open file \"/tmp/hc_test_rw2.txt\"\n"
        "  write \"ligne1\" & return & \"ligne2\" to file \"/tmp/hc_test_rw2.txt\"\n"
        "  close file \"/tmp/hc_test_rw2.txt\"");
    run(btn, "read from file for N (grammaire couverte)",
        "open file \"/tmp/hc_test_rw2.txt\"\n"
        "  read from file \"/tmp/hc_test_rw2.txt\" for 6\n"
        "  put it\n"
        "  close file \"/tmp/hc_test_rw2.txt\"");
    run(btn, "read ... at N for M (grammaire NON couverte ?)",
        "open file \"/tmp/hc_test_rw2.txt\"\n"
        "  read from file \"/tmp/hc_test_rw2.txt\" at 8 for 6\n"
        "  put it\n"
        "  close file \"/tmp/hc_test_rw2.txt\"");
    run(btn, "write ... at end (grammaire NON couverte ?)",
        "open file \"/tmp/hc_test_rw2.txt\"\n"
        "  write \"FIN\" to file \"/tmp/hc_test_rw2.txt\" at end\n"
        "  close file \"/tmp/hc_test_rw2.txt\"");

    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
