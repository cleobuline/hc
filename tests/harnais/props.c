#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("      = %s\n",t); else if(k==HC_ERR)printf("      [ERR] %s\n",t);}
static Object *b;
static void essai(const char *expr){
  char s[512]; snprintf(s,sizeof s,"on t\n  put %s\nend t\n",expr);
  hc_set_script(b,s); hc_v3_bilan_remise_a_zero(); hc_send(b,"t");
  printf("%-46s ",expr); fflush(stdout); hc_v3_bilan();}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  b=hc_new_button(c,"B"); Object *f=hc_new_field(c,"data");
  hc_set_field_text(f,"une ligne");
  hc_set_current_card(c);
  essai("the hilite of me");
  essai("the width of me");
  essai("the number of me");
  essai("the short name of me");
  essai("the textStyle of card field \"data\"");
  essai("the name of card field \"data\"");
  essai("the rect of me");
  essai("the visible of me");
  hc_free(st);return 0;}
