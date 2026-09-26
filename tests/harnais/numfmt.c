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
   *     gabarit "0.0"    put 10*sqrt(2)          ->  14.1
   *     gabarit "0.0"    put 1000*sin(z)         ->  21.8    z = pi/144
   *     gabarit "0.0"    put 1000*sin(0.021817)  ->  21.8
   *     gabarit "0.000"  put sqrt(2)             ->  1.414
   *
   * UNE SIXIÈME FIGURAIT ICI ET ELLE ÉTAIT FAUSSE. On lisait « gabarit 0.0,
   * put 1/3*3 -> 0.9, HyperCard ». C'est la valeur de HC, pas celle
   * d'HyperCard, relevée un soir avec les deux fenêtres côte à côte. Le
   * booléen l'a démentie sans appel :
   *
   *     gabarit "0.0"    put (1/3*3 = 1)         ->  true    HyperCard
   *                                                  false   HC
   *
   * La comparaison est immunisée contre l'affichage — « true » n'est pas un
   * nombre, le gabarit ne peut pas le toucher. Chez HyperCard « 1/3*3 » vaut
   * donc EXACTEMENT un, ce qui interdit que la division ait été arrondie.
   *
   * C'EST LA MÉTHODE QUI COMPTE ICI, plus que le chiffre : tant qu'on
   * AFFICHE, on mesure l'affichage autant que le calcul, et on ne peut pas
   * les séparer. En comparant, on voit le calcul seul.
   *
   * LA RÈGLE, RELEVÉE PAR QUATRE BOOLÉENS SOUS LE GABARIT « 0.0 » :
   *
   *                              HyperCard    HC
   *     (0.34*1 = 0.34)            true      false
   *     (1/3*3 = 1)                true      false
   *     (1/3 = 0.3)                false     true
   *     (sqrt(2)*1 = 1.4)          false     true
   *     (sqrt(2) into x, x = 1.4)  false     true
   *     (… puis 10*x = 14)         false     true
   *
   * HyperCard n'arrondit RIEN — ni les opérateurs, ni le rangement dans une
   * variable. HC arrondit tout. La règle est donc :
   *
   *     l'arithmétique garde toute sa précision, et le RANGEMENT aussi ;
   *     le gabarit n'intervient qu'à la SORTIE — affichage, concaténation.
   *
   * ET LA PREUVE EN GRANDEUR NATURE, dix points de la chaîne de tracé :
   *
   *     HyperCard        416.0 416.0 414.0 413.0 410.0 407.0 403.0 …
   *     HC               416.0 416.0 416.0 416.0 416.0 416.0 416.0 …
   *     HC sans gabarit  416   416   414   413   410   407   403   …
   *
   * Les nombres d'HyperCard sont EXACTEMENT ceux de HC sans gabarit, avec un
   * « .0 » ajouté : le gabarit n'a touché que la sortie. Chez HC la courbe
   * ne bouge pas pendant dix points — c'est le palier plat que l'on voit à
   * l'écran.
   *
   * CE QUI ÉTAIT FAUX DANS HC, et qui EST CORRIGÉ depuis : les opérateurs
   * mettaient en forme leur résultat, et le rangement conservait ce texte
   * arrondi. Les six booléens sont maintenant dans la colonne d'HyperCard, et
   * la chaîne de tracé donne ses dix valeurs au chiffre près. Les sections
   * ci-dessous mesurent ce que HC fait AUJOURD'HUI, avec la valeur
   * d'HyperCard en face à chaque fois.
   *
   * CE QUE HC FAISAIT DE DIFFÉRENT, et c'est corrigé : il mettait en forme le
   * RETOUR des fonctions. « 1000*sin(z) » rendait 0.0 au lieu de 21.8, parce
   * que sin(z) était écrasé à 0.0 AVANT la multiplication. Mille fois zéro
   * font zéro — ce n'était plus un arrondi, c'était la valeur perdue.
   *
   * CE QUE ÇA COÛTAIT. Un traceur polaire d'époque qui pose « set the
   * numberFormat to 0.0 » dans sa boucle sortait une rosace en marches
   * d'escalier de douze pixels là où HyperCard en trace une lisse — deux
   * captures côte à côte l'ont montré. cos(t) ne valait plus que 0.0, 0.1,
   * 0.2 : vingt-et-une valeurs pour tout un cercle. Et le dessin n'était que
   * la partie visible ; TOUT calcul scientifique sous gabarit étroit était
   * faussé, en silence.
   *
   * POURQUOI LA CORRECTION ÉVIDENTE ÉTAIT FAUSSE, et elle a dormi des
   * semaines dans un bloc-notes à ce titre : rendre hct_val_nombre au lieu de
   * hct_val_calcul dans les deux sites qui enveloppent math_un_arg corrige
   * bien « 10*sqrt(2) », mais casse l'affichage en échange — « put sqrt(2) »
   * montrerait 1.414214 là où HyperCard montre 1.414.
   *
   * CE QUI A DÉBLOQUÉ, ce sont CINQ MESURES prises en parallèle dans
   * Basilisk II et dans HC, avec une sonde qui accumule pour ne pas effacer
   * sa propre preuve :
   *
   *     set the numberFormat to 0.0 / put sqrt(2) into x
   *     A  put x                  1.4        l'affichage met en forme
   *     B  put 10*x               14.0       RANGER A FIGÉ la mise en forme
   *     C  put sqrt(2) & \"\"       1.4        la concaténation aussi
   *     D  put sqrt(2)            1.4        idem
   *     E  gabarit effacé, put x  1.4        x est bien du texte figé
   *
   * ON LES A CRUES IDENTIQUES DES DEUX CÔTÉS, et on en a conclu que HC était
   * déjà fidèle partout sauf sur un seul chemin : celui où le retour d'une
   * fonction part DIRECTEMENT dans un opérateur. C'ÉTAIT UNE CONCLUSION DE
   * TROP, et le B en est la cause : il dit que ranger fige, les booléens et
   * la chaîne de tracé disent que non. La dernière section du harnais expose
   * la contradiction et dit pourquoi on a suivi les booléens.
   *
   * D'OÙ LA CORRECTION : le texte reste EXACTEMENT celui d'avant — mis en
   * forme à la sortie — et le nombre non arrondi voyage à côté, dans
   * HctValeur.brut. Seuls le regardent l'arithmétique, la comparaison, les
   * arguments de fonction et les opérandes d'accumulation. Tout ce qui lit
   * .txt voit le même texte qu'hier, donc ne peut pas bouger, et le reste de
   * la suite l'a confirmé : sur 234 harnais, celui-ci est le seul qui ait
   * changé.
   *
   * IL A FALLU CINQ FRONTIÈRES, et chacune annulait la précédente : le retour
   * des fonctions, puis les opérateurs, puis le rangement dans une variable,
   * puis les arguments de fonction, puis les opérandes d'accumulation, et
   * enfin le LITTÉRAL NUMÉRIQUE lui-même. Tant que « 0.34 » écrit dans le
   * script n'était qu'un texte, « (0.34*1 = 0.34) » comparait un nombre à un
   * texte arrondi et la comparaison retombait sur le texte : quatre booléens
   * restaient faux alors que tout le reste était corrigé. Une correction de
   * précision qui s'arrête à une frontière est défaite par la suivante.
   *
   * Le contournement « 0.000 » plutôt que « 0.0 » n'est plus nécessaire. */
  essai("les OPERATEURS n'arrondissent plus leur resultat : conforme",
   "  set the numberFormat to \"0.0\"\n"
   "  put \"1/3*3         -> \" & (1/3*3) & \"   HyperCard : 1.0\"\n"
   "  put \"value(1/3*3)  -> \" & value(\"1/3*3\") & \"   HyperCard : 1.0\"");

  essai("LES QUATRE BOOLEENS, immunises contre l'affichage : conformes",
   "  set the numberFormat to \"0.0\"\n"
   "  put \"(0.34*1 = 0.34)   -> \" & (0.34*1 = 0.34) & \"   HyperCard : true\"\n"
   "  put \"(1/3*3 = 1)       -> \" & (1/3*3 = 1) & \"   HyperCard : true\"\n"
   "  put \"(1/3 = 0.3)       -> \" & (1/3 = 0.3) & \"   HyperCard : false\"\n"
   "  put \"(sqrt(2)*1 = 1.4) -> \" & (sqrt(2)*1 = 1.4) & \"   HyperCard : false\"");

  essai("RANGER dans une variable : ni l'un ni l'autre ne fige, conforme",
   "  set the numberFormat to \"0.0\"\n"
   "  put sqrt(2) into x\n"
   "  put \"(x = 1.4)         -> \" & (x = 1.4) & \"   HyperCard : false\"\n"
   "  put \"(10*x = 14)       -> \" & (10*x = 14) & \"   HyperCard : false\"");

  essai("UN TEXTE reste un texte : le gabarit ne le touche pas",
   "  set the numberFormat to \"0.0\"\n"
   "  put \"un litteral de texte -> \" & \"1.414214\" & \"   les deux : 1.414214\"\n"
   "  put \"un nombre calcule     -> \" & (1.414214*1) & \"   HyperCard : 1.4\"");

  essai("LA CHAINE DE TRACE, dix points — la rosace en chiffres",
   "  set the numberFormat to \"0.0\"\n"
   "  put empty into s\n"
   "  put 0 into t\n"
   "  repeat 10 times\n"
   "    put 8*cos(3*t) into r\n"
   "    put s & round(r*cos(t)*20+256) & \" \" into s\n"
   "    add pi/144 to t\n"
   "  end repeat\n"
   "  put s\n"
   "  put \"HyperCard : 416.0 416.0 414.0 413.0 410.0 407.0 403.0 398.0 392.0 386.0\"\n"
   "  put \"   (la courbe de HC ne bouge pas : c'est le palier plat a l'ecran)\"");

  essai("les FONCTIONS ne le suivent PAS : conforme a HyperCard",
   "  set the numberFormat to \"0.######\"\n"
   "  put pi/144 into z\n"
   "  set the numberFormat to \"0.0\"\n"
   "  put \"10*sqrt(2)    -> \" & (10*sqrt(2)) & \"   (HyperCard : 14.1)\"\n"
   "  put \"1000*sin(z)   -> \" & (1000*sin(z)) & \"   (HyperCard : 21.8)\"\n"
   "  put \"sin(z) seul   -> \" & sin(z) & \"   mis en forme : c'est une CONCATENATION\"");

  essai("le JUMEAU : « the sqrt of » doit suivre « sqrt() »",
   "  set the numberFormat to \"0.0\"\n"
   "  put \"10*(the sqrt of 2) -> \" & (10*(the sqrt of 2)) & \"   (HyperCard : 14.1)\"");

  essai("LA ROSACE, telle que la pile la calcule",
   "  set the numberFormat to \"0.0\"\n"
   "  put 0.3 into t\n"
   "  put 8*cos(2*t) into r\n"
   "  put \"r*cos(t),r*sin(t) -> \" & (r*cos(t)) & \",\" & (r*sin(t))\n"
   "  put \"   (avant, cos(t) etait arrondi a UNE decimale avant la\"\n"
   "  put \"    multiplication : vingt-et-une valeurs pour tout un\"\n"
   "  put \"    cercle, d'ou les marches de douze pixels)\"");

  essai("l'affichage direct, lui, est conforme",
   "  set the numberFormat to \"0.000\"\n"
   "  put \"sqrt(2)       -> \" & sqrt(2) & \"   (HyperCard : 1.414)\"");

  /* LA BATTERIE A-E, ET LA SEULE MESURE QUI RESTE EN CONTRADICTION.
   *
   * Elle ne figurait que dans un commentaire ; on l'inscrit ici pour qu'elle
   * soit surveillée, parce que le B a bougé avec ce chantier et qu'il faut
   * que ça se voie.
   *
   * RELEVÉ SOUS HYPERCARD :  A 1.4   B 14.0   C 1.4   D 1.4   E 1.4
   * HC AUJOURD'HUI :         A 1.4   B 14.1   C 1.4   D 1.4   E 1.4
   *
   * LE B EST INCOMPATIBLE AVEC DEUX AUTRES MESURES, et ce n'est pas une
   * nuance : si « put sqrt(2) into x » figeait le texte « 1.4 » sous le
   * gabarit 0.0, alors 10*x vaudrait quatorze EXACTEMENT, donc :
   *
   *     (10*x = 14)  serait  true      — relevé sous HyperCard : false
   *
   * et la chaîne de tracé ne pourrait pas donner ses valeurs. On l'a
   * vérifiée au crayon sur le quatrième point : avec un r figé à une
   * décimale on obtient 412, sans figeage 413. HyperCard donne 413.
   *
   * DEUX MESURES CONTRE UNE, dont une en grandeur nature sur dix points :
   * c'est le rangement qui ne fige pas, et « B 14.0 » a dû être lu dans la
   * fenêtre de HC — la même méprise que « 1/3*3 -> 0.9 » plus haut, prise le
   * même soir avec les deux fenêtres côte à côte.
   *
   * ON NE TRANCHE PAS DÉFINITIVEMENT POUR AUTANT : la référence enregistrée
   * ci-dessous est ce que HC fait, et la ligne du B porte la valeur d'époque
   * à côté. Si une nouvelle mesure dans Basilisk II redonne 14.0, c'est tout
   * le modèle du rangement qu'il faudra reprendre, pas cette ligne. */
  essai("la batterie A-E : le B attend une contre-mesure",
   "  set the numberFormat to \"0.0\"\n"
   "  put sqrt(2) into x\n"
   "  put \"A  x            -> \" & x & \"   HyperCard : 1.4\"\n"
   "  put \"B  10*x         -> \" & (10*x) & \"   releve d'epoque : 14.0 (douteux)\"\n"
   "  put \"C  sqrt(2) & _  -> \" & (sqrt(2) & \"\") & \"   HyperCard : 1.4\"\n"
   "  put \"D  sqrt(2)      -> \" & sqrt(2) & \"   HyperCard : 1.4\"\n"
   "  set the numberFormat to empty\n"
   "  put \"E  gabarit vide -> \" & x & \"   HyperCard : 1.4\"");

  hc_free(st);return 0;}
