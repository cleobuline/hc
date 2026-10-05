/* aidemenus — CE QUE LA PILE D'AIDE D'APPLE DEMANDE, ET QUE HC NE SAVAIT PAS.
 *
 * « HyperCard Help », convertie par l'utilisatrice le 4 octobre, s'arrêtait
 * dès l'ouverture, DANS HC (l'application) :
 *
 *     propriété inconnue (v3, ligne 91 de stack "HyperCard Help".checkCantModify)
 *     ne sait pas faire : show menuBar
 *     menu introuvable (× 3)
 *     ne sait pas faire : push recent card
 *     ne sait pas faire : pop card into theCard
 *     ne sait pas faire : show groups
 *
 * Les gestionnaires sont recopiés ici dans leur FORME — pas dans leur texte,
 * la pile d'Apple n'entre pas dans le dépôt. Les trois « menu introuvable »
 * venaient de addHelpMenu, arrêté par « show menuBar » avant d'avoir créé
 * son menu.
 *
 * Ce que l'hôte fait de « menuBar » et « showGroups » se lit ici dans les
 * réglages qu'il reçoit ; le rendu à l'écran est dans HCview.m et HCtext.m,
 * non exécuté ici. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

/* Les réglages que l'hôte reçoit : c'est par eux que « show menuBar » et
 * « show groups » arrivent à l'interface. */
static void reglage(const char *nom, const char *val)
{
    if (!strcmp(nom, "menuBar") || !strcmp(nom, "showGroups"))
        printf("   -> hôte : %s = %s\n", nom, val);
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne; h.global_set = reglage;
    hc_set_host(&h);

    Object *st = hc_new_stack("Aide");
    Object *bg = hc_new_background(st, "Fond");
    Object *c1 = hc_new_card(st, bg, "Une");
    hc_new_card(st, bg, "Deux");
    hc_new_card(st, bg, "Trois");
    hc_register_stack(st);
    hc_set_current_card(c1);
    Object *b = hc_new_button(c1, "B");

    hc_set_script(st,
        "on addHelpMenu\n"
        "  global gHMnu\n"
        "  show menuBar\n"
        "  create menu gHMnu\n"
        "  put \"Overview of Help,-,HyperCard Help,HyperTalk Reference\" into menu gHMnu with menuMessages \"goOverview,empty,goHCHelp,goTalk\"\n"
        "  set cmdChar of menuItem \"HyperCard Help\" of menu gHMnu to \"?\"\n"
        "end addHelpMenu\n"
        "on toggleActiveText trueOrFalse\n"
        "  if trueOrFalse is true then\n"
        "    show groups\n"
        "  else\n"
        "    hide groups\n"
        "  end if\n"
        "end toggleActiveText\n"
        "function whereICameFrom\n"
        "  push recent card\n"
        "  pop card into theCard\n"
        "  return theCard\n"
        "end whereICameFrom\n"
        "on checkCantModify\n"
        "  if the cantModify of this stack is TRUE\n"
        "  then set the cantModify of this stack to false\n"
        "  else set the cantModify of this stack to false\n"
        "end checkCantModify\n");

    puts("== 1. addHelpMenu : show menuBar, puis le menu et son raccourci ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  global gHMnu\n"
        "  put \"Reference\" into gHMnu\n"
        "  addHelpMenu\n"
        "  set the checkMark of menuItem 3 of menu gHMnu to true\n"
        "  put \"menuMsg 3 : \" & the menuMsg of menuItem 3 of menu gHMnu\n"
        "  put \"cmdChar 3 : \" & the cmdChar of menuItem 3 of menu gHMnu\n"
        "  put \"cmdChar 4 : [\" & the cmdChar of menuItem 4 of menu gHMnu & \"]\"\n"
        "  put \"there is : \" & (there is a menu gHMnu)\n"
        "  put \"HyperTalk Reference\" into art\n"
        "  put \"article par variable : \" & the menuMsg of menuItem art of menu gHMnu\n"
        "  put \"rang par variable : \" & the name of menuItem (1 + 2) of menu gHMnu\n"
        "  hide menuBar\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");
    {
        int i = -1;
        for (int k = 0; k < hc_menu_nombre(); k++)
            if (!strcmp(hc_menu_nom(k), "Reference")) i = k;
        printf("   l'hôte lit : article 3 « %s », touche « %c »\n",
               i >= 0 ? hc_menu_article(i, 2) : "?",
               i >= 0 && hc_menu_article_touche(i, 2) ? hc_menu_article_touche(i, 2) : '-');
    }

    puts("\n== 2. toggleActiveText : show groups, hide groups ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  toggleActiveText true\n"
        "  toggleActiveText false\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== 3. whereICameFrom : d'où l'on vient, sans y aller ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  go card 2\n"
        "  go card 3\n"
        "  put \"vient de : \" & whereICameFrom()\n"
        "  put \"est sur  : carte \" & the number of this card\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== 4. checkCantModify ==");
    hc_set_current_card(c1);
    hc_set_script(b,
        "on mouseUp\n"
        "  set the cantModify of this stack to true\n"
        "  checkCantModify\n"
        "  put \"cantModify : \" & the cantModify of this stack\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== 5. select line 0 / line empty : éteindre une liste ==");
    /* L'idiome d'Apple : exitDemo (« HyperTalk Reference ») finit par
     * « select line 0 of me » ; « HyperCard Help » écrit six fois l'une ou
     * l'autre forme. HC exigeait un rang. */
    {
        Object *liste = hc_new_field(c1, "Index");
        hc_set_field_text(liste, "un\ndeux\ntrois");
        liste->locktext = 1; liste->auto_select = 1;
        hc_set_script(b,
            "on mouseUp\n"
            "  put \"Index\" into fieldName\n"
            "  select line 2 of card field fieldName\n"
            "  put \"allumée : \" & the selectedLine of card field fieldName\n"
            "  select line empty of card field fieldName\n"
            "  put \"line empty : [\" & the selectedLine of card field fieldName & \"]\"\n"
            "  select line 3 of card field \"Index\"\n"
            "  select line 0 of card field \"Index\"\n"
            "  put \"line 0 : [\" & the selectedLine of card field \"Index\" & \"]\"\n"
            "  select line 9 of card field \"Index\"\n"
            "end mouseUp\n");
        hc_send(b, "mouseUp");
    }

    hc_free(st);
    return 0;
}
