/* Les nombres qui ne tiennent pas dans le tampon, et les booléens trop laxistes.
 *
 * « put 1e100 + 0 » demande cent un chiffres en écriture décimale ; le tampon
 * en fait soixante-quatre. L'ancien code rendait « taille - 1 » et laissait
 * les soixante-deux premiers chiffres, c'est-à-dire 1e62 au lieu de 1e100 —
 * un nombre COMPLÈTEMENT FAUX, en silence. La bascule en notation
 * scientifique se décide maintenant sur la place disponible et non sur une
 * magnitude choisie à la main, si bien que tout ce qui tenait tient encore.
 *
 * Et « true patate » était vrai : hct_vers_bool ne regardait que le premier
 * mot, alors que le contrat dit que seuls true et false sont acceptés. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *corps){
  char s[400]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %-32s", corps); hc_set_script(b,s); hc_send(b,"t");
}
int main(void){
  setbuf(stdout,NULL);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");Object *c=hc_new_card(st,bg,"U");
  b=hc_new_button(c,"B");hc_set_current_card(c);

  printf("=== ce qui TIENT en décimal ne change pas ===\n");
  essai("put 1e14 + 0");
  essai("put 1e15 + 0");
  essai("put 1e16 + 0");
  essai("put 1e20 + 0");
  essai("put 3 + 4");
  essai("put 1.5 + 0.25");

  printf("\n=== ce qui ne tient pas bascule en scientifique ===\n");
  essai("put 1e100 + 0");
  essai("put 2.5e100 * 2");
  essai("put 1e308 + 0");
  essai("put 0-1e100 + 0");

  printf("\n=== l'infini reste l'infini, et LUI SEUL ===\n");
  essai("put 1e309 + 0");
  /* Le test de l'infini portait sur la borne 1e308, écrite à la main. Or le
   * plus grand double vaut à peu près 1,7976931348623157e308 : tout
   * l'intervalle entre les deux est FINI et parfaitement représentable, et
   * s'écrivait pourtant « INF ». isinf dit exactement ce qu'on voulait
   * savoir : les trois premiers ci-dessous ont un résultat et doivent le
   * montrer, le dernier déborde pour de bon et reste INF. */
  essai("put 1.5e308 + 0");
  essai("put 1e308 * 1.5");
  essai("put 0 - 1.5e308");
  essai("put 1.5e308 * 2");

  /* Les très petits restent à zéro : c'est le format par défaut à six
   * décimales, celui d'HyperCard. Ce n'est pas une troncature accidentelle,
   * et le changer serait une décision, pas un correctif. */
  printf("\n=== les très petits, au format par défaut ===\n");
  essai("put 1e-10 + 0");
  essai("put 0.25 + 0");

  printf("\n=== booléens : rien d'autre derrière ===\n");
  essai("put \"true\" is a boolean");
  essai("put \"false\" is a boolean");
  essai("put \"  true  \" is a boolean");
  essai("put \"true patate\" is a boolean");
  essai("put \"patate\" is a boolean");
  essai("if \"true\" then put \"pris\"");
  hc_unregister_stack(st);hc_free(st);return 0;}
