#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_ERR)printf("      [ERR] %s\n",t);}
static Object *b, *f;
static void essai(const char *ligne){
  hc_set_field_text(f,"alpha,beta,gamma");
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",ligne);
  hc_set_script(b,s); hc_v3_bilan_remise_a_zero(); hc_send(b,"t");
  printf("%-46s ", ligne);
  /* recours ? on relit le bilan en le capturant */
  fflush(stdout);
  hc_v3_bilan();
  printf("      champ -> [%s]\n", f->contents ? f->contents : "");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  b=hc_new_button(c,"B"); f=hc_new_field(c,"data");
  hc_set_current_card(c);
  essai("put \"X\" into field \"data\"");
  essai("put \"X\" into item 2 of field \"data\"");
  essai("put \"X\" into line 1 of field \"data\"");
  essai("put \"X\" into char 1 of field \"data\"");
  essai("put \">\" before field \"data\"");
  essai("put \"!\" after field \"data\"");
  essai("put \"X\" after item 1 of field \"data\"");
  hc_free(st);return 0;}
