/* Quelles commandes provoquent encore un recours vers l'ancien interprète ? */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static int g_recours;
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_INFO && strstr(t,"commande ")) g_recours=1;}
static Object *b;
static void essai(const char *ligne){
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",ligne);
  hc_set_script(b,s); hc_v3_bilan_remise_a_zero(); g_recours=0;
  hc_send(b,"t");
  /* le relevé des recours est dans la table : on le lit par le bilan */
  printf("%-44s", ligne);
  fflush(stdout);
  hc_v3_bilan();
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");hc_new_card(st,bg,"Deux");
  b=hc_new_button(c,"B");hc_new_field(c,"F1");hc_set_current_card(c);
  const char *cmds[]={
    "put 1 into x","get 1","add 1 to x","subtract 1 from x",
    "multiply x by 2","divide x by 2","do \"beep\"","global g",
    "beep","wait 1 ticks","go next card","choose browse tool",
    "hide card field 1","show card field 1","select empty",
    "set the itemDelimiter to \",\"","delete char 1 of x",
    "answer \"a\"","lock screen","unlock screen","push card","pop card",
    "sort cards by 1","print card","mark all cards","unmark all cards",
    "convert \"1/1/90\" to seconds","find \"z\"","click at 1,1",
    "drag from 1,1 to 2,2","type \"a\"","visual effect dissolve",
    "play \"boing\"","reset paint","create menu \"M\"","reset menuBar",
    "send \"t\" to me","start using stack \"z\"","stop using stack \"z\"",
    NULL};
  for(int i=0;cmds[i];i++) essai(cmds[i]);
  hc_free(st);return 0;}
