/* « go home » : la pile nommée Home, par le chemin de « go to stack ». */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);
  else if(k==HC_TRACE && strstr(t,"message \"")&&strstr(t,"→"))printf("   %s\n",t);}
static Object *g_home;
/* L'hôte ouvre la pile Home à la demande, comme le fera Cocoa. */
static Object *mon_open(const char *nom){
  if(strcasecmp(nom,"Home")) return NULL;
  if(!g_home){
    g_home = hc_new_stack("Home");
    Object *bg = hc_new_background(g_home,"Fond");
    hc_new_card(g_home,bg,"Accueil");
    hc_register_stack(g_home);
    printf("   (l'hôte ouvre la pile Home)\n");
  }
  return g_home;}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.open_stack=mon_open;
  hc_set_host(&h);
  Object *st=hc_new_stack("Travail");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");
  Object *b=hc_new_button(c,"B");hc_set_current_card(c);
  hc_set_script(b,
   "on t\n  go card \"Deux\"\n  put the name of this card\n"
   "  go home\n  put the name of this card\n"
   "  put \"result : [\" & the result & \"]\"\n"
   "  go back\n  put the name of this card\n"
   "  put the stacks\n  put \"--\"\n  put the recent cards\nend t\n");
  hc_send(b,"t");
  hc_unregister_stack(st);
  if(g_home){hc_unregister_stack(g_home);hc_free(g_home);}
  hc_free(st);return 0;}
