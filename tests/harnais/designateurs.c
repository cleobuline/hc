/* LES DEUX FAMILLES DE DÉSIGNATEURS, CÔTE À CÔTE.
 *
 * « the foundChunk » et « the selectedChunk » disent la même sorte de chose —
 * où se trouve un bout de texte — et doivent donc s'écrire pareil. Mesuré
 * dans Basilisk le 27/09 :
 *
 *     the selectedChunk -> char 2 to 4 of card field 1
 *     the selectedField -> card field 1
 *     the selectedLine  -> line 1 of card field 1
 *     the foundChunk    -> char 7 to 10 of card field 1
 *     the foundField    -> card field 1
 *
 * LA COUCHE ET LE NUMÉRO, JAMAIS LE NOM, et « bkgnd » et non « bg ». HC
 * écrivait trois formes différentes :
 *
 *     selectedChunk     char 2 to 4 of card field "A"     le NOM
 *     selectedField     field "A"                          ni couche ni numéro
 *     selectedLine      1                                  un nombre NU
 *
 * Le nom seul est celui qui coûte : un champ de carte et un champ de fond
 * portant le même nom rendaient exactement le même désignateur, et un script
 * qui relit la sélection pour y écrire visait l'un ou l'autre au hasard.
 *
 * RIEN NE COUVRAIT selectedField NI selectedLine. Les corriger n'a fait bouger
 * aucun témoin : deux propriétés que les piles lisent couramment, et pas une
 * ligne de harnais. C'est ce trou-là que ce fichier ferme.
 *
 * L'ALLER-RETOUR EST LA SEULE VÉRIFICATION QUI COMPTE. Un désignateur sert à
 * être RELU — « put the selectedChunk into ou » puis « value(ou) ». Un numéro
 * faux casserait cet aller-retour en silence, là où une forme simplement
 * inhabituelle se verrait tout de suite.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if      (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *b;
static void essai(const char *titre, const char *corps)
{
    char s[900];
    snprintf(s, sizeof s, "on t\n%s\nend t\n", corps);
    hc_set_script(b, s);
    printf("== %s\n", titre);
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);
    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    Object *f1 = hc_new_field(c, "A");
    hc_set_field_text(f1, "alpha beta gamma\nseconde ligne");
    hc_new_field(c, "B");
    hc_new_field(bg, "T");
    b = hc_new_button(c, "Z");
    hc_set_current_card(c);

    essai("1. LA SÉLECTION dans un champ de CARTE",
     "  select char 2 to 4 of card field \"A\"\n"
     "  put \"chunk : \" & the selectedChunk\n"
     "  put \"field : \" & the selectedField\n"
     "  put \"line  : \" & the selectedLine\n"
     "  put \"text  : \" & the selectedText\n"
     "  put \"   (HyperCard : char 2 to 4 of card field 1 / card field 1\"\n"
     "  put \"    / line 1 of card field 1)\"");

    essai("2. LA SÉLECTION dans un champ de FOND : « bkgnd », pas « bg »",
     "  put \"sigma tau\" into bg field \"T\"\n"
     "  select word 2 of bg field \"T\"\n"
     "  put \"chunk : \" & the selectedChunk\n"
     "  put \"field : \" & the selectedField\n"
     "  put \"line  : \" & the selectedLine");

    essai("3. LE NUMÉRO EST CELUI DU CHAMP, et il change",
     "  put \"omega\" into card field \"B\"\n"
     "  select word 1 of card field \"B\"\n"
     "  put \"field : \" & the selectedField & \"   (le DEUXIÈME champ de carte)\"");

    /* §4 — ET UNE FORME QUE « select » NE SAIT PAS FAIRE, trouvée en écrivant
     * ce harnais : « select word 1 of line 2 of card field "A" » — un morceau
     * DE morceau — rend « ne sait pas faire ». La lecture les gère depuis ce
     * matin, l'écriture depuis des jours ; c'est « select » qui ne les prend
     * pas. Inscrit tel quel, sans l'élargir : ce n'est pas le sujet de ce
     * fichier, et une correction glissée là dedans se mélangerait au format
     * des désignateurs. */
    essai("4. LA SECONDE LIGNE : le numéro de ligne suit la sélection",
     "  select line 2 of card field \"A\"\n"
     "  put \"line  : \" & the selectedLine\n"
     "  put \"chunk : \" & the selectedChunk\n"
     "  put \"   (et un morceau DE morceau, que select ne sait pas faire :)\"\n"
     "  select word 1 of line 2 of card field \"A\"");

    essai("5. LA FAMILLE « found… », qui s'écrit pareil",
     "  go to card 1\n"
     "  find \"bet\"\n"
     "  put \"chunk : \" & the foundChunk\n"
     "  put \"field : \" & the foundField\n"
     "  put \"line  : \" & the foundLine");

    essai("6. L'ALLER-RETOUR : le désignateur SE RELIT",
     "  select char 7 to 10 of card field \"A\"\n"
     "  put the selectedChunk into ou\n"
     "  put \"carte : \" & ou & \"  ->  [\" & value(ou) & \"]\"\n"
     "  select word 2 of bg field \"T\"\n"
     "  put the selectedChunk into ou2\n"
     "  put \"fond  : \" & ou2 & \"  ->  [\" & value(ou2) & \"]\"");

    essai("7. LE NIVEAU OBJET dit la même chose que le niveau global",
     "  select char 2 to 4 of card field \"A\"\n"
     "  put \"global : \" & the selectedChunk\n"
     "  put \"objet  : \" & the selectedChunk of card field \"A\"\n"
     "  put \"ligne  : \" & the selectedLine of card field \"A\"\n"
     "  put \"   (un autre champ ne porte pas la sélection :)\"\n"
     "  put \"autre  : [\" & the selectedChunk of card field \"B\" & \"]\"");

    essai("8. SANS SÉLECTION, les trois sont vides",
     "  select empty\n"
     "  put \"chunk : [\" & the selectedChunk & \"]\"\n"
     "  put \"field : [\" & the selectedField & \"]\"\n"
     "  put \"line  : [\" & the selectedLine & \"]\"");

    hc_free(st);
    return 0;
}
