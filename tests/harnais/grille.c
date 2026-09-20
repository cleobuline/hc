/* « the grid » : la grille de dessin, vue du noyau.
 *
 * CE QUE CE HARNAIS PEUT PROUVER, ET CE QU'IL NE PEUT PAS.
 *
 * Le CALAGE lui-même — huit pixels, les formes et les objets — vit dans
 * HCview.m et demande AppKit : il ne se vérifie qu'à la compilation Xcode,
 * et rien ici ne le mesure. Le dire est plus utile que de faire semblant.
 *
 * Ce qui vit dans le NOYAU, en revanche, est exactement l'endroit où une
 * propriété s'oublie : les deux listes — celle de ce qui EXISTE et celle de
 * ce qui se POSE. En oublier une seule donne le défaut le plus courant de
 * cette famille : une propriété qu'on lit et qu'on ne peut pas écrire, ou
 * l'inverse. On mesure donc les deux sens, et le refus d'à côté.
 *
 * Mesuré AVANT la correction : « put the grid » et « set the grid to true »
 * disaient tous deux « inconnue ». */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   %s\n", t);
           else if (k == HC_ERR) printf("   [ERR] %s\n", t); }

/* L'hôte imite cocoa_global_set : il ne retient que ce qu'il connaît, et
 * traduit la valeur comme le fait l'interface — non vide et différent de
 * « false » vaut vrai. */
static int  g_grille  = 0;
static int  g_cotes   = 4;
static int  g_repond  = 1;     /* l'hôte connaît-il « grid » ? */
static void mon_set(const char *n, const char *v)
{
    if (!strcasecmp(n, "grid")) {
        g_grille = (v && *v && strcasecmp(v, "false") != 0 &&
                    strcmp(v, "0") != 0);
        printf("   [HOTE] grid <- « %s » donc %s\n", v ? v : "",
               g_grille ? "vrai" : "faux");
    } else if (!strcasecmp(n, "polySides")) {
        /* L'hôte borne, comme cocoa_global_set : trois est le minimum qui
         * enferme une surface. */
        int k = v ? atoi(v) : g_cotes;
        if (k < 3)  k = 3;
        if (k > 50) k = 50;
        g_cotes = k;
        printf("   [HOTE] polySides <- « %s » donc %d\n", v ? v : "", g_cotes);
    } else {
        printf("   [HOTE] ignore « %s »\n", n);
    }
}
static const char *mon_get(const char *n)
{
    static char b[32];
    if (!strcasecmp(n, "grid") && g_repond) return g_grille ? "true" : "false";
    if (!strcasecmp(n, "polySides")) { snprintf(b, sizeof b, "%d", g_cotes); return b; }
    if (!strcasecmp(n, "filled")) return "false";
    return NULL;
}

static Object *g_pile, *g_carte;
static void fais(const char *ligne)
{
    char script[512];
    snprintf(script, sizeof script, "on essai\n  %s\nend essai\n", ligne);
    printf("   %s\n", ligne);
    hc_set_script(g_pile, script);
    hc_send(g_carte, "essai");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.global_set = mon_set; h.global_get = mon_get;
    hc_set_host(&h);

    g_pile = hc_new_stack("Grille");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);

    printf("=== 1. elle se LIT ===\n");
    fais("put the grid");

    printf("=== 2. elle s'ÉCRIT, et la lecture suit ===\n");
    fais("set the grid to true");
    fais("put the grid");
    fais("set the grid to false");
    fais("put the grid");

    printf("=== 3. l'aller-retour dans un même script ===\n");
    printf("   (une pile qui cale un réglage et le rend : l'usage réel)\n");
    fais("put the grid into avant\n"
         "  set the grid to true\n"
         "  put \"pendant : \" & the grid\n"
         "  set the grid to avant\n"
         "  put \"après : \" & the grid");

    printf("=== 4. on ne l'a PAS rendue écrivable au prix d'une porte ouverte ===\n");
    printf("   (une coquille doit toujours se plaindre — c'est tout l'objet\n");
    printf("    de setmuet.c, et une liste élargie pourrait l'avoir défait)\n");
    fais("set the gird to true");
    fais("put the gird");
    fais("set the screenRect to \"0,0,1,1\"");

    printf("=== 5. si l'hôte ne répond pas, on ne se tait pas : on le dit ===\n");
    g_repond = 0;
    fais("put the grid");
    g_repond = 1;

    printf("=== 6. le témoin : une propriété voisine n'a pas bougé ===\n");
    fais("put the filled");

    printf("=== 7. « the polySides », l'autre réglage posé au même endroit ===\n");
    printf("   (ajoutée aux deux MÊMES listes ; si l'une avait été oubliée,\n");
    printf("    elle se lirait sans s'écrire, ou l'inverse)\n");
    fais("put the polySides");
    fais("set the polySides to 6");
    fais("put the polySides");

    printf("=== 8. hors domaine : l'hôte ramène dans les bornes ===\n");
    printf("   (un polygone à deux côtés n'enferme rien, et à mille il n'est\n");
    printf("    plus qu'un cercle — mais c'est l'hôte qui tranche, pas le\n");
    printf("    noyau : il ne connaît pas le sens de cette propriété)\n");
    fais("set the polySides to 2");
    fais("put the polySides");
    fais("set the polySides to 9999");
    fais("put the polySides");

    printf("=== 9. et la coquille reste refusée ===\n");
    fais("set the polySide to 6");
    return 0;
}
