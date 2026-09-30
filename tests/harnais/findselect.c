/* UN « select » DANS UN CHAMP EFFACE LA TROUVAILLE ; « select empty » NON.
 *
 * Mesuré DANS HYPERCARD (Basilisk II) le 30 septembre, docs/mesures/find.txt.
 * Les réponses d'HyperCard, ligne par ligne :
 *
 *     S1  avant le select            char 7 to 10 of bkgnd field 1
 *     S2  foundChunk après select    []
 *     S3  foundText                  []
 *     S4  foundField                 []
 *     S5  foundLine                  []
 *     S6  après un AUTRE select      []
 *     S7  après « select empty »     char 18 to 22 of bkgnd field 1
 *     S8  sans select (le témoin)    char 1 to 5 of bkgnd field 1
 *
 * HC gardait tout jusqu'à la recherche suivante — son commentaire l'affirmait
 * sans mesure. Voir g_found_lisible, hc_core.c.
 *
 * S3 à S5 sont lus, comme dans le banc, APRÈS l'écriture de S2 dans R : ce
 * banc ne sépare pas « vidé par le select » de « vidé par l'écriture ». */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG || k == HC_ERR) printf("%s\n", t);
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("P");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *u  = hc_new_card(st, bg, "U");
    hc_set_current_card(u);
    hc_new_field(u, "A");  hc_new_field(u, "R");  hc_new_field(bg, "T");
    Object *b = hc_new_button(u, "Select");

    hc_set_script(b,
        "on mouseUp\n"
        "  put \"alpha beta gamma delta\" into bg field \"T\"\n"
        "  put \"un deux trois\" into card field \"A\"\n"
        "  put empty into card field \"R\"\n"
        "  find \"bet\"\n"
        "  put \"S1 avant   : \" & the foundChunk & return after card field \"R\"\n"
        "  select the foundChunk\n"
        "  put \"S2 chunk   : [\" & the foundChunk & \"]\" & return after card field \"R\"\n"
        "  put \"S3 text    : [\" & the foundText & \"]\" & return after card field \"R\"\n"
        "  put \"S4 field   : [\" & the foundField & \"]\" & return after card field \"R\"\n"
        "  put \"S5 line    : [\" & the foundLine & \"]\" & return after card field \"R\"\n"
        "  find \"gam\"\n"
        "  select word 2 of card field \"A\"\n"
        "  put \"S6 autre select : [\" & the foundChunk & \"]\" & return after card field \"R\"\n"
        "  find \"del\"\n"
        "  select empty\n"
        "  put \"S7 select empty : [\" & the foundChunk & \"]\" & return after card field \"R\"\n"
        "  find \"alp\"\n"
        "  put \"S8 sans select  : [\" & the foundChunk & \"]\" & return after card field \"R\"\n"
        "  put card field \"R\"\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
