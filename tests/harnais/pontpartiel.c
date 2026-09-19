/* UN TRAMPOLINE POSE SANS CONDITION MENT SUR LA PRESENCE DU RAPPEL.
 *
 * REGRESSION DE MA PROPRE CORRECTION, signalee par un audit exterieur et
 * confirmee par la mesure. hct_exec_init construit un « pont » — un HctHote
 * destine a l'evaluateur — dont `donnees` pointe sur l'executeur. Les rappels
 * de l'hote doivent donc passer par un trampoline qui leur rend LEURS
 * donnees : c'est pontdonnees.c, et c'etait juste.
 *
 * Seulement cette correction les installait TOUS, sans regarder si l'hote
 * servait quoi que ce soit derriere. Son commentaire annoncait « aucun
 * changement de comportement » — vrai pour HC, dont l'hote sert tout ; FAUX
 * pour tout hote partiel. La phrase affirmait plus que la mesure ne montrait.
 *
 * CAR LE POINTEUR NE SERT PAS QU'A APPELER. Ailleurs dans hct_exec.c il sert
 * a SAVOIR si l'hote possede la faculte :
 *
 *     int sait = ... : (x->ctx.hote.ecrit_message != NULL);
 *     if (!sait && x->ctx.hote.commande) { ... deleguer ... }
 *
 * Avec un trampoline pose d'office, la reponse devient « oui, toujours ».
 * Mesure, avec un hote qui fournit `commande` mais pas `ecrit_message` :
 *
 *     put "bonjour"    -> commande=0  ecrit_message=0
 *
 * Le trampoline etait appele, constatait le NULL, rendait 0 — retour que
 * l'appelant n'examine pas. LE TEXTE DISPARAISSAIT. Meme chose pour
 * `globale` : « global g » avale sans un mot. Et pour resout/ecrit_objet,
 * cible_connue() croyait la cible inscriptible et evaluait l'operande avant
 * de le decouvrir : soit la double evaluation que cette fonction existe
 * precisement pour empecher.
 *
 * NULL RESTE DONC NULL. C'est la meme lecon que pour « of X » ou X n'existe
 * pas : « absent » est une information, et la changer en un pointeur
 * generique fait prendre une mauvaise decision a l'etage du dessus.
 *
 * lit_var, ecrit_var et fonction font exception a bon droit : l'executeur y
 * ajoute sa propre logique — les portees locales, la priorite d'un
 * gestionnaire du script sur une fonction de l'hote — et doit donc les servir
 * meme quand l'hote ne fournit rien.
 *
 * POURQUOI pontdonnees.c NE POUVAIT PAS LE VOIR : il renseigne justement TOUS
 * les rappels, puisque c'est ce qu'il mesure. Un instrument complet ne peut
 * rien dire d'un hote partiel. C'est la troisieme fois ce mois-ci qu'un
 * harnais passe a cote parce qu'il regarde un cas plus riche que celui qui
 * casse.
 */
#include "hct_exec.h"
#include "hct_eval.h"
#include "hct_arbre.h"
#include "hct_lex.h"
#include "hct_bloc.h"
#include <stdio.h>
#include <string.h>

static int g_cmd, g_msg, g_glob;

static int h_commande(void *d, const HctNoeud *n, HctContexte *c)
{ (void)d; (void)n; (void)c; g_cmd++; return 1; }
static int h_ecrit_message(void *d, const char *v, int m)
{ (void)d; (void)m; g_msg++; printf("      boite : %s\n", v ? v : ""); return 1; }
static int h_globale(void *d, const char *n)
{ (void)d; (void)n; g_glob++; return 1; }
static int h_lit_var(void *d, const char *n, HctValeur *o)
{ (void)d; (void)n; *o = hct_val_texte(""); return 0; }
static int h_ecrit_var(void *d, const char *n, const char *v)
{ (void)d; (void)n; (void)v; return 0; }

/* `avec` est un masque : 1 = ecrit_message, 2 = globale. `commande` est
 * toujours la — c'est le repli dont on verifie qu'il est bien pris. */
static void tourne(const char *titre, const char *src, int avec)
{
    HctHote h; memset(&h, 0, sizeof h);
    h.lit_var   = h_lit_var;
    h.ecrit_var = h_ecrit_var;
    h.commande  = h_commande;
    if (avec & 1) h.ecrit_message = h_ecrit_message;
    if (avec & 2) h.globale       = h_globale;

    HctExec x;
    hct_exec_init(&x, h);

    /* CE QUE LE PONT ANNONCE A L'EVALUATEUR : c'est le coeur du test, et il
     * se lit avant meme d'executer quoi que ce soit. Un rappel que l'hote ne
     * sert pas doit rester NULL dans le contexte. */
    printf("   %s\n", titre);
    printf("      le pont annonce : ecrit_message=%s  globale=%s  commande=%s\n",
           x.ctx.hote.ecrit_message ? "oui" : "non",
           x.ctx.hote.globale       ? "oui" : "non",
           x.ctx.hote.commande      ? "oui" : "non");

    HctLot lot; HctReserve r; memset(&r, 0, sizeof r);
    hct_lex(src, &lot);
    HctAnalyseur an; hct_analyseur_init(&an, &lot, &r);
    HctNoeud *n = hct_bloc_script(&an);

    g_cmd = g_msg = g_glob = 0;
    if (n) hct_exec(&x, n);
    printf("      appels : commande=%d  ecrit_message=%d  globale=%d\n\n",
           g_cmd, g_msg, g_glob);

    hct_exec_libere(&x);
    hct_reserve_libere(&r);
    hct_lot_libere(&lot);
}

int main(void)
{
    puts("== 1. hote COMPLET : rien ne doit changer ==");
    tourne("put \"bonjour\"", "put \"bonjour\"\n", 1 | 2);

    puts("== 2. hote SANS ecrit_message : le texte doit partir a commande ==");
    tourne("put \"bonjour\"", "put \"bonjour\"\n", 2);

    puts("== 3. hote SANS globale : la declaration doit partir a commande ==");
    tourne("global g", "global g\n", 1);

    puts("== 4. hote sans NI l'un NI l'autre ==");
    tourne("global g, puis put", "global g\nput \"bonjour\"\n", 0);

    return 0;
}
