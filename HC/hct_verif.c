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
/* ÉCRITS DANS LEUR CASSE D'ORIGINE, celle du guide. Les comparaisons sont
 * toutes insensibles à la casse — « on OPENCARD » reste reconnu —, mais ces
 * noms se RECOPIENT désormais dans les remarques, et « vouliez-vous dire
 * commandkeydown ? » à côté d'un script qui écrit commandKeyDown donne un
 * conseil qui a l'air faux. */
static const char *MESSAGES_SYSTEME[] = {
    "openStack","closeStack","suspendStack","resumeStack","startUp","quit",
    "openCard","closeCard","openBackground","closeBackground",
    "openField","closeField","exitField","returnInField","enterInField",
    "mouseUp","mouseDown","mouseStillDown","mouseEnter","mouseLeave",
    "mouseWithin","mouseDoubleClick",
    "keyDown","arrowKey","controlKey","commandKeyDown","functionKey",
    "enterKey","returnKey","tabKey",
    "idle","newCard","newBackground","newField","newButton","newStack",
    "deleteCard","deleteBackground","deleteField","deleteButton","deleteStack",
    "doMenu","help","hide","show","resume","suspend",
    "openNow","closeNow","hideWindow","showWindow","moveWindow","sizeWindow",
    "appleEvent","errorDialog",
    NULL
};

static int connu(const char **table, const char *mot)
{
    for (int i = 0; table[i]; i++)
        if (!strcasecmp(table[i], mot)) return 1;
    return 0;
}

/* Distance d'édition, plafonnée. Deux chaînes qui diffèrent de plus de `max`
 * n'ont pas à être comparées jusqu'au bout : on rend max + 1 dès qu'on le
 * dépasse, ce qui suffit à les écarter.
 *
 * Deux lignes de 64 octets sur la pile : les noms de gestionnaires sont bornés
 * par l'appelant, et une allocation ici serait un chemin d'échec de plus dans
 * un code qui ne sert qu'à rendre un conseil. */
#define VERIF_NOM_MAX 64
static int distance(const char *a, const char *b, int max)
{
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la >= VERIF_NOM_MAX || lb >= VERIF_NOM_MAX) return max + 1;
    if (la - lb > max || lb - la > max) return max + 1;

    int prec[VERIF_NOM_MAX + 1], cour[VERIF_NOM_MAX + 1];
    for (int j = 0; j <= lb; j++) prec[j] = j;

    for (int i = 1; i <= la; i++) {
        cour[0] = i;
        int meilleur = cour[0];
        for (int j = 1; j <= lb; j++) {
            int cout = (tolower((unsigned char)a[i-1]) ==
                        tolower((unsigned char)b[j-1])) ? 0 : 1;
            int d = prec[j] + 1;
            if (cour[j-1] + 1 < d) d = cour[j-1] + 1;
            if (prec[j-1] + cout < d) d = prec[j-1] + cout;
            cour[j] = d;
            if (d < meilleur) meilleur = d;
        }
        if (meilleur > max) return max + 1;      /* inutile de continuer */
        for (int j = 0; j <= lb; j++) prec[j] = cour[j];
    }
    return prec[lb];
}

/* LE MESSAGE SYSTÈME LE PLUS PROCHE, ou NULL.
 *
 * « n'est pas un message système connu » est vrai et n'apprend rien : il ne
 * dit pas ce que l'auteur voulait écrire. Le cas qui a motivé ceci est venu de
 * l'usage — un script portait « on commandKey » à côté de « on
 * commandKeyDown », et le premier ne se déclenche jamais. La remarque disait
 * bien qu'il était inconnu ; elle ne disait pas qu'il manquait quatre lettres.
 *
 * DEUX SIGNAUX, du plus sûr au moins sûr :
 *
 *   1. le nom écrit est un PRÉFIXE d'un message connu — « commandKey » dans
 *      « commandKeyDown ». C'est le cas le plus net : on a arrêté d'écrire
 *      trop tôt. Limité à huit lettres de différence, sans quoi « on » serait
 *      proposé comme un préfixe de tout.
 *
 *   2. une distance d'édition de 1 ou 2 — « mouseDwon » pour « mouseDown ».
 *      Réservée aux noms d'au moins cinq lettres : en dessous, deux fautes
 *      changent le mot entier et le conseil serait du hasard.
 *
 * On rend le plus proche, et rien du tout si personne n'approche. Un conseil
 * faux est pire que pas de conseil : il envoie corriger ce qui va bien. */
static const char *proche(const char *nom)
{
    size_t ln = strlen(nom);
    if (ln < 3) return NULL;

    const char *meilleur = NULL;
    int meilleure = 3;                     /* strictement mieux que 3 */
    const char *prefixe = NULL;
    int nprefixes = 0;

    for (int i = 0; MESSAGES_SYSTEME[i]; i++) {
        const char *cand = MESSAGES_SYSTEME[i];
        size_t lc = strlen(cand);

        if (lc > ln && lc - ln <= 8 && !strncasecmp(cand, nom, ln)) {
            if (!nprefixes) prefixe = cand;
            nprefixes++;
            continue;
        }

        if (ln < 5) continue;
        int d = distance(nom, cand, 2);
        if (d < meilleure) { meilleure = d; meilleur = cand; }
    }

    /* UN PRÉFIXE AMBIGU NE CONSEILLE RIEN. « on mouse » est le début de cinq
     * messages ; en proposer un serait tirer au sort, et un conseil faux
     * envoie corriger ce qui va bien. « commandKey » n'en a qu'un, et c'est
     * justement pour ce cas-là que la remarque existe. */
    if (nprefixes == 1) return prefixe;
    if (nprefixes > 1)  return NULL;
    return meilleur;
}

/* ------------------------------------------------------- parcours de l'arbre */

static void texte_du(const HctNoeud *n, char *out, int outlen)
{
    int l = n->jeton.len;
    if (l >= outlen) l = outlen - 1;
    if (l > 0) memcpy(out, n->jeton.deb, (size_t)l);
    out[l > 0 ? l : 0] = '\0';
}

/* Récolte les nœuds HCTN_ERREUR — les fautes de syntaxe proprement dites.
 *
 * Le niveau dépend de la POSITION : voir niveau_selon_place. Il faut que la même
 * règle s'applique ici et au parcours des jetons, sans quoi la déduplication
 * d'`ajoute` garderait celui des deux qui arrive d'abord, et le niveau d'une
 * faute dépendrait de l'ordre des passes. */
/* Sa définition est plus bas, après Etendues : elle en a besoin, et son seul
 * appelant — hct_verifie — vient après les deux. */

/* LE SCRIPT APPELLE-T-IL LUI-MÊME `nom` ? Par un message nu — « closeCards
 * 3 » —, ou par la chaîne d'un send dont c'est le premier mot — « send
 * "closeCards 2" to me ». Ce que d'autres scripts appellent, on ne le voit
 * pas d'ici. */
static int appele_dans(const HctNoeud *n, const char *nom)
{
    if (!n) return 0;
    char t[64];
    if (n->genre == HCTN_MESSAGE) {
        texte_du(n, t, sizeof t);
        if (!strcasecmp(t, nom)) return 1;
    }
    if (n->genre == HCTN_COMMANDE && n->op && !strcasecmp(n->op, "send") &&
        n->nfils >= 1 && n->fils[0] && n->fils[0]->genre == HCTN_CHAINE) {
        texte_du(n->fils[0], t, sizeof t);
        char *p = t;
        while (*p == ' ') p++;
        char *q = p;
        while (*q && *q != ' ' && *q != ',') q++;
        *q = '\0';
        if (!strcasecmp(p, nom)) return 1;
    }
    for (int i = 0; i < n->nfils; i++)
        if (appele_dans(n->fils[i], nom)) return 1;
    return 0;
}

/* LA REMARQUE SUR LES NOMS DE GESTIONNAIRES : SEULEMENT QUAND ELLE APPREND
 * QUELQUE CHOSE.
 *
 * Elle tombait sur TOUT « on » qui n'était pas un message système : « on
 * markToday », « on calculeTout », le moindre sous-programme. Or c'est
 * l'ordinaire d'un script — un gestionnaire porte le nom qu'on veut, et
 * HyperCard n'en disait rien. L'éditeur, lui, encadrait la ligne : un
 * utilisateur de Reddit, jon23, a pris la remarque pour un refus — « The
 * script didn't even verify, stopping on my "on subroutine param"
 * statement » —, en français de surcroît.
 *
 * Elle ne vaut que pour la COQUILLE, le cas qui l'a fait naître : « on
 * commandKey » à côté de « on commandKeyDown », « on mouseDwon ». Elle ne
 * paraît donc plus que si proche() a un message à proposer. Et pas même alors
 * si le script APPELLE ce nom lui-même (appele_dans) : « on closeCards »,
 * appelé plus haut par « closeCards », est un sous-programme voulu, pas un
 * « closeCard » mal tapé. Idée de l'utilisatrice, le 9 octobre : on sait
 * déjà lister les gestionnaires d'un script, autant s'en servir.
 *
 * Seuls les « on » sont regardés : un « function » porte par nature un nom
 * inventé par l'auteur. */
static void recolte_avertissements(const HctNoeud *n, const HctNoeud *racine,
                                   HctRapport *r)
{
    if (!n) return;

    if (n->genre == HCTN_GESTIONNAIRE && n->nfils >= 1) {
        char nom[64];
        texte_du(n->fils[0], nom, sizeof nom);

        if (n->op && !strcasecmp(n->op, "on") &&
            !connu(MESSAGES_SYSTEME, nom)) {
            const char *voisin = proche(nom);
            if (voisin && !appele_dans(racine, nom)) {
                char msg[220];
                snprintf(msg, sizeof msg,
                         "« %.40s » n'est pas un message système connu — "
                         "vouliez-vous dire « %.40s » ?", nom, voisin);
                ajoute(r, HCT_V_AVERTISSEMENT, n->fils[0]->jeton.ligne,
                       n->fils[0]->jeton.col, msg, nom, (int)strlen(nom));
            }
        }
    }

    for (int i = 0; i < n->nfils; i++)
        recolte_avertissements(n->fils[i], racine, r);
}

/* ---------------------------------------------- dedans ou dehors ? */

/* CE QU'HYPERCARD NE COMPILE JAMAIS NE PEUT PAS EMPÊCHER L'EXÉCUTION.
 *
 * En HyperCard, un gestionnaire est compilé À SON APPEL, et seul son bloc l'est.
 * Le texte qui traîne hors de tout « on … end » n'est donc jamais analysé, et
 * ne refuse rien. Les piles d'Apple s'en servent : le script de « Découvrir
 * HyperCard » commence par un bandeau de copyright SANS « -- » — un trait de
 * « ∞ », le titre, la version, « ©Copyright 1993-1995 by Apple Computer,Inc. »,
 * « Tous droits réservés. », un second trait — et ce n'est qu'APRÈS que les
 * commentaires commencent.
 *
 * Notre exécuteur fait comme HyperCard : mesuré sur quatre cas — bandeau non
 * commenté, bandeau commenté, rien devant, et du charabia APRÈS le
 * gestionnaire. Les quatre trouvent le gestionnaire et l'exécutent.
 *
 * Ce vérificateur, lui, analysait le texte ENTIER et rendait ERREUR. Or
 * hct_verif.h définit ERREUR comme « le script ne peut pas s'exécuter tel quel
 * — HyperCard refuserait aussi », ce qui était faux ici : quatre scripts de la
 * pile d'Apple étaient déclarés fautifs alors qu'ils tournent. Une faute hors de
 * tout gestionnaire est donc un AVERTISSEMENT.
 *
 * ET LE CARACTÈRE, LUI, N'EST PAS EN CAUSE — mesuré aussi : « ∞ », « © », « # »
 * et les accents passent sans un mot dans un commentaire et dans une chaîne, et
 * sont refusés nus À L'INTÉRIEUR d'un gestionnaire, ce qui est juste puisque ce
 * n'est pas du HyperTalk. Seule la POSITION comptait. */
typedef struct { const char *deb; int len; } Etendue;
typedef struct { Etendue t[64]; int n; } Etendues;

static void collecte_gestionnaires(const HctNoeud *n, Etendues *e)
{
    if (!n) return;
    if (n->genre == HCTN_GESTIONNAIRE && e->n < (int)(sizeof e->t / sizeof e->t[0])) {
        const char *deb; int len;
        if (hct_noeud_etendue(n, &deb, &len) && len > 0) {
            e->t[e->n].deb = deb;
            e->t[e->n].len = len;
            e->n++;
        }
    }
    for (int i = 0; i < n->nfils; i++) collecte_gestionnaires(n->fils[i], e);
}

/* Le niveau d'un JETON fautif du lexer selon sa POSITION dans le source.
 *
 * Réservé aux fautes du lexer, qui n'ont pas de place dans l'arbre : les nœuds
 * d'erreur, eux, se jugent par la STRUCTURE — voir recolte_erreurs, et la
 * mesure qui a fait abandonner la position pour eux.
 *
 */
static HctNiveau niveau_selon_place(const Etendues *e, const char *ou)
{
    /* Sans position exploitable on ne sait pas juger : on garde ERREUR, parce
     * que dégrader sur une ignorance serait la mauvaise moitié du doute. Un
     * script SANS gestionnaire, en revanche, n'a rien qui puisse échouer — voir
     * hct_verifie et la pile d'Apple qui l'a montré. */
    if (!ou) return HCT_V_ERREUR;
    if (e->n == 0) return HCT_V_AVERTISSEMENT;
    for (int i = 0; i < e->n; i++)
        if (ou >= e->t[i].deb && ou < e->t[i].deb + e->t[i].len)
            return HCT_V_ERREUR;
    return HCT_V_AVERTISSEMENT;
}

/* LA RÈGLE EST STRUCTURELLE, ET UNE MESURE A CORRIGÉ SA PREMIÈRE VERSION.
 *
 * Je l'avais écrite POSITIONNELLE — la faute tombe-t-elle dans l'étendue d'un
 * gestionnaire ? — et mon propre témoin positif l'a démolie aussitôt : « on
 * mouseUp / beep », sans « end », se mettait à PASSER. Sa faute est signalée
 * au-delà du dernier jeton du gestionnaire, donc hors de son étendue, donc
 * dégradée en avertissement. Or un « end » manquant est une faute DU
 * gestionnaire : HyperCard le refuserait à son appel.
 *
 * Les deux arbres le disent clairement, et c'est en les affichant qu'on le
 * voit :
 *
 *     end manquant       bloc > gestionnaire on > ERREUR      <- DESCENDANT
 *     bandeau d'Apple    bloc > ERREUR, ERREUR, gestionnaire  <- FRÈRE
 *
 * Donc : une faute DESCENDANTE d'un gestionnaire est une ERREUR, une faute
 * SŒUR des gestionnaires est un avertissement. Rien à voir avec les positions
 * dans le texte. */
static void recolte_erreurs(const HctNoeud *n, HctRapport *r, int dans_gestionnaire)
{
    if (!n) return;
    if (n->genre == HCTN_ERREUR)
        ajoute(r, dans_gestionnaire ? HCT_V_ERREUR : HCT_V_AVERTISSEMENT,
               n->jeton.ligne, n->jeton.col,
               n->msg ? n->msg : "forme invalide", n->jeton.deb, n->jeton.len);
    if (n->genre == HCTN_GESTIONNAIRE) dans_gestionnaire = 1;
    for (int i = 0; i < n->nfils; i++) recolte_erreurs(n->fils[i], r, dans_gestionnaire);
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

    /* L'ANALYSE PASSE AVANT LE RAPPORT, et l'ordre compte : c'est elle qui dit
     * où sont les gestionnaires, et le niveau d'une faute en dépend. */
    HctAnalyseur a;
    hct_analyseur_init(&a, &lot, &reserve);
    HctNoeud *arbre = hct_bloc_script(&a);

    Etendues dedans;
    dedans.n = 0;
    collecte_gestionnaires(arbre, &dedans);

    /* Les fautes du lexer : guillemet non fermé, caractère inattendu. Elles
     * portent déjà leur position. */
    for (int i = 0; i < lot.n; i++) {
        const HctJeton *j = &lot.jetons[i];
        if (j->genre == HCT_ERREUR)
            ajoute(rap, niveau_selon_place(&dedans, j->deb), j->ligne, j->col,
                   j->msg ? j->msg : "jeton invalide", j->deb, j->len);
    }

    /* La racine n'est pas un gestionnaire : on part donc « dehors », et seuls
     * les sous-arbres des gestionnaires basculent en ERREUR.
     *
     * UN SCRIPT SANS AUCUN GESTIONNAIRE NE FAIT PAS EXCEPTION, et j'avais
     * d'abord écrit le contraire. Le script de pile de « Découvrir HyperCard »
     * n'a QUE son bandeau et des commentaires — l'un d'eux dit « THE SCRIPTS FOR
     * THIS STACK ARE IN THE BACKGROUND SCRIPT » — et la garde le faisait
     * refuser en entier.
     *
     * Or un script sans gestionnaire n'a rien qui puisse échouer : HyperCard n'y
     * compile jamais rien. Le contrat d'ERREUR — « ne peut pas s'exécuter tel
     * quel » — ne s'y applique donc pas.
     *
     * Et ça ne cache rien à personne : le bouton « Vérifier » de l'éditeur
     * (HCview.m, checkScript) affiche TOUS les signalements avec leur niveau, et
     * ne bloque aucun enregistrement. Seul le titre de l'alerte change — « 3
     * fautes de syntaxe » devient « Script correct, avec des remarques », ce qui
     * est la vérité pour la pile d'Apple. */
    recolte_erreurs(arbre, rap, 0);
    if (avec_avertissements) recolte_avertissements(arbre, arbre, rap);

    hct_reserve_libere(&reserve);
    hct_lot_libere(&lot);

    return rap->nerreurs > 0;
}

/* Le mot de rang `rang` (0 ou 1) d'une ligne [p, fin), dans `out`. Un mot
 * s'arrête aux blancs, à la virgule et au début d'un commentaire. */
static void mot_de_ligne(const char *p, const char *fin, int rang,
                         char *out, int outlen)
{
    out[0] = '\0';
    for (int r = 0; r <= rang; r++) {
        while (p < fin && (*p == ' ' || *p == '\t')) p++;
        const char *d = p;
        while (p < fin && *p != ' ' && *p != '\t' && *p != ',' &&
               !(p[0] == '-' && p + 1 < fin && p[1] == '-'))
            p++;
        if (r == rang) {
            int l = (int)(p - d);
            if (l >= outlen) l = outlen - 1;
            memcpy(out, d, (size_t)l);
            out[l] = '\0';
        }
        if (p == d) return;              /* plus de mot sur la ligne */
    }
}

/* La fin de la ligne qui commence en p : \n, \r\n ou \r — les piles
 * d'époque écrivent \r. Rend le début de la suivante dans *suite. */
static const char *fin_de_ligne(const char *p, const char **suite)
{
    while (*p && *p != '\n' && *p != '\r') p++;
    const char *f = p;
    if (*p == '\r') { p++; if (*p == '\n') p++; }
    else if (*p == '\n') p++;
    *suite = p;
    return f;
}

int hct_gestionnaires(const char *src, HctGestionnaire *out, int max)
{
    if (!src || !out || max <= 0) return 0;

    int k = 0, ligne = 1;
    const char *p = src;
    while (*p && k < max) {
        const char *suite, *fin = fin_de_ligne(p, &suite);
        char m0[16], nom[64];
        mot_de_ligne(p, fin, 0, m0, sizeof m0);
        int fonction = !strcasecmp(m0, "function");
        if (fonction || !strcasecmp(m0, "on")) {
            mot_de_ligne(p, fin, 1, nom, sizeof nom);
            /* LE « end » DU MÊME NOM, plus loin : sans lui ce n'est pas un
             * gestionnaire, et la ligne reste du texte. */
            int l = ligne;
            const char *q = suite, *trouve = NULL;
            while (nom[0] && *q) {
                const char *s2, *f2 = fin_de_ligne(q, &s2);
                char e0[8], e1[64];
                l++;
                mot_de_ligne(q, f2, 0, e0, sizeof e0);
                if (!strcasecmp(e0, "end")) {
                    mot_de_ligne(q, f2, 1, e1, sizeof e1);
                    if (!strcasecmp(e1, nom)) { trouve = s2; break; }
                }
                q = s2;
            }
            if (trouve) {
                out[k].fonction = fonction;
                out[k].ligne    = ligne;
                snprintf(out[k].nom, sizeof out[k].nom, "%s", nom);
                k++;
                p = trouve;              /* on reprend après le « end » */
                ligne = l + 1;
                continue;
            }
        }
        p = suite;
        ligne++;
    }
    return k;
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
