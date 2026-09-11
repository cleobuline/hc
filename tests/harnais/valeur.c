/* « the value of » — le script de torture, guillemet réparé. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_ERR)printf("[ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  Object *f=hc_new_field(c,"data2");
  Object *b=hc_new_button(c,"torture 2");hc_set_current_card(c);
  hc_set_script(b,
  "on essai\n"
  "  put \"2 + 3 * 4\" into e\n"
  "  put \"arith  : \" & the value of e into line 1 of field \"data2\"\n"
  "  put \"the id of this card\" into r\n"
  "  put \"objet  : \" & the id of this card into line 2 of field \"data2\"\n"
  "  put \"bonjour\" into t\n"
  "  put \"texte  : \" & the value of t into line 3 of field \"data2\"\n"
  "  put \"litt   : \" & the value of 7 * 6 into line 4 of field \"data2\"\n"
  "  put 5 into n\n"
  "  put the value of \"n + 1\" into e2\n"
  "  put \"varia  : \" & the value of e2 into line 5 of field \"data2\"\n"
  "  put 1234 into boucle\n"
  "  put the value of boucle into boucle\n"
  "  put \"boucle : \" & the value of boucle into line 6 of field \"data2\"\n"
  "  put \"vide   : [\" & the value of \"\" & \"]\" into line 7 of field \"data2\"\n"
  "  put \"casse  : \" & the value of \"3 > 2\" into line 8 of field \"data2\"\n"
  "  put \"indir  : \" & the value of r into line 9 of field \"data2\"\n"
  "end essai\n"
  "on mouseUp\n  put empty into field \"data2\"\n  debug raz\n  essai\n  debug bilan\nend mouseUp\n");
  hc_send(b,"mouseUp");
  printf("\n───── field data2 ─────\n%s\n", hc_field_text(f));
  hc_unregister_stack(st);hc_free(st);return 0;}
