/* Une boucle emballée se VOIT : elle ne casse plus en silence.
 *
 * L'exécuteur borne toute boucle à dix millions de tours, pour qu'un script
 * fautif ne gèle pas l'application définitivement. Mais les trois sorties
 * étaient de simples « break » : la boucle s'arrêtait, et le gestionnaire
 * CONTINUAIT comme si elle s'était terminée normalement. Le script rendait un
 * résultat faux, sans un mot.
 *
 * Le commentaire de v3_respire promettait pourtant l'inverse depuis le
 * premier jour — « le même message : une boucle emballée doit se voir ». Le
 * message n'a jamais existé. Un commentaire qui décrit une intention plutôt
 * qu'un comportement est un piège de plus.
 *
 * SIGNALÉ PAR UNE PILE RÉELLE, et la cause vaut d'être inscrite parce qu'elle
 * n'a rien d'évident. HypoGraph 0.91 pose, DANS sa boucle de tracé polaire :
 *
 *     set the numberFormat to 0.0
 *     ...
 *     add theInt to t          -- theInt vaut pi/144, soit 0.0218
 *
 * Le gabarit remet en forme le résultat de « add », si bien que t vaut 0.0
 * après le premier tour, 0.0 après le deuxième, et ainsi de suite. Le
 * « repeat until t > 2*pi » ne finit jamais. L'application a l'air figée
 * plusieurs minutes, puis repart sans rien avoir tracé et sans rien
 * expliquer.
 *
 * MESURÉ DEPUIS, et corrigé : sous HyperCard dans Basilisk II le même bouton
 * trace sa courbe et s'arrête. La boucle avance donc là-bas, et « add » n'y
 * est pas mis en forme. Voir les sections 5 et 6, qui tiennent ensemble les
 * DEUX mesures — celle-ci et celle du traceur cartésien, qui prouve l'inverse
 * pour les expressions.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *g_pile, *g_carte;
static void fais(const char *ligne)
{
    char script[1024];
    snprintf(script, sizeof script, "on essai\n  %s\nend essai\n", ligne);
    hc_set_script(g_pile, script);
    hc_send(g_carte, "essai");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    g_pile  = hc_new_stack("Emballee");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);

    printf("=== 1. « repeat N times » au-delà du plafond ===\n");
    printf("   (et la ligne d'après NE DOIT PAS s'exécuter : la faute\n");
    printf("    interrompt le gestionnaire, elle ne se contente pas de sortir)\n");
    fais("repeat 20000000 times\n"
         "  end repeat\n"
         "  put \"cette ligne ne doit pas paraître\"");

    printf("=== 2. « repeat until » dont la condition ne vient jamais ===\n");
    fais("repeat until 1 is 2\n"
         "  end repeat\n"
         "  put \"cette ligne ne doit pas paraître\"");

    printf("=== 3. « repeat while » de même ===\n");
    fais("repeat while 1 is 1\n"
         "  end repeat\n"
         "  put \"cette ligne ne doit pas paraître\"");

    printf("=== 4. les témoins : une boucle normale ne dit rien ===\n");
    fais("put 0 into n\n"
         "  repeat 1000 times\n"
         "    add 1 to n\n"
         "  end repeat\n"
         "  put \"mille tours : n = \" & n");
    fais("put 0 into n\n"
         "  repeat with i = 1 to 100\n"
         "    add i to n\n"
         "  end repeat\n"
         "  put \"somme de 1 à 100 = \" & n");
    fais("put 0 into n\n"
         "  repeat for each item k in \"a,b,c,d\"\n"
         "    add 1 to n\n"
         "  end repeat\n"
         "  put \"quatre articles : n = \" & n");

    printf("=== 5. LA BOUCLE DE LA PILE, écrite comme elle l'écrit ===\n");
    printf("   (« put pi/144 into theInt » AVANT, au format par défaut, puis\n");
    printf("    « add theInt to t » DANS la boucle, sous un gabarit à une\n");
    printf("    décimale. Une VARIABLE, pas une expression : la distinction\n");
    printf("    est tout le sujet.\n");
    printf("\n");
    printf("    Avant correction, t valait 0.0 à chaque tour et la boucle ne\n");
    printf("    finissait jamais)\n");
    fais("put pi/144 into theInt\n"
         "  put \"theInt vaut \" & theInt\n"
         "  put 0 into t\n"
         "  put 0 into tours\n"
         "  repeat until t > 2*pi\n"
         "    set the numberFormat to 0.0\n"
         "    add theInt to t\n"
         "    add 1 to tours\n"
         "  end repeat\n"
         "  set the numberFormat to \"0.######\"\n"
         "  put \"la boucle a fait \" & tours & \" tours, t = \" & t");

    printf("=== 6. ET L'EXPRESSION, ELLE, RESTE MISE EN FORME ===\n");
    printf("   (c'est l'autre mesure, et elle vient du traceur cartésien de\n");
    printf("    la même pile : sous HyperCard il affiche 1.188 et non 1.194,\n");
    printf("    ce qui prouve que le gabarit s'applique aux calculs, y compris\n");
    printf("    intermédiaires.\n");
    printf("\n");
    printf("    La frontière n'est donc pas « le numberFormat s'applique ou\n");
    printf("    non » : elle passe entre les OPÉRATEURS, qui montrent, et les\n");
    printf("    COMMANDES d'accumulation, qui comptent. Corriger l'un sans\n");
    printf("    vérifier l'autre aurait cassé le traceur pour réparer le\n");
    printf("    tracé polaire)\n");
    fais("set the numberFormat to \"0.000\"\n"
         "  put \"pi/144 en expression : \" & (pi/144)\n"
         "  put \"(x+2)*(x-3/2)^2*(x+1/2)/(x+1/2)/5\" into thev\n"
         "  put -31/64 into x\n"
         "  put \"le traceur doit lire 1.188 : \" & value(thev)");

    printf("=== 7. les quatre commandes d'accumulation, ensemble ===\n");
    printf("   (MESURÉ pour « add » seul. Les trois autres partagent la même\n");
    printf("    ligne de code et la même forme de phrase ; les traiter\n");
    printf("    autrement demanderait une raison, et il n'y en a pas. Mais\n");
    printf("    c'est une extension, pas un relevé, et ça se dit)\n");
    fais("set the numberFormat to \"0.######\"\n"
         "  put 1/3 into p\n"
         "  put \"p vaut \" & p & \" (calculé AVANT le gabarit étroit)\"\n"
         "  set the numberFormat to \"0.0\"\n"
         "  put 1 into a\n  add p to a\n      put \"add      : \" & a\n"
         "  put 1 into b\n  subtract p from b\n put \"subtract : \" & b\n"
         "  put 1 into c\n  multiply c by p\n  put \"multiply : \" & c\n"
         "  put 1 into d\n  divide d by 3\n    put \"divide   : \" & d");
    return 0;
}
