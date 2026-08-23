/* hct_cmd.c — commandes HyperTalk, pilotées par une table de motifs.
 *
 * Le principe : chaque commande est décrite par un MOTIF, une petite chaîne
 * qui dit ce qui doit suivre le verbe. Ajouter une commande est donc une
 * ligne de table, pas une fonction — ce qui compte, vu qu'il y en a 65 au
 * guide et qu'on en découvrira d'autres.
 *
 * Alphabet des motifs, un élément par mot séparé par des espaces :
 *
 *   e            une expression
 *   c            un conteneur (variable, champ, chunk) — analysé comme e,
 *                la distinction relevant du sens, pas de la forme
 *   r            une référence d'objet ou une expression
 *   mot          ce mot exactement, obligatoire
 *   mot|mot|mot  l'un de ces mots, obligatoire
 *   [ ... ]      groupe facultatif ; sauté si le premier élément ne colle pas
 *   *            zéro à N expressions séparées par des virgules
 *   ...          tout le reste de la ligne, sans analyse (pour « do »)
 *
 * Les mots consommés par un motif deviennent des nœuds HCTN_MOTCLE, gardés
 * dans l'arbre : « put x into y » et « put x after y » ne se distinguent que
 * par là, et l'exécuteur en a besoin.
 *
 * Ce que le motif ne couvre pas — les formes vraiment irrégulières — est
 * traité par du code dédié, signalé au cas par cas.
 */

#include "hct_cmd.h"
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------ la table
 *
 * Ordre alphabétique, celui du guide. Les commandes dont le motif est "*"
 * n'ont pas de syntaxe propre : un verbe et des expressions, ce qui couvre
 * la majorité d'entre elles.
 */
static const HctCommande TABLE[] = {
    { "add",        "e to c" },
    { "answer",     "b [with b [or b [or b]]]" },
    { "answer file","e [of type e]" },
    { "arrowkey",   "e" },
    { "ask",        "e [with e]" },
    { "ask file",   "e [with e]" },
    { "beep",       "[e]" },
    { "choose",     "W [tool]" },
    { "click",      "at * [with *]" },
    { "close",      "[file] *" },
    { "commandkeydown", "e" },
    { "controlkey", "e" },
    { "convert",    "c [from *] to *" },
    { "create",     "*" },
    { "debug",      "*" },
    { "delete",     "e" },
    { "dial",       "e [with *]" },
    { "disable",    "r" },
    { "divide",     "c by e" },
    { "do",         "..." },
    { "domenu",     "*" },
    { "drag",       "from * to * [with *]" },
    { "edit",       "script of r" },
    { "enable",     "r" },
    { "enterinfield", "" },
    { "enterkey",   "" },
    { "exit",       "[to] *" },
    { "export",     "paint to file e" },
    { "find",       "*" },
    { "functionkey","e" },
    { "get",        "e" },
    { "global",     "*" },
    { "go",         "[to] *" },
    { "help",       "" },
    { "hide",       "*" },
    { "import",     "paint from file e" },
    { "keydown",    "e" },
    { "lock",       "*" },
    { "mark",       "*" },
    { "multiply",   "c by e" },
    { "next",       "repeat" },
    { "open",       "[file] *" },
    { "palette",    "*" },
    { "pass",       "e" },
    { "picture",    "*" },
    { "play",       "*" },
    { "pop",        "card [into c]" },
    { "print",      "* [with e]" },
    { "push",       "*" },
    { "put",        "e [into|before|after c]" },
    { "read",       "from file e [for|until e]" },
    { "reply",      "e [with keyword e]" },
    { "request",    "*" },
    { "reset",      "*" },
    { "return",     "[e]" },
    { "returnkey",  "" },
    { "returninfield", "" },
    { "save",       "r as e" },
    { "select",     "*" },
    { "send",       "e [to r]" },
    { "set",        "e to *" },
    { "show",       "* [at *]" },
    { "sort",       "* [ascending|descending] [text|numeric|international|datetime] [by e] " },
    { "start",      "using r" },
    { "stop",       "using r" },
    { "subtract",   "e from c" },
    { "tabkey",     "" },
    { "type",       "e [with *]" },
    { "unlock",     "*" },
    { "unmark",     "*" },
    { "visual",     "[effect] W" },
    { "wait",       "[while|until] [for] e [seconds|second|secs|sec|ticks|tick]" },
    { "write",      "e to file e" },
    { NULL, NULL }
};

/* Les verbes en deux mots — « answer file », « ask file » — doivent être
 * essayés avant leur forme courte, sans quoi « answer file "x" » serait pris
 * pour « answer » suivi d'une variable nommée « file ». */
static const char *DEUX_MOTS[] = { "answer file", "ask file", NULL };

const HctCommande *hct_commande_table(void) { return TABLE; }

/* ------------------------------------------------------------ services */

static int mot_egal(const HctJeton *j, const char *mot, int len)
{
    if (j->genre != HCT_IDENT) return 0;
    const char *t = j->norme ? j->norme : j->deb;
    int tl = j->norme ? (int)strlen(j->norme) : j->len;
    if (tl != len) return 0;
    for (int k = 0; k < len; k++)
        if (tolower((unsigned char)t[k]) != tolower((unsigned char)mot[k]))
            return 0;
    return 1;
}

/* Découpe le motif en éléments successifs. Rend la longueur de l'élément et
 * avance `p` derrière lui. */
static const char *element_suivant(const char **p, int *len)
{
    while (**p == ' ') (*p)++;
    if (!**p) return NULL;
    const char *deb = *p;
    if (**p == '[' || **p == ']') { (*p)++; *len = 1; return deb; }
    while (**p && **p != ' ' && **p != '[' && **p != ']') (*p)++;
    *len = (int)(*p - deb);
    return deb;
}

/* Un élément « mot|mot|mot » colle-t-il au jeton courant ? Rend l'indice de
 * l'alternative retenue, ou -1. */
static int alternative_colle(const HctJeton *j, const char *el, int len)
{
    int k = 0, deb = 0;
    for (int i = 0; i <= len; i++) {
        if (i == len || el[i] == '|') {
            if (mot_egal(j, el + deb, i - deb)) return k;
            deb = i + 1; k++;
        }
    }
    return -1;
}

/* ------------------------------------------------- application d'un motif
 *
 * Retour : 1 si le motif s'applique entièrement, 0 si l'on doit renoncer.
 * Les fautes rencontrées à l'intérieur d'un groupe obligatoire sont posées
 * dans l'arbre et n'interrompent pas : on veut toutes les signaler.
 */
static int applique(HctAnalyseur *a, const char *motif, HctNoeud *cmd)
{
    const char *p = motif;
    int len;
    const char *el;

    /* Profondeur des groupes facultatifs sautés. Tant qu'elle est non nulle,
     * on ne fait qu'avancer dans le motif sans rien consommer. */
    int saut = 0;

    while ((el = element_suivant(&p, &len)) != NULL) {

        if (len == 1 && *el == '[') {
            if (saut) { saut++; continue; }
            /* Le groupe s'ouvre : colle-t-il ? On regarde son premier
             * élément sans rien consommer. */
            const char *q = p;
            int l2;
            const char *e2 = element_suivant(&q, &l2);
            int colle = 0;
            if (e2) {
                if (l2 == 1 && *e2 == '*')       colle = !hct_expr_fini(a);
                else if (l2 == 3 && !memcmp(e2, "...", 3)) colle = !hct_expr_fini(a);
                else if (*e2 == 'e' && l2 == 1)  colle = !hct_expr_fini(a);
                else if (*e2 == 'c' && l2 == 1)  colle = !hct_expr_fini(a);
                else if (*e2 == 'r' && l2 == 1)  colle = !hct_expr_fini(a);
                else if (*e2 == 'b' && l2 == 1)  colle = !hct_expr_fini(a);
                else if (*e2 == 'W' && l2 == 1)  colle = !hct_expr_fini(a);
                else colle = alternative_colle(hct_expr_jeton(a), e2, l2) >= 0;
            }
            if (!colle) saut = 1;
            continue;
        }
        if (len == 1 && *el == ']') {
            if (saut) saut--;
            continue;
        }
        if (saut) continue;

        if (len == 1 && *el == '*') {
            /* Zéro à N expressions séparées par des virgules.
             *
             * « * » doit s'arrêter devant le mot que le motif attend ensuite,
             * sans quoi « sort by X » verrait « by » avalé comme une variable
             * et « show me at 1,2 » perdrait sa position. On regarde donc
             * l'élément suivant du motif, en sautant les crochets. */
            char but[256]; int lbut = 0; int prof = 0;
            {
                const char *q = p; int l2; const char *e2;
                while ((e2 = element_suivant(&q, &l2)) != NULL) {
                    if (l2 == 1 && *e2 == '[') { prof++; continue; }
                    if (l2 == 1 && *e2 == ']') { prof--; continue; }
                    if (l2 == 1 && (*e2=='e'||*e2=='c'||*e2=='r'||*e2=='b'
                                    ||*e2=='*'||*e2=='W')) break;
                    if (lbut && lbut < (int)sizeof but - 1) but[lbut++] = '|';
                    for (int k = 0; k < l2 && lbut < (int)sizeof but - 1; k++)
                        but[lbut++] = e2[k];
                    if (prof == 0) break;   /* littéral obligatoire : on arrête */
                }
                but[lbut] = 0;
            }
            const char *butoir = lbut ? but : NULL;
            while (!hct_expr_fini(a)) {
                if (butoir && alternative_colle(hct_expr_jeton(a), butoir, lbut) >= 0)
                    break;
                hct_ajoute_fils(a->reserve, cmd, hct_expression(a));
                if (hct_expr_virgule(a)) continue;
                break;
            }
            continue;
        }
        if (len == 3 && !memcmp(el, "...", 3)) {
            hct_ajoute_fils(a->reserve, cmd, hct_expression(a));
            continue;
        }
        if (len == 1 && *el == 'W') {
            /* Mots bruts jusqu'au mot suivant du motif. Un nom d'outil n'est
             * pas une expression : « choose line tool » ferait un morceau de
             * ligne si on l'analysait comme telle, et « spray can » tient en
             * deux mots. */
            const char *q = p; int l2; const char *e2 = element_suivant(&q, &l2);
            int pris = 0;
            while (!hct_expr_fini(a)) {
                const HctJeton *jj = hct_expr_jeton(a);
                if (jj->genre != HCT_IDENT) {
                    /* Apple écrit « choose "Select Tool" » aussi bien que
                     * « choose browse tool » : le nom peut être une chaîne,
                     * voire une variable. On accepte donc une expression
                     * quand aucun mot n'a encore été pris. */
                    if (!pris) hct_ajoute_fils(a->reserve, cmd, hct_expression(a));
                    break;
                }
                if (e2 && alternative_colle(jj, e2, l2) >= 0) break;
                hct_ajoute_fils(a->reserve, cmd, hct_expr_avale_mot(a));
                pris = 1;
            }
            continue;
        }
        if (len == 1 && *el == 'b') {
            /* Expression analysée SOUS le rang du « or » : dans
             * « answer X with "ok" or "non" », le « or » sépare deux boutons,
             * il n'est pas l'opérateur logique. */
            hct_ajoute_fils(a->reserve, cmd, hct_expression_sans_ou(a));
            continue;
        }
        if (len == 1 && (*el == 'e' || *el == 'c' || *el == 'r')) {
            hct_ajoute_fils(a->reserve, cmd, hct_expression(a));
            continue;
        }

        /* un mot imposé, seul ou en alternative */
        int k = alternative_colle(hct_expr_jeton(a), el, len);
        if (k < 0) return 0;                    /* le motif ne colle pas */
        HctNoeud *m = hct_expr_avale_motcle(a, el, len, k);
        hct_ajoute_fils(a->reserve, cmd, m);
    }
    return 1;
}

/* ---------------------------------------------------------- l'entrée */

HctNoeud *hct_commande(HctAnalyseur *a)
{
    const HctJeton *j = hct_expr_jeton(a);
    if (j->genre != HCT_IDENT) return NULL;

    /* verbes en deux mots d'abord */
    for (int i = 0; DEUX_MOTS[i]; i++) {
        const char *espace = strchr(DEUX_MOTS[i], ' ');
        int l1 = (int)(espace - DEUX_MOTS[i]);
        if (!mot_egal(j, DEUX_MOTS[i], l1)) continue;
        const HctJeton *j2 = hct_expr_jeton_apres(a, 1);
        if (!j2 || !mot_egal(j2, espace + 1, (int)strlen(espace + 1))) continue;
        for (int k = 0; TABLE[k].verbe; k++) {
            if (strcmp(TABLE[k].verbe, DEUX_MOTS[i])) continue;
            HctNoeud *cmd = hct_expr_ouvre_commande(a, TABLE[k].verbe, 2);
            if (!applique(a, TABLE[k].motif, cmd))
                hct_ajoute_fils(a->reserve, cmd,
                                hct_expr_faute(a, "forme inattendue"));
            return cmd;
        }
    }

    for (int k = 0; TABLE[k].verbe; k++) {
        if (strchr(TABLE[k].verbe, ' ')) continue;   /* déjà traité */
        if (!mot_egal(j, TABLE[k].verbe, (int)strlen(TABLE[k].verbe))) continue;

        int garde = a->i;
        HctNoeud *cmd = hct_expr_ouvre_commande(a, TABLE[k].verbe, 1);
        if (applique(a, TABLE[k].motif, cmd)) return cmd;

        /* Le motif n'a pas collé : ce n'était pas cette commande. On revient
         * et on laisse le filet des messages s'en charger — « put » employé
         * comme nom de variable est légal en HyperTalk. */
        a->i = garde;
        break;
    }
    return NULL;
}
