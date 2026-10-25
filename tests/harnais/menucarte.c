/* menucarte.c — New Card et New Background : la place, les messages, la cible.
 *
 * MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II) par l'utilisatrice le 24
 * octobre, par le même journal qu'ici, tenu dans le script de la pile
 * (docs/mesures/audits_octobre.txt) :
 *
 *   A (New Card, depuis la carte 4) :
 *       closeCard - 4 - card id 10709
 *       newCard - 5 - card id 12129
 *       openCard - 5 - card id 12129
 *   C (New Background, depuis la carte 5) :
 *       closeCard - 5 - card id 12129
 *       closeBackground - 5 - card id 12129
 *       newBackground - 6 - card id 12698
 *       newCard - 6 - card id 12698
 *       openBackground - 6 - card id 12698
 *       openCard - 6 - card id 12698
 *
 * La carte neuve va juste après la courante, et les messages de fond
 * partent à une CARTE : the target de newBackground est la carte neuve.
 * HC posait la carte en fin de pile, n'envoyait rien pour New Card, et
 * newBackground au fond. Les deux derniers cas — un gestionnaire qui
 * supprime une des cartes — ne sont PAS mesurés dans HyperCard : ils
 * tiennent seulement qu'HC ne lit pas de carte morte (sous ASan). */
#include "hc_core.h"
#include <stdio.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG || k == HC_ERR) printf("   %s\n", t); }

static void fais(const char *l)
{
    printf("%s\n", l);
    hc_do(l);
}

static const char *JOURNAL =
    "on newCard\n  journalise \"newCard\"\n  pass newCard\nend newCard\n"
    "on openCard\n  journalise \"openCard\"\n  pass openCard\nend openCard\n"
    "on closeCard\n  journalise \"closeCard\"\n  pass closeCard\nend closeCard\n"
    "on newBackground\n  journalise \"newBackground\"\n  pass newBackground\nend newBackground\n"
    "on openBackground\n  journalise \"openBackground\"\n  pass openBackground\nend openBackground\n"
    "on closeBackground\n  journalise \"closeBackground\"\n  pass closeBackground\nend closeBackground\n"
    "on journalise quoi\n  global journal\n"
    "  put quoi && \"-\" && the number of this card && \"-\" && the name of the target"
    " & return after journal\nend journalise\n";

static Object *st, *bg;

static void monte(void)
{
    st = hc_new_stack("Banc");
    hc_register_stack(st);
    bg = hc_new_background(st, "");
    for (int i = 0; i < 4; i++) hc_new_card(st, bg, "");
    hc_set_script(st, JOURNAL);
    hc_set_current_card(hc_new_card(st, bg, ""));   /* la 5, la dernière */
    fais("go card 2");
}

static void defait(void)
{
    hc_unregister_stack(st);
    hc_free(st);
}

int main(void)
{
    static HcHost h;
    h.line = ligne;
    hc_set_host(&h);

    puts("== A. New Card depuis la carte 2 ==");
    monte();
    fais("put empty into journal");
    hc_nouvelle_carte(0);
    fais("put journal");
    fais("put the number of this card && the number of cards");
    defait();

    puts("\n== C. New Background depuis la carte 2 ==");
    monte();
    fais("put empty into journal");
    hc_nouvelle_carte(1);
    fais("put journal");
    fais("put the number of this card && the number of cards && the number of bkgnds");

    puts("\n== newBackground passe par la carte neuve ==");
    /* Il partait au fond : un « on newBackground » de carte n'existait pas
     * pour HC. Ici la carte neuve n'a pas encore de script ; c'est la pile
     * qui répond, et the target dit que le message est passé par la carte. */
    fais("put empty into journal");
    hc_nouvelle_carte(1);
    fais("put line 3 of journal");
    defait();

    puts("\n== G. go d'un fond à l'autre, et retour ==");
    /* MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II) le 25 octobre, même journal,
     * un fond neuf posé après la carte 1 :
     *   G  (go card 1, depuis la 2)       G2 (go next card, depuis la 1)
     *     closeCard - 2 - card id 13150     closeCard - 1 - card id 3014
     *     closeBackground - 2 - card id 13150
     *                                       closeBackground - 1 - card id 3014
     *     openBackground - 1 - card id 3014 openBackground - 2 - card id 13150
     *     openCard - 1 - card id 3014       openCard - 2 - card id 13150
     * Les messages de fond partent à une CARTE. HC les envoyait au fond. */
    monte();
    fais("go card 1");
    hc_nouvelle_carte(1);
    fais("put empty into journal");
    fais("go card 1");
    fais("put journal");
    fais("put empty into journal");
    fais("go next card");
    fais("put journal");
    puts("   -- un « on openBackground » dans le script d'une carte : il est appelé");
    fais("set the script of card 1 to \"on openBackground\" & return & "
         "\"  global journal\" & return & "
         "\"  put \" & quote & \"vu par la carte\" & quote & \" & return after journal\" & return & "
         "\"  pass openBackground\" & return & \"end openBackground\"");
    fais("put empty into journal");
    fais("go card 1");
    fais("put journal");
    defait();

    puts("\n== newCard supprime la carte neuve (non mesuré dans HyperCard) ==");
    monte();
    hc_set_script(st, "on newCard\n  delete this card\nend newCard\n"
                      "on openCard\n  put \"openCard\" && the number of this card\nend openCard");
    {
        Object *c = hc_nouvelle_carte(0);
        printf("   carte rendue : %s\n", c ? "oui" : "aucune");
    }
    fais("put the number of this card && the number of cards");
    defait();

    puts("\n== closeCard supprime l'ancienne carte (non mesuré dans HyperCard) ==");
    monte();
    hc_set_script(st, "on closeCard\n  global fait\n  if fait is not true then\n"
                      "    put true into fait\n    delete this card\n  end if\nend closeCard");
    {
        Object *c = hc_nouvelle_carte(1);
        printf("   carte rendue : %s\n", c ? "oui" : "aucune");
    }
    fais("put the number of this card && the number of cards");
    defait();
    return 0;
}
