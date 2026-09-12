/* Pourquoi un appel de fonction DÉFINIE dans le script emprunte-t-il encore
 * l'ancien moteur ? Le calendrier d'HyperCard 2.x en appelle cinq. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;printf("%s\n",t);}
static const char *mon_get(const char*n){
  static const char *C[]={"cursor","ticks","tool","editBkgnd",NULL};
  for(int i=0;C[i];i++) if(!strcasecmp(n,C[i])) return "<hote>";
  return NULL;}
static void mon_set(const char*n,const char*v){(void)n;(void)v;}
static const char *SCRIPT =
"function calData\n"
"  return \"2026,9,11,0,0,0,6,field\"\n"
"end calData\n"
"\n"
"function monthNameData\n"
"  return \"January,February,March,April,May,June,July,August,September,October,November,December\"\n"
"end monthNameData\n"
"\n"
"function dayNameData\n"
"  return \"Sun Mon Tue Wed Thu Fri Sat,4\"\n"
"end dayNameData\n"
"\n"
"function spaces\n"
"  return \"                                        \"\n"
"end spaces\n"
"\n"
"on essai\n"
"  debug raz\n"
"  repeat with i = 1 to 3\n"
"    put item 1 to 2 of calData()\n"
"  end repeat\n"
"  put last item of dayNameData()\n"
"  put char 1 to 4 of spaces()\n"
"  put item 3 of monthNameData()\n"
"  debug bilan\n"
"end essai\n";
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_get=mon_get;h.global_set=mon_set;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  Object *f=hc_new_field(c,"cal");
  hc_set_current_card(c);
  hc_set_script(f,SCRIPT);
  hc_send(f,"essai");
  puts("\n===== DEUXIEME PASSE =====");
  hc_send(f,"essai");
  hc_free(st);return 0;}
