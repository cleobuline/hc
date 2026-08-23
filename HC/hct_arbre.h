/* hct_arbre.h — arbre syntaxique de HyperTalk, pour HC v3.
 *
 * C99 pur. Un nœud est un type, zéro à N enfants, et le jeton d'où il vient.
 *
 * Ce dernier point est le cœur du dispositif : chaque nœud sait à quelle
 * ligne et à quelle colonne du script il correspond. C'est ce qui permettra
 * de montrer l'erreur à l'utilisateur exactement au bon endroit, y compris
 * pour une faute découverte à l'exécution — « le champ "toto" n'existe pas »
 * doit pouvoir surligner « toto », pas la ligne entière.
 *
 * Les nœuds vivent dans une réserve (HctReserve) libérée d'un coup. Aucun
 * comptage de références, aucune libération individuelle : un arbre naît et
 * meurt avec le script qu'il représente.
 */

#ifndef HCT_ARBRE_H
#define HCT_ARBRE_H

#include "hct_lex.h"

typedef enum {
    /* --- feuilles --------------------------------------------------- */
    HCTN_NOMBRE,      /* 42, 3.5                                        */
    HCTN_CHAINE,      /* "abc"                                          */
    HCTN_IDENT,       /* nom nu : variable, propriété, constante…       */

    /* --- opérateurs -------------------------------------------------- */
    HCTN_BINAIRE,     /* op dans `texte`, deux enfants                  */
    HCTN_UNAIRE,      /* not, -, there is a…                            */

    /* --- accès ------------------------------------------------------- */
    HCTN_CHUNK,       /* char/word/item/line d'une expression           */
    HCTN_OF,          /* X of Y : propriété, chunk, appartenance        */
    HCTN_APPEL,       /* f(a, b) ou « the length of x »                 */
    HCTN_LISTE,       /* liste d'arguments, N enfants                   */

    HCTN_OBJET,       /* référence d'objet : field "x" of card 3        */
    HCTN_COMMANDE,    /* verbe reconnu ; op porte le verbe canonique    */
    HCTN_MESSAGE,     /* nom non reconnu : envoi dans la hiérarchie     */
    HCTN_MOTCLE,      /* mot-clé littéral consommé par un motif        */
    HCTN_BLOC,        /* suite d'instructions                          */
    HCTN_SI,          /* if : condition, alors, [sinon]                 */
    HCTN_REPETE,      /* repeat ; op dit la forme                       */
    HCTN_GESTIONNAIRE,/* on/function : nom, params, corps               */

    HCTN_ERREUR       /* sous-arbre non analysable ; msg décrit         */
} HctGenreNoeud;

/* --- références d'objets ------------------------------------------------
 *
 * Ces trois énumérations sont calquées sur ce que hc_core.c consulte pour
 * résoudre une référence, et non sur une belle abstraction : le but est que
 * la greffe dans HC se réduise à une fonction de traduction
 * « HctNoeud * -> Object * », sans rien changer d'autre.
 *
 * HCT_OBJ_PART est à part : « part 3 » compte boutons et champs mêlés, alors
 * que « field 3 » ne compte que les champs. hc_part_number et
 * hc_object_number sont les deux compteurs correspondants.
 */
typedef enum {
    HCT_OBJ_STACK, HCT_OBJ_BACKGROUND, HCT_OBJ_CARD,
    HCT_OBJ_BUTTON, HCT_OBJ_FIELD, HCT_OBJ_PART,
    HCT_OBJ_ME, HCT_OBJ_TARGET, HCT_OBJ_MESSAGE
} HctTypeObjet;

/* « card field 1 » et « bg field 1 » sont DEUX objets différents : la
 * numérotation est propre au propriétaire. Sans cette portée, une référence
 * est ambiguë. */
typedef enum {
    HCT_PORTEE_AUCUNE, HCT_PORTEE_CARTE, HCT_PORTEE_FOND
} HctPortee;

typedef enum {
    HCT_DES_AUCUN,     /* « me », « this card »            */
    HCT_DES_NOM,       /* field "test"                     */
    HCT_DES_RANG,      /* field 3                          */
    HCT_DES_ID,        /* field id 4                       */
    HCT_DES_ORDINAL,   /* last card, any card              */
    HCT_DES_RELATIF    /* this / next / previous card      */
} HctDesignateur;

typedef enum {
    HCT_REL_AUCUN, HCT_REL_CE, HCT_REL_SUIVANT, HCT_REL_PRECEDENT
} HctRelatif;

/* Les quatre morceaux de HyperTalk. L'ordre suit celui du guide. */
typedef enum {
    HCT_CH_CHAR = 0, HCT_CH_WORD, HCT_CH_ITEM, HCT_CH_LINE
} HctSorteChunk;

/* Ordinaux de l'annexe I. HCT_ORD_AUCUN quand le rang est une expression. */
typedef enum {
    HCT_ORD_AUCUN = 0,
    HCT_ORD_PREMIER, HCT_ORD_DEUXIEME, HCT_ORD_TROISIEME, HCT_ORD_QUATRIEME,
    HCT_ORD_CINQUIEME, HCT_ORD_SIXIEME, HCT_ORD_SEPTIEME, HCT_ORD_HUITIEME,
    HCT_ORD_NEUVIEME, HCT_ORD_DIXIEME,
    HCT_ORD_MILIEU, HCT_ORD_DERNIER, HCT_ORD_QUELCONQUE
} HctOrdinal;

typedef struct HctNoeud HctNoeud;

struct HctNoeud {
    HctGenreNoeud genre;
    HctJeton      jeton;      /* d'où vient ce nœud, pour les erreurs    */

    /* Opérateur, en minuscules et sous sa forme canonique : "+", "&&",
     * "is not", "is a", "there is a", "not", "neg" pour le moins unaire.
     * Pointe sur une constante, jamais alloué. */
    const char   *op;

    /* Chunk : sorte, ordinal, et bornes. Les bornes sont des enfants. */
    HctSorteChunk sorte;
    HctOrdinal    ordinal;

    /* HCTN_OBJET seulement. Le désignateur est le fils 0 quand il en faut un
     * (nom, rang, id) ; la cible du « of » est le dernier fils. */
    HctTypeObjet   typeobj;
    HctPortee      portee;
    HctDesignateur designateur;
    HctRelatif     relatif;

    HctNoeud    **fils;
    int           nfils;
    const char   *msg;        /* HCTN_ERREUR : la raison                 */
};

/* Réserve : allocation par blocs, libération en un seul appel. */
typedef struct HctBloc  HctBloc;
typedef struct HctAlloc HctAlloc;
typedef struct {
    HctBloc  *tete;      /* blocs de nœuds                              */
    HctAlloc *allocs;    /* tableaux de fils, libérés avec la réserve   */
    int       nnoeuds;
} HctReserve;

HctNoeud *hct_noeud(HctReserve *r, HctGenreNoeud genre, HctJeton jeton);
int       hct_ajoute_fils(HctReserve *r, HctNoeud *pere, HctNoeud *fils);
void      hct_reserve_libere(HctReserve *r);

/* Copie `len` octets dans la réserve et rend une chaîne terminée par un zéro,
 * libérée avec elle. Sert aux mots-clés des motifs, qui pointent dans des
 * littéraux séparés par « | » et ne peuvent donc pas être utilisés tels quels. */
const char *hct_reserve_texte(HctReserve *r, const char *s, int len);

const char *hct_genre_noeud_nom(HctGenreNoeud g);
const char *hct_sorte_chunk_nom(HctSorteChunk s);
const char *hct_ordinal_nom(HctOrdinal o);
const char *hct_type_objet_nom(HctTypeObjet t);
const char *hct_portee_nom(HctPortee p);

/* Affiche l'arbre indenté sur `sortie`. Pour déboguer, et pour le banc
 * d'essai : c'est ainsi qu'on vérifie une priorité d'opérateur. */
void hct_arbre_montre(const HctNoeud *n, int profondeur, void *sortie);

#endif
