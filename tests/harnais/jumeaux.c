#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c1=hc_new_card(st,bg,"Une");
 Object *bg2=hc_new_background(st,"F2");Object *c2=hc_new_card(st,bg2,"Deux");
 Object *b=hc_new_button(c1,"Bouton");Object *f=hc_new_field(c1,"Champ");
 hc_set_current_card(c1);
 hc_set_script(st,
   "on deleteButton\n  put \"adieu bouton \" & the name of the target\nend deleteButton\n"
   "on deleteField\n  put \"adieu champ \" & the name of the target\nend deleteField\n"
   "on deleteCard\n  put \"adieu carte \" & the name of the target\nend deleteCard\n"
   "on deleteBackground\n  put \"adieu fond \" & the name of the target\nend deleteBackground\n");
 printf("--- supprimer un bouton ---\n");   hc_delete_part(b);
 printf("--- supprimer un champ ---\n");    hc_delete_part(f);
 printf("--- supprimer la carte Deux (dernière de son fond) ---\n");
 hc_delete_card(c2);
 printf("cartes restantes = %d\n", 1);
 hc_free(st);return 0;}
