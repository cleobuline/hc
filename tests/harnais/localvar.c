/* « local » : déclaration de variables de gestionnaire. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *titre,const char *corps){
  char s[1024]; snprintf(s,sizeof s,"on t\n  debug raz\n  %s\n  debug bilan\nend t\n",corps);
  printf("── %s\n",titre);
  hc_set_script(b,s); hc_send(b,"t"); printf("\n");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");hc_set_current_card(c);
  essai("un nom","local tR\n  put \"[\" & tR & \"]\"");
  essai("trois noms","local tR, tG, tB\n  put \"[\" & tR & tG & tB & \"]\"");
  essai("puis on s'en sert","local acc\n  repeat with k = 1 to 3\n    put k after acc\n  end repeat\n  put acc");
  essai("n'écrase pas une valeur posée avant","put 7 into z\n  local z\n  put \"[\" & z & \"]\"");
  essai("le résultat reste vide","local a\n  put \"[\" & the result & \"]\"");
  hc_free(st);return 0;}
