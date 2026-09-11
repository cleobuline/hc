#include "hc_core.h"
#include <stdio.h>

static void run(Object *btn, const char *script) {
    hc_set_script(btn, script);
    hc_send(btn, "mouseUp");
}

int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *card2 = hc_new_card(stack, bg, "Deux");
    Object *card3 = hc_new_card(stack, bg, "Trois");

    Object *fld = hc_new_field(card1, "Data");
    fld->x = 10; fld->y = 10; fld->w = 200; fld->h = 100;
    hc_set_field_text(fld, "banane\npomme\ncerise");

    Object *btn = hc_new_button(card1, "GoBtn");
    hc_set_current_card(card1);

    printf("### 1) visual + go\n");
    run(btn,
        "on mouseUp\n"
        "  visual effect dissolve very fast\n"
        "  go to card 2\n"
        "  put the short name of this card\n"
        "end mouseUp\n");
    printf("--> carte courante: %s (attendu: Deux)\n\n", hc_current_card()->name);

    hc_set_current_card(card1);
    printf("### 2) delete (chunk)\n");
    hc_set_field_text(fld, "banane\npomme\ncerise");
    run(btn,
        "on mouseUp\n"
        "  delete line 2 of card field \"Data\"\n"
        "  put card field \"Data\"\n"
        "end mouseUp\n");
    printf("--> attendu: banane puis cerise\n\n");

    printf("### 3) sort (container, lines)\n");
    hc_set_field_text(fld, "banane\npomme\ncerise");
    run(btn,
        "on mouseUp\n"
        "  sort lines of card field \"Data\"\n"
        "  put card field \"Data\"\n"
        "end mouseUp\n");
    printf("--> attendu: banane, cerise, pomme\n\n");

    printf("### 4) sort (cards, descending, by short name) -- evite && sur 'of'\n");
    run(btn,
        "on mouseUp\n"
        "  sort cards descending by the short name of this card\n"
        "  put the short name of card 1\n"
        "  put the short name of card 2\n"
        "  put the short name of card 3\n"
        "end mouseUp\n");
    printf("--> attendu (desc alpha Une>Trois>Deux): Une / Trois / Deux\n\n");

    printf("### 5) find\n");
    hc_set_current_card(card1);
    hc_set_field_text(fld, "banane\npomme\ncerise");
    run(btn,
        "on mouseUp\n"
        "  find \"cerise\"\n"
        "  put the result\n"
        "  put the foundText\n"
        "end mouseUp\n");
    printf("--> attendu: (vide, succes) puis cerise\n\n");

    printf("### 6) answer / ask (stdin vide -> defaut/annulation)\n");
    run(btn,
        "on mouseUp\n"
        "  answer \"Ca va ?\" with \"oui\" or \"non\"\n"
        "  put it\n"
        "  ask \"ton nom ?\" with \"Bob\"\n"
        "  put it\n"
        "end mouseUp\n");
    printf("\n");

    printf("### 7) send\n");
    hc_set_script(card1,
        "on direBonjour x\n"
        "  put \"bonjour\" && x\n"
        "end direBonjour\n");
    run(btn,
        "on mouseUp\n"
        "  send \"direBonjour \" & quote & \"monde\" & quote to this card\n"
        "end mouseUp\n");
    printf("--> attendu: bonjour monde\n\n");

    printf("### 8) precedence 'of' / '&&' -- test isole, hors de mes ports\n");
    run(btn,
        "on mouseUp\n"
        "  put (the short name of card 1) && (the short name of card 2)\n"
        "end mouseUp\n");
    printf("--> avec parentheses, attendu: Une Deux\n");
    run(btn,
        "on mouseUp\n"
        "  put the short name of card 1 && the short name of card 2\n"
        "end mouseUp\n");
    printf("--> sans parentheses (le cas suspect)\n\n");

    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
