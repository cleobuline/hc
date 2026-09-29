/* LA LIGNE 5 A ÉCHOUÉ DEPUIS L'ÉCRITURE DE CE HARNAIS, ET PERSONNE NE L'A VU.
 *
 * Elle s'écrivait « \"Une\" » : un échappement du C, recopié tel quel dans le
 * script. HyperTalk n'a pas d'échappement — la chaîne s'arrêtait donc sur
 * « card \ », et « Une\" … » restait sur la ligne. Le témoin enregistrait
 * « card \ » puis « texte inattendu », et les CINQ lignes suivantes — next
 * card, me, le champ, le comptage, le bilan — ne s'exécutaient jamais : ce
 * harnais mesurait deux lignes sur sept.
 *
 * Trouvé en corrigeant l'exécution partielle d'une ligne fautive (hct_exec.c) :
 * le début de la ligne n'était plus affiché, et le trou est devenu visible.
 * Les guillemets passent maintenant par « quote », comme dans une vraie pile. */
#include "hc_core.h"
#include <stdio.h>
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *c1    = hc_new_card(stack, bg, "Une");
    Object *c2    = hc_new_card(stack, bg, "Deux");
    Object *c3    = hc_new_card(stack, bg, "Trois");
    Object *btn   = hc_new_button(c2, "B");
    hc_new_field(c2, "F1");
    hc_new_field(c2, "F2");
    hc_set_current_card(c2);
    hc_set_script(btn,
      "on mouseUp\n"
      "  debug raz\n"
      "  put \"this card      = \" & the number of this card\n"
      "  put \"card 3         = \" & the number of card 3\n"
      "  put \"card \" & quote & \"Une\" & quote & \"   = \" & the number of card \"Une\"\n"
      "  put \"next card      = \" & the number of next card\n"
      "  put \"me             = \" & the number of me\n"
      "  put \"cd field F2    = \" & the number of card field \"F2\"\n"
      "  put \"-- comptage : cards = \" & the number of cards\n"
      "  debug bilan\n"
      "end mouseUp\n");
    hc_send(btn, "mouseUp");
    hc_free(stack);
    return 0;
}
