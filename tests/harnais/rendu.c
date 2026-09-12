/* Un faux hôte qui DESSINE : il exécute le script d'arc-en-ciel et rend une
   image en caractères, pour vérifier la forme et l'ordre des couleurs. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define W 78
#define H 26
static char toile[H][W+1];
static int  ech_x, ech_y;          /* facteurs d'échelle carte -> toile */
static int  curR=0,curV=0,curB=0;
static int  nDrag=0;
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   [msg] %s\n",t);else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static const char *mon_get(const char *n){
  static char b[32];
  if(!strcasecmp(n,"paintColor")){ snprintf(b,32,"%d,%d,%d",curR,curV,curB); return b; }
  return NULL;
}
static void mon_set(const char *n,const char *v){
  if(strcasecmp(n,"paintColor")) return;
  int rgb=hc_color_from_name(v);
  if(rgb==HC_COLOR_INHERIT) { printf("   [HÔTE] couleur refusée : « %s »\n", v); return; }
  curR=(rgb>>16)&255; curV=(rgb>>8)&255; curB=rgb&255;
}
/* la lettre qui représente la teinte dominante */
static char lettre(void){
  if(curR>200&&curV<80&&curB<80) return 'R';        /* rouge   */
  if(curR>200&&curV>150&&curV<230) return 'o';      /* orange  */
  if(curR>200&&curV>200&&curB<80) return 'J';       /* jaune   */
  if(curR<120&&curV>150) return 'V';                /* vert    */
  if(curB>200&&curV<160) return 'B';                /* bleu    */
  if(curB>120&&curR>60&&curR<160) return 'i';       /* indigo  */
  if(curB>150&&curR>120) return 'v';                /* violet  */
  return '#';
}
static void trace(int x1,int y1,int x2,int y2,const char *m){
  (void)m; nDrag++;
  int n=abs(x2-x1)>abs(y2-y1)?abs(x2-x1):abs(y2-y1); if(n<1)n=1;
  for(int i=0;i<=n;i++){
    int x=x1+(x2-x1)*i/n, y=y1+(y2-y1)*i/n;
    int cx=x*W/ech_x, cy=y*H/ech_y;
    if(cx>=0&&cx<W&&cy>=0&&cy<H) toile[cy][cx]=lettre();
  }
}
int main(void){
  for(int y=0;y<H;y++){ memset(toile[y],' ',W); toile[y][W]='\0'; }
  static HcHost h; memset(&h,0,sizeof h);
  h.line=ma_ligne; h.global_set=mon_set; h.global_get=mon_get; h.drag=trace;
  hc_set_host(&h);
  Object *st=hc_new_stack("demo"); st->w=512; st->h=342;
  ech_x=512; ech_y=342;
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une"); Object *b=hc_new_button(c,"B");
  hc_set_current_card(c);
  FILE *f=fopen("arcenciel.txt","rb");
  if(!f){ printf("script introuvable\n"); return 1; }
  static char src[16384]; size_t n=fread(src,1,sizeof src-1,f); src[n]='\0'; fclose(f);
  hc_set_script(b,src);
  hc_send(b,"mouseUp");
  printf("\n   %d segments tracés\n\n", nDrag);
  for(int y=0;y<H;y++) printf("   %s\n",toile[y]);
  printf("\n   R=rouge o=orange J=jaune V=vert B=bleu i=indigo v=violet\n");
  hc_free(st); return 0;}
