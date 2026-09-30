/* hc_importe.h — Bâtir une pile HC à partir d'une pile d'origine lue.
 *
 * LA SÉPARATION AVEC hc_origine.c EST VOULUE, et c'est la même raison qui fait
 * qu'un extracteur précède un importateur : hc_origine.c est un ANALYSEUR, il
 * n'a aucune opinion sur notre modèle et ne dépend pas de hc_core.h. Il se teste
 * donc seul, et une faute qu'il commet est une faute de lecture, jamais une
 * faute de traduction. Ce fichier-ci est un BÂTISSEUR : il ne touche pas un
 * octet du format d'Apple, et une faute qu'il commet est une faute de
 * traduction, jamais de lecture. Le jour où une pile arrive de travers, on sait
 * lequel des deux interroger.
 *
 * CE QUI N'EST PAS TRADUIT, et c'est la même liste que ce que hc_origine ne lit
 * pas : les icônes (elles vivent dans le RESOURCE fork, qui ne survit pas à une
 * copie ordinaire hors du Mac), les motifs, les réglages d'impression. Les
 * dessins (blocs BMAP) et les styles par plage (bloc STBL) SONT traduits
 * maintenant ; cet en-tête disait le contraire jusqu'à l'audit qui l'a
 * relu — un commentaire qui date ment aussi sûrement qu'un code faux.
 */
#ifndef HC_IMPORTE_H
#define HC_IMPORTE_H

#include "hc_core.h"
#include "hc_origine.h"

/* Rend une pile neuve, à libérer par hc_free, ou NULL si la mémoire manque —
 * y compris pour un DESSIN qui n'a pas pu être converti : une pile qui
 * s'importerait sans lui prétendrait être complète.
 * `nom` est celui qu'on veut donner à la pile ; le format d'origine n'en porte
 * pas — le nom d'une pile HyperCard était celui de son FICHIER. */
Object *hc_importe_pile(const HcOrigPile *orig, const char *nom);

#endif
