/* Un script que la v3 sait lire, et un qu'elle ne sait pas : le relevé
 * doit nommer le second et ignorer le premier. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  Object *bon=hc_new_button(c,"Propre");
  Object *mauvais=hc_new_button(c,"Tordu");
  hc_set_current_card(c);
  hc_set_script(bon,"on mouseUp\n  put 1 into x\n  repeat with i = 1 to 5\n"
                    "    add i to x\n  end repeat\nend mouseUp\n");
  /* une tournure que l'analyseur v3 refuse : le gestionnaire entier
     retombera sur la v1 */
  hc_set_script(mauvais,"on mouseUp\n  put 1 into x\n  repeat with i = 1 to 5\n"
                        "    add i to x\n  end repeat\n  ) ( mauvais\nend mouseUp\n");
  hc_v3_bilan_remise_a_zero();
  hc_send(bon,"mouseUp");
  hc_send(mauvais,"mouseUp");
  hc_v3_bilan();
  hc_free(st);return 0;}
