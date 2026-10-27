/* « go to stack the short name of me » : une propriété désigne la pile.
 *
 * La référence s'arrêtait au mot « stack » — « the » est structurel —, et
 * « go » allait à la pile où l'on était déjà, sans erreur ni the result.
 * Relevé DANS HC (l'application) le 27 octobre, sur le bouton d'un lanceur
 * de piles. Ce qu'en fait HyperCard sans parenthèses : NON MESURÉ.
 *
 * CE QUE CE HARNAIS TIENT : la forme sans parenthèses, sa jumelle pour une
 * carte, les formes qui marchaient déjà (parenthèses, variable), le refus
 * PARLANT d'un nom qui n'est pas celui d'une pile, et « the » qui reste
 * structurel hors de « the <propriété> of ». */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("      -> %s\n", t ? t : "");
    else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : "");
}

static Object *g_cible;
static Object *mon_open(const char *nom)
{
    printf("      (l'hôte cherche la pile « %s »)\n", nom);
    if (strcasecmp(nom, "Cible")) return NULL;
    if (!g_cible) {
        g_cible = hc_new_stack("Cible");
        Object *bg = hc_new_background(g_cible, "Fond");
        hc_new_card(g_cible, bg, "Accueil");
        hc_register_stack(g_cible);
    }
    return g_cible;
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    h.open_stack = mon_open;
    hc_set_host(&h);

    Object *st = hc_new_stack("Lanceur");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "Une");
    hc_new_card(st, bg, "Deux");
    Object *b = hc_new_button(c, "Cible");
    Object *b2 = hc_new_button(c, "Deux");

    static const struct { int bouton; const char *corps; } cas[] = {
        { 1, "go to stack the short name of me" },
        { 1, "go stack the short name of me" },
        { 1, "go to stack (the short name of me)" },
        { 1, "put the short name of me into n\n  go to stack n" },
        { 2, "go to card the short name of me" },
        { 1, "go to stack the name of me" },
        { 1, "put the short name of stack && the short name of me" },
        { 1, "put the number of cards && the short name of card 2" },
    };
    for (unsigned i = 0; i < sizeof cas / sizeof *cas; i++) {
        hc_set_current_card(c);
        char s[512];
        snprintf(s, sizeof s,
                 "on t\n  %s\n  put \"carte : \" & the long name of this card\n"
                 "  put \"result : [\" & the result & \"]\"\nend t\n",
                 cas[i].corps);
        printf("--- %s\n", cas[i].corps);
        Object *o = cas[i].bouton == 1 ? b : b2;
        hc_set_script(o, s);
        hc_send(o, "t");
    }

    hc_set_current_card(c);
    hc_unregister_stack(st);
    if (g_cible) { hc_unregister_stack(g_cible); hc_free(g_cible); }
    hc_free(st);
    return 0;
}
