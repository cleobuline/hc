/* « the value of » d'un texte qui n'est pas une expression : le texte, et la
 * source n'est évaluée QU'UNE fois.
 *
 * « the value of f() » appelait f deux fois quand f rendait « bonjour le
 * monde », et rendait « value of f » — le source de la ligne. Un littéral
 * donnait une erreur, une variable ce même texte fabriqué. MESURÉ DANS
 * HYPERCARD (Basilisk II) par l'utilisatrice le 10 novembre : « put the
 * value of "bonjour le monde" » y rend « bonjour le monde ».
 *
 * Et « delete word » d'un mot entre guillemets collé à une lettre accentuée
 * n'emporte plus la moitié de la lettre. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");Object *b=hc_new_button(c,"B");
  Object *f=hc_new_field(c,"F1");hc_set_field_text(f,"un deux");
  hc_set_current_card(c);
  hc_set_script(b,
    "on t\n"
    "  global g\n"
    "  put 0 into g\n"
    "  put the value of f9() into r\n"
    "  put \"appels :\" && g && \"/ rendu : [\" & r & \"]\"\n"
    "  put \"[\" & the value of \"bonjour le monde\" & \"]\"\n"
    "  put \"bonjour le monde\" into x\n"
    "  put \"[\" & the value of x & \"]\"\n"
    "  put \"cd fld 1\" into x\n"
    "  put \"[\" & the value of x & \"]\"\n"
    "  put \"2 + 3\" into x\n"
    "  put \"[\" & the value of x & \"]\"\n"
    "  put quote & \"a\" & quote & \"é\" into x\n"
    "  delete word 1 of x\n"
    "  put \"[\" & x & \"]\" && the length of x\n"
    "  put \"a b  c\" into x\n"
    "  delete word 2 of x\n"
    "  put \"[\" & x & \"]\"\n"
    "  delete word 2 of x\n"
    "  put \"[\" & x & \"]\"\n"
    "end t\n"
    "function f9\n"
    "  global g\n"
    "  add 1 to g\n"
    "  return \"bonjour le monde\"\n"
    "end f9\n");
  hc_send(b,"t");
  hc_free(st);return 0;}
