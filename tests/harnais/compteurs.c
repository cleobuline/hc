#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
static Object *b, *c;
static void section(const char *titre){ printf("\n══ %s\n", titre); hc_v3_bilan_remise_a_zero(); }
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  c=hc_new_card(st,bg,"Une"); hc_new_card(st,bg,"Deux");
  b=hc_new_button(c,"B"); hc_new_field(c,"F1"); hc_set_current_card(c);

  section("un gestionnaire tout v3 : boucle, calcul, put");
  hc_set_script(b,"on mouseUp\n  put 0 into t\n  repeat with i = 1 to 50\n"
                  "    put t + i*2 into t\n  end repeat\n  put t into card field 1\n"
                  "end mouseUp\n");
  hc_send(b,"mouseUp"); hc_v3_bilan();

  section("la boite de message");
  hc_do("put 2+2");
  hc_do("go next card");
  hc_v3_bilan();

  section("un article de menu du noyau");
  hc_do_menu("Next");
  hc_v3_bilan();

  section("les commandes qui reevaluent du texte : set, sort, print");
  hc_set_script(b,"on mouseUp\n  set the numberFormat to \"0.00\"\n"
                  "  set the userLevel to 4\n  sort cards by the short name of me\n"
                  "end mouseUp\n");
  hc_send(b,"mouseUp"); hc_v3_bilan();

  hc_free(st);return 0;}
