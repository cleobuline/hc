/* L'HOTE RECOIT-IL SES PROPRES DONNEES ? On le lui demande.
 *
 * SIGNALE PAR UN AUDIT EXTERIEUR, et confirme par la mesure. hct_exec_init
 * construit un « pont » — un HctHote destine a l'evaluateur — dont `donnees`
 * pointe sur l'EXECUTEUR, parce que lit_var, ecrit_var et fonction sont
 * servis par l'executeur lui-meme. Mais huit autres rappels y etaient copies
 * DIRECTEMENT depuis l'hote :
 *
 *     pont.donnees = x;              (l'executeur)
 *     pont.resout  = hote.resout;    (qui attend hote.donnees)
 *
 * Chacun recevait donc un HctExec* la ou il attendait le pointeur que
 * l'appelant lui avait confie. Mesure avant correction :
 *
 *     resout           recoit AUTRE CHOSE  ***
 *     recours          recoit AUTRE CHOSE  ***
 *     ecrit_message    recoit AUTRE CHOSE  ***
 *     commande         recoit AUTRE CHOSE  ***
 *     fonction         recoit SES donnees        <- le seul trampoline
 *
 * POURQUOI AUCUN TEST NE POUVAIT LE VOIR. Les rappels v3_* de hc_core.c font
 * tous « (void)d; » — ils ignorent le pointeur et lisent des globales. Le
 * defaut etait donc parfaitement invisible pour le seul hote existant, et ne
 * se serait manifeste que chez le suivant : un dereferencement de mauvais
 * type, donc un plantage ou une corruption silencieuse.
 *
 * D'OU CE HARNAIS. Il est le SECOND hote de la bibliotheque, et le premier a
 * se servir vraiment de `donnees`. C'est sa raison d'etre : tenir la promesse
 * de l'en-tete — « opaque, transmis tel quel a chaque rappel » — que le seul
 * hote reel ne pouvait pas verifier.
 *
 * IL AVAIT POURTANT DEJA MORDU. Le commentaire de resultat_vide, dans
 * hct_exec.c, explique qu'on s'adresse a « x->hote, et non x->ctx.hote »
 * parce que les donnees du pont pointent sur l'executeur. Quelqu'un s'est
 * cogne dedans et a contourne a UN site d'appel au lieu de remonter a la
 * cause.
 */
#include "hct_exec.h"
#include "hct_eval.h"
#include "hct_arbre.h"
#include "hct_lex.h"
#include "hct_bloc.h"
#include <stdio.h>
#include <string.h>

/* Une structure a NOUS. Seule son ADRESSE sert : on ne la dereference jamais,
 * puisque c'est justement ce qui planterait si le pointeur etait mauvais. */
typedef struct { unsigned long magie; const char *nom; } MonHote;
static MonHote g_mien = { 0xC0FFEEUL, "moi" };

static int g_bons, g_mauvais;

static void verifie(const char *rappel, void *d)
{
    if (d == &g_mien) { g_bons++;   printf("   %-16s recoit SES donnees\n", rappel); }
    else              { g_mauvais++; printf("   %-16s recoit AUTRE CHOSE ***\n", rappel); }
}

static int   h_lit_var(void *d, const char *n, HctValeur *o)
{ (void)n; verifie("lit_var", d); *o = hct_val_texte(""); return 0; }
static int   h_ecrit_var(void *d, const char *n, const char *v)
{ (void)n; (void)v; verifie("ecrit_var", d); return 1; }
/* UN OBJET BIDON, ET C'EST INDISPENSABLE.
 *
 * La premiere version de ce harnais rendait NULL depuis resout. lit_objet,
 * lit_prop et ecrit_objet n'etaient donc JAMAIS appeles — l'evaluateur ne les
 * sollicite que sur un objet resolu — et le test les declarait sains sans les
 * avoir touches. Un rappel jamais appele passe tous les tests du monde.
 *
 * On rend donc une adresse valide. Elle n'est jamais dereferencee : seul son
 * passage nous interesse. */
static int g_objet_bidon;
static void *h_resout(void *d, const HctNoeud *r, HctContexte *c)
{ (void)r; (void)c; verifie("resout", d); return &g_objet_bidon; }
static int   h_lit_objet(void *d, void *o, HctValeur *v)
{ (void)o; (void)v; verifie("lit_objet", d); return 0; }
static int   h_lit_prop(void *d, void *o, const char *p, HctValeur *v)
{ (void)o; (void)p; (void)v; verifie("lit_prop", d); return 0; }
static int   h_recours(void *d, const HctNoeud *n, HctValeur *o)
{ (void)n; verifie("recours", d); *o = hct_val_texte(""); return 1; }
static int   h_commande(void *d, const HctNoeud *n, HctContexte *c)
{ (void)n; (void)c; verifie("commande", d); return 1; }
static int   h_ecrit_message(void *d, const char *v, int m)
{ (void)v; (void)m; verifie("ecrit_message", d); return 1; }
static int   h_ecrit_objet(void *d, void *o, const char *v, int m)
{ (void)o; (void)v; (void)m; verifie("ecrit_objet", d); return 0; }
static int   h_globale(void *d, const char *n)
{ (void)n; verifie("globale", d); return 1; }
static int   h_fonction(void *d, const char *n, HctValeur *a, int na, HctValeur *o)
{ (void)n; (void)a; (void)na; verifie("fonction", d); *o = hct_val_texte(""); return 0; }

static void tourne(const char *titre, const char *src)
{
    HctHote h; memset(&h, 0, sizeof h);
    h.donnees       = &g_mien;
    h.lit_var       = h_lit_var;
    h.ecrit_var     = h_ecrit_var;
    h.resout        = h_resout;
    h.lit_objet     = h_lit_objet;
    h.lit_prop      = h_lit_prop;
    h.recours       = h_recours;
    h.commande      = h_commande;
    h.ecrit_message = h_ecrit_message;
    h.ecrit_objet   = h_ecrit_objet;
    h.globale       = h_globale;
    h.fonction      = h_fonction;

    HctExec x;
    hct_exec_init(&x, h);

    HctLot lot; HctReserve r; memset(&r, 0, sizeof r);
    hct_lex(src, &lot);
    HctAnalyseur an; hct_analyseur_init(&an, &lot, &r);
    HctNoeud *n = hct_bloc_script(&an);

    printf("== %s ==\n", titre);
    if (n) hct_exec(&x, n);
    puts("");

    hct_exec_libere(&x);
    hct_reserve_libere(&r);
    /* Le LOT aussi : hct_lex alloue son tableau de jetons. L'oublier faisait
     * fuir 12 Ko par appel, et c'est LeakSanitizer qui l'a dit — un harnais
     * qui fuit ferait passer la suite au rouge pour une raison etrangere a ce
     * qu'il mesure. */
    hct_lot_libere(&lot);
}

int main(void)
{
    /* Chaque ligne vise un rappel different, pour qu'aucun ne reste non
     * exerce — un rappel jamais appele passerait le test sans rien prouver. */
    tourne("references d'objets et proprietes",
           "  put the width of card button \"z\"\n"
           "  put card field \"z\"\n");
    tourne("recours et commandes",
           "  put zorglub\n"
           "  go to card 2\n"
           "  set the visible of me to false\n");
    tourne("ecritures",
           "  put \"x\" into card field \"z\"\n"
           "  put \"vers la boite\"\n");
    tourne("globales et fonctions",
           "  global g\n"
           "  put g\n"
           "  put the ticks\n");

    printf("   rappels ayant recu LEURS donnees : %d\n", g_bons);
    printf("   rappels ayant recu AUTRE CHOSE   : %d\n", g_mauvais);
    return 0;
}
