#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");
  hc_set_current_card(c);
  hc_set_script(b,
   "on t\n"
   "  put \"a,b,c\" into x\n"
   "  put \"X\" into any item of x\n"
   "  put \"ecriture dans « any item » -> [\" & x & \"]\"\n"
   "  put \"a,b,c\" into y\n"
   "  put \"lecture de « any item »    -> [\" & any item of y & \"]\"\n"
   "end t\n");
  hc_send(b,"t"); hc_free(st);return 0;}
