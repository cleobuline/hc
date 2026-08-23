/* hct_chunk.h — les morceaux de HyperTalk : char, word, item, line.
 *
 * Ils fonctionnent dans LES DEUX SENS, et c'est ce qui fait leur difficulté :
 *
 *   get item 2 of "a,b,c"              rend "b"
 *   put "x" into item 2 of liste       reconstruit liste
 *   put "x" into item 5 of "a,b"       rend "a,b,,,x" — les vides sont créés
 *
 * Les quatre sortes ne se découpent pas de la même façon :
 *
 *   char   position brute, pas de séparateur
 *   word   séparés par des blancs, MULTIPLES, et les vides ne comptent pas :
 *          « word 2 of "a   b" » vaut "b", pas la chaîne vide
 *   item   séparés par UNE virgule (ou itemDelimiter), et les vides comptent :
 *          « item 2 of "a,,c" » est vide
 *   line   séparées par un saut de ligne, les vides comptent
 *
 * L'écriture au-delà de la fin étend la chaîne pour item et line, en ajoutant
 * les séparateurs manquants. Pour char et word, HyperCard ajoute simplement à
 * la fin, sans créer de vides intermédiaires.
 */

#ifndef HCT_CHUNK_H
#define HCT_CHUNK_H

#include "hct_arbre.h"
#include "hct_val.h"

/* Bornes d'un morceau dans une chaîne, en octets. `deb` et `fin` délimitent
 * le contenu ; `deb_sep` inclut le séparateur qui précède, ce dont l'écriture
 * a besoin pour remplacer proprement. */
typedef struct {
    int deb, fin;        /* contenu, fin exclusive       */
    int trouve;          /* 0 si le rang dépasse la fin  */
} HctBornes;

/* Localise le morceau de rang `n` (1-based). `n2` permet une plage
 * « item 1 to 3 » ; passer n2 <= 0 pour un morceau simple.
 * `delim` est le séparateur d'items, ',' par défaut. */
HctBornes hct_chunk_bornes(const char *s, HctSorteChunk sorte,
                           int n, int n2, char delim);

/* Combien de morceaux de cette sorte dans la chaîne ? */
int hct_chunk_compte(const char *s, HctSorteChunk sorte, char delim);

/* Lecture : rend une valeur neuve, vide si le rang dépasse. */
HctValeur hct_chunk_lit(const char *s, HctSorteChunk sorte,
                        int n, int n2, char delim);

/* Écriture : rend une chaîne neuve où le morceau visé vaut `val`.
 * Étend la chaîne si le rang dépasse, en créant les séparateurs manquants. */
HctValeur hct_chunk_ecrit(const char *s, HctSorteChunk sorte,
                          int n, int n2, char delim, const char *val);

#endif
