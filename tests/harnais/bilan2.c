/* Deuxième passe : une phrase comprise n'est pas une phrase qui MARCHE.
 * On pose, on relit, on compare. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static char g_sortie[4096];
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG||k==HC_ERR){ strncat(g_sortie,k==HC_ERR?"[ERR] ":"",4000);
                            strncat(g_sortie,t,4000); strncat(g_sortie,"\n",4000);} }
static void mon_set(const char*n,const char*v){(void)n;(void)v;}
static const char *mon_get(const char*n){(void)n;return NULL;}
static Object *b;
static const char *run(const char *corps){
  char s[2048]; snprintf(s,sizeof s,"on t\n%s\nend t\n",corps);
  hc_set_script(b,s); g_sortie[0]=0; hc_send(b,"t"); return g_sortie;
}
static void essai(const char *quoi,const char *corps,const char *attendu){
  const char *r=run(corps);
  char vu[256]; snprintf(vu,sizeof vu,"%s",r);
  for(char *p=vu;*p;p++) if(*p=='\n') *p=' ';
  int ok = attendu ? (strstr(r,attendu)!=NULL) : (r[0] && !strstr(r,"[ERR]"));
  printf("   %s  %-28s -> %s\n", ok?"marche ":"INERTE ", quoi, vu[0]?vu:"(vide)");
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");
  Object *f=hc_new_field(c,"F1");(void)f;hc_set_current_card(c);

  puts("\n── PROPRIETES GLOBALES : posees puis relues");
  essai("numberFormat","set the numberFormat to \"0.00\"\nput the numberFormat","0.00");
  essai("  -> effet reel","set the numberFormat to \"0.00\"\nput 1/3","0.33");
  essai("cursor","set the cursor to watch\nput the cursor",NULL);
  essai("userLevel","set the userLevel to 2\nput the userLevel","2");
  essai("blindTyping","set the blindTyping to true\nput the blindTyping","true");
  essai("powerKeys","set the powerKeys to true\nput the powerKeys","true");
  essai("dragSpeed","set the dragSpeed to 50\nput the dragSpeed","50");
  essai("lockRecent","set the lockRecent to true\nput the lockRecent","true");
  essai("language","put the language",NULL);
  essai("itemDelimiter","set the itemDelimiter to \"-\"\nput item 2 of \"a-b\"","b");

  puts("\n── PROPRIETES D'OBJET : posees puis relues");
  essai("autoTab","set the autoTab of card field 1 to true\nput the autoTab of card field 1","true");
  essai("lockText","set the lockText of card field 1 to true\nput the lockText of card field 1","true");
  essai("dontSearch","set the dontSearch of card field 1 to true\nput the dontSearch of card field 1","true");
  essai("sharedText","set the sharedText of card field 1 to true\nput the sharedText of card field 1","true");
  essai("wideMargins","set the wideMargins of card field 1 to true\nput the wideMargins of card field 1","true");
  essai("showLines","set the showLines of card field 1 to true\nput the showLines of card field 1","true");
  essai("multipleLines","set the multipleLines of card field 1 to true\nput the multipleLines of card field 1","true");
  essai("autoSelect","set the autoSelect of card field 1 to true\nput the autoSelect of card field 1","true");
  essai("titleWidth","set the titleWidth of me to 40\nput the titleWidth of me","40");
  essai("partNumber","put the partNumber of me",NULL);
  essai("family","set the family of me to 2\nput the family of me","2");

  puts("\n── FONCTIONS : rendent-elles quelque chose ?");
  essai("the stacks","put the stacks",NULL);
  essai("the windows","put the windows",NULL);
  essai("the programs","put the programs",NULL);
  essai("the sound","put the sound",NULL);
  essai("the menus","put the menus",NULL);
  essai("the diskSpace","put the diskSpace",NULL);
  essai("the heapSpace","put the heapSpace",NULL);
  essai("the systemVersion","put the systemVersion",NULL);
  essai("the version","put the version",NULL);
  essai("the destination","put the destination",NULL);
  essai("the size","put the size",NULL);
  essai("the freeSize","put the freeSize",NULL);
  hc_free(st);return 0;
}
