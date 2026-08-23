/* hct_verif.c — vérification d'un script. Voir hct_verif.h pour les niveaux. */

#include "hct_verif.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ------------------------------------------------------------ rapport */

static void ajoute(HctRapport *r, HctNiveau niv, int ligne, int col,
                   const char *msg, const char *extrait, int lex)
{
    /* Une faute du lexer est signalée deux fois : une fois au parcours des
     * jetons, une fois par le nœud HCTN_ERREUR que l'analyseur pose à sa
     * place. Même position, même message — on n'en garde qu'une, sans quoi
     * l'utilisateur verrait chaque guillemet non fermé en double. */
    for (int i = 0; i < r->n; i++)
        if (r->liste[i].ligne == ligne && r->liste[i].col == col &&
            msg && !strcmp(r->liste[i].message, msg))
            return;

    if (r->n == r->cap) {
        int c = r->cap ? r->cap * 2 : 16;
        HctSignalement *p = realloc(r->liste, (size_t)c * sizeof *p);
        if (!p) return;
        r->liste = p;
        r->cap = c;
    }
    HctSignalement *s = &r->liste[r->n++];
    s->niveau = niv;
    s->ligne = ligne;
    s->col = col;
    snprintf(s->message, sizeof s->message, "%s", msg ? msg : "");
    if (extrait && lex > 0) {
        int n = lex < (int)sizeof s->extrait - 1 ? lex : (int)sizeof s->extrait - 1;
        memcpy(s->extrait, extrait, (size_t)n);
        s->extrait[n] = '\0';
    } else s->extrait[0] = '\0';

    if (niv == HCT_V_ERREUR) r->nerreurs++;
    else                     r->navertissements++;
}

void hct_rapport_libere(HctRapport *r)
{
    free(r->liste);
    memset(r, 0, sizeof *r);
}

const HctSignalement *hct_premier(const HctRapport *r)
{
    const HctSignalement *meilleur = NULL;
    for (int i = 0; i < r->n; i++) {
        const HctSignalement *s = &r->liste[i];
        if (!meilleur) { meilleur = s; continue; }
        /* une erreur prime sur un avertissement ; à niveau égal, la première */
        if (s->niveau < meilleur->niveau) meilleur = s;
    }
    return meilleur;
}

/* ------------------------------------------------- vocabulaire connu
 *
 * Extrait de l'annexe I du guide Apple. Sert aux AVERTISSEMENTS seulement :
 * un nom absent de ces listes n'est pas une faute — un gestionnaire peut
 * porter n'importe quel nom, une propriété peut venir d'un XCMD — mais il
 * mérite d'être signalé, car c'est le plus souvent une coquille.
 *
 * « on mouseDwon » ne se déclenchera jamais, et rien à l'exécution ne le
 * dira : le gestionnaire est simplement ignoré. C'est le genre de faute qui
 * coûte une heure, et qu'aucune grammaire ne peut détecter.
 */
static const char *MESSAGES_SYSTEME[] = {
    "openstack","closestack","suspendstack","resumestack","startup","quit",
    "opencard","closecard","openbackground","closebackground",
    "openfield","closefield","exitfield","returninfield","enterinfield",
    "mouseup","mousedown","mousestilldown","mouseenter","mouseleave",
    "mousewithin","mousedoubleclick",
    "keydown","arrowkey","controlkey","commandkeydown","functionkey",
    "enterkey","returnkey","tabkey",
    "idle","newcard","newbackground","newfield","newbutton","newstack",
    "deletecard","deletebackground","deletefield","deletebutton","deletestack",
    "domenu","help","hide","show","resume","suspend",
    "opennow","closenow","hidewindow","showwindow","moveWindow","sizewindow",
    "appleevent","errordialog",
    NULL
};

static int connu(const char **table, const char *mot)
{
    for (int i = 0; table[i]; i++)
        if (!strcasecmp(table[i], mot)) return 1;
    return 0;
}

/* ------------------------------------------------------- parcours de l'arbre */

static void texte_du(const HctNoeud *n, char *out, int outlen)
{
    int l = n->jeton.len;
    if (l >= outlen) l = outlen - 1;
    if (l > 0) memcpy(out, n->jeton.deb, (size_t)l);
    out[l > 0 ? l : 0] = '\0';
}

/* Récolte les nœuds HCTN_ERREUR — les fautes de syntaxe proprement dites. */
static void recolte_erreurs(const HctNoeud *n, HctRapport *r)
{
    if (!n) return;
    if (n->genre == HCTN_ERREUR)
        ajoute(r, HCT_V_ERREUR, n->jeton.ligne, n->jeton.col,
               n->msg ? n->msg : "forme invalide", n->jeton.deb, n->jeton.len);
    for (int i = 0; i < n->nfils; i++) recolte_erreurs(n->fils[i], r);
}

/* Les avertissements : noms de gestionnaires suspects, « end » discordants. */
static void recolte_avertissements(const HctNoeud *n, HctRapport *r)
{
    if (!n) return;

    if (n->genre == HCTN_GESTIONNAIRE && n->nfils >= 1) {
        char nom[64];
        texte_du(n->fils[0], nom, sizeof nom);

        /* Seuls les « on » sont vérifiés : un « function » porte par nature
         * un nom inventé par l'auteur, alors qu'un « on » qui ne correspond à
         * aucun message système ne se déclenchera que s'il est appelé
         * explicitement — ce qui arrive, d'où le simple avertissement. */
        if (n->op && !strcasecmp(n->op, "on") &&
            !connu(MESSAGES_SYSTEME, nom)) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "« %.40s » n'est pas un message système connu",
                     nom);
            ajoute(r, HCT_V_AVERTISSEMENT, n->fils[0]->jeton.ligne,
                   n->fils[0]->jeton.col, msg, nom, (int)strlen(nom));
        }
    }

    for (int i = 0; i < n->nfils; i++) recolte_avertissements(n->fils[i], r);
}

/* ------------------------------------------------------------- l'entrée */

int hct_verifie(const char *src, HctRapport *rap, int avec_avertissements)
{
    memset(rap, 0, sizeof *rap);
    if (!src || !*src) return 0;

    HctLot lot;
    HctReserve reserve;
    memset(&reserve, 0, sizeof reserve);

    hct_lex(src, &lot);

    /* Les fautes du lexer d'abord : guillemet non fermé, caractère
     * inattendu. Elles portent déjà leur position. */
    for (int i = 0; i < lot.n; i++) {
        const HctJeton *j = &lot.jetons[i];
        if (j->genre == HCT_ERREUR)
            ajoute(rap, HCT_V_ERREUR, j->ligne, j->col,
                   j->msg ? j->msg : "jeton invalide", j->deb, j->len);
    }

    HctAnalyseur a;
    hct_analyseur_init(&a, &lot, &reserve);
    HctNoeud *arbre = hct_bloc_script(&a);

    recolte_erreurs(arbre, rap);
    if (avec_avertissements) recolte_avertissements(arbre, rap);

    hct_reserve_libere(&reserve);
    hct_lot_libere(&lot);

    return rap->nerreurs > 0;
}

int hct_rapport_texte(const HctRapport *r, char *out, int outlen)
{
    int p = 0;
    out[0] = '\0';
    for (int i = 0; i < r->n && p < outlen - 1; i++) {
        const HctSignalement *s = &r->liste[i];
        int n = snprintf(out + p, (size_t)(outlen - p),
                         "%s ligne %d, colonne %d : %s%s%s%s\n",
                         s->niveau == HCT_V_ERREUR ? "Erreur" : "Attention",
                         s->ligne, s->col, s->message,
                         s->extrait[0] ? "  [" : "",
                         s->extrait[0] ? s->extrait : "",
                         s->extrait[0] ? "]" : "");
        if (n < 0) break;
        p += n;
    }
    return p;
}
