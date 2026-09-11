/* Quels gestionnaires de ce script la v3 prend-elle, et lesquels laisse-t-elle
 * à l'ancien ? Le compteur le dit, un message à la fois. */
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
int main(int argc,char**argv){
  if(argc<2){fprintf(stderr,"usage: %s script.txt\n",argv[0]);return 2;}
  FILE *f=fopen(argv[1],"rb"); if(!f){perror(argv[1]);return 2;} fseek(f,0,SEEK_END); long n=ftell(f);
  fseek(f,0,SEEK_SET); char *src=malloc((size_t)n+1); fread(src,1,(size_t)n,f);
  src[n]=0; fclose(f);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  /* SUR LE FOND, comme dans la vraie pile : « bg fld "Data" » ne peut pas
     se résoudre si le champ est sur la carte. */
  hc_new_field(bg,"Data"); hc_new_field(bg,"Total"); hc_new_field(bg,"Percents");
  hc_new_field(bg,"Percent Total"); hc_new_field(bg,"Labels");
  hc_new_field(bg,"Labels Title"); hc_new_field(bg,"Graph Title");
  hc_new_field(bg,"Drew Data");
  hc_new_button(bg,"Frame"); hc_new_button(bg,"Legend");
  hc_new_button(bg,"growlegend"); hc_new_button(bg,"Total:");
  hc_new_button(bg,"Total Frame"); hc_new_button(bg,"GrowLegend");
  hc_set_current_card(c);
  hc_set_script(bg, src);
  const char *msgs[]={"drawChart","doPieChart","doLegend","updateTotals",
                      "openCard","closeCard","closeField","exitField",NULL};
  for(int i=0;msgs[i];i++){
    hc_v3_bilan_remise_a_zero();
    hc_send(bg,msgs[i]);
    printf("── %-14s ", msgs[i]);
    fflush(stdout);
    hc_v3_bilan();
    printf("\n");
  }
  hc_free(st);free(src);return 0;}
