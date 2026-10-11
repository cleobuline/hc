/* verroucmd — « lock recent » et « lock error dialogs », avec leurs « unlock ».
 *
 * Les raccourcis de « set the lockRecent » et « set the lockErrorDialogs »,
 * comme « lock messages » l'est de lockMessages. La carte « unlock » de
 * HyperTalk Reference les écrit ; HC tenait les deux propriétés et répondait
 * « ne sait pas faire » aux deux commandes. Relevé le 11 octobre.
 *
 * La section 2 vérifie que la commande AGIT, pas seulement qu'elle pose la
 * valeur : l'erreur part en message errorDialog, et le dialogue de l'hôte se
 * tait — le même détournement que tient le harnais lockerreur par « set ».
 * NON MESURÉ dans HyperCard au-delà de ce que la référence écrit. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      = %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

/* L'hôte ouvrirait le dialogue : s'il parle, il n'y a pas eu détournement. */
static void dialogue(const char *t, Object *o, int l)
{ (void)o; (void)l; printf("      -> hôte : DIALOGUE D'ERREUR (%.40s)\n", t ? t : ""); }

static Object *g_b;
static void essai(const char *titre, const char *corps)
{
    char s[512];
    printf("   %s\n", titre);
    snprintf(s, sizeof s, "on t\n%s\nend t\n", corps);
    hc_set_script(g_b, s);
    hc_send(g_b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne; h.erreur = dialogue;
    hc_set_host(&h);
    Object *st = hc_new_stack("S"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "C");
    g_b = hc_new_button(c, "B");
    hc_set_current_card(c);
    hc_set_script(c,
        "on errorDialog quoi\n"
        "  put \"errorDialog a reçu le message\"\n"
        "end errorDialog\n");

    puts("== 1. lock recent / unlock recent ==");
    essai("lock recent", "  lock recent\n  put the lockRecent");
    essai("unlock recent", "  unlock recent\n  put the lockRecent");

    puts("\n== 2. lock error dialogs : l'erreur part en message ==");
    essai("lock error dialogs, puis une faute",
          "  lock error dialogs\n  put the lockErrorDialogs\n  put zorglub()");
    essai("au repos, la serrure est retirée", "  put the lockErrorDialogs");
    essai("unlock error dialogs",
          "  lock error dialogs\n  unlock error dialogs\n  put the lockErrorDialogs");

    puts("\n== 3. témoins : lock messages, lock screen ==");
    essai("lock messages", "  lock messages\n  put the lockMessages\n  unlock messages");
    essai("lock screen", "  lock screen\n  put the lockScreen");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
