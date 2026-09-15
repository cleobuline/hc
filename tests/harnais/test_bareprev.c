#include "hc_core.h"
#include <stdio.h>
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *card2 = hc_new_card(stack, bg, "Deux");
    Object *card3 = hc_new_card(stack, bg, "Trois");
    (void)card2; (void)card3;
    Object *btn = hc_new_button(card1, "GoBtn");

    hc_set_current_card(card1);
    printf("=== depuis PREMIERE : go prev (court, sans 'to'/'card') ===\n");
    hc_set_script(btn, "on mouseUp\n  go prev\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Trois)\n\n", hc_current_card()->name);

    hc_set_current_card(card3);
    printf("=== depuis DERNIERE : go next (court) ===\n");
    hc_set_script(btn, "on mouseUp\n  go next\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Une)\n\n", hc_current_card()->name);

    hc_set_current_card(card1);
    printf("=== depuis PREMIERE : go previous (court) ===\n");
    hc_set_script(btn, "on mouseUp\n  go previous\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Trois)\n\n", hc_current_card()->name);

    /* ─── UN MOT NU APRES « go » N'EST PAS UN APPEL DE FONCTION ────────
     *
     * C'est ce qui distingue la correction d'un simple deplacement de
     * compteur, et il a fallu deux essais pour trouver le cas qui MORD.
     *
     * « function prev » ne prouve rien : l'analyseur refuse « end prev », ce
     * mot ne pouvant pas nommer un gestionnaire. « on prev » non plus : la
     * recherche se fait en mode fonction, et un gestionnaire de message n'y
     * repond pas.
     *
     * « function first » mord. Le nom est parfaitement legal, et sans la
     * correction « go first » APPELAIT cette fonction et suivait sa valeur de
     * retour. Mesure : on arrivait sur Deux — la carte que la fonction
     * nommait — au lieu de la premiere carte de la pile.
     *
     * Une pile qui s'ecrit une fonction « first », « last » ou « this »
     * detournait donc sa propre navigation, sans un mot. */
    hc_set_script(stack,
        "function first\n"
        "  return \"card \" & quote & \"Deux\" & quote\n"
        "end first\n");
    hc_set_current_card(card2);
    printf("=== « function first » ne doit PAS detourner « go first » ===\n");
    hc_set_script(btn, "on mouseUp\n  go first\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Une, PAS Deux)\n\n", hc_current_card()->name);
    hc_set_script(stack, "");

    /* ─── les autres designateurs nus ─── */
    hc_set_current_card(card2);
    printf("=== go first / go last / go this ===\n");
    hc_set_script(btn, "on mouseUp\n  go first\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Une)\n", hc_current_card()->name);
    hc_set_script(btn, "on mouseUp\n  go last\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Trois)\n", hc_current_card()->name);
    hc_set_script(btn, "on mouseUp\n  go this\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Trois)\n\n", hc_current_card()->name);

    /* ─── LA VARIABLE GARDE LA PRIORITE ───────────────────────────────
     *
     * « put ... into prev » puis « go prev » doit suivre la variable. C'est
     * ce que faisait l'evaluation — lit_var passe avant tout le reste — et
     * la correction ne devait pas le changer en passant. */
    hc_set_current_card(card1);
    printf("=== une VARIABLE nommee prev garde la priorite ===\n");
    hc_set_script(btn,
        "on mouseUp\n"
        "  put \"card \" & quote & \"Deux\" & quote into prev\n"
        "  go prev\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (attendu Deux, pas Trois)\n\n", hc_current_card()->name);

    /* ─── un mot qui ne designe rien se plaint ─── */
    printf("=== go zorglub ===\n");
    hc_set_script(btn, "on mouseUp\n  go zorglub\nend mouseUp\n");
    hc_send(btn, "mouseUp");
    printf("--> carte: %s (inchangee)\n\n", hc_current_card()->name);

    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
