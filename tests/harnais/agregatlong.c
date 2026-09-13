/* Les agrégats devant des éléments plus longs que leur tampon.
 *
 * « the sum of <champ> » découpe le conteneur sur les virgules et les sauts
 * de ligne, puis recopiait chaque élément dans un char[64] en le COUPANT à
 * soixante-trois caractères, sans un mot. Les deux conséquences étaient
 * fausses chacune à sa façon :
 *
 *   — un nombre de soixante-dix chiffres entrait dans la somme amputé de ses
 *     sept derniers, donc divisé par dix millions ;
 *   — « 123…(63 chiffres)…abc », qui n'est PAS un nombre et doit être
 *     ignoré, passait pour un nombre une fois sa queue coupée, et s'ajoutait
 *     à la somme.
 *
 * Une moyenne changeait ainsi selon la longueur des lignes voisines. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *b, *f;

static void essai(const char *titre, const char *contenu, const char *corps)
{
    char s[512];
    hc_set_field_text(f, contenu);
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", corps);
    printf("── %s\n", titre);
    hc_set_script(b, s);
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    f = hc_new_field(c, "F");
    b = hc_new_button(c, "B");
    hc_set_current_card(c);

    /* Soixante-dix chiffres : 1 suivi de soixante-neuf zéros, soit 1e69.
     * Coupé à soixante-trois caractères, il valait 1e62 — dix millions de
     * fois moins. */
    char gros[128];
    gros[0] = '1';
    memset(gros + 1, '0', 69);
    gros[70] = '\0';

    char contenu[256];
    snprintf(contenu, sizeof contenu, "%s", gros);
    essai("un nombre de 70 chiffres, seul", contenu,
          "put the sum of card field \"F\"");
    printf("   (la valeur juste est 1e+69)\n\n");

    /* Le même, dans une liste, pour que le découpage entre en jeu. */
    snprintf(contenu, sizeof contenu, "1,%s,1", gros);
    essai("le même entre deux 1", contenu,
          "put the sum of card field \"F\"");
    printf("   (la valeur juste est 1e+69)\n\n");

    /* Soixante-trois chiffres suivis de lettres : ce n'est pas un nombre, et
     * l'agrégat doit l'ignorer. Une fois coupé, il en devenait un. */
    char faux[128];
    memset(faux, '9', 63);
    memcpy(faux + 63, "abc", 4);
    snprintf(contenu, sizeof contenu, "10,%s,20", faux);
    essai("« 999…abc » n'est pas un nombre", contenu,
          "put the sum of card field \"F\"");
    printf("   (la valeur juste est 30)\n\n");

    snprintf(contenu, sizeof contenu, "10,%s,20", faux);
    essai("et il ne compte pas dans la moyenne", contenu,
          "put the average of card field \"F\"");
    printf("   (la valeur juste est 15)\n\n");

    /* max sur des éléments longs : le plus grand doit l'emporter en entier. */
    snprintf(contenu, sizeof contenu, "5,%s", gros);
    essai("le max prend le nombre entier", contenu,
          "put the max of card field \"F\"");
    printf("   (la valeur juste est 1e+69)\n\n");

    /* Les listes courtes, pour vérifier qu'on n'a rien cassé. */
    essai("liste ordinaire", "10,20,30",
          "put \"somme \" & the sum of card field \"F\""
          " & \", moyenne \" & the average of card field \"F\""
          " & \", min \" & the min of card field \"F\""
          " & \", max \" & the max of card field \"F\"");

    hc_free(st);
    return 0;
}
