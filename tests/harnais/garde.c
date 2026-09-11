/* Un « end » manquant ne doit PAS faire exécuter n'importe quoi. */
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
   "-- une banniere, comme les piles d'epoque\n"
   "on premier\n  put \"premier : ok\"\nend premier\n\n"
   "on debordant\n  put \"debordant : ok\"\n\n"   /* end manquant */
   "on troisieme\n  put \"troisieme : AVALE, ne doit pas s'afficher seul\"\n"
   "end troisieme\n");
  printf("── premier (sain, doit marcher)\n");   hc_send(b,"premier");
  printf("── debordant (fautif, ancien interprete)\n"); hc_send(b,"debordant");
  printf("── troisieme (avale, ancien interprete)\n");  hc_send(b,"troisieme");
  hc_free(st);return 0;}
