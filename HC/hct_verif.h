/* hct_verif.h — vérification d'un script HyperTalk, sans l'exécuter.
 *
 * Deux niveaux, et la distinction compte :
 *
 *   ERREUR       le script ne peut pas s'exécuter tel quel — parenthèse non
 *                fermée, « end » manquant, morceau sans « of ». HyperCard
 *                refuserait aussi.
 *
 *   AVERTISSEMENT  le script est syntaxiquement valide mais quelque chose est
 *                suspect : un gestionnaire nommé « mouseDwon » ne se
 *                déclenchera jamais, une propriété inconnue ne rendra rien.
 *                Ce ne sont PAS des fautes : un gestionnaire peut porter le
 *                nom qu'on veut, et une propriété peut venir d'un XCMD. D'où
 *                un niveau séparé, qu'on peut ignorer.
 *
 * Chaque signalement porte sa ligne et sa colonne, en base 1, pour que
 * l'éditeur puisse placer le curseur exactement dessus.
 */

#ifndef HCT_VERIF_H
#define HCT_VERIF_H

#include "hct_bloc.h"

typedef enum { HCT_V_ERREUR = 0, HCT_V_AVERTISSEMENT } HctNiveau;

typedef struct {
    HctNiveau niveau;
    int       ligne, col;
    char      message[160];
    char      extrait[64];    /* le texte fautif, pour le montrer */
} HctSignalement;

typedef struct {
    HctSignalement *liste;
    int n, cap;
    int nerreurs, navertissements;
} HctRapport;

/* Vérifie `src`. Rend 0 si aucune ERREUR (des avertissements restent
 * possibles), 1 sinon. Le rapport doit être libéré par hct_rapport_libere. */
int  hct_verifie(const char *src, HctRapport *rap, int avec_avertissements);
void hct_rapport_libere(HctRapport *rap);

/* Rend le signalement le plus grave, ou NULL si le rapport est vide.
 * Sert à l'éditeur, qui place le curseur sur la première faute. */
const HctSignalement *hct_premier(const HctRapport *rap);

/* LES GESTIONNAIRES D'UN SCRIPT, dans l'ordre du texte : ce que listent les
 * menus « Handlers » et « Functions » de la fenêtre de script d'HyperCard
 * 2.4, que l'éditeur de HC reprend (HCview.m).
 *
 * `fonction` vaut 1 pour « function », 0 pour « on ». `ligne` est la ligne
 * du mot « on » ou « function », en base 1, comptée comme celle des
 * signalements : l'éditeur y place le curseur. Le nom est tronqué à 63
 * octets. Rend le nombre de gestionnaires écrits, au plus `max`.
 *
 * Comme l'exécuteur trouve un gestionnaire, et non par l'arbre : « on X »
 * ne compte que si une ligne « end X » le suit. L'arbre prenait une ligne de
 * bandeau commençant par « on » — les bandeaux d'Apple n'ont pas de « -- »
 * — pour un gestionnaire sans fin, qui avalait tout le reste du script. Un
 * gestionnaire dont le corps porte une faute reste listé : c'est là qu'on
 * veut aller. */
typedef struct {
    int  fonction;
    int  ligne;
    char nom[64];
} HctGestionnaire;

int hct_gestionnaires(const char *src, HctGestionnaire *out, int max);

/* Rédige le rapport en texte, une ligne par signalement. Rend le nombre
 * d'octets écrits. */
int hct_rapport_texte(const HctRapport *rap, char *out, int outlen);

#endif
