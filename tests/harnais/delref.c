/* La référence : ce que l'ancien interprète rend pour « delete <morceau> ».
 * Tout portage devra rendre exactement ceci. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("%s\n",t);}
static Object *b;
static void cas(const char *init,const char *cmd){
  char s[600];
  snprintf(s,sizeof s,"on t\n  put \"%s\" into x\n  %s\n"
                      "  put \"%-34s de [%s] -> [\" & x & \"]\"\nend t\n",
           init,cmd,cmd,init);
  hc_set_script(b,s); hc_send(b,"t");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");
  hc_set_current_card(c);
  cas("a,b,c","delete item 1 of x");
  cas("a,b,c","delete item 2 of x");
  cas("a,b,c","delete item 3 of x");
  cas("a","delete item 1 of x");
  cas("a,b,c","delete item 1 to 2 of x");
  cas("a,b,c","delete item 2 to 3 of x");
  cas("a,,c","delete item 2 of x");
  cas("un deux trois","delete word 1 of x");
  cas("un deux trois","delete word 2 of x");
  cas("un deux trois","delete word 3 of x");
  cas("un  deux","delete word 1 of x");
  cas("abcdef","delete char 1 of x");
  cas("abcdef","delete char 3 of x");
  cas("abcdef","delete char 6 of x");
  cas("abcdef","delete char 2 to 4 of x");
  cas("a,b,c","delete last item of x");
  cas("un deux trois","delete last word of x");
  cas("a,b,c","delete item 9 of x");
  hc_free(st);return 0;}
