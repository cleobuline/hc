#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_ERR)printf("      [ERR] %s\n",t);}
static Object *b, *f1, *f2;
static void essai(const char *ligne){
  hc_set_field_text(f1,"un,deux"); hc_set_field_text(f2,"trois,quatre");
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",ligne);
  hc_set_script(b,s); hc_v3_bilan_remise_a_zero(); hc_send(b,"t");
  printf("%-44s ",ligne); fflush(stdout); hc_v3_bilan();
  printf("      « data » -> [%s]   « data 2 » -> [%s]\n",
         f1->contents?f1->contents:"", f2->contents?f2->contents:"");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  b=hc_new_button(c,"B"); f1=hc_new_field(c,"data"); f2=hc_new_field(c,"data 2");
  hc_set_current_card(c);
  essai("put \"X\" into field \"data\"");
  essai("put \"X\" into field \"data 2\"");
  essai("put \">\" before field \"data 2\"");
  essai("put \"X\" into item 2 of field \"data 2\"");
  essai("put field \"data 2\" into z");
  hc_free(st);return 0;}
