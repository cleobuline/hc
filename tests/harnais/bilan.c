/* Ce qui répond et ce qui ne répond pas : on DEMANDE au moteur, plutôt que
 * de lire les tables à l'œil. Un nom présent dans un tableau ne prouve pas
 * qu'il marche ; une ligne qui s'exécute sans « ?? verbe inconnu », si. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static int g_err;
static char g_dernier[512];
static void ma_ligne(HcLineKind k, int d, const char *t) {
    (void)d;
    if (k == HC_ERR) { g_err = 1; snprintf(g_dernier, sizeof g_dernier, "%s", t); }
}
static void mon_set(const char *n, const char *v) { (void)n; (void)v; }
static const char *mon_get(const char *n) { (void)n; return NULL; }

static Object *b;

/* Rend 1 si la ligne passe sans erreur d'analyse ou de verbe inconnu. */
static int essai(const char *ligne)
{
    char script[1024];
    snprintf(script, sizeof script, "on t\n  %s\nend t\n", ligne);
    hc_set_script(b, script);
    g_err = 0; g_dernier[0] = 0;
    hc_send(b, "t");
    if (!g_err) return 1;
    /* Une erreur d'EXÉCUTION (fichier absent, objet introuvable) prouve que
     * la commande existe. Seul le refus de COMPRENDRE compte comme manquant. */
    if (strstr(g_dernier, "verbe inconnu") || strstr(g_dernier, "inconnu") ||
        strstr(g_dernier, "Syntax") || strstr(g_dernier, "syntaxe") ||
        strstr(g_dernier, "compris")) return 0;
    return 1;
}

static void section(const char *titre, const char **lignes)
{
    printf("\n── %s\n", titre);
    for (int i = 0; lignes[i]; i++) {
        int ok = essai(lignes[i]);
        printf("   %s  %-46s", ok ? "OK  " : "MANQ", lignes[i]);
        if (!ok && g_dernier[0]) printf("  %s", g_dernier);
        printf("\n");
    }
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.global_set = mon_set; h.global_get = mon_get;
    hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    b = hc_new_button(c, "B");
    hc_set_current_card(c);

    static const char *commandes[] = {
        "put 1 into x", "get 1", "add 1 to x", "subtract 1 from x",
        "multiply x by 2", "divide x by 2",
        "answer \"a\"", "ask \"a\"", "beep", "wait 1 ticks",
        "go to card 1", "visual effect dissolve", "choose browse tool",
        "doMenu \"New Card\"", "find \"x\"", "sort by 1",
        "show card field 1", "hide card field 1",
        "select text of card field 1", "click at 1,1", "drag from 1,1 to 2,2",
        "type \"a\"", "delete char 1 of x",
        "set the name of me to \"z\"", "push card", "pop card",
        "lock screen", "unlock screen", "mark all cards", "unmark all cards",
        "create menu \"M\"", "reset menuBar", "enable menuItem 1 of menu \"M\"",
        "convert \"1/1/90\" to seconds", "read from file \"z\" until end",
        "write \"a\" to file \"z\"", "open file \"z\"", "close file \"z\"",
        "play \"boing\"", "start using stack \"z\"", "stop using stack \"z\"",
        "print card", "save this stack", "send \"t\" to me",
        "dial \"555\"", "export paint to file \"z\"", "import paint from file \"z\"",
        "edit script of me", "palette \"z\"", "picture \"z\"",
        "request \"z\" from program \"z\"", "reply \"z\"", "help",
        "do \"beep\"", "global g", "exit t", "pass t", "return 1",
        "delete card button \"nope\"", "answer file \"a\"", "ask password \"a\"",
        "answer program \"a\"", "select empty", "select card button 1",
        NULL
    };
    section("COMMANDES", commandes);

    static const char *fonctions[] = {
        "put the date", "put the time", "put the seconds", "put the ticks",
        "put the long date", "put the abbreviated date",
        "put the number of cards", "put the number of backgrounds",
        "put the result", "put the selection", "put the selectedText",
        "put the selectedChunk", "put the selectedField", "put the selectedLine",
        "put the clickText", "put the clickLoc", "put the clickChunk",
        "put the mouse", "put the mouseLoc", "put the mouseClick",
        "put the shiftKey", "put the optionKey", "put the commandKey",
        "put the tool", "put the screenRect", "put the target",
        "put the me", "put the params", "put the paramCount", "put the param(1)",
        "put the value of \"1+1\"", "put the length of \"abc\"",
        "put the number of chars of \"abc\"", "put the random of 10",
        "put the sum of 1", "put the average of 1", "put the min of 1",
        "put the trunc of 1.5", "put the round of 1.5", "put the abs of -1",
        "put the sqrt of 4", "put the sin of 1", "put the annuity of 1,2",
        "put the compound of 1,2", "put the charToNum of \"a\"",
        "put the numToChar of 65", "put the offset of \"a\",\"bab\"",
        "put the diskSpace", "put the heapSpace", "put the stacks",
        "put the windows", "put the programs", "put the sound",
        "put the menus", "put the version", "put the systemVersion",
        "put the freeSize", "put the size", "put the destination",
        NULL
    };
    section("FONCTIONS", fonctions);

    static const char *props[] = {
        "put the name of me", "put the id of me", "put the script of me",
        "put the visible of me", "put the rect of me", "put the loc of me",
        "put the width of me", "put the height of me",
        "put the style of me", "put the textFont of me", "put the textSize of me",
        "put the textStyle of me", "put the textAlign of me",
        "put the textHeight of me", "put the icon of me",
        "put the hilite of me", "put the autoHilite of me", "put the enabled of me",
        "put the showName of me", "put the family of me",
        "put the cursor", "put the itemDelimiter", "put the numberFormat",
        "put the lockScreen", "put the lockMessages", "put the lockRecent",
        "put the userLevel", "put the blindTyping", "put the powerKeys",
        "put the dragSpeed", "put the editBkgnd", "put the language",
        "put the freeSize of this stack", "put the name of this stack",
        "put the number of this card", "put the short name of this card",
        "put the owner of me", "put the partNumber of me",
        "put the sharedText of card field 1", "put the sharedHilite of me",
        "put the dontSearch of card field 1", "put the autoTab of card field 1",
        "put the lockText of card field 1", "put the wideMargins of card field 1",
        "put the showLines of card field 1", "put the scroll of card field 1",
        "put the multipleLines of card field 1", "put the autoSelect of card field 1",
        "put the titleWidth of me", "put the highlight of me",
        NULL
    };
    section("PROPRIETES", props);

    hc_free(st);
    return 0;
}
