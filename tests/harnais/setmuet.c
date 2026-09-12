/* « set the » d'une propriete inconnue : se plaint-il, ou se tait-il ? */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static void mon_set(const char *n,const char *v){
  /* l'hote imite cocoa_global_set : il ignore ce qu'il ne connait pas */
  if(!strcasecmp(n,"brush")||!strcasecmp(n,"lineSize"))
    printf("   [HOTE] %s <- %s\n",n,v);
}
static const char *mon_get(const char *n){
  if(!strcasecmp(n,"brush")) return "5";
  if(!strcasecmp(n,"lineSize")) return "1";
  return NULL;}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");
  hc_set_current_card(c);
  hc_set_script(b,
   "on mouseUp\n"
   "  put \"1. une propriete qui existe :\"\n"
   "  set the brush to 4\n"
   "  put \"2. une qui n'existe pas :\"\n"
   "  set the brushSize to 4\n"
   "  put \"3. une completement inventee :\"\n"
   "  set the zorglub to 4\n"
   "  put \"4. et la LECTURE, corrigee ce matin :\"\n"
   "  put the brushSize\n"
   "  put \"5. (cette ligne ne doit pas paraitre)\"\n"
   "end mouseUp\n");
  hc_send(b,"mouseUp");hc_free(st);return 0;}
