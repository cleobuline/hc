#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;if(k==HC_MSG)printf("   %s\n",t);else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
 static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
 Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
 Object *c=hc_new_card(st,bg,"U");Object *b=hc_new_button(c,"B");hc_set_current_card(c);
 FILE *f=fopen("/tmp/claude-0/-home-user-hc/0f5ea498-57a7-5535-bfbb-a6b014480ae0/scratchpad/arcenciel.txt","rb");
 static char src[16384]; size_t n=fread(src,1,sizeof src-1,f); src[n]='\0'; fclose(f);
 /* on remplace le mouseUp par un banc d'essai de la fonction spectre */
 static char essai[20000];
 snprintf(essai,sizeof essai,
   "on essai\n"
   "  repeat with b = 0 to 15\n"
   "    put b & \" -> \" & spectre(b/15)\n"
   "  end repeat\n"
   "end essai\n%s", strstr(src,"function spectre"));
 hc_set_script(b,essai);
 hc_send(b,"essai");
 hc_free(st);return 0;}
