/* Une variable posée dans la boîte de message survit-elle à la ligne
 * suivante ? Il faut le savoir AVANT de porter hc_do. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   [msg] %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");hc_set_current_card(c);
  hc_do("put 1 into x");
  hc_do("put \"x vaut : \" & x");
  hc_do("global g");
  hc_do("put 7 into g");
  hc_do("put \"g vaut : \" & g");
  hc_free(st);return 0;}
