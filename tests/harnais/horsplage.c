/* Un nombre démesuré ne se convertit pas en entier AVANT d'être borné.
 *
 * Deux conversions double -> int, dans l'ancien moteur, venaient avant le test
 * qui les aurait refusées :
 *
 *     set the dragSpeed to 1e31850     l'infini converti, puis comparé à
 *                                      0..32767
 *     go to marked card 1e30           le rang converti, puis comparé au
 *                                      nombre de cartes marquées
 *
 * C'est un comportement indéfini dès que la valeur sort de la plage d'un int :
 * le compilateur a le droit d'en faire n'importe quoi, et la comparaison qui
 * suit ne protège plus rien. UBSan l'a relevé sous le fuzzing.
 *
 * La sortie ordinaire de ce harnais ne peut PAS le voir — la faute ne changeait
 * rien de visible sur cette machine-ci. C'est « ./lance.sh --asan », qui
 * compile avec float-cast-overflow, qui la voit : c'est là que ce harnais sert.
 * Le message cite désormais la valeur telle qu'écrite ; son image en int n'avait
 * pas de sens. */
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
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *u = hc_new_card(st, bg, "U");
    hc_new_card(st, bg, "V");
    b = hc_new_button(bg, "B");
    hc_set_current_card(u);

    puts("== un réglage ==");
    v("set the dragSpeed to 1e31850");
    v("set the dragSpeed to -1e31850");
    v("set the dragSpeed to 12");
    v("put the dragSpeed");

    puts("\n== le rang d'une carte marquée ==");
    v("mark card 2");
    v("go to marked card 1e30");
    v("put the short name of this card");
    v("go to marked card 1");
    v("put the short name of this card");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
