/* boutonactif — « enable » et « disable » d'un BOUTON.
 *
 * La carte « disable » de HyperTalk Reference : un raccourci de « set the
 * enabled » pour un article de menu, un menu, OU UN BOUTON. HC ne connaissait
 * que les menus et répondait « ne sait pas faire » au bouton. Relevé le 11
 * octobre. Les exemples de la référence sont repris tels quels ; NON MESURÉ
 * dans HyperCard au-delà de ce qu'elle écrit.
 *
 * Les témoins de la section 4 — les menus — ne doivent pas bouger. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      = %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static void fais(const char *l) { printf("   %s\n", l); hc_do(l); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Boutons"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    hc_new_button(bg, "Prev");
    hc_new_button(bg, "Next");
    hc_new_button(bg, "Index");
    Object *c = hc_new_card(st, bg, "Une");
    hc_new_button(c, "OK");
    Object *b2 = hc_new_button(c, "Annuler");
    hc_new_field(c, "Texte");
    hc_set_current_card(c);

    puts("== 1. les exemples de la référence ==");
    fais("disable button 1");
    fais("put the enabled of button 1");
    fais("disable background button 1");
    fais("put the enabled of bkgnd button \"Prev\"");
    fais("enable background button 3");
    fais("put the enabled of bkgnd button \"Index\"");
    fais("enable last button");
    fais("put the enabled of button \"Annuler\"");
    fais("enable button 1");
    fais("put the enabled of button 1");

    puts("\n== 2. par un nom, par un id, par une variable ==");
    char l[128];
    fais("disable button \"Annuler\"");
    fais("put the enabled of button 2");
    snprintf(l, sizeof l, "enable card button id %d", b2->id);
    fais(l);
    fais("put the enabled of button 2");
    fais("put \"bkgnd button 2\" into b");
    fais("disable b");
    fais("put the enabled of bkgnd button \"Next\"");

    puts("\n== 3. ce qui n'est pas un bouton ==");
    fais("disable button \"Absent\"");
    fais("disable field \"Texte\"");

    puts("\n== 4. témoins : les menus ==");
    fais("create menu \"Outils\"");
    fais("put \"Un,Deux\" into menu \"Outils\"");
    fais("disable menuItem 2 of menu \"Outils\"");
    fais("put the enabled of menuItem 2 of menu \"Outils\"");
    fais("disable menu \"Outils\"");
    fais("put the enabled of menu \"Outils\"");
    fais("enable menu \"Outils\"");
    fais("put the enabled of menu \"Outils\"");
    fais("delete menu \"Outils\"");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
