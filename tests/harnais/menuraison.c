/* TROIS ECHECS DE MENU, UN SEUL MESSAGE — ET IL ETAIT FAUX DEUX FOIS.
 *
 * SIGNALE A L'USAGE : « propriete de menu inconnue : checkMark ». Or
 * checkMark EXISTE, et marche : sur un article qui existe, dans un menu qui
 * existe, elle se lit et s'ecrit. Le message envoyait donc chercher du cote
 * du nom de la propriete — le seul endroit ou il n'y avait rien.
 *
 * Trois causes bien distinctes se disaient de la meme facon :
 *
 *     set the checkMark of menu "Essai" to true          la propriete
 *     set the checkMark of menuItem 9 of menu "Essai"    l'ARTICLE
 *     set the checkMark of menuItem 1 of menu "Absent"   le MENU
 *     -> « propriete de menu inconnue : checkMark », les trois fois
 *
 * LA LECTURE MENTAIT AUSSI, dans l'autre sens : les memes trois cas, plus
 * « the zorglub of menuItem 1 », disaient tous « objet introuvable », meme
 * quand le menu ET l'article etaient la. Elle sortait en plus TROIS lignes
 * pour une seule faute, le recours reessayant avant de renoncer.
 *
 * C'est la troisieme fois qu'on corrige cette forme-la — apres
 * « Can't understand » pose sur un objet manquant, et « propriete inconnue »
 * pose sur une cible absente. Un diagnostic FAUX coute plus cher qu'un
 * diagnostic vague : le vague fait chercher partout, le faux fait chercher
 * au mauvais endroit, et donne confiance en le faisant.
 *
 * La raison est posee LA OU L'ECHEC SE PRODUIT — v3_menu_index sait que le
 * menu manque, v3_article_index que l'article manque, les deux tables de
 * proprietes que le nom est inconnu — et lue par les deux appelants. Une
 * seule source, deux chemins, et ils ne peuvent plus se contredire.
 *
 * CE QUE CE HARNAIS TIENT SURTOUT : que les proprietes qui MARCHENT marchent
 * toujours. Un « dire toujours menu introuvable » passerait les trois cas
 * d'echec sans rien valoir.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

static void execute(const char *corps)
{
    char s[2048];
    printf("   %s\n", corps);
    snprintf(s, sizeof s, "on essaie\n%s\nend essaie\n", corps);
    hc_set_script(b, s);
    hc_send(b, "essaie");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);
    b = hc_new_button(c, "B");

    puts("== 1. mise en place ==");
    execute("  create menu \"Essai\"\n"
            "  put \"Alpha,Beta,Gamma\" into menu \"Essai\"\n"
            "  put the number of menuItems of menu \"Essai\" & \" articles\"");

    puts("\n== 2. CE QUI MARCHE, ET QUI DOIT CONTINUER ==");
    /* La moitie qui coute : un « dire toujours la meme chose » passerait la
     * section 3 sans broncher. */
    execute("  set the checkMark of menuItem 1 of menu \"Essai\" to true\n"
            "  put the checkMark of menuItem 1 of menu \"Essai\"");
    execute("  set the checkMark of menuItem \"Beta\" of menu \"Essai\" to true\n"
            "  put the checkMark of menuItem \"Beta\" of menu \"Essai\"");
    execute("  set the checkMark of menuItem 1 of menu \"Essai\" to false\n"
            "  put the checkMark of menuItem 1 of menu \"Essai\"");
    execute("  put the name of menuItem 2 of menu \"Essai\"");
    execute("  put the number of menuItem \"Gamma\" of menu \"Essai\"");
    execute("  set the enabled of menuItem 3 of menu \"Essai\" to false\n"
            "  put the enabled of menuItem 3 of menu \"Essai\"");
    execute("  put the name of menu \"Essai\"");
    execute("  put the enabled of menu \"Essai\"");

    puts("\n== 3. LES TROIS ECHECS, CHACUN NOMME ==");
    puts("   -- la PROPRIETE n'existe pas --");
    execute("  set the checkMark of menu \"Essai\" to true");
    execute("  put the checkMark of menu \"Essai\"");
    execute("  set the zorglub of menuItem 1 of menu \"Essai\" to true");
    execute("  put the zorglub of menuItem 1 of menu \"Essai\"");
    puts("   -- l'ARTICLE n'existe pas --");
    execute("  set the checkMark of menuItem 9 of menu \"Essai\" to true");
    execute("  put the checkMark of menuItem 9 of menu \"Essai\"");
    execute("  set the checkMark of menuItem \"Zorglub\" of menu \"Essai\" to true");
    puts("   -- le MENU n'existe pas --");
    execute("  set the checkMark of menuItem 1 of menu \"Absent\" to true");
    execute("  put the checkMark of menuItem 1 of menu \"Absent\"");
    execute("  put the name of menu \"Absent\"");

    puts("\n== 4. et « the result » suit le message ==");
    execute("  set the checkMark of menuItem 1 of menu \"Absent\" to true\n"
            "  put the result");
    execute("  set the checkMark of menuItem 9 of menu \"Essai\" to true\n"
            "  put the result");
    execute("  set the zorglub of menuItem 1 of menu \"Essai\" to true\n"
            "  put the result");

    hc_free(st);
    return 0;
}
