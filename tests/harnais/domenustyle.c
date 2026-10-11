/* domenustyle — « doMenu "Bold" », et les autres articles du menu Style.
 *
 * MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II) par l'utilisatrice le 11 octobre :
 *
 *     select word 2 of card field 1
 *     doMenu "Bold"
 *     doMenu "Group"
 *
 * met le mot 2 en gras et le groupe. HC répondait « article inconnu » ; c'est
 * le constat 4 de l'audit des sources.
 *
 * Sans éditeur de champ ouvert — c'est le cas ici, sans interface —, le noyau
 * applique l'article aux plages de la sélection. Avec un éditeur ouvert, l'hôte
 * le prend (style_menu) : la section 7 le simule.
 *
 * NON MESURÉ dans HyperCard : la bascule (section 3), la sélection mêlée
 * (section 4), et doMenu sans sélection (section 6). HC suit la règle de son
 * menu Style, celle de tout éditeur Macintosh. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      = %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static void fais(const char *l) { printf("   %s\n", l); hc_do(l); }

static void styles(void)
{
    hc_do("put \"      mots : \" & the textStyle of word 1 of card field 1 & \" | \" & "
          "the textStyle of word 2 of card field 1 & \" | \" & "
          "the textStyle of word 3 of card field 1");
}

static int g_hote_prend = 0;
static int hote_style(const char *a)
{
    printf("      (hôte : « %s », %s)\n", a, g_hote_prend ? "pris" : "laissé au noyau");
    return g_hote_prend;
}
static void hote_menu(const char *a) { printf("      (hôte, doMenu : « %s »)\n", a); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    h.style_menu = hote_style;
    h.do_menu = hote_menu;
    hc_set_host(&h);
    Object *st = hc_new_stack("S"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "C");
    hc_set_current_card(c);
    Object *f = hc_new_field(c, "Texte");
    hc_set_field_text(f, "un deux trois");

    puts("== 1. le banc d'HyperCard : Bold, puis Group, sur le mot 2 ==");
    fais("select word 2 of card field 1");
    fais("doMenu \"Bold\"");
    styles();
    fais("doMenu \"Group\"");
    styles();

    puts("\n== 2. les deux autres mots n'ont pas bougé, et la sélection reste ==");
    fais("put the selectedText");

    puts("\n== 3. Bold de nouveau : toute la sélection l'a, il s'ôte ==");
    fais("doMenu \"Bold\"");
    styles();

    puts("\n== 4. une sélection mêlée : Italic se pose partout, le groupe reste ==");
    fais("select word 1 to 2 of card field 1");
    fais("doMenu \"Italic\"");
    styles();

    puts("\n== 5. Plain ôte tout, un corps se pose ==");
    fais("doMenu \"Plain\"");
    styles();
    fais("select word 3 of card field 1");
    fais("doMenu \"18\"");
    fais("put the textSize of word 3 of card field 1 && the textSize of word 1 of card field 1");

    puts("\n== 6. sans sélection, rien ne bouge, et pas d'erreur ==");
    fais("select empty");
    fais("doMenu \"Bold\"");
    styles();

    puts("\n== 7. un éditeur de champ ouvert : l'hôte prend l'article ==");
    g_hote_prend = 1;
    fais("select word 3 of card field 1");
    fais("doMenu \"Bold\"");
    styles();
    g_hote_prend = 0;

    puts("\n== 8. la pile peut détourner l'article, comme tout doMenu ==");
    hc_set_script(st, "on doMenu quoi\n"
                      "  if quoi is \"Bold\" then put \"détourné : \" & quoi\n"
                      "  else pass doMenu\n"
                      "end doMenu\n");
    fais("doMenu \"Bold\"");
    styles();

    puts("\n== 9. un article qui n'est pas du menu Style va toujours à l'hôte ==");
    fais("doMenu \"Card Info...\"");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
