#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;printf("%s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c1=hc_new_card(st,bg,"Une");Object *c2=hc_new_card(st,bg,"Deux");
  Object *b=hc_new_button(c1,"B"); hc_set_current_card(c1);
  hc_set_script(b,"on mouseUp\n  delete this card\nend mouseUp\n");
  printf("avant : %d cartes\n",hc_card_count(st));
  hc_send(b,"mouseUp");
  printf("après : %d cartes\n",hc_card_count(st));
  hc_free(st);return 0;}
