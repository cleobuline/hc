/* hc_interne.h — ce que les morceaux du noyau se prêtent entre eux.
 *
 * PAS UNE API. hc_core.h décrit ce que le noyau offre au MONDE : l'interface
 * Cocoa, les harnais, tout ce qui l'emploie de l'extérieur. Celui-ci décrit
 * ce que ses propres fichiers se passent sous la table, et il n'a aucune
 * raison d'être inclus ailleurs que dans HC/.
 *
 * POURQUOI IL EXISTE. hc_core.c faisait 15 700 lignes. Le découper oblige à
 * rendre visibles des fonctions qui étaient `static` — c'est le prix du
 * découpage en C, et il se paie en symboles exposés. On le paie donc AU PLUS
 * JUSTE : ce fichier doit rester lisible d'un trait, sans quoi il ne protège
 * plus rien. Chaque nom ajouté ici est un nom qui n'est plus privé ; si la
 * liste s'allonge au point qu'on ne la lise plus, c'est que la frontière
 * choisie était mauvaise.
 *
 * Mesuré avant de couper, en comptant pour chaque découpe possible le nombre
 * de symboles qu'elle obligerait à exposer :
 *
 *     le presse-papiers        704 lignes    7 symboles
 *     le modèle entier       2 324 lignes   17 symboles
 *     les dates                715 lignes   24 symboles
 *     les plages de style    1 022 lignes   40 symboles
 *     les propriétés         1 307 lignes   47 symboles
 *     le pont v3             6 796 lignes   86 symboles
 *
 * Le presse-papiers est le bloc le plus isolé du fichier, et de loin : c'est
 * pour ça qu'il part le premier.
 */
#ifndef HC_INTERNE_H
#define HC_INTERNE_H

#include "hc_core.h"

/* ---- ce que hc_core.c prête à hc_presse_papiers.c ---------------------- */

/* Duplique une chaîne, ou rend NULL pour NULL. */
char   *dupstr(const char *s);

/* Attache un objet à la fin des parts[] de son propriétaire. */
void    add_part(Object *owner, Object *o);

/* La pile qui contient cet objet — l'objet lui-même si c'en est une. */
Object *owning_stack(Object *o);

/* Un identifiant libre dans cette pile. La fabrique unique : tout objet neuf
 * ou collé passe par elle, sans quoi deux objets porteraient le même numéro. */
int     id_neuf(Object *pile);

/* Cet identifiant d'icône est-il celui d'une icône livrée avec HC ? Une
 * icône intégrée n'est pas transportée avec l'objet : elle existe partout. */
int     icon_id_is_builtin(int id);

/* Fait de la place dans une liste de plages de style. Rend 0 si la mémoire
 * manque. */
int     runs_room(struct RunList *rl, int need);

/* ---- ce que hc_core.c prête à hc_script.c ------------------------------ */

/* UNE ERREUR VERS L'HÔTE, sans exporter `emit`.
 *
 * hc_core.c a un `emit(HcLineKind, fmt, …)` qui sert partout ; hc_script.c
 * n'en emploie qu'une forme, HC_ERR, cinq fois. Exporter `emit` aurait mis un
 * nom de trois lettres — parmi les plus courants qui soient — dans l'espace
 * des symboles que l'éditeur de liens partage avec AppKit et le reste. Un nom
 * préfixé ne coûte rien et ne peut entrer en collision avec personne.
 *
 * L'attribut de format est là pour que gcc vérifie les arguments comme il le
 * ferait pour printf : sans lui, une erreur de format dans un message
 * d'erreur ne se verrait qu'au moment où ce message sort. */
void    hc_emet_erreur(const char *fmt, ...)
        __attribute__((format(printf, 1, 2)));

/* ---- ce que hc_script.c prête à hc_core.c ------------------------------ */

/* L'ARBRE D'UN SCRIPT, analysé une fois et gardé dans l'objet.
 *
 * Rend NULL si le script est vide, ou si l'analyse a échoué — auquel cas
 * elle a déjà été tentée et ne le sera plus : l'objet retient son échec.
 * Le type est opaque ici ; hc_script.c et le pont v3 incluent hct_arbre.h. */
struct HctNoeud;
const struct HctNoeud *script_arbre(Object *o);

/* ---- ce que hc_presse_papiers.c prête à hc_core.c ---------------------- */

/* UN OBJET MEURT : que le presse-papiers oublie ce qu'il en retenait.
 *
 * Il garde deux pointeurs qu'il ne possède PAS — le fond emprunté à la pile
 * d'origine, et cette pile — et qui resteraient pendants.
 *
 * Une fonction plutôt que deux variables partagées : oublie_objet_interne
 * les remettait à NULL lui-même, ce qui obligeait à sortir ces deux globales
 * de leur fichier. Le presse-papiers garde son état privé et expose un
 * VERBE ; c'est un symbole au lieu de deux, et personne d'autre ne peut
 * écrire dedans.
 *
 * Elle ne déréférence PAS `mort` : elle compare des adresses et remet à
 * zéro. C'est la même règle que oublie_objet_interne, qui l'appelle alors
 * que l'objet est déjà en cours de libération. */
void    hc_pp_oublie(Object *mort);

#endif
