/* Les quatre portes, une par une : chacune doit se dénoncer par son nom. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_ERR)printf("   %s\n",t); else if(k==HC_MSG)printf("   [msg] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");
  Object *b=hc_new_button(c,"B");hc_set_current_card(c);

  printf("── PORTE 1 : gestionnaire au cadre cassé\n");
  hc_set_script(b,"on casse\n  put \"dedans\"\n");   /* pas de « end » */
  hc_send(b,"casse");

  printf("\n── PORTE 2 : message sans gestionnaire\n");
  hc_set_script(b,"on t\n  dessineLeChat 3\nend t\n");
  hc_send(b,"t");

  printf("\n── PORTE 2 bis : commande que la v3 ne sait pas faire\n");
  hc_set_script(b,"on t\n  put \"x\" into field \"absentDeToutesLesCartes\"\nend t\n");
  hc_send(b,"t");

  printf("\n── PORTE 3 : article de menu du noyau\n");
  hc_do_menu("Next");
  printf("   carte : %s\n", hc_current_card()->name);

  printf("\n── PORTE 4 : la boîte de message\n");
  hc_do("put 2 + 3");
  hc_do("zzz brrr ###");

  printf("\n── et le reste marche toujours\n");
  hc_set_script(b,"on t\n  put \"tout va bien\"\nend t\n");
  hc_send(b,"t");
  hc_unregister_stack(st);hc_free(st);return 0;}
