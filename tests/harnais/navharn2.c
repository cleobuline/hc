/* Le script de navigation de l'utilisatrice, sans interface. */
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_ERR)printf("[ERR] %s\n",t);
  else if(k==HC_MSG)printf("[msg] %s\n",t);}
int main(int argc,char**argv){
  if(argc<2){fprintf(stderr,"usage: navharn script.txt\n");return 2;}
  FILE *fp=fopen(argv[1],"rb");if(!fp){perror(argv[1]);return 2;}
  fseek(fp,0,SEEK_END);long n=ftell(fp);fseek(fp,0,SEEK_SET);
  char *src=malloc((size_t)n+1);fread(src,1,(size_t)n,fp);src[n]=0;fclose(fp);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("Nav");hc_register_stack(st);
  Object *bg=hc_new_background(st,"Fond");
  Object *c1=hc_new_card(st,bg,NULL);
  Object *c2=hc_new_card(st,bg,NULL);
  hc_new_card(st,bg,NULL);
  Object *f=hc_new_field(c1,"data2");
  Object *b=hc_new_button(c1,"nav");
  (void)c2;
  hc_set_current_card(c1);
  hc_set_script(b,src);
  hc_send(b,"mouseUp");
  printf("\n───── data2 ─────\n%s\n", hc_field_text(f));
  hc_unregister_stack(st);hc_free(st);free(src);return 0;}
