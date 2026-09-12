#include "hc_core.h"
int main(void)
{
    Object *st = hc_new_stack("T"); Object *bg = hc_new_background(st,"F");
    Object *c = hc_new_card(st,bg,"U"); Object *b = hc_new_button(c,"B");
    hc_set_current_card(c);
    hc_set_script(b,
      "on mouseUp\n"
      "  set dragSpeed to 0\n"
      "  set the numberFormat to \"0.00\"\n"
      "  put 1/3\n"
      "  set the userLevel to 5\n"
      "  set the editBkgnd to true\n"
      "  put the userLevel\n"
      "  put the sound\n"
      "  put exp1(1) & \" \" & ln1(1) & \" \" & log2(8) & \" \" & exp2(3)\n"
      "  put the long name of me\n"
      "  put the destination\n"
      "end mouseUp\n");
    hc_send(b,"mouseUp"); hc_free(st); return 0;
}
