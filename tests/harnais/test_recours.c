/* CE HARNAIS PROUVE DESORMAIS L'INVERSE DE SON NOM, ET C'EST LE BUT.
 *
 * Il a ete ecrit pour verifier que le RECOURS vers l'ancien moteur servait
 * bien quatre tournures que la v3 ne savait pas traiter. Sa reference
 * enregistrait donc des bilans pleins :
 *
 *     recours objet: bg btn "GoBtn2"           3
 *     recours of hilite                        1
 *     v1 terme recours expr                    6
 *
 * Les trois premieres tournures sont maintenant servies par la v3 elle-meme :
 * bilan vide, « (aucun) » et « (rien) ». Le harnais n'en garde pas moins tout
 * son interet — il tient l'invariant « ces formes-la n'ont plus besoin de
 * l'ancien moteur », et un bilan qui redeviendrait plein signalerait un
 * retour en arriere.
 *
 * LA QUATRIEME A CHANGE AUTREMENT. « the hilite of bg btn "GoBtn2" », ou ce
 * bouton n'existe pas, faisait ceci :
 *
 *     !! objet introuvable dans « bg btn "GoBtn2" »
 *     -> message "hilite" a button "GoBtn"
 *          (pas de gestionnaire dans button "GoBtn")
 *          (pas de gestionnaire dans card "Une")
 *          ... toute la hierarchie ...
 *     [message box] hilite of bg btn "GoBtn2"
 *
 * Lire une propriete sur un objet absent DIFFUSAIT le nom de la propriete
 * comme un message dans toute la hierarchie, puis rendait l'echo. Desormais :
 * une erreur nommee, et le gestionnaire s'arrete. */
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
    Object *bb = hc_new_button(bg, "\xe2\x80\x9c" "Depth" "\xe2\x80\x9d"); /* curly quotes UTF-8 */

    run(btn, "hilite of bg btn avec guillemets typographiques",
        "put hilite of bg btn \"\xe2\x80\x9c" "Depth" "\xe2\x80\x9d\" into dep\n  put dep");

    run(btn, "the value of fld", "put the value of fld \"IncremU\" into incU\n  put incU");

    run(btn, "fld tout court comme expression", "put fld \"IncremU\" into incU\n  put incU");

    run(btn, "bg btn ordinaire", "put the hilite of bg btn \"GoBtn2\" into x\n  put x");

    (void)bb;
    hc_free(stack);
    return 0;
}
