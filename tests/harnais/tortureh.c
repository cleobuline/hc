/* Un hôte qui imite HCview pour les globales que Cocoa sert vraiment :
 * couleurs, curseur, editBkgnd, outil. Le reste vient du noyau. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static char g_ink[64]="0,0,0", g_back[64]="255,255,255";
static char g_cur[32]="arrow";
static int  g_editbg = 0;
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("%s\n",t); else if(k==HC_ERR)printf("[ERR] %s\n",t);}
static int quelle(const char *n){
  static const char *E[]={"foreColor","foregroundColor","paintColor","inkColor",NULL};
  static const char *F[]={"backColor","backgroundColor","paintBackColor",NULL};
  for(int i=0;E[i];i++) if(!strcasecmp(n,E[i])) return 1;
  for(int i=0;F[i];i++) if(!strcasecmp(n,F[i])) return -1;
  return 0;}
static void mon_set(const char *n,const char *v){
  if(!strcasecmp(n,"cursor")){
    if(!strcasecmp(v,"none")) snprintf(g_cur,32,"none");
    else if(!strcasecmp(v,"watch")||!strcasecmp(v,"busy")) snprintf(g_cur,32,"watch");
    else if(!strcasecmp(v,"ibeam")) snprintf(g_cur,32,"ibeam");
    else snprintf(g_cur,32,"arrow");
    return;}
  if(!strcasecmp(n,"editBkgnd")){ g_editbg=(!strcasecmp(v,"true")||!strcmp(v,"1")); return;}
  if(quelle(n)){
    int a=255,rgb=hc_color_from_name_alpha(v,&a);
    if(rgb==HC_COLOR_INHERIT) return;
    char *c=(quelle(n)<0)?g_back:g_ink;
    if(a>=255) snprintf(c,64,"%d,%d,%d",(rgb>>16)&255,(rgb>>8)&255,rgb&255);
    else       snprintf(c,64,"%d,%d,%d,%d",(rgb>>16)&255,(rgb>>8)&255,rgb&255,a);
  }}
static const char *mon_get(const char *n){
  if(!strcasecmp(n,"cursor")) return g_cur;
  if(!strcasecmp(n,"editBkgnd")) return g_editbg?"true":"false";
  if(!strcasecmp(n,"tool")) return "browse tool";
  int q=quelle(n); if(q) return (q<0)?g_back:g_ink;
  return NULL;}
int main(int argc,char**argv){
  if(argc<3){fprintf(stderr,"usage: %s bouton.txt pile.txt\n",argv[0]);return 2;}
  char *src=NULL,*srcp=NULL;
  for (int k=1;k<=2;k++){
    FILE *f=fopen(argv[k],"rb"); if(!f){perror(argv[k]);return 2;}
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    char *t=malloc((size_t)n+1); fread(t,1,(size_t)n,f); t[n]=0; fclose(f);
    if(k==1) src=t; else srcp=t;
  }
  static HcHost h;memset(&h,0,sizeof h);
  h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;
  hc_set_host(&h);
  Object *st=hc_new_stack("Demo");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  hc_new_field(c,"data2");
  Object *b=hc_new_button(c,"torture");
  hc_set_current_card(c);
  hc_set_script(st,srcp);
  hc_set_script(b,src);
  hc_send(b,"mouseUp");
  {   /* le detail vit dans le champ, pas dans la boite de message */
    for (int i = 0; i < c->nparts; i++) {
      Object *f = c->parts[i];
      if (f->type == OBJ_FIELD && f->name && !strcmp(f->name, "data2")) {
        printf("\n───── champ « data2 » ─────%s\n", f->contents ? f->contents : "(vide)");
        break;
      }
    }
  }
  hc_free(st); free(src); free(srcp); return 0;}
