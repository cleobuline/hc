/* « do "get line N of field \"menu\"" » depuis un script de FOND.
 *
 * LE DEUXIEME CAS A CHANGE DE REPONSE, et c'est voulu. « card field "menu" »,
 * alors que « menu » est un champ DE FOND, rendait [beta] : le resolveur v1
 * cherchait sur la carte puis se repliait sur le fond QUOI QU'ON AIT ECRIT, le
 * prefixe « card » etant consomme sans laisser de trace. Il rend maintenant
 * « objet introuvable », comme l'executeur v3 le faisait deja de son cote : une
 * portee explicite ne se replie pas. Le titre du cas le dit lui-meme — « champ
 * absent de cette carte ».
 *
 * CE QUI N'EST PAS MESURE : la reponse d'HyperCard. Les deux moteurs disent
 * maintenant la meme chose, ce qui est un progres dans tous les cas ; si un banc
 * dans Basilisk montre qu'HyperCard se replie, c'est les DEUX qu'il faudra
 * changer, et non revenir a l'incoherence. Le banc est demande dans
 * docs/mesures/identifiants_de_part.txt. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);
  else if(k==HC_INFO&&strstr(t,"recours"))printf("   %s\n",t);}
static Object *b, *st, *bg;
static void essai(const char *titre,const char *corps){
  char s[900]; snprintf(s,sizeof s,"on t\n  debug raz\n  %s\n  debug bilan\nend t\n",corps);
  printf("── %s\n",titre); hc_set_script(b,s); hc_send(b,"t"); printf("\n");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  st=hc_new_stack("T");hc_register_stack(st);
  bg=hc_new_background(st,"first");
  Object *c=hc_new_card(st,bg,"Une");
  Object *fm=hc_new_field(bg,"menu");     /* champ de FOND, comme chez elle */
  hc_set_current_card(c);
  hc_set_field_text(fm,"alpha\nbeta\ngamma");
  b=hc_new_button(c,"B");
  essai("x vide","put empty into x\n  put 2 into lineNumber\n  do \"get line\"&&lineNumber&&\"of\"&& x\n  put \"[\" & it & \"]\"");
  essai("champ absent de cette carte","put \"card field \" & quote & \"menu\" & quote into x\n  put 2 into lineNumber\n  do \"get line\"&&lineNumber&&\"of\"&& x\n  put \"[\" & it & \"]\"");
  essai("lecture directe","put line 2 of field \"menu\"");
  essai("par do","do \"get line 2 of field \" & quote & \"menu\" & quote\n  put it");
  essai("par do, comme le script","put \"field \" & quote & \"menu\" & quote into x\n"
        "  put 2 into lineNumber\n"
        "  do \"get line\"&&lineNumber&&\"of\"&& x\n  put it");
  essai("the value of x","put \"field \" & quote & \"menu\" & quote into x\n"
        "  put the value of x");
  essai("number of chars of line 1 to 2 of the value of x",
        "put \"field \" & quote & \"menu\" & quote into x\n"
        "  put (number of chars of line 1 to 2 of the value of x) + 1");
  hc_unregister_stack(st);hc_free(st);return 0;}
