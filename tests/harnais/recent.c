/* L'historique de navigation vu du menu Go.
 *
 * hc_recent_count / hc_recent_at l'énumèrent, hc_go_recent y saute. C'est ce
 * dont se sert l'article Recent, qui nomme les cartes visitées là où HyperCard
 * en montrait les vignettes. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *corps){
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  hc_set_script(b,s); hc_send(b,"t");
}
static void liste(const char *titre){
  printf("── %s\n", titre);
  int n = hc_recent_count();
  if (n <= 0) { printf("   (aucune)\n"); return; }
  for (int i = 0; i < n; i++) {
    Object *c = hc_recent_at(i);
    char buf[256]; buf[0] = '\0';
    if (c) hc_describe(c, buf, sizeof buf);
    printf("   %d%s %s\n", i, i ? " " : "*", buf);   /* * = carte courante */
  }
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c1=hc_new_card(st,bg,"Une");
  Object *c2=hc_new_card(st,bg,"Deux");
  Object *c3=hc_new_card(st,bg,"Trois");
  (void)c2;(void)c3;
  b=hc_new_button(c1,"B");
  hc_set_current_card(c1);

  liste("au départ");
  essai("go to card \"Deux\"");
  essai("go to card \"Trois\"");
  essai("go to card \"Une\"");
  liste("après trois navigations");

  /* Le menu Recent saute au rang choisi ; le rang 0 est la carte courante et
   * ne s'affiche pas. */
  printf("── saut au rang 2\n");
  printf("   hc_go_recent(2) = %d\n", hc_go_recent(2));
  essai("put the short name of this card");
  liste("après le saut");

  printf("── rang inexistant\n");
  printf("   hc_go_recent(99) = %d\n", hc_go_recent(99));

  printf("── the recent names\n");
  essai("put the recent names");
  hc_unregister_stack(st);hc_free(st);return 0;}
