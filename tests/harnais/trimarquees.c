/* « SORT MARKED CARDS » TRIAIT TOUTE LA PILE.
 *
 * SIGNALE PAR UN AUDIT EXTERIEUR, et le code le disait lui-meme :
 *
 *     if (ci_word(a, "marked")) a = skip_spaces(a + 6);   // accepte, ignore
 *
 * Le mot passait, le tri portait sur TOUTES les cartes. Mesure, avec quatre
 * cartes D C B A dont les deux du milieu marquees :
 *
 *     sort marked cards by the short name of this card   -> A B C D
 *
 * c'est-a-dire la pile entiere reordonnee. Un script qui marque un
 * sous-ensemble pour le ranger — le cas d'usage meme de « mark » — deplacait
 * tout le reste avec.
 *
 * LA REGLE D'HYPERCARD EST UN TRI EN PLACE : les cartes marquees se
 * redistribuent entre les SEULES POSITIONS QU'ELLES OCCUPAIENT DEJA, et les
 * autres ne bougent pas d'un cran. D C B A avec C et B marquees devient donc
 * D B C A — les places 2 et 3 echangent leur contenu, les places 1 et 4 sont
 * intouchees.
 *
 * C'est ce que ce harnais verifie, et c'est plus exigeant qu'il n'y parait :
 * un tri qui ne regarderait que les cartes marquees mais les reecrirait a
 * partir du debut donnerait B C D A, ce qui passe l'oeil et casse la pile.
 * Les positions des non-marquees sont donc controlees une a une.
 *
 * ATTENTION AU JEU D'ESSAI. Il est choisi pour que le resultat de
 * « sort marked » differe A LA FOIS de l'ordre de depart ET du tri complet :
 *
 *     depart          D C B A
 *     sort marked     D B C A      <- ni l'un ni l'autre
 *     sort tout       A B C D
 *
 * Avec des cartes deja dans l'ordre entre elles, le test aurait passe avant
 * comme apres la correction sans rien prouver.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

/* Le script qui recompose l'ordre des cartes en une chaine. */
static const char *LISTE =
    "  put empty into l\n"
    "  repeat with i = 1 to the number of cards\n"
    "    put the short name of card i after l\n"
    "  end repeat\n";

static void execute(const char *titre, const char *corps)
{
    char s[4096];
    printf("   %s\n", titre);
    snprintf(s, sizeof s, "on essaie\n%s\nend essaie\n", corps);
    hc_set_script(b, s);
    hc_send(b, "essaie");
}

/* Le titre est porte par le SCRIPT, pas par l'appelant : sinon il paraissait
 * deux fois, une fois en en-tete et une fois dans la ligne rendue. */
static void ordre(const char *titre, const char *avant)
{
    char s[4096];
    snprintf(s, sizeof s, "on essaie\n%s%s  put \"%s\" & l\nend essaie\n",
             avant, LISTE, titre);
    hc_set_script(b, s);
    hc_send(b, "essaie");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c1 = hc_new_card(st, bg, "D");
    hc_new_card(st, bg, "C");
    hc_new_card(st, bg, "B");
    hc_new_card(st, bg, "A");
    hc_set_current_card(c1);
    hc_register_stack(st);
    b = hc_new_button(c1, "B");

    puts("== 1. le tri d'un sous-ensemble marque ==");
    ordre("ordre de depart : ", "");
    execute("on marque les cartes 2 et 3, soit C et B",
            "  set the marked of card 2 to true\n"
            "  set the marked of card 3 to true\n"
            "  put the number of marked cards & \" marquees\"");
    ordre("apres sort MARKED : ",
          "  sort marked cards by the short name of this card\n");

    puts("\n== 2. les cartes NON marquees n'ont pas bouge d'un cran ==");
    /* Le controle qui distingue un vrai tri en place d'une reecriture depuis
     * le debut : celle-ci donnerait B C D A, dont la premiere lettre suffit
     * a trahir. */
    execute("la place 1 et la place 4",
            "  put \"place 1 : \" & the short name of card 1\n"
            "  put \"place 4 : \" & the short name of card 4");
    execute("et elles ne sont toujours pas marquees",
            "  put \"card 1 marquee : \" & the marked of card 1\n"
            "  put \"card 4 marquee : \" & the marked of card 4");

    puts("\n== 3. le tri COMPLET, lui, n'a pas change ==");
    ordre("apres sort TOUT : ",
          "  sort cards by the short name of this card\n");

    puts("\n== 4. les cas ou « marked » ne peut rien trier ==");
    execute("on demarque tout",
            "  repeat with i = 1 to the number of cards\n"
            "    set the marked of card i to false\n"
            "  end repeat\n"
            "  put the number of marked cards & \" marquees\"");
    ordre("sort marked sans aucune marquee : ",
          "  sort marked cards by the short name of this card\n");
    execute("une seule marquee",
            "  set the marked of card 3 to true\n"
            "  put the number of marked cards & \" marquee\"");
    ordre("sort marked avec une seule : ",
          "  sort marked cards by the short name of this card\n");

    puts("\n== 5. « sort marked cards » descendant ==");
    execute("on remarque deux cartes",
            "  set the marked of card 1 to true\n"
            "  set the marked of card 2 to true\n"
            "  set the marked of card 3 to false\n"
            "  put the number of marked cards & \" marquees\"");
    /* LE SENS PRECEDE LE « BY », et ma premiere version l'ecrivait apres.
     * La grammaire d'HyperCard est
     *
     *     sort [marked] cards [ascending|descending] [style] by <expression>
     *
     * et « ... by <expr> descending » donne donc, a juste titre, « texte
     * inattendu en fin de ligne ». Le harnais mesurait ma faute de syntaxe,
     * pas le tri. */
    ordre("descendant : ",
          "  sort marked cards descending by the short name of this card\n");

    hc_free(st);
    return 0;
}
