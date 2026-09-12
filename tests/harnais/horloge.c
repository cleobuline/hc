#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   %s\n",t);}
int main(void){static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c=hc_new_card(st,bg,"U");Object *b=hc_new_button(c,"B");hc_set_current_card(c);
 hc_set_script(b,"on t\n  put the long date\n  put the time\n  put the seconds\n"
   "  convert the date to dateItems\n  put it\nend t\n");
 hc_send(b,"t");hc_free(st);return 0;}
