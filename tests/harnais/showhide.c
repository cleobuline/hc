#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
static Object *b;
static void essai(const char *ligne){
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",ligne);
  hc_set_script(b,s); hc_v3_bilan_remise_a_zero(); hc_send(b,"t");
  printf("%-38s -> ",ligne); fflush(stdout); hc_v3_bilan();}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  b=hc_new_button(c,"B"); hc_new_field(c,"F1"); hc_new_field(bg,"FondF");
  hc_new_button(bg,"FondB"); hc_set_current_card(c);
  essai("show card field \"F1\"");
  essai("show card field 1");
  essai("show bg field \"FondF\"");
  essai("show bg btn \"FondB\"");
  essai("hide card field \"F1\"");
  essai("hide card field 1");
  essai("show card button \"B\"");
  essai("show me");
  hc_free(st);return 0;}
