/* « return » est le caractère 13.
 *
 * HC range ses lignes avec l'octet 10 : « charToNum(return) » rendait 10,
 * « numToChar(13) » n'était pas « return », et un texte bâti avec lui ne
 * comptait qu'une ligne. MESURÉ DANS HYPERCARD (Basilisk II) par
 * l'utilisatrice le 13 novembre : « charToNum(return) » y rend 13.
 *
 * NON MESURÉ : « charToNum(linefeed) ». La constante est restée le même
 * caractère que « return », et rend donc 13 elle aussi. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");Object *b=hc_new_button(c,"B");
  Object *f=hc_new_field(c,"F1");hc_set_field_text(f,"un\ndeux");
  hc_set_current_card(c);
  hc_set_script(b,
    "on t\n"
    "  put charToNum(return) && charToNum(tab) && charToNum(space)\n"
    "  put numToChar(13) is return\n"
    "  put \"a\" & numToChar(13) & \"b\" into x\n"
    "  put the number of lines of x && line 2 of x\n"
    "  put charToNum(numToChar(13)) && charToNum(numToChar(10))\n"
    "  put charToNum(char 3 of cd fld 1)\n"
    "  put numToChar(10) is return\n"
    "  put charToNum(linefeed)\n"
    "  put charToNum(numToChar(65)) && charToNum(numToChar(233))\n"
    "end t\n");
  hc_send(b,"t");
  hc_free(st);return 0;}
