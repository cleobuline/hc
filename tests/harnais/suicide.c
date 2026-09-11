/* « delete me » dans le propre gestionnaire de l'objet. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;printf("%s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  Object *b=hc_new_button(c,"B");hc_set_current_card(c);
  hc_set_script(b,"on mouseUp\n  delete me\nend mouseUp\n");
  printf("avant : %d objets sur la carte\n", c->nparts);
  hc_send(b,"mouseUp");
  printf("après : %d objets sur la carte\n", c->nparts);
  hc_free(st);return 0;}
