/* Une erreur dans un gestionnaire APPELÉ arrête-t-elle l'appelant ? */
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
  hc_set_script(st,
   "on planteur\n  put \"  dans planteur, avant\"\n  put the zorglub\n"
   "  put \"  dans planteur, APRES — ne doit pas paraitre\"\nend planteur\n");
  hc_set_script(b,
   "on mouseUp\n"
   "  put \"1. avant l'appel\"\n"
   "  planteur\n"
   "  put \"2. apres l'appel — l'appelant survit-il ?\"\n"
   "  global g\n  put \"vu\" into g\n"
   "  put \"3. et une globale posee ensuite : \" & g\n"
   "end mouseUp\n");
  hc_send(b,"mouseUp");hc_free(st);return 0;}
