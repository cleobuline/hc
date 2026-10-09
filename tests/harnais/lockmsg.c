#include "hc_core.h"
#include <stdio.h>
static void run(Object *btn, const char *l, const char *b){
  char s[1024]; snprintf(s,sizeof s,"on mouseUp\n  %s\nend mouseUp\n",b);
  hc_set_script(btn,s); printf(">>> %s\n", l); hc_send(btn,"mouseUp");
}
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *c1    = hc_new_card(stack, bg, "Une");
    Object *c2    = hc_new_card(stack, bg, "Deux");
    Object *btn   = hc_new_button(c1, "B");
    hc_set_current_card(c1);
    /* la carte 2 crie quand on y entre */
    hc_set_script(c2, "on openCard\n  put \"!! openCard de la carte Deux\"\nend openCard\n");
    run(btn, "navigation NORMALE (openCard doit crier)",
        "go to card 2\n  go to card 1");
    run(btn, "navigation avec LOCK MESSAGES (silence attendu)",
        "lock messages\n  go to card 2\n  go to card 1\n  unlock messages");
    run(btn, "apres unlock, openCard revient",
        "go to card 2\n  go to card 1");
    run(btn, "lock sans unlock : deverrouillage automatique en sortie",
        "lock messages\n  go to card 2");
    run(btn, "  -> le gestionnaire suivant doit crier",
        "go to card 1\n  go to card 2\n  go to card 1");

    /* LA PROPRIÉTÉ, SYNONYME DE LA COMMANDE. « set lockMessages to true »
     * répondait « propriété inconnue » : seule la commande posait le verrou.
     * La forme est celle de la démonstration lockMessages de « HyperTalk
     * Reference » (Apple), en minuscules comme elle l'écrit. */
    run(btn, "SET LOCKMESSAGES TO TRUE (silence attendu)",
        "set lockmessages to true\n  go to card 2\n  go to card 1\n"
        "  put \"lu : \" & the lockMessages\n  set lockmessages to false\n"
        "  put \"lu : \" & the lockMessages");
    run(btn, "set the lockMessages, avec « the »",
        "set the lockMessages to true\n  go to card 2\n  go to card 1");
    run(btn, "  -> sans « false », il retombe en sortie : le suivant doit crier",
        "put \"lu : \" & the lockMessages\n  go to card 2\n  go to card 1");
    hc_free(stack);
    return 0;
}
