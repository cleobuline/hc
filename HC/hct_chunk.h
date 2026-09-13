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

/* ------------------------------------------------------------------ UTF-8
 *
 * « the number of chars of "été" » rendait CINQ : le découpage comptait des
 * octets, et un é en fait deux. Pire que le compte faux, « char 1 of "été" »
 * rendait la MOITIÉ d'un é — une demi-séquence, que rien ne sait afficher.
 *
 * HyperCard était en MacRoman, un octet par caractère, et la question ne se
 * posait pas. Nos fichiers sont en UTF-8 : un caractère y tient sur un à
 * quatre octets, et c'est le caractère que l'utilisateur compte.
 *
 * La règle est simple et tient en une phrase : un octet de CONTINUATION
 * (10xxxxxx) appartient au caractère qui le précède, tout autre en commence
 * un nouveau. Elle a le mérite de toujours progresser, même sur une chaîne
 * mal formée — du Latin-1 arrivé là par accident sera découpé octet par
 * octet plutôt que de faire boucler le programme. */

/* Combien de caractères — pas d'octets — dans cette chaîne ? */
int hct_utf8_compte(const char *s);

/* Combien d'octets occupe le caractère qui commence à s[i] ? Toujours au
 * moins 1, pour que l'appelant avance à coup sûr. */
int hct_utf8_octets(const char *s, int i, int len);

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

/* Suppression : rend une chaîne neuve d'où le morceau visé a disparu, AVEC
 * son séparateur — « delete item 2 of "a,b,c" » rend "a,c" et non "a,,c".
 * C'est ce qui la distingue d'une écriture de vide. Un rang hors limites
 * rend la chaîne inchangée. Voir la note à sa définition. */
HctValeur hct_chunk_supprime(const char *s, HctSorteChunk sorte,
                             int n, int n2, char delim);

#endif
