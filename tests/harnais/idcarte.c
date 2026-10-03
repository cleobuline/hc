/* idcarte — « THE ID », COURT, ABRÉGÉ ET LONG.
 *
 * Les deux bancs de l'utilisatrice, joués DANS HYPERCARD (Basilisk II) le 3
 * octobre, rejoués ici ligne pour ligne (docs/mesures/long_id.txt). Ce
 * qu'HyperCard a rendu :
 *
 *     id / short / abbr / long id d'un bouton   3
 *     the id of this card                       card id 2865
 *     the short id of this card                 2865
 *     the long id of this card                  card id 2865 of stack "Saved HD:…:test5"
 *     the id, the long id of this bkgnd         2703
 *     the long id of fld 1                      1
 *     the abbreviated id of this card           card id 2865
 *     the id of card 2                          card id 3805
 *     go x  (x = the id of this card)           revient sur la carte 1
 *     go card id x                              ne bouge pas
 *
 * HC rendait le nombre nu pour la CARTE aussi, et « go x » y répondait « ne
 * sait pas faire ». Le bouton, le champ et le fond étaient déjà conformes :
 * ils sont ici pour qu'ils le RESTENT.
 *
 * LA PILE N'A PAS D'ID : « the long id of this stack », HyperCard le refuse
 * et arrête le script. HC rendait 1 ; il refuse maintenant, « propriété
 * inconnue », et s'arrête aussi. Les autres formes de l'id d'une pile ne sont
 * pas mesurées dans HyperCard : refusées avec la longue. */
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    Object *st = hc_new_stack("test5");
    Object *bg = hc_new_background(st, "Fond");
    Object *c1 = hc_new_card(st, bg, "Une");
    hc_new_card(st, bg, "Deux");
    hc_new_card(st, bg, "Trois");
    Object *b  = hc_new_button(c1, "B");
    hc_new_field(c1, "F");
    hc_set_current_card(c1);

    puts("== 1. le premier banc, tel que joué dans HyperCard ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  put \"id: \" & the id of me\n"
        "  put \"short id: \" & the short id of me\n"
        "  put \"abbr id: \" & the abbreviated id of me\n"
        "  put \"long id: \" & the long id of me\n"
        "  put \"card id: \" & the id of this card\n"
        "  put \"card short id: \" & the short id of this card\n"
        "  put \"card long id: \" & the long id of this card\n"
        "  put \"bg id: \" & the id of this bkgnd\n"
        "  put \"bg long id: \" & the long id of this bkgnd\n"
        "  put \"fld long id: \" & the long id of fld 1\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== 2. le second banc : l'abrégé, une autre carte, et go ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  put \"card abbr id: \" & the abbreviated id of this card\n"
        "  put \"card 2 id: \" & the id of card 2\n"
        "  put the id of this card into x\n"
        "  go card 2\n"
        "  go x\n"
        "  put \"go x -> card \" & the number of this card\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== 3. le long id se relit ; card id attend le court ==");
    hc_set_current_card(c1);
    hc_set_script(b,
        "on mouseUp\n"
        "  put the long id of this card into x\n"
        "  go card 3\n"
        "  go x\n"
        "  put \"go long id -> card \" & the number of this card\n"
        "  go card 2\n"
        "  go card id (the short id of card 1)\n"
        "  put \"go card id short -> card \" & the number of this card\n"
        "  put \"there is a card id short : \" & (there is a card id (the short id of card 3))\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== 4. le chemin du fichier, comme dans HyperCard ==");
    hc_set_stack_path(st, "/Volumes/Saved HD/More Stacks/test5");
    hc_set_current_card(c1);
    hc_set_script(b,
        "on mouseUp\n"
        "  put \"card long id: \" & the long id of this card\n"
        "  put \"long name   : \" & the long name of this card\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== 5. la pile n'a pas d'id : refus, et le script s'arrete ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  put \"stack long id: \" & the long id of this stack\n"
        "  put \"apres : ne doit pas s'ecrire\"\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");
    hc_set_script(b,
        "on mouseUp\n"
        "  put \"stack id: \" & the id of this stack\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    hc_free(st);
    return 0;
}
