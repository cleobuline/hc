/* Un champ de plus de 64 Ko : trier, recopier, mesurer.
 *
 * HC_VAL vaut 65 536. Trois endroits du chemin d'écriture recopiaient la
 * valeur dans un tampon de cette taille avant de la reposer, et coupaient là
 * SANS RIEN DIRE :
 *
 *   — « sort lines of card field "gros" » relisait la source par
 *     v3_val_texte, puis reconstruisait le résultat dans l'arène avec un
 *     « break » quand elle était pleine ;
 *   — et container_set, en bout de course, faisait un snprintf dans HC_VAL
 *     avant hc_set_field_text — c'est celui-là qui tronquait encore une fois
 *     les deux premiers corrigés.
 *
 * Un champ de 200 000 octets et 20 000 lignes ressortait donc d'un tri à
 * 65 525 octets et 6 553 lignes. Un tri est la dernière commande dont on
 * relit le résultat : personne ne s'en apercevait avant longtemps. */
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *b, *gros, *copie;

static void mesure(const char *quoi, Object *f)
{
    const char *t = hc_field_text(f);
    size_t n = strlen(t);
    int lignes = n ? 1 : 0;
    for (const char *p = t; *p; p++) if (*p == '\n') lignes++;
    printf("   %s : %zu octets, %d lignes\n", quoi, n, lignes);
}

static void essai(const char *titre, const char *corps)
{
    char s[1024];
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
    gros  = hc_new_field(c, "gros");
    copie = hc_new_field(c, "copie");
    b     = hc_new_button(c, "B");
    hc_set_current_card(c);

    /* 20 000 lignes en ordre DÉCROISSANT, pour que le tri ait du travail et
     * que son résultat se distingue de la source. Chaque ligne fait neuf
     * octets plus le saut : 200 000 octets en tout. */
    size_t cap = 20000 * 10 + 1;
    char *src = malloc(cap);
    size_t o = 0;
    for (int i = 20000; i >= 1; i--)
        o += (size_t)snprintf(src + o, cap - o, "ligne%04d\n", i % 10000);
    src[o ? o - 1 : 0] = '\0';        /* pas de saut de ligne final */
    hc_set_field_text(gros, src);
    mesure("au départ", gros);
    printf("\n");

    essai("sort lines of card field \"gros\"",
          "sort lines of card field \"gros\"");
    mesure("après le tri", gros);
    printf("   première ligne : %.9s\n\n", hc_field_text(gros));

    essai("put card field \"gros\" into card field \"copie\"",
          "put card field \"gros\" into card field \"copie\"");
    mesure("la copie", copie);
    printf("\n");

    essai("le détour par une variable",
          "put card field \"gros\" into v\n"
          "  put the length of v\n"
          "  put empty into card field \"copie\"\n"
          "  put v into card field \"copie\"");
    mesure("la copie", copie);
    printf("\n");

    essai("sort descending",
          "sort lines of card field \"gros\" descending");
    mesure("après le tri", gros);
    printf("   première ligne : %.9s\n", hc_field_text(gros));

    free(src);
    hc_free(st);
    return 0;
}
