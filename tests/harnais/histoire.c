/* L'historique de navigation : go back, the recent cards. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *titre,const char *corps){
  char s[2048]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",titre); hc_set_script(b,s); hc_send(b,"t"); printf("\n");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T"); hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c1=hc_new_card(st,bg,"Une");
  hc_new_card(st,bg,"Deux"); hc_new_card(st,bg,"Trois"); hc_new_card(st,bg,"Quatre");
  b=hc_new_button(c1,"B"); hc_set_current_card(c1);
  essai("l'historique est vide au départ","put \"[\" & the recent cards & \"]\"");
  essai("on visite trois cartes",
    "go card \"Deux\"\n  go card \"Trois\"\n  go card \"Quatre\"\n"
    "  put the name of this card\n  put \"--\"\n  put the recent names");
  essai("go back retrace les pas",
    "go back\n  put the name of this card\n"
    "  go back\n  put the name of this card\n"
    "  go back\n  put the name of this card");
  essai("au bout, il ne se passe rien","go back\n  put the result\n  put the name of this card");
  essai("revenir sur la même carte ne compte pas",
    "go card \"Deux\"\n  go card \"Deux\"\n  go card \"Deux\"\n  put the recent names");
  essai("la forme longue","put the recent cards");
  essai("on visite Trois puis Quatre, et on supprime Trois",
    "go card \"Trois\"\n  go card \"Quatre\"\n  delete card \"Trois\"\n"
    "  put \"pendant le gestionnaire : \" & line 1 to 3 of the recent names");
  essai("la carte supprimée est sortie de l'historique",
    "put the recent names\n  put \"--\"\n  go back\n  put the name of this card");
  hc_unregister_stack(st); hc_free(st); return 0;}
