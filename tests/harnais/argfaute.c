/* Un argument qui échoue ne doit pas laisser partir le message. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   [msg] %s\n",t);
  else if(k==HC_ERR)printf("   [ERR] %s\n",t);
  else if(k==HC_TRACE&&strstr(t,"→ message"))printf("   %s\n",t);}
static Object *b;
static void v(const char *l){char s[600];
  snprintf(s,sizeof s,"on t\n  %s\nend t\n"
   "on recois a,bb\n  put \"RECU [\" & a & \"] [\" & bb & \"]\"\nend recois\n",l);
  printf("── %s\n",l);hc_set_script(b,s);hc_send(b,"t");printf("\n");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");b=hc_new_button(c,"B");hc_set_current_card(c);
  v("recois 1, 2");
  v("recois 1, field \"pasLa\"");          /* second argument fautif */
  v("recois field \"pasLa\", 2");          /* premier argument fautif */
  v("put 1, field \"pasLa\"");
  hc_unregister_stack(st);hc_free(st);return 0;}
