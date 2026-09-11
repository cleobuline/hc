/* Une ligne de champ plus longue que 4096 : survit-elle au cycle
 * sauvegarde → relecture ? */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  static char L1[9000], L2[9000];
  memset(L1,'A',8000); L1[8000]=0;
  memset(L2,'B',300);  L2[300]=0;
  char texte[20000];
  snprintf(texte,sizeof texte,"%s\n%s\n",L1,L2);

  Object *st=hc_new_stack("Longue");
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");
  Object *f=hc_new_field(c,"gros");
  hc_set_current_card(c);
  hc_set_field_text(f,texte);

  /* et un script d'une seule ligne très longue */
  char sc[9000]; int p=snprintf(sc,sizeof sc,"on t\n  put \"");
  memset(sc+p,'C',5000); p+=5000;
  snprintf(sc+p,sizeof sc-p,"\"\nend t\n");
  hc_set_script(st,sc);

  printf("avant  : champ %d octets (ligne 1 = %d), script %d octets\n",
         (int)strlen(hc_field_text(f)), (int)strcspn(hc_field_text(f),"\n"),
         (int)strlen(st->script));
  if (hc_save(st,"/tmp/hc_longue.stack")!=0){puts("sauvegarde KO");return 1;}
  hc_free(st);

  Object *r = hc_load("/tmp/hc_longue.stack");
  if(!r){puts("relecture KO");return 1;}
  Object *c2=NULL; for(int i=0;i<r->nparts;i++) if(r->parts[i]->type==OBJ_CARD){c2=r->parts[i];break;}
  hc_set_current_card(c2);
  Object *f2=NULL; for(int i=0;i<c2->nparts;i++) if(c2->parts[i]->type==OBJ_FIELD){f2=c2->parts[i];break;}
  const char *t2 = f2?hc_field_text(f2):"";
  printf("après  : champ %d octets (ligne 1 = %d), script %d octets\n",
         (int)strlen(t2),(int)strcspn(t2,"\n"),(int)(r->script?strlen(r->script):0));
  printf("verdict: %s\n", (strlen(t2)==strlen(texte)) ? "INTACT" : "*** TRONQUÉ ***");
  hc_free(r); return 0;}
