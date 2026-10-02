#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void mon_menu(const char *item){printf("   [HÔTE] %s\n",item);}
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.do_menu=mon_menu;h.line=ma_ligne;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *a=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");hc_new_card(st,bg,"Trois");
 Object *b=hc_new_button(a,"B");hc_set_current_card(a);
 hc_set_script(b,"on mouseUp\n"
  "doMenu \"Next\"\n     put the name of this card\n"
  "doMenu \"Last\"\n     put the name of this card\n"
  "doMenu \"Prev\"\n     put the name of this card\n"
  "doMenu \"Previous\"\n put the name of this card\n"
  "doMenu \"First\"\n    put the name of this card\n"
  "doMenu \"Find...\"\n"
  "doMenu \"Clear Picture\"\n"
  "end mouseUp\n");
 hc_send(b,"mouseUp");

 /* LA PALETTE NAVIGATOR, mesurée dans HyperCard le 2 octobre : « the
  * commands of window "Navigator" » rend ces onze doMenu, dans cet ordre.
  * Le noyau sert Back, Home et les quatre flèches ; les cinq autres vont à
  * l'hôte, qui sert Find..., Message, Recent et Next window — Help reste
  * sans réponse, HC n'ayant pas de pile d'aide. Home mène à la pile
  * « Home » : il n'y en a pas ici, et la faute le dit au lieu du silence
  * d'avant. Chaque article joue dans son propre clic, depuis la carte Deux,
  * pour qu'une faute n'emporte pas les suivants. */
 puts("== la palette Navigator ==");
 static const char *NAV[]={"Back","Home","Help","Recent","First","Prev","Next",
                           "Last","Find...","Message","Next window",NULL};
 Object *deux=NULL;
 for(int i=0;i<st->nparts;i++) if(st->parts[i]->type==OBJ_CARD&&st->parts[i]->name&&!strcmp(st->parts[i]->name,"Deux")) deux=st->parts[i];
 for(int i=0;NAV[i];i++){
  char s[256];
  if(deux) hc_set_current_card(deux);
  printf(" doMenu \"%s\"\n",NAV[i]);
  snprintf(s,sizeof s,"on mouseUp\n doMenu \"%s\"\n put \"  -> \" & the short name of this card\nend mouseUp\n",NAV[i]);
  hc_set_script(b,s);hc_send(b,"mouseUp");
 }
 hc_free(st);return 0;}
