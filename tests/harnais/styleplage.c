/* La cible d'une écriture de style, forme par forme : on pose, puis on
 * relit caractère par caractère. Toute dérive d'un seul caractère se voit. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_ERR) printf("      %s\n",t);}
static const char *mon_get(const char*n){(void)n;return NULL;}
static void mon_set(const char*n,const char*v){(void)n;(void)v;}
static Object *f, *b;
static const char *TXT = "Sun Mon Tue\nWed Thu Fri\nSat Nil Duo\n";
static void essai(const char *ligne){
  char s[600];
  hc_set_field_text(f, TXT);
  snprintf(s,sizeof s,
    "on t\n  put 2 into d\n  put 3 into numLines\n  %s\nend t\n", ligne);
  hc_set_script(b,s);
  printf("── %s\n", ligne);
  hc_send(b,"t");
  /* relecture : le style de chaque caractère, en une ligne */
  const char *txt = hc_field_text(f);
  int n = (int)strlen(txt);
  char carte[256];
  for (int i=0;i<n && i<250;i++) carte[i] = txt[i]=='\n' ? '/' : '.';
  if (n>250) n=250;
  carte[n]=0;
  for (int r=0;r<hc_run_count(f);r++){
    int sp,ln,sty,sz; const char *fo;
    hc_run_attrs(f,r,&sp,&ln,&sty,&sz,&fo);
    for(int i=sp;i<sp+ln && i<n;i++)
      if (carte[i]!='/') carte[i] = sty? 'X' : (sz!=12? 'S' : (fo&&strcmp(fo,"Geneva")==0?'F':'.'));
  }
  printf("   texte : %s\n","Sun Mon Tue/Wed Thu Fri/Sat Nil Duo/");
  printf("   style : %s\n\n", carte);
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_get=mon_get;h.global_set=mon_set;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  f=hc_new_field(c,"cal"); b=hc_new_button(c,"B"); hc_set_current_card(c);
  essai("set the textStyle of word 2 of card field \"cal\" to bold");
  essai("set the textStyle of word 2 of line 2 of card field \"cal\" to bold");
  essai("set the textStyle of word 1 to 2 of line 3 of card field \"cal\" to bold");
  essai("set the textStyle of char 5 to 7 of card field \"cal\" to bold");
  essai("set the textStyle of line 2 of card field \"cal\" to bold");
  essai("set the textStyle of last word of card field \"cal\" to bold");
  essai("set the textStyle of first word of line 2 of card field \"cal\" to bold");
  essai("set the textStyle of middle word of card field \"cal\" to bold");
  essai("set the textStyle of word d of line 1 to numLines of card field \"cal\" to bold");
  essai("set the textStyle of word (d+1) of card field \"cal\" to bold,underline");
  essai("set the textStyle of item 2 of line 1 of card field \"cal\" to bold");
  essai("set the textSize of word 2 of card field \"cal\" to 18");
  essai("set the textFont of word 3 of line 1 of card field \"cal\" to \"Geneva\"");
  essai("set the textStyle of word 99 of card field \"cal\" to bold");
  essai("set the textStyle of word 2 of card field \"absent\" to bold");
  essai("set the rect of word 2 of card field \"cal\" to \"1,2,3,4\"");
  hc_free(st);return 0;}
