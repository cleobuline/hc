#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *titre,const char *corps){
  char s[2048]; snprintf(s,sizeof s,"on t\n%s\nend t\n",corps);
  hc_set_script(b,s); printf("── %s\n",titre); hc_send(b,"t");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");hc_set_current_card(c);

  essai("defaut : inchange",
   "  put 1/3\n  put 2+3\n  put 10/4\n  put the numberFormat & \"<- vide\"");
  essai("0.00 : deux decimales imposees",
   "  set the numberFormat to \"0.00\"\n"
   "  put 1/3\n  put 2+3\n  put 10/4\n  put 1/8");
  essai("#.## : deux decimales au plus, aucune imposee",
   "  set the numberFormat to \"#.##\"\n"
   "  put 1/3\n  put 2+3\n  put 10/4\n  put 1/8");
  essai("0.0## : une imposee, trois au plus",
   "  set the numberFormat to \"0.0##\"\n"
   "  put 1/3\n  put 2+3\n  put 10/4");
  essai("000 : partie entiere completee",
   "  set the numberFormat to \"000\"\n"
   "  put 3+4\n  put 0-7\n  put 1234+1");
  essai("relecture",
   "  set the numberFormat to \"0.00\"\n  put the numberFormat");
  essai("retour au defaut",
   "  set the numberFormat to \"\"\n  put 1/3\n  put the numberFormat & \"<- vide\"");
  essai("ce qui n'est PAS un calcul reste brut",
   "  set the numberFormat to \"0.00\"\n"
   "  put the length of \"abcde\"\n"
   "  put the number of cards\n"
   "  put offset(\"b\",\"abc\")\n"
   "  put \"-- indexes intacts :\"\n"
   "  repeat with i = 1 to 3\n    put char i of \"xyz\"\n  end repeat");
  essai("les fonctions de calcul suivent",
   "  set the numberFormat to \"0.000\"\n"
   "  put the sqrt of 2\n  put the round of 1.5\n  put the average of 1,2\n"
   "  put the abs of -3");
  /* LES COMMANDES D'ACCUMULATION NE SUIVENT PAS. CETTE SECTION DISAIT LE
   * CONTRAIRE, ET ELLE AVAIT TORT.
   *
   * Elle s'appelait « add / subtract suivent aussi » et attendait 6.00 puis
   * 1.50. C'était une EXTENSION : ayant établi que le gabarit s'applique aux
   * calculs, on l'avait étendu aux commandes qui en font, sans le mesurer.
   *
   * Mesuré depuis, sur la pile qui s'en sert. Le bouton de tracé polaire de
   * HypoGraph 0.91 pose « set the numberFormat to 0.0 » DANS sa boucle et y
   * fait « add theInt to t », theInt valant pi/144. Si le gabarit s'appliquait,
   * t vaudrait 0.0 à chaque tour et « repeat until t > 2*pi » ne finirait
   * jamais. Sous HyperCard dans Basilisk II, ce bouton trace sa courbe et
   * s'arrête : la boucle avance donc là-bas.
   *
   * LA FRONTIÈRE PASSE ENTRE LES OPÉRATEURS ET LES COMMANDES, pas entre
   * « calcul » et « autre chose ». Les sections précédentes de ce harnais
   * restent vraies mot pour mot — un opérateur MONTRE un résultat, une
   * commande d'accumulation COMPTE avec. Appliquer un format d'affichage à un
   * accumulateur détruit l'accumulation.
   *
   * On garde donc la section, avec ses valeurs retournées : c'est le même
   * test, et sa nouvelle référence dit ce que HyperCard fait. */
  essai("les commandes d'accumulation ne suivent PAS",
   "  set the numberFormat to \"0.00\"\n"
   "  put 5 into x\n  add 1 to x\n  put x\n  divide x by 4\n  put x\n"
   "  put \"-- mais l'operateur, lui, suit toujours :\"\n"
   "  put 5/4");
  /* CE QUE HYPERCARD FAIT VRAIMENT, ET CE QUE HC FAIT DE DIFFÉRENT.
   *
   * ATTENTION : LE BLOC QUI ÉTAIT ICI AFFIRMAIT QUE HC ÉTAIT FIDÈLE. IL AVAIT
   * TORT. Il s'appuyait sur trois relevés qui ne séparaient pas les cas, et il
   * a été écrit avant le quatrième, qui a tout retourné. On le remplace plutôt
   * que de le compléter : un commentaire qui décrit une croyance est pire que
   * pas de commentaire.
   *
   * CINQ RELEVÉS SOUS HYPERCARD, dans Basilisk II :
   *
   *     gabarit "0.0"    put 1/3*3               ->  0.9
   *     gabarit "0.0"    put 10*sqrt(2)          ->  14.1
   *     gabarit "0.0"    put 1000*sin(z)         ->  21.8    z = pi/144
   *     gabarit "0.0"    put 1000*sin(0.021817)  ->  21.8
   *     gabarit "0.000"  put sqrt(2)             ->  1.414
   *
   * LA RÈGLE qu'ils dessinent, et la seule qui les explique tous les cinq :
   *
   *     les OPÉRATEURS mettent en forme leur résultat ;
   *     les FONCTIONS ne le font jamais ;
   *     la conversion en TEXTE met en forme.
   *
   * CE QUE HC FAIT DE DIFFÉRENT, et c'est un défaut ouvert : il met en forme
   * le RETOUR des fonctions. « 1000*sin(z) » rend donc 0.0 au lieu de 21.8,
   * parce que sin(z) a été écrasé à 0.0 avant la multiplication.
   *
   * CE QUE ÇA COÛTE. Un traceur polaire d'époque qui pose « set the
   * numberFormat to 0.0 » dans sa boucle sort une rosace en marches
   * d'escalier de douze pixels là où HyperCard en trace une lisse — deux
   * captures côte à côte l'ont montré. Et le dessin n'est que la partie
   * visible : TOUT calcul scientifique sous gabarit étroit est faussé, en
   * silence.
   *
   * POURQUOI CE N'EST PAS CORRIGÉ ICI. La correction tient en deux lignes —
   * rendre hct_val_nombre au lieu de hct_val_calcul dans les deux sites qui
   * enveloppent math_un_arg — mais elle rend l'affichage faux en échange :
   * « put sqrt(2) » montrerait 1.414214 là où HyperCard montre 1.414. Les deux
   * sont vrais en même temps chez HyperCard parce que ses valeurs sont TYPÉES :
   * il garde le nombre entier et ne met en forme qu'à la conversion. HC n'a que
   * du texte, et une seule représentation ne peut pas servir les deux usages.
   *
   * La vraie correction est donc de typer les valeurs. En attendant, les trois
   * sections ci-dessous mesurent ce que HC fait AUJOURD'HUI — pas ce qu'il
   * devrait faire. Les lignes marquées « ÉCART » sont celles qui diffèrent de
   * HyperCard, avec sa valeur en face.
   *
   * Un contournement existe et marche : écrire « 0.000 » plutôt que « 0.0 »
   * dans la pile. C'est ce que fait la pile qui a servi de banc d'essai. */
  essai("les OPERATEURS suivent le gabarit : conforme a HyperCard",
   "  set the numberFormat to \"0.0\"\n"
   "  put \"1/3*3         -> \" & (1/3*3) & \"   (HyperCard : 0.9)\"\n"
   "  put \"value(1/3*3)  -> \" & value(\"1/3*3\") & \"   (HyperCard : 0.9)\"");

  essai("les FONCTIONS ne devraient PAS le suivre : ECART connu",
   "  set the numberFormat to \"0.######\"\n"
   "  put pi/144 into z\n"
   "  set the numberFormat to \"0.0\"\n"
   "  put \"10*sqrt(2)    -> \" & (10*sqrt(2)) & \"   ECART, HyperCard : 14.1\"\n"
   "  put \"1000*sin(z)   -> \" & (1000*sin(z)) & \"   ECART, HyperCard : 21.8\"\n"
   "  put \"sin(z) seul   -> \" & sin(z) & \"   ecrase avant la multiplication\"");

  essai("l'affichage direct, lui, est conforme",
   "  set the numberFormat to \"0.000\"\n"
   "  put \"sqrt(2)       -> \" & sqrt(2) & \"   (HyperCard : 1.414)\"");

  hc_free(st);return 0;}
