/* « PopUpMenu » : L'XFCN D'ÉPOQUE, IMITÉ.
 *
 * Rapporté le 2 octobre dans « Minkowski Stack 1 » (1992) :
 *
 *     put PopUpMenu(list,,tp,lp) into it
 *     get item it of list
 *
 * PopUpMenu est un XFCN d'Andrew Gilmartin, dont le code 68000 vivait dans
 * la ressource de la pile. HC l'imite — voir v3_popupmenu dans hc_core.c —
 * au rang où HyperCard l'aurait trouvé : après la chaîne des messages, et
 * seulement si l'hôte sait montrer un menu.
 *
 * Le faux hôte imprime ce qu'il reçoit et « choisit » le rang qu'on lui a
 * dit de choisir. Ce harnais tient :
 *
 *   1. la ligne de la pile : la liste découpée à la virgule, « (- » en trait,
 *      le point de l'écran transmis, le rang rendu, l'article retrouvé ;
 *   2. l'article coché, et l'absence de point ;
 *   3. rien de choisi : 0, et « item 0 of list » vide ;
 *   4. une pile qui définit SA fonction PopUpMenu garde la main ;
 *   5. sans hôte qui sache montrer un menu, ou avec trop d'arguments, la
 *      fonction reste inconnue, comme avant.
 *
 * NON MESURÉ, faute de l'XFCN sous la main : ce qu'il rend quand rien n'est
 * choisi, et ce qu'il fait des autres métacaractères du Menu Manager. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static int g_choix = 0;

static int menu(const char *articles, int coche, int haut, int gauche)
{
    printf("      [menu] coché %d, ", coche);
    if (haut == HC_PAS_DE_POINT) printf("à la souris\n");
    else printf("au point %d,%d de l'écran\n", gauche, haut);
    int i = 1;
    for (const char *p = articles; ; i++) {
        const char *f = strchr(p, '\n');
        printf("      [menu]   %d. %.*s\n", i, (int)(f ? f - p : (long)strlen(p)), p);
        if (!f) break;
        p = f + 1;
    }
    printf("      [menu] choisi : %d\n", g_choix);
    return g_choix;
}

static Object *b;

static void joue(const char *corps, int choix)
{
    char s[2048];
    g_choix = choix;
    printf("   %s\n", corps);
    snprintf(s, sizeof s, "on mouseUp\n  %s\nend mouseUp\n", corps);
    hc_set_script(b, s);
    hc_send(b, "mouseUp");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    h.popup_menu = menu;
    hc_set_host(&h);
    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_register_stack(st);
    hc_set_current_card(c);
    b = hc_new_button(c, "Data");
    b->x = 310; b->y = 1; b->w = 37; b->h = 15;

    const char *MINKOWSKI =
        "put (bottom of target + top of card window + 1) into tp\n"
        "  put (left of target + left of card window) into lp\n"
        "  put \"Axes,Velocity,(-,Click Points,Grid,Bisector,Line,(-,"
        "Clear Points,Clear Polygons\" into list\n"
        "  put PopUpMenu(list,,tp,lp) into it\n"
        "  get item it of list\n"
        "  put \"rendu : [\" & it & \"]\"";

    puts("== 1. la ligne de Minkowski Stack 1 ==");
    joue(MINKOWSKI, 4);
    joue(MINKOWSKI, 10);

    puts("\n== 2. un article coché, et pas de point ==");
    joue("put PopUpMenu(\"Un,Deux,Trois\",2)", 3);
    joue("put PopUpMenu(\"Un,(Deux,Trois\")", 1);

    puts("\n== 3. rien de choisi ==");
    joue("put \"Un,Deux\" into l\n  put PopUpMenu(l) into r\n"
         "  put r & \" [\" & item r of l & \"]\"", 0);

    puts("\n== 4. la fonction de la pile garde la main ==");
    hc_set_script(st, "function PopUpMenu l\n  return \"celle de la pile\"\nend PopUpMenu\n");
    joue("put PopUpMenu(\"Un,Deux\")", 1);
    hc_set_script(st, "");

    puts("\n== 5. ce qui reste inconnu ==");
    joue("put PopUpMenu(\"Un\",0,10,10,99)", 1);
    h.popup_menu = NULL;
    hc_set_host(&h);
    joue("put PopUpMenu(\"Un,Deux\")", 1);

    hc_free(st);
    return 0;
}
