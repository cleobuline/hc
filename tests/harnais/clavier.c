#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");
 hc_set_current_card(c);
 hc_set_script(st,
   "on arrowKey d\n  put \"flèche : \" & d\n  if d is \"right\" then pass arrowKey\nend arrowKey\n"
   "on keyDown k\n  put \"touche : \" & k\nend keyDown\n"
   "on functionKey n\n  put \"F\" & n\nend functionKey\n"
   "on tabKey\n  put \"tab\"\nend tabKey\n");
 printf("arrowKey left   pris=%d\n", hc_send_arg(hc_current_card(),"arrowKey","left"));
 printf("arrowKey right  pris=%d  (le gestionnaire fait « pass »)\n", hc_send_arg(hc_current_card(),"arrowKey","right"));
 printf("keyDown a       pris=%d\n", hc_send_arg(hc_current_card(),"keyDown","a"));
 printf("functionKey 3   pris=%d\n", hc_send_arg(hc_current_card(),"functionKey","3"));
 printf("tabKey          pris=%d\n", hc_send_arg(hc_current_card(),"tabKey",NULL));
 printf("returnKey       pris=%d  (aucun gestionnaire)\n", hc_send_arg(hc_current_card(),"returnKey",NULL));
 hc_free(st);return 0;}
