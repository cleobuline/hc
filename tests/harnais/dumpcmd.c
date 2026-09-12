#include <stdio.h>
#include <string.h>
#include "hct_cmd.h"
#include "hct_expr.h"
#include "hct_lex.h"
#include "hct_arbre.h"
static void aff(const HctNoeud *n,int p){
  if(!n)return; char t[64]; hct_texte(&n->jeton,t,sizeof t);
  for(int i=0;i<p;i++)printf("  ");
  printf("%s «%s»",hct_genre_noeud_nom(n->genre),t);
  if(n->op)printf(" op=%s",n->op);
  if(n->article)printf(" ARTICLE");
  if(n->genre==HCTN_OBJET)printf(" typeobj=%d des=%d",n->typeobj,n->designateur);
  printf(" nfils=%d\n",n->nfils);
  for(int i=0;i<n->nfils;i++)aff(n->fils[i],p+1);
}
int main(int argc,char**argv){
  for(int a=1;a<argc;a++){
    HctLot lot; HctReserve r; memset(&r,0,sizeof r);
    hct_lex(argv[a], &lot);
    HctAnalyseur an; hct_analyseur_init(&an,&lot,&r);
    printf("=== « %s » ===\n",argv[a]);
    HctNoeud *n=hct_instruction(&an);
    printf("erreurs=%d\n",an.nerreurs);
    aff(n,0); printf("\n");
    hct_reserve_libere(&r);
  }
  return 0;}
