/* Trois issues possibles, et il faut les distinguer :
 *   servi   — le noyau rend une valeur ;
 *   hote    — le noyau passe la main à l'hôte (Cocoa la sert, pas nous ici) ;
 *   TROU    — personne ne connaît le nom, et « the xxx » se dégrade en
 *             « xxx » sans rien signaler. C'est le cas qui compte. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static char g_out[2048];
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG) strncat(g_out,t,2000); }
static void mon_set(const char*n,const char*v){(void)n;(void)v;}
/* L'hôte répond à tout, par un marqueur : ce qui revient marqué a bien été
 * ROUTÉ vers Cocoa, donc le noyau connaît le nom. */
/* L'hôte ne répond que pour ce que Cocoa sert VRAIMENT (relevé dans
 * cocoa_global_get). Tout le reste, il le décline — comme il le ferait dans
 * l'application. Ce qui revient alors égal à son propre nom est un vrai trou. */
static const char *mon_get(const char*n){
  static const char *COCOA[]={"brush","clickChunk","clickH","clickLine",
    "clickLoc","clickText","clickV","commandKey","filled","lineSize","mouse",
    "mouseClick","mouseH","mouseLine","mouseLoc","mouseV","optionKey",
    "pattern","screenRect","shiftKey","textAlign","textFont","textHeight",
    "textSize","textStyle","ticks","tool","foreColor","backColor",
    "foregroundColor","backgroundColor","paintColor","paintBackColor",
    "inkColor",NULL};
  for(int i=0;COCOA[i];i++) if(!strcasecmp(n,COCOA[i])) return "<hote>";
  return NULL;}
static Object *b;
static void f(const char *nom){
  char s[512]; snprintf(s,sizeof s,"on t\n  put the %s\nend t\n",nom);
  hc_set_script(b,s); g_out[0]=0; hc_send(b,"t");
  const char *r=g_out;
  const char *etat = (strcasecmp(r,nom)==0)      ? "TROU "
                   : (strcmp(r,"<hote>")==0)     ? "hote "
                   : "servi";
  printf("   %s  the %-22s -> %.40s\n", etat, nom, r[0]?r:"(vide)");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");hc_set_current_card(c);
  puts("── témoin (doit dire TROU)");
  f("zorglub"); f("blurp");
  puts("\n── ce qu'HyperCard offrait");
  const char *n[]={"date","time","seconds","ticks","result","selection",
    "selectedText","selectedChunk","selectedField","selectedLine",
    "clickText","clickLoc","clickChunk","mouse","mouseLoc","mouseClick",
    "shiftKey","optionKey","commandKey","tool","screenRect","target",
    "paramCount","params","itemDelimiter","number of cards",
    "foreColor","pattern","brush","lineSize","textFont","textSize",
    "stacks","windows","programs","sound","menus","diskSpace","heapSpace",
    "systemVersion","version","destination","size","freeSize","language",
    "cursor","numberFormat","userLevel","blindTyping","powerKeys",
    "dragSpeed","lockRecent","lockScreen","lockMessages","editBkgnd",
    "scriptTextSize","scriptTextFont","suspended","messageWatcher",
    "variableWatcher","printMargins","printTextFont","idleRate",
    "environment","address","longWindowTitles","userModify","debugger",
    NULL};
  for(int i=0;n[i];i++) f(n[i]);
  hc_free(st);return 0;
}
