/* reglagesretenus — les réglages globaux qui arrêtaient un script au « set ».
 *
 * Relevé le 11 octobre sur HyperTalk Reference : userModify, printMargins,
 * printText*, scriptText*, traceDelay, debugger, scriptEditor,
 * messageWatcher, variableWatcher, dialingVolume, longWindowTitles
 * répondaient « propriété inconnue » au set ; environment, suspended et
 * stackSpace à la lecture. Ils sont RETENUS et RELUS, comme userLevel (harnais
 * reglages), et ne sont pas consultés par HC.
 *
 * Les défauts sont ceux qu'écrit la référence, carte par carte. NON MESURÉ
 * dans HyperCard : dialingVolume (7 est un choix), la réponse de stackSpace
 * (un mégaoctet, un choix aussi). */
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

    static const char *noms[] = {
        "userModify", "longWindowTitles", "traceDelay", "dialingVolume",
        "printTextHeight", "printTextSize", "scriptTextSize", "printTextFont",
        "printTextAlign", "printTextStyle", "printMargins", "scriptTextFont",
        "debugger", "scriptEditor", "messageWatcher", "variableWatcher", NULL };

    puts("== 1. les défauts de la référence ==");
    for (int i = 0; noms[i]; i++) {
        char l[128];
        snprintf(l, sizeof l, "put \"%s \" & the %s", noms[i], noms[i]);
        hc_do(l);
    }

    puts("\n== 2. les exemples de la référence, posés puis relus ==");
    fais("set the userModify to true");
    fais("put the userModify");
    fais("set the userModify of this stack to false");
    fais("put the userModify of this stack");
    fais("set the printMargins to 10,10,10,10");
    fais("put the printMargins");
    fais("set the printTextFont to \"Times\"");
    fais("set the printTextAlign to Center");
    fais("set the printTextStyle to \"bold,italic\"");
    fais("put the printTextFont && the printTextAlign && the printTextStyle");
    fais("set the printTextHeight to 20");
    fais("set the printTextSize to 18");
    fais("put the printTextHeight && the printTextSize");
    fais("set the scriptTextFont to \"Geneva\"");
    fais("set the scriptTextSize to 10");
    fais("put the scriptTextFont && the scriptTextSize");
    fais("set the traceDelay to 10");
    fais("set dialingVolume to 5");
    fais("put the traceDelay && the dialingVolume");
    fais("set the longWindowTitles to true");
    fais("put the longWindowTitles");
    fais("set debugger to \"Excellent Debugger Dude\"");
    fais("set the scriptEditor to \"Mr. Ed\"");
    fais("set messageWatcher to \"Cool Message Watcher\"");
    fais("set the variableWatcher to \"The Inspector\"");
    fais("put the debugger & \"|\" & the scriptEditor & \"|\" & the messageWatcher"
         " & \"|\" & the variableWatcher");

    puts("\n== 3. hors du domaine : refusé, la valeur reste ==");
    fais("set the printTextAlign to diagonal");
    fais("set the printTextStyle to \"gras\"");
    fais("set the printMargins to 1,2");
    fais("set the dialingVolume to 9");
    fais("put the printTextAlign && the printTextStyle && the printMargins"
         " && the dialingVolume");

    puts("\n== 4. les trois lectures ==");
    fais("put the environment");
    fais("put the suspended");
    fais("put the stackSpace");
    fais("put stackSpace() > 8000");

    puts("\n== 5. témoins : userLevel et lockRecent ==");
    fais("set the userLevel to 3");
    fais("put the userLevel");
    fais("set the userLevel to 5");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
