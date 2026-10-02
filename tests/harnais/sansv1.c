/* CE QUE L'ANCIEN MOTEUR SERVAIT ENCORE, ET QUE LA V3 SERT DÉSORMAIS SEULE.
 *
 * La v3 rappelait l'ancien moteur d'expressions — term_value, call_function,
 * parse_expr — dès qu'elle ne savait pas faire. Pour le retirer, il fallait
 * savoir ce qu'il servait encore, et la seule façon de le savoir était de le
 * couper : HC_SANS_V1=1. La suite entière, rejouée ainsi, perdait :
 *
 *   · « the number of cards of bg 1 » : 1 au lieu de 2, SANS ERREUR. Et en
 *     cherchant les formes voisines, que la suite n'exerçait pas : tout
 *     comptage qui porte une cible — « cards of this stack », « buttons of
 *     card 2 », « bgs of this stack ».
 *   · la forme « the » de charToNum, numToChar et random ;
 *   · « the maFonction of "ok" », une fonction de la pile sous cette forme ;
 *   · toute fonction du noyau appelée avec un argument de moins ou de trop —
 *     « length() », « sqrt() », « offset("a") », « sqrt(4,9) » —, que la v3
 *     refusait d'un « fonction inconnue : length », faux puisqu'elle existe.
 *     L'ancien moteur, lui, complétait par le vide et ignorait le surplus ;
 *     c'est ce qu'on avait d'abord repris. MESURÉ ENSUITE DANS HYPERCARD
 *     (Basilisk II), le 1er octobre : « length() » et « sqrt(4, 9) » y sont
 *     des erreurs. Les sections 4 et 5 lèvent donc « mauvais nombre
 *     d'arguments », avec et sans l'ancien moteur.
 *
 * Mesurées aussi, et ce sont des défauts de l'ancien moteur lui-même :
 *
 *   · « the number of cards of bg 9 », sur une pile de deux fonds, et « the
 *     number of cards of card 1 » rendaient le TOTAL de la pile ;
 *   · « sqrt("abc") » valait 0, « exp2("x") » valait 1 — là où « "abc" + 1 »
 *     est une faute ;
 *   · « the zorglub of 3 » rendait « zorglub of 3 » en clair ;
 *   · « the charToNum of ("a") » levait une erreur d'analyse, PUIS rendait 0.
 *
 * L'ANCIEN MOTEUR A ÉTÉ RETIRÉ LE 2 OCTOBRE. Jusque-là, ce harnais jouait
 * chaque cas deux fois, dans deux processus — avec et sans lui —, et
 * signalait toute réponse que la v3 ne savait pas donner seule. Il n'y en
 * avait plus aucune ; la dernière section, où l'écart était VOULU, prouvait
 * que le réglage coupait bien. Le moteur parti, il n'y a plus rien à
 * comparer : le harnais joue chaque cas une fois, et sa référence garde ce
 * que la v3 répond. Une réponse qui change ici est une perte à expliquer.
 *
 * Ce que fait HyperCard d'une fonction de la pile sous la forme « the »
 * — « the maF of "ok" » — est mesuré : c'est une erreur, et HC répond encore.
 * À décider. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static const char *CAS[] = {
    "== 1. un comptage qui porte une cible ==",
    "put the number of cards of bg 1",
    "put the number of cards of bg 2",
    "put the number of cards of background \"Deux\"",
    "put the number of cards in bg 2",
    "put the number of cds of bg 2",
    "put the number of cards of this bg",
    "put the number of marked cards of bg 2",
    "put the number of cards of this stack",
    "put the number of cards of stack \"Pile\"",
    "put the number of cards of bg 2 of this stack",
    "put the number of bgs of this stack",
    "put the number of backgrounds of stack \"Pile\"",
    "put the number of buttons of card 2",
    "put the number of card buttons of card \"C\"",
    "put the number of fields of card 2",
    "put the number of card fields of card 2",
    "put the number of bg fields of card 2",
    "put the number of parts of card 2",
    "put the number of fields of bg 2",
    "put the number of buttons of bg 1",
    "put the number of bg buttons of bg 1",
    "-- sans cible, rien n'a change",
    "put the number of cards",
    "put the number of buttons",
    "put the number of fields",
    "put the number of card 2",
    "-- une cible qui manque, ou qui ne contient pas ce type",
    "put the number of cards of bg 9",
    "put the number of cards of card 1",
    "put the number of card buttons of bg 1",
    "put the number of bgs of card 1",

    "== 2. les fonctions du noyau sous la forme « the » ==",
    "put the charToNum of \"a\"",
    "put the numToChar of 65",
    "put the random of 1",
    "put the sqrt of 16",
    "put the sqrt of \"\"",
    "put the sqrt of \"abc\"",
    "put the charToNum of (\"a\")",
    "put the sqrt of 4 + 1",
    "put the offset of \"a\"",

    "== 3. une fonction de la pile sous la forme « the » ==",
    "put the maF of \"ok\"",
    "put the maF of 3",
    "put the length of maF(\"x\")",
    "put the zorglub of 3",
    "put the zorglub of \"x\"",
    "-- les refus d'a cote gardent leur message",
    "put the zorglub of me",
    "put the textStyle of word 2 of card field \"Absent\"",

    "== 4. un argument de moins ==",
    "put \"[\" & length() & \"]\"",
    "put \"[\" & numToChar() & \"]\"",
    "put \"[\" & charToNum() & \"]\"",
    "put \"[\" & offset() & \"]\"",
    "put \"[\" & offset(\"a\") & \"]\"",
    "put \"[\" & random() & \"]\"",
    "put \"[\" & sqrt() & \"]\"",
    "put \"[\" & value() & \"]\"",
    "put \"[\" & annuity(1) & \"]\"",

    "== 5. un argument de trop ==",
    "put \"[\" & length(1,2) & \"]\"",
    "put \"[\" & charToNum(\"a\",\"b\") & \"]\"",
    "put \"[\" & sqrt(4,9) & \"]\"",

    "== 6. un argument vide, ou qui n'est pas un nombre ==",
    "put \"[\" & sqrt(\"\") & \"]\"",
    "put \"[\" & abs(empty) & \"]\"",
    "put \"[\" & annuity(\"\",\"\") & \"]\"",
    "put \"[\" & sqrt(\"abc\") & \"]\"",
    "put \"[\" & exp2(\"x\") & \"]\"",
    "put \"[\" & numToChar(\"x\") & \"]\"",
    "-- la regle qu'elles suivent, celle de l'arithmetique",
    "put \"[\" & (empty + 1) & \"]\"",
    "put \"[\" & (\"abc\" + 1) & \"]\"",

    "== 7. la ou l'ancien moteur se trompait ==",
    "-- il repetait l'erreur de la v3 :",
    "put the name of card 1 of stack \"PileAbsente\"",
    "-- le texte qu'on lui reconstituait perdait sa parenthese fermante :",
    "put the maF of (\"o\" & \"k\")",
    NULL
};

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    /* Deux fonds ENTRELACÉS — A C B D —, pour que le rang dans le fond et
     * le rang dans la pile ne coïncident pour aucune carte au-delà de la
     * première. Voir cartedufond. */
    Object *st = hc_new_stack("Pile");
    Object *f1 = hc_new_background(st, "Un");
    Object *f2 = hc_new_background(st, "Deux");
    Object *A  = hc_new_card(st, f1, "A");
    Object *C  = hc_new_card(st, f2, "C");
    Object *B  = hc_new_card(st, f1, "B");
    Object *D  = hc_new_card(st, f2, "D");
    hc_register_stack(st);
    hc_set_current_card(A);
    Object *b = hc_new_button(A, "B1");
    hc_new_button(A, "B2");
    hc_new_button(C, "C1");
    hc_new_field(C, "CF");
    hc_new_field(f2, "F2a");
    hc_new_field(f2, "F2b");
    hc_new_button(f1, "FB");
    D->marked = 1;
    (void)B;

    for (int i = 0; CAS[i]; i++) {
        if (CAS[i][0] == '=' || CAS[i][0] == '-') {
            printf("%s%s\n", CAS[i][0] == '=' ? "\n" : "   ", CAS[i]);
            continue;
        }
        char s[1024];
        printf("   %s\n", CAS[i]);
        snprintf(s, sizeof s,
                 "function maF x\n  return x & \"!\"\nend maF\n"
                 "on t\n  %s\nend t\n", CAS[i]);
        hc_set_script(b, s);
        hc_send(b, "t");
    }
    hc_free(st);
    return 0;
}
