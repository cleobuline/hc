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
   "on startUp\n  noter \"startUp\"\nend startUp\n"
   "on quit\n  noter \"quit\"\nend quit\n"
   "on noter quoi\n  global gVus\n"
   "  if gVus is empty then put quoi into gVus\n"
   "  else put gVus & \",\" & quoi into gVus\n"
   "end noter\n");
  hc_set_script(b,
   "on mouseUp\n"
   "  global gVus\n  put empty into gVus\n"
   "  send \"startUp\" to this card\n"
   "  put \"apres 1 : [\" & gVus & \"]\"\n"
   "  send \"quit\" to this card\n"
   "  put \"apres 2 : [\" & gVus & \"]\"\n"
   "  put \"items : \" & the number of items of gVus\n"
   "end mouseUp\n");
  hc_send(b,"mouseUp");hc_free(st);return 0;}
