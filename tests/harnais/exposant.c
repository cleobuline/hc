/* La notation scientifique : « 1e3 », « 2.5E-7 », « 1e+9 ».
 *
 * HyperCard ne la connaissait pas. Le reste du noyau si : hct_est_nombre et
 * hct_vers_nombre passent par strtod, qui l'accepte. D'où TROIS réponses
 * différentes pour le même texte, mesurées avant correction :
 *
 *     "1e3" is a number   ->  true
 *     "1e3" + 0           ->  1000
 *     value("1e3")        ->  1        (le lexeur s'arrêtait au « e »)
 *     put 1e3             ->  erreur d'analyse
 *
 * Le lexeur était le seul à ne pas suivre. Il l'accepte maintenant, et les
 * quatre s'accordent. Rien ne pouvait en dépendre : c'était une erreur.
 *
 * Le « e » n'est avalé QUE s'il est suivi d'au moins un chiffre : sans cette
 * exigence, « put 1 + e » cesserait de marcher, alors qu'une variable nommée
 * « e » est parfaitement légale. C'est la moitié du test. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *corps){
  char s[400]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",corps); hc_set_script(b,s); hc_send(b,"t");
}
int main(void){
  setbuf(stdout,NULL);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");
  b=hc_new_button(c,"B");hc_set_current_card(c);

  printf("=== les quatre chemins disent la même chose ===\n");
  essai("put 1e3 is a number");
  essai("put \"1e3\" is a number");
  essai("put 1e3 + 0");
  essai("put \"1e3\" + 0");
  essai("put value(\"1e3\") + 0");

  printf("\n=== les formes acceptées ===\n");
  essai("put 1e3 + 0 & \"/\" & 1E3 + 0");
  essai("put 1e+9 + 0");
  essai("put 2.5e-3 + 0");
  essai("put 1e3 * 2 & \"/\" & 6e2 / 3");

  /* SANS ESPACES : c'est là que le lexeur doit trancher. Dans « 1e+3+1 », le
   * premier « + » appartient à l'exposant et le second est l'addition — la
   * règle « un chiffre doit suivre le signe » ne suffit pas à le dire, c'est
   * la consommation gloutonne de l'exposant qui le fait. */
  printf("\n=== collé, sans espaces ===\n");
  essai("put 1e3+1");
  essai("put 1e3+1e1");
  essai("put 1e+3+1");
  essai("put 1e-3+1");
  essai("put 2e3-1e3");
  essai("put 0-1e3+1");

  printf("\n=== un littéral se rend tel quel, comme 007 et 1.50 ===\n");
  essai("put 007 & \"/\" & 1.50 & \"/\" & 1e3");

  printf("\n=== ce que le « e » ne doit PAS avaler ===\n");
  essai("put 2 into e\n  put 1 + e");
  essai("put 5 into e2\n  put e2");
  essai("put 3 into e\n  put 2 * e");
  /* « 1e » sans chiffre derrière : le nombre 1, puis la variable e. */
  essai("put 7 into e\n  put 1 + e & \"/\" & e");

  printf("\n=== toujours refusé ===\n");
  essai("put 1.5.2");
  hc_unregister_stack(st);hc_free(st);return 0;}
