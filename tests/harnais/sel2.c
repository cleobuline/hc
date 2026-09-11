#include "hc_core.h"
#include <stdio.h>
static Object *gf;
static void run(Object *btn, const char *b){
  char s[512]; snprintf(s,sizeof s,"on mouseUp\n  debug raz\n  %s\n  debug bilan\nend mouseUp\n",b);
  hc_set_script(btn,s);
  hc_set_selection(NULL,0,0);
  printf(">>> %-46s", b);
  hc_send(btn,"mouseUp");
  Object *f; int st,len;
  if (hc_get_selection(&f,&st,&len)) printf("  sel: debut=%d long=%d\n", st, len);
  else printf("  sel: aucune\n");
}
int main(void)
{
    Object *stack = hc_new_stack("T");
    Object *bg = hc_new_background(stack,"F");
    Object *c = hc_new_card(stack,bg,"U");
    Object *btn = hc_new_button(c,"B");
    gf = hc_new_field(c,"notes");
    hc_set_field_text(gf,"abcdefghij");
    hc_set_current_card(c);
    run(btn, "select empty");
    run(btn, "select text of card field \"notes\"");
    run(btn, "select before text of card field \"notes\"");
    run(btn, "select after text of card field \"notes\"");
    run(btn, "select card field \"notes\"");
    run(btn, "select char 3 to 5 of card field \"notes\"");
    run(btn, "select char 2 of card field \"notes\"");
    run(btn, "select before char 3 of card field \"notes\"");
    run(btn, "select last char of card field \"notes\"");
    hc_free(stack); return 0;
}
