#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static int g_changes = 0;
static void ma_barre(void){ g_changes++; }
static void montre(void)
{
    printf("   ── barre : %d menu(s), %d notification(s) ──\n", hc_menu_nombre(), g_changes);
    for (int i = 0; i < hc_menu_nombre(); i++) {
        printf("      menu %d « %s »%s\n", i+1, hc_menu_nom(i),
               hc_menu_est_actif(i) ? "" : "  (désactivé)");
        for (int j = 0; j < hc_menu_nb_articles(i); j++)
            printf("         %d. %-14s %s\n", j+1, hc_menu_article(i,j),
                   hc_menu_article_actif(i,j) ? "" : "(désactivé)");
    }
}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.menus_changed=ma_barre;hc_set_host(&h);
 Object *st=hc_new_stack("surfaces");Object *bg=hc_new_background(st,"Surfaces");
 Object *c=hc_new_card(st,bg,"Une");hc_set_current_card(c);

 /* le script de la pile, mot pour mot */
 hc_set_script(st,
  "on createMenu\n"
  "  create menu \"3DEquations\"\n"
  "  put menuItems() into menu \"3DEquations\" with menuMsg menuMsgs()\n"
  "end createMenu\n"
  "\n"
  "function menuItems\n"
  "  put return into rt\n"
  "  return \"Curves\"&rt&\"Surfaces\"&rt&\"-\"&rt&\"Export…\"&rt&\"Choose color…\"&rt&\"-\"&rt&\"Help\"\n"
  "end menuItems\n"
  "\n"
  "function menuMsgs\n"
  "  put return into rt\n"
  "  return \"goCurves\"&rt&\"goSurf\"&rt&rt&\"ExportIt\"&rt&\"chooseColr\"&rt&rt&\"goHelp\"\n"
  "end menuMsgs\n"
  "\n"
  "on openStack\n"
  "  if there is no menu \"3DEquations\" then createMenu\n"
  "  disable menuItem 4 of menu \"3DEquations\"\n"
  "end openStack\n"
  "\n"
  "on closeStack\n"
  "  if there is a menu \"3DEquations\" then delete menu \"3DEquations\"\n"
  "end closeStack\n"
  "\n"
  "on goCurves\n  put \"→ goCurves !\"\nend goCurves\n"
  "on goHelp\n  put \"→ goHelp !\"\nend goHelp\n"
  "on ExportIt\n  put \"→ ExportIt !\"\nend ExportIt\n");

 printf("=== openStack ===\n");            hc_send(c,"openStack"); montre();
 printf("=== openStack une SECONDE fois (ne doit rien recréer) ===\n");
 hc_send(c,"openStack"); montre();
 printf("=== on choisit « Curves » (article 1) ===\n");   hc_menu_choisi(0,0);
 printf("=== on choisit le séparateur (article 3) ===\n"); hc_menu_choisi(0,2);
 printf("=== on choisit « Export… », désactivé (article 4) ===\n"); hc_menu_choisi(0,3);
 printf("=== on choisit « Help » (article 7) ===\n");     hc_menu_choisi(0,6);
 printf("=== closeStack ===\n");           hc_send(c,"closeStack"); montre();
 hc_free(st);return 0;}
