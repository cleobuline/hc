#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void mon_menu(const char *i){printf("   [HÔTE] %s\n",i);}
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k!=HC_TRACE)printf("   [%d] %s\n",(int)k,t);}
static void nom(const char *q){
  Object *c=hc_current_card();
  printf("%-22s → %s\n", q, c&&c->name?c->name:"(?)");
}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.do_menu=mon_menu;h.line=ma_ligne;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *a=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");hc_new_card(st,bg,"Trois");
 hc_set_current_card(a);
 /* exactement ce que fait un clic de menu : hors de tout gestionnaire */
 printf("départ                 → %s\n", hc_current_card()->name);
 hc_do_menu("Next");  nom("clic Next");
 hc_do_menu("Next");  nom("clic Next");
 hc_do_menu("Last");  nom("clic Last");
 hc_do_menu("Prev");  nom("clic Prev");
 hc_do_menu("First"); nom("clic First");
 hc_free(st);return 0;}
