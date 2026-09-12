/* La boîte de message, portée : mêmes résultats, sans l'ancien interprète. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une"); hc_new_card(st,bg,"Deux");
  Object *f=hc_new_field(c,"F1"); (void)f;
  hc_set_current_card(c);
  hc_set_script(st,"function double n\n  return n*2\nend double\n");
  hc_v3_bilan_remise_a_zero();
  hc_do("put 2+2");
  hc_do("put 1 into x");
  hc_do("put \"x survit : \" & x");
  hc_do("put \"le double : \" & double(21)");
  hc_do("put \"abc\" into card field \"F1\"");
  hc_do("put \"le champ : \" & card field \"F1\"");
  hc_do("go next card");
  hc_do("put \"carte : \" & the short name of this card");
  hc_do("go prev card");
  hc_do("repeat with i = 1 to 3\nput \"tour \" & i\nend repeat");
  hc_do("set the itemDelimiter to \";\"");
  hc_do("put \"item 2 : \" & item 2 of \"a;b\"");
  hc_do("set the itemDelimiter to \",\"");
  hc_do("beep");
  hc_v3_bilan();
  hc_free(st);return 0;}
