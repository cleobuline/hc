/* morceaunom — UN MORCEAU RANGÉ DANS UN NOM, LU ET ÉCRIT COMME LE MORCEAU.
 *
 * Help Extras, champ du texte, signalé le 10 octobre DANS HC (l'application) :
 * « cliquer sur un mot souligné ne va nulle part ». Le script :
 *
 *     put the clickChunk into theChunk
 *     get the textStyle of theChunk
 *     if it contains "group" then
 *       select theChunk
 *       go card (theSection & the value of theChunk)
 *
 * theChunk vaut « char 13 to 17 of bg field "Texte" ». HC rendait la question
 * — « textStyle of theChunk » —, le test échouait, et le gestionnaire
 * finissait sans rien faire ni rien dire. HyperCard Help (checkActiveText)
 * lit de même « the textStyle » et « the textFont of theChunkOfText ».
 *
 * La référence d'Apple écrit les deux sens, et c'est pourquoi l'écriture est
 * corrigée du même coup :
 *
 *     put the textStyle of the clickChunk into theStyle
 *     put the selectedChunk into theChunk
 *     set the textStyle of theChunk to bold
 *
 * MESURÉ pour les OBJETS (docs/mesures/designateur_calcule.txt) : HyperCard
 * évalue une référence avant de la résoudre, et « set the textStyle of ch to
 * bold » pose le style. Pour les MORCEAUX : déduit des piles d'Apple, NON
 * MESURÉ dans HyperCard. Les gestionnaires sont recopiés dans leur FORME ;
 * les piles d'Apple n'entrent pas dans le dépôt. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

/* L'hôte tient the clickChunk, comme HCview.m après un clic. */
static const char *g_clic = "";
static const char *glob(const char *n)
{ return strcasecmp(n, "clickChunk") == 0 ? g_clic : NULL; }

static Object *g_texte;
static void clic(const char *chunk)
{
    g_clic = chunk;
    printf("-> clic sur « %s »\n", chunk);
    hc_go_card(hc_resolve("card \"Sujet\""));
    hc_send(g_texte, "mouseUp");
    char d[96]; hc_describe(hc_current_card(), d, sizeof d);
    printf("   carte : %s\n", d);
}

static void fais(Object *b, const char *script)
{
    hc_set_script(b, script);
    hc_send(b, "mouseUp");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne; h.global_get = glob;
    hc_set_host(&h);

    Object *st = hc_new_stack("Extras"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    hc_new_field(bg, "Section");
    g_texte = hc_new_field(bg, "Texte");
    Object *c1 = hc_new_card(st, bg, "Sujet");
    hc_new_card(st, bg, "Glossary:stack");
    hc_new_card(st, bg, "Placeholder:stack");
    Object *b = hc_new_button(c1, "B");
    hc_set_current_card(c1);
    hc_do("put \"3\" into bkgnd field \"Section\"");
    hc_do("put \"Voir le mot stack ici\" into bkgnd field \"Texte\"");
    hc_do("set the textStyle of char 13 to 17 of bkgnd field \"Texte\" to group");
    hc_do("set the textFont of char 13 to 17 of bkgnd field \"Texte\" to Monaco");

    hc_set_script(g_texte,
        "on mouseUp\n"
        "get bkgnd field \"Section\"\n"
        "if not (it = 3 or it = 4) then exit mouseUp\n"
        "if it = 3 then put \"Glossary:\" into theSection\n"
        "else put \"Placeholder:\" into theSection\n"
        "put the clickChunk into theChunk\n"
        "get the textStyle of theChunk\n"
        "if it contains \"group\" then\n"
        "select theChunk\n"
        "lock screen\n"
        "go card (theSection & the value of theChunk)\n"
        "unlock screen\n"
        "end if\n"
        "end mouseUp\n");

    puts("== 1. Help Extras : un mot souligné mène au glossaire ==");
    clic("char 13 to 17 of bg field \"Texte\"");
    clic("char 1 to 4 of bg field \"Texte\"");
    hc_go_card(c1);
    hc_do("put \"4\" into bkgnd field \"Section\"");
    clic("char 13 to 17 of bg field \"Texte\"");
    hc_go_card(c1);
    hc_do("put \"3\" into bkgnd field \"Section\"");

    puts("\n== 2. lire par un nom : variable, it, the clickChunk ==");
    g_clic = "char 13 to 17 of bg field \"Texte\"";
    fais(b,
        "on mouseUp\n"
        "  put the clickChunk into theChunkOfText\n"
        "  put \"style    \" & the textStyle of theChunkOfText\n"
        "  put \"police   \" & the textFont of theChunkOfText\n"
        "  put \"taille   \" & (the textSize of theChunkOfText = the textSize of char 13 to 17 of bg field \"Texte\")\n"
        "  put \"couleur  \" & the textColor of theChunkOfText\n"
        "  put \"clic     \" & the textStyle of the clickChunk\n"
        "  get theChunkOfText\n"
        "  put \"it       \" & the textStyle of it\n"
        "end mouseUp\n");

    puts("\n== 3. écrire par un nom ==");
    fais(b,
        "on mouseUp\n"
        "  select char 1 to 4 of bg field \"Texte\"\n"
        "  put the selectedChunk into theChunk\n"
        "  set the textStyle of theChunk to bold\n"
        "  put \"selectedChunk  \" & the textStyle of char 1 to 4 of bg field \"Texte\"\n"
        "  set the textStyle of the clickChunk to italic\n"
        "  put \"clickChunk     \" & the textStyle of char 13 to 17 of bg field \"Texte\"\n"
        "  put \"le reste       \" & the textStyle of char 6 to 11 of bg field \"Texte\"\n"
        "end mouseUp\n");

    puts("\n== 4. témoins : ce qui n'est pas un morceau de champ ne change pas ==");
    fais(b,
        "on mouseUp\n"
        "  put \"bg field \" & quote & \"Texte\" & quote into objet\n"
        "  put \"un objet       \" & the textStyle of objet\n"
        "  put \"word 2 of x\" into pasUnChamp\n"
        "  put \"morceau de nom \" & the textStyle of pasUnChamp\n"
        "  put \"nom nu         \" & the textStyle of jamaisPose\n"
        "end mouseUp\n");
    /* Il rendait la question et continuait. La lecture DIRECTE d'un morceau
     * de champ absent arrête le gestionnaire — mesuré, voir
     * v3_prop_sur_morceau — et le nom suit maintenant la même règle :
     * « suite » ne doit pas paraître. */
    puts("\n== 5. un champ absent arrête le gestionnaire, comme en direct ==");
    fais(b,
        "on mouseUp\n"
        "  put \"char 1 to 3 of bg field \" & quote & \"Absent\" & quote into c\n"
        "  put \"champ absent   \" & the textStyle of c\n"
        "  put \"suite\"\n"
        "end mouseUp\n");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
