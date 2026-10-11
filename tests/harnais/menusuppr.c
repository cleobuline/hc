/* menusuppr — « delete menuItem … FROM menu … », et ce qui suit l'article.
 *
 * La carte « delete » de HyperTalk Reference écrit les deux formes :
 *
 *     delete menuItem "New" from menu "File"
 *     delete menuItem 2 of menu 2
 *
 * HC refusait la première à la lecture : l'expression s'arrêtait devant
 * « from ». Relevé le 11 octobre ; NON MESURÉ dans HyperCard au-delà de ce
 * que la référence écrit.
 *
 * Et en l'écrivant, un défaut plus ancien : supprimer un article décalait
 * les textes, les messages et l'état actif, mais ni la COCHE ni le RACCOURCI.
 * Ils restaient à leur rang, donc sur l'article suivant. Section 3. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      = %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static void fais(const char *l) { printf("   %s\n", l); hc_do(l); }

/* Les articles sur une ligne : « put menu » rend chacun suivi d'un retour. */
static void articles(void)
{
    hc_do("put empty into r");
    hc_do("repeat with i = 1 to the number of menuItems of menu \"M\"\n"
          "  put menuItem i of menu \"M\" & \" \" after r\n"
          "end repeat");
    hc_do("put \"[\" & r & \"]\"");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("S"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    hc_set_current_card(hc_new_card(st, bg, "C"));

    hc_do("create menu \"M\"");
    hc_do("put \"Un,Deux,Trois,Quatre,Cinq,Six\" into menu \"M\"");

    puts("== 1. la forme « from » ==");
    fais("delete menuItem \"Deux\" from menu \"M\"");
    articles();
    fais("delete menuItem 1 from menu \"M\"");
    articles();
    fais("delete last menuItem from menu \"M\"");
    articles();

    puts("\n== 2. la forme « of », témoin ==");
    fais("delete menuItem 2 of menu \"M\"");
    articles();

    puts("\n== 3. la coche et le raccourci suivent leur article ==");
    fais("put \"Un,Deux,Trois,Quatre\" into menu \"M\"");
    fais("set the checkMark of menuItem \"Trois\" of menu \"M\" to true");
    fais("set the cmdChar of menuItem \"Quatre\" of menu \"M\" to \"Q\"");
    fais("delete menuItem \"Deux\" of menu \"M\"");
    articles();
    fais("put the checkMark of menuItem \"Trois\" of menu \"M\"");
    fais("put the cmdChar of menuItem \"Quatre\" of menu \"M\"");
    fais("put the checkMark of menuItem \"Quatre\" of menu \"M\"");
    fais("delete menuItem \"Quatre\" from menu \"M\"");
    fais("put \"Cinq\" after menu \"M\"");
    fais("put \"[\" & the cmdChar of menuItem \"Cinq\" of menu \"M\" & \"]\"");

    puts("\n== 4. ce qui manque ==");
    fais("delete menuItem \"Absent\" from menu \"M\"");
    fais("put the result");

    puts("\n== 5. témoin : un conteneur ==");
    fais("put \"a,b,c\" into x");
    fais("delete item 2 of x");
    fais("put x");

    hc_do("delete menu \"M\"");
    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
