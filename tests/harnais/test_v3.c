/* Harnais de test minimal pour l'interpréteur v3, sans Cocoa : construit une
 * pile à la main, pose des scripts, et envoie des messages. Utilise l'hôte
 * console déjà présent par défaut dans hc_core.c. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *card2 = hc_new_card(stack, bg, "Deux");
    Object *card3 = hc_new_card(stack, bg, "Trois");
    (void)card2; (void)card3;

    Object *fld = hc_new_field(card1, "Data");
    fld->x = 10; fld->y = 10; fld->w = 200; fld->h = 100;
    hc_set_field_text(fld, "banane\npomme\ncerise");

    Object *btn = hc_new_button(card1, "GoBtn");

    hc_set_current_card(card1);

    printf("=== TEST visual + go ===\n");
    hc_set_script(btn,
        "on mouseUp\n"
        "  visual effect dissolve very fast\n"
        "  go to card 2\n"
        "  put the name of this card\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("carte courante apres go : %s\n", hc_current_card()->name);

    hc_set_current_card(card1);

    printf("\n=== TEST delete (chunk) ===\n");
    hc_set_script(btn,
        "on mouseUp\n"
        "  delete line 2 of card field \"Data\"\n"
        "  put card field \"Data\"\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    printf("\n=== TEST sort (container) ===\n");
    hc_set_field_text(fld, "banane\npomme\ncerise");
    hc_set_script(btn,
        "on mouseUp\n"
        "  sort lines of card field \"Data\"\n"
        "  put card field \"Data\"\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    printf("\n=== TEST sort (cards, descending, by name) ===\n");
    hc_set_script(btn,
        "on mouseUp\n"
        "  sort cards descending by the name of this card\n"
        "  put the name of card 1 && the name of card 2 && the name of card 3\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    printf("\n=== TEST find ===\n");
    hc_set_current_card(card1);
    hc_set_field_text(fld, "banane\npomme\ncerise");
    hc_set_script(btn,
        "on mouseUp\n"
        "  find \"cerise\"\n"
        "  put the result\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    printf("\n=== TEST answer / ask (repli console, stdin vide -> defaut) ===\n");
    hc_set_script(btn,
        "on mouseUp\n"
        "  answer \"Ca va ?\" with \"oui\" or \"non\"\n"
        "  put it\n"
        "  ask \"ton nom ?\" with \"Bob\"\n"
        "  put it\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    printf("\n=== TEST send ===\n");
    hc_set_script(card1,
        "on direBonjour x\n"
        "  put \"bonjour\" && x\n"
        "end direBonjour\n");
    hc_set_script(btn,
        "on mouseUp\n"
        "  send \"direBonjour \" & quote & \"monde\" & quote to this card\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");

    printf("\n=== bilan v3 (retours vers l'ancien interpreteur) ===\n");
    hc_v3_bilan();

    hc_free(stack);
    return 0;
}
