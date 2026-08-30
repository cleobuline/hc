#ifndef hciconedit_h
#define hciconedit_h

#import <Cocoa/Cocoa.h>
#include "hc_core.h"

/* Edition des icones — la moitie « gros bits » de la fenetre Icones.
 *
 * Le catalogue et le choix restent a IconGrid ; ce fichier ne s'occupe que de
 * MODIFIER. Les operations sont des fonctions C, la grille est une vue : rien
 * ici ne connait le panneau qui les assemble.
 *
 * Toutes les operations rafraichissent la copie de travail de HCicons. C'est
 * leur seul effet de bord invisible, et il est indispensable : sans lui
 * l'affichage resterait sur l'etat precedent sans que rien ne le signale.
 */

/* Grille d'edition, un pixel d'icone = une case de 8 points.
 * target/action sont prevenus APRES chaque modification, pour que le panneau
 * rafraichisse le catalogue et la pile. */
@interface HCFatBits : NSView
@property (assign) int      iconId;   /* icone editee ; 0 = aucune */
@property (assign) Object  *stack;
@property (assign) id       target;
@property (assign) SEL      action;
+ (CGFloat)side;                      /* cote de la vue, en points */
@end

/* ---- operations sur le catalogue de la pile ----
 * Celles qui rendent un int rendent le numero a selectionner ensuite, ou 0. */

/* Rend l'icone modifiable portant ce numero.
 *
 * HCICONS est const : une icone d'origine ne peut pas etre retouchee. On la
 * recopie donc dans la pile EN GARDANT SON NUMERO — le bouton qui la porte
 * continue de marcher, la version modifiee masque l'originale, et seulement
 * dans cette pile. C'est ce que faisait une pile HyperCard emportant sa
 * propre ressource ICON. */
struct StackIcon *hcicon_edit_editable(Object *stack, int id);

/* Un numero libre dans LES DEUX catalogues, celui de la pile et celui d'origine.
 *
 * Les identifiants d'Apple ne sont pas groupes : ils s'etalent de 128 a plus de
 * 32000, avec des trous partout — 1000, par exemple, est « Stack ». Aucun
 * plancher ne met donc a l'abri, il faut chercher. C'est pour cette raison que
 * la fonction est ici et non dans le noyau, qui ne voit que la pile. */
int  hcicon_edit_free_id(void);

int  hcicon_edit_new(Object *stack);
int  hcicon_edit_duplicate(Object *stack, int id);
void hcicon_edit_erase(Object *stack, int id);

/* Pivote l'icone d'un quart de tour, dans le sens des aiguilles. Quatre appels
 * ramenent donc au point de depart, sans perte : une grille carree tourne sans
 * qu'aucun pixel ne sorte. */
void hcicon_edit_rotate(Object *stack, int id);
void hcicon_edit_rename(Object *stack, int id, const char *name);

/* Combien de boutons de la pile portent ce numero. A montrer avant de
 * supprimer : ils garderont un numero mort et n'afficheront plus rien. */
int  hcicon_edit_users(Object *stack, int id);
void hcicon_edit_delete(Object *stack, int id);

/* Refait la copie de travail de HCicons a partir de la pile.
 * Les operations ci-dessus l'appellent deja. */
void hcicon_edit_sync(Object *stack);

/* Lie le catalogue a une pile, sans travail si c'est deja la bonne.
 *
 * A appeler au debut de chaque dessin. HCicons ne retient qu'UNE copie de
 * travail, alors que plusieurs piles peuvent etre ouvertes en meme temps :
 * lier au chargement ne suffirait pas, la seconde pile ouverte ecraserait le
 * catalogue de la premiere, qui afficherait alors de mauvaises icones. C'est
 * la fenetre en train de se dessiner qui doit imposer la sienne.
 *
 * Le raccourci se fait sur le POINTEUR de pile. Une pile fermee puis une
 * nouvelle allouee a la meme adresse passeraient donc au travers : appeler
 * hcicon_edit_sync(NULL) a la fermeture d'une pile pour rompre le lien. */
void hcicon_edit_bind(Object *stack);

#endif /* hciconedit_h */
