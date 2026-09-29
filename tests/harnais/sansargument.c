/* Une fonction appelée SANS argument ne fait pas tomber l'application.
 *
 * L'ancien moteur, que la v3 appelle encore en recours, réservait ses lignes
 * d'arguments selon leur nombre — aucune pour « charToNum() » — puis lisait
 * vals[0] sans regarder ce nombre. strlen(NULL), et l'application entière
 * s'arrêtait sur un script d'une ligne. « offset() » faisait de même, et
 * « offset("a") » lisait une ligne au-delà de ce qu'il avait réservé.
 *
 * Trouvé par l'analyseur statique de clang, confirmé en l'exécutant sous ASan.
 * Un argument absent vaut maintenant le vide, comme « length() » et
 * « numToChar() », que la v3 servait déjà ainsi — d'où leur présence ici : le
 * comportement qu'on imite doit rester celui qu'il était.
 *
 * Ce que rend HyperCard pour ces appels n'est PAS mesuré. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *b;

static void v(const char *l)
{
    char s[400];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", l);
    printf("── %s\n", l);
    hc_set_script(b, s);
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "U");
    b = hc_new_button(c, "B");
    hc_set_current_card(c);

    puts("== ce qui plantait ==");
    v("put \"[\" & charToNum() & \"]\"");
    v("put \"[\" & offset() & \"]\"");
    v("put \"[\" & offset(\"a\") & \"]\"");

    puts("\n== les témoins ==");
    v("put \"[\" & length() & \"]\"");
    v("put \"[\" & numToChar() & \"]\"");
    v("put \"[\" & charToNum(\"A\") & \"]\"");
    v("put \"[\" & offset(\"b\", \"abc\") & \"]\"");

    hc_free(st);
    return 0;
}
