#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *titre,const char *corps){
  char s[2048]; snprintf(s,sizeof s,"on t\n%s\nend t\n",corps);
  hc_set_script(b,s); printf("── %s\n",titre); hc_send(b,"t");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");hc_set_current_card(c);

  essai("valeurs par defaut",
   "  put \"userLevel   \" & the userLevel\n"
   "  put \"dragSpeed   \" & the dragSpeed\n"
   "  put \"blindTyping \" & the blindTyping\n"
   "  put \"powerKeys   \" & the powerKeys\n"
   "  put \"lockRecent  \" & the lockRecent\n"
   "  put \"textArrows  \" & the textArrows");
  essai("poser puis relire",
   "  set the userLevel to 3\n  put the userLevel\n"
   "  set the dragSpeed to 120\n  put the dragSpeed\n"
   "  set the lockRecent to true\n  put the lockRecent\n"
   "  set the blindTyping to false\n  put the blindTyping");
  essai("les booleens acceptent les formes usuelles",
   "  set the powerKeys to true\n  put the powerKeys\n"
   "  set the powerKeys to 0\n  put the powerKeys\n"
   "  set the textArrows to \"true\"\n  put the textArrows");
  essai("hors bornes : refuse, et l'ancienne valeur tient",
   "  set the userLevel to 3\n"
   "  set the userLevel to 47\n"
   "  put \"reste a \" & the userLevel");
  essai("pas un nombre : refuse aussi",
   "  set the dragSpeed to \"vite\"\n  put \"reste a \" & the dragSpeed");
  essai("utilisables dans un test, ce qui est tout l'interet",
   "  set the userLevel to 2\n"
   "  if the userLevel < 3 then put \"   niveau bas : on n'edite pas\"\n"
   "  set the userLevel to 5\n"
   "  if the userLevel is 5 then put \"   niveau auteur\"\n"
   "  debug bilan");
  hc_free(st);return 0;}
