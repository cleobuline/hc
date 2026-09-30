/* Une globale déclarée et jamais posée vaut le VIDE, et zéro dans un calcul.
 *
 * frame_declare_global créait sa case sans poser le nombre non arrondi (brut)
 * ni son drapeau (a_brut). La case sortait d'un realloc : elle contenait ce
 * que le tas y avait laissé, et v3_lit_var, voyant a_brut non nul, prenait ce
 * brut-là pour la valeur de la variable. Signalé par un audit extérieur.
 *
 *     global g1, …, g12
 *     put g12 + 1          -> -5.31401e+303 au lieu de 1
 *
 * AUCUN SANITIZER DE LA SUITE NE LE VOIT : la lecture tombe dans un bloc
 * valide. C'est le terrain de MemorySanitizer, que gcc n'a pas. Ce harnais
 * rend donc le tas SALE lui-même : mallopt(M_PERTURB) de la glibc remplit
 * chaque allocation d'un motif non nul, exactement comme MALLOC_PERTURB_ l'a
 * fait pour la mesure. Sous ASan, mallopt est sans effet, mais ASan remplit
 * déjà ses allocations d'octets 0xbe : le défaut s'y voit aussi.
 *
 * Vérifié en retirant la correction : ce harnais rend alors un nombre absurde
 * au lieu de 1, dans les deux compilations. Douze globales parce qu'il faut
 * dépasser la capacité initiale de la table (8) pour que la case vienne d'un
 * realloc et non de la première allocation. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *b;

static void v(const char *l)
{
    char s[600];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", l);
    printf("── %s\n", l);
    hc_set_script(b, s);
    hc_send(b, "t");
}

int main(void)
{
#if defined(__GLIBC__) && defined(M_PERTURB)
    mallopt(M_PERTURB, 1);   /* toute allocation neuve : octets 0xFE */
#endif
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "U");
    b = hc_new_button(c, "B");
    hc_set_current_card(c);

    v("global g1,g2,g3,g4,g5,g6,g7,g8,g9,g10,g11,g12\n"
      "  put g12 + 1 & \" / [\" & g12 & \"]\"");
    v("global g13\n  put g13 * 2 & \" / \" & (g13 is empty)");
    v("global g14\n  add 5 to g14\n  put g14");

    hc_free(st);
    return 0;
}
