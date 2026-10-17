/* forth_coeur.c — le FORTH de MAC•CAM, sans HC autour (forth_coeur.h).
 *
 * LE MODÈLE EST L'ORIGINAL, LU DANS SON CODE 68000 (l'XFCN FORTH de MAC•CAM
 * Workshop, désassemblée ; docs/mesures/xcmd.txt), et dans la pile de
 * démonstration où l'utilisatrice le décrivait en 1990 (MAC•CAM FORTH,
 * jamais commise). L'original compilait du vrai code 68000 : A3 était la pile
 * des paramètres, A7 celle du système, où vivaient les adresses de retour et
 * les boucles. On en refait la mécanique, case pour case, parce que la pile de
 * démonstration s'en sert : HOP retire une adresse de retour par
 * « R> R> DROP DROP », et des mots y sont écrits en code machine par « , ».
 *
 *   · les nombres font 16 BITS, signés ; vrai vaut -1 ;
 *   · un mot entre au dictionnaire dès « : » — la récursion directe —, et
 *     la recherche rend la PREMIÈRE entrée de ce nom : une redéfinition ne
 *     cache pas l'ancienne ;
 *   · les majuscules comptent : « mille » n'est pas « MILLE » ;
 *   · au premier niveau, seuls « : », les mots du dictionnaire (ceux de
 *     l'utilisatrice, MAKE-TABLE, CONSTANT…), les constantes, les variables
 *     et les nombres agissent ; un mot de base y est sans effet ;
 *   · une faute s'ENVOIE à la carte (SendCardMessage), et FORTH() rend la
 *     pile quand même, sommet en tête.
 *
 * Écrit pour tourner dans un module de HC (le kit xcmd/, sans système) comme
 * dans un programme ordinaire, pour l'éprouver sur les oracles : il n'emploie
 * que ce que le kit fournit. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forth_coeur.h"

/* ---- la disposition des bits ---- */

/* LES HUIT TABLES DU FORTH D'ORIGINE, recopiées de son code, dans l'ordre
 * des bits (docs/mesures/xcmd.txt). Chaque déclaration recopie ses douze noms
 * dans les douze premières constantes : CENTER, NORTH… sont des constantes,
 * que MAKE-TABLE remplit à chaque entrée. Trois sont aussi MESURÉES sur les
 * tables de la pile — Moore, von Neumann, Margolus-PH —, et le moteur de
 * MACCAM fabrique ses index de la même façon. Les numéros sont ceux de
 * « major », dans les scripts comme dans MACCAM. */
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

/* ---- les mots ---- */

/* LES MOTS DE BASE, dans l'ordre de la table de l'original. Ils ne servent
 * que dans une définition. « ≠ », « ≥ » et « ≤ » y sont, mais leur
 * compilation tombait sur le piège _Debugger et n'émettait rien : ils sont
 * sans effet. Les trois sont écrits ici en UTF-8, celui des champs de HC. */
enum {
    P_DUP, P_DROP, P_SWAP, P_ROT, P_OVER,
    P_PLUS, P_MOINS, P_FOIS, P_DIVISE,
    P_NOT, P_AND, P_XOR, P_OR,
    P_INF, P_SUP, P_DIFF, P_EGAL, P_SUPEG, P_INFEG,
    P_ACCOLADE, P_FERMANTE, P_IF, P_THEN, P_ELSE, P_DO, P_LOOP,
    P_BEGIN, P_WHILE, P_REPEAT, P_UNTIL,
    P_VIRGULE, P_LIT, P_ECRIT, P_VERS_R, P_DE_R,
    P_NOMBRE
};

static const char *const PRIMITIVES[P_NOMBRE] = {
    "DUP", "DROP", "SWAP", "ROT", "OVER",
    "+", "-", "*", "/",
    "NOT", "AND", "XOR", "OR",
    "<", ">", "\xe2\x89\xa0", "=", "\xe2\x89\xa5", "\xe2\x89\xa4",
    "{", "}", "IF", "THEN", "ELSE", "DO", "LOOP",
    "BEGIN", "WHILE", "REPEAT", "UNTIL",
    ",", "@", "!", ">R", "R>",
};

/* LE DICTIONNAIRE de départ, dans l'ordre où l'original l'enregistre. Les
 * mots de l'utilisatrice s'y ajoutent ; on y cherche avant les mots de base. */
enum {
    M_MAKE_TABLE, M_PLN0, M_PLN1, M_PLN2, M_PLN3, M_BEEP,
    M_VARIABLE, M_CONSTANT, M_DECLARATION,
    M_INTERNES = M_DECLARATION + FORTH_VOISINAGES
};

static const char *const INTERNES[M_INTERNES] = {
    "MAKE-TABLE", ">PLN0", ">PLN1", ">PLN2", ">PLN3", "BEEP",
    "VARIABLE", "CONSTANT",
    "N/MOORE", "N/VONN", "N/MARG-PH", "N/MARG-HV",
    "N/MOORE-PH", "N/MOORE-HV", "N/VONN-PH", "N/VONN-HV",
};

/* ---- les tailles de l'original ---- */

enum {
    /* La pile des paramètres : 800 octets sous son fond, dans un tampon de
     * $2008. Au-delà du fond, des zéros : l'original n'y voyait pas de faute,
     * et un DROP de trop y lisait 0. */
    PILE_FOND   = 400,
    PILE_CASES  = 0x2008 / 2,
    PILE_MARGE  = 10,           /* MAKE-TABLE s'arrête à 20 octets du bas */
    /* La pile du système. L'original n'avait pas d'autre borne que la
     * mémoire, et une récursion sans fin « vous emporte très vite vers le
     * redémarrage obligatoire » ; ici, elle s'arrête en STACK_ERR. */
    RETOUR_CASES = 8192,
    /* La zone des constantes et des variables, $190 octets : les constantes
     * à partir de son origine (0, 2, 4…), les variables en dessous (-2, -4…).
     * @ et ! y lisent et y écrivent. */
    ZONE_CASES   = 0x190 / 2,
    ZONE_ORIGINE = 0xc6,
    CONSTANTES   = 100,
    VARIABLES    = 99,
    DICTIONNAIRE = 200
};

/* ---- le code compilé ---- */

enum {
    OP_LIT,         /* pousse arg */
    OP_CONST,       /* pousse la constante arg, lue à l'exécution */
    OP_APPEL,       /* appelle le mot arg du dictionnaire */
    OP_PRIM,        /* un mot de base, arg */
    OP_SAUT,        /* va en arg */
    OP_SAUTZ,       /* dépile ; va en arg si c'est 0 */
    OP_DO,          /* limite et départ, vers la pile retour */
    OP_TEST,        /* l'index avance ; si la limite est dépassée, va en arg */
    OP_FINDO,       /* retire index et limite de la pile retour */
    OP_ACCOLADE,    /* { … } : arg est son rang dans m->acc */
    OP_BRUT,        /* un mot de code machine, déposé par « , » : arg */
    OP_BRUT_FAUX,   /* « , » après autre chose qu'un nombre */
    OP_INSN,        /* une instruction 68000 décodée au « ; » : arg, dans m->insns */
    OP_RETOUR
};

typedef struct { int op, arg; } Instr;
typedef struct { const char *nom; int debut; } Mot;   /* debut < 0 : -1 - interne */

/* Les structures ouvertes pendant une définition. */
enum { S_IF, S_ELSE, S_BEGIN, S_WHILE, S_DO, S_ACCOLADE };
typedef struct { int genre, ou; } Ouvert;

typedef struct {
    char      **mots;        /* le texte, découpé */
    int         nmots;
    int         lu;          /* le prochain mot à lire */
    int         position;    /* le rang du dernier mot lu, commentaires compris */

    Mot        *dico;
    int         ndico;
    const char *constantes[CONSTANTES];
    int         nconstantes;
    const char *variables[VARIABLES];
    int         nvariables;
    short       zone[ZONE_CASES];

    Instr      *code;
    int         ncode, capcode;
    int        *acc;         /* pour chaque { … } : nombre, fin, puis les débuts */
    int         nacc, capacc;
    struct Insn *insns;      /* les instructions 68000 décodées */
    int         ninsns, capinsns;

    int         compile;     /* une définition est ouverte */
    int         rang_nom;    /* le rang de son nom, pour NEST_ERR */
    int         mal_imbrique;
    Ouvert      ouverts[64];
    int         nouverts;
    int         elements;    /* dans une { … } : les débuts, en attente */
    int         debuts[64];

    short       pile[PILE_CASES];
    int         a3;          /* le sommet ; vide : PILE_FOND */
    short       retour[RETOUR_CASES];
    int         a7;          /* le sommet ; vide : RETOUR_CASES */
    int         d[8];        /* les registres de données du code machine */

    int         voisinage;
    int         entree;      /* l'entrée de MAKE-TABLE ; >PLNk y écrit */
    int         pile_hors;   /* MAKE-TABLE a trouvé la pile hors de ses bornes */

    ForthSortie *s;
    const char  *faute;
    int          rang_faute;
} Vm;

/* ---- les fautes ---- */

/* LES FAUTES, dans la forme de l'original : ses chaînes, lues dans son code,
 * « INTERPRET_ERR "Can't understand ", %i ». Il les ENVOYAIT à la carte : les
 * piles de l'utilisatrice ont les gestionnaires qui les reçoivent, et qui
 * sélectionnent le mot fautif par « select word b of fld "forth" ». Le
 * nombre est donc le rang du mot dans le texte, commentaires compris — le
 * compteur que l'original avance à chaque mot lu. */
static const char ERR_COMPRIS[]   = "INTERPRET_ERR \"Can't understand \"";
static const char ERR_SEULEMENT[] =
    "INTERPRET_ERR \"End definition (;) Compile Only  \"";
static const char ERR_PILE[]      = "STACK_ERR \"Stack problem during execution \"";
static const char ERR_INCONNU[]   = "COMPILE_ERR \"Unknow'd word \"";
static const char ERR_COLON[]     = "COMPILE_ERR \"COLON inside definition  \"";
static const char ERR_IMBRIQUE[]  = "NEST_ERR \"Unnested condition \"";
static const char ERR_MEMOIRE[]   = "out of memory";

static void faute(Vm *m, const char *quoi, int rang)
{
    if (m->faute) return;
    m->faute = quoi;
    m->rang_faute = rang;
}

/* ---- le texte ---- */

/* LES MOTS sont séparés par des espaces et des retours — le retour de HC
 * comme celui du Mac. Une tabulation, l'original la gardait dans le mot. */
static int separateur(char c) { return c == ' ' || c == '\r' || c == '\n'; }

static int decoupe(Vm *m, const char *s)
{
    int cap = 64;
    m->mots = malloc((size_t)cap * sizeof *m->mots);
    if (!m->mots) return -1;
    for (const char *p = s;;) {
        while (*p && separateur(*p)) p++;
        if (!*p) break;
        const char *d = p;
        while (*p && !separateur(*p)) p++;
        if (m->nmots == cap) {
            cap *= 2;
            char **n = realloc(m->mots, (size_t)cap * sizeof *n);
            if (!n) return -1;
            m->mots = n;
        }
        char *t = malloc((size_t)(p - d) + 1);
        if (!t) return -1;
        memcpy(t, d, (size_t)(p - d));
        t[p - d] = '\0';
        m->mots[m->nmots++] = t;
    }
    return 0;
}

/* Le mot suivant, ou NULL. Chaque mot lu avance le compteur. */
static const char *lis(Vm *m)
{
    if (m->lu >= m->nmots) return NULL;
    m->position = ++m->lu;
    return m->mots[m->lu - 1];
}

/* « ( … ) » : un mot qui COMMENCE par « ( » ouvre le commentaire, le premier
 * mot qui COMMENCE par « ) » le ferme, et le mot d'après est lu. Sans « ) »,
 * l'original tournait sans fin ; ici, le texte s'arrête.
 *
 * UN ÉCART, VOULU : l'original ne sautait qu'UN commentaire par mot lu, et
 * deux de suite lui faisaient lire « ( » comme un mot — « Can't
 * understand ». Deux cartes de MAC•CAM Workshop, « Terminator 3• » et
 * « B-Gas Dentrite VF• », ont ALTERNATE mis entre parenthèses juste avant un
 * autre commentaire, retouche faite après leurs tables : le champ Pile de
 * Terminator 3• garde le -4 qu'ALTERNATE y laissait. HC saute tous les
 * commentaires qui se suivent, pour qu'elles se compilent encore. */
static const char *saute_commentaire(Vm *m, const char *t)
{
    while (t && t[0] == '(') {
        do t = lis(m); while (t && t[0] != ')');
        if (t) t = lis(m);
    }
    return t;
}

/* ---- le dictionnaire ---- */

static int cherche(const char *const *noms, int n, const char *t)
{
    for (int i = 0; i < n; i++)
        if (noms[i] && !strcmp(noms[i], t)) return i;
    return -1;
}

static int cherche_mot(const Vm *m, const char *t)
{
    for (int i = 0; i < m->ndico; i++)
        if (!strcmp(m->dico[i].nom, t)) return i;
    return -1;
}

/* Un nombre : un signe s'il en faut un, puis des chiffres, et rien d'autre.
 * L'original le lit en entier long, puis n'en garde que les 16 bits bas. */
static int nombre(const char *t, short *v)
{
    const char *p = t + (*t == '-' || *t == '+');
    if (!*p) return 0;
    for (const char *q = p; *q; q++)
        if (*q < '0' || *q > '9') return 0;
    *v = (short)(strtol(t, NULL, 10) & 0xffff);
    return 1;
}

enum { C_DEUX_POINTS, C_POINT_VIRGULE, C_MOT, C_PRIMITIVE, C_CONSTANTE,
       C_VARIABLE, C_NOMBRE, C_INCONNU };

/* Ce que désigne un mot, dans l'ordre de l'original : « : », « ; », le
 * dictionnaire, les mots de base, les constantes, les variables, les
 * nombres. */
static int classe(const Vm *m, const char *t, int *val)
{
    short v;
    if (!strcmp(t, ":")) return C_DEUX_POINTS;
    if (!strcmp(t, ";")) return C_POINT_VIRGULE;
    if ((*val = cherche_mot(m, t)) >= 0) return C_MOT;
    if ((*val = cherche(PRIMITIVES, P_NOMBRE, t)) >= 0) return C_PRIMITIVE;
    if ((*val = cherche(m->constantes, m->nconstantes, t)) >= 0) return C_CONSTANTE;
    if ((*val = cherche(m->variables, m->nvariables, t)) >= 0) return C_VARIABLE;
    if (nombre(t, &v)) { *val = v; return C_NOMBRE; }
    return C_INCONNU;
}

/* La zone des constantes et des variables, par son adresse en octets. */
static short *case_zone(Vm *m, int adresse)
{
    int o = adresse + ZONE_ORIGINE;
    if (o < 0 || (o & 1) || o / 2 >= ZONE_CASES) return NULL;
    return &m->zone[o / 2];
}

static int adresse_variable(int n) { return -2 * (n + 1); }

/* N/MOORE… : les noms de ses douze bits deviennent les douze premières
 * constantes. N/MOORE est posé au départ. */
static void declare(Vm *m, int v)
{
    m->voisinage = v;
    for (int b = 0; b < FORTH_BITS; b++)
        m->constantes[b] = forth_nom_voisin(forth_disposition[v][b]);
    if (m->nconstantes < FORTH_BITS) m->nconstantes = FORTH_BITS;
}

/* ---- les piles ---- */

static void empile(Vm *m, int v)
{
    if (m->a3 <= 0) { faute(m, ERR_PILE, m->position); return; }
    m->pile[--m->a3] = (short)v;
}

static short depile(Vm *m)
{
    if (m->a3 >= PILE_CASES) { faute(m, ERR_PILE, m->position); return 0; }
    return m->pile[m->a3++];
}

/* La case k sous le sommet. */
static short *case_pile(Vm *m, int k)
{
    if (m->a3 + k >= PILE_CASES) { faute(m, ERR_PILE, m->position); return NULL; }
    return &m->pile[m->a3 + k];
}

static void rempile(Vm *m, int v)
{
    if (m->a7 <= 0) { faute(m, ERR_PILE, m->position); return; }
    m->retour[--m->a7] = (short)v;
}

static short rdepile(Vm *m)
{
    if (m->a7 >= RETOUR_CASES) { faute(m, ERR_PILE, m->position); return 0; }
    return m->retour[m->a7++];
}

/* UNE ADRESSE DE RETOUR fait quatre octets, deux cases, le mot fort au
 * sommet — comme un JSR du 68000. « R> R> » la lit donc en deux nombres. */
#define RETOUR_HOTE 0x7fffffff

static void pousse_retour(Vm *m, int ip)
{
    rempile(m, ip & 0xffff);
    rempile(m, ip >> 16);
}

static int tire_retour(Vm *m)
{
    int fort = rdepile(m) & 0xffff, faible = rdepile(m) & 0xffff;
    return (fort << 16) | faible;
}

/* ---- le code ---- */

static int emets(Vm *m, int op, int arg)
{
    if (!m->code || m->ncode == m->capcode) {
        int cap = m->code ? 2 * m->capcode : 256;
        Instr *n = realloc(m->code, (size_t)cap * sizeof *n);
        if (!n) { faute(m, ERR_MEMOIRE, 0); return -1; }
        m->code = n;
        m->capcode = cap;
    }
    m->code[m->ncode] = (Instr){ op, arg };
    return m->ncode++;
}

static int ajoute_acc(Vm *m, int v)
{
    if (!m->acc || m->nacc == m->capacc) {
        int cap = m->acc ? 2 * m->capacc : 64;
        int *n = realloc(m->acc, (size_t)cap * sizeof *n);
        if (!n) { faute(m, ERR_MEMOIRE, 0); return -1; }
        m->acc = n;
        m->capacc = cap;
    }
    m->acc[m->nacc] = v;
    return m->nacc++;
}

/* ---- le code machine ---- */

/* « , » DÉPOSE UN MOT DE CODE 68000 dans la définition : la pile de
 * démonstration écrit ainsi EXIT, LEAVE, PICK et l'index des boucles. On en
 * exécute le petit jeu d'instructions qu'elle emploie — MOVE.W et MOVE.L
 * entre registres de données et les deux piles, ADDQ et SUBQ sur A3 et A7,
 * les décalages, NOP, RTS et le piège _Debugger —, sur les piles telles que
 * l'original les disposait. Le reste est refusé, par la faute
 * « Can't understand » du mot en cours. */

static int mot_brut(Vm *m, int *ip, int *w)
{
    if (*ip >= m->ncode || m->code[*ip].op != OP_BRUT) return 0;
    *w = m->code[(*ip)++].arg & 0xffff;
    return 1;
}

/* UNE INSTRUCTION DÉCODÉE : son genre, sa longueur en mots, extensions
 * comprises, et ses opérandes telles que ses mots les décrivent. Le décodage
 * ne dépend que du code, qui ne change plus une fois la définition faite :
 * il se fait une fois, au « ; », et l'exécution ne refait que le calcul des
 * adresses. Fermat y passe 1,27 million d'instructions. */
typedef struct { int mode, reg, xreg, xlong, deplacement; } Ea;
enum { I_RIEN, I_RTS, I_ADDQ, I_DECALE, I_MOVE, I_PICK };
typedef struct Insn {
    int genre, mots, longueur;
    int reg, n, gauche, logique;        /* ADDQ et SUBQ, les décalages ; I_PICK : Dn, et An en n */
    Ea  src, dst;                       /* MOVE */
} Insn;

static int decode_ea(Vm *m, int *ip, int mode, int reg, Ea *e)
{
    int w;
    *e = (Ea){ mode, reg, 0, 0, 0 };
    if (mode == 0) return 1;
    if (mode < 2 || mode > 6 || (reg != 0 && reg != 3 && reg != 7)) return 0;
    if (mode == 5) {
        if (!mot_brut(m, ip, &w)) return 0;
        e->deplacement = (short)w;
    } else if (mode == 6) {
        if (!mot_brut(m, ip, &w) || (w & 0x8000)) return 0;   /* un index en An : non */
        e->xreg = (w >> 12) & 7;
        e->xlong = (w & 0x800) != 0;
        e->deplacement = (signed char)(w & 0xff);
    }
    return 1;
}

/* Décode l'instruction qui commence en `*ip` : 1, ou 0 si elle n'est pas du
 * jeu. */
static int decode(Vm *m, int *ip, Insn *i)
{
    int w, debut = *ip;
    if (!mot_brut(m, ip, &w)) return 0;
    memset(i, 0, sizeof *i);
    if (w == 0x4e71 || w == 0xa9ff) i->genre = I_RIEN;    /* NOP ; _Debugger, sans débogueur */
    else if (w == 0x4e75) i->genre = I_RTS;
    else if ((w & 0xf038) == 0x5008 && (w & 0xc0) != 0xc0) {
        /* ADDQ et SUBQ #q, An */
        int q = (w >> 9) & 7;
        if (!q) q = 8;
        i->genre = I_ADDQ;
        i->reg = w & 7;
        i->n = w & 0x100 ? -q : q;
        if (i->reg != 3 && i->reg != 7) return 0;
    } else if ((w & 0xf000) == 0xe000 && (w & 0x20) == 0 && ((w >> 3) & 3) <= 1
               && ((w >> 6) & 3) != 3) {
        /* ASL, LSL, ASR, LSR #n, Dn — en mot ou en long ; l'octet : non */
        if (((w >> 6) & 3) == 0) return 0;
        i->genre = I_DECALE;
        i->n = (w >> 9) & 7;
        if (!i->n) i->n = 8;
        i->reg = w & 7;
        i->longueur = ((w >> 6) & 3) == 2 ? 4 : 2;
        i->gauche = (w & 0x100) != 0;
        i->logique = (w >> 3) & 1;
    } else if ((w & 0xc000) == 0 && ((w >> 12) == 3 || (w >> 12) == 2)) {
        /* MOVE.W et MOVE.L : les extensions de la source d'abord */
        i->genre = I_MOVE;
        i->longueur = (w >> 12) == 3 ? 2 : 4;
        if (!decode_ea(m, ip, (w >> 3) & 7, w & 7, &i->src)) return 0;
        if (!decode_ea(m, ip, (w >> 6) & 7, (w >> 9) & 7, &i->dst)) return 0;
    } else return 0;
    i->mots = *ip - debut;
    return 1;
}

/* LA CASE D'UNE OPÉRANDE, à l'exécution, en un seul passage : son adresse
 * calculée sur les index des piles, (An)+ et -(An) bougeant leur registre ;
 * NULL si elle sort de sa pile, ou tombe sur un octet impair — et le
 * registre ne bouge alors pas : A3 et A7 restent toujours dans leur pile.
 * A0 ne bouge pas : il désigne la zone des constantes et des variables. */
static short *case_ea(Vm *m, const Ea *e, int longueur)
{
    int *r = e->reg == 3 ? &m->a3 : e->reg == 7 ? &m->a7 : NULL;
    int octet = r ? 2 * *r : ZONE_ORIGINE, cases = longueur / 2;
    switch (e->mode) {
    case 4: octet -= longueur; break;
    case 5: octet += e->deplacement; break;
    case 6: octet += (e->xlong ? m->d[e->xreg] : (short)m->d[e->xreg]) + e->deplacement; break;
    default: break;
    }
    if (octet & 1) return NULL;
    int k = octet / 2, n = e->reg == 3 ? PILE_CASES : e->reg == 7 ? RETOUR_CASES : ZONE_CASES;
    if (k < 0 || k + cases > n) return NULL;
    if (r && e->mode == 3) *r += cases;
    if (r && e->mode == 4) *r -= cases;
    return (e->reg == 3 ? m->pile : e->reg == 7 ? m->retour : m->zone) + k;
}

static int lis_ea(Vm *m, const Ea *e, int longueur, int *v)
{
    if (e->mode == 0) {
        *v = longueur == 2 ? (short)m->d[e->reg] : m->d[e->reg];
        return 1;
    }
    short *c = case_ea(m, e, longueur);
    if (!c) return 0;
    *v = longueur == 2 ? c[0] : (int)(((unsigned)(unsigned short)c[0] << 16) | (unsigned short)c[1]);
    return 1;
}

static int ecris_ea(Vm *m, const Ea *e, int longueur, int v)
{
    if (e->mode == 0) {
        if (longueur == 2)
            m->d[e->reg] = (int)(((unsigned)m->d[e->reg] & 0xffff0000u) | (unsigned short)v);
        else
            m->d[e->reg] = v;
        return 1;
    }
    short *c = case_ea(m, e, longueur);
    if (!c) return 0;
    if (longueur == 2) c[0] = (short)v;
    else { c[0] = (short)((unsigned)v >> 16); c[1] = (short)v; }
    return 1;
}

/* Exécute une instruction décodée. Rend 1, 0 si une adresse sort des piles,
 * ou 2 pour un RTS, que l'appelant fait comme un retour. */
static int execute_insn(Vm *m, const Insn *i)
{
    int v;
    switch (i->genre) {
    case I_RIEN: return 1;
    case I_RTS:  return 2;
    case I_ADDQ: {                                  /* A3 ou A7, en octets */
        int *r = i->reg == 3 ? &m->a3 : &m->a7, k = *r + i->n / 2;
        if ((i->n & 1) || k < 0 || k > (i->reg == 3 ? PILE_CASES : RETOUR_CASES)) return 0;
        *r = k;
        return 1;
    }
    case I_DECALE: {
        int l = i->longueur;
        v = l == 2 ? (short)m->d[i->reg] : m->d[i->reg];
        if (i->gauche) v = (int)((unsigned)v << i->n);
        else if (i->logique) v = (int)((l == 2 ? (unsigned)(unsigned short)v : (unsigned)v) >> i->n);
        else v >>= i->n;
        Ea d = { 0, i->reg, 0, 0, 0 };
        return ecris_ea(m, &d, l, v);
    }
    case I_PICK: {
        /* MOVE.W (A3)+,Dn ; ASL.W #1,Dn ; MOVE.W 0(An,Dn.W),-(A3), d'un coup,
         * dans l'ordre des trois : la case n de la pile, ou de la pile retour. */
        if (m->a3 >= PILE_CASES) return 0;
        unsigned d = ((unsigned)m->d[i->reg] & 0xffff0000u) |
                     (unsigned short)(m->pile[m->a3++] << 1);
        m->d[i->reg] = (int)d;
        int octet = 2 * (i->n == 3 ? m->a3 : m->a7) + (short)d;
        int k = octet / 2;
        if ((octet & 1) || k < 0 || k >= (i->n == 3 ? PILE_CASES : RETOUR_CASES)) return 0;
        v = i->n == 3 ? m->pile[k] : m->retour[k];
        if (m->a3 <= 0) return 0;                   /* -(A3) : comme case_ea */
        m->pile[--m->a3] = (short)v;
        return 1;
    }
    default:                                        /* I_MOVE : la source, puis la destination */
        return lis_ea(m, &i->src, i->longueur, &v) && ecris_ea(m, &i->dst, i->longueur, v);
    }
}

/* LA SUITE DE PICK ET DE L'INDEX DES BOUCLES : « MOVE.W (A3)+,Dn ; ASL.W
 * #1,Dn ; MOVE.W 0(An,Dn.W),-(A3) », An valant A3 (PICK) ou A7 (l'index de
 * la pile de démonstration, que i, j et k appellent). */
static int est_pick(const Insn *a, const Insn *b, const Insn *c)
{
    int n = a->dst.reg;
    return a->genre == I_MOVE && a->longueur == 2 && a->src.mode == 3 && a->src.reg == 3
        && a->dst.mode == 0
        && b->genre == I_DECALE && b->longueur == 2 && b->gauche && b->n == 1 && b->reg == n
        && c->genre == I_MOVE && c->longueur == 2 && c->src.mode == 6
        && (c->src.reg == 3 || c->src.reg == 7) && c->src.xreg == n && !c->src.xlong
        && c->src.deplacement == 0 && c->dst.mode == 4 && c->dst.reg == 3;
}

/* Une instruction que le « ; » n'a pas décodée — un saut au milieu d'une
 * suite de mots, ou une instruction hors du jeu —, décodée ici. */
static int instruction(Vm *m, int *ip)
{
    Insn i;
    return decode(m, ip, &i) ? execute_insn(m, &i) : 0;
}

/* Au « ; » : chaque instruction de la définition est décodée une fois, et son
 * premier mot devient OP_INSN. Ses mots d'extension restent OP_BRUT, comme
 * une instruction hors du jeu, que l'exécution décode si elle y arrive. */
static void decode_definition(Vm *m, int debut)
{
    for (int q = debut; q < m->ncode && !m->faute;) {
        int ip = q;
        Insn i;
        if (m->code[q].op != OP_BRUT || !decode(m, &ip, &i)) { q++; continue; }
        if (!m->insns || m->ninsns == m->capinsns) {
            int cap = m->insns ? 2 * m->capinsns : 16;
            Insn *n = realloc(m->insns, (size_t)cap * sizeof *n);
            if (!n) { faute(m, ERR_MEMOIRE, 0); return; }
            m->insns = n;
            m->capinsns = cap;
        }
        m->insns[m->ninsns] = i;
        m->code[q] = (Instr){ OP_INSN, m->ninsns++ };
        q = ip;
    }
    /* PICK et l'index, fusionnés : un seul passage pour les trois. Chacune
     * reste décodée à sa place, pour un saut qui tomberait au milieu. */
    for (int q = debut; q < m->ncode && !m->faute; q++) {
        if (m->code[q].op != OP_INSN) continue;
        const Insn *a = &m->insns[m->code[q].arg];
        int q2 = q + a->mots;
        if (q2 >= m->ncode || m->code[q2].op != OP_INSN) continue;
        const Insn *b = &m->insns[m->code[q2].arg];
        int q3 = q2 + b->mots;
        if (q3 >= m->ncode || m->code[q3].op != OP_INSN) continue;
        const Insn *c = &m->insns[m->code[q3].arg];
        if (!est_pick(a, b, c)) continue;
        Insn f = { I_PICK, q3 + c->mots - q, 2, a->dst.reg, c->src.reg, 0, 0,
                   { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 } };
        if (m->ninsns == m->capinsns) {
            Insn *n = realloc(m->insns, (size_t)(2 * m->capinsns) * sizeof *n);
            if (!n) { faute(m, ERR_MEMOIRE, 0); return; }
            m->insns = n;
            m->capinsns *= 2;
        }
        m->insns[m->ninsns] = f;
        m->code[q].arg = m->ninsns++;
    }
}

/* ---- l'exécution ---- */

static void interne(Vm *m, int n);

static short vrai(int c) { return c ? -1 : 0; }

static void primitive(Vm *m, int p)
{
    short a, b, c, *x;
    switch (p) {
    case P_DUP:    if ((x = case_pile(m, 0))) empile(m, *x); break;
    case P_DROP:   depile(m); break;
    case P_SWAP:   b = depile(m); a = depile(m); empile(m, b); empile(m, a); break;
    case P_ROT:    c = depile(m); b = depile(m); a = depile(m);
                   empile(m, b); empile(m, c); empile(m, a); break;
    case P_OVER:   if ((x = case_pile(m, 1))) empile(m, *x); break;
    case P_PLUS:   b = depile(m); a = depile(m); empile(m, a + b); break;
    case P_MOINS:  b = depile(m); a = depile(m); empile(m, a - b); break;
    case P_FOIS:   b = depile(m); a = depile(m); empile(m, a * b); break;
    case P_DIVISE:
        /* DIVS.W : par zéro, l'original s'arrêtait sur une erreur système ;
         * ici 0. Un quotient trop grand laisse le dividende, comme le 68000. */
        b = depile(m); a = depile(m);
        if (!b) empile(m, 0);
        else if (a / b > 32767 || a / b < -32768) empile(m, a);
        else empile(m, a / b);
        break;
    case P_NOT:    empile(m, ~depile(m)); break;           /* NOT.W : bit à bit */
    case P_AND:    b = depile(m); a = depile(m); empile(m, a & b); break;
    case P_XOR:    b = depile(m); a = depile(m); empile(m, a ^ b); break;
    case P_OR:     b = depile(m); a = depile(m); empile(m, a | b); break;
    case P_INF:    b = depile(m); a = depile(m); empile(m, vrai(a < b)); break;
    case P_SUP:    b = depile(m); a = depile(m); empile(m, vrai(a > b)); break;
    case P_EGAL:   b = depile(m); a = depile(m); empile(m, vrai(a == b)); break;
    case P_LIT:
        if ((x = case_pile(m, 0))) {
            short *z = case_zone(m, *x);
            *x = z ? *z : 0;
        }
        break;
    case P_ECRIT: {
        a = depile(m); b = depile(m);
        short *z = case_zone(m, a);
        if (z) *z = b;
        break;
    }
    case P_VERS_R: rempile(m, depile(m)); break;
    case P_DE_R:   empile(m, rdepile(m)); break;
    default: break;                                         /* ≠ ≥ ≤ */
    }
}

/* Exécute le mot n du dictionnaire, jusqu'à son retour. */
static void execute(Vm *m, int n)
{
    if (m->dico[n].debut < 0) { interne(m, -1 - m->dico[n].debut); return; }
    int ip = m->dico[n].debut;
    pousse_retour(m, RETOUR_HOTE);
    while (!m->faute) {
        if (ip < 0 || ip >= m->ncode) { faute(m, ERR_PILE, m->position); return; }
        Instr in = m->code[ip++];
        switch (in.op) {
        case OP_LIT:   empile(m, in.arg); break;
        case OP_CONST: { short *z = case_zone(m, 2 * in.arg); empile(m, z ? *z : 0); break; }
        case OP_PRIM:  primitive(m, in.arg); break;
        case OP_SAUT:  ip = in.arg; break;
        case OP_SAUTZ: if (!depile(m)) ip = in.arg; break;
        case OP_DO: {
            short depart = depile(m), limite = depile(m);
            rempile(m, limite);
            rempile(m, depart);
            break;
        }
        case OP_TEST:
            /* L'index avance AVANT le corps, puis la limite est comparée :
             * « 10 0 DO » fait dix tours, l'index y valant 1 à 10. MESURÉ
             * sur Fermat, dont les 225 nombres ne sortent qu'ainsi. */
            if (m->a7 + 1 >= RETOUR_CASES) { faute(m, ERR_PILE, m->position); break; }
            m->retour[m->a7] = (short)(m->retour[m->a7] + 1);
            if (m->retour[m->a7 + 1] < m->retour[m->a7]) ip = in.arg;
            break;
        case OP_FINDO: rdepile(m); rdepile(m); break;
        case OP_ACCOLADE: {
            const int *a = &m->acc[in.arg];
            short k = depile(m);
            if (k >= 0 && k < a[0]) { pousse_retour(m, a[1]); ip = a[2 + k]; }
            else { empile(m, 0); ip = a[1]; }       /* MESURÉ sur « Grow flow » */
            break;
        }
        case OP_APPEL:
            if (m->dico[in.arg].debut < 0) interne(m, -1 - m->dico[in.arg].debut);
            else { pousse_retour(m, ip); ip = m->dico[in.arg].debut; }
            break;
        case OP_INSN: {
            const Insn *i = &m->insns[in.arg];
            int r = execute_insn(m, i);
            ip += i->mots - 1;
            if (!r) faute(m, ERR_COMPRIS, m->position);
            else if (r == 2 && (ip = tire_retour(m)) == RETOUR_HOTE) return;
            break;
        }
        case OP_BRUT: {
            ip--;
            int r = instruction(m, &ip);
            if (!r) faute(m, ERR_COMPRIS, m->position);
            else if (r == 2 && (ip = tire_retour(m)) == RETOUR_HOTE) return;
            break;
        }
        case OP_RETOUR:
            ip = tire_retour(m);
            if (ip == RETOUR_HOTE) return;
            break;
        default:                                    /* OP_BRUT_FAUX */
            faute(m, ERR_COMPRIS, m->position);
            break;
        }
    }
}

/* ---- les mots internes ---- */

static void ajoute_constante(Vm *m, const char *nom, short v)
{
    if (m->nconstantes >= CONSTANTES) { faute(m, ERR_MEMOIRE, 0); return; }
    m->constantes[m->nconstantes] = nom;
    m->zone[(ZONE_ORIGINE + 2 * m->nconstantes) / 2] = v;
    m->nconstantes++;
}

static void make_table(Vm *m)
{
    const char *t = lis(m);
    int n;
    if (!t || classe(m, t, &n) != C_MOT) return;     /* l'original ne disait rien */
    for (m->entree = 0; m->entree < FORTH_TABLE && !m->pile_hors && !m->faute; m->entree++) {
        for (int b = 0; b < FORTH_BITS; b++)
            m->zone[ZONE_ORIGINE / 2 + b] = (short)((m->entree >> b) & 1);
        /* LA PILE N'EST PAS REMISE entre deux entrées : la règle trouve ce
         * que la précédente a laissé. Elle est contrôlée avant chacune. */
        if (m->a3 > PILE_FOND || m->a3 < PILE_MARGE) m->pile_hors = 1;
        else execute(m, n);
    }
}

static void interne(Vm *m, int n)
{
    ForthSortie *s = m->s;
    switch (n) {
    case M_MAKE_TABLE: make_table(m); break;
    case M_PLN0: case M_PLN1: case M_PLN2: case M_PLN3: {
        int k = n - M_PLN0;
        short v = depile(m);
        if (m->entree < FORTH_TABLE) s->plans[k][m->entree] = v ? '1' : '0';
        s->ecrits |= 1 << k;
        break;
    }
    case M_BEEP: s->bips++; break;
    case M_VARIABLE: {
        const char *t = lis(m);
        if (!t) break;
        if (m->nvariables >= VARIABLES) { faute(m, ERR_MEMOIRE, 0); break; }
        m->variables[m->nvariables++] = t;
        break;
    }
    case M_CONSTANT: {
        const char *t = lis(m);
        if (!t) break;
        ajoute_constante(m, t, depile(m));
        break;
    }
    default:
        declare(m, n - M_DECLARATION);
        break;
    }
}

/* ---- la compilation ---- */

static Ouvert *ouvert(Vm *m, int genre)
{
    if (m->nouverts && m->ouverts[m->nouverts - 1].genre == genre)
        return &m->ouverts[m->nouverts - 1];
    m->mal_imbrique = 1;
    return NULL;
}

static void ouvre(Vm *m, int genre, int ou)
{
    if (m->nouverts == (int)(sizeof m->ouverts / sizeof *m->ouverts)) {
        m->mal_imbrique = 1;
        return;
    }
    m->ouverts[m->nouverts++] = (Ouvert){ genre, ou };
}

/* Un mot de base dans une définition : la structure qu'il ouvre ou ferme,
 * ou son effet. */
static void compile_primitive(Vm *m, int p)
{
    Ouvert *o;
    int k;
    switch (p) {
    case P_IF:
        ouvre(m, S_IF, emets(m, OP_SAUTZ, -1));
        break;
    case P_ELSE:
        if (!(o = ouvert(m, S_IF))) break;
        k = emets(m, OP_SAUT, -1);
        if (k < 0) break;
        m->code[o->ou].arg = m->ncode;
        *o = (Ouvert){ S_ELSE, k };
        break;
    case P_THEN:
        if (m->nouverts && (m->ouverts[m->nouverts - 1].genre == S_IF ||
                            m->ouverts[m->nouverts - 1].genre == S_ELSE)) {
            m->code[m->ouverts[--m->nouverts].ou].arg = m->ncode;
        } else m->mal_imbrique = 1;
        break;
    case P_BEGIN:
        ouvre(m, S_BEGIN, m->ncode);
        break;
    case P_WHILE:
        if (!ouvert(m, S_BEGIN)) break;
        ouvre(m, S_WHILE, emets(m, OP_SAUTZ, -1));
        break;
    case P_REPEAT:
        if (!(o = ouvert(m, S_WHILE)) || m->nouverts < 2 ||
            m->ouverts[m->nouverts - 2].genre != S_BEGIN) { m->mal_imbrique = 1; break; }
        emets(m, OP_SAUT, m->ouverts[m->nouverts - 2].ou);
        m->code[o->ou].arg = m->ncode;
        m->nouverts -= 2;
        break;
    case P_UNTIL:
        if (!(o = ouvert(m, S_BEGIN))) break;
        emets(m, OP_SAUTZ, o->ou);
        m->nouverts--;
        break;
    case P_DO:
        emets(m, OP_DO, 0);
        ouvre(m, S_DO, emets(m, OP_TEST, -1));
        break;
    case P_LOOP:
        if (!(o = ouvert(m, S_DO))) break;
        emets(m, OP_SAUT, o->ou);
        m->code[o->ou].arg = m->ncode;
        emets(m, OP_FINDO, 0);
        m->nouverts--;
        break;
    case P_ACCOLADE:
        if (m->elements >= 0) { m->mal_imbrique = 1; break; }
        ouvre(m, S_ACCOLADE, emets(m, OP_ACCOLADE, -1));
        m->elements = 0;
        break;
    case P_FERMANTE: {
        if (!(o = ouvert(m, S_ACCOLADE)) || m->elements < 0) break;
        int a = ajoute_acc(m, m->elements);
        if (a < 0) break;
        ajoute_acc(m, m->ncode);
        for (int i = 0; i < m->elements; i++) ajoute_acc(m, m->debuts[i]);
        m->code[o->ou].arg = a;
        m->elements = -1;
        m->nouverts--;
        break;
    }
    case P_VIRGULE: {
        /* Le nombre qui précède devient le mot de code lui-même. */
        Instr *d = m->ncode > 0 ? &m->code[m->ncode - 1] : NULL;
        if (d && d->op == OP_LIT) *d = (Instr){ OP_BRUT, d->arg & 0xffff };
        else if (d && d->op == OP_CONST) *d = (Instr){ OP_BRUT, 2 * d->arg };
        else emets(m, OP_BRUT_FAUX, 0);
        break;
    }
    case P_DIFF: case P_SUPEG: case P_INFEG:
        break;                          /* le piège _Debugger, et rien d'émis */
    default:
        emets(m, OP_PRIM, p);
        break;
    }
}

/* Dans une { … }, chaque mot est un élément : son code, puis un retour. */
static int element(Vm *m)
{
    if (m->elements < 0) return 0;
    if (m->elements == (int)(sizeof m->debuts / sizeof *m->debuts)) {
        m->mal_imbrique = 1;
        return 0;
    }
    m->debuts[m->elements++] = m->ncode;
    return 1;
}

static void compile_mot(Vm *m, const char *t)
{
    int v, k = classe(m, t, &v);
    int dans = k != C_PRIMITIVE || v != P_FERMANTE ? element(m) : 0;
    switch (k) {
    case C_DEUX_POINTS:   faute(m, ERR_COLON, m->position); return;
    case C_POINT_VIRGULE:
        emets(m, OP_RETOUR, 0);
        decode_definition(m, m->dico[m->ndico - 1].debut);
        m->compile = 0;
        if (m->nouverts || m->mal_imbrique || m->elements >= 0) {
            m->s->bips++;
            faute(m, ERR_IMBRIQUE, m->rang_nom);
        }
        return;
    case C_CONSTANTE:     emets(m, OP_CONST, v); break;
    case C_VARIABLE:      emets(m, OP_LIT, adresse_variable(v)); break;
    case C_NOMBRE:        emets(m, OP_LIT, v); break;
    case C_MOT:           emets(m, OP_APPEL, v); break;
    case C_PRIMITIVE:     compile_primitive(m, v); break;
    default:              faute(m, ERR_INCONNU, m->position); return;
    }
    if (dans) emets(m, OP_RETOUR, 0);
}

static void ouvre_definition(Vm *m)
{
    const char *t = lis(m);
    if (!t) return;
    if (m->ndico >= DICTIONNAIRE) { faute(m, ERR_MEMOIRE, 0); return; }
    /* LE MOT ENTRE AU DICTIONNAIRE MAINTENANT, avant son corps : il peut
     * s'appeler lui-même. « La récursion indirecte n'est pas possible » :
     * un mot défini plus loin n'est pas encore connu. */
    m->dico[m->ndico++] = (Mot){ t, m->ncode };
    m->compile = 1;
    m->rang_nom = m->position;
    m->nouverts = 0;
    m->mal_imbrique = 0;
    m->elements = -1;
}

/* ---- le premier niveau ---- */

static void interprete_mot(Vm *m, const char *t)
{
    int v, rang = m->position;
    short *z;
    switch (classe(m, t, &v)) {
    case C_DEUX_POINTS:   ouvre_definition(m); break;
    case C_POINT_VIRGULE: faute(m, ERR_SEULEMENT, rang); break;
    case C_MOT:
        m->pile_hors = 0;
        execute(m, v);
        if (m->pile_hors) faute(m, ERR_PILE, rang + 1);
        break;
    case C_PRIMITIVE:     break;              /* sans effet hors d'une définition */
    case C_CONSTANTE:
        z = case_zone(m, 2 * v);
        /* Deux SysBeep restés dans l'original : la treizième constante, et
         * une constante qui vaut 123, le code de « { ». */
        if (v == 12) m->s->bips++;
        if (z && *z == 123) m->s->bips++;
        empile(m, z ? *z : 0);
        break;
    case C_VARIABLE:      empile(m, adresse_variable(v)); break;
    case C_NOMBRE:        empile(m, v); break;
    default:              faute(m, ERR_COMPRIS, rang); break;
    }
}

static void libere(Vm *m)
{
    for (int i = 0; i < m->nmots; i++) free(m->mots[i]);
    free(m->mots);
    free(m->dico);
    free(m->code);
    free(m->acc);
    free(m->insns);
}

int forth_compile(const char *source, ForthSortie *s)
{
    memset(s, 0, sizeof *s);
    Vm *m = calloc(1, sizeof *m);
    if (m) m->dico = malloc(DICTIONNAIRE * sizeof *m->dico);
    if (!m || !m->dico || decoupe(m, source ? source : "") < 0) {
        snprintf(s->rendu, sizeof s->rendu, "%s", ERR_MEMOIRE);
        if (m) { libere(m); free(m); }
        return -1;
    }
    m->s = s;
    m->a3 = PILE_FOND;
    m->a7 = RETOUR_CASES;
    m->elements = -1;
    for (int i = 0; i < M_INTERNES; i++)
        m->dico[m->ndico++] = (Mot){ INTERNES[i], -1 - i };
    m->variables[m->nvariables++] = "BASE";
    m->variables[m->nvariables++] = "ALTERNATE";       /* d'où le « -4 » */
    *case_zone(m, adresse_variable(0)) = 10;
    declare(m, FORTH_MOORE);

    for (const char *t = lis(m); t && !m->faute; t = lis(m)) {
        t = saute_commentaire(m, t);
        if (!t) break;
        if (m->compile) compile_mot(m, t);
        else interprete_mot(m, t);
        if (m->pile_hors) break;
    }

    s->voisinage = m->voisinage;
    int r = 0;
    if (m->faute == ERR_MEMOIRE) {
        snprintf(s->rendu, sizeof s->rendu, "%s", ERR_MEMOIRE);
        r = -1;
    } else {
        if (m->faute) {
            snprintf(s->faute, sizeof s->faute, "%s, %d ", m->faute, m->rang_faute);
            r = -1;
        }
        size_t n = 0;
        for (int k = m->a3; k < PILE_FOND && n < sizeof s->rendu - 8; k++) {
            int w = snprintf(s->rendu + n, sizeof s->rendu - n, "%d ", m->pile[k]);
            if (w > 0) n += (size_t)w;
        }
    }
    libere(m);
    free(m);
    return r;
}
