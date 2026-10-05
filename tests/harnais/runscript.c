/* runscript — CE QUE DEMANDE « RUN THE SCRIPT », DANS « HYPERTALK REFERENCE ».
 *
 * La pile d'Apple, convertie par l'utilisatrice le 4 octobre, joue ses
 * exemples par un bouton « Run the Script ». Avant de jouer, il range l'état
 * de la peinture et des trois fenêtres d'HyperCard ; après, il le remet.
 * DANS HC (l'application), le 5 octobre :
 *
 *     propriété inconnue : cantAbort (× 2)        corrigé la veille (verrou)
 *     propriété inconnue : multiple
 *     propriété inconnue : multiSpace
 *     objet introuvable : it (× 6)
 *
 * Les six « it » : la démonstration range dans une variable le mot-clé de
 * chaque fenêtre — « tool window », « pattern window », « message window » —
 * puis en lit et en repose « the visible of it » et « the loc of it ». Les
 * gestionnaires sont recopiés ici dans leur FORME, pas dans leur texte : la
 * pile d'Apple n'entre pas dans le dépôt.
 *
 * Mesurés en route, et déjà faux avant :
 *   · « show msg », « hide message box » répondaient « objet introuvable » ;
 *   · « set the loc of window "Navigator" to 10,20 » posait « 20 », le seul
 *     dernier élément — fenetres.c écrivait la valeur entre guillemets, et ne
 *     pouvait pas le voir ;
 *   · « choose tool 3 », la forme par le numéro, était une faute de syntaxe.
 *
 * Le noyau ne tient pas les fenêtres : un faux hôte les tient ici. Ce que
 * l'interface en fait — les palettes de HCview.m — n'est pas exécuté ici.
 * NON MESURÉ DANS HYPERCARD : la valeur de « the loc » de ces palettes. */
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

/* Le faux hôte : quatre fenêtres, chacune un visible et une loc. */
static struct { const char *nom; int visible; char loc[64]; } F[] = {
    { "Tools",     1, "600,40"  },
    { "Patterns",  0, "600,300" },
    { "Message",   1, "40,500"  },
    { "Navigator", 1, "10,20"   },
};

static int fenetre(const char *nom, const char *quoi, const char *prop,
                   const char *val, char *out, int outlen)
{
    int i = -1;
    for (int k = 0; k < 4; k++) if (nom && !strcasecmp(nom, F[k].nom)) i = k;
    printf("      -> hôte : %s %s%s%s%s%s\n", quoi, nom ? nom : "?",
           prop ? " " : "", prop ? prop : "", val ? " = " : "", val ? val : "");
    if (i < 0) return 0;
    if (!strcmp(quoi, "existe")) return 1;
    if (!strcmp(quoi, "montre")) { F[i].visible = 1; return 1; }
    if (!strcmp(quoi, "cache") || !strcmp(quoi, "ferme")) { F[i].visible = 0; return 1; }
    if (!strcmp(quoi, "lit")) {
        if      (!strcasecmp(prop, "visible")) snprintf(out, (size_t)outlen, "%s", F[i].visible ? "true" : "false");
        else if (!strcasecmp(prop, "loc"))     snprintf(out, (size_t)outlen, "%s", F[i].loc);
        else return -1;
        return 1;
    }
    if (!strcmp(quoi, "pose")) {
        if      (!strcasecmp(prop, "visible")) F[i].visible = !strcasecmp(val, "true");
        else if (!strcasecmp(prop, "loc")) {
            int x, y;
            if (sscanf(val, "%d,%d", &x, &y) != 2) return -1;   /* l'hôte refuse */
            snprintf(F[i].loc, sizeof F[i].loc, "%d,%d", x, y);
        }
        else return -1;
        return 1;
    }
    return -1;
}

/* Les réglages de peinture, tenus par le faux hôte comme par HCview.m. */
static char g_multiple[16] = "false", g_multispace[16] = "1";
static const char *reglage_lu(const char *nom)
{
    if (!strcasecmp(nom, "multiple"))   return g_multiple;
    if (!strcasecmp(nom, "multiSpace")) return g_multispace;
    if (!strcasecmp(nom, "filled"))     return "false";
    return NULL;
}
static void reglage_pose(const char *nom, const char *val)
{
    printf("      -> hôte : %s = %s\n", nom, val);
    if (!strcasecmp(nom, "multiple"))   snprintf(g_multiple, sizeof g_multiple, "%s", val);
    if (!strcasecmp(nom, "multiSpace")) snprintf(g_multispace, sizeof g_multispace, "%s", val);
}
static void outil(const char *nom) { printf("      -> hôte : outil « %s »\n", nom); }

static Object *b;

static void joue(const char *corps)
{
    char s[2048];
    printf("   %s\n", corps);
    snprintf(s, sizeof s, "on mouseUp\n  %s\nend mouseUp\n", corps);
    hc_set_script(b, s);
    hc_send(b, "mouseUp");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("Reference");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_register_stack(st);
    hc_set_current_card(c);
    b = hc_new_button(c, "Run the Script");
    Object *f = hc_new_field(c, "Exemple");
    hc_set_field_text(f, "texte");

    puts("== 1. sans hôte qui réponde : les fenêtres le disent ==");
    joue("show tool window");
    joue("put the visible of pattern window");
    joue("hide msg");

    h.fenetre = fenetre; h.global_get = reglage_lu; h.global_set = reglage_pose;
    h.choose_tool = outil;
    hc_set_host(&h);

    puts("\n== 2. les mots-clés, écrits en clair ==");
    joue("put the visible of tool window");
    joue("put the loc of pattern window");
    joue("put the visible of message window");
    joue("put the visible of the message box");
    joue("set the loc of tool window to 610,50");
    joue("put the loc of tool window");
    joue("hide pattern window");
    joue("show pattern window");
    joue("close pattern window");
    joue("put there is a tool window");
    joue("put the visible of window \"Tools\"");

    puts("\n== 3. la boîte de messages : show, hide, et toujours un conteneur ==");
    joue("hide msg");
    joue("show message box");
    joue("hide message window");
    joue("put \"abc\" into msg\n  put the number of chars of msg into n\n  put n");

    puts("\n== 4. la forme de « Run the Script » : le mot-clé dans une variable ==");
    joue("put \"tool window,pattern window,message window\" into theWindows\n"
         "  repeat with i = 1 to the number of items of theWindows\n"
         "    get item i of theWindows\n"
         "    put the visible of it & \",\" & the loc of it into line i of saved\n"
         "  end repeat\n"
         "  put saved\n"
         "  repeat with i = 1 to the number of items of theWindows\n"
         "    get item i of theWindows\n"
         "    set the visible of it to false\n"
         "    set the loc of it to 5,i * 10\n"
         "  end repeat\n"
         "  repeat with i = 1 to the number of items of theWindows\n"
         "    get item i of theWindows\n"
         "    set the visible of it to item 1 of line i of saved\n"
         "    set the loc of it to item 2 to 3 of line i of saved\n"
         "  end repeat");
    printf("      état final : Tools %d %s, Patterns %d %s, Message %d %s\n",
           F[0].visible, F[0].loc, F[1].visible, F[1].loc, F[2].visible, F[2].loc);

    puts("\n== 5. une variable qui n'est pas une fenêtre reste ce qu'elle était ==");
    joue("put \"card field 1\" into w\n  put the visible of w");
    joue("put \"tool\" into tool\n  put tool");
    joue("put \"pattern\" into w\n  put the visible of w");

    puts("\n== 6. le point sans guillemets, pour la Navigator aussi ==");
    joue("set the loc of window \"Navigator\" to 30,40");
    joue("put the loc of window \"Navigator\"");
    joue("put 70 into x\n  set the loc of window \"Navigator\" to x,x + 1");
    joue("put the loc of window \"Navigator\"");

    puts("\n== 7. Draw Multiple : the multiple, the multiSpace ==");
    joue("put the multiple && the multiSpace");
    joue("set the multiple to true\n  set the multiSpace to 4");
    joue("put the multiple && the multiSpace");
    joue("put the multiple into m\n  set the multiple to false\n  set the multiple to m\n  put the multiple");
    /* La forme exacte de la démonstration, qui range la peinture par nom. */
    joue("put \"filled,multiple,multiSpace\" into theProperties\n"
         "  repeat with itemNum = 1 to 3\n"
         "    put the value of (\"the\" && item itemNum of theProperties) into item itemNum of saved\n"
         "  end repeat\n"
         "  put saved");

    puts("\n== 8. choose tool <numéro> ==");
    joue("choose tool 3");
    joue("choose tool 1 + 8");
    joue("put 17 into t\n  choose tool t");
    joue("choose tool 19");
    joue("choose line tool");
    joue("choose spray can tool");
    joue("choose \"Select Tool\"");

    hc_free(st);
    return 0;
}
