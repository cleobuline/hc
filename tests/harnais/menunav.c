#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void mon_menu(const char *item){printf("   [HÔTE] %s\n",item);}
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.do_menu=mon_menu;h.line=ma_ligne;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *a=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");hc_new_card(st,bg,"Trois");
 Object *b=hc_new_button(a,"B");hc_set_current_card(a);
 hc_set_script(b,"on mouseUp\n"
  "doMenu \"Next\"\n     put the name of this card\n"
  "doMenu \"Last\"\n     put the name of this card\n"
  "doMenu \"Prev\"\n     put the name of this card\n"
  "doMenu \"Previous\"\n put the name of this card\n"
  "doMenu \"First\"\n    put the name of this card\n"
  "doMenu \"Find...\"\n"
  "doMenu \"Clear Picture\"\n"
  "end mouseUp\n");
 hc_send(b,"mouseUp");hc_free(st);return 0;}
