/* hc_wasm.h — les modules d'une pile : des XCMD et des XFCN en WebAssembly.
 *
 * Décidé par l'utilisatrice le 15 octobre (CLAUDE.md, la quatrième pièce) ;
 * le pourquoi et les mesures : docs/mesures/xcmd.txt ; le kit pour en
 * écrire : xcmd/.
 *
 * Un MODULE est un fichier .wasm rangé DANS la pile, sous un nom. Il apporte
 * des externes, qu'il exporte sous « XCMD <nom> » et « XFCN <nom> ». Il
 * tourne enfermé dans WAMR : il ne voit que sa mémoire et les rappels que HC
 * lui tend (hc_xcmd.h, côté module). Le noyau garde les OCTETS tels qu'on
 * les lui a donnés, pour les réécrire à l'identique ; l'exécution, elle, se
 * crée au premier appel et se jette après une faute.
 *
 * Les appels rendent :
 *    1  le module avait cet externe, et il a fini ;
 *    0  aucun module de la pile ne l'a : la recherche continue ;
 *   -1  il l'avait, et il a échoué : `faute` dit pourquoi, en français. */

#ifndef HC_WASM_H
#define HC_WASM_H

#include <stddef.h>
#include "hc_core.h"

/* Un module plus gros est refusé à l'ajout : 8 Mo, quand bonjour fait 2,4 Ko
 * et le moteur de MAC•CAM, en 68000, 24 Ko. */
#define HC_MODULE_OCTETS_MAX (8 * 1024 * 1024)
/* Seize paramètres au plus : le bloc de HyperCard en a seize places. C'est
 * UN de plus que les gestionnaires de HyperTalk (HC_ARGS_MAX, hc_core.c). */
#define HC_MODULE_PARAMS_MAX 16
/* La mémoire qu'un module peut prendre, en pages de 64 Ko : 64 Mo. */
#define HC_MODULE_PAGES_MAX  1024

/* ---- les modules d'une pile ---- */

/* Ajoute (ou remplace, à nom égal) un module. Il est VÉRIFIÉ d'abord : un
 * module que WAMR refuse, qui importe autre chose que les rappels de HC, ou
 * qui n'apporte aucun externe, n'entre pas — `pourquoi` dit la raison, et la
 * pile reste telle qu'elle était. Rend 0, ou -1. */
int  hc_module_ajoute(Object *pile, const char *nom,
                      const unsigned char *octets, size_t n,
                      char *pourquoi, size_t npourquoi);

/* Le même, SANS vérification et sans refus : pour la lecture d'un fichier,
 * qui doit garder un module même si WAMR, demain, le refusait. Il échouera
 * alors à son premier appel, en le disant. Rend 0, ou -1 (mémoire). */
int  hc_module_pose(Object *pile, const char *nom,
                    const unsigned char *octets, size_t n);

int  hc_module_retire(Object *pile, const char *nom);   /* 1 s'il y était */
int  hc_module_nombre(const Object *pile);
const char *hc_module_nom(const Object *pile, int i);
const unsigned char *hc_module_octets(const Object *pile, int i, size_t *n);

/* Les externes du module i, un par ligne : « XCMD bonjour », « XFCN
 * bonjour ». Rend leur nombre, ou -1 si le module ne se lit pas. */
int  hc_module_externes(const Object *pile, int i, char *liste, size_t taille);

/* Libère les modules d'une pile, avec la pile. */
void hc_modules_libere(Object *pile);

/* ---- l'appel ---- */

/* Un XCMD de nom `nom` dans un module de `pile`, avec ses arguments déjà
 * évalués. *passe vaut 1 si le module a posé passFlag. Son returnValue va
 * dans `res` : chez HyperCard, celui d'un XCMD devient « the result ». */
int  hc_module_xcmd(Object *pile, const char *nom,
                    const char *const *argv, int argc, int *passe,
                    char *res, size_t nres, char *faute, size_t nfaute);

/* Un XFCN : sa valeur rendue va dans `res` (vide si le module n'en rend
 * pas). */
int  hc_module_xfcn(Object *pile, const char *nom,
                    const char *const *argv, int argc, int *passe,
                    char *res, size_t nres, char *faute, size_t nfaute);

/* LE DÉLAI. Un module qui dépasse ce temps, en secondes, est arrêté ; 0 :
 * jamais. Sert aux harnais.
 *
 * L'ARRÊT DEMANDÉ, lui, n'a pas d'entrée ici : un module calcule dans un
 * fil à part, et le fil de HC, qui l'attend, respire comme dans un « wait »
 * (hc_respire_module, hc_core.h). Cmd-. arrête donc un module par le chemin
 * de tous les scripts. */
void hc_module_delai(double secondes);

/* 1 si un module est en train de calculer. Du fil de HC seulement. */
int  hc_module_actif(void);

/* LES FENÊTRES DES MODULES — « NewXWindow » (xcmd/hc_xcmd.h).
 *
 * Le noyau les numérote, l'hôte les montre (les rappels xw_ de HcHost), et
 * les scripts les désignent par leur titre : « window "CAM" ». Une fenêtre
 * vit autant que l'instance de son module ; une faute, un arrêt, le module
 * retiré ou la pile fermée la ferment.
 *
 * hc_module_fenetre : ce que v3_fenetre_hote demande à l'hôte, demandé
 * d'abord ici, avec les mêmes mots — "existe", "montre", "cache", "ferme",
 * "lit", "pose" —, et "envoie" pour « send <valeur> to window ». "lit",
 * "pose" et "envoie" vont au module (xGetPropEvt, xSetPropEvt, xSendEvt) ;
 * ce qu'il passe, et ce qu'on lui demande pendant qu'il tourne, HC le sert
 * comme pour toute fenêtre. "ferme" lui envoie xCloseEvt, puis ferme.
 *
 * Rend -2 si aucune fenêtre de module ne porte ce nom ; sinon comme le
 * rappel `fenetre` — 1 fait, 0 introuvable, -1 propriété inconnue — et -3
 * si le module s'est arrêté, sur une faute dite dans `faute`, ou par Cmd-..
 *
 * hc_module_fenetre_fermee : l'utilisatrice l'a fermée par sa case. Le
 * module reçoit xCloseEvt, puis l'hôte la ferme (xw_ferme). Rend -1 si le
 * module s'est arrêté sur une faute en la recevant, dite dans `faute`. */
int  hc_module_fenetre(const char *nom, const char *quoi, const char *prop,
                       const char *valeur, char *out, int outlen,
                       char *faute, size_t nfaute);
int  hc_module_fenetre_fermee(int id, char *faute, size_t nfaute);

#endif
