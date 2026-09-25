/* Un synonyme de l'annexe F comme NOM de gestionnaire ou de fonction.
 *
 * MESURÉ AVANT LA CORRECTION : « function sec x … end sec » se faisait
 * refuser d'un « « end » ne reprend pas le nom » — alors que le nom était
 * bien repris. Dix-sept noms étaient dans ce cas, et un seul suffisait à
 * faire tomber TOUT le script qui le contenait : HypoGraph 0.91 définit
 * « function sec x » pour la sécante, si bien que ses dix autres
 * gestionnaires — openStack, errorDialog, arrowKey, csc, cot… — n'existaient
 * plus non plus.
 *
 * LA RACINE, une de plus dans la même famille : un mot lu BRUT d'un côté et
 * NORMALISÉ de l'autre. Le lexer déplie les synonymes partout, sans regarder
 * la position, si bien que « end sec » arrive au parseur comme « seconds » ;
 * mais analyse_gestionnaire comparait ce jeton au texte brut du nom
 * d'ouverture, « sec ». Deux formes, une comparaison, un refus faux.
 *
 * CE QUE CE HARNAIS TIENT :
 *   — les dix-sept noms s'ouvrent et se ferment ;
 *   — ils sont APPELABLES, pas seulement analysables (le vrai test) ;
 *   — le synonyme reste une unité de temps là où c'en est une : définir
 *     « function sec » ne doit pas casser « wait 2 sec » ;
 *   — un « end » qui se trompe VRAIMENT de nom se plaint toujours. Sans ce
 *     dernier point, on aurait pu « corriger » en ne comparant plus rien.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static int g_faute;
static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) { g_faute = 1; printf("   [ERR] %s\n", t); }
}

static Object *g_pile, *g_carte;

/* Les synonymes de hct_lex.c, terme à terme. Les ajouter ici quand la table
 * grandit est le prix d'une ligne, et c'est ce qui fait que le prochain
 * synonyme ne repassera pas par le même trou. */
static const char *SYNONYMES[] = {
    "abbr", "abbrev", "bg", "bkgnd", "bgs", "bkgnds", "botright",
    "btn", "btns", "cd", "cds", "char", "chars", "fld", "flds",
    "grey", "hilite", "highlite", "hilight", "loc", "mid", "msg",
    "poly", "prev", "rect", "reg", "sec", "secs", "tick",
    NULL
};

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    g_pile  = hc_new_stack("Synonymes");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);

    printf("=== 1. LA DÉFINITION s'accepte, pour les vingt-neuf ===\n");
    printf("   (c'est exactement ce que la correction change : le « end »\n");
    printf("    reprend le nom, et le gestionnaire existe. On le prouve en\n");
    printf("    APPELANT un voisin du même script : s'il répond, c'est que le\n");
    printf("    script tout entier a survécu à la définition d'à côté — ce\n");
    printf("    qui était faux avant, où une seule suffisait à tout perdre)\n");
    int perdus = 0;
    for (int i = 0; SYNONYMES[i]; i++) {
        char script[512];
        snprintf(script, sizeof script,
                 "function %s x\n"
                 "  return x * 2\n"
                 "end %s\n"
                 "on essai\n"
                 "  put \"voisin vivant après « function %s » : \" & temoin(21)\n"
                 "end essai\n"
                 "function temoin n\n"
                 "  return n + 21\n"
                 "end temoin\n",
                 SYNONYMES[i], SYNONYMES[i], SYNONYMES[i]);
        g_faute = 0;
        hc_set_script(g_pile, script);
        hc_send(g_carte, "essai");
        if (g_faute) perdus++;
    }
    printf("   -> %d script(s) perdu(s) sur 29\n", perdus);

    printf("=== 1b. L'APPEL, lui, dépend du nom : onze restent masqués ===\n");
    printf("   (« cd(21) » se lit « card 21 », « char(21) » attend « of », et\n");
    printf("    « msg » est la boîte de message. Ce n'est PAS le défaut qu'on\n");
    printf("    corrige, et HyperCard les masquait de même : un mot-clé de\n");
    printf("    type l'emporte sur une fonction de l'auteur. On l'inscrit ici\n");
    printf("    pour que la frontière soit écrite quelque part, et pour voir\n");
    printf("    si elle bouge un jour sans qu'on l'ait voulu)\n");
    printf("\n");
    printf("    LA LIGNE « msg(21) = ... » SURPREND et elle est juste : depuis\n");
    printf("    que la boîte de messages se LIT, « msg » s'évalue et rend son\n");
    printf("    contenu — ici ce que le tour précédent y a laissé. C'est le\n");
    printf("    « (21) » qui reste sur la ligne et la fait échouer. Le nom est\n");
    printf("    toujours masqué, et on voit maintenant PAR QUOI.\n");
    int masques = 0;
    for (int i = 0; SYNONYMES[i]; i++) {
        char script[512];
        snprintf(script, sizeof script,
                 "function %s x\n"
                 "  return x * 2\n"
                 "end %s\n"
                 "on essai\n"
                 "  put \"%s(21) = \" & %s(21)\n"
                 "end essai\n",
                 SYNONYMES[i], SYNONYMES[i], SYNONYMES[i], SYNONYMES[i]);
        g_faute = 0;
        hc_set_script(g_pile, script);
        hc_send(g_carte, "essai");
        if (g_faute) { masques++; printf("   masqué : %s\n", SYNONYMES[i]); }
    }
    printf("   -> %d nom(s) masqué(s) à l'appel sur 29\n", masques);

    printf("=== 2. « on » aussi, pas seulement « function » ===\n");
    hc_set_script(g_pile,
        "on msg quoi\n"
        "  put \"gestionnaire msg reçoit : \" & quoi\n"
        "end msg\n"
        "on essai\n"
        "  msg \"bonjour\"\n"
        "end essai\n");
    hc_send(g_carte, "essai");

    printf("=== 3. les trois fonctions de HypoGraph, dont « sec » ===\n");
    printf("   (csc et cot ne sont pas des synonymes et passaient déjà ;\n");
    printf("    sec tombait, et emportait les deux autres avec tout le script)\n");
    hc_set_script(g_pile,
        "function csc x\n  return 1/sin(x)\nend csc\n"
        "function sec x\n  return 1/cos(x)\nend sec\n"
        "function cot x\n  return 1/tan(x)\nend cot\n"
        "on essai\n"
        "  put \"sec(0) = \" & sec(0)\n"
        "  put \"csc(pi/2) = \" & csc(pi/2)\n"
        "  put \"cot(pi/4) = \" & cot(pi/4)\n"
        "end essai\n");
    hc_send(g_carte, "essai");

    printf("=== 4. le voisin : « wait 2 sec » reste une durée ===\n");
    printf("   (si le synonyme avait cessé d'en être un, on aurait échangé\n");
    printf("    un défaut contre un autre)\n");
    hc_set_script(g_pile,
        "function sec x\n  return 1/cos(x)\nend sec\n"
        "on essai\n"
        "  wait 1 tick\n"
        "  wait 2 ticks\n"
        "  put \"les attentes s'analysent, et sec(0) vaut \" & sec(0)\n"
        "end essai\n");
    hc_send(g_carte, "essai");

    printf("=== 5. et un « end » qui se trompe VRAIMENT se plaint ===\n");
    printf("   (la correction ne devait pas consister à ne plus comparer.\n");
    printf("    Le rapport des fautes est paresseux : il sort à l'usage, pas\n");
    printf("    à la pose — d'où le send)\n");
    hc_set_script(g_pile,
        "function sec x\n"
        "  return 1/cos(x)\n"
        "end cosec\n"
        "on essai\n"
        "  put sec(0)\n"
        "end essai\n");
    hc_send(g_carte, "essai");
    printf("=== 6. le témoin : un nom ordinaire n'a pas bougé ===\n");
    hc_set_script(g_pile,
        "function tangente x\n  return tan(x)\nend tangente\n"
        "on essai\n  put \"tangente(0) = \" & tangente(0)\nend essai\n");
    hc_send(g_carte, "essai");
    return 0;
}
