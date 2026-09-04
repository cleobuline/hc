/* hct_expr.c — expressions HyperTalk, descente récursive.
 * Voir hct_expr.h pour la table des priorités. */

#include "hct_expr.h"
#include "hct_cmd.h"
#include "hct_bloc.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------- petits services */

static const HctJeton *ici(HctAnalyseur *a)
{
    return &a->lot->jetons[a->i];
}

static int fini(HctAnalyseur *a)
{
    HctGenre g = ici(a)->genre;
    return g == HCT_FIN || g == HCT_EOL;
}

static void avance(HctAnalyseur *a)
{
    if (a->lot->jetons[a->i].genre != HCT_FIN) a->i++;
}

/* Le mot du jeton courant est-il `mot` ? Insensible à la casse, et l'on
 * regarde la forme normalisée quand le lexer en a trouvé une — ainsi « bg »
 * répond oui à « background ». */
static int mot_est(const HctJeton *j, const char *mot)
{
    if (j->genre != HCT_IDENT) return 0;
    if (j->norme) return !strcmp(j->norme, mot);
    int n = (int)strlen(mot);
    if (j->len != n) return 0;
    for (int k = 0; k < n; k++)
        if (tolower((unsigned char)j->deb[k]) != tolower((unsigned char)mot[k]))
            return 0;
    return 1;
}

static int mot_ici(HctAnalyseur *a, const char *mot)
{
    return mot_est(ici(a), mot);
}

/* Le jeton à `d` positions en avant est-il ce mot ? Sans dépasser la fin. */
static int mot_apres(HctAnalyseur *a, int d, const char *mot)
{
    int k = a->i + d;
    if (k >= a->lot->n) return 0;
    return mot_est(&a->lot->jetons[k], mot);
}

static int op_est(const HctJeton *j, const char *s)
{
    if (j->genre != HCT_OP) return 0;
    int n = (int)strlen(s);
    return j->len == n && !memcmp(j->deb, s, (size_t)n);
}

static int op_ici(HctAnalyseur *a, const char *s)
{
    return op_est(ici(a), s);
}

static HctNoeud *faute(HctAnalyseur *a, const char *msg)
{
    HctNoeud *n = hct_noeud(a->reserve, HCTN_ERREUR, *ici(a));
    if (n) n->msg = msg;
    a->nerreurs++;
    return n;
}

static HctNoeud *binaire(HctAnalyseur *a, const char *op, HctJeton j,
                         HctNoeud *g, HctNoeud *d)
{
    HctNoeud *n = hct_noeud(a->reserve, HCTN_BINAIRE, j);
    if (!n) return NULL;
    n->op = op;
    hct_ajoute_fils(a->reserve, n, g);
    hct_ajoute_fils(a->reserve, n, d);
    return n;
}

static HctNoeud *unaire(HctAnalyseur *a, const char *op, HctJeton j,
                        HctNoeud *f)
{
    HctNoeud *n = hct_noeud(a->reserve, HCTN_UNAIRE, j);
    if (!n) return NULL;
    n->op = op;
    hct_ajoute_fils(a->reserve, n, f);
    return n;
}

/* ------------------------------------------------ déclarations mutuelles */

static HctNoeud *rang_ou(HctAnalyseur *a);          /* 10, le plus faible */
static HctNoeud *rang_et(HctAnalyseur *a);          /* 9  */
static HctNoeud *rang_egalite(HctAnalyseur *a);     /* 8  */
static HctNoeud *rang_comparaison(HctAnalyseur *a); /* 7  */
static HctNoeud *rang_concat(HctAnalyseur *a);      /* 6  */
static HctNoeud *rang_somme(HctAnalyseur *a);       /* 5  */
static HctNoeud *rang_produit(HctAnalyseur *a);     /* 4  */
static HctNoeud *rang_puissance(HctAnalyseur *a);   /* 3  */
static HctNoeud *rang_unaire(HctAnalyseur *a);      /* 2  */
static HctNoeud *facteur(HctAnalyseur *a);          /* 1  */
static HctNoeud *chunk_ou_of(HctAnalyseur *a);
static HctNoeud *reference(HctAnalyseur *a);
static int reference_ici(HctAnalyseur *a);

/* ------------------------------------------- opérateurs en plusieurs mots
 *
 * Le lexer rend des IDENT séparés : « is not within » fait trois jetons. On
 * les assemble ici, en essayant les formes les plus longues d'abord — sans
 * quoi « is not in » serait pris pour « is not » suivi d'un « in » égaré.
 *
 * L'ordre de ce tableau est donc significatif.
 */
typedef struct { const char *m1, *m2, *m3; const char *op; } Compose;

static const Compose COMPOSES[] = {
    /* rang 7 */
    { "is", "not", "within", "is not within" },
    { "is", "not", "in",     "is not in"     },
    { "is", "not", "a",      "is not a"      },
    { "is", "not", "an",     "is not a"      },
    { "is", "within", NULL,  "is within"     },
    { "is", "in",     NULL,  "is in"         },
    { "is", "a",      NULL,  "is a"          },
    { "is", "an",     NULL,  "is a"          },
    { "contains", NULL, NULL, "contains"     },
    /* rang 8 */
    { "is", "not",    NULL,  "is not"        },
    { "is", NULL,     NULL,  "is"            },
    { NULL, NULL, NULL, NULL }
};

/* Les types que « is a » peut tester. La liste vient de l'annexe I.
 *
 * Elle sert à lever une ambiguïté RÉELLE du langage : « x is a » compare x à
 * la variable nommée « a », alors que « x is a number » teste un type. Le
 * seul moyen de distinguer est de regarder si le mot qui suit « a » est un
 * type connu — c'est ce que fait HyperCard, et c'est pourquoi on ne peut pas
 * traiter « is a » comme un opérateur composé sans condition. */
static const char *TYPES_TESTES[] = {
    "number", "integer", "logical", "boolean", "point", "rect", "rectangle",
    "date", "string", NULL
};

static int type_teste_apres(HctAnalyseur *a, int d)
{
    int k = a->i + d;
    if (k >= a->lot->n) return 0;
    const HctJeton *j = &a->lot->jetons[k];
    if (j->genre != HCT_IDENT) return 0;
    for (int i = 0; TYPES_TESTES[i]; i++) {
        int n = (int)strlen(TYPES_TESTES[i]);
        if (j->len != n) continue;
        int egal = 1;
        for (int c = 0; c < n; c++)
            if (tolower((unsigned char)j->deb[c]) !=
                tolower((unsigned char)TYPES_TESTES[i][c])) { egal = 0; break; }
        if (egal) return 1;
    }
    return 0;
}

/* Si un opérateur composé commence ici, rend sa forme canonique et avance
 * d'autant. Sinon rend NULL sans rien consommer. `rang` filtre : 7 pour les
 * comparaisons, 8 pour les égalités — « is » seul ne doit pas être happé
 * par le rang 7. */
static const char *compose_ici(HctAnalyseur *a, int rang)
{
    for (int k = 0; COMPOSES[k].op; k++) {
        const Compose *c = &COMPOSES[k];
        int r = (!strcmp(c->op, "is") || !strcmp(c->op, "is not")) ? 8 : 7;
        if (r != rang) continue;
        if (!mot_ici(a, c->m1)) continue;
        if (c->m2 && !mot_apres(a, 1, c->m2)) continue;
        if (c->m3 && !mot_apres(a, 2, c->m3)) continue;
        int n = 1 + (c->m2 != NULL) + (c->m3 != NULL);

        /* « is a » / « is not a » : n'est l'opérateur de test de type que si
         * un type connu suit. Sinon « a » est une variable, et l'on ne prend
         * que « is » ou « is not ». */
        if (!strcmp(c->op, "is a") && !type_teste_apres(a, n)) continue;
        if (!strcmp(c->op, "is not a") && !type_teste_apres(a, n)) continue;
        for (int i = 0; i < n; i++) avance(a);
        return c->op;
    }
    return NULL;
}

/* ------------------------------------------------------- rangs 10 et 9 */

#define HCT_PROF_MAX 400

static HctNoeud *rang_ou(HctAnalyseur *a)
{
    if (++a->prof > HCT_PROF_MAX) {
        a->prof--;
        return faute(a, "expression trop imbriquée");
    }
    HctNoeud *g = rang_et(a);
    while (!fini(a) && mot_ici(a, "or")) {
        HctJeton j = *ici(a); avance(a);
        g = binaire(a, "or", j, g, rang_et(a));
    }
    a->prof--;
    return g;
}

static HctNoeud *rang_et(HctAnalyseur *a)
{
    HctNoeud *g = rang_egalite(a);
    while (!fini(a) && mot_ici(a, "and")) {
        HctJeton j = *ici(a); avance(a);
        g = binaire(a, "and", j, g, rang_egalite(a));
    }
    return g;
}

/* ------------------------------------------------------------- rang 8 */

static HctNoeud *rang_egalite(HctAnalyseur *a)
{
    HctNoeud *g = rang_comparaison(a);
    for (;;) {
        if (fini(a)) break;
        HctJeton j = *ici(a);
        const char *op = NULL;

        if      (op_ici(a, "="))            { op = "=";  avance(a); }
        else if (op_ici(a, "<>"))           { op = "<>"; avance(a); }
        else if (op_ici(a, "\xE2\x89\xA0")) { op = "<>"; avance(a); }  /* ≠ */
        else                                 { op = compose_ici(a, 8); }

        if (!op) break;
        g = binaire(a, op, j, g, rang_comparaison(a));
    }
    return g;
}

/* ------------------------------------------------------------- rang 7 */

static HctNoeud *rang_comparaison(HctAnalyseur *a)
{
    HctNoeud *g = rang_concat(a);
    for (;;) {
        if (fini(a)) break;
        HctJeton j = *ici(a);
        const char *op = NULL;

        if      (op_ici(a, "<="))           { op = "<="; avance(a); }
        else if (op_ici(a, ">="))           { op = ">="; avance(a); }
        else if (op_ici(a, "\xE2\x89\xA4")) { op = "<="; avance(a); }  /* ≤ */
        else if (op_ici(a, "\xE2\x89\xA5")) { op = ">="; avance(a); }  /* ≥ */
        else if (op_ici(a, "<"))            { op = "<";  avance(a); }
        else if (op_ici(a, ">"))            { op = ">";  avance(a); }
        else                                 { op = compose_ici(a, 7); }

        if (!op) break;
        g = binaire(a, op, j, g, rang_concat(a));
    }
    return g;
}

/* ------------------------------------------------------------- rang 6 */

static HctNoeud *rang_concat(HctAnalyseur *a)
{
    HctNoeud *g = rang_somme(a);
    while (!fini(a) && (op_ici(a, "&&") || op_ici(a, "&"))) {
        const char *op = op_ici(a, "&&") ? "&&" : "&";
        HctJeton j = *ici(a); avance(a);
        g = binaire(a, op, j, g, rang_somme(a));
    }
    return g;
}

/* ------------------------------------------------------------- rang 5 */

static HctNoeud *rang_somme(HctAnalyseur *a)
{
    HctNoeud *g = rang_produit(a);
    while (!fini(a) && (op_ici(a, "+") || op_ici(a, "-"))) {
        const char *op = op_ici(a, "+") ? "+" : "-";
        HctJeton j = *ici(a); avance(a);
        g = binaire(a, op, j, g, rang_produit(a));
    }
    return g;
}

/* ------------------------------------------------------------- rang 4 */

static HctNoeud *rang_produit(HctAnalyseur *a)
{
    HctNoeud *g = rang_puissance(a);
    for (;;) {
        if (fini(a)) break;
        const char *op = NULL;
        if      (op_ici(a, "*"))     op = "*";
        else if (op_ici(a, "/"))     op = "/";
        else if (mot_ici(a, "div"))  op = "div";
        else if (mot_ici(a, "mod"))  op = "mod";
        if (!op) break;
        HctJeton j = *ici(a); avance(a);
        g = binaire(a, op, j, g, rang_puissance(a));
    }
    return g;
}

/* ------------------------------------------------------------- rang 3
 *
 * Associative à DROITE : 2^3^2 vaut 2^(3^2) = 512, et non (2^3)^2 = 64.
 * C'est la seule exception de tout le langage, et le guide la signale
 * explicitement. La récursion à droite suffit à l'obtenir. */

static HctNoeud *rang_puissance(HctAnalyseur *a)
{
    HctNoeud *g = rang_unaire(a);
    if (!fini(a) && op_ici(a, "^")) {
        HctJeton j = *ici(a); avance(a);
        return binaire(a, "^", j, g, rang_puissance(a));
    }
    return g;
}

/* ------------------------------------------------------------- rang 2
 *
 * Attention : « not » est ici, donc PLUS prioritaire que les comparaisons.
 * « not x = y » vaut donc « (not x) = y ». C'est contre-intuitif mais c'est
 * bien ce que dit l'annexe E, et HyperCard se comporte ainsi. */

static HctNoeud *rang_unaire(HctAnalyseur *a)
{
    if (fini(a)) return faute(a, "expression attendue");

    if (mot_ici(a, "not")) {
        HctJeton j = *ici(a); avance(a);
        return unaire(a, "not", j, rang_unaire(a));
    }
    if (op_ici(a, "-")) {
        HctJeton j = *ici(a); avance(a);
        return unaire(a, "neg", j, rang_unaire(a));
    }
    /* « there is a X » et ses variantes : unaire, rang 2 dans le guide. */
    if (mot_ici(a, "there") && mot_apres(a, 1, "is")) {
        HctJeton j = *ici(a);
        int d = 2;
        const char *op = "there is a";
        if (mot_apres(a, 2, "no"))                          { d = 3; op = "there is no"; }
        else if (mot_apres(a, 2, "not") &&
                 (mot_apres(a, 3, "a") || mot_apres(a, 3, "an"))) { d = 4; op = "there is no"; }
        else if (mot_apres(a, 2, "a") || mot_apres(a, 2, "an"))    { d = 3; }
        for (int k = 0; k < d; k++) avance(a);
        return unaire(a, op, j, rang_unaire(a));
    }
    return facteur(a);
}

/* ------------------------------------------------------------- rang 1 */

static HctNoeud *facteur(HctAnalyseur *a)
{
    if (fini(a)) return faute(a, "expression attendue");

    HctJeton j = *ici(a);

    if (j.genre == HCT_NOMBRE) {
        avance(a);
        return hct_noeud(a->reserve, HCTN_NOMBRE, j);
    }
    if (j.genre == HCT_CHAINE) {
        avance(a);
        return hct_noeud(a->reserve, HCTN_CHAINE, j);
    }
    if (op_est(&j, "(")) {
        avance(a);
        int garde = a->sans_of; a->sans_of = 0;
        HctNoeud *dedans = rang_ou(a);
        a->sans_of = garde;
        if (!op_ici(a, ")")) return faute(a, "parenthèse fermante attendue");
        avance(a);
        return dedans;
    }
    if (j.genre == HCT_IDENT)
        return chunk_ou_of(a);

    if (j.genre == HCT_ERREUR) {
        avance(a);
        HctNoeud *n = hct_noeud(a->reserve, HCTN_ERREUR, j);
        if (n) n->msg = j.msg;
        a->nerreurs++;
        return n;
    }
    return faute(a, "expression attendue");
}

/* --------------------------------------------------------- chunks et of
 *
 * Trois formes se ressemblent et commencent toutes par un IDENT :
 *
 *   char 3 of x            un chunk
 *   item 1 to 2 of x       un chunk avec bornes
 *   last word of x         un chunk avec ordinal
 *   the name of bg 3       une propriété
 *   random(6)              un appel
 *   x                      une simple variable
 *
 * On les distingue au fur et à mesure, sans revenir en arrière.
 */

static const struct { const char *mot; HctSorteChunk sorte; } SORTES[] = {
    { "character", HCT_CH_CHAR }, { "characters", HCT_CH_CHAR },
    { "word",      HCT_CH_WORD }, { "words",      HCT_CH_WORD },
    { "item",      HCT_CH_ITEM }, { "items",      HCT_CH_ITEM },
    { "line",      HCT_CH_LINE }, { "lines",      HCT_CH_LINE },
    { NULL, 0 }
};

static const struct { const char *mot; HctOrdinal ord; } ORDINAUX[] = {
    { "first", HCT_ORD_PREMIER }, { "second", HCT_ORD_DEUXIEME },
    { "third", HCT_ORD_TROISIEME }, { "fourth", HCT_ORD_QUATRIEME },
    { "fifth", HCT_ORD_CINQUIEME }, { "sixth", HCT_ORD_SIXIEME },
    { "seventh", HCT_ORD_SEPTIEME }, { "eighth", HCT_ORD_HUITIEME },
    { "ninth", HCT_ORD_NEUVIEME }, { "tenth", HCT_ORD_DIXIEME },
    { "middle", HCT_ORD_MILIEU }, { "last", HCT_ORD_DERNIER },
    { "any", HCT_ORD_QUELCONQUE },
    { NULL, 0 }
};

static int sorte_ici(HctAnalyseur *a, HctSorteChunk *s)
{
    for (int k = 0; SORTES[k].mot; k++)
        if (mot_ici(a, SORTES[k].mot)) { *s = SORTES[k].sorte; return 1; }
    return 0;
}

static int ordinal_ici(HctAnalyseur *a, HctOrdinal *o)
{
    for (int k = 0; ORDINAUX[k].mot; k++)
        if (mot_ici(a, ORDINAUX[k].mot)) { *o = ORDINAUX[k].ord; return 1; }
    return 0;
}

/* Un chunk complet : [ordinal] sorte [expr [to expr]] of <cible>.
 * Les chunks s'emboîtent — « word 2 of line 3 of me » — ce qu'on obtient
 * naturellement en analysant la cible par un appel récursif. */
static HctNoeud *chunk(HctAnalyseur *a, HctOrdinal ord)
{
    HctSorteChunk sorte;
    HctJeton j = *ici(a);
    if (!sorte_ici(a, &sorte)) return faute(a, "sorte de morceau attendue");
    avance(a);

    HctNoeud *n = hct_noeud(a->reserve, HCTN_CHUNK, j);
    if (!n) return NULL;
    n->sorte = sorte;
    n->ordinal = ord;

    /* Bornes, sauf si un ordinal les remplace. « the » est facultatif. */
    if (!ord && !mot_ici(a, "of") && !mot_ici(a, "in") && !fini(a)) {
        a->sans_of++;
        hct_ajoute_fils(a->reserve, n, rang_concat(a));
        if (mot_ici(a, "to")) {
            avance(a);
            hct_ajoute_fils(a->reserve, n, rang_concat(a));
        }
        a->sans_of--;
    }

    if (mot_ici(a, "of") || mot_ici(a, "in")) {
        avance(a);
        hct_ajoute_fils(a->reserve, n, chunk_ou_of(a));
    } else {
        HctNoeud *e = faute(a, "« of » attendu après un morceau");
        hct_ajoute_fils(a->reserve, n, e);
    }
    return n;
}


/* ------------------------------------------------- références d'objets
 *
 * La grammaire complète est :
 *
 *   [the] [card|bg] <type> [id <expr> | <nom> | <rang> | <ordinal>]
 *                                                  [of <référence>]
 *   me | the target | this|next|previous card|background|stack
 *
 * Trois choses la rendent délicate, et ce sont les trois qui comptent pour
 * la greffe dans hc_core.c :
 *
 *  - « card field 1 » et « bg field 1 » sont deux objets différents. La
 *    portée doit être retenue, pas devinée.
 *  - « part 3 » compte boutons et champs mêlés, « field 3 » non.
 *  - « second » peut être un ordinal OU le nom d'un objet. On n'en décide
 *    qu'en regardant le mot suivant, et l'on revient en arrière sinon.
 */

static const struct { const char *mot; HctTypeObjet type; } TYPES_OBJ[] = {
    { "stack",      HCT_OBJ_STACK      },
    { "background", HCT_OBJ_BACKGROUND },
    { "backgrounds",HCT_OBJ_BACKGROUND },
    { "card",       HCT_OBJ_CARD       },
    { "cards",      HCT_OBJ_CARD       },
    { "button",     HCT_OBJ_BUTTON     },
    { "buttons",    HCT_OBJ_BUTTON     },
    { "field",      HCT_OBJ_FIELD      },
    { "fields",     HCT_OBJ_FIELD      },
    { "part",       HCT_OBJ_PART       },
    { "parts",      HCT_OBJ_PART       },
    { NULL, 0 }
};

static int type_obj_ici(HctAnalyseur *a, HctTypeObjet *t)
{
    for (int k = 0; TYPES_OBJ[k].mot; k++)
        if (mot_ici(a, TYPES_OBJ[k].mot)) { *t = TYPES_OBJ[k].type; return 1; }
    return 0;
}

/* La portée n'a de sens que devant un bouton, un champ ou une part : « card »
 * seul est un type, « card field » une portée. On ne consomme donc le mot que
 * si un type le suit. */
static int portee_ici(HctAnalyseur *a, HctPortee *p)
{
    const char *m = NULL;
    if      (mot_ici(a, "card"))       { *p = HCT_PORTEE_CARTE; m = "card"; }
    else if (mot_ici(a, "background")) { *p = HCT_PORTEE_FOND;  m = "background"; }
    if (!m) return 0;

    int garde = a->i;
    a->i++;
    HctTypeObjet t;
    if (type_obj_ici(a, &t) &&
        (t == HCT_OBJ_BUTTON || t == HCT_OBJ_FIELD || t == HCT_OBJ_PART))
        return 1;                 /* le type suivant sera lu par l'appelant */
    a->i = garde;
    *p = HCT_PORTEE_AUCUNE;
    return 0;
}

/* Mots qui ne peuvent pas être un désignateur : ils appartiennent à la
 * commande ou à l'expression qui entoure la référence. Sans cette liste,
 * « set rect of card c to X » verrait « to » pris pour le nom de la carte. */
static const char *STRUCTURELS[] = {
    "of", "in", "to", "into", "from", "before", "after", "by", "with", "at",
    "then", "else", "end", "is", "and", "or", "not", "contains", "while",
    "until", "down", "times", "time", "for", "as", "using", "the", "there",
    "div", "mod", "up", "repeat", NULL
};

static int mot_structurel(HctAnalyseur *a)
{
    for (int k = 0; STRUCTURELS[k]; k++)
        if (mot_ici(a, STRUCTURELS[k])) return 1;
    return 0;
}

/* Le « of <cible> » facultatif, commun à toutes les formes. */
static void attache_cible(HctAnalyseur *a, HctNoeud *n)
{
    if (!a->sans_of && (mot_ici(a, "of") || mot_ici(a, "in"))) {
        avance(a);
        hct_ajoute_fils(a->reserve, n, chunk_ou_of(a));
    }
}

/* La boîte de messages, écrite « msg », « message box » ou « message window ».
 *
 * On ne prend PAS « message » tout seul : c'est un nom trop ordinaire — le
 * paramètre d'un gestionnaire, une variable — et l'en priver casserait des
 * scripts qui marchent. L'abréviation « msg », elle, ne désigne rien d'autre ;
 * on la reconnaît à ce que le lexer lui a posé une forme normalisée, ce que
 * le mot plein n'a pas.
 *
 * Sans cette reconnaissance, « put x into the message box » n'était pas
 * analysable : « box » restait sur la ligne et donnait « texte inattendu en
 * fin de ligne ». La faute suffisait à faire refuser le script ENTIER par
 * script_arbre(), donc à renvoyer tous ses gestionnaires à l'ancien
 * exécuteur — une ligne perdue en désactivait des centaines. */
static int boite_message_ici(HctAnalyseur *a)
{
    if (!mot_ici(a, "message")) return 0;
    if (ici(a)->norme) return 1;                  /* « msg » */
    return mot_apres(a, 1, "box") || mot_apres(a, 1, "window");
}

/* Analyse une référence d'objet à partir du jeton courant, en supposant que
 * l'appelant a déjà reconnu qu'il s'agit d'une. */
static HctNoeud *reference(HctAnalyseur *a)
{
    HctJeton j = *ici(a);

    /* me, the target, the message box */
    if (mot_ici(a, "me") || mot_ici(a, "target") || boite_message_ici(a)) {
        HctTypeObjet t = mot_ici(a, "me")     ? HCT_OBJ_ME
                       : mot_ici(a, "target") ? HCT_OBJ_TARGET
                                              : HCT_OBJ_MESSAGE;
        avance(a);
        if (t == HCT_OBJ_MESSAGE &&
            (mot_ici(a, "box") || mot_ici(a, "window"))) avance(a);
        HctNoeud *n = hct_noeud(a->reserve, HCTN_OBJET, j);
        if (!n) return NULL;
        n->typeobj = t;
        n->designateur = HCT_DES_AUCUN;
        return n;
    }

    /* this / next / previous <type> */
    HctRelatif rel = HCT_REL_AUCUN;
    if      (mot_ici(a, "this"))     rel = HCT_REL_CE;
    else if (mot_ici(a, "next"))     rel = HCT_REL_SUIVANT;
    else if (mot_ici(a, "previous")) rel = HCT_REL_PRECEDENT;
    if (rel) {
        int garde = a->i;
        a->i++;
        HctTypeObjet t;
        if (type_obj_ici(a, &t)) {
            /* Le jeton couvre « this background », pas le seul « this » :
             * la reconstitution du texte source doit rendre la référence
             * entière. Sans cela le pont envoyait « cards of this » à
             * term_value, qui ne reconnaissait rien et retombait sur le
             * total de la pile. */
            const HctJeton *jtype = ici(a);
            avance(a);
            HctNoeud *n = hct_noeud(a->reserve, HCTN_OBJET, j);
            if (!n) return NULL;
            n->jeton.len = (int)((jtype->deb + jtype->len) - j.deb);
            n->typeobj = t;
            n->designateur = HCT_DES_RELATIF;
            n->relatif = rel;
            attache_cible(a, n);
            return n;
        }
        a->i = garde;
    }

    /* [ordinal] [portée] <type> [désignateur] */
    HctOrdinal ord = HCT_ORD_AUCUN;
    if (ordinal_ici(a, &ord)) {
        int garde = a->i;
        a->i++;
        HctPortee p2 = HCT_PORTEE_AUCUNE;
        HctTypeObjet t2;
        portee_ici(a, &p2);
        if (!type_obj_ici(a, &t2)) { a->i = garde; ord = HCT_ORD_AUCUN; }
        else a->i = garde + 1;    /* on garde l'ordinal, on relit la suite */
    }

    HctPortee portee = HCT_PORTEE_AUCUNE;
    portee_ici(a, &portee);

    HctTypeObjet type;
    if (!type_obj_ici(a, &type)) return faute(a, "type d'objet attendu");
    avance(a);

    HctNoeud *n = hct_noeud(a->reserve, HCTN_OBJET, j);
    if (!n) return NULL;
    n->typeobj = type;
    n->portee = portee;

    if (ord) {
        n->designateur = HCT_DES_ORDINAL;
        n->ordinal = ord;
    } else if (mot_ici(a, "id")) {
        avance(a);
        n->designateur = HCT_DES_ID;
        a->sans_of++;
        /* rang_somme, pas rang_concat : voir le commentaire sous le
         * désignateur numérique, deux blocs plus bas — même raison, même
         * remède, ici pour « card id n ». */
        hct_ajoute_fils(a->reserve, n, rang_somme(a));
        a->sans_of--;
    } else if (ici(a)->genre == HCT_CHAINE) {
        n->designateur = HCT_DES_NOM;
        a->sans_of++;
        /* Idem : un nom cité ne doit pas avaler un « && » qui suit. */
        hct_ajoute_fils(a->reserve, n, rang_somme(a));
        a->sans_of--;
    } else if (ici(a)->genre == HCT_NOMBRE || op_ici(a, "(")) {
        n->designateur = HCT_DES_RANG;
        a->sans_of++;
        /* rang_somme (5), pas rang_concat (6) : le désignateur numérique
         * garde le droit à l'arithmétique — « card i + 1 » — mais s'arrête
         * avant « & »/« && ». Avec rang_concat, « card 1 && x » lisait « 1
         * && x » comme LE RANG, avalait tout le « && x » dans la référence,
         * et ne laissait rien à l'opérateur de concaténation qui l'entoure :
         * « the name of card 1 && the name of card 2 » perdait le second
         * terme en entier, sans le moindre message d'erreur. */
        hct_ajoute_fils(a->reserve, n, rang_somme(a));
        a->sans_of--;
    } else if (ici(a)->genre == HCT_IDENT && !mot_structurel(a)) {
        /* Désignateur pris dans une variable : « card whichCard »,
         * « bg field "x" of card theCard ». Le nom ou le rang n'est connu
         * qu'à l'exécution, d'où un désignateur qui porte une expression.
         * La liste des mots structurels évite d'avaler « to » dans
         * « set rect of ... of card c to ... ». */
        n->designateur = HCT_DES_RANG;
        a->sans_of++;
        hct_ajoute_fils(a->reserve, n, rang_somme(a));
        a->sans_of--;
    } else {
        n->designateur = HCT_DES_AUCUN;   /* « the name of stack » */
    }

    attache_cible(a, n);
    return n;
}

/* Une référence commence-t-elle ici ? On regarde sans consommer. */
static int reference_ici(HctAnalyseur *a)
{
    if (mot_ici(a, "me") || mot_ici(a, "target")) return 1;
    /* Sans cette ligne, la branche « boîte de messages » de reference() était
     * inatteignable : c'est ici que l'on décide d'y aller. */
    if (boite_message_ici(a)) return 1;
    HctTypeObjet t;
    if (type_obj_ici(a, &t)) return 1;
    if (mot_ici(a, "this") || mot_ici(a, "next") || mot_ici(a, "previous")) {
        int k = a->i + 1;
        if (k < a->lot->n) {
            HctAnalyseur b = *a; b.i = k;
            if (type_obj_ici(&b, &t)) return 1;
        }
        return 0;
    }
    HctOrdinal o;
    if (ordinal_ici(a, &o)) {
        HctAnalyseur b = *a; b.i = a->i + 1;
        HctPortee p;
        portee_ici(&b, &p);
        if (type_obj_ici(&b, &t)) return 1;
    }
    return 0;
}

/* Adjectifs de propriété, annexe I du guide : « the long name of me »,
 * « the abbreviated date », « the short id of this card ». Ils qualifient la
 * propriété qui suit et ne sont pas des noms en eux-mêmes. */
static const char *ADJECTIFS[] = {
    "long", "short", "abbreviated", "english", "plain", "numeric", NULL
};

static int adjectif_ici(HctAnalyseur *a)
{
    for (int k = 0; ADJECTIFS[k]; k++)
        if (mot_ici(a, ADJECTIFS[k])) return 1;
    return 0;
}

static HctNoeud *chunk_ou_of(HctAnalyseur *a)
{
    /* « the » facultatif devant une propriété ou un ordinal. */
    if (mot_ici(a, "the")) avance(a);

    /* Un adjectif ne vaut que s'il qualifie quelque chose. */
    
    /* Un adjectif ne vaut que s'il qualifie quelque chose.
     *
     * On retient son JETON, pas son texte : le nœud qui suivra étendra ses
     * bornes pour couvrir « long time » et non le seul « time ». Sans cela
     * l'adjectif disparaissait de l'arbre, et le pont — qui reconstitue le
     * texte source à partir des jetons — demandait « the time » au lieu de
     * « the long time », rendant l'heure sans les secondes. */
    const HctJeton *jadj = NULL;
    if (adjectif_ici(a)) {
        const HctJeton *suiv = (a->i + 1 < a->lot->n) ? &a->lot->jetons[a->i+1]
                                                      : NULL;
        if (suiv && suiv->genre == HCT_IDENT) {
            jadj = ici(a);
            avance(a);
        }
    }

    /* Un moins unaire comme cible : « line 5 of -24 ». Rare à la main, mais
     * parfaitement légal, et le générateur aléatoire le produit vite. */
    if (op_ici(a, "-")) return rang_unaire(a);

    /* Une chaîne ou un nombre comme cible : « char 2 of "abcde" ». Le cas
     * paraît artificiel mais il vient vite dans les tests, et HyperTalk
     * l'autorise partout où une expression est attendue. */
    if (ici(a)->genre == HCT_CHAINE || ici(a)->genre == HCT_NOMBRE) {
        HctJeton jl = *ici(a);
        avance(a);
        return hct_noeud(a->reserve,
                         jl.genre == HCT_CHAINE ? HCTN_CHAINE : HCTN_NOMBRE, jl);
    }

    /* Une parenthèse comme cible : « the number of lines of (char 1 to n of x) ». */
    if (op_ici(a, "(")) {
        HctJeton jp = *ici(a);
        avance(a);
        int garde = a->sans_of; a->sans_of = 0;
        HctNoeud *dedans = rang_ou(a);
        a->sans_of = garde;
        if (!op_ici(a, ")")) return faute(a, "parenthèse fermante attendue");
        avance(a);
        (void)jp;
        return dedans;
    }

    HctOrdinal ord = HCT_ORD_AUCUN;
    if (ordinal_ici(a, &ord)) {
        /* « last word of x » : ordinal puis sorte. Sinon l'ordinal était en
         * fait un nom ordinaire — « second » peut nommer un fond. */
        HctSorteChunk s;
        int j = a->i;
        a->i++;
        if (sorte_ici(a, &s)) return chunk(a, ord);
        a->i = j;
        ord = HCT_ORD_AUCUN;
    }

    HctSorteChunk s;
    if (sorte_ici(a, &s)) return chunk(a, HCT_ORD_AUCUN);

    /* Une référence d'objet ? */
    if (reference_ici(a)) return reference(a);

    /* Sinon un nom : variable, propriété, constante, ou appel de fonction. */
    HctJeton j = *ici(a);
    if (j.genre != HCT_IDENT) return faute(a, "nom attendu");
    avance(a);

    HctNoeud *n = hct_noeud(a->reserve, HCTN_IDENT, j);
    if (!n) return NULL;
    if (jadj) {
        /* Le jeton s'étend de l'adjectif à la fin du nom, pour que la
         * reconstitution du texte source rende « long time ». */
        n->jeton.deb = jadj->deb;
        n->jeton.len = (int)((j.deb + j.len) - jadj->deb);
        n->jeton.col = jadj->col;
    }
    if (op_ici(a, "(")) {                       /* appel : f(a, b) */
        avance(a);
        HctNoeud *appel = hct_noeud(a->reserve, HCTN_APPEL, j);
        if (!appel) return NULL;
        hct_ajoute_fils(a->reserve, appel, n);
        int garde = a->sans_of; a->sans_of = 0;
        if (!op_ici(a, ")")) {
            for (;;) {
                hct_ajoute_fils(a->reserve, appel, rang_ou(a));
                if (op_ici(a, ",")) { avance(a); continue; }
                break;
            }
        }
        a->sans_of = garde;
        if (!op_ici(a, ")"))
            hct_ajoute_fils(a->reserve, appel,
                            faute(a, "parenthèse fermante attendue"));
        else avance(a);
        n = appel;
    }

    /* « X of Y » : propriété, ou appartenance. */
    while (!a->sans_of && (mot_ici(a, "of") || mot_ici(a, "in"))) {
        HctJeton jof = *ici(a);
        avance(a);
        HctNoeud *of = hct_noeud(a->reserve, HCTN_OF, jof);
        if (!of) return n;
        hct_ajoute_fils(a->reserve, of, n);
        hct_ajoute_fils(a->reserve, of, chunk_ou_of(a));
        n = of;
    }
    return n;
}

/* ------------------------------------------------------------- entrées */

void hct_analyseur_init(HctAnalyseur *a, const HctLot *lot, HctReserve *r)
{
    a->lot = lot;
    a->i = 0;
    a->reserve = r;
    a->nerreurs = 0;
    a->sans_of = 0;
    a->prof = 0;
    a->prof_si = 0;
}

HctNoeud *hct_expression(HctAnalyseur *a)
{
    return rang_ou(a);
}

/* Analyse une expression en s'arrêtant sous le rang du « or ». Sert aux
 * commandes où « or » est un séparateur et non un opérateur — les boutons
 * d'« answer ». */
HctNoeud *hct_expression_sans_ou(HctAnalyseur *a)
{
    return rang_et(a);
}

HctNoeud *hct_analyse_texte(const char *src, HctLot *lot, HctReserve *r,
                            int *nerr)
{
    hct_lex(src, lot);
    HctAnalyseur a;
    hct_analyseur_init(&a, lot, r);
    HctNoeud *n = hct_expression(&a);
    if (!fini(&a)) {
        a.nerreurs++;
        HctNoeud *e = hct_noeud(r, HCTN_ERREUR, *ici(&a));
        if (e) {
            e->msg = "texte inattendu après l'expression";
            HctNoeud *pere = hct_noeud(r, HCTN_LISTE, *ici(&a));
            if (pere) {
                hct_ajoute_fils(r, pere, n);
                hct_ajoute_fils(r, pere, e);
                n = pere;
            }
        }
    }
    if (nerr) *nerr = a.nerreurs + lot->nerr;
    return n;
}

/* ------------------------------------------ services offerts à hct_cmd.c
 *
 * hct_cmd.c pilote l'analyse par une table de motifs, mais n'a pas accès aux
 * fonctions statiques d'ici. Ces quelques passe-plats lui suffisent, et
 * gardent le reste de l'analyseur privé. */

int hct_expr_fini(HctAnalyseur *a) { return fini(a); }

const HctJeton *hct_expr_jeton(HctAnalyseur *a) { return ici(a); }

const HctJeton *hct_expr_jeton_apres(HctAnalyseur *a, int d)
{
    int k = a->i + d;
    return (k < a->lot->n) ? &a->lot->jetons[k] : NULL;
}

int hct_expr_virgule(HctAnalyseur *a)
{
    if (!op_ici(a, ",")) return 0;
    avance(a);
    return 1;
}

HctNoeud *hct_expr_faute(HctAnalyseur *a, const char *msg)
{
    return faute(a, msg);
}

HctNoeud *hct_expr_ouvre_commande(HctAnalyseur *a, const char *verbe, int nmots)
{
    HctJeton j = *ici(a);
    for (int k = 0; k < nmots; k++) avance(a);
    HctNoeud *n = hct_noeud(a->reserve, HCTN_COMMANDE, j);
    if (n) n->op = verbe;
    return n;
}

/* Consomme le mot imposé par un motif et en fait un HCTN_MOTCLE. L'arbre le
 * garde : « put x into y » et « put x after y » ne diffèrent que par lui. */
HctNoeud *hct_expr_avale_motcle(HctAnalyseur *a, const char *el, int len,
                                int alternative)
{
    HctJeton j = *ici(a);
    avance(a);
    HctNoeud *n = hct_noeud(a->reserve, HCTN_MOTCLE, j);
    if (!n) return NULL;
    /* Retrouver l'alternative retenue et la copier dans la réserve.
     *
     * Une copie est nécessaire : `el` pointe dans un littéral de la table des
     * motifs, où les alternatives sont séparées par « | » et non terminées
     * par un zéro. Un tampon tournant, lui, se faisait recycler au bout de
     * seize mots-clés — « set cursor to watch » s'affichait alors avec le
     * « into » d'une commande antérieure. */
    int k = 0, deb = 0;
    for (int i = 0; i <= len; i++) {
        if (i == len || el[i] == '|') {
            if (k == alternative) {
                n->op = hct_reserve_texte(a->reserve, el + deb, i - deb);
                return n;
            }
            deb = i + 1; k++;
        }
    }
    return n;
}

void hct_expr_avance(HctAnalyseur *a) { avance(a); }

int hct_expr_op_ici(HctAnalyseur *a, const char *s) { return op_ici(a, s); }

/* Consomme un mot brut, sans l'analyser comme expression. */
HctNoeud *hct_expr_avale_mot(HctAnalyseur *a)
{
    HctJeton j = *ici(a);
    avance(a);
    return hct_noeud(a->reserve, HCTN_IDENT, j);
}

/* --------------------------------------------------------- instruction */

HctNoeud *hct_instruction(HctAnalyseur *a)
{
    if (fini(a)) return NULL;

    HctNoeud *c = hct_commande(a);
    if (c) return c;

    /* Filet : un nom suivi d'arguments est un message envoyé dans la
     * hiérarchie d'objets. C'est ainsi que « dieFall » ou
     * « markToday newDay,oldDay » s'exécutent, et c'est pour cela qu'un
     * verbe inconnu ne doit JAMAIS produire une erreur de syntaxe. */
    HctJeton j = *ici(a);
    if (j.genre != HCT_IDENT) {
        HctNoeud *e = faute(a, "instruction attendue");
        avance(a);
        return e;
    }
    avance(a);
    HctNoeud *m = hct_noeud(a->reserve, HCTN_MESSAGE, j);
    if (!m) return NULL;
    m->op = "message";
    while (!fini(a)) {
        hct_ajoute_fils(a->reserve, m, rang_ou(a));
        if (op_ici(a, ",")) { avance(a); continue; }
        break;
    }
    return m;
}
