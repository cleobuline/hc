/* Ce que la v3 sait faire, verbe par verbe, sans le filet. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static int g_err; static char g_dernier[256];
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_ERR){g_err=1;snprintf(g_dernier,sizeof g_dernier,"%s",t);}}
static Object *b;
static void v(const char *ligne){
  char s[600]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",ligne);
  hc_set_script(b,s); g_err=0; g_dernier[0]=0; hc_send(b,"t");
  printf("  %-6s %-44s %s\n", g_err?"MANQUE":"ok", ligne, g_err?g_dernier:"");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");
  Object *f=hc_new_field(c,"F"); hc_set_field_text(f,"a\nb\nc");
  b=hc_new_button(c,"B");hc_set_current_card(c);
  puts("── commandes sans gestionnaire v3 dédié (traitées par hct_exec, ou pas)");
  const char *L[]={
    "add 1 to x","subtract 1 from x","multiply x by 2","divide x by 2",
    "get 1+1","global g1, g2","local l1, l2","do \"put 1\"","pass t",
    "return 3","exit t","put 1 into x","next repeat",
    "dial \"555\"","edit script of me","help","palette \"z\"","picture \"z\"",
    "export paint to file \"z\"","import paint from file \"z\"",
    "request \"z\" from program \"z\"","reply \"z\"",
    "arrowKey \"left\"","tabKey","returnKey","enterKey","functionKey 1",
    "controlKey 1","commandKeyDown \"b\"","keyDown \"x\"",
    "enterInField","returnInField", NULL};
  for(int i=0;L[i];i++) v(L[i]);
  puts("\n── quelques formes d'expression");
  const char *E[]={
    "put the long date","put the seconds","put the ticks","put the paramCount",
    "put the selectedText","put the clickText","put the foundText",
    "put the target","put the id of me","put the owner of me",
    "put the number of chars of \"abc\"","put offset(\"b\",\"abc\")",
    "put there is a card \"Deux\"","put random(10) > 0",
    "put annuity(0.1,5)","put compound(0.1,5)","put numToChar(65)",
    "put \"a\" is in \"abc\"","put 3 is within \"1,1,5,5\"",
    "put the abbreviated date","put the heapSpace","put the windows",
    "put the programs","put the sound","put the freeSize","put the size of me",
    "put the destination","put the language","put the version", NULL};
  for(int i=0;E[i];i++) v(E[i]);
  hc_unregister_stack(st);hc_free(st);return 0;}
