/* Une expression démesurée se REFUSE, elle ne fait plus planter.
 *
 * « 1+1+…+1 » à 50 000 termes, donné à value(), débordait la pile : l'arbre
 * d'une chaîne d'opérateurs est aussi profond qu'elle est longue. De même
 * 50 000 « char 1 of » enchaînés, dans l'analyseur lui-même. Les parenthèses
 * étaient bornées, pas les chaînes. La borne : 1 000 opérateurs par
 * expression, 400 « of » imbriqués. En deçà, tout calcule comme avant. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void v(const char *titre,const char *l){char s[900];
  snprintf(s,sizeof s,"on t\n  %s\nend t\n",l);
  printf("── %s\n",titre);hc_set_script(b,s);hc_send(b,"t");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");b=hc_new_button(c,"B");hc_set_current_card(c);
  v("900 additions : calculées",
    "put \"1\" into s\n  repeat 900 times\n  put \"+1\" after s\n  end repeat\n  put the value of s");
  v("60 000 additions : le texte rendu tel quel, pas de plantage",
    "put \"1\" into s\n  repeat 60000 times\n  put \"+1\" after s\n  end repeat\n  put the length of (the value of s)");
  v("60 000 concaténations par do : refusées",
    "put \"a\" into s\n  repeat 60000 times\n  put \" & a\" after s\n  end repeat\n  do \"put the length of (\" & s & \")\"\n  put \"après\"");
  v("300 « char 1 of » : calculés",
    "put \"x\" into s\n  repeat 300 times\n  put \"char 1 of \" before s\n  end repeat\n  put \"abc\" into x\n  do \"put \" & s");
  v("60 000 « char 1 of » : refusés",
    "put \"x\" into s\n  repeat 60000 times\n  put \"char 1 of \" before s\n  end repeat\n  put \"abc\" into x\n  do \"put \" & s\n  put \"après\"");
  v("deux expressions de 900 à la suite : le compte repart",
    "put \"1\" into s\n  repeat 900 times\n  put \"+1\" after s\n  end repeat\n  put (the value of s) + (the value of s)");
  hc_free(st);return 0;}
