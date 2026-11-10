/* « convert » sur un NOMBRE NU range son résultat dans `it`.
 *
 * « convert 86400 to short date » ne rangeait rien : le nombre partait vers
 * la branche des conteneurs, `it` gardait son ancienne valeur, sans erreur.
 * Entre guillemets, dans une variable ou par une expression, tout marchait
 * déjà : ces formes sont rejouées à côté, pour qu'elles ne bougent pas.
 * Ce que fait HyperCard d'un nombre nu : non mesuré. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void v(const char *l){char s[600];snprintf(s,sizeof s,"on t\n  %s\nend t\n",l);
  printf("── %s\n",l);hc_set_script(b,s);hc_send(b,"t");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");b=hc_new_button(c,"B");hc_set_current_card(c);
  v("put \"avant\" into it\n  convert 86400 to short date\n  put \"[\" & it & \"]\"");
  v("put \"avant\" into it\n  convert 0 to long date\n  put \"[\" & it & \"]\"");
  v("put \"avant\" into it\n  convert 3000000000 to dateItems\n  put \"[\" & it & \"]\"");
  v("put \"avant\" into it\n  convert \"86400\" to short date\n  put \"[\" & it & \"]\"");
  v("put \"avant\" into it\n  convert 86400 + 86400 to short date\n  put \"[\" & it & \"]\"");
  v("put \"avant\" into it\n  put 86400 into x\n  convert x to short date\n  put \"[\" & it & \"]\" && x");
  hc_free(st);return 0;}
