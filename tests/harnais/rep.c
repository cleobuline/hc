#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static int n; static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)n++; else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void v(const char *l){char s[400];snprintf(s,sizeof s,"on t\n  %s\nend t\n",l);
  n=0;hc_set_script(b,s);hc_send(b,"t");printf("  %-34s -> %d tours\n",l,n);}
int main(void){static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");b=hc_new_button(c,"B");hc_set_current_card(c);
  v("repeat 3 times\n    put 1\n  end repeat");
  v("repeat 0 times\n    put 1\n  end repeat");
  v("repeat -1 times\n    put 1\n  end repeat");
  v("repeat -5 times\n    put 1\n  end repeat");
  v("put -2 into k\n  repeat k times\n    put 1\n  end repeat");
  hc_free(st);return 0;}
