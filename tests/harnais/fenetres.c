/* fenetres — LES FENÊTRES NOMMÉES, ET LA PALETTE NAVIGATOR.
 *
 * « window "Navigator" » n'était pas compris : l'analyseur s'arrêtait sur le
 * mot « window », et « palette », « close window », « show window »
 * répondaient « ne sait pas faire ». Or une palette EST une fenêtre nommée,
 * et les piles la pilotent ainsi — Minkowski Stack 1 :
 *
 *     palette "ClickPoints"
 *     set hilitedButton of window "ClickPoints" to 1
 *     if there is a window "ClickPoints" then …
 *     on closePalette palName
 *
 * Le noyau ne tient aucun état de fenêtre : il lit le nom et interroge
 * l'hôte (le rappel `fenetre`, hc_core.h). Ce harnais joue :
 *
 *   1. sans hôte qui réponde : aucune fenêtre n'existe, et chaque forme le
 *      dit ;
 *   2. avec un faux hôte qui tient la palette Navigator — ses onze commandes
 *      sont celles MESURÉES dans HyperCard le 2 octobre, « the commands of
 *      window "Navigator" » — : ouverture, openPalette, lecture et écriture
 *      des propriétés, show, hide, close et closePalette, la case de
 *      fermeture (hc_palette_fermee) ;
 *   3. « card window » n'a pas bougé.
 *
 * NON MESURÉ DANS HYPERCARD : la valeur de « the hilitedButton » d'une
 * palette Navigator — le faux hôte la tient comme un nombre posé, à 0 au
 * départ —, et « the loc of window » d'une palette. */
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static const char *COMMANDES =
    "doMenu \"Back\"\ndoMenu \"Home\"\ndoMenu \"Help\"\ndoMenu \"Recent\"\n"
    "doMenu \"First\"\ndoMenu \"Prev\"\ndoMenu \"Next\"\ndoMenu \"Last\"\n"
    "doMenu \"Find...\"\ndoMenu \"Message\"\ndoMenu \"Next window\"";

static int  g_ouverte = 0, g_visible = 0, g_hilite = 0;
static char g_loc[64] = "10,20";

static int fenetre(const char *nom, const char *quoi, const char *prop,
                   const char *val, char *out, int outlen)
{
    int navigator = nom && !strcasecmp(nom, "Navigator");
    if (!strcmp(quoi, "palette")) {
        if (!navigator) return 0;
        g_ouverte = g_visible = 1;
        if (val && *val) snprintf(g_loc, sizeof g_loc, "%s", val);
        snprintf(out, (size_t)outlen, "7");
        return 1;
    }
    if (!navigator || !g_ouverte) return 0;
    if (!strcmp(quoi, "existe")) return 1;
    if (!strcmp(quoi, "montre")) { g_visible = 1; return 1; }
    if (!strcmp(quoi, "cache"))  { g_visible = 0; return 1; }
    if (!strcmp(quoi, "ferme"))  { g_ouverte = 0; snprintf(out, (size_t)outlen, "palette 7"); return 1; }
    if (!strcmp(quoi, "lit")) {
        if      (!strcasecmp(prop, "commands"))      snprintf(out, (size_t)outlen, "%s", COMMANDES);
        else if (!strcasecmp(prop, "buttonCount"))   snprintf(out, (size_t)outlen, "11");
        else if (!strcasecmp(prop, "hilitedButton")) snprintf(out, (size_t)outlen, "%d", g_hilite);
        else if (!strcasecmp(prop, "visible"))       snprintf(out, (size_t)outlen, "%s", g_visible ? "true" : "false");
        else if (!strcasecmp(prop, "loc") || !strcasecmp(prop, "location"))
                                                     snprintf(out, (size_t)outlen, "%s", g_loc);
        else if (!strcasecmp(prop, "name"))          snprintf(out, (size_t)outlen, "Navigator");
        else return -1;
        return 1;
    }
    if (!strcmp(quoi, "pose")) {
        if      (!strcasecmp(prop, "hilitedButton")) g_hilite = atoi(val);
        else if (!strcasecmp(prop, "loc") || !strcasecmp(prop, "location"))
                                                     snprintf(g_loc, sizeof g_loc, "%s", val);
        else if (!strcasecmp(prop, "visible"))       g_visible = !strcasecmp(val, "true");
        else return -1;
        return 1;
    }
    return 0;
}

static Object *b;

static void joue(const char *corps)
{
    char s[1024];
    printf("   %s\n", corps);
    snprintf(s, sizeof s, "on mouseUp\n  %s\nend mouseUp\n", corps);
    hc_set_script(b, s);
    hc_send(b, "mouseUp");
}

static const char *TOUT[] = {
    "palette navigator",
    "put there is a window \"Navigator\"",
    "put there is no window \"Zut\"",
    "put the commands of window \"Navigator\"",
    "put the buttonCount of window \"Navigator\"",
    "put the hilitedButton of window \"Navigator\"",
    "set the hilitedButton of window \"Navigator\" to 2",
    "put the hilitedButton of window \"Navigator\"",
    "put \"Navigator\" into w\n  put the loc of window w",
    "set the loc of window \"Navigator\" to \"50,60\"",
    "put the loc of window \"Navigator\"",
    "hide window \"Navigator\"",
    "put the visible of window \"Navigator\"",
    "show window \"Navigator\"",
    "put the visible of window \"Navigator\"",
    "set the zorglub of window \"Navigator\" to 3",
    "put the zorglub of window \"Navigator\"",
    "close window \"Navigator\"",
    "put there is a window \"Navigator\"",
    "close window \"Navigator\"",
    "put the commands of window \"Navigator\"",
    "palette \"Zut\"",
    "palette \"Navigator\", \"100,100\"",
    "put the loc of window \"Navigator\"",
    NULL
};

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_register_stack(st);
    hc_set_current_card(c);
    b = hc_new_button(c, "B");
    hc_set_script(st,
        "on openPalette nom, id\n  put \"openPalette \" & nom & \", \" & id\nend openPalette\n"
        "on closePalette nom, id\n  put \"closePalette \" & nom & \", \" & id\nend closePalette\n");

    puts("== 1. sans hôte qui réponde : aucune fenêtre ==");
    joue("palette navigator");
    joue("put there is a window \"Navigator\"");
    joue("close window \"Navigator\"");
    joue("hide window \"Navigator\"");
    joue("put the commands of window \"Navigator\"");

    puts("\n== 2. un hôte qui tient la palette Navigator ==");
    h.fenetre = fenetre;
    hc_set_host(&h);
    for (int i = 0; TOUT[i]; i++) joue(TOUT[i]);
    puts("   -- la case de fermeture : hc_palette_fermee");
    hc_palette_fermee("Navigator", 7);

    puts("\n== 3. « card window » n'a pas bougé ==");
    joue("put the top of card window + 1");
    joue("put the width of card window");

    hc_free(st);
    return 0;
}
