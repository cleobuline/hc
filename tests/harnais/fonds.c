/* Un fond désigné rend un FOND — quel que soit le désignateur.
 *
 * Deux branches de hct_resout rendaient une CARTE : la première carte du
 * fond visé pour l'ordinal (« first background », « last background »), et
 * pour le relatif la carte suivante de la pile, qui n'a rien à voir avec le
 * fond suivant. Le désignateur seul changeait la nature de l'objet obtenu :
 *
 *    the short name of bg 2                → fondDeux   (juste)
 *    the short name of first background    → carteUn    (une carte !)
 *    the short name of this background     → carteUn
 *    the short name of next background     → carteDeux  (la carte suivante)
 *
 * L'intention était bonne et l'endroit mauvais : on ne se tient jamais SUR
 * un fond, donc « go to last background » doit mener à une carte. Mais
 * c'est go qui fait cette conversion, pour toutes les formes à la fois —
 * les deux derniers essais le vérifient. */
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

static void essai(const char *corps)
{
    char s[900];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", corps);
    hc_set_script(b, s);
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("T"); hc_register_stack(st);
    Object *b1 = hc_new_background(st, "fondUn");
    Object *b2 = hc_new_background(st, "fondDeux");
    Object *b3 = hc_new_background(st, "fondTrois");
    Object *c1 = hc_new_card(st, b1, "carteUn");
    hc_new_card(st, b2, "carteDeux");
    hc_new_card(st, b3, "carteTrois");
    /* Une deuxième carte sur le premier fond : le fond suivant n'est alors
     * pas celui de la carte suivante, et l'ancien défaut se voit encore
     * mieux. */
    hc_new_card(st, b1, "carteQuatre");
    b = hc_new_button(c1, "B");
    hc_set_current_card(c1);

    puts("── le nom, par chaque désignateur");
    essai("put \"bg 2               : \" & the short name of bg 2");
    essai("put \"bg par son nom     : \" & the short name of bg \"fondTrois\"");
    essai("put \"first background   : \" & the short name of first background");
    essai("put \"second background  : \" & the short name of second background");
    essai("put \"middle background  : \" & the short name of middle background");
    essai("put \"last background    : \" & the short name of last background");
    essai("put \"this background    : \" & the short name of this background");
    essai("put \"next background    : \" & the short name of next background");
    essai("put \"previous background: \" & the short name of previous background");
    essai("put \"background (nu)    : \" & the short name of background");

    puts("\n── le relatif boucle, comme pour les cartes");
    essai("go to last background\n"
          "  put \"sur \" & the short name of this card"
          " & \", fond \" & the short name of this background\n"
          "  put \"next background    : \" & the short name of next background");

    puts("\n── mais « go » mène bien à une CARTE");
    essai("go to first background\n"
          "  put \"go first bg  → \" & the short name of this card\n"
          "  go to background \"fondDeux\"\n"
          "  put \"go bg par nom → \" & the short name of this card\n"
          "  go to last background\n"
          "  put \"go last bg   → \" & the short name of this card");

    puts("\n── le type de l'objet obtenu");
    essai("put \"the name of first background : \" & the name of first background");
    essai("put \"the name of bg 1             : \" & the name of bg 1");

    hc_free(st);
    return 0;
}
