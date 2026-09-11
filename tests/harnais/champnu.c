/* « field "X" » sans préfixe : HyperCard cherche la carte PUIS le fond. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);
  else if(k==HC_INFO && strstr(t,"recours"))printf("   %s\n",t);}
static Object *b;
static void essai(const char *corps){
  char s[512]; snprintf(s,sizeof s,"on t\n  debug raz\n  %s\n  debug bilan\nend t\n",corps);
  printf("── %s\n",corps); hc_set_script(b,s); hc_send(b,"t"); printf("\n");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  Object *fc=hc_new_field(c,"surCarte");
  Object *fb=hc_new_field(bg,"menu");          /* champ de FOND */
  hc_set_field_text(fc,"carte");
  b=hc_new_button(c,"B");hc_set_current_card(c);
  hc_set_field_text(fb,"fond");
  essai("put field \"surCarte\"");
  essai("put card field \"surCarte\"");
  essai("put bg field \"menu\"");
  essai("put field \"menu\"");
  essai("put the number of lines of field \"menu\"");
  essai("put \"x\" into field \"menu\"\n  put bg field \"menu\"");
  hc_unregister_stack(st);hc_free(st);return 0;}
