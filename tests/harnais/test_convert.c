#include "hc_core.h"
#include <stdio.h>

static int g_printed_n = 0;
static void mon_print(Object **cartes, int n) {
    (void)cartes;
    g_printed_n = n;
    printf("   [hote] print_cards appele avec %d carte(s)\n", n);
}
static HcHost g_host_test;

static void run(Object *btn, const char *label, const char *body) {
    char script[2048];
    snprintf(script, sizeof script, "on mouseUp\n  %s\nend mouseUp\n", body);
    hc_set_script(btn, script);
    printf(">>> %s\n", label);
    hc_send(btn, "mouseUp");
}

int main(void)
{
    g_host_test.print_cards = mon_print;
    hc_set_host(&g_host_test);

    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *card2 = hc_new_card(stack, bg, "Deux");
    Object *card3 = hc_new_card(stack, bg, "Trois");
    (void)card2; (void)card3;
    Object *btn = hc_new_button(card1, "GoBtn");
    hc_set_current_card(card1);

    run(btn, "convert une date vers dateItems",
        "convert \"6/15/2024\" to dateItems\n  put it");

    run(btn, "convert un conteneur (variable) vers short date",
        "put \"6/15/2024\" into d\n  convert d to short date\n  put d");

    run(btn, "print carte courante",
        "print card");

    run(btn, "print toute la pile",
        "print all cards");

    run(btn, "print une plage",
        "print card 1 to 3");

    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
