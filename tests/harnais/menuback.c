#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c1=hc_new_card(st,bg,"Une");
  hc_new_card(st,bg,"Deux"); hc_new_card(st,bg,"Trois");
  Object *b=hc_new_button(c1,"B"); hc_set_current_card(c1);
  hc_set_script(b,"on t\n  go card \"Deux\"\n  go card \"Trois\"\n"
                  "  doMenu \"Back\"\n  put the name of this card\n"
                  "  doMenu \"Back\"\n  put the name of this card\n"
                  "  doMenu \"First\"\n  put the name of this card\nend t\n");
  hc_send(b,"t");
  printf("--- hc_go_back depuis l'interface : %d\n", hc_go_back());
  printf("--- carte : %d recents\n", hc_recent_count());
  hc_unregister_stack(st);hc_free(st);return 0;}
