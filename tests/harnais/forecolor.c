#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static char g_ink[64]  = "0,0,0";
static char g_back[64] = "255,255,255";
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);else if(k==HC_ERR)printf("   [ERR] %s\n",t);}

/* Copie fidèle de hcv_quelle_couleur : si les deux tables divergent un jour,
 * ce harnais ne le verra pas, mais l'écart se lit d'un coup d'œil ici. */
static int quelle(const char *n){
  static const char *E[]={"foreColor","foregroundColor","paintColor","inkColor",NULL};
  static const char *F[]={"backColor","backgroundColor","paintBackColor",NULL};
  for(int i=0;E[i];i++) if(!strcasecmp(n,E[i])) return  1;
  for(int i=0;F[i];i++) if(!strcasecmp(n,F[i])) return -1;
  return 0;
}
static void mon_set(const char *nom,const char *val){
  if(!quelle(nom)) return;
  int a=255, rgb=hc_color_from_name_alpha(val,&a);
  if(rgb==HC_COLOR_INHERIT){ printf("   [HÔTE] couleur incomprise « %s » — rien changé\n",val); return; }
  char *cible = (quelle(nom)<0) ? g_back : g_ink;
  if(a>=255) snprintf(cible,64,"%d,%d,%d",(rgb>>16)&255,(rgb>>8)&255,rgb&255);
  else       snprintf(cible,64,"%d,%d,%d,%d",(rgb>>16)&255,(rgb>>8)&255,rgb&255,a);
  printf("   [HÔTE] %-16s <- %s\n",nom,cible);
}
static const char *mon_get(const char *nom){
  int q=quelle(nom); if(!q) return NULL;
  return (q<0)?g_back:g_ink;
}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");hc_set_current_card(c);
 hc_set_script(b,
  "on mouseUp\n"
  "  set the foreColor to red\n"
  "  put \"foreColor  = \" & the foreColor\n"
  "  put \"paintColor = \" & the paintColor    -- meme couleur, autre nom\n"
  "  put \"inkColor   = \" & the inkColor\n"
  "  set the backColor to \"navy\"\n"
  "  put \"backColor      = \" & the backColor\n"
  "  put \"backgroundColor= \" & the backgroundColor\n"
  "  put \"paintBackColor = \" & the paintBackColor\n"
  "  set the foregroundColor to \"0,128,255,90\"\n"
  "  put \"avec alpha : \" & the foreColor\n"
  "  set the FORECOLOR to \"gold\"   -- la casse ne compte pas\n"
  "  put \"casse : \" & the foreColor\n"
  "  put \"item 2 of the foreColor = \" & item 2 of the foreColor\n"
  "  -- une variable de pile nommee foreColor garde la priorite\n"
  "  put \"a moi\" into foreColor\n"
  "  put \"variable : \" & foreColor\n"
  "  debug bilan\n"
  "end mouseUp\n");
 hc_send(b,"mouseUp");hc_free(st);return 0;}
