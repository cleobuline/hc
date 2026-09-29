/* « value » d'un texte qui porte un saut de ligne ne gèle plus l'application.
 *
 * collect_ref, dans l'ancien moteur, avance mot par mot. Elle sautait les
 * blancs avec skip_spaces — l'espace et la tabulation seulement — et coupait
 * les mots sur tout ce qu'isspace reconnaît : le saut de ligne, le retour
 * chariot, \v, \f. Posée sur l'un d'eux, elle lisait un mot de ZÉRO octet, ne
 * bougeait plus, et recommençait. Pour toujours : l'application gelait, sans
 * erreur et sans retour.
 *
 *     put value("the name of" & return & "me")
 *
 * suffisait. Trouvé par le fuzzing, qui l'atteignait autrement — par une
 * ligne fautive exécutée en partie, chemin fermé depuis (voir lignefautive).
 * Celui-ci reste ouvert, et la boucle aussi l'était.
 *
 * Ce harnais ne peut pas « échouer » au sens ordinaire : sans le garde il ne
 * finit pas, et c'est le délai de lance.sh qui le signale. Ce qu'il imprime
 * n'est que la preuve qu'on est revenu ; ce que HyperCard rend pour ces
 * valeurs-là n'est PAS mesuré. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d; (void)t;
    if (k == HC_MSG) printf("   %s\n", t);
}

static Object *b;

static void v(const char *l)
{
    char s[400];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", l);
    printf("── %s\n", l);
    fflush(stdout);
    hc_set_script(b, s);
    hc_send(b, "t");
    puts("   (revenu)");
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

    v("put value(\"the name of\" & return & \"me\") into x");
    v("put value(\"the name of\" & numToChar(11) & \"me\") into x");
    v("put value(\"the name of\" & numToChar(12) & \"me\") into x");

    hc_free(st);
    return 0;
}
