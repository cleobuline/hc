#include "hct_lex.h"
#include "hct_expr.h"
#include "hct_arbre.h"
#include "hct_bloc.h"
#include <stdio.h>
#include <string.h>
static void aff(const HctNoeud *n,int p){
  if(!n)return; char t[48]; hct_texte(&n->jeton,t,sizeof t);
  for(int i=0;i<p;i++)printf("  ");
  printf("%s «%s»", hct_genre_noeud_nom(n->genre), t);
  if(n->op)printf(" op=%s",n->op);
  printf(" nfils=%d\n",n->nfils);
  for(int i=0;i<n->nfils;i++)aff(n->fils[i],p+1);}
int main(int argc,char**argv){
  for(int k=1;k<argc;k++){
    HctLot lot; HctReserve r; memset(&r,0,sizeof r);
    int sain=hct_lex(argv[k],&lot);
    HctAnalyseur a; hct_analyseur_init(&a,&lot,&r);
    HctNoeud *n=hct_bloc_script(&a);
    printf("=== « %s »  (lex %s, %d erreur(s)) ===\n",argv[k],
           sain?"ok":"faute", a.nerreurs);
    aff(n,0);
    printf("\n");
  }
  return 0;}
