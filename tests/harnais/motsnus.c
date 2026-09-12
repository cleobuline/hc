/* Les mots nus passés en argument ne doivent plus coûter un emprunt à la v1
 * qu'UNE fois chacun, quel que soit le nombre de tours de boucle. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");
  hc_set_current_card(c);
  hc_set_script(st,
    "on setFont police,taille,align,style\n"
    "  -- on ne fait rien, seul l'appel compte\n"
    "end setFont\n"
    "function getPattern n\n  return item (n mod 14) + 1 of \"13,11,22,15\"\n"
    "end getPattern\n");
  hc_set_script(b,
    "on mouseUp\n"
    "  repeat with i = 1 to 9\n"
    "    setFont geneva,10,center,bold\n"
    "    setFont geneva,10,left,plain\n"
    "    put getPattern(i) into z\n"
    "  end repeat\n"
    "end mouseUp\n");
  hc_v3_bilan_remise_a_zero();
  hc_send(b,"mouseUp");
  hc_v3_bilan();
  hc_free(st);return 0;}
