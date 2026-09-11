/* Comment ajouter une ligne a un champ sans laisser de ligne vide ? */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");
  hc_new_field(c,"data2"); hc_set_current_card(c);
  hc_set_script(b,
   "on mouseUp\n"
   "  -- A : texte puis return\n"
   "  put empty into card field \"data2\"\n"
   "  repeat with i = 1 to 3\n"
   "    put \"ligne \" & i & return after card field \"data2\"\n"
   "  end repeat\n"
   "  put \"A : \" & the number of lines of card field \"data2\" & \" lignes\"\n"
   "  -- B : return puis texte\n"
   "  put empty into card field \"data2\"\n"
   "  repeat with i = 1 to 3\n"
   "    put return & \"ligne \" & i after card field \"data2\"\n"
   "  end repeat\n"
   "  put \"B : \" & the number of lines of card field \"data2\" & \" lignes\"\n"
   "  put \"B ligne 1 = [\" & line 1 of card field \"data2\" & \"]\"\n"
   "  -- C : return SEULEMENT si le champ n'est pas vide\n"
   "  put empty into card field \"data2\"\n"
   "  repeat with i = 1 to 3\n"
   "    if card field \"data2\" is not empty then\n"
   "      put return after card field \"data2\"\n"
   "    end if\n"
   "    put \"ligne \" & i after card field \"data2\"\n"
   "  end repeat\n"
   "  put \"C : \" & the number of lines of card field \"data2\" & \" lignes\"\n"
   "  put \"C ligne 1 = [\" & line 1 of card field \"data2\" & \"]\"\n"
   "  put \"C ligne 3 = [\" & line 3 of card field \"data2\" & \"]\"\n"
   "end mouseUp\n");
  hc_send(b,"mouseUp");hc_free(st);return 0;}
