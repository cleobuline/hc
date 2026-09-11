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
   "  put \"a,b,c\" into x\n  delete item 2 of x\n  put \"delete item 2  -> [\" & x & \"]\"\n"
   "  put \"a,b,c\" into y\n  put empty into item 2 of y\n  put \"put empty     -> [\" & y & \"]\"\n"
   "  put \"un deux trois\" into w\n  delete word 2 of w\n  put \"delete word 2  -> [\" & w & \"]\"\n"
   "  put \"abcdef\" into z\n  delete char 2 to 4 of z\n  put \"delete char 2-4-> [\" & z & \"]\"\n"
   "  put \"L1\" & return & \"L2\" & return & \"L3\" into l\n"
   "  delete line 2 of l\n  put \"delete line 2  -> [\" & l & \"]\"\n"
   "  debug bilan\n"
   "end t\n");
  hc_v3_bilan_remise_a_zero(); hc_send(b,"t");
  hc_free(st);return 0;}
