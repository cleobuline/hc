/* hct_val.h — la valeur HyperTalk, et ses conversions.
 *
 * HyperTalk n'a qu'un seul type : le TEXTE. Un nombre est du texte qui
 * ressemble à un nombre, `true` est la chaîne "true", une liste est du texte
 * séparé par des virgules. Toute la subtilité du langage tient dans les
 * conversions implicites, et c'est ici qu'elles vivent.
 *
 * Quelques conséquences qu'il vaut mieux avoir en tête :
 *
 *   "3" + "4"      vaut 7      — lecture numérique
 *   "3" & "4"      vaut "34"   — concaténation
 *   "3" = "3.0"    vaut true   — comparés comme nombres
 *   "abc" = "ABC"  vaut true   — la comparaison de texte ignore la casse
 *   "10" < "9"     vaut false  — nombres, pas texte
 *   "b" < "a10"    dépend      — texte, car « a10 » n'est pas un nombre
 *
 * La règle générale : si les DEUX opérandes se lisent comme des nombres, la
 * comparaison est numérique ; sinon elle est textuelle et insensible à la
 * casse. C'est ce que fait HyperCard, et c'est ce qui surprend le plus quand
 * on vient d'un autre langage.
 *
 * Une valeur possède son texte. hct_val_libere() le rend.
 */

#ifndef HCT_VAL_H
#define HCT_VAL_H

#include <stddef.h>

typedef struct {
    char *txt;      /* toujours non NULL après construction, terminé par 0 */
    int   len;
} HctValeur;

/* --- construction --- */
HctValeur hct_val_vide(void);
HctValeur hct_val_texte(const char *s);
HctValeur hct_val_texte_n(const char *s, int len);
HctValeur hct_val_nombre(double x);
HctValeur hct_val_bool(int vrai);
HctValeur hct_val_copie(HctValeur v);
void      hct_val_libere(HctValeur *v);

/* --- lecture --- */

/* Le texte se lit-il entièrement comme un nombre ? Les espaces de tête et de
 * fin sont tolérés, comme dans HyperCard. */
int    hct_est_nombre(const char *s);
double hct_vers_nombre(const char *s);

/* Vrai / faux. HyperTalk n'accepte que « true » et « false », casse ignorée ;
 * tout le reste est une erreur d'exécution, d'où le drapeau `valide`. */
int hct_vers_bool(const char *s, int *valide);

/* --- écriture d'un nombre ---
 *
 * HyperCard affiche les entiers sans décimale et les réels selon
 * `the numberFormat`. On s'en tient ici au format par défaut, qui est
 * l'entier quand la valeur en est un, et jusqu'à six décimales sinon,
 * zéros de fin retirés. */
int hct_ecrit_nombre(double x, char *out, int taille);

/* --- comparaison ---
 *
 * Rend -1, 0 ou +1. `numerique` reçoit 1 si la comparaison a été faite sur
 * des nombres — utile pour les messages d'erreur et pour les tests. */
int hct_compare(const char *a, const char *b, int *numerique);

/* Égalité au sens de HyperTalk : numérique si les deux sont des nombres,
 * sinon textuelle et insensible à la casse. */
int hct_egal(const char *a, const char *b);

#endif
