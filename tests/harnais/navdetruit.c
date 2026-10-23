/* navdetruit.c — un closeCard qui supprime la carte où l'on allait.
 *
 * go, find et pop envoient closeCard (puis closeBackground) à la carte de
 * départ AVANT de poser la carte d'arrivée. Si ce gestionnaire supprime
 * l'arrivée, elle est libérée au retour de son envoi, et la navigation la
 * lisait encore — dst->bg, son nom, openCard (second audit de Jules,
 * octobre). Sous --asan, une lecture morte se voit ici.
 *
 * Ce que fait HyperCard dans ce cas n'est PAS MESURÉ : HC reste sur sa carte,
 * et the result le dit. */
#include "hc_core.h"
#include <stdio.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG || k == HC_ERR) printf("   %s\n", t); }

static Object *st, *bg;

static void monte(void)
{
    st = hc_new_stack("Nav");
    hc_register_stack(st);
    bg = hc_new_background(st, "F");
    Object *un = hc_new_card(st, bg, "Un");
    Object *deux = hc_new_card(st, bg, "Deux");
    hc_new_card(st, bg, "Trois");
    /* Le piège ne s'arme que sur demande : les navigations de mise en place
     * passent sans rien supprimer. */
    hc_set_script(un, "on closeCard\n  global piege\n  if piege is true then\n"
                      "    put false into piege\n    delete card \"Deux\"\n  end if\n"
                      "end closeCard");
    hc_set_script(st, "on arme\n  global piege\n  put true into piege\nend arme");
    hc_set_script(deux, "on openCard\n  put \"openCard de Deux\"\nend openCard");
    Object *f = hc_new_field(deux, "texte");
    (void)f;
    hc_set_current_card(deux);
    hc_do("put \"grenouille\" into card field \"texte\"");
    hc_set_current_card(un);
}

static void defait(void)
{
    hc_unregister_stack(st);
    hc_free(st);
}

static void fais(const char *l)
{
    printf("%s\n", l);
    hc_do(l);
}

int main(void)
{
    static HcHost h;
    h.line = ligne;
    hc_set_host(&h);

    puts("== go : l'arrivée supprimée par closeCard ==");
    monte();
    fais("arme");
    fais("go to card \"Deux\"");
    fais("put the result");
    fais("put the short name of this card && the number of cards");
    defait();

    puts("\n== find : la carte trouvée supprimée par closeCard ==");
    monte();
    fais("arme");
    fais("find \"grenouille\"");
    fais("put the result");
    fais("put the short name of this card && the number of cards");
    defait();

    puts("\n== pop : la carte dépilée supprimée par closeCard ==");
    monte();
    fais("go to card \"Deux\"");
    fais("push this card");
    fais("go to card \"Un\"");
    fais("arme");
    fais("pop card");
    fais("put the result");
    fais("put the short name of this card && the number of cards");
    defait();

    /* Le jumeau du jumeau : closeCard supprime SA propre carte, seule de
     * son fond — le fond part avec elle, et closeBackground ne doit pas
     * partir vers lui. */
    puts("\n== go : closeCard supprime sa propre carte et son fond ==");
    st = hc_new_stack("Nav2");
    hc_register_stack(st);
    Object *f1 = hc_new_background(st, "Seul");
    Object *f2 = hc_new_background(st, "Autre");
    Object *seule = hc_new_card(st, f1, "Seule");
    hc_new_card(st, f2, "Ailleurs");
    hc_new_card(st, f2, "Encore");
    hc_set_script(seule, "on closeCard\n  delete this card\nend closeCard");
    hc_set_current_card(seule);
    fais("go to card \"Ailleurs\"");
    fais("put the result");
    fais("put the short name of this card && the number of cards && the number of backgrounds");
    defait();
    return 0;
}
