/* « set … of button (expression) » : le « of » ENTRE PARENTHÈSES.
 *
 * Trouvé le 2 octobre en écrivant le flipper de l'utilisatrice :
 *
 *     set the hilite of button (item k of noms) to false
 *     -> !! objet introuvable : button (item k of noms)
 *
 * La lecture marchait — elle résout par l'arbre —, `hide` aussi ; seule
 * l'écriture, qui relit la référence en TEXTE, prenait le « of » de
 * l'expression pour la portée et cherchait un objet « noms) ».
 * derniere_portee et son jumeau find_kw sautaient les guillemets, pas les
 * parenthèses.
 *
 * Aucune des huit piles d'HyperCard reçues n'emploie cette tournure : le
 * trou n'avait rien cassé de connu. Non mesuré dans HyperCard, mais la forme
 * « button (expression) » y est ordinaire. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("      %s\n", t);
    else if (k == HC_ERR) printf("      [ERR] %s\n", t);
}

static Object *B;
static void joue(const char *s)
{
    char b[512];
    printf("   %s\n", s);
    snprintf(b, sizeof b, "on t\n put \"a,b\" into noms\n put 2 into k\n"
                          " put \"x,y\" into x\n%s\nend t\n", s);
    hc_set_script(B, b);
    hc_send(B, "t");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "C");
    hc_new_button(c, "a");
    hc_new_button(c, "b");
    B = hc_new_button(c, "s");
    hc_set_current_card(c);

    puts("== le « of » de l'expression n'est pas la portée ==");
    joue("set the hilite of button (item k of noms) to true");
    joue("put the hilite of button \"b\"");
    joue("set the textSize of card button (item 1 of noms) to 18");
    joue("put the textSize of button \"a\"");
    joue("set the name of button (item 1 of noms) to \"z\"");
    joue("put the short name of button 1");

    puts("\n== ni le « to » ==");
    joue("set the name of button (char 1 to 1 of \"zut\") to \"w\"");
    joue("put the short name of button 1");

    puts("\n== témoins : ce qui marchait déjà ==");
    joue("set the hilite of button (\"w\" & \"\") to false");
    joue("put the hilite of button \"w\"");
    joue("set the hilite of button \"b\" of this card to false");
    joue("put the hilite of button \"b\"");
    joue("set the hilite of button 1 of card 1 to true");
    joue("put the hilite of button 1");

    hc_free(st);
    return 0;
}
