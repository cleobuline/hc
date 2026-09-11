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
   "on mouseUp\n"
   "  put \"depart      : \" & the lockScreen & \" / \" & the lockMessages\n"
   "  lock screen\n  lock messages\n"
   "  put \"apres lock  : \" & the lockScreen & \" / \" & the lockMessages\n"
   "  unlock screen\n  unlock messages\n"
   "  put \"apres unlock: \" & the lockScreen & \" / \" & the lockMessages\n"
   "  set the lockScreen to true\n"
   "  put \"par set     : \" & the lockScreen\n"
   "  if the lockScreen then put \"   et le test marche\"\n"
   "  set the lockScreen to false\n"
   "  debug bilan\n"
   "end mouseUp\n");
  hc_send(b,"mouseUp");hc_free(st);return 0;}
