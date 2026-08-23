/* hct_exec.h — exécution des instructions HyperTalk.
 *
 * L'exécuteur tient les variables, la pile des gestionnaires, et déroule les
 * structures de contrôle. Tout ce qui touche aux objets et à l'écran passe
 * par l'hôte, comme pour l'évaluateur.
 *
 * Portée des variables, telle que la définit HyperTalk :
 *
 *   - une variable est LOCALE au gestionnaire qui l'emploie ;
 *   - sauf si celui-ci l'a déclarée `global` ;
 *   - une variable jamais affectée vaut son propre nom ;
 *   - `it` est locale, et reçoit le résultat de `get`, `ask`, `answer`…
 *
 * La sortie d'une boucle ou d'un gestionnaire ne se fait pas par des codes de
 * retour disséminés : elle remonte par un « signal » dans le contexte, que
 * chaque niveau consulte. C'est ce qui permet à `exit repeat` de traverser
 * plusieurs `if` imbriqués sans que chacun ait à s'en occuper.
 */

#ifndef HCT_EXEC_H
#define HCT_EXEC_H

#include "hct_eval.h"
#include "hct_arbre.h"

typedef enum {
    HCT_SIG_AUCUN = 0,
    HCT_SIG_EXIT_REPEAT,     /* exit repeat            */
    HCT_SIG_NEXT_REPEAT,     /* next repeat            */
    HCT_SIG_EXIT_HANDLER,    /* exit <nom> ou return   */
    HCT_SIG_EXIT_TOUT,       /* exit to HyperCard      */
    HCT_SIG_PASS             /* pass <nom>             */
} HctSignal;

typedef struct HctPortee HctPortee_;

typedef struct {
    HctContexte  ctx;         /* l'évaluateur                              */
    HctHote      hote;        /* celui de l'appelant, avec SES données     */
    HctPortee_  *locales;     /* pile des gestionnaires                    */
    HctPortee_  *globales;    /* une seule, vit tout le temps              */
    HctSignal    signal;
    HctValeur    retour;      /* valeur rendue par `return`                */
    int          profondeur;  /* garde-fou contre la récursion infinie     */
    const HctNoeud *script;   /* le script courant, pour retrouver ses
                               * gestionnaires quand une expression appelle
                               * une fonction définie dedans               */
} HctExec;

void hct_exec_init(HctExec *x, HctHote hote);
void hct_exec_libere(HctExec *x);

/* Exécute un arbre : instruction, bloc ou script entier. */
void hct_exec(HctExec *x, const HctNoeud *n);

/* Appelle un gestionnaire par son nom, avec ses arguments.
 * Rend 0 si aucun gestionnaire de ce nom n'existe dans le script. */
int hct_appelle(HctExec *x, const HctNoeud *script, const char *nom,
                HctValeur *args, int nargs, HctValeur *out);

/* Accès aux variables, pour l'hôte et les tests. */
int  hct_var_lit(HctExec *x, const char *nom, HctValeur *out);
void hct_var_ecrit(HctExec *x, const char *nom, const char *val);
void hct_var_globale(HctExec *x, const char *nom);

#endif
