/* hct_chunk_supprime, comparée aux dix-huit réponses de l'ancien interprète. */
#include "hct_chunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int nko;
static void cas(const char *s, HctSorteChunk so, int n, int n2,
                const char *attendu, const char *quoi){
  HctValeur v = hct_chunk_supprime(s, so, n, n2, ',');
  int ok = v.txt && !strcmp(v.txt, attendu);
  if(!ok) nko++;
  printf("  %s  %-26s de [%s] -> [%s]%s\n", ok?"ok  ":"ECHEC", quoi, s,
         v.txt?v.txt:"(nul)", ok?"":"  attendu [");
  if(!ok) printf("        attendu [%s]\n", attendu);
  free(v.txt);
}
int main(void){
  cas("a,b,c",HCT_CH_ITEM,1,0,"b,c","delete item 1");
  cas("a,b,c",HCT_CH_ITEM,2,0,"a,c","delete item 2");
  cas("a,b,c",HCT_CH_ITEM,3,0,"a,b","delete item 3");
  cas("a",HCT_CH_ITEM,1,0,"","delete item 1");
  cas("a,b,c",HCT_CH_ITEM,1,2,"c","delete item 1 to 2");
  cas("a,b,c",HCT_CH_ITEM,2,3,"a","delete item 2 to 3");
  cas("a,,c",HCT_CH_ITEM,2,0,"a,c","delete item 2");
  cas("un deux trois",HCT_CH_WORD,1,0,"deux trois","delete word 1");
  cas("un deux trois",HCT_CH_WORD,2,0,"un trois","delete word 2");
  cas("un deux trois",HCT_CH_WORD,3,0,"un deux","delete word 3");
  cas("un  deux",HCT_CH_WORD,1,0," deux","delete word 1");
  cas("abcdef",HCT_CH_CHAR,1,0,"bcdef","delete char 1");
  cas("abcdef",HCT_CH_CHAR,3,0,"abdef","delete char 3");
  cas("abcdef",HCT_CH_CHAR,6,0,"abcde","delete char 6");
  cas("abcdef",HCT_CH_CHAR,2,4,"aef","delete char 2 to 4");
  cas("a,b,c",HCT_CH_ITEM,3,0,"a,b","delete last item");
  cas("un deux trois",HCT_CH_WORD,3,0,"un deux","delete last word");
  cas("a,b,c",HCT_CH_ITEM,9,0,"a,b,c","delete item 9 (hors limites)");
  cas("L1\nL2\nL3",HCT_CH_LINE,2,0,"L1\nL3","delete line 2");
  cas("L1\nL2",HCT_CH_LINE,1,0,"L2","delete line 1");
  cas("L1\nL2",HCT_CH_LINE,2,0,"L1","delete line 2");
  printf("\n  %s\n", nko ? "DES ÉCARTS" : "les 21 cas passent");
  return nko?1:0;}
