/* « the average of 1,2 » et sa famille. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *corps){
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",corps); hc_set_script(b,s); hc_send(b,"t");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  Object *f=hc_new_field(c,"F"); hc_set_field_text(f,"10,20,30");
  b=hc_new_button(c,"B");hc_set_current_card(c);
  essai("put the average of 1,2");
  essai("put average(1,2)");
  essai("put the average of 1,2,3,4");
  essai("put the sum of 1,2,3");
  essai("put the min of 5,2,9");
  essai("put the max of 5,2,9");
  essai("put the average of 2");
  essai("put the average of item 1 to 3 of card field \"F\"");
  essai("put 7 into a\n  put 3 into z\n  put the sum of a, z, a + z");
  essai("put the number of cards");
  essai("put the length of \"abc\"");
  essai("put \"[\" & the average of 1,2 & \"]\"");
  essai("put the average of 1,2 into r\n  put r");
  hc_free(st);return 0;}
