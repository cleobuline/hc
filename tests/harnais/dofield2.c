/* « do "get line N of field \"menu\"" » depuis un script de FOND. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);
  else if(k==HC_INFO&&strstr(t,"recours"))printf("   %s\n",t);}
static Object *b, *st, *bg;
static void essai(const char *titre,const char *corps){
  char s[900]; snprintf(s,sizeof s,"on t\n  debug raz\n  %s\n  debug bilan\nend t\n",corps);
  printf("── %s\n",titre); hc_set_script(b,s); hc_send(b,"t"); printf("\n");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  st=hc_new_stack("T");hc_register_stack(st);
  bg=hc_new_background(st,"first");
  Object *c=hc_new_card(st,bg,"Une");
  Object *fm=hc_new_field(bg,"menu");     /* champ de FOND, comme chez elle */
  hc_set_current_card(c);
  hc_set_field_text(fm,"alpha\nbeta\ngamma");
  b=hc_new_button(c,"B");
  essai("x vide","put empty into x\n  put 2 into lineNumber\n  do \"get line\"&&lineNumber&&\"of\"&& x\n  put \"[\" & it & \"]\"");
  essai("champ absent de cette carte","put \"card field \" & quote & \"menu\" & quote into x\n  put 2 into lineNumber\n  do \"get line\"&&lineNumber&&\"of\"&& x\n  put \"[\" & it & \"]\"");
  essai("lecture directe","put line 2 of field \"menu\"");
  essai("par do","do \"get line 2 of field \" & quote & \"menu\" & quote\n  put it");
  essai("par do, comme le script","put \"field \" & quote & \"menu\" & quote into x\n"
        "  put 2 into lineNumber\n"
        "  do \"get line\"&&lineNumber&&\"of\"&& x\n  put it");
  essai("the value of x","put \"field \" & quote & \"menu\" & quote into x\n"
        "  put the value of x");
  essai("number of chars of line 1 to 2 of the value of x",
        "put \"field \" & quote & \"menu\" & quote into x\n"
        "  put (number of chars of line 1 to 2 of the value of x) + 1");
  hc_unregister_stack(st);hc_free(st);return 0;}
