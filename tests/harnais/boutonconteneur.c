/* Un bouton est un CONTENEUR, comme un champ.
 *
 * MESURÉ DANS HYPERCARD (Basilisk II) par l'utilisatrice le 10 novembre :
 * « put "un" into cd btn 1 » range « un » dans le contenu du bouton, et
 * « put cd button 1 » affiche « un ». HC refusait l'écriture (« ne sait pas
 * faire ») et rendait le NOM du bouton à la lecture ; seul « set the
 * contents of » passait. C'est par là qu'un bouton popup reçoit ses
 * articles.
 *
 * after, before, les morceaux et delete : non mesurés un par un, ils suivent
 * ce qu'un champ fait déjà. Le nom du bouton, lui, ne bouge pas. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");Object *b=hc_new_button(c,"B");hc_new_button(bg,"Fond");
  hc_set_current_card(c);
  hc_set_script(b,
    "on t\n"
    "  put \"[\" & cd btn 1 & \"]\"\n"
    "  put \"un\" into cd btn 1\n"
    "  put \"[\" & cd btn 1 & \"]\"\n"
    "  put \"[\" & the contents of cd btn 1 & \"]\" && the short name of cd btn 1\n"
    "  put return & \"deux\" after cd btn 1\n"
    "  put \"zéro\" & return before cd btn 1\n"
    "  put the number of lines of cd btn 1 && \"/\" && line 2 of cd btn 1\n"
    "  put \"X\" into line 2 of cd btn 1\n"
    "  delete line 1 of cd btn 1\n"
    "  put \"[\" & cd btn 1 & \"]\"\n"
    "  put \"moi\" into me\n"
    "  put \"[\" & me & \"]\" && the short name of me\n"
    "  put \"fond\" into bg btn \"Fond\"\n"
    "  put \"[\" & bg btn \"Fond\" & \"]\"\n"
    "  put empty into cd btn 1\n"
    "  put \"[\" & cd btn 1 & \"]\"\n"
    "end t\n");
  hc_send(b,"t");
  hc_free(st);return 0;}
