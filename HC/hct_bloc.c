/* hct_bloc.c — structures de contrôle et gestionnaires.
 *
 * C'est ici que HyperTalk est le moins régulier, et les formes ci-dessous
 * viennent toutes de scripts réels, pas du guide seul.
 *
 * IF, trois formes attestées :
 *
 *   if C then I                       tout sur une ligne
 *   if C then I \n else I             le « else » d'une ligne suit
 *   if C then \n bloc \n end if       « then » seul en fin de ligne
 *   if C \n then \n bloc \n end if    « then » seul sur SA ligne
 *
 * La dernière surprend, mais elle est dans le script du calendrier :
 *
 *     if (length(x) - r) mod (w * 7) = 0
 *     then
 *       put return after x
 *     end if
 *
 * REPEAT, six formes :
 *
 *   repeat                            sans fin, sortie par « exit repeat »
 *   repeat forever
 *   repeat N [times]
 *   repeat while C
 *   repeat until C
 *   repeat with v = D to F [by P]     « down to » pour décompter
 *
 * GESTIONNAIRES :
 *
 *   on NOM [p1, p2…] \n … \n end NOM
 *   function NOM [p1, p2…] \n … \n end NOM
 *
 * Le corps d'un bloc s'analyse jusqu'au mot qui le ferme. Un « end » manquant
 * n'interrompt pas : on pose la faute et on rend le bloc tel quel, pour que
 * le reste du script reste analysable et que toutes les erreurs sortent d'un
 * coup.
 */

#include "hct_bloc.h"
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------ services */

static int mot(const HctJeton *j, const char *m)
{
    if (j->genre != HCT_IDENT) return 0;
    const char *t = j->norme ? j->norme : j->deb;
    int tl = j->norme ? (int)strlen(j->norme) : j->len;
    int ml = (int)strlen(m);
    if (tl != ml) return 0;
    for (int k = 0; k < ml; k++)
        if (tolower((unsigned char)t[k]) != tolower((unsigned char)m[k]))
            return 0;
    return 1;
}

static int mot_ici_b(HctAnalyseur *a, const char *m)
{
    return mot(hct_expr_jeton(a), m);
}

/* Passe les fins de ligne. Les lignes vides ont déjà été fondues par le
 * lexer, mais un bloc peut en commencer par une. */
static void saute_eol(HctAnalyseur *a)
{
    while (hct_expr_jeton(a)->genre == HCT_EOL) hct_expr_avance(a);
}

static HctNoeud *ouvre(HctAnalyseur *a, HctGenreNoeud g, const char *op,
                       int nmots)
{
    HctJeton j = *hct_expr_jeton(a);
    for (int k = 0; k < nmots; k++) hct_expr_avance(a);
    HctNoeud *n = hct_noeud(a->reserve, g, j);
    if (n) n->op = op;
    return n;
}

/* Un bloc d'instructions, jusqu'à l'un des mots de fin. `fins` est une liste
 * terminée par NULL ; le mot trouvé n'est PAS consommé. */
static HctNoeud *corps(HctAnalyseur *a, const char **fins)
{
    HctNoeud *bloc = hct_noeud(a->reserve, HCTN_BLOC, *hct_expr_jeton(a));
    if (!bloc) return NULL;

    for (;;) {
        saute_eol(a);
        const HctJeton *j = hct_expr_jeton(a);
        if (j->genre == HCT_FIN) break;

        int stop = 0;
        for (int k = 0; fins[k]; k++)
            if (mot(j, fins[k])) { stop = 1; break; }
        if (stop) break;

        HctNoeud *i = hct_bloc_instruction(a);
        if (!i) break;
        hct_ajoute_fils(a->reserve, bloc, i);

        /* Une instruction doit se terminer avec sa ligne. Ce qui reste est
         * du texte que personne n'a su lire : on le signale et on saute la
         * ligne, plutôt que de boucler dessus. */
        if (hct_expr_jeton(a)->genre != HCT_EOL &&
            hct_expr_jeton(a)->genre != HCT_FIN) {
            hct_ajoute_fils(a->reserve, bloc,
                            hct_expr_faute(a, "texte inattendu en fin de ligne"));
            while (hct_expr_jeton(a)->genre != HCT_EOL &&
                   hct_expr_jeton(a)->genre != HCT_FIN)
                hct_expr_avance(a);
        }
    }
    return bloc;
}

/* Consomme « end X », en vérifiant que X est bien attendu. */
static void ferme(HctAnalyseur *a, HctNoeud *n, const char *attendu)
{
    saute_eol(a);
    if (!mot_ici_b(a, "end")) {
        hct_ajoute_fils(a->reserve, n,
                        hct_expr_faute(a, "« end » manquant"));
        return;
    }
    hct_expr_avance(a);
    if (attendu && !mot_ici_b(a, attendu)) {
        hct_ajoute_fils(a->reserve, n,
                        hct_expr_faute(a, "« end » ne correspond pas"));
        return;
    }
    if (!hct_expr_fini(a)) hct_expr_avance(a);
}

/* ----------------------------------------------------------------- IF */

static HctNoeud *analyse_if(HctAnalyseur *a)
{
    HctNoeud *n = ouvre(a, HCTN_SI, "if", 1);
    if (!n) return NULL;

    hct_ajoute_fils(a->reserve, n, hct_expression(a));   /* la condition */

    /* « then » peut être sur la ligne de la condition, ou sur la sienne. */
    saute_eol(a);
    if (!mot_ici_b(a, "then")) {
        hct_ajoute_fils(a->reserve, n, hct_expr_faute(a, "« then » attendu"));
        return n;
    }
    hct_expr_avance(a);

    static const char *FINS[] = { "else", "end", NULL };

    if (hct_expr_jeton(a)->genre == HCT_EOL) {
        /* forme à blocs */
        HctNoeud *b = corps(a, FINS);
        hct_ajoute_fils(a->reserve, n, b);

        saute_eol(a);
        if (mot_ici_b(a, "else")) {
            hct_expr_avance(a);
            if (hct_expr_jeton(a)->genre == HCT_EOL) {
                static const char *F2[] = { "end", NULL };
                hct_ajoute_fils(a->reserve, n, corps(a, F2));
            } else {
                /* « else » suivi d'une instruction SUR LA MÊME LIGNE clôt le
                 * si : il n'y a pas de « end if ». Attesté dans le script du
                 * dé de MacCam :
                 *     if decay > 4 then
                 *       …
                 *     else add 2 to horz
                 *     repeat                 <- instruction suivante */
                HctNoeud *sinon = hct_noeud(a->reserve, HCTN_BLOC,
                                            *hct_expr_jeton(a));
                hct_ajoute_fils(a->reserve, sinon, hct_bloc_instruction(a));
                hct_ajoute_fils(a->reserve, n, sinon);
                return n;
            }
        }
        ferme(a, n, "if");
        return n;
    }

    /* forme d'une ligne : « if C then I », avec « else I » possible ensuite */
    HctNoeud *alors = hct_noeud(a->reserve, HCTN_BLOC, *hct_expr_jeton(a));
    hct_ajoute_fils(a->reserve, alors, hct_bloc_instruction(a));
    hct_ajoute_fils(a->reserve, n, alors);

    /* Le « else » qui suit peut être sur la ligne suivante — c'est le cas
     * dans le calendrier :  if … then drawCalendar it  /  else … */
    int garde = a->i;
    saute_eol(a);
    if (mot_ici_b(a, "else")) {
        hct_expr_avance(a);
        if (hct_expr_jeton(a)->genre == HCT_EOL) {
            static const char *F2[] = { "end", NULL };
            hct_ajoute_fils(a->reserve, n, corps(a, F2));
            ferme(a, n, "if");
        } else {
            HctNoeud *sinon = hct_noeud(a->reserve, HCTN_BLOC,
                                        *hct_expr_jeton(a));
            hct_ajoute_fils(a->reserve, sinon, hct_bloc_instruction(a));
            hct_ajoute_fils(a->reserve, n, sinon);
        }
    } else {
        a->i = garde;    /* pas de « else » : on rend la fin de ligne */
    }
    return n;
}

/* ------------------------------------------------------------- REPEAT */

static HctNoeud *analyse_repeat(HctAnalyseur *a)
{
    HctNoeud *n = ouvre(a, HCTN_REPETE, "repeat", 1);
    if (!n) return NULL;

    if (mot_ici_b(a, "forever")) {
        hct_expr_avance(a);
        n->op = "forever";
    } else if (mot_ici_b(a, "while") || mot_ici_b(a, "until")) {
        n->op = mot_ici_b(a, "while") ? "while" : "until";
        hct_expr_avance(a);
        hct_ajoute_fils(a->reserve, n, hct_expression(a));
    } else if (mot_ici_b(a, "with")) {
        /* repeat with v = D to F [by P], ou « down to » pour décompter */
        n->op = "with";
        hct_expr_avance(a);
        hct_ajoute_fils(a->reserve, n, hct_expr_avale_mot(a));   /* la variable */
        if (hct_expr_op_ici(a, "=")) hct_expr_avance(a);
        else hct_ajoute_fils(a->reserve, n, hct_expr_faute(a, "« = » attendu"));
        hct_ajoute_fils(a->reserve, n, hct_expression(a));       /* départ */
        if (mot_ici_b(a, "down")) { hct_expr_avance(a); n->op = "with down"; }
        if (mot_ici_b(a, "to")) hct_expr_avance(a);
        else hct_ajoute_fils(a->reserve, n, hct_expr_faute(a, "« to » attendu"));
        hct_ajoute_fils(a->reserve, n, hct_expression(a));       /* arrivée */
    } else if (hct_expr_jeton(a)->genre != HCT_EOL) {
        /* repeat N [times] */
        n->op = "times";
        hct_ajoute_fils(a->reserve, n, hct_expression(a));
        if (mot_ici_b(a, "times") || mot_ici_b(a, "time")) hct_expr_avance(a);
    } else {
        n->op = "forever";       /* « repeat » nu : boucle sans fin */
    }

    static const char *FINS[] = { "end", NULL };
    hct_ajoute_fils(a->reserve, n, corps(a, FINS));
    ferme(a, n, "repeat");
    return n;
}

/* ------------------------------------------------------ GESTIONNAIRES */

static HctNoeud *analyse_gestionnaire(HctAnalyseur *a, int fonction)
{
    HctNoeud *n = ouvre(a, HCTN_GESTIONNAIRE,
                        fonction ? "function" : "on", 1);
    if (!n) return NULL;

    if (hct_expr_jeton(a)->genre != HCT_IDENT) {
        hct_ajoute_fils(a->reserve, n, hct_expr_faute(a, "nom attendu"));
        return n;
    }
    HctJeton nom = *hct_expr_jeton(a);
    hct_ajoute_fils(a->reserve, n, hct_expr_avale_mot(a));

    /* Paramètres, séparés par des virgules :
     *   on markToday theNewDay,theOldDay
     *   function FindHandler HandlerName, HandlerType, TheScript */
    HctNoeud *params = hct_noeud(a->reserve, HCTN_LISTE, *hct_expr_jeton(a));
    while (hct_expr_jeton(a)->genre == HCT_IDENT) {
        hct_ajoute_fils(a->reserve, params, hct_expr_avale_mot(a));
        if (hct_expr_virgule(a)) continue;
        break;
    }
    hct_ajoute_fils(a->reserve, n, params);

    static const char *FINS[] = { "end", NULL };
    hct_ajoute_fils(a->reserve, n, corps(a, FINS));

    /* Le « end » doit reprendre le nom du gestionnaire. */
    saute_eol(a);
    if (!mot_ici_b(a, "end")) {
        hct_ajoute_fils(a->reserve, n, hct_expr_faute(a, "« end » manquant"));
        return n;
    }
    hct_expr_avance(a);
    char attendu[128];
    hct_texte(&nom, attendu, sizeof attendu);
    if (!mot_ici_b(a, attendu))
        hct_ajoute_fils(a->reserve, n,
                        hct_expr_faute(a, "« end » ne reprend pas le nom"));
    else hct_expr_avance(a);
    return n;
}

/* ------------------------------------------------------------- entrées */

HctNoeud *hct_bloc_instruction(HctAnalyseur *a)
{
    saute_eol(a);
    if (hct_expr_fini(a)) return NULL;

    /* Même garde-fou que pour les expressions : des « if » ou des « repeat »
     * imbriqués sans fin feraient déborder la pile. */
    if (a->prof > 400) return hct_expr_faute(a, "structure trop imbriquée");

    if (mot_ici_b(a, "if"))       return analyse_if(a);
    if (mot_ici_b(a, "repeat"))   return analyse_repeat(a);
    if (mot_ici_b(a, "on"))       return analyse_gestionnaire(a, 0);
    if (mot_ici_b(a, "function")) return analyse_gestionnaire(a, 1);

    return hct_instruction(a);
}

HctNoeud *hct_bloc_script(HctAnalyseur *a)
{
    HctNoeud *s = hct_noeud(a->reserve, HCTN_BLOC, *hct_expr_jeton(a));
    if (!s) return NULL;
    for (;;) {
        saute_eol(a);
        if (hct_expr_fini(a)) break;
        HctNoeud *i = hct_bloc_instruction(a);
        if (!i) break;
        hct_ajoute_fils(a->reserve, s, i);
        if (hct_expr_jeton(a)->genre != HCT_EOL &&
            hct_expr_jeton(a)->genre != HCT_FIN) {
            hct_ajoute_fils(a->reserve, s,
                            hct_expr_faute(a, "texte inattendu en fin de ligne"));
            while (hct_expr_jeton(a)->genre != HCT_EOL &&
                   hct_expr_jeton(a)->genre != HCT_FIN)
                hct_expr_avance(a);
        }
    }
    return s;
}
