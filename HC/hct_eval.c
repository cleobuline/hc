/* hct_eval.c — évaluation des expressions. Voir hct_eval.h pour l'hôte. */

#include "hct_eval.h"
#include "hct_chunk.h"
#include "hct_expr.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>

void hct_ctx_init(HctContexte *ctx, HctHote hote)
{
    ctx->hote = hote;
    ctx->erreur = NULL;
    ctx->fautif = NULL;
    ctx->prof_valeur = 0;
}
 

void hct_ctx_faute(HctContexte *ctx, const HctNoeud *n, const char *msg)
{
    if (ctx->erreur) return;        /* on garde la PREMIÈRE faute */
    ctx->erreur = msg;
    ctx->fautif = n;
}

/* ---------------------------------------------------------- constantes
 *
 * HyperTalk en compte une vingtaine. Elles ne sont pas des variables : on ne
 * peut pas leur affecter de valeur, et elles priment sur une variable de même
 * nom. `empty` vaut la chaîne vide, `return` le saut de ligne, etc. */
static const struct { const char *nom, *val; } CONSTANTES[] = {
    { "empty",    ""      },
    { "return",   "\n"    },
    { "space",    " "     },
    { "tab",      "\t"    },
    { "comma",    ","     },
    { "quote",    "\""    },
    { "colon",    ":"     },
    { "linefeed", "\n"    },
    { "formfeed", "\f"    },
    { "true",     "true"  },
    { "false",    "false" },
    { "up",       "up"    },
    { "down",     "down"  },
    { "zero",     "0"     }, { "one",   "1" }, { "two",   "2" },
    { "three",    "3"     }, { "four",  "4" }, { "five",  "5" },
    { "six",      "6"     }, { "seven", "7" }, { "eight", "8" },
    { "nine",     "9"     }, { "ten",   "10" },
    { NULL, NULL }
};

static const char *constante(const char *nom)
{
    for (int k = 0; CONSTANTES[k].nom; k++)
        if (!strcasecmp(nom, CONSTANTES[k].nom)) return CONSTANTES[k].val;
    return NULL;
}

/* « pi » a sa propre entrée : sa valeur est celle que HyperCard donne, à
 * vingt décimales, et non un M_PI tronqué. */
static const char *PI_HC = "3.14159265358979323846";

/* ------------------------------------------------------------ services */

static char *texte_du(const HctNoeud *n)
{
    char *s = malloc((size_t)n->jeton.len + 1);
    if (!s) return NULL;
    memcpy(s, n->jeton.deb, (size_t)n->jeton.len);
    s[n->jeton.len] = '\0';
    return s;
}

static HctValeur concat(HctValeur a, HctValeur b, const char *entre)
{
    int le = entre ? (int)strlen(entre) : 0;
    int n = a.len + le + b.len;
    HctValeur r;
    r.txt = malloc((size_t)n + 1);
    if (!r.txt) { r.len = 0; return r; }
    memcpy(r.txt, a.txt, (size_t)a.len);
    if (le) memcpy(r.txt + a.len, entre, (size_t)le);
    memcpy(r.txt + a.len + le, b.txt, (size_t)b.len);
    r.txt[n] = '\0';
    r.len = n;
    return r;
}

/* --------------------------------------------------------- arithmétique */

/* Une chaine vide, ou faite de blancs, vaut-elle pour rien ? */
static int est_vide(const char *s)
{
    if (!s) return 1;
    while (*s) { if (!isspace((unsigned char)*s)) return 0; s++; }
    return 1;
}

static HctValeur arith(HctContexte *ctx, const HctNoeud *n, const char *op,
                       HctValeur a, HctValeur b)
{
    /* Le VIDE vaut zero en arithmetique, comme dans HyperCard ou « empty + 1 »
     * donne 1.
     *
     * Ce n'est pas une commodite : des piles entieres en dependent. Graph Maker
     * boucle « repeat with slice = 1 to dataCount + 1 », un tour de plus que le
     * nombre de lignes pour refermer le camembert, et son dernier passage
     * calcule « pi * (line slice of pieData) / total » sur une ligne qui
     * n'existe pas. Refuser le vide y arretait le trace.
     *
     * Un texte non vide et non numerique reste une faute : c'est la difference
     * entre une case laissee blanche et une donnee erronee. */
    if (!est_vide(a.txt) && !hct_est_nombre(a.txt)) {
        hct_ctx_faute(ctx, n, "un nombre est attendu ici");
        return hct_val_vide();
    }
    if (!est_vide(b.txt) && !hct_est_nombre(b.txt)) {
        hct_ctx_faute(ctx, n, "un nombre est attendu ici");
        return hct_val_vide();
    }
    double x = est_vide(a.txt) ? 0 : hct_vers_nombre(a.txt);
    double y = est_vide(b.txt) ? 0 : hct_vers_nombre(b.txt);
    double r = 0;

    if      (!strcmp(op, "+")) r = x + y;
    else if (!strcmp(op, "-")) r = x - y;
    else if (!strcmp(op, "*")) r = x * y;
    else if (!strcmp(op, "/")) {
        if (y == 0) { hct_ctx_faute(ctx, n, "division par zéro"); return hct_val_vide(); }
        r = x / y;
    }
    else if (!strcmp(op, "div")) {
        if (y == 0) { hct_ctx_faute(ctx, n, "division par zéro"); return hct_val_vide(); }
        r = trunc(x / y);
    }
    else if (!strcmp(op, "mod")) {
        if (y == 0) { hct_ctx_faute(ctx, n, "division par zéro"); return hct_val_vide(); }
        r = fmod(x, y);
    }
    else if (!strcmp(op, "^")) r = pow(x, y);

    return hct_val_nombre(r);
}

/* ------------------------------------------------------- appartenance */

/* « a is in b » : recherche de sous-chaîne, casse ignorée comme partout
 * ailleurs en HyperTalk. */
static int contient(const char *grand, const char *petit)
{
    if (!*petit) return 1;
    size_t lg = strlen(grand), lp = strlen(petit);
    if (lp > lg) return 0;
    for (size_t i = 0; i + lp <= lg; i++)
        if (!strncasecmp(grand + i, petit, lp)) return 1;
    return 0;
}

/* « x is within r » : le point x est-il dans le rectangle r ?
 * Les deux sont des listes de nombres séparés par des virgules. */
static int dans_rect(const char *pt, const char *rect)
{
    double p[2], r[4];
    if (sscanf(pt, "%lf,%lf", &p[0], &p[1]) != 2) return 0;
    if (sscanf(rect, "%lf,%lf,%lf,%lf", &r[0], &r[1], &r[2], &r[3]) != 4) return 0;
    return p[0] >= r[0] && p[0] <= r[2] && p[1] >= r[1] && p[1] <= r[3];
}

/* « x is a number », « is an integer », « is a rect »… */
static int est_de_type(const char *v, const char *type)
{
    if (!strcasecmp(type, "number"))  return hct_est_nombre(v);
    if (!strcasecmp(type, "integer")) {
        if (!hct_est_nombre(v)) return 0;
        double x = hct_vers_nombre(v);
        return x == floor(x);
    }
    if (!strcasecmp(type, "logical") || !strcasecmp(type, "boolean")) {
        int ok; hct_vers_bool(v, &ok); return ok;
    }
    if (!strcasecmp(type, "point")) {
        double a, b; char reste;
        return sscanf(v, "%lf,%lf%c", &a, &b, &reste) == 2;
    }
    if (!strcasecmp(type, "rect") || !strcasecmp(type, "rectangle")) {
        double a, b, c, d; char reste;
        return sscanf(v, "%lf,%lf,%lf,%lf%c", &a, &b, &c, &d, &reste) == 4;
    }
    if (!strcasecmp(type, "date")) {
        int j, m, an;
        return sscanf(v, "%d/%d/%d", &m, &j, &an) == 3;
    }
    return 0;
}

/* ------------------------------------------------------------ binaires */

static HctValeur binaire(HctContexte *ctx, const HctNoeud *n)
{
    const char *op = n->op ? n->op : "";

    /* « and » et « or » évaluent à la demande : le second opérande n'est pas
     * calculé si le premier suffit. C'est ce que fait HyperCard, et ça compte
     * quand le second a des effets — un appel de fonction, par exemple. */
    if (!strcmp(op, "and") || !strcmp(op, "or")) {
        HctValeur a = hct_evalue(ctx, n->fils[0]);
        if (ctx->erreur) return a;
        int valide, va = hct_vers_bool(a.txt, &valide);
        if (!valide) {
            hct_ctx_faute(ctx, n->fils[0], "true ou false attendu ici");
            hct_val_libere(&a);
            return hct_val_vide();
        }
        hct_val_libere(&a);
        if (!strcmp(op, "and") && !va) return hct_val_bool(0);
        if (!strcmp(op, "or")  &&  va) return hct_val_bool(1);

        HctValeur b = hct_evalue(ctx, n->fils[1]);
        if (ctx->erreur) return b;
        int vb = hct_vers_bool(b.txt, &valide);
        if (!valide) {
            hct_ctx_faute(ctx, n->fils[1], "true ou false attendu ici");
            hct_val_libere(&b);
            return hct_val_vide();
        }
        hct_val_libere(&b);
        return hct_val_bool(vb);
    }
    /* « is a <type> » : le second opérande est un MOT-CLÉ, pas une valeur.
     *
     * L'évaluer était une faute : « date » est aussi une fonction, et
     * l'évaluation rendait la date du jour au lieu du mot « date ».
     * est_de_type recevait « 26/8/26 » et ne reconnaissait évidemment rien —
     * « x is a date » était donc toujours faux. Même piège pour « point »,
     * si un script définit une fonction de ce nom. */
    if (!strcmp(op, "is a") || !strcmp(op, "is not a")) {
        HctValeur g = hct_evalue(ctx, n->fils[0]);
        if (ctx->erreur) return g;

        char type[32];
        int l = n->fils[1]->jeton.len;
        if (l > (int)sizeof type - 1) l = (int)sizeof type - 1;
        memcpy(type, n->fils[1]->jeton.deb, (size_t)l);
        type[l] = '\0';

        int vrai = est_de_type(g.txt, type);
        hct_val_libere(&g);
        return hct_val_bool(!strcmp(op, "is a") ? vrai : !vrai);
    }
    HctValeur a = hct_evalue(ctx, n->fils[0]);
    if (ctx->erreur) return a;
    HctValeur b = hct_evalue(ctx, n->fils[1]);
    if (ctx->erreur) { hct_val_libere(&a); return b; }

    HctValeur r;

    if (!strcmp(op, "&"))       r = concat(a, b, NULL);
    else if (!strcmp(op, "&&")) r = concat(a, b, " ");
    else if (!strcmp(op, "+") || !strcmp(op, "-") || !strcmp(op, "*") ||
             !strcmp(op, "/") || !strcmp(op, "div") || !strcmp(op, "mod") ||
             !strcmp(op, "^"))
        r = arith(ctx, n, op, a, b);
    else if (!strcmp(op, "=") || !strcmp(op, "is"))
        r = hct_val_bool(hct_egal(a.txt, b.txt));
    else if (!strcmp(op, "<>") || !strcmp(op, "is not"))
        r = hct_val_bool(!hct_egal(a.txt, b.txt));
    else if (!strcmp(op, "<"))  r = hct_val_bool(hct_compare(a.txt, b.txt, NULL) <  0);
    else if (!strcmp(op, ">"))  r = hct_val_bool(hct_compare(a.txt, b.txt, NULL) >  0);
    else if (!strcmp(op, "<=")) r = hct_val_bool(hct_compare(a.txt, b.txt, NULL) <= 0);
    else if (!strcmp(op, ">=")) r = hct_val_bool(hct_compare(a.txt, b.txt, NULL) >= 0);
    else if (!strcmp(op, "contains"))   r = hct_val_bool(contient(a.txt, b.txt));
    else if (!strcmp(op, "is in"))      r = hct_val_bool(contient(b.txt, a.txt));
    else if (!strcmp(op, "is not in"))  r = hct_val_bool(!contient(b.txt, a.txt));
    else if (!strcmp(op, "is within"))  r = hct_val_bool(dans_rect(a.txt, b.txt));
    else if (!strcmp(op, "is not within")) r = hct_val_bool(!dans_rect(a.txt, b.txt));
    else if (!strcmp(op, "is a"))       r = hct_val_bool(est_de_type(a.txt, b.txt));
    else if (!strcmp(op, "is not a"))   r = hct_val_bool(!est_de_type(a.txt, b.txt));
    else {
        hct_ctx_faute(ctx, n, "opérateur inconnu");
        r = hct_val_vide();
    }

    hct_val_libere(&a);
    hct_val_libere(&b);
    return r;
}

/* ------------------------------------------------------------- unaires */

static HctValeur unaire(HctContexte *ctx, const HctNoeud *n)
{
    const char *op = n->op ? n->op : "";

    /* « there is a X » se traite AVANT d'évaluer son fils.
     *
     * Le fils est justement l'objet dont on demande l'existence : l'évaluer
     * pose une erreur quand il n'existe pas, et l'on sortait alors sans
     * jamais atteindre le recours. Or « there is a bg btn "absent" » doit
     * rendre false, pas échouer — c'est tout son intérêt. */
    if (strcmp(op, "not") && strcmp(op, "neg")) {
        /* L'objet se cherche par l'ARBRE quand l'hôte sait le résoudre.
         *
         * Le recours reconstitue le texte source et le confie à l'ancien
         * interpréteur, dont l'analyse de référence est moins complète :
         * « there is a button (nomDansUneVariable) » y rendait false, alors
         * que « the short name of button (nomDansUneVariable) » — qui passe
         * par le même désignateur entre parenthèses, mais par l'arbre —
         * trouvait le bouton. Résoudre ici règle l'incohérence et supprime
         * un aller-retour.
         *
         * On ne s'en charge que pour un nœud d'OBJET : « there is a
         * <expression> » a d'autres formes, que le recours garde. */
        if (n->nfils >= 1 && n->fils[0] &&
            n->fils[0]->genre == HCTN_OBJET && ctx->hote.resout) {
            int existe = ctx->hote.resout(ctx->hote.donnees, n->fils[0], ctx) != NULL;
            if (!strcmp(op, "there is no")) existe = !existe;
            return hct_val_bool(existe);
        }

        HctValeur vr;
        if (ctx->hote.recours &&
            ctx->hote.recours(ctx->hote.donnees, n, &vr)) return vr;
        return hct_val_bool(0);
    }

    HctValeur a = hct_evalue(ctx, n->fils[0]);
    if (ctx->erreur) return a;

    HctValeur r;
    if (!strcmp(op, "not")) {
        int valide, v = hct_vers_bool(a.txt, &valide);
        if (!valide) {
            hct_ctx_faute(ctx, n->fils[0], "true ou false attendu ici");
            r = hct_val_vide();
        } else r = hct_val_bool(!v);
    } else if (!strcmp(op, "neg")) {
        if (!hct_est_nombre(a.txt)) {
            hct_ctx_faute(ctx, n->fils[0], "un nombre est attendu ici");
            r = hct_val_vide();
        } else r = hct_val_nombre(-hct_vers_nombre(a.txt));
    } else {
        if (!hct_est_nombre(a.txt)) {
            hct_ctx_faute(ctx, n->fils[0], "un nombre est attendu ici");
            r = hct_val_vide();
        } else r = hct_val_nombre(-hct_vers_nombre(a.txt));
    }
    hct_val_libere(&a);
    return r;

}

/* ------------------------------------------------------------ feuilles */

static HctValeur feuille(HctContexte *ctx, const HctNoeud *n)
{
    if (n->genre == HCTN_NOMBRE || n->genre == HCTN_CHAINE)
        return hct_val_texte_n(n->jeton.deb, n->jeton.len);

    /* HCTN_IDENT : constante, variable, ou son propre nom. */
    char *nom = texte_du(n);
    if (!nom) return hct_val_vide();

    const char *c = constante(nom);
    if (c) { HctValeur v = hct_val_texte(c); free(nom); return v; }
    if (!strcasecmp(nom, "pi")) {
        HctValeur v = hct_val_texte(PI_HC); free(nom); return v;
    }

    HctValeur v;
    if (ctx->hote.lit_var && ctx->hote.lit_var(ctx->hote.donnees, nom, &v)) {
        free(nom);
        return v;
    }

    /* Une fonction sans parenthèses : « the ticks », « the mouse ». */
    if (ctx->hote.fonction &&
        ctx->hote.fonction(ctx->hote.donnees, nom, NULL, 0, &v)) {
        free(nom);
        return v;
    }

    /* Rien de tout cela : en HyperTalk, une variable jamais affectée vaut son
     * propre nom. C'est ce qui fait marcher « go card canard » sans
     * guillemets, et il ne faut donc PAS en faire une erreur. */
    v = hct_val_texte(nom);
    free(nom);
    return v;
}

/* -------------------------------------------------------------- appels */

/* Définie plus bas, avec « the value of » dont elle est le moteur. */
static int evalue_texte(HctContexte *ctx, const char *src,
                        const HctNoeud *origine, HctValeur *out);

static HctValeur appel(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return hct_val_vide();

    char *nom = texte_du(n->fils[0]);
    if (!nom) return hct_val_vide();

    int nargs = n->nfils - 1;
    HctValeur *args = nargs ? calloc((size_t)nargs, sizeof *args) : NULL;
    for (int i = 0; i < nargs; i++) {
        args[i] = hct_evalue(ctx, n->fils[i + 1]);
        if (ctx->erreur) {
            for (int k = 0; k <= i; k++) hct_val_libere(&args[k]);
            free(args); free(nom);
            return hct_val_vide();
        }
    }

    HctValeur r;
    int fait = 0;

    /* Fonctions purement calculatoires : l'exécuteur les fait lui-même, sans
     * déranger l'hôte. Celles qui dépendent du monde — the ticks, the mouse —
     * lui reviennent. */
    if (nargs == 1 && hct_est_nombre(args[0].txt)) {
        double x = hct_vers_nombre(args[0].txt);
        double y = 0; int ok = 1;
        if      (!strcasecmp(nom, "abs"))   y = fabs(x);
        else if (!strcasecmp(nom, "sqrt"))  y = sqrt(x);
        else if (!strcasecmp(nom, "trunc")) y = trunc(x);
        else if (!strcasecmp(nom, "round")) y = round(x);
        else if (!strcasecmp(nom, "sin"))   y = sin(x);
        else if (!strcasecmp(nom, "cos"))   y = cos(x);
        else if (!strcasecmp(nom, "tan"))   y = tan(x);
        else if (!strcasecmp(nom, "atan"))  y = atan(x);
        else if (!strcasecmp(nom, "exp"))   y = exp(x);
        else if (!strcasecmp(nom, "exp1"))  y = expm1(x);
        else if (!strcasecmp(nom, "exp2"))  y = exp2(x);
        else if (!strcasecmp(nom, "ln"))    y = log(x);
        else if (!strcasecmp(nom, "ln1"))   y = log1p(x);
        else if (!strcasecmp(nom, "log2"))  y = log2(x);
        else if (!strcasecmp(nom, "numtochar")) {
            char c[2] = { (char)(int)x, 0 };
            r = hct_val_texte(c); fait = 1; ok = 0;
        }
        else ok = 0;
        if (ok) { r = hct_val_nombre(y); fait = 1; }
    }
    if (!fait && nargs == 1 && !strcasecmp(nom, "length")) {
        r = hct_val_nombre(args[0].len); fait = 1;
    }
    if (!fait && nargs == 1 && !strcasecmp(nom, "chartonum")) {
        r = hct_val_nombre((unsigned char)args[0].txt[0]); fait = 1;
    }
    if (!fait && nargs == 1 && !strcasecmp(nom, "random")) {
        double m = hct_vers_nombre(args[0].txt);
        if (m < 1) m = 1;
        r = hct_val_nombre((double)(rand() % (long)m + 1)); fait = 1;
    }
    if (!fait && nargs == 2 && !strcasecmp(nom, "offset")) {
        const char *g = args[1].txt, *p = args[0].txt;
        size_t lp = strlen(p), lg = strlen(g);
        long pos = 0;
        if (lp && lp <= lg)
            for (size_t i = 0; i + lp <= lg; i++)
                if (!strncasecmp(g + i, p, lp)) { pos = (long)i + 1; break; }
        r = hct_val_nombre((double)pos); fait = 1;
    }
    /* value(x) : le pendant en forme d'appel de « the value of x ». Même
     * moteur, donc même règle — evalue_texte refuse ce qui n'est pas une
     * expression complète et propre, et l'on garde alors le recours, où
     * l'ancien évaluateur est plus tolérant. */
    if (!fait && nargs == 1 && !strcasecmp(nom, "value"))
        fait = evalue_texte(ctx, args[0].txt, n, &r);

    /* Fonctions financières d'HyperCard. Deux formules, rien de plus, et
     * aucun besoin du monde extérieur :
     *   annuity(taux, n)  = (1 − (1+taux)^−n) / taux
     *   compound(taux, n) = (1+taux)^n
     * Le taux nul est un cas limite légitime : l'annuité vaut alors le
     * nombre de périodes, et la division ferait une erreur. */
    if (!fait && nargs == 2 &&
        hct_est_nombre(args[0].txt) && hct_est_nombre(args[1].txt) &&
        (!strcasecmp(nom, "annuity") || !strcasecmp(nom, "compound"))) {
        double taux = hct_vers_nombre(args[0].txt);
        double per  = hct_vers_nombre(args[1].txt);
        double y;
        if (!strcasecmp(nom, "compound")) y = pow(1.0 + taux, per);
        else y = (taux == 0) ? per : (1.0 - pow(1.0 + taux, -per)) / taux;
        r = hct_val_nombre(y); fait = 1;
    }

    if (!fait && (!strcasecmp(nom, "min") || !strcasecmp(nom, "max") ||
                  !strcasecmp(nom, "sum") || !strcasecmp(nom, "average") ||
                  !strcasecmp(nom, "avg"))) {
        /* Ces quatre acceptent une liste de longueur quelconque. */
        double acc = 0; int compte = 0, premier = 1;
        for (int i = 0; i < nargs; i++) {
            if (!hct_est_nombre(args[i].txt)) continue;
            double x = hct_vers_nombre(args[i].txt);
            compte++;
            if (premier) { acc = x; premier = 0; }
            else if (!strcasecmp(nom, "min")) { if (x < acc) acc = x; }
            else if (!strcasecmp(nom, "max")) { if (x > acc) acc = x; }
            else acc += x;
        }
        if (!strcasecmp(nom, "average") || !strcasecmp(nom, "avg"))
            acc = compte ? acc / compte : 0;
        r = hct_val_nombre(acc); fait = 1;
    }

    if (!fait && ctx->hote.fonction)
        fait = ctx->hote.fonction(ctx->hote.donnees, nom, args, nargs, &r);

    /* Dernier recours : l'appel ENTIER, sous sa forme d'origine.
     *
     * C'est indispensable pour les fonctions écrites en HyperTalk par
     * l'utilisateur — « clickLine(me) » — que l'évaluateur ne peut pas
     * calculer et dont les arguments ne se reconstituent pas depuis leurs
     * valeurs : « me » désigne un OBJET, pas le texte de cet objet.
     *
     * L'hôte reçoit le nœud et retrouve le texte source exact. */
    if (!fait && ctx->hote.recours)
        fait = ctx->hote.recours(ctx->hote.donnees, n, &r);

    if (!fait) {
        hct_ctx_faute(ctx, n, "fonction inconnue");
        r = hct_val_vide();
    }

    for (int i = 0; i < nargs; i++) hct_val_libere(&args[i]);
    free(args);
    free(nom);
    return r;
}

/* ------------------------------------------------------------ morceaux
 *
 * Un nœud HCTN_CHUNK porte sa sorte, un ordinal éventuel, ses bornes en
 * enfants 0 et 1, et sa cible en dernier enfant. L'ordinal remplace les
 * bornes : « last word of x » n'a pas de rang écrit.
 */

static int rang_ordinal(HctOrdinal o, int total)
{
    switch (o) {
        case HCT_ORD_PREMIER:   return 1;
        case HCT_ORD_DEUXIEME:  return 2;
        case HCT_ORD_TROISIEME: return 3;
        case HCT_ORD_QUATRIEME: return 4;
        case HCT_ORD_CINQUIEME: return 5;
        case HCT_ORD_SIXIEME:   return 6;
        case HCT_ORD_SEPTIEME:  return 7;
        case HCT_ORD_HUITIEME:  return 8;
        case HCT_ORD_NEUVIEME:  return 9;
        case HCT_ORD_DIXIEME:   return 10;
        case HCT_ORD_MILIEU:
            /* total/2 + 1, et non (total+1)/2 : en arithmétique entière la
             * seconde forme donne 2 pour quatre éléments, alors que
             * HyperCard rend le TROISIÈME — vérifié : « middle item of
             * "a,b,c,d" » vaut « c ». Les deux formules coïncident sur un
             * nombre impair, ce qui masquait l'erreur. */
            return total > 0 ? total / 2 + 1 : 0;
        case HCT_ORD_DERNIER:   return total;
        case HCT_ORD_QUELCONQUE:return total > 0 ? (rand() % total) + 1 : 0;
        default:                return 0;
    }
}

/* Le séparateur d'items vient de l'hôte : « the itemDelimiter » est une
 * propriété globale, et un script peut la changer en cours de route. */
static char delimiteur(HctContexte *ctx)
{
    HctValeur v;
    if (ctx->hote.fonction &&
        ctx->hote.fonction(ctx->hote.donnees, "itemDelimiter", NULL, 0, &v)) {
        char d = v.txt[0] ? v.txt[0] : ',';
        hct_val_libere(&v);
        return d;
    }
    return ',';
}

static int rang_de(HctContexte *ctx, const HctNoeud *n, int *ok)
{
    HctValeur v = hct_evalue(ctx, n);
    *ok = 0;
    int r = 0;
    if (!ctx->erreur) {
        if (!hct_est_nombre(v.txt))
            hct_ctx_faute(ctx, n, "un rang numérique est attendu ici");
        else { r = (int)hct_vers_nombre(v.txt); *ok = 1; }
    }
    hct_val_libere(&v);
    return r;
}

static HctValeur chunk(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return hct_val_vide();

    /* La cible est toujours le dernier enfant. */
    HctValeur cible = hct_evalue(ctx, n->fils[n->nfils - 1]);
    if (ctx->erreur) return cible;

    char d = delimiteur(ctx);
    int n1 = 0, n2 = 0;

    if (n->ordinal) {
        int total = hct_chunk_compte(cible.txt, n->sorte, d);
        n1 = rang_ordinal(n->ordinal, total);
        if (n1 < 1) { hct_val_libere(&cible); return hct_val_vide(); }
    } else {
        int ok = 0;
        if (n->nfils >= 2) n1 = rang_de(ctx, n->fils[0], &ok);
        if (!ok) { hct_val_libere(&cible); return hct_val_vide(); }
        if (n->nfils >= 3) {
            n2 = rang_de(ctx, n->fils[1], &ok);
            if (!ok) { hct_val_libere(&cible); return hct_val_vide(); }
        }
    }

    HctValeur r = hct_chunk_lit(cible.txt, n->sorte, n1, n2, d);
    hct_val_libere(&cible);
    return r;
}

/* --------------------------------------------------- références d'objets
 *
 * « put bg field "Data" into x » : la référence désigne un objet, et sa
 * valeur est son CONTENU — le texte d'un champ, le nom d'un bouton. Seul
 * l'hôte sait résoudre et lire ; l'évaluateur ne fait que transmettre. */
static HctValeur objet(HctContexte *ctx, const HctNoeud *n)
{
    /* L'ARBRE d'abord, le texte ensuite.
     *
     * L'ordre inverse — recours en tête — rendait resout et lit_objet
     * INATTEIGNABLES chez tout hôte dont le recours est un fourre-tout : il
     * reconstitue le texte source de n'importe quel nœud et ne refuse que si
     * ce texte est vide, donc il répond toujours, donc les lignes suivantes
     * n'étaient jamais atteintes. Toute la résolution depuis l'arbre était
     * écrite et ne tournait pas, et les références repassaient par la
     * reconstitution avec les pertes qui vont avec — le « the » avalé, les
     * parenthèses d'un appel, le guillemet fermant d'une chaîne.
     *
     * Le recours reste en filet, pour deux cas : l'hôte qui ne sait pas
     * résoudre CE nœud, et celui qui préfère tout traiter lui-même et ne
     * fournit ni resout ni lit_objet. */
    HctValeur v;

    if (ctx->hote.resout && ctx->hote.lit_objet) {
        void *o = ctx->hote.resout(ctx->hote.donnees, n, ctx);
        if (o && ctx->hote.lit_objet(ctx->hote.donnees, o, &v)) return v;
    }

    if (ctx->hote.recours &&
        ctx->hote.recours(ctx->hote.donnees, n, &v)) return v;

    if (!ctx->hote.resout || !ctx->hote.lit_objet) {
        hct_ctx_faute(ctx, n, "aucun hôte pour résoudre cet objet");
    } else {
        hct_ctx_faute(ctx, n, "objet introuvable");
    }
    return hct_val_vide();
}

/* Relit un TEXTE comme une expression, et l'évalue dans le contexte courant.
 *
 * C'est ce que demande « the value of X » : évaluer X, puis considérer son
 * contenu comme du HyperTalk. Si x contient « card field 3 », la valeur est
 * le texte du champ, pas la chaîne « card field 3 ».
 *
 * Ce travail partait jusqu'ici au recours, qui reconstituait le texte source
 * et le confiait à l'ancien interpréteur — treize mille fois sur une seule
 * session de dessin. La bibliothèque sait désormais le faire : elle a le
 * lexer et l'analyseur qui lui manquaient quand ce chemin a été écrit.
 *
 * Rend 0 si le texte n'est PAS une expression complète et propre ; l'appelant
 * garde alors l'ancien chemin. Ce refus est délibéré : « a,b,c » s'analyse
 * sans faute jusqu'à la virgule et rendrait « a », alors que HyperCard rend
 * la chaîne entière. On exige donc que l'analyse ait consommé toute la ligne.
 */
static int evalue_texte(HctContexte *ctx, const char *src,
                        const HctNoeud *origine, HctValeur *out)
{
    if (!src)  return 0;
    if (!*src) { *out = hct_val_vide(); return 1; }

    if (ctx->prof_valeur >= 32) {
        hct_ctx_faute(ctx, origine, "« the value of » trop imbriqué");
        *out = hct_val_vide();
        return 1;
    }

    HctLot lot;
    if (!hct_lex(src, &lot)) { hct_lot_libere(&lot); return 0; }

    HctReserve res;
    memset(&res, 0, sizeof res);

    HctAnalyseur a;
    hct_analyseur_init(&a, &lot, &res);
    HctNoeud *arbre = hct_expression(&a);

    /* Tout le texte doit avoir été consommé, et sans faute. */
    const HctJeton *reste = &lot.jetons[a.i];
    if (!arbre || a.nerreurs ||
        (reste->genre != HCT_FIN && reste->genre != HCT_EOL)) {
        hct_reserve_libere(&res);
        hct_lot_libere(&lot);
        return 0;
    }

    int deja = ctx->erreur != NULL;

    ctx->prof_valeur++;
    HctValeur v = hct_evalue(ctx, arbre);
    ctx->prof_valeur--;

    /* Le nœud fautif pointerait dans une réserve qu'on libère à l'instant :
     * on le ramène sur le nœud d'origine, qui vit dans le script et porte
     * donc une ligne et une colonne que l'utilisateur peut voir. */
    if (!deja && ctx->erreur) ctx->fautif = origine;

    hct_reserve_libere(&res);
    hct_lot_libere(&lot);

    *out = v;
    return 1;
}
/* ---------------------------------------------------- « X of Y »
 *
 * Trois choses très différentes prennent cette forme :
 *
 *   the number of words of X    un comptage, que l'exécuteur sait faire
 *   the name of card 3          une propriété, que seul l'hôte connaît
 *   the value of X              une évaluation du contenu
 */
static HctValeur noeud_of(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 2) return hct_val_vide();
    const HctNoeud *quoi = n->fils[0];
    const HctNoeud *sur  = n->fils[1];

    if (quoi->genre == HCTN_IDENT) {
        char *nom = texte_du(quoi);
        if (!nom) return hct_val_vide();

        /* « the number of <morceaux> of X » : le fils est un CHUNK sans
         * bornes, dont le dernier enfant porte la cible. */
        if (!strcasecmp(nom, "number") && sur->genre == HCTN_CHUNK &&
            sur->nfils >= 1) {
            HctValeur cible = hct_evalue(ctx, sur->fils[sur->nfils - 1]);
            if (!ctx->erreur) {
                char d = delimiteur(ctx);
                int c = hct_chunk_compte(cible.txt, sur->sorte, d);
                hct_val_libere(&cible);
                free(nom);
                return hct_val_nombre(c);
            }
            hct_val_libere(&cible);
            free(nom);
            return hct_val_vide();
        }
        /* « the number of cards of <fond> » : un comptage d'OBJETS.
         *
         * Le bloc précédent ne couvre que les morceaux — char, word, item,
         * line. Ici la cible est une référence d'objet, et rien ne la
         * traitait : on tombait dans le chemin des propriétés, qui résolvait
         * « cards of this background » en la carte courante et lisait sa
         * propriété « number ». D'où le total de la pile au lieu du compte
         * par fond.
         *
         * On confie donc le nœud entier au recours, qui reconstitue le texte
         * et le donne à term_value — laquelle sait compter par conteneur.
         *
         * Seulement le PLURIEL NU, cependant : « cards », « card buttons »,
         * qui n'ont pas de désignateur. Dès qu'il y en a un — « the number of
         * this card », « the number of card field "x" » —, ce n'est plus un
         * comptage mais le RANG de cet objet-là, et le chemin des propriétés
         * est exactement le bon : il résout l'objet, puis lit sa propriété
         * « number ». Y envoyer aussi le rang évite un aller-retour par le
         * texte pour la forme la plus courante de toutes, celle par laquelle
         * un script sait où il se trouve. */
        if (!strcasecmp(nom, "number") && sur->genre == HCTN_OBJET &&
            sur->designateur == HCT_DES_AUCUN &&
            sur->typeobj != HCT_OBJ_ME && sur->typeobj != HCT_OBJ_TARGET) {
            HctValeur vr;
            if (ctx->hote.recours &&
                ctx->hote.recours(ctx->hote.donnees, n, &vr)) {
                free(nom);
                return vr;
            }
        }
        /* « the length of X » s'écrit aussi bien que « length(X) ». */
        if (!strcasecmp(nom, "length")) {
            HctValeur v = hct_evalue(ctx, sur);
            if (ctx->erreur) { free(nom); return v; }
            HctValeur r = hct_val_nombre(v.len);
            hct_val_libere(&v);
            free(nom);
            return r;
        }

        /* « the value of X » : on évalue X, puis on RÉÉVALUE son contenu
         * comme une expression.
         *
         * C'est là toute la différence : si x contient « card field 3 »,
         * « the value of x » rend le TEXTE du champ, pas la chaîne
         * « card field 3 ». Rendre simplement la valeur de x cassait tout
         * script qui range un nom d'objet dans une variable — un motif très
         * courant.
         *
         * La bibliothèque s'en charge seule depuis qu'elle a son analyseur.
         * Le recours ne garde que ce que celui-ci refuse : un texte qui n'est
         * pas une expression complète, où l'ancien évaluateur reste plus
         * tolérant. */
        if (!strcasecmp(nom, "value")) {
            HctValeur v = hct_evalue(ctx, sur);
            if (ctx->erreur) { free(nom); return v; }

            HctValeur r;
            if (evalue_texte(ctx, v.txt, n, &r)) {
                hct_val_libere(&v);
                free(nom);
                return r;
            }
            hct_val_libere(&v);

            HctValeur vr;
            if (ctx->hote.recours &&
                ctx->hote.recours(ctx->hote.donnees, n, &vr)) {
                free(nom);
                return vr;
            }
            free(nom);
            return hct_evalue(ctx, sur);   /* sans hôte : au mieux */
        }
        /* Sinon c'est une propriété : seul l'hôte sait la lire.
         *
         * lit_prop AVANT le recours, pour la même raison que dans objet() :
         * un recours fourre-tout accepte tout, donc lit_prop ne serait jamais
         * appelée. Et le chemin par le texte perd ici précisément ce dont la
         * propriété a besoin — l'analyseur consomme « the » sans le ranger
         * dans aucun nœud, si bien que la reconstitution rend « name of card
         * field "x" » et non « the name of… ».
         *
         * Le recours garde tout ce que lit_prop refuse : les propriétés
         * globales, celles que l'hôte ne connaît que par son propre
         * évaluateur, et le cas où la cible ne se résout pas. */
        HctValeur vr;

        if (ctx->hote.resout && ctx->hote.lit_prop) {
            void *objet = ctx->hote.resout(ctx->hote.donnees, sur, ctx);
            if (objet) {
                HctValeur r;
                if (ctx->hote.lit_prop(ctx->hote.donnees, objet, nom, &r)) {
                    free(nom);
                    return r;
                }
            }
        }

        if (ctx->hote.recours &&
            ctx->hote.recours(ctx->hote.donnees, n, &vr)) {
            free(nom);
            return vr;
        }

        hct_ctx_faute(ctx, n, "propriété inconnue");
        free(nom);
        return hct_val_vide();
    }

    hct_ctx_faute(ctx, n, "forme « of » non gérée");
    return hct_val_vide();
}

/* ------------------------------------------------------------- entrée */

HctValeur hct_evalue(HctContexte *ctx, const HctNoeud *n)
{
    if (!n) return hct_val_vide();
    if (ctx->erreur) return hct_val_vide();

    switch (n->genre) {
        case HCTN_NOMBRE:
        case HCTN_CHAINE:
        case HCTN_IDENT:
            return feuille(ctx, n);

        case HCTN_BINAIRE: return binaire(ctx, n);
        case HCTN_UNAIRE:  return unaire(ctx, n);
        case HCTN_APPEL:   return appel(ctx, n);
        case HCTN_CHUNK:   return chunk(ctx, n);
        case HCTN_OF:      return noeud_of(ctx, n);
        case HCTN_OBJET:   return objet(ctx, n);

        case HCTN_ERREUR:
            hct_ctx_faute(ctx, n, n->msg ? n->msg : "expression invalide");
            return hct_val_vide();

        default:
            /* Les chunks, les références d'objets et les propriétés viendront
             * dans les modules suivants ; ici on signale plutôt que de rendre
             * une valeur fausse en silence. */
            hct_ctx_faute(ctx, n, "forme non encore évaluable");
            return hct_val_vide();
    }
}
