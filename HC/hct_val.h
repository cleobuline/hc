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

/* LE CONTRAT, ET IL EST TENU CETTE FOIS.
 *
 * `txt` n'est JAMAIS nul après construction. Il l'était : quand une allocation
 * échouait, les constructeurs rendaient NULL et len = 0, indiscernable d'une
 * chaîne vide valide. L'en-tête demandait alors à chaque consommateur de
 * traiter le cas — une promesse que personne ne pouvait tenir, avec des
 * centaines de lectures de .txt dans le noyau.
 *
 * Mesuré, en faisant échouer UN SEUL malloc et en balayant tous les points
 * d'allocation d'un script ordinaire : sept plantages sur quatre cents.
 *
 * Une valeur en échec porte désormais une chaîne vide STATIQUE — lisible,
 * terminée par zéro — et lève un drapeau COLLANT. La sentinelle empêche le
 * plantage, le drapeau empêche le silence : sans lui, une pénurie se
 * déguiserait en chaîne vide, ce qui est le défaut qu'on chasse partout
 * ailleurs. L'exécuteur regarde le drapeau et lève une faute. */
/* LE NOMBRE BRUT QUI VOYAGE À CÔTÉ DU TEXTE.
 *
 * LE DÉFAUT, mesuré côte à côte avec HyperCard sous Basilisk II :
 *
 *     gabarit "0.0",  put 10*sqrt(2)     HyperCard 14.1   HC 14.0
 *     gabarit "0.0",  put 1000*sin(z)    HyperCard 21.8   HC  0.0
 *
 * Le second dit tout : ce n'était pas un arrondi, c'était une perte totale.
 * HC mettait en forme le retour de « sin » — qui valait 0.0218 — et rendait
 * 0.0 ; mille fois zéro font zéro.
 *
 * CE QUE ÇA COÛTAIT À L'ÉCRAN. Un traceur polaire calcule « r*cos(t) » sous
 * un gabarit d'une décimale. Chez HyperCard, cos(t) reste brut et seule la
 * multiplication est arrondie. Chez HC, cos(t) ne valait plus que 0.0, 0.1,
 * 0.2 — vingt-et-une valeurs pour tout un cercle — et la rosace sortait en
 * marches de douze pixels. Sous « 0.000 » le défaut restait, simplement en
 * dessous du pixel.
 *
 * LA RÈGLE, ET ELLE EST MESURÉE DANS LES DEUX SENS. Cinq relevés identiques
 * chez HyperCard et chez HC ont montré que tout le reste était déjà juste :
 *
 *     set the numberFormat to 0.0 / put sqrt(2) into x
 *     put x                   -> 1.4       l'affichage met en forme
 *     put 10*x                -> 14.0      RANGER A FIGÉ la mise en forme
 *     put sqrt(2) & ""        -> 1.4       la concaténation met en forme
 *     put sqrt(2)             -> 1.4       idem
 *     gabarit effacé, put x   -> 1.4       x est bien du texte figé
 *
 * Il ne restait donc qu'un seul chemin fautif : celui où le retour d'une
 * fonction part DIRECTEMENT dans un opérateur, sans être rangé, ni
 * concaténé, ni affiché.
 *
 * D'OÙ CE CHAMP, ET PAS UN TYPAGE. Différer la mise en forme jusqu'à la
 * conversion en texte aurait demandé de rattraper toutes les frontières, et
 * « .txt » se lit à des centaines d'endroits dans le noyau : une seule
 * oubliée et un affichage change sans qu'on l'ait voulu. On fait l'inverse.
 * Le texte reste EXACTEMENT celui d'avant — mis en forme au retour, comme
 * hier — et le nombre non arrondi voyage à côté, dans `brut`. Seul
 * l'opérateur arithmétique le regarde. Tout ce qui lit .txt voit le même
 * texte qu'avant, donc ne peut pas bouger.
 *
 * `a_brut` n'est posé QUE par le retour d'une fonction mathématique. Surtout
 * pas par un opérateur : « put 1/3*3 » rend 0.9 chez HyperCard, ce qui n'est
 * vrai que si « / » arrondit son résultat AVANT la multiplication. Les
 * opérateurs mettent en forme, les fonctions non — c'est toute la règle.
 *
 * Le drapeau ne survit pas à un rangement : une variable contient du TEXTE,
 * et c'est ce qui rend « put sqrt(2) into x / put 10*x » égal à 14.0 des deux
 * côtés. Sa durée de vie est celle d'une sous-expression. */
typedef struct {
    char  *txt;     /* terminé par 0, JAMAIS nul après construction */
    int    len;
    int    a_brut;  /* 1 si `brut` porte le nombre non mis en forme */
    double brut;
} HctValeur;

/* --- construction --- */
HctValeur hct_val_vide(void);
HctValeur hct_val_texte(const char *s);
HctValeur hct_val_texte_n(const char *s, int len);
HctValeur hct_val_nombre(double x);
HctValeur hct_val_calcul(double x);   /* idem, mais mis en forme */
/* Le retour d'une FONCTION : texte mis en forme comme hct_val_calcul, plus le
 * nombre non arrondi à côté. Voir la note sur HctValeur. */
HctValeur hct_val_fonction(double x);
HctValeur hct_val_bool(int vrai);
HctValeur hct_val_copie(HctValeur v);
void      hct_val_libere(HctValeur *v);

/* La valeur qu'on rend quand l'allocation a échoué : une chaîne vide statique,
 * et le drapeau levé. À employer partout où l'on construisait « txt = NULL ». */
HctValeur hct_val_echec(void);

/* Le drapeau est COLLANT : une fois levé il ne retombe que sur demande. C'est
 * ce qui permet de le consulter une fois par instruction plutôt qu'après
 * chaque sous-expression, sans risquer de manquer la panne. */
int  hct_val_manque(void);
void hct_val_manque_efface(void);

/* Sortir le texte d'une valeur en en prenant la propriété : l'appelant devra
 * free(). C'est la SEULE façon correcte, et la sentinelle en est la raison —
 * une valeur vide porte une chaîne STATIQUE, que free() ferait exploser.
 * La valeur est vidée au passage. */
char *hct_val_prend(HctValeur *v);

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
/* Un NaN portant le code SANE `code` : 4 pour 0/0, comme HyperCard l'écrit.
 * Le code ressort tel quel de hct_ecrit_nombre, sous la forme NAN(004). */
double hct_nan_code(unsigned long code);

/* Le même, en choisissant le bit de signe. Mesuré sous HyperCard :
 * « put sqrt(-1) » rend « -NAN(001) », avec le moins. */
double hct_nan_signe(unsigned long code, int negatif);

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
