/* « FAMILY N » ET « MENU I » : UN DESIGNATEUR QUI SE CALCULE.
 *
 * SIGNALE PAR UN AUDIT EXTERIEUR. La reference HyperTalk donne un intExpr —
 * une EXPRESSION entiere — la ou nous n'acceptions qu'un chiffre ecrit a la
 * main. Mesure avant correction :
 *
 *     put the selectedButton of family 1      -> repond
 *     put 6 into n
 *     put the selectedButton of family n      -> ECHO + faute de syntaxe
 *     put the selectedButton of family (3+3)  -> erreur d'analyse
 *
 * DEUX CAUSES SUPERPOSEES, et la premiere etait loin de la famille.
 *
 * 1. LE RECOURS NE RECEVAIT PAS DE CONTEXTE. Une famille n'est pas un Object,
 *    un menu non plus : hct_resout ne peut rien pour eux, et le recours est
 *    leur SEUL chemin. Or `recours` etait le seul rappel de HctHote a ne pas
 *    porter de HctContexte — `commande`, son symetrique pour les
 *    instructions, l'a toujours porte. Sans contexte, l'hote ne peut rien
 *    EVALUER : il lisait le jeton litteral et s'arretait la. Trois
 *    commentaires de hc_core.c annoncaient cette limite comme une fatalite ;
 *    elle tenait a un parametre manquant.
 *
 * 2. L'ANALYSEUR REFUSAIT LE MOT NU. designateur_suit garde « menu » et
 *    « family » contre une lecture trop gourmande, et son commentaire
 *    disait : « "menu maVariable" serait legal en HyperTalk, mais l'accepter
 *    ferait de "put menu into x" un menu nomme into ». L'objection etait
 *    juste et le remede trop large : il suffit d'exclure les mots qui
 *    STRUCTURENT une phrase, ce que la liste STRUCTURELS fait deja pour tous
 *    les autres types d'objets.
 *
 *    Les deux formes a proteger le sont par cette liste :
 *
 *        put menu into x            « into » structurel -> pas un menu
 *        the family of button "X"   « of »   structurel -> pas une famille
 *
 *    et la seconde compte double, « family » etant aussi un nom de
 *    propriete : sans ce garde, « family of » se lirait comme une famille
 *    designee par une variable nommee « of ».
 *
 * Ce harnais tient les quatre formes du designateur, les deux pieges que la
 * liste doit continuer d'arreter, et les menus — qui n'etaient pas dans le
 * rapport et que la meme correction repare.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

static void execute(const char *titre, const char *corps)
{
    char s[2048];
    printf("   %s\n", titre);
    snprintf(s, sizeof s, "on essaie\n%s\nend essaie\n", corps);
    hc_set_script(b, s);
    hc_send(b, "essaie");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);
    hc_new_button(c, "Oui");
    hc_new_button(c, "Non");
    hc_new_button(bg, "Bleu");
    b = hc_new_button(c, "B");

    execute("mise en place : Oui et Non en famille 6, Non allume",
            "  set the style of button \"Oui\" to radioButton\n"
            "  set the style of button \"Non\" to radioButton\n"
            "  set the family of button \"Oui\" to 6\n"
            "  set the family of button \"Non\" to 6\n"
            "  set the hilite of button \"Non\" to true\n"
            "  put \"pret\"");

    puts("\n== 1. les quatre ecritures du designateur donnent la meme chose ==");
    execute("un chiffre",        "  put the selectedButton of family 6");
    execute("une variable",      "  put 6 into n\n"
                                 "  put the selectedButton of family n");
    execute("une parenthese",    "  put the selectedButton of family (3+3)");
    execute("un calcul en variable",
                                 "  put 3 into a\n"
                                 "  put the selectedButton of family (a * 2)");
    execute("une variable TEXTE","  put \"6\" into s\n"
                                 "  put the selectedButton of family s");

    puts("\n== 2. la portee explicite marche avec un designateur calcule ==");
    execute("card family n",     "  put 6 into n\n"
                                 "  put the selectedButton of card family n");
    execute("bg family n",       "  set the family of button \"Bleu\" to 2\n"
                                 "  set the style of button \"Bleu\" to radioButton\n"
                                 "  set the hilite of button \"Bleu\" to true\n"
                                 "  put 2 into n\n"
                                 "  put the selectedButton of bg family n");

    puts("\n== 3. une famille sans choix, une famille hors bornes ==");
    execute("famille legale, personne d'allume",
                                 "  put 5 into n\n"
                                 "  put \"[\" & the selectedButton of family n & \"]\"");
    execute("hors bornes, par variable",
                                 "  put 20 into n\n"
                                 "  put the selectedButton of family n");

    puts("\n== 4. LES DEUX PIEGES QUE LA LISTE DOIT ARRETER ==");
    /* Si « of » passait pour un designateur, cette lecture de propriete
     * deviendrait une reference de famille et ne rendrait plus rien. */
    execute("the family of button, la propriete",
            "  put the family of button \"Non\"");
    /* Et si « into » passait, « menu » deviendrait un menu nomme into. */
    execute("put menu into x, la variable",
            "  put \"une valeur\" into menu\n"
            "  put menu into x\n"
            "  put x");

    puts("\n== 5. les MENUS, que la meme correction repare ==");
    execute("menu par chiffre",  "  create menu \"M\"\n"
                                 "  put the name of menu 1");
    execute("menu par variable", "  put 1 into i\n"
                                 "  put the name of menu i");
    execute("menu par calcul",   "  put the name of menu (2 - 1)");
    execute("there is a menu",   "  put there is a menu \"M\"");

    hc_free(st);
    return 0;
}
