#include "hc_core.h"
int main(void){
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *a=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");hc_new_card(st,bg,"Trois");
 Object *b=hc_new_button(a,"B");hc_set_current_card(a);
 hc_set_script(b,"on mouseUp\n"
  "go next\n put the name of this card\n"
  "go next\n put the name of this card\n"
  "go prev\n put the name of this card\n"
  "go first\n put the name of this card\n"
  "go last\n put the name of this card\n"
  "go back\n put the name of this card\n"
  "go home\n put the name of this card\n"
  "end mouseUp\n");
 hc_send(b,"mouseUp");hc_free(st);return 0;}
