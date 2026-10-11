/* marquage — « mark cards where … », « mark cards by finding … »,
 * « unmark first marked card ».
 *
 * Relevé le 11 octobre sur HyperTalk Reference, dont la carte « mark » donne
 * ces trois formes. « where » répondait « objet introuvable » — l'exécuteur
 * renvoyait à un ancien chemin retiré depuis —, « by finding » marquait la
 * carte COURANTE en silence, et l'ordinal « ne sait pas faire ».
 *
 * Les formes viennent de la référence d'Apple ; NON MESURÉ dans HyperCard :
 * que « mark cards where » laisse intactes les cartes où la condition est
 * fausse. Les témoins de la section 6 ne doivent pas bouger. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      = %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static void fais(const char *l) { printf("   %s\n", l); hc_do(l); }

static Object *g_pile;
static void marques(void)
{
    printf("      marquées :");
    int n = 0;
    for (int i = 0; i < g_pile->nparts; i++) {
        Object *c = g_pile->parts[i];
        if (c->type == OBJ_CARD && c->marked) { printf(" %s", c->name); n++; }
    }
    printf("%s\n", n ? "" : " aucune");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);

    g_pile = hc_new_stack("Fiches"); hc_register_stack(g_pile);
    Object *bg = hc_new_background(g_pile, "Fiche");
    hc_new_field(bg, "title");
    hc_new_field(bg, "Pages");
    hc_new_field(bg, "Author");
    const char *noms[] = { "C1", "C2", "C3", "C4", "C5" };
    const char *titres[] = { "select a card", "the find command",
                             "select text", "go to a card", "Selection rules" };
    const char *pages[] = { "Page 1", "Page 2", "Page 1", "Page 3", "Page 1" };
    const char *auteurs[] = { "Daffy Duck", "Duffy Moon", "Moon Unit",
                              "Duffy", "Cupertino, CA" };
    Object *cartes[5];
    for (int i = 0; i < 5; i++) cartes[i] = hc_new_card(g_pile, bg, noms[i]);
    for (int i = 0; i < 5; i++) {
        hc_set_current_card(cartes[i]);
        char l[256];
        snprintf(l, sizeof l, "put \"%s\" into bg field \"title\"", titres[i]); hc_do(l);
        snprintf(l, sizeof l, "put \"%s\" into bg field \"Pages\"", pages[i]); hc_do(l);
        snprintf(l, sizeof l, "put \"%s\" into bg field \"Author\"", auteurs[i]); hc_do(l);
    }
    hc_set_current_card(cartes[1]);
    hc_set_script(g_pile,
        "function isaCommand\n"
        "  return word 1 of field \"title\" is \"go\"\n"
        "end isaCommand\n");

    puts("== 1. mark cards where, les trois exemples de la référence ==");
    fais("mark cards where field \"title\" contains \"select\"");
    marques();
    fais("unmark all cards");
    fais("mark cards where (word 2 of field \"Pages\" = 1)");
    marques();
    fais("unmark all cards");
    fais("mark cards where isaCommand()");
    marques();
    fais("put the short name of this card");

    puts("\n== 2. unmark cards where : n'efface que là où c'est vrai ==");
    fais("mark all cards");
    fais("unmark cards where field \"Pages\" is \"Page 1\"");
    marques();
    fais("put the number of marked cards");

    puts("\n== 3. mark cards by finding ==");
    fais("unmark all cards");
    fais("mark cards by finding \"Duffy\" in bkgnd field \"Author\"");
    marques();
    fais("unmark all cards");
    fais("mark cards by finding whole \"Cupertino, CA\" in field \"Author\"");
    marques();
    fais("unmark all cards");
    fais("mark cards by finding chars \"uff\"");
    marques();
    fais("unmark all cards");
    fais("mark cards by finding \"card select\"");
    marques();

    puts("\n== 4. l'ordinal des cartes marquées ==");
    fais("mark all cards");
    fais("unmark first marked card");
    marques();
    fais("unmark last marked card");
    marques();

    puts("\n== 5. une condition en faute arrête, et la carte reste la même ==");
    fais("unmark all cards");
    fais("mark cards where field \"Absent\" is empty");
    marques();
    fais("put the short name of this card");

    puts("\n== 6. témoins : les formes d'avant ==");
    fais("unmark all cards");
    fais("mark card 3");
    fais("mark this card");
    marques();
    fais("go next marked card");
    fais("put the short name of this card");
    fais("unmark all cards");
    marques();

    hc_unregister_stack(g_pile);
    hc_free(g_pile);
    return 0;
}
