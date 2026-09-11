#include "hc_core.h"
#include <stdio.h>
static void run(Object *btn, const char *l, const char *b){
  char s[1024]; snprintf(s,sizeof s,"on mouseUp\n  debug raz\n  %s\n  debug bilan\nend mouseUp\n",b);
  hc_set_script(btn,s); printf(">>> %s\n", l); hc_send(btn,"mouseUp");
}
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *c1    = hc_new_card(stack, bg, "Une");
    Object *c2    = hc_new_card(stack, bg, "Deux");
    Object *btn   = hc_new_button(c1, "B");
    Object *fb    = hc_new_field(bg, "menu");
    fb->shared_text = 1;
    hc_set_current_card(c1);
    hc_set_field_text(fb, "TEXTE-FOND");
    run(btn, "of this card",      "put field \"menu\" of this card");
    run(btn, "of card 2",         "put field \"menu\" of card 2");
    run(btn, "of card \"Deux\"",  "put field \"menu\" of card \"Deux\"");
    run(btn, "line 1 of field",   "put line 1 of field \"menu\"");
    run(btn, "the text of field", "put the text of field \"menu\"");
    run(btn, "champ absent",      "put field \"pasLa\"");
    run(btn, "set du champ",      "set the visible of field \"menu\" to true");
    run(btn, "put dans le champ", "put \"X\" into field \"menu\"");
    run(btn, "select",            "select text of field \"menu\"");
    hc_free(stack);
    return 0;
}
