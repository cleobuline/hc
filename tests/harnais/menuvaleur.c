/* menuvaleur — UN ARTICLE DE MENU LU COMME UNE VALEUR.
 *
 *     put menuItem 4 of menu "Essai"      -> Quatre
 *
 * HC répondait « objet introuvable » : seules les PROPRIÉTÉS d'un article
 * se lisaient. Essayé par l'utilisatrice dans la boîte de message le 10
 * octobre, et MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II) le même jour :
 * « Quatre ».
 *
 * NON MESURÉ, et dit comme tel : le menu entier lu comme une valeur
 * (« put menu "Essai" », toujours refusé ici), et le texte exact des refus
 * d'HyperCard pour un article ou un menu absent. */
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
    Object *st = hc_new_stack("S"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    hc_set_current_card(hc_new_card(st, bg, "C"));

    hc_do("create menu \"Essai\"");
    hc_do("put \"Un,-,Trois,Quatre\" into menu \"Essai\"");

    puts("== 1. le banc mesuré ==");
    fais("put menuItem 4 of menu \"Essai\"");

    puts("\n== 2. les autres désignations ==");
    fais("put menuItem \"Trois\" of menu \"Essai\"");
    fais("put last menuItem of menu \"Essai\"");
    fais("put menuItem 2 of menu \"Essai\"");
    fais("put \"Essai\" into gHMnu");
    fais("put menuItem 4 of menu gHMnu");
    fais("put \"[\" & menuItem 1 of menu \"Essai\" & \"]\"");
    fais("put menuItem 4 of menu \"Essai\" into x");
    fais("put x");

    puts("\n== 3. la valeur suit l'article ==");
    fais("set the name of menuItem 4 of menu \"Essai\" to \"Quatre bis\"");
    fais("put menuItem 4 of menu \"Essai\"");

    puts("\n== 4. ce qui manque se dit ==");
    fais("put menuItem 9 of menu \"Essai\"");
    fais("put menuItem 1 of menu \"Absent\"");

    puts("\n== 5. non mesuré : le menu entier, toujours refusé ==");
    fais("put menu \"Essai\"");

    hc_do("delete menu \"Essai\"");
    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
