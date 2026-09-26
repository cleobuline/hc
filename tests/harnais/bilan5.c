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
    "stacks","sound","menus","version","destination","size","freeSize",
    "language",
    NULL};
  for(int i=0;n[i];i++) f(n[i]);

  /* QUATRE GLOBALES SORTIES DU DÉCOMPTE, ET C'EST UNE DÉCISION, pas un oubli.
   *
   * « the windows », « the programs », « the diskSpace », « the heapSpace » :
   * des globales de System 7 dont aucune pile utile ne dépend. Les Fenêtres
   * et les Programmes d'HyperCard nommaient des objets du Finder de 1991 ;
   * l'espace disque et l'espace tas répondaient à une machine qui comptait
   * ses kilo-octets.
   *
   * ELLES FIGURAIENT ICI COMME « TROU », ce qui gonflait le travail restant
   * sans rien apprendre. Un inventaire qui compte ce qu'on ne fera jamais
   * dit moins bien où l'on en est qu'un inventaire plus court.
   *
   * ON NE LES RETIRE PAS DU NOYAU pour autant : HCview.m en sert déjà trois,
   * chacune avec son équivalent macOS expliqué à sa place. Ce qui change est
   * ce que ce harnais COMPTE, pas ce que l'application SAIT.
   *
   * « the systemVersion » sort avec elles, pour une autre raison : son
   * numéro ne veut plus rien dire. Une pile qui testerait « systemVersion »
   * pour se croire sous System 6 ou 7 se tromperait bien plus sûrement en
   * recevant « 15.0 » qu'en ne recevant rien. */
  puts("\n── hors decompte : globales de System 7 sans usage moderne");
  puts("   (windows, programs, diskSpace, heapSpace, systemVersion --");
  puts("    decision, pas oubli : voir le commentaire de ce harnais)");

  hc_free(st);return 0;}
