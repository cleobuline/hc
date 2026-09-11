#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static int g_nclicks = 0;   /* mouseClick est CONSOMMATEUR : on compte les lectures */

static const char *mon_global_get(const char *name)
{
    if (!strcasecmp(name, "mouseClick")) { g_nclicks++; return "true"; }
    if (!strcasecmp(name, "textHeight")) return "16";
    if (!strcasecmp(name, "textFont"))   return "Geneva";
    if (!strcasecmp(name, "pattern"))    return "12";
    if (!strcasecmp(name, "lineSize"))   return "3";
    if (!strcasecmp(name, "clickText"))  return "mot-clique";
    return NULL;
}
static void ligne(HcLineKind kind, int depth, const char *text)
{
    (void)depth;
    if (kind == HC_MSG) printf("   [message box] %s\n", text);
    else                printf("   %s\n", text);
}

int main(void)
{
    HcHost h;
    memset(&h, 0, sizeof h);
    h.global_get = mon_global_get;
    h.line = ligne;
    hc_set_host(&h);

    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *btn   = hc_new_button(card1, "GoBtn");
    hc_set_current_card(card1);

    hc_set_script(btn,
        "on mouseUp\n"
        "  debug raz\n"
        "  put the mouseClick\n"
        "  put the textHeight\n"
        "  put the textFont\n"
        "  put the pattern\n"
        "  put the lineSize\n"
        "  put the clickText\n"
        "  put 42 into pattern\n"
        "  put the pattern -- la variable doit gagner\n"
        "  debug bilan\n"
        "end mouseUp\n");
    hc_send(btn, "mouseUp");
    printf(">>> lectures de mouseClick chez l'hote : %d (doit valoir 1)\n", g_nclicks);
    hc_free(stack);
    return 0;
}
