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
    Object *fld   = hc_new_field(card1, "Data");
    fld->x = 10; fld->y = 10; fld->w = 200; fld->h = 100;
    hc_set_field_text(fld, "banane\npomme\ncerise");
    Object *btn = hc_new_button(card1, "GoBtn");
    hc_set_current_card(card1);

    run(btn, "set name d'un objet",
        "set the name of card field \"Data\" to \"Fruits\"\n"
        "  put the short name of card field \"Fruits\"");

    run(btn, "set visible", "set the visible of card field \"Fruits\" to false\n"
        "  put the visible of card field \"Fruits\"");

    run(btn, "set locktext (booleen)", "set the locktext of card field \"Fruits\" to true\n"
        "  put the locktext of card field \"Fruits\"");

    run(btn, "set textStyle sur un objet entier", "set the textStyle of card field \"Fruits\" to bold\n"
        "  put the textStyle of card field \"Fruits\"");

    run(btn, "set textStyle sur un morceau (plage de style)",
        "set the textStyle of word 1 of card field \"Fruits\" to bold,italic\n"
        "  put the textStyle of word 1 of card field \"Fruits\"");

    run(btn, "set global : itemDelimiter", "set itemDelimiter to \"|\"\n"
        "  put item 2 of \"a|b|c\"");

    run(btn, "set global : lockScreen", "set lockScreen to true\n"
        "  put the lockScreen");

    run(btn, "set geometrie (rect via width)", "set the width of card field \"Fruits\" to 250\n"
        "  put the width of card field \"Fruits\"");

    run(btn, "set propriete inconnue -> erreur propre", "set the frobnicate of card field \"Fruits\" to 3");

    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
