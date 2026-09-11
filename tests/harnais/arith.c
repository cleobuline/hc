/* « add 1 to x » sur une variable vide : HyperCard traite le vide comme 0. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void v(const char *l){char s[400];snprintf(s,sizeof s,"on t\n  %s\nend t\n",l);
  printf("── %s\n",l);hc_set_script(b,s);hc_send(b,"t");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");b=hc_new_button(c,"B");hc_set_current_card(c);
  v("add 1 to x\n  put \"[\" & x & \"]\"");
  v("put empty into y\n  add 1 to y\n  put \"[\" & y & \"]\"");
  v("put 5 into z\n  add 1 to z\n  put z");
  v("put empty into w\n  subtract 3 from w\n  put w");
  v("put empty into q\n  multiply q by 4\n  put q");
  v("put \"\" into e\n  put e + 1");
  v("put \"\" into e2\n  put e2 & \"|\" & (e2 * 3)");
  hc_free(st);return 0;}
