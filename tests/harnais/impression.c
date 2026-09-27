/* « print » : les bornes, qui étaient fausses dans les deux sens.
 *
 * La liste des cartes à imprimer tenait dans un tableau FIXE de 512 entrées.
 * Une pile de sept cents cartes en imprimait cinq cent douze, sans un mot.
 *
 * Et surtout : « print card 1 to 2147483647 » GELAIT l'application. Passé la
 * dernière carte, nth_card rend NULL, le compteur cesse d'augmenter, la garde
 * « np < 512 » reste donc vraie, et la boucle parcourt deux milliards
 * d'indices pour rien — avec, au bout, un débordement signé sur « i++ ».
 * Une faute de frappe suffisait.
 *
 * La liste est maintenant allouée à la taille de la pile, et les bornes sont
 * ramenées au nombre réel de cartes avant de boucler. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void mon_print(Object **cartes, int n){
  printf("   [hôte] %d carte(s)", n);
  if (n > 0) printf(", de « %s » à « %s »",
                    cartes[0]->name ? cartes[0]->name : "?",
                    cartes[n-1]->name ? cartes[n-1]->name : "?");
  printf("\n");
}
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *corps){
  char s[400]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",corps); hc_set_script(b,s); hc_send(b,"t");
}
int main(void){
  setbuf(stdout,NULL);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.print_cards=mon_print;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c1=hc_new_card(st,bg,"C001");
  for (int i=2;i<=700;i++){ char n[16]; snprintf(n,sizeof n,"C%03d",i);
                            hc_new_card(st,bg,n); }
  b=hc_new_button(c1,"B"); hc_set_current_card(c1);

  printf("=== une pile de 700 cartes, au-delà de l'ancien plafond de 512 ===\n");
  essai("print all cards");
  essai("print card 1 to 600");
  essai("print card 690 to 700");

  printf("\n=== les plages démesurées ne gèlent plus ===\n");
  essai("print card 1 to 2147483647");
  essai("print card 0 to 3");
  essai("print card 900 to 1000");
  essai("put 10^300 into n\n  print card 1 to n");

  printf("\n=== les marquées, au-delà de 512 aussi ===\n");
  essai("mark all cards\n  print marked cards");
  essai("unmark all cards\n  mark card 5\n  mark card 600\n  print marked cards");
  /* ─── TOUTES LES FORMES, RELEVÉES AVANT TOUT PORTAGE ──────────────────
   *
   * Le harnais n'en couvrait que trois — « all cards », les plages, les
   * marquées — et c'est exactement ce qui manque avant de toucher à la
   * commande. « print » lit encore sa ligne en texte : le relevé du corpus dit
   * « v3 relit print » seize fois, et le portage vers l'arbre est le genre de
   * changement qui déplace une forme non couverte sans qu'on le voie.
   *
   * Ces lignes sont donc enregistrées comme un ÉTAT, pas comme un idéal :
   * elles disent ce que la commande fait aujourd'hui, y compris là où elle
   * refuse. Le portage devra les laisser mot pour mot. Ce qui est refusé ici
   * et qui aurait dû marcher se corrigera après, et séparément — sinon on ne
   * saurait plus lequel des deux changements a bougé la sortie. */
  printf("\n=== TOUTES LES FORMES, relevées avant le portage vers l'arbre ===\n");
  essai("unmark all cards\n  go to card 1\n  print");
  essai("print this card");
  essai("print card");
  essai("print card 3");
  essai("print card \"C005\"");
  essai("print cd 7");
  essai("print stack");
  essai("print all");
  essai("print all cards");
  essai("print marked");
  essai("go to card 4\n  print");
  essai("go to card 1\n  print card 3 to 5");
  essai("print card \"C005\" to 8");
  essai("print card 99999");
  essai("print card \"PasLa\"");
  /* Les deux formes que la commande NE sait pas faire, inscrites comme telles :
   * une plage sans le mot « card », et l'impression d'un document. La seconde
   * est une vraie forme d'HyperCard — « print <document> with <application> » —
   * et le motif de commande la prévoit déjà par son « [with e] », mais
   * l'exécuteur n'en fait rien. C'est une porte annoncée et non percée ; elle
   * est nommée ici pour ne pas croire le chantier fini. */
  essai("print 2 to 7");
  essai("print \"doc.txt\" with \"TeachText\"");

  hc_unregister_stack(st);hc_free(st);return 0;}
