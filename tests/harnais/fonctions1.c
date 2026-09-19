/* LES FONCTIONS DE L'UTILISATEUR A UN ARGUMENT : servies SANS sonder l'ancien
 * moteur ?
 *
 * Le pendant de mondenoms.c pour l'autre porte. Le chemin a un argument
 * numerique de v3_fonction n'avait aucune liste : il sondait call_function
 * pour TOUT nom qu'il ne servait pas lui-meme. Or les noms qui arrivent la
 * sont precisement ceux que la v3 ne sert pas — c'est-a-dire les fonctions
 * ECRITES PAR L'UTILISATEUR. On demandait donc a l'ancien moteur s'il
 * connaissait « double » ou « spectre », pour s'entendre repondre non.
 *
 * Mesure sur les 197 harnais, instrument repare : 12 sondes, six noms —
 * getPattern, spectre, daysInMonth, carre, aplat, double. Apres la liste
 * V3_V1_FONCTIONS_1 : zero.
 *
 * L'invariant tenu ici :
 *
 *   1. une fonction utilisateur a un argument NUMERIQUE repond juste, et sans
 *      aucune sonde — c'est le cas qui produisait les douze ;
 *   2. les fonctions du monde que l'ancien moteur sert vraiment repondent
 *      toujours : la liste ne doit pas avoir ferme une porte utile ;
 *   3. un nom inconnu part au RECOURS, et n'est pas avale en silence par la
 *      liste.
 *
 * Le point 3 garde le point 1 : une liste qui laisse tout passer sans rien
 * servir donnerait aussi « zero sonde ».
 *
 * CE QUE LE POINT 3 REVELAIT — ET QUI EST CORRIGE DEPUIS. « nExistePas(3) »
 * ne produisait aucune erreur : il rendait son propre texte, « nExistePas
 * (3) ». Verifie avant ET apres la liste a l'epoque, le comportement etait
 * ANTERIEUR et n'etait donc pas une regression : c'etait la vieille regle
 * « nom inconnu = son propre nom », la meme qui faisait passer « short name
 * of o » pour un resultat. HyperCard, lui, disait « No such function ».
 *
 * Ce commentaire disait : « on l'enregistre ici pour que le jour ou on le
 * corrige, l'ecart se voie dans cette reference ». Ce jour est venu, et
 * l'ecart s'est vu exactement la — cette reference a bouge d'une ligne quand
 * v3_recours a cesse de rendre en clair les appels de fonction. La ligne 47
 * porte desormais :
 *
 *   !! fonction inconnue : nExistePas (v3, ligne 15 de …)
 *
 * Le detail de la correction et ce qu'elle tient sont dans appelinconnu.c ;
 * ici on garde seulement la trace que le point 3 a fini par servir.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG)  printf("      %s\n", t ? t : "");
  else if (k == HC_ERR)  printf("      [ERR] %s\n", t ? t : "");
  /* HC_INFO porte le bilan. L'oublier ici le rendait invisible : la premiere
   * version de ce harnais affichait une section vide et avait l'air de
   * prouver « aucune sonde ». Un test qui ne regarde pas ne prouve rien. */
  else if (k == HC_INFO) printf("   %s\n", t ? t : ""); }

static Object *g_bouton;

/* Le script complet, fonctions comprises, sur le bouton : c'est la hierarchie
 * normale de recherche d'une fonction utilisateur. */
static const char *SCRIPT =
    "function double n\n"
    "  return n * 2\n"
    "end double\n"
    "function spectre n\n"
    "  return \"teinte \" & n\n"
    "end spectre\n"
    "function daysInMonth m\n"
    "  get \"31,28,31,30,31,30,31,31,30,31,30,31\"\n"
    "  return item m of it\n"
    "end daysInMonth\n"
    "on essai\n"
    "  put the params\n"
    "end essai\n";

static void demande(const char *expr)
{
    char s[512];
    printf("   %s\n", expr);
    snprintf(s, sizeof s, "%son essaie\n  put %s\nend essaie\n", SCRIPT, expr);
    hc_set_script(g_bouton, s);
    hc_send(g_bouton, "essaie");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    g_bouton = hc_new_button(c, "B");
    hc_set_current_card(c);

    hc_v3_bilan_remise_a_zero();

    puts("== 1. fonctions de l'utilisateur, argument numerique ==");
    demande("double(21)");
    demande("spectre(7)");
    demande("daysInMonth(2)");
    /* Imbriquee : l'argument est lui-meme un appel. Chaque niveau passait par
     * la sonde. */
    demande("double(double(5))");
    /* Argument NON numerique : ce chemin ne sondait deja pas, on verifie
     * qu'il n'a pas change. */
    demande("spectre(\"x\")");

    puts("\n== 2. ce que l'ancien moteur sert vraiment, toujours servi ==");
    /* Ces noms sont dans V3_V1_FONCTIONS_1. Qu'ils passent par la v3 ou par
     * l'ancien moteur importe peu ici : ce qui compte est qu'ils REPONDENT,
     * et juste. La liste ne doit pas avoir ferme une porte utile. */
    demande("abs(-3)");
    demande("sqrt(16)");
    demande("trunc(7.9)");
    demande("round(2.5)");
    demande("length(\"abcde\")");
    demande("numToChar(65)");
    demande("charToNum(\"A\")");
    demande("value(\"2+3\")");
    demande("min(4,9)");
    demande("max(4,9)");
    demande("sum(1,2,3)");

    puts("\n== 3. un nom inconnu : parti au recours, PAS avale par la liste ==");
    puts("   (il dit desormais franchement qu'il ne connait pas ce nom)");
    demande("nExistePas(3)");

    puts("\n== le bilan : aucune sonde de nom ne doit apparaitre ==");
    hc_v3_bilan();

    hc_free(st);
    return 0;
}
