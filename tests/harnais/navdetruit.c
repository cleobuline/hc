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
#include <string.h>

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

    /* DANS UN GESTIONNAIRE, la carte supprimée n'est d'abord que DÉTACHÉE :
     * sa libération attend le retour au repos. La 0.8.6 n'y voyait que du
     * feu, posait la carte détachée comme courante, puis la perdait au
     * retour — plus aucun script n'atteignait la pile (revue du 24 octobre).
     * Après le clic, la pile doit encore répondre. */
    puts("\n== go dans un mouseUp : l'arrivée supprimée par closeCard ==");
    monte();
    {
        Object *b = hc_new_button(hc_current_card(), "aller");
        hc_set_script(b, "on mouseUp\n  arme\n  go to card \"Deux\"\n"
                         "  put \"résultat : \" & the result\nend mouseUp");
    }
    fais("send mouseUp to button \"aller\"");
    fais("put the short name of this card && the number of cards");
    fais("go next card");
    fais("put the short name of this card");
    defait();

    puts("\n== find dans un mouseUp : la carte trouvée supprimée par closeCard ==");
    monte();
    {
        Object *b = hc_new_button(hc_current_card(), "chercher");
        hc_set_script(b, "on mouseUp\n  arme\n  find \"grenouille\"\n"
                         "  put \"résultat : \" & the result\nend mouseUp");
    }
    fais("send mouseUp to button \"chercher\"");
    fais("put the short name of this card && the number of cards");
    fais("go next card");
    fais("put the short name of this card");
    defait();

    /* LES MESSAGES D'OUVERTURE aussi font courir des gestionnaires : un
     * openBackground qui supprime l'arrivée, un openCard qui se supprime. */
    puts("\n== go : l'arrivée supprimée par openBackground ==");
    st = hc_new_stack("Nav3");
    hc_register_stack(st);
    {
        Object *fa = hc_new_background(st, "A");
        Object *fb = hc_new_background(st, "B");
        Object *depart = hc_new_card(st, fa, "Départ");
        hc_new_card(st, fb, "Cible");
        hc_new_card(st, fb, "Voisine");
        hc_set_script(fb, "on openBackground\n  delete card \"Cible\"\nend openBackground");
        hc_set_current_card(depart);
    }
    fais("go to card \"Cible\"");
    fais("put the result");
    fais("put the short name of this card && the number of cards");
    defait();

    puts("\n== find et pop : openCard supprime la carte d'arrivée ==");
    monte();
    hc_set_script(hc_current_card(), "");     /* plus de piège au départ */
    fais("go to card \"Deux\"");
    fais("push this card");
    fais("go to card \"Un\"");
    {
        Object *deux = NULL;
        for (int k = 0; k < st->nparts; k++)
            if (st->parts[k]->type == OBJ_CARD && st->parts[k]->name &&
                !strcmp(st->parts[k]->name, "Deux"))
                deux = st->parts[k];
        if (deux)
            hc_set_script(deux, "on openCard\n  global piege\n  if piege is true then\n"
                                "    put false into piege\n    delete this card\n  end if\n"
                                "end openCard");
    }
    fais("arme");
    fais("pop card");
    fais("put the result");
    fais("put the short name of this card && the number of cards");
    defait();

    monte();
    hc_set_script(hc_current_card(), "");
    {
        Object *deux = NULL;
        for (int k = 0; k < st->nparts; k++)
            if (st->parts[k]->type == OBJ_CARD && st->parts[k]->name &&
                !strcmp(st->parts[k]->name, "Deux"))
                deux = st->parts[k];
        if (deux)
            hc_set_script(deux, "on openCard\n  global piege\n  if piege is true then\n"
                                "    put false into piege\n    delete this card\n  end if\n"
                                "end openCard");
    }
    fais("arme");
    fais("find \"grenouille\"");
    fais("put the result");
    fais("put the short name of this card && the number of cards");
    defait();

    /* UN CHAMP DE FOND SURVIT À SA CARTE : la recherche qui échoue en route
     * doit oublier son résultat, foundText compris. */
    puts("\n== find : un champ de fond, la carte supprimée par closeCard ==");
    st = hc_new_stack("Nav4");
    hc_register_stack(st);
    {
        Object *f = hc_new_background(st, "F");
        Object *un = hc_new_card(st, f, "Un");
        Object *deux = hc_new_card(st, f, "Deux");
        hc_new_field(f, "texte");
        hc_set_script(st, "on arme\n  global piege\n  put true into piege\nend arme");
        hc_set_script(un, "on closeCard\n  global piege\n  if piege is true then\n"
                          "    put false into piege\n    delete card \"Deux\"\n  end if\n"
                          "end closeCard");
        hc_set_current_card(deux);
        hc_do("put \"une grenouille verte\" into bkgnd field \"texte\"");
        hc_set_current_card(un);
    }
    fais("arme");
    fais("find \"grenouille\"");
    fais("put the result");
    fais("put \"[\" & the foundText & \"] [\" & the foundChunk & \"]\"");
    defait();
    return 0;
}
