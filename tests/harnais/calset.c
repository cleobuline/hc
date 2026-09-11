/* Quelles formes de « set » et de « convert » se relisent encore ? Une par
 * passe, chacune précédée de « debug raz » : le relevé nomme la coupable. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static int g_bilan; static char g_sortie[4096];
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;
  if(g_bilan) snprintf(g_sortie+strlen(g_sortie),sizeof g_sortie-strlen(g_sortie),"%s\n",t);}
static const char *mon_get(const char*n){
  static const char *C[]={"cursor","ticks","tool","editBkgnd","textStyle",NULL};
  for(int i=0;C[i];i++) if(!strcasecmp(n,C[i])) return "<hote>";
  return NULL;}
static void mon_set(const char*n,const char*v){(void)n;(void)v;}
static Object *f;
static void essai(const char *ligne){
  char s[1024];
  snprintf(s,sizeof s,
    "on t\n  put \"lundi 7 septembre 2026\" into d\n  put \"a b c\" into me\n"
    "  debug raz\n  %s\n  debug bilan\nend t\n", ligne);
  hc_set_script(f,s);
  g_sortie[0]=0; g_bilan=1; hc_send(f,"t"); g_bilan=0;
  printf("── %s\n", ligne);
  const char *p=strstr(g_sortie,"exécute encore");
  if(p){p=strchr(p,'\n'); while(p){ p++; const char *e=strchr(p,'\n'); if(!e)break;
        printf("   %.*s\n",(int)(e-p),p); p=e; if(strstr(p,"\n—"))break;}}
  printf("\n");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_get=mon_get;h.global_set=mon_set;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  f=hc_new_field(c,"cal"); hc_set_current_card(c);
  essai("set cursor to watch");
  essai("set textStyle of word 1 of line 1 to 1 of me to plain");
  essai("set sharedText of me to true");
  essai("set the script of me to \"on zz\\nend zz\"");
  essai("convert d to dateItems");
  essai("convert the date to dateItems");
  essai("convert it to short date");
  hc_free(st);return 0;}
