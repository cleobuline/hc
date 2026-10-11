/* refaire — « go forth » après « go back », et « go recent card ».
 *
 * La carte « go » de HyperTalk Reference écrit les deux à côté de « go
 * back » ; HC répondait « ne sait pas faire ». Relevé le 11 octobre.
 *
 * « go back » dépile sans effacer : la carte quittée peut se refaire. Une
 * navigation NOUVELLE oublie ce qu'on pouvait refaire, comme le bouton
 * « précédent » d'un navigateur (section 2), et une carte supprimée en sort
 * (section 4).
 *
 * NON MESURÉ dans HyperCard : que « go forth » oublie lui aussi ses pas après
 * un détour, et que « go recent card » soit le même geste que « go back »
 * (c'est ce que HC fait déjà de « go recent »). Le harnais histoire tient
 * l'historique et « go back » ; il ne doit pas bouger. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      = %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static void fais(const char *l) { printf("   %s\n", l); hc_do(l); }
static void ou(void) { hc_do("put \"ici : \" & the short name of this card"); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("S"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c1 = hc_new_card(st, bg, "C1");
    for (int i = 2; i <= 6; i++) {
        char nom[8]; snprintf(nom, sizeof nom, "C%d", i);
        hc_new_card(st, bg, nom);
    }
    hc_set_current_card(c1);
    hc_go_card(c1);

    puts("== 1. reculer, puis refaire ==");
    fais("go card 2");
    fais("go card 3");
    fais("go back");      ou();
    fais("go back");      ou();
    fais("go forth");     ou();
    fais("go forth");     ou();
    fais("go forth");     ou();

    puts("\n== 2. une navigation nouvelle oublie ce qu'on pouvait refaire ==");
    fais("go back");      ou();
    fais("go card 5");    ou();
    fais("go forth");     ou();
    fais("go back");      ou();

    puts("\n== 3. go recent card, comme go back ==");
    fais("go card 6");
    fais("go recent card"); ou();
    fais("go to recent card"); ou();
    fais("go forth");     ou();

    puts("\n== 4. une carte supprimée sort des pas à refaire ==");
    fais("go card 1");
    fais("go card 3");
    fais("go card 4");
    fais("go back");
    fais("go back");      ou();
    fais("delete card \"C3\"");
    fais("go forth");     ou();
    fais("go forth");     ou();

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
