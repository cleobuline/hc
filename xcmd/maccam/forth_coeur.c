/* forth_coeur.c — le FORTH de MAC•CAM, sans HC autour (forth_coeur.h).
 *
 * Il ne compile pas au sens strict : pour chacune des 4 096 entrées d'une
 * table, il pose les valeurs du voisinage que l'index désigne, et EXÉCUTE la
 * règle, mot après mot. Ce qu'elle envoie par >PLN0 … >PLN3 devient le
 * caractère de cette entrée. C'est la mécanique du Forth de maccam-6
 * (cam_forth.c, du même auteur) ; le dialecte, lui, est celui des années 90,
 * et l'ordre des bits celui des tables que la pile garde.
 *
 * Un mot de l'utilisatrice passe AVANT les mots de base : ses programmes
 * définissent eux-mêmes RAND, ECHO, CENTERS, >PLNA ou <>, et c'est leur
 * définition qui doit répondre.
 *
 * Écrit pour tourner dans un module de HC (le kit xcmd/, sans système) comme
 * dans un programme ordinaire, pour l'éprouver sur l'oracle : il n'emploie
 * que ce que le kit fournit. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forth_coeur.h"

/* ---- la disposition des bits ---- */

/* LES HUIT TABLES DU FORTH D'ORIGINE, recopiées de son code, dans l'ordre
 * des bits (docs/mesures/xcmd.txt). Trois sont aussi MESURÉES sur les tables
 * de la pile — Moore, von Neumann, Margolus-PH —, et le moteur de MACCAM
 * fabrique ses index de la même façon : trois sources d'accord. Les numéros
 * sont ceux de « major », dans les scripts comme dans MACCAM. */
signed char forth_disposition[FORTH_VOISINAGES][FORTH_BITS] = {
    [FORTH_MOORE] = {
        V_EAST, V_SEAST, V_NEAST, V_CENTER, V_SOUTH, V_NORTH,
        V_WEST, V_SWEST, V_NWEST, V_CENTERP, V_ACENTERP, V_ACENTER },
    [FORTH_VONN] = {
        V_EASTP, V_EAST, V_CENTERP, V_CENTER, V_SOUTHP, V_SOUTH,
        V_NORTHP, V_NORTH, V_WESTP, V_WEST, V_ACENTERP, V_ACENTER },
    [FORTH_MARG_PH] = {
        V_CCWP, V_CCW, V_OPPP, V_OPP, V_CWP, V_CW,
        V_CENTERP, V_CENTER, V_ACENTERP, V_ACENTER, V_PHASE, V_PHASEP },
    [FORTH_MARG_HV] = {
        V_CCWP, V_CCW, V_OPPP, V_OPP, V_CWP, V_CW,
        V_CENTERP, V_CENTER, V_ACENTERP, V_ACENTER, V_HORZ, V_VERT },
    [FORTH_MOORE_PH] = {
        V_EAST, V_SEAST, V_NEAST, V_CENTER, V_SOUTH, V_NORTH,
        V_WEST, V_SWEST, V_NWEST, V_CENTERP, V_PHASE, V_PHASEP },
    [FORTH_MOORE_HV] = {
        V_EAST, V_SEAST, V_NEAST, V_CENTER, V_SOUTH, V_NORTH,
        V_WEST, V_SWEST, V_NWEST, V_CENTERP, V_HORZ, V_VERT },
    [FORTH_VONN_PH] = {
        V_EASTP, V_EAST, V_CENTERP, V_CENTER, V_SOUTHP, V_SOUTH,
        V_NORTHP, V_NORTH, V_WESTP, V_WEST, V_PHASE, V_PHASEP },
    [FORTH_VONN_HV] = {
        V_EASTP, V_EAST, V_CENTERP, V_CENTER, V_SOUTHP, V_SOUTH,
        V_NORTHP, V_NORTH, V_WESTP, V_WEST, V_HORZ, V_VERT },
};

static const char *const NOMS_VOISINS[V_NOMBRE] = {
    "CENTER", "CENTER'",
    "NORTH", "SOUTH", "EAST", "WEST",
    "N.EAST", "N.WEST", "S.EAST", "S.WEST",
    "NORTH'", "SOUTH'", "EAST'", "WEST'",
    "CW", "CCW", "OPP", "CW'", "CCW'", "OPP'",
    "PHASE", "PHASE'", "HORZ", "VERT",
    "&CENTER", "&CENTER'",
};

const char *forth_nom_voisin(int v)
{
    return v >= 0 && v < V_NOMBRE ? NOMS_VOISINS[v] : "-";
}

/* ---- les mots de base ---- */

enum {
    P_PLUS, P_MOINS, P_FOIS, P_DIVISE, P_MOD, P_NEGATE, P_ABS,
    P_2FOIS, P_2DIVISE, P_1PLUS, P_1MOINS,
    P_AND, P_OR, P_XOR, P_NOT,
    P_EGAL, P_INF, P_SUP, P_ZEGAL, P_ZINF, P_ZSUP,
    P_DUP, P_DROP, P_SWAP, P_OVER, P_ROT,
    P_LIT, P_ECRIT,
    P_PLN0, P_PLN1, P_PLN2, P_PLN3,
    P_BEEP,
    P_NOMBRE_
};

static const struct { const char *nom; int p; } PRIMITIVES[] = {
    { "+", P_PLUS }, { "-", P_MOINS }, { "*", P_FOIS }, { "/", P_DIVISE },
    { "MOD", P_MOD }, { "NEGATE", P_NEGATE }, { "ABS", P_ABS },
    { "2*", P_2FOIS }, { "2/", P_2DIVISE }, { "1+", P_1PLUS },
    { "1-", P_1MOINS },
    { "AND", P_AND }, { "OR", P_OR }, { "XOR", P_XOR }, { "NOT", P_NOT },
    { "=", P_EGAL }, { "<", P_INF }, { ">", P_SUP }, { "0=", P_ZEGAL },
    { "0<", P_ZINF }, { "0>", P_ZSUP },
    { "DUP", P_DUP }, { "DROP", P_DROP }, { "SWAP", P_SWAP },
    { "OVER", P_OVER }, { "ROT", P_ROT },
    { "@", P_LIT }, { "!", P_ECRIT },
    { ">PLN0", P_PLN0 }, { ">PLN1", P_PLN1 }, { ">PLN2", P_PLN2 },
    { ">PLN3", P_PLN3 },
    { "BEEP", P_BEEP },
};

/* Les déclarations : lues au premier niveau, sans effet sur la pile. Les
 * scripts de la pile lisent ALTERNATE eux-mêmes dans le texte. */
static const struct { const char *nom; int voisinage; } DECLARATIONS[] = {
    { "N/MOORE", FORTH_MOORE }, { "N/VONN", FORTH_VONN },
    { "N/MARG-PH", FORTH_MARG_PH }, { "N/MARG-HV", FORTH_MARG_HV },
    { "N/MOORE-HV", FORTH_MOORE_HV }, { "N/MOORE-PH", FORTH_MOORE_PH },
    { "N/VONN-PH", FORTH_VONN_PH }, { "N/VONN-HV", FORTH_VONN_HV },
    { "ALTERNATE", -1 },
};

/* ---- les jetons ---- */

enum {
    K_RIEN, K_NOMBRE, K_VOISIN, K_DEF, K_CONSTANTE, K_VARIABLE, K_PRIM,
    K_IF, K_ELSE, K_THEN, K_ACCOLADE, K_FERMANTE,
    K_DO, K_LOOP, K_I, K_BEGIN, K_UNTIL, K_WHILE, K_REPEAT,
    K_INCONNU
};

typedef struct {
    char *t;            /* le mot, en capitales */
    int   lien;         /* l'autre bout d'une structure, ou -1 */
    int   genre, val;   /* ce qu'il désigne, résolu… */
    int   version;      /* … pour cette version du dictionnaire, */
    int   fige;         /* … ou une fois pour toutes, dans une définition */
} Jeton;

typedef struct { const char *nom; int debut, fin; } Definition;
typedef struct { const char *nom; int val, variable; } Constante;

enum { PILE = 64, RETOUR = 32, PROFONDEUR = 64, CELLULES = 64 };

typedef struct {
    Jeton      *j;
    int         nj;
    Definition *defs;
    int         ndefs;
    Constante  *consts;
    int         nconsts;
    int         variables[CELLULES];
    int         nvariables;
    int         version;

    int  pile[PILE];
    int  sp;
    int  retour[RETOUR][2];         /* DO … LOOP : index, limite */
    int  rp;
    int  profondeur;

    unsigned char voisin[V_NOMBRE];
    int  envoi[FORTH_PLANS];
    int  envoye;                    /* bit k : >PLNk pendant cette entrée */

    const char *faute;
    int         position;           /* le rang du mot du premier niveau */
} Vm;

static char *copie_capitales(const char *s, size_t n)
{
    char *c = malloc(n + 1);
    if (!c) return NULL;
    for (size_t i = 0; i < n; i++) c[i] = (char)toupper((unsigned char)s[i]);
    c[n] = '\0';
    return c;
}

/* Découpe le source en mots. « ( … ) » est un commentaire, sur autant de
 * lignes qu'il veut. */
static int decoupe(Vm *m, const char *s)
{
    int cap = 64;
    m->j = malloc((size_t)cap * sizeof *m->j);
    if (!m->j) return -1;
    m->nj = 0;
    const char *p = s;
    for (;;) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *d = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*d == '(') {
            const char *f = strchr(d, ')');
            p = f ? f + 1 : d + strlen(d);
            continue;
        }
        if (m->nj == cap) {
            cap *= 2;
            Jeton *n = realloc(m->j, (size_t)cap * sizeof *n);
            if (!n) return -1;
            m->j = n;
        }
        Jeton *j = &m->j[m->nj++];
        memset(j, 0, sizeof *j);
        j->t = copie_capitales(d, (size_t)(p - d));
        j->lien = -1;
        j->version = -1;
        if (!j->t) return -1;
    }
    return 0;
}

static int est(const Jeton *j, const char *mot) { return !strcmp(j->t, mot); }

/* Relie IF, ELSE et THEN, { et }, DO et LOOP, BEGIN et UNTIL ou REPEAT,
 * entre `debut` et `fin`. Rend 0, ou -1 si une structure est mal fermée. */
static int relie(Vm *m, int debut, int fin)
{
    int ouvert[PILE], n = 0;
    for (int i = debut; i < fin; i++) {
        Jeton *j = &m->j[i];
        if (est(j, "IF") || est(j, "{") || est(j, "DO") || est(j, "BEGIN")) {
            if (n == PILE) return -1;
            ouvert[n++] = i;
        } else if (est(j, "ELSE")) {
            if (!n || !est(&m->j[ouvert[n - 1]], "IF")) return -1;
            m->j[ouvert[n - 1]].lien = i;
            ouvert[n - 1] = i;
        } else if (est(j, "THEN")) {
            if (!n) return -1;
            Jeton *o = &m->j[ouvert[n - 1]];
            if (!est(o, "IF") && !est(o, "ELSE")) return -1;
            o->lien = i;
            n--;
        } else if (est(j, "}")) {
            if (!n || !est(&m->j[ouvert[n - 1]], "{")) return -1;
            m->j[ouvert[--n]].lien = i;
        } else if (est(j, "LOOP")) {
            if (!n || !est(&m->j[ouvert[n - 1]], "DO")) return -1;
            j->lien = ouvert[--n];
        } else if (est(j, "UNTIL") || est(j, "REPEAT")) {
            if (!n || !est(&m->j[ouvert[n - 1]], "BEGIN")) return -1;
            j->lien = ouvert[--n];
        } else if (est(j, "WHILE")) {
            /* Relié à son REPEAT quand on le trouvera : on cherche devant. */
            int k = i + 1, prof = 0;
            for (; k < fin; k++) {
                if (est(&m->j[k], "BEGIN")) prof++;
                else if (est(&m->j[k], "UNTIL")) prof--;
                else if (est(&m->j[k], "REPEAT") && prof-- == 0) break;
            }
            if (k == fin) return -1;
            j->lien = k;
        }
    }
    return n ? -1 : 0;
}

/* ---- le dictionnaire ---- */

static int cherche_def(const Vm *m, const char *nom)
{
    for (int i = m->ndefs - 1; i >= 0; i--)
        if (!strcmp(m->defs[i].nom, nom)) return i;
    return -1;
}

static int cherche_const(const Vm *m, const char *nom)
{
    for (int i = m->nconsts - 1; i >= 0; i--)
        if (!strcmp(m->consts[i].nom, nom)) return i;
    return -1;
}

static int nombre(const char *t, int *v)
{
    char *fin;
    long n = strtol(t, &fin, 10);
    if (fin == t || *fin) return 0;
    *v = (int)n;
    return 1;
}

/* Ce que désigne un mot, dans l'ordre : l'utilisatrice, puis les nombres,
 * puis les mots de base. */
static void resous(Vm *m, Jeton *j)
{
    if (j->fige || j->version == m->version) return;
    j->version = m->version;
    int k;
    if ((k = cherche_def(m, j->t)) >= 0)   { j->genre = K_DEF; j->val = k; return; }
    if ((k = cherche_const(m, j->t)) >= 0) {
        /* Une constante, ou une variable, qui rend son adresse. */
        j->genre = m->consts[k].variable ? K_VARIABLE : K_CONSTANTE;
        j->val = m->consts[k].val;
        return;
    }
    if (nombre(j->t, &j->val)) { j->genre = K_NOMBRE; return; }
    static const struct { const char *nom; int genre; } STRUCT[] = {
        { "IF", K_IF }, { "ELSE", K_ELSE }, { "THEN", K_THEN },
        { "{", K_ACCOLADE }, { "}", K_FERMANTE },
        { "DO", K_DO }, { "LOOP", K_LOOP }, { "I", K_I },
        { "BEGIN", K_BEGIN }, { "UNTIL", K_UNTIL }, { "WHILE", K_WHILE },
        { "REPEAT", K_REPEAT },
    };
    for (size_t i = 0; i < sizeof STRUCT / sizeof *STRUCT; i++)
        if (!strcmp(j->t, STRUCT[i].nom)) { j->genre = STRUCT[i].genre; return; }
    for (int v = 0; v < V_NOMBRE; v++)
        if (!strcmp(j->t, NOMS_VOISINS[v])) { j->genre = K_VOISIN; j->val = v; return; }
    for (size_t i = 0; i < sizeof PRIMITIVES / sizeof *PRIMITIVES; i++)
        if (!strcmp(j->t, PRIMITIVES[i].nom)) {
            j->genre = K_PRIM;
            j->val = PRIMITIVES[i].p;
            return;
        }
    j->genre = K_INCONNU;
}

/* ---- l'exécution ---- */

/* LES FAUTES, dans la forme de l'original : ses chaînes, lues dans son
 * code, « INTERPRET_ERR "Can't understand ", %i ». Le nombre n'est PAS
 * MESURÉ : l'original y met un compteur, sans doute la position du mot ;
 * on y met le rang du mot du premier niveau qui a échoué, à partir de 1. */
static const char ERR_COMPRIS[]  = "INTERPRET_ERR \"Can't understand \"";
static const char ERR_SEULEMENT[] =
    "INTERPRET_ERR \"End definition (;) Compile Only  \"";
static const char ERR_PILE[]     = "STACK_ERR \"Stack problem during execution \"";
static const char ERR_INCONNU[]  = "COMPILE_ERR \"Unknow'd word \"";
static const char ERR_COLON[]    = "COMPILE_ERR \"COLON inside definition  \"";
static const char ERR_IMBRIQUE[] = "NEST_ERR \"Unnested condition \"";
static const char ERR_MEMOIRE[]  = "out of memory";

static void faute(Vm *m, const char *quoi)
{
    if (!m->faute) m->faute = quoi;
}

static int depile(Vm *m)
{
    if (m->sp <= 0) { faute(m, ERR_PILE); return 0; }
    return m->pile[--m->sp];
}

static void empile(Vm *m, int v)
{
    if (m->sp >= PILE) { faute(m, ERR_PILE); return; }
    m->pile[m->sp++] = v;
}

/* VRAI vaut 1. NON MESURÉ contre -1 (la convention de Forth-83) : les deux
 * donnent les mêmes tables tant qu'on n'additionne pas des comparaisons. */
#define VRAI (-1)

static void primitive(Vm *m, int p)
{
    int a, b, c;
    switch (p) {
    case P_PLUS:    b = depile(m); a = depile(m); empile(m, a + b); break;
    case P_MOINS:   b = depile(m); a = depile(m); empile(m, a - b); break;
    case P_FOIS:    b = depile(m); a = depile(m); empile(m, a * b); break;
    case P_DIVISE:  b = depile(m); a = depile(m); empile(m, b ? a / b : 0); break;
    case P_MOD:     b = depile(m); a = depile(m); empile(m, b ? a % b : 0); break;
    case P_NEGATE:  empile(m, -depile(m)); break;
    case P_ABS:     a = depile(m); empile(m, a < 0 ? -a : a); break;
    case P_2FOIS:   empile(m, depile(m) * 2); break;
    case P_2DIVISE: empile(m, depile(m) / 2); break;
    case P_1PLUS:   empile(m, depile(m) + 1); break;
    case P_1MOINS:  empile(m, depile(m) - 1); break;
    case P_AND:     b = depile(m); a = depile(m); empile(m, a & b); break;
    case P_OR:      b = depile(m); a = depile(m); empile(m, a | b); break;
    case P_XOR:     b = depile(m); a = depile(m); empile(m, a ^ b); break;
    case P_NOT:     empile(m, depile(m) ? 0 : VRAI); break;
    case P_EGAL:    b = depile(m); a = depile(m); empile(m, a == b ? VRAI : 0); break;
    case P_INF:     b = depile(m); a = depile(m); empile(m, a < b ? VRAI : 0); break;
    case P_SUP:     b = depile(m); a = depile(m); empile(m, a > b ? VRAI : 0); break;
    case P_ZEGAL:   empile(m, depile(m) == 0 ? VRAI : 0); break;
    case P_ZINF:    empile(m, depile(m) < 0 ? VRAI : 0); break;
    case P_ZSUP:    empile(m, depile(m) > 0 ? VRAI : 0); break;
    case P_DUP:     a = depile(m); empile(m, a); empile(m, a); break;
    case P_DROP:    depile(m); break;
    case P_SWAP:    b = depile(m); a = depile(m); empile(m, b); empile(m, a); break;
    case P_OVER:    b = depile(m); a = depile(m); empile(m, a); empile(m, b);
                    empile(m, a); break;
    case P_ROT:     c = depile(m); b = depile(m); a = depile(m); empile(m, b);
                    empile(m, c); empile(m, a); break;
    case P_LIT:     a = depile(m);
                    empile(m, a >= 0 && a < CELLULES ? m->variables[a] : 0);
                    break;
    case P_ECRIT:   a = depile(m); b = depile(m);
                    if (a >= 0 && a < CELLULES) m->variables[a] = b;
                    break;
    case P_PLN0: case P_PLN1: case P_PLN2: case P_PLN3:
                    m->envoi[p - P_PLN0] = depile(m);
                    m->envoye |= 1 << (p - P_PLN0);
                    break;
    case P_BEEP:    break;
    }
}

static int execute(Vm *m, int debut, int fin);

static int execute_un(Vm *m, int i)
{
    Jeton *j = &m->j[i];
    resous(m, j);
    switch (j->genre) {
    case K_NOMBRE: case K_CONSTANTE: empile(m, j->val); break;
    case K_VARIABLE: empile(m, j->val); break;
    case K_VOISIN:   empile(m, m->voisin[j->val]); break;
    case K_PRIM:     primitive(m, j->val); break;
    case K_DEF:
        if (m->profondeur >= PROFONDEUR) { faute(m, ERR_PILE); break; }
        m->profondeur++;
        execute(m, m->defs[j->val].debut, m->defs[j->val].fin);
        m->profondeur--;
        break;
    default:
        faute(m, ERR_COMPRIS);     /* au premier niveau : les définitions sont vérifiées */
        break;
    }
    return m->faute ? -1 : 0;
}

static int execute(Vm *m, int debut, int fin)
{
    int i = debut;
    while (i < fin && !m->faute) {
        Jeton *j = &m->j[i];
        resous(m, j);
        switch (j->genre) {
        case K_IF:
            if (depile(m)) i++;
            else i = j->lien + 1;           /* après ELSE, ou après THEN */
            break;
        case K_ELSE:
            i = j->lien + 1;                /* la fin de la branche vraie */
            break;
        case K_THEN: case K_BEGIN:
            i++;
            break;
        case K_ACCOLADE: {
            /* { a b c } : le n-ième mot de la liste, n pris sur la pile.
             * Hors de la liste, 0 — MESURÉ sur « Grow flow », dont la somme
             * de neuf cellules atteint 9 pour une liste de neuf mots. */
            int n = depile(m), nb = j->lien - i - 1;
            if (n >= 0 && n < nb) execute_un(m, i + 1 + n);
            else empile(m, 0);
            i = j->lien + 1;
            break;
        }
        case K_DO: {
            int depart = depile(m), limite = depile(m);
            if (m->rp >= RETOUR) { faute(m, ERR_PILE); break; }
            m->retour[m->rp][0] = depart;
            m->retour[m->rp][1] = limite;
            m->rp++;
            i++;
            break;
        }
        case K_LOOP:
            if (!m->rp) { faute(m, ERR_IMBRIQUE); break; }
            if (++m->retour[m->rp - 1][0] < m->retour[m->rp - 1][1]) i = j->lien + 1;
            else { m->rp--; i++; }
            break;
        case K_I:
            empile(m, m->rp ? m->retour[m->rp - 1][0] : 0);
            i++;
            break;
        case K_UNTIL:
            i = depile(m) ? i + 1 : j->lien + 1;
            break;
        case K_WHILE:
            i = depile(m) ? i + 1 : j->lien + 1;
            break;
        case K_REPEAT:
            i = j->lien + 1;
            break;
        case K_FERMANTE:
            faute(m, ERR_IMBRIQUE);
            break;
        default:
            execute_un(m, i);
            i++;
            break;
        }
    }
    return m->faute ? -1 : 0;
}

/* ---- MAKE-TABLE ---- */

static int fait_table(Vm *m, int def, int voisinage, ForthSortie *s)
{
    int base[PILE], nbase = m->sp;
    memcpy(base, m->pile, sizeof base);
    int envoyes = 0;
    for (int e = 0; e < FORTH_TABLE; e++) {
        memset(m->voisin, 0, sizeof m->voisin);
        for (int b = 0; b < FORTH_BITS; b++) {
            int v = forth_disposition[voisinage][b];
            if (v >= 0) m->voisin[v] = (unsigned char)((e >> b) & 1);
        }
        memcpy(m->pile, base, sizeof base);
        m->sp = nbase;
        m->rp = 0;
        m->envoye = 0;
        if (execute(m, m->defs[def].debut, m->defs[def].fin) < 0) return -1;
        for (int k = 0; k < FORTH_PLANS; k++)
            if (m->envoye & (1 << k))
                s->plans[k][e] = (m->envoi[k] & 1) ? '1' : '0';
        envoyes |= m->envoye;
    }
    s->ecrits |= envoyes;
    return 0;
}

/* ---- le premier niveau ---- */

static void libere(Vm *m)
{
    for (int i = 0; i < m->nj; i++) free(m->j[i].t);
    free(m->j);
    free(m->defs);
    free(m->consts);
}

static int ajoute_def(Vm *m, const char *nom, int debut, int fin)
{
    Definition *d = realloc(m->defs, (size_t)(m->ndefs + 1) * sizeof *d);
    if (!d) return -1;
    m->defs = d;
    m->defs[m->ndefs++] = (Definition){ nom, debut, fin };
    m->version++;
    return 0;
}

static int ajoute_const(Vm *m, const char *nom, int val, int variable)
{
    Constante *c = realloc(m->consts, (size_t)(m->nconsts + 1) * sizeof *c);
    if (!c) return -1;
    m->consts = c;
    m->consts[m->nconsts++] = (Constante){ nom, val, variable };
    m->version++;
    return 0;
}

int forth_compile(const char *source, ForthSortie *s)
{
    memset(s, 0, sizeof *s);
    for (int k = 0; k < FORTH_PLANS; k++) memset(s->plans[k], '0', FORTH_TABLE);
    Vm *m = calloc(1, sizeof *m);
    if (!m) { snprintf(s->rendu, sizeof s->rendu, "%s", ERR_MEMOIRE); return -1; }
    if (decoupe(m, source ? source : "") < 0) {
        snprintf(s->rendu, sizeof s->rendu, "%s", ERR_MEMOIRE);
        libere(m);
        free(m);
        return -1;
    }
    int voisinage = FORTH_MOORE;
    int i = 0;
    while (i < m->nj && !m->faute) {
        Jeton *j = &m->j[i];
        m->position = i + 1;
        int d;
        for (d = 0; d < (int)(sizeof DECLARATIONS / sizeof *DECLARATIONS); d++)
            if (est(j, DECLARATIONS[d].nom)) break;
        if (d < (int)(sizeof DECLARATIONS / sizeof *DECLARATIONS)) {
            if (DECLARATIONS[d].voisinage >= 0) voisinage = DECLARATIONS[d].voisinage;
            i++;
            continue;
        }
        if (est(j, ":")) {
            int f = i + 2;
            while (f < m->nj && !est(&m->j[f], ";")) {
                if (est(&m->j[f], ":")) { faute(m, ERR_COLON); break; }
                f++;
            }
            if (m->faute) break;
            if (i + 1 >= m->nj || f >= m->nj) { faute(m, ERR_IMBRIQUE); break; }
            if (relie(m, i + 2, f) < 0) { faute(m, ERR_IMBRIQUE); break; }
            /* COMPILÉE, comme dans l'original : chaque mot est cherché MAINTENANT,
             * dans le dictionnaire tel qu'il est, et le reste. Un mot défini
             * plus tard est une faute ; une redéfinition plus tardive ne change
             * pas ce qu'appelle une définition déjà faite. */
            for (int k = i + 2; k < f; k++) {
                resous(m, &m->j[k]);
                if (m->j[k].genre == K_INCONNU) { faute(m, ERR_INCONNU); break; }
                m->j[k].fige = 1;
            }
            if (m->faute) break;
            if (ajoute_def(m, m->j[i + 1].t, i + 2, f) < 0) { faute(m, ERR_MEMOIRE); break; }
            i = f + 1;
            continue;
        }
        if (est(j, "MAKE-TABLE")) {
            int k = i + 1 < m->nj ? cherche_def(m, m->j[i + 1].t) : -1;
            if (k < 0) { faute(m, ERR_COMPRIS); break; }
            if (fait_table(m, k, voisinage, s) < 0) break;
            i += 2;
            continue;
        }
        if (est(j, "CONSTANT") || est(j, "VARIABLE")) {
            if (i + 1 >= m->nj) { faute(m, ERR_COMPRIS); break; }
            int variable = est(j, "VARIABLE"), v;
            if (!variable) v = depile(m);
            else if (m->nvariables >= CELLULES) { faute(m, ERR_PILE); break; }
            else v = m->nvariables++;
            if (m->faute) break;
            if (ajoute_const(m, m->j[i + 1].t, v, variable) < 0) {
                faute(m, ERR_MEMOIRE);
                break;
            }
            i += 2;
            continue;
        }
        /* Au premier niveau, un mot s'exécute tout de suite. « ; » et les
         * structures de contrôle n'y ont pas de sens : « Compile Only ». */
        if (est(j, ";")) { faute(m, ERR_SEULEMENT); break; }
        resous(m, j);
        if (j->genre >= K_IF && j->genre <= K_REPEAT) { faute(m, ERR_SEULEMENT); break; }
        execute_un(m, i);
        i++;
    }
    s->voisinage = voisinage;
    int r = 0;
    if (m->faute == ERR_MEMOIRE) {
        snprintf(s->rendu, sizeof s->rendu, "%s", m->faute);
        r = -1;
    } else if (m->faute) {
        snprintf(s->rendu, sizeof s->rendu, "%s, %d ", m->faute, m->position);
        r = -1;
    } else {
        size_t n = 0;
        for (int k = 0; k < m->sp && n < sizeof s->rendu - 16; k++) {
            int w = snprintf(s->rendu + n, sizeof s->rendu - n, "%d ", m->pile[k]);
            if (w > 0) n += (size_t)w;
        }
    }
    for (int k = 0; k < FORTH_PLANS; k++) s->plans[k][FORTH_TABLE] = '\0';
    libere(m);
    free(m);
    return r;
}
