/* Rangs de morceau démesurés.
 *
 * « put 10^300 into n » puis « put "z" into line n of field "x" » convertissait
 * un double hors bornes en int — comportement INDÉFINI, que le compilateur est
 * libre de traduire comme il veut — puis demandait quatre gigaoctets pour y
 * loger les lignes vides intermédiaires. UBSan le voyait, mais aucun test ne
 * lui donnait l'occasion.
 *
 * Le rang passe maintenant par hct_vers_rang, qui teste AVANT de convertir et
 * refuse au-delà de HCT_RANG_MAX. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b,*f;
static void essai(const char *titre,const char *corps){
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",titre);
  hc_set_field_text(f,"a\nb\nc");
  hc_set_script(b,s); hc_send(b,"t");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");
  f=hc_new_field(c,"x");b=hc_new_button(c,"B");hc_set_current_card(c);

  /* D'abord ce qui doit continuer de marcher. */
  essai("rang normal, lecture",  "put line 2 of field \"x\"");
  essai("rang normal, écriture", "put \"z\" into line 2 of field \"x\"\n  put field \"x\"");
  essai("extension raisonnable", "put \"z\" into line 6 of field \"x\"\n"
                                 "  put the number of lines of field \"x\"");
  essai("rang zéro",             "put line 0 of field \"x\" & \"|\"");
  essai("rang au-delà du texte", "put line 99 of field \"x\" & \"|\"");

  /* Puis ce qui doit être refusé. */
  essai("lecture, rang démesuré", "put 10^300 into n\n  put char n of \"abc\"");
  essai("écriture, rang démesuré","put 10^300 into n\n"
                                  "  put \"z\" into line n of field \"x\"");
  essai("suppression, démesuré",  "put 10^300 into n\n"
                                  "  delete line n of field \"x\"");
  essai("rang négatif démesuré",  "put 0-10^300 into n\n  put item n of \"a,b,c\"");
  essai("juste sous le plafond",  "put 16777216 into n\n  put char n of \"abc\" & \"|\"");
  essai("juste au-dessus",        "put 16777217 into n\n  put char n of \"abc\" & \"|\"");

  /* numToChar bornait aussi une conversion indéfinie. */
  essai("numToChar normal",   "put numToChar(65)");
  essai("numToChar démesuré", "put 10^300 into n\n  put \"[\" & numToChar(n) & \"]\"");

  /* param(n) : hors bornes vaut « pas de tel paramètre ». */
  essai("param démesuré", "put 10^300 into n\n  put \"[\" & param(n) & \"]\"");
  hc_free(st);return 0;}
