/* « sort … by » et « send … to » : la clé et les arguments, sans relecture. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);
  else if(k==HC_INFO&&(strstr(t,"relit")||strstr(t,"recours")))printf("   %s\n",t);}
static Object *b, *f;
static void essai(const char *titre,const char *corps){
  char s[1200]; snprintf(s,sizeof s,"on t\n  debug raz\n  %s\n  debug bilan\nend t\n"
    "on recu a, bb\n  put \"reçu [\" & a & \"] [\" & bb & \"]\"\nend recu\n"
    "function cle\n  return the short name of this card\nend cle\n", corps);
  printf("── %s\n",titre); hc_set_script(b,s); hc_send(b,"t"); printf("\n");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"delta");
  hc_new_card(st,bg,"alpha"); hc_new_card(st,bg,"charlie"); hc_new_card(st,bg,"bravo");
  f=hc_new_field(c,"L"); b=hc_new_button(c,"B"); hc_set_current_card(c);
  hc_set_field_text(f,"poire\npomme\nabricot\nkiwi");
  essai("tri d'un conteneur, sans clé","sort lines of card field \"L\"\n  put card field \"L\"");
  hc_set_field_text(f,"poire\npomme\nabricot\nkiwi");
  essai("tri par clé (each)","sort lines of card field \"L\" by length(each)\n  put card field \"L\"");
  hc_set_field_text(f,"3,10,2,25");
  essai("tri numérique","sort items of card field \"L\" numeric\n  put card field \"L\"");
  essai("tri de cartes par nom","sort cards by the short name of this card\n"
        "  put the short name of card 1 & \",\" & the short name of card 2 & \",\""
        " & the short name of card 3 & \",\" & the short name of card 4");
  essai("tri de cartes, ordre inverse","sort cards descending by the short name of this card\n"
        "  put the short name of card 1 & \",\" & the short name of card 4");
  essai("send sans argument","send \"recu\" to me");
  essai("send avec arguments","put 7 into n\n  send \"recu\" && n & \",\" & (n*2) to me");
  essai("send avec texte construit","put \"recu\" into m\n  send m && quote & \"x\" & quote to me");
  hc_unregister_stack(st);hc_free(st);return 0;}
