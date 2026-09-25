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
 * RESTE OUVERT, et ce n'est pas ce harnais qui le tranche : « add » DOIT-il
 * passer par le numberFormat ? Section 5.
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

    printf("=== 5. LA CAUSE, chez la pile qui l'a signalé ===\n");
    printf("   (trois « add » suffisent à la montrer : inutile d'en faire dix\n");
    printf("    millions pour comprendre pourquoi t n'avance pas)\n");
    fais("set the numberFormat to \"0.######\"\n"
         "  put \"pi/144 vaut \" & (pi/144)\n"
         "  set the numberFormat to \"0.0\"\n"
         "  put 0 into t\n"
         "  add pi/144 to t\n"
         "  put \"après un add   : t = \" & t\n"
         "  add pi/144 to t\n"
         "  put \"après deux add : t = \" & t\n"
         "  add pi/144 to t\n"
         "  put \"après trois add : t = \" & t");

    printf("=== 6. et sans le gabarit, le même compteur avance ===\n");
    fais("set the numberFormat to \"0.######\"\n"
         "  put 0 into t\n"
         "  put 0 into tours\n"
         "  repeat until t > 2*pi\n"
         "    add pi/144 to t\n"
         "    add 1 to tours\n"
         "  end repeat\n"
         "  put \"la boucle a fait \" & tours & \" tours, t = \" & t");
    return 0;
}
