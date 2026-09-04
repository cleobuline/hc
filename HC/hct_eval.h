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

    /* Recours pour les COMMANDES, symétrique du précédent.
     *
     * L'exécuteur ne traite lui-même que ce qui ne touche pas au monde : put,
     * get, global, l'arithmétique, les structures de contrôle, les sorties.
     * Tout le reste — go, set, show, answer, visual — lui revient.
     *
     * Le nœud lui est passé TEL QUEL, et non ses opérandes évalués. La
     * différence est décisive : « go to card 3 » n'a de sens que si l'hôte
     * voit la référence d'objet intacte ; évaluée d'abord, elle serait déjà
     * réduite à du texte et la commande perdrait son sens.
     *
     * L'hôte peut donc reconstituer la ligne d'origine par
     * hct_noeud_etendue() et la confier à son propre interpréteur. C'est le
     * mécanisme de la transition : la bibliothèque prend ce qu'elle sait
     * faire, l'ancien code garde le reste, et la frontière se déplace sans
     * que rien ne s'arrête.
     *
     * Rend 1 si la commande a été traitée, 0 pour laisser l'exécuteur se
     * rabattre sur `fonction`. */
    int (*commande)(void *d, const HctNoeud *n, HctContexte *ctx);

    /* Respiration : appelée à la fin de chaque tour de boucle. L'hôte
     * redessine, vide sa file d'événements, et rend 0 s'il faut interrompre.
     *
     * Indispensable : « repeat while the mouse is down » ne peut pas voir la
     * souris se relever si l'hôte n'a jamais la main. NULL = ne rien faire. */
    int (*respire)(void *d);

    /* Écrire dans un objet résolu — le texte d'un champ. `mode` vaut 0 pour
     * remplacer, 1 pour insérer AVANT, 2 pour ajouter APRÈS, comme `put`.
     *
     * Sans ce rappel, « put x into card field "n" » repartait tout entier à
     * l'hôte, qui réanalysait la ligne et réévaluait l'expression : le chemin
     * le plus fréquent d'un script était aussi le plus cher. Rend 0 si l'hôte
     * ne sait pas écrire dans cet objet — un bouton n'est pas un conteneur —
     * et l'exécuteur se rabat alors sur `commande`. */
    int (*ecrit_objet)(void *d, void *objet, const char *val, int mode);

    /* Écrire dans la BOÎTE DE MESSAGES : « put X » sans destination, et
     * « put X into the message box ».
     *
     * Sans ce rappel, la forme la plus courante d'un script — un simple
     * « put » — repartait tout entière à l'hôte, qui réanalysait la ligne et
     * réévaluait l'expression que l'exécuteur venait déjà d'évaluer.
     *
     * `mode` suit la convention d'ecrit_objet : 0 remplace, 1 insère AVANT,
     * 2 ajoute APRÈS. Un hôte qui se contente d'afficher la valeur peut
     * l'ignorer et rendre 1 quand même.
     *
     * Rend 0 si l'hôte n'a pas de boîte ; l'exécuteur se rabat sur
     * `commande`. */
    int (*ecrit_message)(void *d, const char *val, int mode);
    int (*globale)(void *d, const char *nom);
} HctHote;

 

struct HctContexte {
    HctHote     hote;
    const char *erreur;        /* NULL tant que tout va bien           */
    const HctNoeud *fautif;    /* le nœud où l'erreur s'est produite   */

    /* Imbrication des « the value of ». Une variable peut contenir un texte
     * qui redemande sa propre valeur : sans plafond, la récursion est
     * infinie et la pile C meurt sans message. */
    int         prof_valeur;
};
void hct_ctx_init(HctContexte *ctx, HctHote hote);
void hct_ctx_faute(HctContexte *ctx, const HctNoeud *n, const char *msg);

/* Évalue un arbre d'expression. En cas d'erreur, rend une valeur vide et
 * renseigne ctx->erreur — l'appelant doit le tester. */
HctValeur hct_evalue(HctContexte *ctx, const HctNoeud *n);
/* Déclarer une variable GLOBALE dans le gestionnaire courant.
 *
 * Quand l'hôte tient les variables, il tient aussi la distinction entre
 * locale et globale : l'exécuteur ne peut pas la poser lui-même, ses
 * propres portées ne servant alors à rien. Sans ce rappel, « global »
 * repartait tout entier à l'hôte par `commande` — cinquante-trois fois
 * sur une seule pile, pour une commande qui ne touche à rien.
 *
 * Rend 0 si l'hôte ne sait pas ; l'exécuteur se rabat sur `commande`. */

#endif
