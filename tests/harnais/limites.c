/* Quatre limites qui étaient fausses, muettes, ou les deux.
 *
 *  — random(10^300) convertissait un double hors bornes en long : le même
 *    comportement indéfini que les rangs de morceaux, resté seul de son
 *    espèce ;
 *  — une neuvième « start using » CHARGEAIT la pile avant de constater le
 *    plafond, laissant une pile vivante et invisible ;
 *  — une fonction définie dans une pile perdait ses arguments après le
 *    huitième, en silence, alors que la table des paramètres en tient
 *    quinze ;
 *  — « send » s'arrêtait à seize arguments pour en jeter un à l'arrivée. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static int g_charges = 0;          /* combien de fois load_stack a été appelée */
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}

/* Un hôte qui fabrique la pile demandée, comme le fait Cocoa : c'est le seul
 * moyen de voir si le noyau la charge avant ou après avoir vérifié le
 * plafond. */
static Object *faux_load(const char *nom){
  g_charges++;
  Object *st = hc_new_stack(nom);
  hc_register_stack(st);
  Object *bg = hc_new_background(st,"F");
  hc_new_card(st,bg,"U");
  return st;
}
static Object *b;
static void essai(const char *corps){
  char s[900]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",corps); hc_set_script(b,s); hc_send(b,"t");
}
int main(void){
  setbuf(stdout,NULL);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.load_stack=faux_load;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");
  b=hc_new_button(c,"B");hc_set_current_card(c);

  printf("=== random : la borne est convertie après vérification ===\n");
  essai("put random(1) & \"/\" & random(0)");
  essai("put 10^300 into m\n  put random(m)");
  essai("put 0-5 into m\n  put random(m)");

  printf("\n=== une neuvième « start using » ne doit rien charger ===\n");
  /* Huit d'abord : chacune est chargée par l'hôte, ce qui est normal. */
  for (int i = 1; i <= 8; i++) {
    char corps[64]; snprintf(corps,sizeof corps,"start using stack \"L%d\"",i);
    char s[128]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
    hc_set_script(b,s); hc_send(b,"t");
  }
  printf("   huit déclarées, load_stack appelée %d fois\n", g_charges);
  int avant = g_charges;
  essai("start using stack \"Neuvieme\"");
  printf("   load_stack appelée %d fois de plus (doit valoir 0)\n",
         g_charges - avant);
  /* Redéclarer une pile DÉJÀ en usage doit rester possible : c'est le geste
   * qui la remet en tête de la chaîne. */
  essai("start using stack \"L3\"\n  put the result & \"|\"");

  printf("\n=== arguments d'une fonction définie dans une pile ===\n");
  hc_set_script(st,
    "function somme a,b,c,d,e,f,g,h,i,j\n"
    "  return a+b+c+d+e+f+g+h+i+j\n"
    "end somme\n"
    "function compte\n"
    "  return the paramCount\n"
    "end compte\n");
  essai("put somme(1,2,3,4,5,6,7,8,9,10)");
  essai("put compte(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)");
  essai("put compte(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)");

  hc_unregister_stack(st);hc_free(st);return 0;}
