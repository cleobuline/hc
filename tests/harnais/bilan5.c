/* Depuis que « the » ne ment plus, un trou se DÉCLARE : il lève « propriété
 * ou fonction inconnue ». La mesure est donc devenue directe. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static int g_trou; static char g_val[256];
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_ERR && strstr(t,"inconnue")) g_trou=1;
  else if(k==HC_MSG) snprintf(g_val,sizeof g_val,"%s",t);}
static void mon_set(const char*n,const char*v){(void)n;(void)v;}
static const char *mon_get(const char*n){
  static const char *COCOA[]={"brush","clickChunk","clickH","clickLine",
    "clickLoc","clickText","clickV","commandKey","filled","lineSize","mouse",
    "mouseClick","mouseH","mouseLine","mouseLoc","mouseV","optionKey",
    "pattern","screenRect","shiftKey","textAlign","textFont","textHeight",
    "textSize","textStyle","ticks","tool","foreColor","backColor",
    "foregroundColor","backgroundColor","paintColor","paintBackColor",
    "inkColor","cursor","editBkgnd",NULL};
  for(int i=0;COCOA[i];i++) if(!strcasecmp(n,COCOA[i])) return "<hote>";
  return NULL;}
static Object *b;
static void f(const char *nom){
  char s[512]; snprintf(s,sizeof s,"on t\n  put the %s\nend t\n",nom);
  hc_set_script(b,s); g_trou=0; g_val[0]=0; hc_send(b,"t");
  printf("   %s  the %-20s %s\n", g_trou?"TROU ":"servi", nom,
         g_trou?"":(strcmp(g_val,"<hote>")?g_val:"(par l'interface)"));
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");hc_set_current_card(c);
  puts("── témoin");
  f("zorglub");
  puts("\n── les trous du bilan de ce matin");
  const char *n[]={"numberFormat","lockScreen","lockMessages","userLevel",
    "blindTyping","powerKeys","dragSpeed","lockRecent","textArrows",
    "cursor","editBkgnd",
    "stacks","windows","programs","sound","menus","diskSpace","heapSpace",
    "systemVersion","version","destination","size","freeSize","language",
    NULL};
  for(int i=0;n[i];i++) f(n[i]);
  hc_free(st);return 0;}
