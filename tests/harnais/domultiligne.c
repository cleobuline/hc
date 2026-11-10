/* « do » exécute TOUTES les lignes de son texte.
 *
 * Il n'analysait qu'une instruction et ignorait la suite sans un mot :
 * « do "put 1" & return & "put 2" » n'affichait que 1. MESURÉ DANS HYPERCARD
 * (Basilisk II) par l'utilisatrice le 10 novembre : toutes les lignes s'y
 * exécutent. Le texte s'analyse en entier d'abord : une faute plus loin
 * refuse le tout, rien ne s'exécute à moitié. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void v(const char *l){char s[900];snprintf(s,sizeof s,"on t\n  %s\nend t\n",l);
  printf("── %s\n",l);hc_set_script(b,s);hc_send(b,"t");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");b=hc_new_button(c,"B");
  Object *f=hc_new_field(c,"F1");hc_set_field_text(f,"put 100\nput 200");
  hc_set_current_card(c);
  v("do \"put 1\" & return & \"put 2\" & return & \"put 3\"");
  v("put \"put 10\" & return & \"put 20\" into v\n  do v");
  v("do cd fld \"F1\"");
  v("do \"put 7; put 8\"");
  v("do \"repeat with i = 1 to 2\" & return & \"put i\" & return & \"end repeat\" & return & \"put 9\"");
  v("do \"put 1\" & return & return & \"put 2\"");
  v("do \"put 1\" & return & \"exit t\" & return & \"put 2\"\n  put \"après\"");
  v("do \"put 1\" & return & \"put 2 +\"");
  v("do \"put 5\"");
  hc_free(st);return 0;}
