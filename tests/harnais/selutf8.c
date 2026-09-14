/* « the selectedChunk » et « the foundChunk » comptent des CARACTÈRES.
 *
 * Le noyau garde ses positions de sélection et de recherche en OCTETS — et
 * c'est la bonne unité interne, ce n'est pas un pis-aller : hct_chunk_bornes
 * rend des octets, la recherche rend des octets, le clic dans un champ rend
 * des octets, et le rendu du texte les convertit déjà vers l'UTF-16 de Cocoa.
 *
 * Mais HyperTalk, lui, compte en caractères. Ces deux propriétés annonçaient
 * l'offset d'octet tel quel :
 *
 *     select char 1 of "été"   ->  char 1 to 2     au lieu de char 1 to 1
 *     select char 2 of "été"   ->  char 3 to 3     au lieu de char 2 to 2
 *     find "demain" dans "été demain"
 *                              ->  char 7 to 12    au lieu de char 5 to 10
 *
 * « the selection », lui, allait bien : il lit ces octets et rend le bon
 * texte. Seule l'ANNONCE mentait — ce qui est pire qu'une erreur franche,
 * parce qu'un script qui réinjecte selectedChunk dans un « put ... into char
 * X to Y » écrit alors au mauvais endroit.
 *
 * La conversion se fait à la frontière du langage, et nulle part ailleurs. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *b;

static void essai(const char *titre, const char *corps)
{
    char s[600];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", corps);
    printf("── %s\n", titre);
    hc_set_script(b, s);
    hc_send(b, "t");
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("T"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "U");
    hc_new_field(c, "x");
    b = hc_new_button(c, "B");
    hc_set_current_card(c);

    puts("=== en ASCII, les deux unités coïncident : rien ne doit changer ===");
    essai("select char 2 of \"abc\"",
          "put \"abc\" into card field \"x\"\n"
          "  select char 2 of card field \"x\"\n"
          "  put the selection & \"  \" & the selectedChunk");

    puts("\n=== un é occupe DEUX octets et UN caractère ===");
    essai("select char 1 of \"été\"",
          "put \"été\" into card field \"x\"\n"
          "  select char 1 of card field \"x\"\n"
          "  put the selection & \"  \" & the selectedChunk");
    essai("select char 2 of \"été\"",
          "put \"été\" into card field \"x\"\n"
          "  select char 2 of card field \"x\"\n"
          "  put the selection & \"  \" & the selectedChunk");
    essai("select char 3 of \"été\"",
          "put \"été\" into card field \"x\"\n"
          "  select char 3 of card field \"x\"\n"
          "  put the selection & \"  \" & the selectedChunk");
    essai("select char 1 to 3 of \"été\"",
          "put \"été\" into card field \"x\"\n"
          "  select char 1 to 3 of card field \"x\"\n"
          "  put the selection & \"  \" & the selectedChunk");

    puts("\n=== l'aller-retour : ce qu'annonce selectedChunk doit être vrai ===");
    /* La vérification qui compte vraiment : reprendre les bornes annoncées et
     * relire le morceau qu'elles désignent. Si l'annonce ment, ce texte-ci ne
     * sera pas celui qu'on avait sélectionné. */
    essai("relire le morceau annoncé",
          "put \"été demain\" into card field \"x\"\n"
          "  select char 2 to 3 of card field \"x\"\n"
          "  put \"sélection : [\" & the selection & \"]\"\n"
          "  put \"annoncé   : \" & the selectedChunk\n"
          "  put \"relu      : [\" & char 2 to 3 of card field \"x\" & \"]\"");

    puts("\n=== find : même règle ===");
    essai("find \"demain\" dans \"été demain\"",
          "put \"été demain\" into card field \"x\"\n"
          "  find \"demain\"\n"
          "  put \"[\" & the foundText & \"]  \" & the foundChunk");
    essai("find sur un motif accentué",
          "put \"voici été ici\" into card field \"x\"\n"
          "  find \"été\"\n"
          "  put \"[\" & the foundText & \"]  \" & the foundChunk");

    puts("\n=== caractères hors du plan de base : un emoji fait quatre octets ===");
    essai("select char 2 of \"a🐟b\"",
          "put \"a🐟b\" into card field \"x\"\n"
          "  select char 2 of card field \"x\"\n"
          "  put the selection & \"  \" & the selectedChunk");
    essai("select char 3 of \"a🐟b\"",
          "put \"a🐟b\" into card field \"x\"\n"
          "  select char 3 of card field \"x\"\n"
          "  put the selection & \"  \" & the selectedChunk");

    hc_free(st);
    return 0;
}
