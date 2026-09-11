#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static void ma_barre(void){}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.menus_changed=ma_barre;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");hc_set_current_card(c);
 hc_set_script(b,
  "on mouseUp\n"
  "  create menu \"3DEquations\"\n"
  "  put \"Curves\" & return & \"Surfaces\" & return & \"-\" & return & \"Help\" into menu \"3DEquations\" with menuMsg \"goCurves\" & return & \"goSurf\" & return & return & \"goHelp\"\n"
  "  put \"articles = \" & the number of menuItems of menu \"3DEquations\"\n"
  "  put \"coche avant  = \" & the checkMark of menuItem 2 of menu \"3DEquations\"\n"
  "  set checkMark of menuItem 2 of menu \"3DEquations\" to true\n"
  "  put \"coche apres  = \" & the checkMark of menuItem 2 of menu \"3DEquations\"\n"
  "  put \"nom art. 2   = \" & the name of menuItem 2 of menu \"3DEquations\"\n"
  "  put \"message art.2= \" & the menuMessage of menuItem 2 of menu \"3DEquations\"\n"
  "  put \"actif art. 4 = \" & the enabled of menuItem 4 of menu \"3DEquations\"\n"
  "  disable menuItem 4 of menu \"3DEquations\"\n"
  "  put \"actif art. 4 = \" & the enabled of menuItem 4 of menu \"3DEquations\"\n"
  "  put \"nom du menu  = \" & the name of menu 1\n"
  "  set the name of menuItem 1 of menu \"3DEquations\" to \"Courbes\"\n"
  "  put \"renomme      = \" & the name of menuItem 1 of menu \"3DEquations\"\n"
  "  set checkMark of menuItem 2 of menu \"3DEquations\" to false\n"
  "  put \"decoche      = \" & the checkMark of menuItem 2 of menu \"3DEquations\"\n"
  "end mouseUp\n");
 hc_send(b,"mouseUp");hc_free(st);return 0;}
