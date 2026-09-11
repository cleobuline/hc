#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("      = %s\n",t); else if(k==HC_ERR)printf("      [ERR] %s\n",t);}
static Object *b;
static void essai(const char *ligne){
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\n  put the selectedChunk\nend t\n",ligne);
  hc_set_script(b,s); hc_v3_bilan_remise_a_zero(); hc_send(b,"t");
  printf("%-48s ",ligne); fflush(stdout); hc_v3_bilan();}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  b=hc_new_button(c,"B"); Object *f=hc_new_field(c,"data");
  hc_set_field_text(f,"alpha beta gamma");
  hc_set_current_card(c);
  essai("select char 2 of card field \"data\"");
  essai("select first char of card field \"data\"");
  essai("select last char of card field \"data\"");
  essai("select second word of card field \"data\"");
  essai("select middle word of card field \"data\"");
  essai("select last word of card field \"data\"");
  essai("select last line of card field \"data\"");
  hc_free(st);return 0;}
