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

/* ATTENTION AU CONTRAT.
 *
 * Ce commentaire promettait « toujours non NULL après construction ». C'était
 * FAUX, et un commentaire faux est pire que pas de commentaire : il fait
 * écrire du code sans garde. Quand une allocation échoue, les constructeurs
 * rendent txt = NULL et len = 0 — indiscernable d'une chaîne vide valide.
 *
 * Tant que le type n'a pas de véritable état d'échec, tout consommateur doit
 * donc traiter txt == NULL. Le chantier est identifié : un drapeau d'échec
 * mémoire collant dans HctContexte, qui distinguerait « vide » de « pas pu ».
 * Il n'est pas fait, et le dire ici vaut mieux que de laisser croire l'inverse. */
typedef struct {
    char *txt;      /* terminé par 0 — mais NULL si l'allocation a échoué */
    int   len;
} HctValeur;

/* --- construction --- */
HctValeur hct_val_vide(void);
HctValeur hct_val_texte(const char *s);
HctValeur hct_val_texte_n(const char *s, int len);
HctValeur hct_val_nombre(double x);
HctValeur hct_val_calcul(double x);   /* idem, mais mis en forme */
HctValeur hct_val_bool(int vrai);
HctValeur hct_val_copie(HctValeur v);
void      hct_val_libere(HctValeur *v);

/* --- lecture --- */

/* Le texte se lit-il entièrement comme un nombre ? Les espaces de tête et de
 * fin sont tolérés, comme dans HyperCard. */
int    hct_est_nombre(const char *s);
double hct_vers_nombre(const char *s);

/* Plafond d'un rang de morceau. « put 10^300 into n » suivi de « put "z" into
 * line n of … » convertissait un double hors bornes en int — comportement
 * INDÉFINI — puis demandait quatre gigaoctets pour y loger les lignes vides
 * intermédiaires. Seize millions de morceaux est déjà au-delà de toute pile
 * réelle, et laisse l'arithmétique de hct_chunk loin de ses bords. */
#define HCT_RANG_MAX 16777216

/* Convertit en rang de morceau. Rend 0 et pose *hors = 1 si la valeur sort des
 * bornes ou n'est pas finie ; l'appelant lève alors une faute, plutôt que de
 * travailler sur un entier né d'une conversion indéfinie. `hors` peut être
 * NULL, auquel cas la valeur est simplement bornée. */
int hct_vers_rang(const char *s, int *hors);

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
int hct_ecrit_nombre_format(double x, char *out, int taille);

/* Le gabarit de « the numberFormat ». Chaîne vide = le défaut d'HyperCard. */
void        hct_format_nombre(const char *f);
const char *hct_format_nombre_lu(void);

/* --- comparaison ---
 *
 * Rend -1, 0 ou +1. `numerique` reçoit 1 si la comparaison a été faite sur
 * des nombres — utile pour les messages d'erreur et pour les tests. */
int hct_compare(const char *a, const char *b, int *numerique);

/* Égalité au sens de HyperTalk : numérique si les deux sont des nombres,
 * sinon textuelle et insensible à la casse. */
int hct_egal(const char *a, const char *b);

#endif
