#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k!=HC_TRACE)printf("   [%d] %s\n",(int)k,t);}
static void mon_set(const char *n,const char *v){
  if(!strcasecmp(n,"paintColor")){
    int rgb=hc_color_from_name(v);
    printf("   [HÔTE] « %-16s » -> %s\n", v,
      rgb==HC_COLOR_INHERIT ? "REFUSÉ" : "");
    if(rgb!=HC_COLOR_INHERIT) printf("                              %d,%d,%d\n",(rgb>>16)&255,(rgb>>8)&255,rgb&255);
  }
}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");hc_set_current_card(c);
 hc_set_script(b,
  "on mouseUp\n"
  "  local tR, tG\n"
  "  set the paintColor to \"255,0,0\"\n"
  "  set the paintColor to \"255,0,0,255\"\n"
  "  set the paintColor to \"255,0,0,128\"\n"
  "  set the paintColor to \"255,0,0,0\"\n"
  "end mouseUp\n");
 hc_send(b,"mouseUp");hc_free(st);return 0;}
