#include "hc_core.h"
#include <stdio.h>
#include <string.h>
/* Un hôte qui répond comme le fera Cocoa pour l'état de la machine. */
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;printf("%s\n",t);}
static const char *mon_get(const char*n){
  if(!strcasecmp(n,"diskSpace")) return "123456789";
  if(!strcasecmp(n,"systemVersion")) return "15.4.1";
  if(!strcasecmp(n,"heapSpace")) return "4194304";
  return NULL;}
int main(void){
 static HcHost h; memset(&h,0,sizeof h); h.line=ma_ligne; h.global_get=mon_get; hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c=hc_new_card(st,bg,"U");Object *b=hc_new_button(c,"B");hc_set_current_card(c);
 hc_set_script(b,"on mouseUp\n"
  /* freeSize et size sont des propriétés de la PILE chez HyperCard — sa
     taille de fichier et l'espace que les suppressions y ont laissé —, pas de
     la machine. Elles ne sont pas ici, et ne seront pas servies par l'hôte. */
  "put the diskSpace & \"|\" & the heapSpace & \"|\" & the systemVersion\n"
  "put the number of menus & \"|\" & the menus\n"
  "put the recent cards\n"
  "put the stacks\n"
  "put the id of this card & \"|\" & the short id of this card\n"
  "put the long id of me\n"
  "put charToNum(\"A\") & \"|\" & the length of \"abc\"\n"
  "end mouseUp\n");
 hc_send(b,"mouseUp");hc_free(st);return 0;}
