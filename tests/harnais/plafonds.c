/* Deux plafonds qui débordaient en SILENCE.
 *
 * Le registre des piles s'arrêtait à seize : la dix-septième pile s'ouvrait,
 * l'hôte l'affichait, mais « go to stack » ne la trouvait pas et « the stacks »
 * ne la nommait pas. Il grandit maintenant.
 *
 * Les piles en usage gardent un plafond — ce sont des maillons de la chaîne de
 * messages, qui se parcourt sur une taille fixe — mais le refus se dit. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *corps){
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",corps); hc_set_script(b,s); hc_send(b,"t");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *piles[24], *cartes[24];
  for(int i=0;i<24;i++){
    char nom[32]; snprintf(nom,sizeof nom,"P%02d",i+1);
    piles[i]=hc_new_stack(nom); hc_register_stack(piles[i]);
    Object *bg=hc_new_background(piles[i],"F");
    cartes[i]=hc_new_card(piles[i],bg,"U");
  }
  printf("── vingt-quatre piles ouvertes\n");
  printf("   hc_stack_count = %d\n\n", hc_stack_count());

  Object *c=cartes[0];
  b=hc_new_button(c,"B"); hc_set_current_card(c);
  essai("put the number of lines of the stacks");
  essai("put line 17 of the stacks");   /* muette avant : au-delà du plafond */
  essai("put line 24 of the stacks");
  printf("\n");

  /* Les piles en usage : huit passent, la neuvième doit se plaindre. */
  for(int i=1;i<=9;i++){
    char corps[64]; snprintf(corps,sizeof corps,"start using stack \"P%02d\"",i+1);
    essai(corps);
  }
  printf("\n");
  for(int i=0;i<24;i++){ hc_unregister_stack(piles[i]); hc_free(piles[i]); }
  return 0;}
