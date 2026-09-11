#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("      = %s\n",t); else if(k==HC_ERR)printf("      [ERR] %s\n",t);}
static Object *b;
static void essai(const char *pose,const char *lit){
  char s[600]; snprintf(s,sizeof s,"on t\n  %s\n  put %s\nend t\n",pose,lit);
  hc_set_script(b,s); hc_v3_bilan_remise_a_zero(); hc_send(b,"t");
  printf("%-52s ",pose); fflush(stdout); hc_v3_bilan();}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  b=hc_new_button(c,"B"); Object *f=hc_new_field(c,"data");
  hc_set_field_text(f,"alpha beta");
  hc_new_button(bg,"L");
  hc_set_current_card(c);
  essai("put \"10,10,120,60\" into r\n  set the rect of bg btn \"L\" to r",
        "the rect of bg btn \"L\"");
  essai("set the visible of card field \"data\" to false",
        "the visible of card field \"data\"");
  essai("set the hilite of me to true","the hilite of me");
  essai("set the textSize of card field \"data\" to 18",
        "the textSize of card field \"data\"");
  essai("set the textStyle of card field \"data\" to bold,italic",
        "the textStyle of card field \"data\"");
  essai("set the name of bg btn \"L\" to \"Legende\"",
        "the short name of bg btn \"Legende\"");
  hc_free(st);return 0;}
