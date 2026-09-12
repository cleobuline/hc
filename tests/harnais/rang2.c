/* Les propriétés de rang 2 : ce que le noyau sait, ce qu'il confie à l'hôte. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
/* Un hôte qui répond comme le fera Cocoa. */
static const char *mon_get(const char*n){
  if(!strcasecmp(n,"diskSpace")) return "123456789";
  if(!strcasecmp(n,"systemVersion")) return "15.4.1";
  return NULL;}
static Object *b;
static void essai(const char *corps){
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",corps); hc_set_script(b,s); hc_send(b,"t"); printf("\n");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_get=mon_get;
  hc_set_host(&h);
  Object *st=hc_new_stack("Demo");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");
  b=hc_new_button(c,"B");hc_set_current_card(c);
  essai("put the stacks");
  essai("create menu \"Outils\"\n  create menu \"Aide\"\n  put the menus");
  essai("put the number of menus");
  essai("go card \"Deux\"\n  go card \"Une\"\n  put the recent cards");
  essai("put the recent names");
  essai("put the diskSpace");
  essai("put the systemVersion");
  essai("put the heapSpace");
  hc_unregister_stack(st);hc_free(st);return 0;}
