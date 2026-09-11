#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("Principale");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");
  hc_set_current_card(c);
  /* une pile d'outils réellement enregistrée */
  Object *o=hc_new_stack("Outils");Object *ob=hc_new_background(o,"F");
  hc_new_card(o,ob,"x"); hc_register_stack(o);
  hc_set_script(b,"on t\n  start using stack \"Outils\"\n"
                  "  stop using stack \"Outils\"\nend t\n");
  hc_v3_bilan_remise_a_zero(); hc_send(b,"t"); hc_v3_bilan();
  hc_free(st);return 0;}
