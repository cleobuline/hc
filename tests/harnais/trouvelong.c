/* « find » dans un long champ, et la longueur de ce qu'il trouve.
 *
 * 1. La forme de base replie les accents dans une copie du texte, et cette
 *    copie s'arrêtait à 8 192 octets de tampon : au-delà de seize mille
 *    caractères, rien n'était cherché, sans un mot. « find "zebre" » dans
 *    un champ de 20 000 caractères répondait « Not found » sur un mot de la
 *    fin que « find international » trouvait.
 *
 * 2. La longueur de la trouvaille était celle du MOTIF, pas celle du texte :
 *    « ele » trouve « élè », trois octets cherchés et cinq trouvés.
 *    « find string "ele" » rendait « él », « find chars "elev" » un « è »
 *    coupé en deux, et the foundChunk s'arrêtait un caractère trop tôt. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");Object *b=hc_new_button(c,"B");hc_new_field(c,"F1");
  hc_set_current_card(c);
  hc_set_script(b,
    "on t\n"
    "  put \"abcdefghi \" into s\n"
    "  repeat 11 times\n"
    "    put s & s into s\n"
    "  end repeat\n"
    "  put s & \"zebre fin\" into cd fld \"F1\"\n"
    "  put the length of cd fld \"F1\"\n"
    "  find \"zebre\"\n"
    "  put \"[\" & the result & \"] [\" & the foundText & \"]\" && the foundChunk\n"
    "  find international \"zebre\"\n"
    "  put \"[\" & the result & \"] [\" & the foundText & \"]\"\n"
    "  find string \"e f\"\n"
    "  put \"[\" & the result & \"] [\" & the foundText & \"]\"\n"
    "  put \"un élève studieux\" into cd fld \"F1\"\n"
    "  find string \"ele\"\n"
    "  put \"[\" & the result & \"] [\" & the foundText & \"]\" && the foundChunk\n"
    "  find chars \"elev\"\n"
    "  put \"[\" & the result & \"] [\" & the foundText & \"]\" && the foundChunk\n"
    "  find whole \"eleve studieux\"\n"
    "  put \"[\" & the result & \"] [\" & the foundText & \"]\" && the foundChunk\n"
    "  find \"eleve\"\n"
    "  put \"[\" & the result & \"] [\" & the foundText & \"]\" && the foundChunk\n"
    "  find string \"stud\"\n"
    "  put \"[\" & the result & \"] [\" & the foundText & \"]\" && the foundChunk\n"
    "end t\n");
  hc_send(b,"t");
  hc_free(st);return 0;}
