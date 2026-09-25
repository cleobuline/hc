/* Diviser par zéro rend une VALEUR, pas une erreur.
 *
 * MESURÉ AVANT LA CORRECTION : « division par zéro (v3, ligne 53 de
 * background "commun".mouseDown) », un dialogue, et la courbe intraçable.
 *
 * MESURÉ SUR LA PILE ORIGINELLE, sous HyperCard : au même point, le traceur
 * affiche « -.500,NAN(004) » et continue. C'est l'arithmétique SANE du
 * Macintosh : « 1/0 » vaut INF, « 0/0 » vaut NAN(004), et le code dit la
 * raison. HyperCard n'a pas d'erreur « division par zéro » du tout.
 *
 * LE DÉFAUT ÉTAIT DOUBLE, et c'est ce qui le rend intéressant : non seulement
 * l'erreur n'existe pas chez HyperCard, mais la pile avait ÉCRIT le code qui
 * traite ces valeurs —
 *
 *     if y contains "NAN" or y is "INF" then
 *       if y≠"NAN(037)" then put "ERR: x=" & x & ",y=" & y into fld errr
 *       if y is "INF" then put "y=±∞" into item 2 of fld errr
 *       else if y is "NAN(004)" then put "y=0/0 (indeterminate)" into …
 *
 * — code qui ne pouvait jamais s'exécuter, puisque le dialogue partait avant.
 * Trois noms de valeurs SANE en dur dans une pile de 1995 : le meilleur
 * témoin qu'on puisse avoir de ce que HyperCard rendait vraiment.
 *
 * L'ÉQUATION, qui n'est pas un cas tordu :
 *
 *     y = (x+2)*(x-3/2)^2*(x+1/2)/(x+1/2)/5
 *
 * Le facteur (x+1/2) est au numérateur ET au dénominateur : la courbe est
 * lisse partout, y compris en -1/2 où elle a une limite parfaitement définie.
 * Seul le CALCUL y passe par 0/0. Avec theScale à 64 la souris avance par pas
 * de 1/64 et frappe -0.5 exactement.
 *
 * LA DISSYMÉTRIE VOULUE : INF est un nombre, NAN n'en est pas un. La même
 * pile compare « if y is "INF" » ET « if ny > itt » sur la même valeur —
 * l'infini doit donc s'ordonner ; mais elle écrit « if y contains "NAN" » et
 * « if y is "NAN(004)" », deux comparaisons de texte. Faire de NaN un nombre
 * les casserait : NaN n'est égal à rien, et hct_compare rendrait « ni
 * inférieur ni supérieur », c'est-à-dire ÉGAL. On le vérifie aux deux
 * sections 6 et 7, sans quoi la correction aurait déplacé le défaut.
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

    g_pile  = hc_new_stack("SANE");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);

    printf("=== 1. les trois cas de base ===\n");
    fais("put 1/0");
    fais("put -1/0");
    fais("put 0/0");

    printf("=== 2. LE CAS DE LA PILE : l'équation, au point qui cassait ===\n");
    printf("   (x = -32/64, soit -0.5 exactement, avec son numberFormat)\n");
    fais("put \"(x+2)*(x-3/2)^2*(x+1/2)/(x+1/2)/5\" into thev\n"
         "  set the numberFormat to \"0.000\"\n"
         "  put -32/64 into x\n"
         "  put x & \",\" & value(thev)");

    printf("=== 3. et le point d'à côté, qui marchait déjà ===\n");
    printf("   (x = -31/64 : elle lit « -0.484,1.188 » à l'écran, et c'est\n");
    printf("    bien 1.188 et non 1.194 — le numberFormat arrondit AUSSI les\n");
    printf("    calculs intermédiaires, comme chez HyperCard)\n");
    fais("put \"(x+2)*(x-3/2)^2*(x+1/2)/(x+1/2)/5\" into thev\n"
         "  set the numberFormat to \"0.000\"\n"
         "  put -31/64 into x\n"
         "  put x & \",\" & value(thev)");

    printf("=== 4. le CODE survit aux calculs qui suivent ===\n");
    printf("   (dans l'équation, le 0/0 est suivi d'une division par cinq :\n");
    printf("    si le code ne voyageait pas, on lirait « NAN » tout court)\n");
    fais("set the numberFormat to \"0.######\"\n"
         "  put 0/0 into z\n"
         "  put \"z      = \" & z\n"
         "  put \"z/5    = \" & (0/0)/5\n"
         "  put \"z*2+1  = \" & (0/0)*2+1");

    printf("=== 5. « divide by 0 », le JUMEAU : la commande, pas l'opérateur ===\n");
    printf("   (une pile peut employer l'une ou l'autre ; corriger la seule\n");
    printf("    expression aurait laissé la moitié du défaut en place)\n");
    fais("put 7 into n\n"
         "  divide n by 0\n"
         "  put \"7 divisé par 0 : \" & n");
    fais("put 0 into n\n"
         "  divide n by 0\n"
         "  put \"0 divisé par 0 : \" & n");

    printf("=== 6. INF s'ORDONNE : c'est un nombre ===\n");
    printf("   (« if ny > itt » de la pile doit trancher sur des nombres)\n");
    fais("put 1/0 into y\n"
         "  put \"y is INF      : \" & (y is \"INF\")\n"
         "  put \"y > 625       : \" & (y > 625)\n"
         "  put \"-1/0 < -625   : \" & (-1/0 < -625)\n"
         "  put \"INF + 1       : \" & (1/0 + 1)");

    printf("=== 7. NAN se CALCULE, mais ne s'ORDONNE pas ===\n");
    printf("   (les deux comparaisons que la pile écrit doivent répondre vrai,\n");
    printf("    et « y = 5 » doit répondre FAUX : un NaN n'est égal à rien\n");
    printf("    d'autre qu'à un NaN du même code)\n");
    fais("put 0/0 into y\n"
         "  put \"y contains NAN   : \" & (y contains \"NAN\")\n"
         "  put \"y is NAN(004)    : \" & (y is \"NAN(004)\")\n"
         "  put \"y is NAN(037)    : \" & (y is \"NAN(037)\")\n"
         "  put \"y = 5            : \" & (y = 5)\n"
         "  put \"y is not 5       : \" & (y is not 5)");

    printf("=== 7b. L'ORDRE se fait sur le TEXTE, et c'est la pile qui tranche ===\n");
    printf("   (j'avais d'abord fait rendre false aux quatre, au nom de la\n");
    printf("    virgule flottante. C'était raisonner sur IEEE au lieu de\n");
    printf("    regarder la pile — voir 7c, qui est la mesure qui compte)\n");
    fais("put 0/0 into y\n"
         "  put \"y > 625   : \" & (y > 625)\n"
         "  put \"y < 625   : \" & (y < 625)\n"
         "  put \"y = 5     : \" & (y = 5)");

    printf("=== 7c. LE GARDE DE LA PILE, tel qu'il est écrit ===\n");
    printf("   (if ny > itt or ny < -itt or nx > itt or nx < -itt then\n");
    printf("      if y contains \"NAN\" or y is \"INF\" then …\n");
    printf("      put empty into nx        -- on lève le crayon\n");
    printf("\n");
    printf("    Le traitement du NaN est DANS le test de bornes : pour être\n");
    printf("    seulement atteignable, « ny > itt » doit répondre VRAI sur un\n");
    printf("    NaN. Avec l'ordre à false le garde ne partait pas, la pile\n");
    printf("    traçait vers un point non fini, et la courbe recevait une\n");
    printf("    barre verticale en travers. Mesuré à l'écran)\n");
    fais("put 625 into itt\n"
         "  put 0/0 into y\n"
         "  put round(-(y-0) * 64 + 171) into ny\n"
         "  put 224 into nx\n"
         "  put \"ny vaut \" & ny\n"
         "  if ny > itt or ny < -itt or nx > itt or nx < -itt then\n"
         "    if y contains \"NAN\" or y is \"INF\" then\n"
         "      put \"ERR: x=-0.5,y=\" & y\n"
         "      if y is \"NAN(004)\" then put \"y=0/0 (indeterminate)\"\n"
         "    end if\n"
         "    put \"crayon levé : la courbe aura un TROU, pas une barre\"\n"
         "  else\n"
         "    put \"le garde n'a pas pris — c'est la barre\"\n"
         "  end if");

    printf("=== 7d. et si une pile y va quand même : on REFUSE de dessiner ===\n");
    printf("   (coord_champ passe par hc_coord, qui rend le DÉFAUT — zéro —\n");
    printf("    quand il ne sait pas lire. Zéro est une coordonnée valide :\n");
    printf("    rien ne distinguait le bord de la carte d'une valeur qu'on\n");
    printf("    n'avait pas su lire, et c'est ce qui rendait la barre muette)\n");
    fais("put 0/0 into ny\n"
         "  drag from 10,20 to 224,ny\n"
         "  click at 224,ny\n"
         "  put \"et le témoin, lui, passe :\"\n"
         "  drag from 10,20 to 30,40");

    printf("=== 7e. LES TROIS CODES MESURÉS SOUS HYPERCARD ===\n");
    printf("   (relevés dans la boîte de message de HyperCard sous Basilisk II,\n");
    printf("    pas déduits d'une table — je m'étais trompé en pensant 022 pour\n");
    printf("    le logarithme. Le C ne les donne pas : sur cette machine les\n");
    printf("    trois rendent un NaN de charge NULLE, il faut les poser)\n");
    fais("set the numberFormat to \"0.######\"\n"
         "  put \"ln(-1)    = \" & ln(-1)\n"
         "  put \"sqrt(-1)  = \" & sqrt(-1)\n"
         "  put \"0*(1/0)   = \" & 0*(1/0)");

    printf("=== 7f. le SIGNE compte : sqrt(-1) porte un moins, pas les autres ===\n");
    printf("   (le bit de signe d'un NaN n'a aucun sens arithmétique, mais SANE\n");
    printf("    le montre et une pile peut le comparer)\n");
    fais("put sqrt(-1) into y\n"
         "  put \"y is -NAN(001) : \" & (y is \"-NAN(001)\")\n"
         "  put \"y is NAN(001)  : \" & (y is \"NAN(001)\")\n"
         "  put \"y contains NAN : \" & (y contains \"NAN\")");

    printf("=== 7g. LA CHARGE SE LIT EN DÉCIMAL, PAS EN OCTAL ===\n");
    printf("   (strtod lit la charge d'un NaN en base 0 : un zéro de tête la\n");
    printf("    fait passer en OCTAL, et quatre des six codes que HyperCard\n");
    printf("    écrit se corrompaient en silence —\n");
    printf("      NAN(008) -> 0    NAN(009) -> 0\n");
    printf("      NAN(036) -> 30   NAN(037) -> 31\n");
    printf("    NAN(037) est écrit EN DUR dans la pile, et le code voyage dans\n");
    printf("    la charge : la corruption aurait survécu à tous les calculs)\n");
    fais("set the numberFormat to \"0.######\"\n"
         "  put \"NAN(001) relu : \" & (\"NAN(001)\" + 0)\n"
         "  put \"NAN(008) relu : \" & (\"NAN(008)\" + 0)\n"
         "  put \"NAN(009) relu : \" & (\"NAN(009)\" + 0)\n"
         "  put \"NAN(036) relu : \" & (\"NAN(036)\" + 0)\n"
         "  put \"NAN(037) relu : \" & (\"NAN(037)\" + 0)\n"
         "  put \"-NAN(001) relu : \" & (\"-NAN(001)\" + 0)");

    printf("=== 7h. et le code survit aux calculs, comme celui de 0/0 ===\n");
    fais("set the numberFormat to \"0.######\"\n"
         "  put \"ln(-1)/5   = \" & ln(-1)/5\n"
         "  put \"sqrt(-1)*2 = \" & sqrt(-1)*2");

    printf("=== 7i. le témoin : les domaines VALIDES n'ont pas bougé ===\n");
    fais("set the numberFormat to \"0.######\"\n"
         "  put \"ln(1)     = \" & ln(1)\n"
         "  put \"ln(0)     = \" & ln(0)\n"
         "  put \"sqrt(4)   = \" & sqrt(4)\n"
         "  put \"0*5       = \" & 0*5\n"
         "  put \"2*(1/0)   = \" & 2*(1/0)");

    printf("=== 8. « mod » par zéro est une ERREUR, pas une valeur ===\n");
    printf("   (MESURÉ : « put 5 mod 0 » sous HyperCard ouvre un dialogue,\n");
    printf("    « can't mod by 0 ». J'avais supposé qu'il rendait un NaN comme\n");
    printf("    la division, faute d'en connaître le code SANE — faux. Les deux\n");
    printf("    opérations se ressemblent et HyperCard ne les traite pas pareil,\n");
    printf("    ce qui est précisément pourquoi on mesure au lieu de déduire)\n");
    fais("set the numberFormat to \"0.######\"\n  put 5 mod 0");
    fais("put 5 mod 2");

    printf("=== 9. « div » par zéro REFUSE aussi ===\n");
    printf("   (MESURÉ : « put 7 div 0 » ET « put 0 div 0 » ouvrent tous deux\n");
    printf("    le dialogue « can't div by zero ». J'avais fait suivre à div la\n");
    printf("    règle de « / » parce que c'est une division — deuxième fois que\n");
    printf("    la déduction se fait démentir sur la même famille.\n");
    printf("\n");
    printf("    La cohérence est lisible une fois qu'on la voit : div et mod\n");
    printf("    sont des opérations ENTIÈRES, et l'arithmétique SANE qui\n");
    printf("    fabrique INF et les NaN est celle des FLOTTANTS. Seule « / »\n");
    printf("    y a droit)\n");
    fais("put 7 div 0");
    fais("put 0 div 0");
    fais("put 7 div 2");

    printf("=== 9b. « divide … by 0 », la commande : une VALEUR ===\n");
    printf("   (MESURÉ, et il le fallait, puisque les deux opérations d'à côté\n");
    printf("    font l'inverse. La ligne de partage ne passe donc pas entre\n");
    printf("    l'expression et la commande, mais entre le FLOTTANT et\n");
    printf("    l'ENTIER : « / » et « divide » sont la même division flottante\n");
    printf("    sous deux écritures, « div » et « mod » sont entières et\n");
    printf("    refusent.\n");
    printf("\n");
    printf("    Le cas 0/0 de la commande n'a pas été relevé à part et n'a pas\n");
    printf("    à l'être : il emprunte la même ligne de code que l'opérateur,\n");
    printf("    les deux ne peuvent pas diverger)\n");
    fais("put 7 into n\n"
         "  divide n by 0\n"
         "  put \"7 divisé par 0 : \" & n");
    fais("put 0 into n\n"
         "  divide n by 0\n"
         "  put \"0 divisé par 0 : \" & n");

    printf("=== 10. les témoins : ce qui marchait marche encore ===\n");
    fais("set the numberFormat to \"0.######\"\n"
         "  put \"6/3     = \" & 6/3\n"
         "  put \"7 div 2 = \" & (7 div 2)\n"
         "  put \"7 mod 2 = \" & (7 mod 2)\n"
         "  put \"1e308*2 = \" & (1e308*2)\n"
         "  put \"1.5e308 = \" & (1.5e308 + 0)");
    return 0;
}
