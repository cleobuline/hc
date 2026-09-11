/* Le point-virgule, séparateur d'instructions. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *titre,const char *corps){
  char s[1024]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n     %s\n",titre,corps);
  hc_set_script(b,s); hc_send(b,"t"); printf("\n");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");
  b=hc_new_button(c,"B");hc_set_current_card(c);
  essai("deux instructions","put \"un\" ; put \"deux\"");
  essai("affectation puis lecture","put 1 into i ; put i + 1");
  essai("trois","put \"a\" ; put \"b\" ; put \"c\"");
  essai("sans espaces","put \"a\";put \"b\"");
  essai("point-virgule final","put \"seul\" ;");
  essai("vides en série","put \"a\" ;; put \"b\"");
  essai("dans un repeat","repeat with k = 1 to 2\n    put k ; put k * 10\n  end repeat");
  essai("if d'une ligne","put 0 into i ; if i = 0 then put \"nul\"");
  essai("if a blocs intact","if 1 = 1 then\n    put \"oui\"\n  else\n    put \"non\"\n  end if");
  essai("le cas de test_v3e","put \"i\" ; put 1 into i\n  put the short name of card (i + 1)");
  essai("avec un commentaire","put \"a\" ; put \"b\" -- fin");
  hc_free(st);return 0;}
