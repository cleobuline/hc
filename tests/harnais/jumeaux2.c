#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c=hc_new_card(st,bg,"Une");
 Object *b=hc_new_button(c,"Suicidaire");
 hc_set_current_card(c);
 /* le gestionnaire tente de supprimer l'objet qu'on est déjà en train de
    supprimer : sans garde, double libération */
 hc_set_script(b,"on deleteButton\n  put \"le gestionnaire tente une seconde suppression\"\n  delete me\nend deleteButton\n");
 printf("boutons avant  = %d\n", c->nparts);
 hc_delete_part(b);
 printf("boutons après  = %d  (attendu 0, et surtout pas de double free)\n", c->nparts);
 hc_free(st);
 printf("pile libérée sans incident\n");
 return 0;}
