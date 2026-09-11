/* « the params » avec des paramètres énormes : le tampon fait 1 Mio, les
 * seize paramètres peuvent en porter seize. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t);
  else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");
  Object *f=hc_new_field(c,"gros");
  hc_new_field(c,"sortie");
  Object *b=hc_new_button(c,"B");hc_set_current_card(c);
  /* un champ de 700 000 caractères */
  static char gros[700001]; memset(gros,'x',700000); gros[700000]=0;
  hc_set_field_text(f,gros);
  hc_set_script(b,
    "on recois a,bb,cc\n  put the params into card field \"sortie\"\n"
    "  put \"longueur de the params : \" & the length of card field \"sortie\"\nend recois\n"
    "on t\n  put card field \"gros\" into g\n"
    "  recois g, g, g\n"          /* 2,1 Mio dans un tampon de 1 Mio */
    "  put the paramCount\nend t\n");
  printf("── trois paramètres de 700 000 caractères\n");
  hc_send(b,"t");
  printf("── survécu\n");
  hc_free(st);return 0;}
