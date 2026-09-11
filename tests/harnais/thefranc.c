/* « the » ne ment plus : un nom inconnu derrière lui est une faute nommée,
 * tandis qu'un mot nu garde la règle du littéral. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG) printf("   [msg] %s\n",t);
  else if(k==HC_ERR) printf("   [ERR] %s\n",t);}
static void mon_set(const char*n,const char*v){(void)n;(void)v;}
static const char *mon_get(const char*n){
  if(!strcasecmp(n,"tool")) return "browse";
  return NULL;}
static Object *b;
static void essai(const char *titre,const char *corps){
  char s[1024]; snprintf(s,sizeof s,"on t\n%s\nend t\n",corps);
  hc_set_script(b,s); printf("── %s\n",titre); hc_send(b,"t");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");
  hc_new_field(c,"F1"); hc_set_current_card(c);

  essai("faute de frappe : doit se plaindre, en nommant",
        "  put the userLevl");
  essai("propriete absente : doit se plaindre",
        "  put the numberFormat");
  essai("mot nu : rend son propre nom, comme avant",
        "  put zorglub");
  essai("« go card canard » sans guillemets marche toujours",
        "  put canard\n  put \"ok, le littéral nu est intact\"");
  essai("variable : « the » devant une variable connue reste servi",
        "  put 42 into x\n  put the x");
  essai("les vraies fonctions passent",
        "  put the date is not empty\n  put the tool\n  put the number of cards");
  essai("les constantes passent",
        "  put the empty is empty\n  put \"[\" & the quote & \"]\"");
  essai("les proprietes d'objet passent",
        "  put the name of me\n  put the number of card fields");
  essai("adjectifs",
        "  put the long date is not empty\n  put the short name of me");
  essai("une faute n'arrete que le gestionnaire, pas le programme",
        "  put \"avant\"\n  put the blurp\n  put \"apres — ne doit PAS s'afficher\"");
  essai("apres la faute, tout remarche",
        "  put \"le suivant tourne normalement\"");
  hc_free(st);return 0;
}
