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

    printf("=== 6. L'EXPRESSION DU TRACEUR CARTÉSIEN ===\n");
    printf("   (CETTE SECTION A PORTÉ UNE RÉFÉRENCE FAUSSE, et il faut le\n");
    printf("    dire ici plutôt que de la corriger en silence. Elle affirmait\n");
    printf("    « sous HyperCard il affiche 1.188 et non 1.194 », et concluait\n");
    printf("    que le gabarit s'applique aux calculs intermédiaires.\n");
    printf("\n");
    printf("    LE 1.188 N'EST PAS UNE MESURE D'HYPERCARD. Il vient du\n");
    printf("    rapport de bogue initial, où il décrivait ce que HC affichait,\n");
    printf("    et il a été recopié comme s'il venait de Basilisk II. C'est la\n");
    printf("    même méprise que « 1/3*3 -> 0.9 » dans le harnais numfmt.\n");
    printf("\n");
    printf("    CE QUE LE MODÈLE MESURÉ PRÉDIT, lui, c'est 1.194 : le gabarit\n");
    printf("    n'intervient qu'à la sortie, donc les intermédiaires gardent\n");
    printf("    leur précision. C'est ce que HC rend maintenant, et c'est\n");
    printf("    cohérent avec les six booléens et la chaîne de tracé.\n");
    printf("\n");
    printf("    À CONFIRMER DANS BASILISK II, avec exactement ces quatre\n");
    printf("    lignes. Tant que ce n'est pas fait, la valeur ci-dessous est\n");
    printf("    celle de HC et de rien d'autre)\n");
    fais("set the numberFormat to \"0.000\"\n"
         "  put \"pi/144 en expression : \" & (pi/144)\n"
         "  put \"(x+2)*(x-3/2)^2*(x+1/2)/(x+1/2)/5\" into thev\n"
         "  put -31/64 into x\n"
         "  put \"le modele predit 1.194 (a confirmer) : \" & value(thev)");

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
