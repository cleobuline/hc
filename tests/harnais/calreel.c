/* Le calendrier d'HyperCard 2.x, dans son champ, avec son relevé. */
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("%s\n",t); else if(k==HC_ERR)printf("[ERR] %s\n",t);}
static char g_cur[32]="arrow";
static void mon_set(const char*n,const char*v){
  if(!strcasecmp(n,"cursor")) snprintf(g_cur,32,"%s",v);}
static const char *mon_get(const char*n){
  if(!strcasecmp(n,"cursor")) return g_cur;
  if(!strcasecmp(n,"tool")) return "browse tool";
  return NULL;}
int main(int argc,char**argv){
  if(argc<2){fprintf(stderr,"usage: calreel script.txt\n");return 2;}
  FILE *fp=fopen(argv[1],"rb"); if(!fp){perror(argv[1]);return 2;}
  fseek(fp,0,SEEK_END); long n=ftell(fp); fseek(fp,0,SEEK_SET);
  char *src=malloc((size_t)n+1); fread(src,1,(size_t)n,fp); src[n]=0; fclose(fp);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;
  hc_set_host(&h);
  Object *st=hc_new_stack("Cal");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  Object *f=hc_new_field(c,"cal"); hc_set_current_card(c);
  hc_set_script(f,src);
  hc_send(f,"essai");
  printf("\n───── le champ ─────\n%s\n", hc_field_text(f));
  printf("───── les plages de style ─────\n");
  for(int r=0;r<hc_run_count(f);r++){int sp,ln,sty,sz;const char *fo;
    hc_run_attrs(f,r,&sp,&ln,&sty,&sz,&fo);
    printf("  [%d..%d) style=%d\n",sp,sp+ln,sty);}
  hc_free(st); free(src); return 0;}
