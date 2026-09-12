/* set : la valeur vient de l'arbre, sans relecture — et rend la même chose. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static char g_pat[32]="1", g_cur[32]="arrow", g_fil[8]="false", g_fnt[32]="";
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static void mon_set(const char*n,const char*v){
  if(!strcasecmp(n,"pattern")) snprintf(g_pat,32,"%s",v);
  else if(!strcasecmp(n,"cursor")) snprintf(g_cur,32,"%s",v);
  else if(!strcasecmp(n,"filled")) snprintf(g_fil,8,"%s",v);
  else if(!strcasecmp(n,"textFont")) snprintf(g_fnt,32,"%s",v);}
static const char *mon_get(const char*n){
  if(!strcasecmp(n,"pattern")) return g_pat;
  if(!strcasecmp(n,"cursor")) return g_cur;
  if(!strcasecmp(n,"filled")) return g_fil;
  if(!strcasecmp(n,"textFont")) return g_fnt;
  return NULL;}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;
  h.global_set=mon_set;h.global_get=mon_get;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");
  hc_set_current_card(c);
  hc_set_script(st,"function getPattern n\n"
                   "  return item (n mod 14) + 1 of \"13,11,22,15,20,17\"\n"
                   "end getPattern\n");
  hc_set_script(b,
   "on mouseUp\n"
   "  set the pattern to 12\n              put \"pattern  = \" & the pattern\n"
   "  set cursor to watch\n                put \"cursor   = \" & the cursor\n"
   "  set filled to true\n                 put \"filled   = \" & the filled\n"
   "  set the textFont to geneva\n         put \"textFont = \" & the textFont\n"
   "  set the pattern to getPattern(3)\n   put \"calculee = \" & the pattern\n"
   "  put 4 into i\n"
   "  set the pattern to getPattern(i)+1\n put \"variable = \" & the pattern\n"
   "  set the numberFormat to \"0.00\"\n   put \"format   = \" & the numberFormat\n"
   "  set the numberFormat to empty\n"
   "  set the userLevel to 3\n             put \"userLevel= \" & the userLevel\n"
   "  set the itemDelimiter to \";\"\n     put \"item 2   = \" & item 2 of \"a;b\"\n"
   "  set the itemDelimiter to \",\"\n"
   "  debug bilan\n"
   "end mouseUp\n");
  hc_v3_bilan_remise_a_zero();
  hc_send(b,"mouseUp");
  hc_free(st);return 0;}
