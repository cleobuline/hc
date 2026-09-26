/* LE GABARIT QUI SURVIT À SON GESTIONNAIRE, ET LA PILE QU'IL CASSE.
 *
 * Relevé sur une vraie pile. « Graph Maker 2.2 », ouverte dans HC après un
 * passage par un traceur polaire, ne dessinait plus aucune barre : les
 * étiquettes et les bordures étaient là, et rien entre les deux.
 *
 * LA CAUSE N'EST PAS CELLE QU'ON A CHERCHÉE D'ABORD. On a soupçonné les
 * motifs, puis les coordonnées refusées en silence, puis le chantier du
 * numberFormat lui-même — trois fausses pistes. Ce qui a tranché est une
 * mesure sur la VRAIE pile, pas un raisonnement : un doRows modifié pour
 * AFFICHER ce qu'il dessinerait, au lieu de le dessiner.
 *
 * Et le relevé portait sa propre réponse :
 *
 *     colWidth=0.0  frameHeight=261.0  maxLines=14.0  stepSize=18.0
 *
 * TOUT PORTAIT UN « .0 ». Le gabarit « 0.0 » était posé pendant tout le
 * tracé, alors que Graph Maker n'y touche jamais — pas une occurrence de
 * numberFormat dans les 121 ko du fichier. Il venait du traceur polaire, qui
 * fait « set the numberFormat to 0.0 » dans sa boucle, et qui l'avait laissé
 * derrière lui.
 *
 * LE MÉCANISME, ET IL EST BRUTAL. addIncrements calcule son diviseur ainsi :
 *
 *     put 10^(length(maxValue div 10)) into divisor
 *
 * length() d'un NOMBRE lit son texte MIS EN FORME. Le gabarit ajoute deux
 * caractères, length passe de 2 à 4, le diviseur de 100 à 10000, et colWidth
 * tombe à zéro. Les quatorze barres sont dessinées — larges de zéro pixel.
 *
 * MESURÉ CHEZ HYPERCARD, et c'est la partie qu'on n'attendait pas :
 *
 *     set the numberFormat to "0.0"
 *     put length(100 div 10)        ->  4     HyperCard ET HC
 *
 * et le gabarit posé par un gestionnaire est encore là dans le suivant, des
 * deux côtés. HC EST DONC FIDÈLE. Graph Maker se casserait de la même façon
 * sous HyperCard, si on y passait par le traceur polaire avant : la pile de
 * 1991 suppose un gabarit par défaut, et rien ne le lui garantit.
 *
 * CE HARNAIS NE GARDE DONC PAS UNE CORRECTION, IL GARDE UNE FIDÉLITÉ. Si un
 * jour quelqu'un trouve « length(100 div 10) = 4 » absurde et le « corrige »,
 * ces lignes diront d'où vient le 4 et qui l'a mesuré. C'est le genre de
 * comportement qu'on répare par erreur.
 *
 * RESTE UNE QUESTION OUVERTE, et elle est notée telle quelle : chez HC le
 * gabarit traverse aussi les PILES (§4). On n'a pas mesuré si HyperCard le
 * remet à zéro en ouvrant une pile. Si oui, l'écart est là et nulle part
 * ailleurs. */
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
  Object *st=hc_new_stack("A");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");hc_set_current_card(c);

  essai("1. length() d'un nombre, SANS gabarit",
   "  set the numberFormat to empty\n"
   "  put \"100 div 10        -> \" & (100 div 10) & \"        HyperCard : 10\"\n"
   "  put \"length(100 div 10)-> \" & length(100 div 10) & \"         HyperCard : 2\"");

  essai("2. LE MEME, sous le gabarit du traceur polaire",
   "  set the numberFormat to \"0.0\"\n"
   "  put \"100 div 10        -> \" & (100 div 10) & \"      HyperCard : 10.0\"\n"
   "  put \"length(100 div 10)-> \" & length(100 div 10) & \"         HyperCard : 4   <<< MESURE\"");

  essai("3. LE GABARIT SURVIT AU GESTIONNAIRE QUI L'A POSE",
   "  put \"relu dans un gestionnaire suivant : [\" & the numberFormat & \"]\"\n"
   "  put \"   HyperCard : 0.0 aussi — mesure au bouton\"");

  /* §4 — LA SEULE CHOSE QUI RESTE À MESURER. On l'inscrit pour ne pas croire
   * le dossier clos : chez HC le gabarit traverse les piles. Si HyperCard le
   * remet à zéro en ouvrant une pile, l'écart est là. */
  printf("── 4. et il traverse meme les PILES (A CONFIRMER chez HyperCard)\n");
  Object *st2=hc_new_stack("B");Object *bg2=hc_new_background(st2,"F2");
  Object *c2=hc_new_card(st2,bg2,"v");
  Object *b2=hc_new_button(c2,"B2");hc_set_current_card(c2);
  hc_set_script(b2,"on t\n"
    "  put \"dans une AUTRE pile : [\" & the numberFormat & \"]\"\n"
    "  put \"   (HyperCard : pas encore mesure)\"\n"
    "end t\n");
  hc_send(b2,"t");
  hc_set_current_card(c);

  essai("5. LE CALCUL DE GRAPH MAKER, tel qu'il est ecrit dans la pile",
   "  set the numberFormat to empty\n"
   "  put 100 into maxValue\n"
   "  put 10^(length(maxValue div 10)) into divisor\n"
   "  put \"sans gabarit : divisor = \" & divisor & \"   colWidth = \" & ((335 div 11)/divisor)\n"
   "  set the numberFormat to \"0.0\"\n"
   "  put 10^(length(maxValue div 10)) into divisor\n"
   "  put \"avec 0.0     : divisor = \" & divisor & \"  colWidth = \" & ((335 div 101)/divisor)\n"
   "  put \"   (colWidth nul : les quatorze barres sont larges de zero pixel)\"");

  hc_free(st);hc_free(st2);return 0;}
