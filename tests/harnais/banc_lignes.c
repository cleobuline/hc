/* Écrire dans la ligne i d'un champ qui grandit : le cas le plus coûteux du
 * chemin d'écriture par morceau. Chaque « put into line i » doit retrouver le
 * i-ème saut de ligne, donc parcourir ce qui précède.
 *
 * Chronomètre : sa sortie n'est pas comparée (voir lance.sh). Il est là pour
 * qu'une régression de complexité se voie — passer de linéaire à quadratique
 * ne casse aucun test, ça rend seulement la pile inutilisable. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
static double ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
  return t.tv_sec*1000.0+t.tv_nsec/1e6;}
static Object *b,*f;
static void banc(const char *titre,int n,const char *sens){
  char s[600];
  snprintf(s,sizeof s,
    "on t\n  lock screen\n  repeat with i = %s\n"
    "    put i into line i of card field \"test\"\n  end repeat\n"
    "  unlock screen\nend t\n", sens);
  hc_set_field_text(f,"");
  hc_set_script(b,s);
  double d0=ms(); hc_send(b,"t"); double d1=ms();
  int lignes=1; const char *p=hc_field_text(f);
  for(;*p;p++) if(*p=='\n') lignes++;
  printf("  %-26s %6d lignes  %8.1f ms  %6.1f µs/ligne\n",
         titre,lignes,d1-d0,(d1-d0)*1000.0/n);
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");
  f=hc_new_field(c,"test");b=hc_new_button(c,"B");hc_set_current_card(c);
  puts("── écriture par ligne dans un champ qui grandit");
  banc("1 000 en descendant", 1000, "1000 down to 1");
  banc("5 000 en descendant", 5000, "5000 down to 1");
  banc("10 000 en descendant",10000,"10000 down to 1");
  banc("10 000 en montant",  10000,"1 to 10000");
  hc_free(st);return 0;}
