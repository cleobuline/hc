/* « the params » avec des paramètres énormes : chacun peut remplir un tampon
 * entier, et leur concaténation dépasse donc largement le plafond.
 *
 * Ce harnais est né d'un débordement réel — ASan avait signalé un « WRITE of
 * size 3001 » — et il MESURE le plafond, donc sa référence bouge quand HC_VAL
 * bouge. Ce n'est pas une régression : ce qu'il vérifie est que la longueur
 * s'arrête exactement à HC_VAL - 1 au lieu d'écrire au-delà.
 *
 * LES PARAMÈTRES FONT 60 000 CARACTÈRES, ET NON PLUS 700 000. Depuis le 10
 * novembre un paramètre qui ne tient pas dans son tampon est REFUSÉ au lieu
 * d'être coupé en silence (harnais grandtexte) : à 700 000 l'appel ne se
 * faisait plus, et ce harnais ne mesurait plus rien. Trois paramètres de
 * 60 000 passent chacun, et leur concaténation dépasse toujours le plafond —
 * c'est elle qu'on mesure. */
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
  /* un champ de 60 000 caractères */
  static char gros[60001]; memset(gros,'x',60000); gros[60000]=0;
  hc_set_field_text(f,gros);
  hc_set_script(b,
    "on recois a,bb,cc\n  put the params into card field \"sortie\"\n"
    "  put \"longueur de the params : \" & the length of card field \"sortie\"\nend recois\n"
    "on t\n  put card field \"gros\" into g\n"
    "  recois g, g, g\n"          /* 180 000 octets dans un tampon de 65 536 */
    "  put the paramCount\nend t\n");
  printf("── trois paramètres de 60 000 caractères\n");
  hc_send(b,"t");
  printf("── survécu\n");
  hc_free(st);return 0;}
