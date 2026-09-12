/* La chaîne de messages doit atteindre TOUTES les piles en usage.
 *
 * « start using stack "X" » insère une pile dans la chaîne : ses gestionnaires
 * deviennent appelables de partout. HC en autorise huit. Mais la chaîne se
 * construit dans un tableau de taille fixe, et quatre places y sont déjà
 * prises par l'objet, la carte, le fond et la pile — si le tableau fait huit
 * entrées, seules les QUATRE piles les plus récemment déclarées sont
 * examinées, et un gestionnaire qui ne vit que dans une plus ancienne devient
 * invisible.
 *
 * Aucune mémoire ne manque, aucun message d'erreur ne le dit : le script
 * reçoit « ne sait pas faire ». */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  setbuf(stdout,NULL);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);

  /* La pile de travail, d'où l'on appellera. */
  Object *st=hc_new_stack("Travail");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");
  Object *b=hc_new_button(c,"B");
  hc_set_current_card(c);

  /* Huit bibliothèques. Chacune porte UN gestionnaire à son nom, pour qu'on
   * sache laquelle a répondu. */
  Object *lib[8];
  for(int i=0;i<8;i++){
    char nom[32]; snprintf(nom,sizeof nom,"Lib%d",i+1);
    lib[i]=hc_new_stack(nom);hc_register_stack(lib[i]);
    Object *lbg=hc_new_background(lib[i],"F");
    hc_new_card(lib[i],lbg,"U");
    char sc[128];
    snprintf(sc,sizeof sc,"on service%d\n  put \"répondu par Lib%d\"\nend service%d\n",
             i+1,i+1,i+1);
    hc_set_script(lib[i],sc);
  }

  /* Déclarées de la première à la huitième : Lib1 est donc la PLUS ANCIENNE,
   * celle que la chaîne tronquée laisse tomber. */
  for(int i=0;i<8;i++){
    char sc[64]; snprintf(sc,sizeof sc,"on t\n  start using stack \"Lib%d\"\nend t\n",i+1);
    hc_set_script(b,sc);hc_send(b,"t");
  }
  printf("── huit piles déclarées en usage\n");
  hc_set_script(b,"on t\n  put the stacksInUse\nend t\n");hc_send(b,"t");

  /* Chacune doit répondre, de la plus ancienne à la plus récente. */
  printf("\n── appel de chaque service depuis un bouton\n");
  for(int i=0;i<8;i++){
    char sc[128];
    snprintf(sc,sizeof sc,"on t\n  service%d\nend t\n",i+1);
    printf("  service%d :\n",i+1);
    hc_set_script(b,sc);hc_send(b,"t");
  }

  for(int i=0;i<8;i++){hc_unregister_stack(lib[i]);hc_free(lib[i]);}
  hc_unregister_stack(st);hc_free(st);return 0;}
