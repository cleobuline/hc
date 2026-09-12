#include "hc_core.h"
#include "hct_arbre.h"
#include <stdio.h>
static void run(Object *btn, const char *b){
  char s[512]; snprintf(s,sizeof s,"on mouseUp\n  debug raz\n  %s\n  debug bilan\nend mouseUp\n",b);
  hc_set_script(btn,s); printf("\n>>> %s\n", b); hc_send(btn,"mouseUp");
}
int main(void)
{
    Object *stack = hc_new_stack("T");
    Object *bg = hc_new_background(stack,"F");
    Object *c = hc_new_card(stack,bg,"U");
    Object *btn = hc_new_button(c,"B");
    Object *f = hc_new_field(c,"notes");
    hc_set_field_text(f,"abcdefghij");
    hc_set_current_card(c);
    run(btn, "select empty");
    run(btn, "select text of card field \"notes\"");
    run(btn, "select before text of card field \"notes\"");
    run(btn, "select after text of card field \"notes\"");
    run(btn, "select card field \"notes\"");
    run(btn, "select char 3 to 5 of card field \"notes\"");
    run(btn, "select line 1 of card field \"notes\"");
    hc_free(stack); return 0;
}
