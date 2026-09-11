/* Le test décisif : « the zorglub » rend « zorglub ». Une fonction absente ne
 * lève donc rien — elle se dégrade en son propre nom. Toute réponse égale au
 * nom demandé est un TROU, pas une valeur. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static char g_out[2048];
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;
  strncat(g_out,t,2000); }
static void mon_set(const char*n,const char*v){(void)n;(void)v;}
static const char *mon_get(const char*n){(void)n;return NULL;}
static Object *b;
static const char *lit(const char *expr){
  char s[512]; snprintf(s,sizeof s,"on t\n  put %s\nend t\n",expr);
  hc_set_script(b,s); g_out[0]=0; hc_send(b,"t"); return g_out;
}
static void f(const char *nom){
  char expr[128]; snprintf(expr,sizeof expr,"the %s",nom);
  const char *r=lit(expr);
  int trou = (strcasecmp(r,nom)==0);
  printf("   %-8s the %-18s -> %.60s\n", trou?"TROU":"servi", nom, r[0]?r:"(vide)");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");hc_set_current_card(c);

  puts("── temoin");
  f("zorglub");
  puts("\n── fonctions du monde");
  const char *n[]={"date","time","seconds","ticks","long date","result",
    "selection","selectedText","selectedChunk","selectedField","selectedLine",
    "clickText","clickLoc","clickChunk","mouse","mouseLoc","mouseClick",
    "shiftKey","optionKey","commandKey","tool","screenRect","target",
    "paramCount","params","stacks","windows","programs","sound","menus",
    "diskSpace","heapSpace","systemVersion","version","destination","size",
    "freeSize","language","cursor","numberFormat","userLevel","blindTyping",
    "powerKeys","dragSpeed","lockRecent","lockScreen","lockMessages",
    "editBkgnd","itemDelimiter","number of cards","number of backgrounds",
    "foreColor","backColor","pattern","brush","lineSize","filled",
    "textFont","textSize","textStyle","textAlign","textHeight",NULL};
  for(int i=0;n[i];i++) f(n[i]);
  hc_free(st);return 0;
}
