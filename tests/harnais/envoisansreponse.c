/* « send » d'un message que personne ne prend est une faute.
 *
 * « send "t2" to me », sans gestionnaire t2 nulle part, se taisait, et
 * « the result » restait vide. MESURÉ DANS HYPERCARD (Basilisk II) par
 * l'utilisatrice le 13 novembre : il y dit « Can't understand "t2" ». HC
 * dit maintenant ce qu'il dit du même message écrit seul sur sa ligne
 * (gestionnaire w).
 *
 * Un message pris ailleurs que dans la chaîne de l'expéditeur (t3, dans le
 * bouton de la carte 2) n'est pas une faute.
 *
 * Et un VERBE envoyé s'exécute : MESURÉ DANS HYPERCARD (Basilisk II) le
 * même jour, « send "put 5 into x" to me » puis « put x » y affiche 5.
 * Et « me », pendant la ligne envoyée, reste l'expéditeur : mesuré le même
 * jour, « send "put the name of me" to cd 1 », depuis un bouton, y affiche
 * card button "New Button". */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");Object *c2=hc_new_card(st,bg,"V");
  Object *b=hc_new_button(c,"B");Object *b2=hc_new_button(c2,"B2");
  hc_set_current_card(c);
  hc_set_script(b2,"on t3\n  put \"t3 de B2\"\nend t3\n");
  hc_set_script(b,
    "on t\n"
    "  send \"t2\" to me\n"
    "  put \"[\" & the result & \"]\"\n"
    "end t\n"
    "on u\n"
    "  send \"t3\" to me\n"
    "  put \"[\" & the result & \"]\"\n"
    "end u\n"
    "on v\n"
    "  send \"t3\" to cd btn 1 of cd 2\n"
    "  put \"[\" & the result & \"]\"\n"
    "  send \"beep\" to me\n"
    "  put \"beep [\" & the result & \"]\"\n"
    "  send \"put 5 into x\" to me\n"
    "  put \"x=[\" & x & \"]\"\n"
    "  send \"add 2 to x\" to me\n"
    "  put \"x=[\" & x & \"]\"\n"
    "  send \"put the name of me\" to cd 1\n"
    "  send \"put\" to me\n"
    "  put \"put seul [\" & the result & \"]\"\n"
    "end v\n"
    "on w\n"
    "  t2\n"
    "  put \"[\" & the result & \"]\"\n"
    "end w\n");
  hc_send(b,"t"); hc_send(b,"u"); hc_send(b,"v"); hc_send(b,"w");
  hc_free(st);return 0;}
