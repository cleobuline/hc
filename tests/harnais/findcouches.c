/* « find » VISITE LE FOND D'ABORD, PUIS LA CARTE.
 *
 * Mesuré DANS HYPERCARD (Basilisk II) le 30 septembre, pile fraîchement
 * ouverte — le curseur de find survit à tout le reste. Le même mot dans le
 * champ de carte n° 1 et dans le champ de fond n° 1 de la même carte ; trois
 * « find » de suite :
 *
 *     F1  bkgnd field 1        HC rendait card field 1
 *     F2  card field 1         HC rendait bkgnd field 1
 *     F3  bkgnd field 1        HC rendait card field 1
 *
 * C'était la dernière divergence ouverte de find (docs/mesures/find.txt), et
 * elle expliquait les deux seules lignes de la pile de torture qui
 * différaient d'HyperCard. La section 2 fixe la REPRISE : après une
 * trouvaille dans un champ de carte, le fond de la même carte a déjà été vu —
 * on passe à la carte suivante. */
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

    Object *st = hc_new_stack("Find");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    Object *u  = hc_new_card(st, bg, "Une");
    Object *d  = hc_new_card(st, bg, "Deux");
    (void)d;
    hc_set_current_card(u);

    /* Le dispositif du banc des couches, dans cet ordre. */
    hc_new_field(u, "A");  hc_new_field(u, "R");
    hc_new_field(u, "Commun");  hc_new_field(u, "SeulCarte");
    hc_new_field(bg, "T");  hc_new_field(bg, "Commun");  hc_new_field(bg, "SeulFond");
    Object *b = hc_new_button(u, "Ordre");

    puts("== 1. le banc mesuré : fond, carte, fond ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  put \"cible\" into card field \"A\"\n"
        "  put \"cible\" into bg field \"T\"\n"
        "  find \"cible\"\n"
        "  put \"F1 \" & the foundField\n"
        "  find \"cible\"\n"
        "  put \"F2 \" & the foundField\n"
        "  find \"cible\"\n"
        "  put \"F3 \" & the foundField\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== 2. la reprise : après la carte, le fond de la même carte est déjà vu ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  put empty into card field \"A\"\n"
        "  put empty into bg field \"T\"\n"
        "  go to card \"Deux\"\n"
        "  put empty into bg field \"T\"\n"
        "  go to card \"Une\"\n"
        "  put \"zorglub\" into card field \"A\"\n"
        "  put \"alpha\" into bg field \"T\"\n"
        "  go to card \"Deux\"\n"
        "  put \"alpha\" into bg field \"T\"\n"
        "  go to card \"Une\"\n"
        "  find \"zorglub\"\n"
        "  put \"zorglub dans \" & the foundField & \" de \" & the short name of this card\n"
        "  find \"alpha\"\n"
        "  put \"alpha 1 : \" & the short name of this card\n"
        "  find \"alpha\"\n"
        "  put \"alpha 2 : \" & the short name of this card\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
