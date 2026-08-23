/* hct_eval.h — évaluation d'un arbre HyperTalk.
 *
 * L'exécuteur ne connaît RIEN aux objets, aux champs ni à l'écran : tout ce
 * qui touche au monde extérieur passe par une structure de rappels, HctHote.
 * C'est ce qui rend la bibliothèque autonome et testable sans HC — un hôte
 * factice suffit aux essais, et hc_core.c fournira le vrai le jour de la
 * greffe.
 *
 * Les erreurs d'exécution ne remontent pas par des codes de retour disséminés
 * partout : elles sont posées dans le contexte, avec le NŒUD fautif. C'est ce
 * nœud qui porte la ligne et la colonne, et donc ce qui permettra de montrer
 * la faute à l'utilisateur là où elle est.
 */

#ifndef HCT_EVAL_H
#define HCT_EVAL_H

#include "hct_arbre.h"
#include "hct_val.h"

typedef struct HctContexte HctContexte;

/* --- l'hôte : tout ce que l'exécuteur ne sait pas faire lui-même --- */
typedef struct {
    void *donnees;    /* opaque, transmis tel quel à chaque rappel */

    /* Lire une variable. Rend 0 si elle n'existe pas — en HyperTalk une
     * variable jamais affectée vaut son propre nom, et c'est l'exécuteur qui
     * applique cette règle, pas l'hôte. */
    int (*lit_var)(void *d, const char *nom, HctValeur *out);
    int (*ecrit_var)(void *d, const char *nom, const char *val);

    /* Lire une propriété ou le contenu d'un objet désigné par un nœud
     * HCTN_OBJET déjà résolu par l'hôte. `objet` est ce que rend resout(). */
    void *(*resout)(void *d, const HctNoeud *ref, HctContexte *ctx);
    int   (*lit_objet)(void *d, void *objet, HctValeur *out);
    int   (*lit_prop)(void *d, void *objet, const char *prop, HctValeur *out);

    /* Fonctions que seul l'hôte peut calculer : the ticks, the mouse,
     * the date… Rend 0 si le nom est inconnu de l'hôte. */
    int (*fonction)(void *d, const char *nom, HctValeur *args, int nargs,
                    HctValeur *out);

    /* Recours pour les formes que l'évaluateur ne sait pas traiter seul —
     * aujourd'hui les références d'objets et les propriétés.
     *
     * L'hôte reçoit le NŒUD et rend une valeur. Il peut, s'il le veut,
     * reconstituer le texte source du sous-arbre et le confier à son propre
     * évaluateur : c'est ce que fait HC pendant la transition, où hc_core.c
     * garde la résolution d'objets pendant que la v3 prend le reste.
     *
     * Rend 0 si l'hôte ne sait pas non plus. */
    int (*recours)(void *d, const HctNoeud *n, HctValeur *out);
} HctHote;

struct HctContexte {
    HctHote     hote;
    const char *erreur;        /* NULL tant que tout va bien           */
    const HctNoeud *fautif;    /* le nœud où l'erreur s'est produite   */
};

void hct_ctx_init(HctContexte *ctx, HctHote hote);
void hct_ctx_faute(HctContexte *ctx, const HctNoeud *n, const char *msg);

/* Évalue un arbre d'expression. En cas d'erreur, rend une valeur vide et
 * renseigne ctx->erreur — l'appelant doit le tester. */
HctValeur hct_evalue(HctContexte *ctx, const HctNoeud *n);

#endif
