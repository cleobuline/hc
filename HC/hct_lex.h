/* hct_lex.h — analyse lexicale de HyperTalk, pour HC v3.
 *
 * C99 pur, aucune dépendance hors bibliothèque standard.
 *
 * Principes retenus :
 *
 *  - Les jetons ne copient rien : chacun pointe dans le texte source, avec sa
 *    longueur. Le source doit donc rester vivant tant que les jetons servent —
 *    ce qui est le cas, le script étant conservé dans l'objet.
 *
 *  - L'analyse se fait en une passe, tout le tableau d'un coup. On peut ainsi
 *    l'afficher en entier pour déboguer, et le parseur peut regarder en avant
 *    autant qu'il veut sans machinerie.
 *
 *  - Le lexer ne connaît PAS les mots-clés. Il rend HCT_IDENT pour tout mot,
 *    et c'est le parseur qui décide, selon le contexte, si « card » est un
 *    mot-clé ou un nom de variable. HyperTalk l'exige : « put card into card »
 *    est un script légal.
 *
 *  - Les synonymes de l'annexe F sont normalisés ici (bg -> background), sauf
 *    « in » pour « of » : ce sont deux mots distincts, qu'on veut pouvoir citer
 *    tels quels dans un message d'erreur.
 *
 * Chaque jeton porte sa ligne et sa colonne, en base 1 : c'est ce qui permettra
 * de montrer l'erreur à l'utilisateur à l'endroit exact.
 */

#ifndef HCT_LEX_H
#define HCT_LEX_H

#include <stddef.h>

typedef enum {
    HCT_FIN = 0,     /* fin du source                                       */
    HCT_EOL,         /* fin de ligne significative (HyperTalk y est sensible)*/
    HCT_IDENT,       /* mot : identificateur, mot-clé, nom de propriété…     */
    HCT_NOMBRE,      /* littéral numérique, entier ou décimal                */
    HCT_CHAINE,      /* littéral entre guillemets, sans les guillemets       */
    HCT_OP,          /* opérateur ou ponctuation                             */
    HCT_ERREUR       /* jeton mal formé ; msg décrit le problème            */
} HctGenre;

typedef struct {
    HctGenre    genre;
    const char *deb;      /* pointe DANS le source, jamais alloué           */
    int         len;      /* longueur en octets                             */
    int         ligne;    /* base 1                                         */
    int         col;      /* base 1, en octets                              */
    const char *norme;    /* forme normalisée d'un IDENT synonyme, ou NULL  */
    const char *msg;      /* pour HCT_ERREUR : la raison                    */
} HctJeton;

typedef struct {
    HctJeton *jetons;
    int       n;          /* nombre de jetons, HCT_FIN compris              */
    int       cap;
    int       nerr;       /* combien de HCT_ERREUR dans le lot              */
} HctLot;

/* Analyse `src` en entier. Rend 0 si au moins un jeton est en erreur, 1 sinon.
 * Dans les deux cas le lot est exploitable : on ne s'arrête pas à la première
 * faute, pour pouvoir toutes les signaler d'un coup. */
int  hct_lex(const char *src, HctLot *lot);

void hct_lot_libere(HctLot *lot);

/* Forme normalisée d'un mot, ou NULL s'il n'est pas un synonyme connu.
 * Comparaison insensible à la casse. */
const char *hct_synonyme(const char *mot, int len);

/* Nom lisible d'un genre, pour l'affichage et les messages. */
const char *hct_genre_nom(HctGenre g);

/* Le texte d'un jeton, recopié dans `out` et terminé par un zéro.
 * Rend `out`. Tronque si nécessaire. */
char *hct_texte(const HctJeton *j, char *out, size_t taille);

#endif
