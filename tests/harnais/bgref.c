#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
static Object *b;
static void essai(const char *titre,const char *corps){
  char s[512]; snprintf(s,sizeof s,"on t\n%s\nend t\n",corps);
  hc_set_script(b,s); hc_v3_bilan_remise_a_zero(); printf("── %-34s ",titre);
  fflush(stdout); hc_send(b,"t"); hc_v3_bilan(); printf("\n");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  hc_new_field(c,"surCarte"); hc_new_field(bg,"surFond");
  hc_new_button(bg,"btnFond");
  b=hc_new_button(c,"B"); hc_set_current_card(c);
  essai("carte : put into card field",      "  put \"x\" into card field \"surCarte\"");
  essai("carte : lire card field",          "  put card field \"surCarte\" into z");
  essai("carte : hide card field",          "  hide card field \"surCarte\"");
  essai("FOND : put into bg field",         "  put \"x\" into bg field \"surFond\"");
  essai("FOND : lire bg field",             "  put bg field \"surFond\" into z");
  essai("FOND : hide bg button",            "  hide bg btn \"btnFond\"");
  essai("FOND : the rect of bg button",     "  put the rect of bg btn \"btnFond\" into z");
  essai("FOND : « background » en toutes lettres","  put bg field \"surFond\" into z");
  hc_free(st);return 0;}
