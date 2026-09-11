#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static char g_ink[64]="0,0,0";
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static void mon_set(const char *n,const char *v){
  if(strcasecmp(n,"paintColor")) return;
  int a=255, rgb=hc_color_from_name_alpha(v,&a);
  if(rgb==HC_COLOR_INHERIT){ printf("   [HÔTE] refusé : « %s »\n",v); return; }
  if(a>=255) snprintf(g_ink,64,"%d,%d,%d",(rgb>>16)&255,(rgb>>8)&255,rgb&255);
  else       snprintf(g_ink,64,"%d,%d,%d,%d",(rgb>>16)&255,(rgb>>8)&255,rgb&255,a);
}
static const char *mon_get(const char *n){ return strcasecmp(n,"paintColor")?NULL:g_ink; }
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");hc_set_current_card(c);
 hc_set_script(b,
  "on mouseUp\n"
  "  local tR, tG, tB, tAlpha\n"
  "  put 128 into tAlpha\n"
  "  put 255 into tR\n  put 0 into tG\n  put 0 into tB\n"
  "  set the paintColor to tR & \",\" & tG & \",\" & tB & \",\" & tAlpha\n"
  "  put \"relu : \" & the paintColor\n"
  "  put \"item 4 : \" & item 4 of the paintColor\n"
  "  set the paintColor to \"vert\"\n"
  "  put \"opaque : \" & the paintColor\n"
  "  debug bilan\n"
  "end mouseUp\n");
 hc_send(b,"mouseUp");hc_free(st);return 0;}
