#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static char g_ink[64] = "0,0,0";
static char g_back[64] = "255,255,255";
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
/* l'hôte imite ce que fait cocoa_global_set/get */
static void mon_set(const char *nom,const char *val){
  if(!strcasecmp(nom,"paintColor")||!strcasecmp(nom,"inkColor")||!strcasecmp(nom,"paintBackColor")){
    int rgb=hc_color_from_name(val);
    if(rgb==HC_COLOR_INHERIT){ printf("   [HÔTE] couleur incomprise « %s » — rien changé\n",val); return; }
    char *cible = !strcasecmp(nom,"paintBackColor") ? g_back : g_ink;
    snprintf(cible,64,"%d,%d,%d",(rgb>>16)&255,(rgb>>8)&255,rgb&255);
    printf("   [HÔTE] %-14s <- %s\n",nom,cible);
  }
}
static const char *mon_get(const char *nom){
  if(!strcasecmp(nom,"paintColor")||!strcasecmp(nom,"inkColor")) return g_ink;
  if(!strcasecmp(nom,"paintBackColor")) return g_back;
  return NULL;
}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");hc_set_current_card(c);
 hc_set_script(b,
  "on mouseUp\n"
  "  set the paintColor to \"vert\"\n"
  "  put \"relu : \" & the paintColor\n"
  "  set the paintColor to \"64,255,30\"\n"
  "  put \"relu : \" & the paintColor\n"
  "  set the paintColor to \"#FF8000\"\n"
  "  put \"relu : \" & the paintColor\n"
  "  set the paintBackColor to blue\n"
  "  put \"fond : \" & the paintBackColor\n"
  "  set the paintColor to \"turquoise\"\n"
  "  put \"apres l'erreur : \" & the paintColor\n"
  "  repeat with i = 1 to 3\n"
  "    set the paintColor to (i*80) & \",\" & (255-i*40) & \",0\"\n"
  "    put \"calculee : \" & the paintColor\n"
  "  end repeat\n"
  "  debug bilan\n"
  "end mouseUp\n");
 hc_send(b,"mouseUp");hc_free(st);return 0;}
