/* « sort … dateTime » trie par date.
 *
 * Il comparait ses clés comme des nombres, et une date n'en est pas un :
 * toutes les clés se valaient et le tri rendait l'ordre de départ, pour
 * « 12/25/99 » comme pour « March 3, 2001 ». La clé est maintenant
 * l'instant que la date désigne ; ce qui ne se lit pas comme une date passe
 * après. Les tris numérique et de texte sont rejoués à côté. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");Object *b=hc_new_button(c,"B");
  hc_set_current_card(c);
  hc_set_script(b,
    "on t\n"
    "  put \"12/25/99,1/1/90,6/15/95\" into v\n"
    "  sort items of v dateTime\n"
    "  put v\n"
    "  sort items of v descending dateTime\n"
    "  put v\n"
    "  put \"March 3, 2001;January 5, 2001;February 1, 2000\" into v\n"
    "  set the itemDelimiter to \";\"\n"
    "  sort items of v dateTime\n"
    "  put v\n"
    "  put \"6/15/95;pas une date;1/1/90;;12/25/99\" into v\n"
    "  sort items of v dateTime\n"
    "  put v\n"
    "  set the itemDelimiter to \",\"\n"
    "  put \"10,9,100\" into v\n"
    "  sort items of v numeric\n"
    "  put v\n"
    "  sort items of v\n"
    "  put v\n"
    "end t\n");
  hc_send(b,"t");
  hc_free(st);return 0;}
