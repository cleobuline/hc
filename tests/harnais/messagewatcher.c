/* Le Message Watcher (la 1.1) : le rappel message_vu.
 *
 * L'hôte d'essai écrit chaque appel : le départ (?) et le retour (pris ou
 * non), avec l'identifiant, la profondeur, le message et la cible. Il tient :
 *   1. un message de l'hôte, pris, part à la profondeur 0 ;
 *   2. ceux qu'envoie son gestionnaire — une commande, une fonction, un
 *      « send » — partent DEDANS, à la profondeur 1, avant que le premier
 *      revienne : l'hôte peut les ranger à leur place ;
 *   3. un message que personne ne prend revient « non pris » ;
 *   4. un « pass » jusqu'au bout n'est pas pris ;
 *   5. sans rappel, rien ne change. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("      %s\n", t);
    else if (k == HC_ERR) printf("      [ERR] %s\n", t);
}

static void vu(int id, const char *message, Object *cible, int prof, int pris)
{
    char qui[64];
    if (pris < 0) hc_describe(cible, qui, sizeof qui);
    printf("   %*s%s #%d %s%s%s\n", prof * 2, "",
           pris < 0 ? "départ" : (pris ? "retour, pris" : "retour, NON pris"),
           id, message, pris < 0 ? " -> " : "", pris < 0 ? qui : "");
}

int main(void)
{
    static HcHost h;
    h.line = ligne;
    h.message_vu = vu;
    hc_set_host(&h);

    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "C");
    Object *b  = hc_new_button(c, "B");
    hc_set_current_card(c);
    hc_set_script(b,
        "on mouseUp\n"
        "  ajoute\n"
        "  put double(3) into x\n"
        "  send \"bonjour\" to this card\n"
        "end mouseUp\n"
        "on ajoute\n"
        "  put 1 into y\n"
        "end ajoute\n"
        "function double n\n"
        "  return n * 2\n"
        "end double\n"
        "on mouseDown\n"
        "  pass mouseDown\n"
        "end mouseDown\n");
    hc_set_script(c, "on bonjour\n  put \"bonjour\"\nend bonjour\n");

    puts("== 1-2. mouseUp, et ce qu'il envoie, à leur place ==");
    hc_send(b, "mouseUp");

    puts("\n== 3. un message que personne ne prend ==");
    hc_send(b, "idle");

    puts("\n== 4. un pass jusqu'au bout ==");
    hc_send(b, "mouseDown");

    puts("\n== 5. sans rappel, rien ==");
    h.message_vu = NULL;
    hc_send(b, "mouseUp");
    puts("   (aucune ligne au-dessus)");

    hc_free(st);
    return 0;
}
