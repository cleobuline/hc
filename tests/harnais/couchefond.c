/* UN CHAMP SANS COUCHE EST UN CHAMP DE FOND ; UN BOUTON, UN BOUTON DE CARTE.
 *
 * Le banc de docs/mesures/couche_implicite.txt, joué DANS HYPERCARD
 * (Basilisk II) le 30 septembre, et ici sur le même dispositif. Chaque ligne
 * porte la réponse d'HyperCard en commentaire ; une seule diffère, et c'est
 * voulu :
 *
 *     A1  field 1                     auFond
 *     A5  the name of field 1         bkgnd field "T"
 *     A6  the number of fields        3          (le fond seul ; HC disait 7)
 *     A7  the number of card fields   4
 *     A8  the number of bkgnd fields  3
 *     B1  the name of button 1        la carte   (BC ici ; « Banc » dans la
 *                                                 pile mesurée, créé avant BC)
 *     B2  the number of buttons       2          (la carte seule ; HC disait 3)
 *     B3  the number of bkgnd buttons 1
 *     A3b field "Commun" (2 couches)  communFond
 *     A2  field "T"                   auFond
 *     A4  put "zzz" into field 1      écrit au FOND
 *     C2  hide field 1                cache le FOND
 *     A3  field "SeulCarte"           ERREUR chez HyperCard ; HC se replie sur
 *                                     la carte — amélioration décidée, voir
 *                                     couche_implicite dans hc_core.c. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG || k == HC_ERR) printf("  %s\n", t);
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Couches");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    Object *u  = hc_new_card(st, bg, "Une");
    hc_set_current_card(u);

    /* Dans cet ordre : le rang compte. */
    hc_new_field(u, "A");  hc_new_field(u, "R");
    hc_new_field(u, "Commun");  hc_new_field(u, "SeulCarte");
    hc_new_field(bg, "T");  hc_new_field(bg, "Commun");  hc_new_field(bg, "SeulFond");
    hc_new_button(u, "BC");
    Object *banc = hc_new_button(u, "Banc");
    hc_new_button(bg, "BF");

    hc_set_script(banc,
        "on mouseUp\n"
        "  put \"surCarte\" into card field \"A\"\n"
        "  put \"auFond\" into bg field \"T\"\n"
        "  put \"communCarte\" into card field \"Commun\"\n"
        "  put \"communFond\" into bg field \"Commun\"\n"
        "  put \"seulFond\" into bg field \"SeulFond\"\n"
        "  put \"seulCarte\" into card field \"SeulCarte\"\n"
        "  put \"A1 \" & field 1\n"
        "  put \"A5 \" & the name of field 1\n"
        "  put \"A6 \" & the number of fields\n"
        "  put \"A7 \" & the number of card fields\n"
        "  put \"A8 \" & the number of bkgnd fields\n"
        "  put \"B1 \" & the name of button 1\n"
        "  put \"B2 \" & the number of buttons\n"
        "  put \"B3 \" & the number of bkgnd buttons\n"
        "  put \"A3b \" & field \"Commun\"\n"
        "  put \"A2 \" & field \"T\"\n"
        "  put \"zzz\" into field 1\n"
        "  put \"A4 carte [\" & card field \"A\" & \"] fond [\" & bg field \"T\" & \"]\"\n"
        "  hide field 1\n"
        "  put \"C2 carte \" & the visible of card field \"A\" & \" fond \" & the visible of bg field \"T\"\n"
        "  show card field \"A\"\n"
        "  show bg field \"T\"\n"
        "  put \"A3 \" & field \"SeulCarte\"\n"
        "end mouseUp\n");
    hc_send(banc, "mouseUp");

    /* Une couche ÉCRITE ne se replie jamais : ni « bg field » vers la carte,
     * ni « card field » vers le fond. */
    puts("== une couche écrite ne se replie pas ==");
    hc_set_script(banc,
        "on mouseUp\n"
        "  put \"bg SeulCarte : \" & (there is a bg field \"SeulCarte\")\n"
        "  put \"card SeulFond : \" & (there is a card field \"SeulFond\")\n"
        "  put \"sans couche, SeulFond : \" & field \"SeulFond\"\n"
        "end mouseUp\n");
    hc_send(banc, "mouseUp");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
