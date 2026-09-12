/* Les cartes MARQUÉES, de bout en bout.
 *
 * Le marquage existait partout — « mark this card », « set the marked of card
 * 3 to true », « go next marked card », « the number of marked cards » — et il
 * s'enregistre avec la pile. Rien ne le vérifiait, et l'Info carte, qui est
 * l'endroit où on s'attend à le trouver, ne le montrait pas : marquer une
 * carte à la main demandait de passer par la boîte de messages.
 *
 * La case « Card Marked » écrit dans le même champ que tout le reste. Ce
 * harnais couvre ce champ-là ; la case elle-même est du Cocoa, hors de portée
 * d'ici. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
/* « print marked cards » est le troisième lecteur du marquage, après la
 * navigation et le comptage. Il passe par l'hôte, donc c'est l'hôte qui dit
 * ce qu'on lui a demandé d'imprimer — et c'est la seule façon de le vérifier
 * d'ici. Rien ne le couvrait : les harnais d'impression existants ne testent
 * que « print card », « print all cards » et « print card 1 to 3 ». */
static void mon_print(Object **cartes, int n){
  printf("   [hôte] imprimer %d carte(s) :", n);
  for (int i = 0; i < n; i++)
    printf(" %s", cartes[i]->name ? cartes[i]->name : "?");
  printf("\n");
}
static Object *b;
static void essai(const char *corps){
  char s[512]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",corps); hc_set_script(b,s); hc_send(b,"t");
}
static void etat(Object *st,const char *titre){
  printf("── %s\n",titre);
  int rang=0;
  for(int i=0;i<st->nparts;i++){
    Object *c=st->parts[i];
    if(c->type!=OBJ_CARD) continue;
    rang++;
    printf("   carte %d (%s) : %s\n", rang, c->name?c->name:"",
           c->marked ? "marquée" : "-");
  }
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.print_cards=mon_print;
  hc_set_host(&h);
  Object *st=hc_new_stack("M");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c1=hc_new_card(st,bg,"Une");
  hc_new_card(st,bg,"Deux");
  hc_new_card(st,bg,"Trois");
  b=hc_new_button(c1,"B");hc_set_current_card(c1);

  etat(st,"au départ");
  essai("put the marked of this card");
  essai("put the number of marked cards");

  essai("mark this card");
  essai("set the marked of card \"Trois\" to true");
  etat(st,"après marquage");
  essai("put the number of marked cards");

  essai("go to next marked card\n  put the short name of this card");
  essai("go to next marked card\n  put the short name of this card");
  essai("go to first marked card\n  put the short name of this card");
  essai("go to last marked card\n  put the short name of this card");
  essai("go to prev marked card\n  put the short name of this card");
  /* SANS le « to » : HyperTalk l'admet partout, et c'est la forme qu'on écrit
   * le plus souvent à la main. Rien ne la couvrait. */
  essai("go next marked card\n  put the short name of this card");
  essai("go first marked card\n  put the short name of this card");
  essai("go prev marked card\n  put the short name of this card");
  /* Les abréviations de « card », qu'un script d'époque emploie sans y
   * penser. marked_card_ref les accepte ; personne ne le vérifiait. */
  essai("go to next marked cd\n  put the short name of this card");
  essai("go next marked cds\n  put the short name of this card");

  essai("set the marked of card \"Trois\" to false");
  essai("put the number of marked cards");

  /* C'est ce que fait la case « Card Marked » : écrire le champ directement.
   * Le script doit le voir. */
  printf("── la case à cocher écrit c->marked directement\n");
  /* parts[0] est le FOND ; les cartes suivent. C'est « Deux » qu'on marque. */
  for (int i = 0; i < st->nparts; i++)
    if (st->parts[i]->type == OBJ_CARD && st->parts[i]->name &&
        !strcmp(st->parts[i]->name, "Deux")) st->parts[i]->marked = 1;
  essai("put the number of marked cards");
  essai("put the marked of card \"Deux\"");

  /* Le troisième lecteur du marquage : l'impression. */
  printf("── print marked cards\n");
  essai("print marked cards");
  essai("print marked card");
  essai("print all cards");
  essai("mark all cards\n  print marked cards");
  essai("unmark all cards\n  print marked cards");

  /* Et cela doit survivre à l'enregistrement. */
  printf("── enregistrement puis relecture\n");
  printf("   hc_save = %d (0 = réussi)\n", hc_save(st,"/tmp/hc_marq.stack"));
  Object *relu = hc_load("/tmp/hc_marq.stack");
  if (!relu) { printf("   relecture ÉCHOUÉE\n"); hc_free(st); return 1; }
  etat(relu,"la pile relue");
  hc_free(relu);
  hc_unregister_stack(st);hc_free(st);return 0;}
