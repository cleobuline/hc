/* « sort cards by f() » quand f change les cartes : le tri se refuse.
 *
 * La clé est un gestionnaire, il peut créer ou supprimer une carte. Les
 * tableaux du tri étaient taillés pour le compte d'avant, et la boucle
 * écrivait sans le regarder : avec une clé qui fait « doMenu "New Card" »,
 * une écriture hors du tableau, mesurée sous ASan (hc_core.c, v3_cmd_sort).
 * Une clé qui supprime une carte arrêtait le tri sur « mémoire
 * insuffisante », ce qui n'était pas la raison.
 *
 * Le tri ordinaire, lui, ne bouge pas : il est rejoué d'abord. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static void menu(const char *article){
  if(!strcasecmp(article,"New Card")) hc_nouvelle_carte(0);}
static Object *b;
static void joue(const char *titre,const char *script){
  printf("── %s\n",titre);hc_set_script(b,script);hc_send(b,"t");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.do_menu=menu;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"C");hc_new_card(st,bg,"A");hc_new_card(st,bg,"B");
  b=hc_new_button(bg,"B");hc_set_current_card(c);
  joue("un tri ordinaire, par le nom",
    "on t\n"
    "  sort cards by the short name of this card\n"
    "  put the short name of card 1 & the short name of card 2 & the short name of card 3\n"
    "end t\n");
  joue("la clé crée une carte à chaque appel",
    "on t\n"
    "  sort cards by cree()\n"
    "  put \"result : [\" & the result & \"] après\"\n"
    "end t\n"
    "function cree\n"
    "  doMenu \"New Card\"\n"
    "  return 1\n"
    "end cree\n");
  joue("la clé supprime une carte",
    "on t\n"
    "  global g\n"
    "  put 0 into g\n"
    "  sort cards by ote()\n"
    "  put \"après\"\n"
    "end t\n"
    "function ote\n"
    "  global g\n"
    "  add 1 to g\n"
    "  if g is 1 then delete card 3\n"
    "  return g\n"
    "end ote\n");
  joue("et la pile tient encore",
    "on t\n"
    "  put (the number of cards > 0) && the short name of this card is not empty\n"
    "end t\n");
  hc_free(st);return 0;}
