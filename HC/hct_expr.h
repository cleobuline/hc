/* hct_expr.h — analyse des expressions HyperTalk.
 *
 * Descente récursive, une fonction par rang de priorité. Les dix rangs
 * suivent l'annexe E du guide Apple, du plus fort au plus faible :
 *
 *   1   ( )                         groupement
 *   2   -  not                      unaires
 *   3   ^                           puissance, associative à DROITE
 *   4   *  /  div  mod
 *   5   +  -
 *   6   &  &&                       concaténation
 *   7   >  <  <=  >=  ≤  ≥  contains  is in  is not in
 *       is a  is an  is not a  is not an  is within  is not within
 *   8   =  ≠  is  is not  <>
 *   9   and
 *   10  or
 *
 * Tous les rangs sont associatifs à gauche sauf ^, comme le précise le guide.
 *
 * L'analyse ne s'arrête pas à la première faute : un sous-arbre HCTN_ERREUR
 * est posé à sa place et l'on poursuit, pour pouvoir tout signaler d'un coup.
 */

#ifndef HCT_EXPR_H
#define HCT_EXPR_H

#include "hct_lex.h"
#include "hct_arbre.h"

typedef struct {
    const HctLot *lot;
    int           i;        /* position courante dans lot->jetons        */
    HctReserve   *reserve;
    int           nerreurs;
    /* Profondeur d'analyse d'une borne de morceau. Tant qu'elle est non
     * nulle, « of » n'est pas absorbé comme propriété : dans
     * « char 1 to colWidth of x », le « of » appartient au morceau, pas à
     * colWidth. Une parenthèse remet le compteur à zéro, puisqu'elle isole
     * ce qu'elle contient. */
    int           sans_of;

    /* Profondeur d'imbrication en cours. L'analyse étant récursive, une
     * expression assez profonde ferait déborder la pile C — mesuré vers
     * 10 000 parenthèses, et le programme MEURT sans message. Un script
     * humain n'y arrive jamais, mais un fichier corrompu, si : c'est donc
     * l'application entière qui tomberait. On refuse au-delà du plafond. */
    int           prof;

    /* Combien de « if » sont ouverts au-dessus de celui qu'on analyse.
     *
     * Sert à trancher une ambiguïté réelle de HyperTalk : après
     * « if C then <bloc> else <instruction> », le si est clos, et un « end if »
     * qui suivrait appartient au si ENGLOBANT. Il n'y a d'exception que
     * lorsqu'il n'y a pas d'englobant — personne ne peut alors le réclamer. */
    int           prof_si;
} HctAnalyseur;

void hct_analyseur_init(HctAnalyseur *a, const HctLot *lot, HctReserve *r);

/* Analyse une expression complète à partir de la position courante.
 * Rend toujours un nœud, éventuellement HCTN_ERREUR. */
HctNoeud *hct_expression(HctAnalyseur *a);

/* Utilitaire de test : analyse une chaîne entière comme une expression.
 * Rend le nœud racine ; `nerr` reçoit le nombre de fautes. */
HctNoeud *hct_analyse_texte(const char *src, HctLot *lot, HctReserve *r,
                            int *nerr);

#endif
