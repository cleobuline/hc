#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG||k==HC_ERR)printf("   %s\n",t);}
int main(int argc,char**argv){
  if(argc<2)return 2;
  FILE *fp=fopen(argv[1],"rb");fseek(fp,0,SEEK_END);long n=ftell(fp);fseek(fp,0,SEEK_SET);
  char *src=malloc(n+1);fread(src,1,n,fp);src[n]=0;fclose(fp);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");Object *b=hc_new_button(c,"B");hc_set_current_card(c);
  hc_set_script(b,src);
  printf("analyse et exécution de 5000 « if » imbriqués...\n");
  hc_send(b,"t");
  printf("survécu\n"); hc_free(st);free(src);return 0;}
