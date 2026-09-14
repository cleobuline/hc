/* itemDelimiter peut faire plusieurs octets.
 *
 * Il tenait dans un char, donc dans UN OCTET. « set the itemDelimiter to "é" »
 * n'en retenait que le premier, 0xC3, et le découpage coupait là-dessus :
 *
 *     item 2 of "aébéc"   ->  "\xA9b"    la seconde moitié de l'é, collée au b
 *     the itemDelimiter   ->  "\xC3"     une chaîne UTF-8 invalide
 *
 * Le COMPTE, lui, donnait 3 — le juste nombre, par accident : l'octet 0xC3
 * apparaît deux fois dans « aébéc ». Compte juste et contenu faux, c'est le
 * pire des deux mondes pour qui cherche le défaut.
 *
 * HyperCard était en encodage mono-octet et la question ne se posait pas. Dès
 * lors que HC est en UTF-8, un délimiteur peut occuper plusieurs octets — et
 * il traverse toute l'API des morceaux : compter, lire, écrire, supprimer. */
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
    char s[700];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", corps);
    printf("── %s\n", titre);
    hc_set_script(b, s);
    hc_send(b, "t");
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "U");
    hc_new_field(c, "x");
    b = hc_new_button(c, "B");
    hc_set_current_card(c);

    puts("=== la virgule par défaut, et un délimiteur ASCII ===");
    essai("virgule",
          "put the number of items of \"a,b,c\" & \" \" & item 2 of \"a,b,c\"");
    essai("point-virgule",
          "set the itemDelimiter to \";\"\n"
          "  put the number of items of \"a;b;c\" & \" \" & item 2 of \"a;b;c\""
          " & \" [\" & the itemDelimiter & \"]\"");

    puts("\n=== un délimiteur accentué : deux octets ===");
    essai("compter et lire",
          "set the itemDelimiter to \"é\"\n"
          "  put the number of items of \"aébéc\" & \" [\" & item 2 of \"aébéc\""
          " & \"] [\" & the itemDelimiter & \"]\"");
    essai("le premier et le dernier",
          "set the itemDelimiter to \"é\"\n"
          "  put \"[\" & item 1 of \"aébéc\" & \"] [\" & item 3 of \"aébéc\" & \"]\"");
    essai("une plage",
          "set the itemDelimiter to \"é\"\n"
          "  put \"[\" & item 1 to 2 of \"aébéc\" & \"]\"");

    puts("\n=== écrire : le délimiteur posé doit être entier ===");
    essai("remplacer un item",
          "set the itemDelimiter to \"é\"\n"
          "  put \"aébéc\" into v\n  put \"X\" into item 2 of v\n"
          "  put \"[\" & v & \"] longueur \" & the length of v");
    essai("étendre au-delà de la fin",
          "set the itemDelimiter to \"é\"\n"
          "  put \"a\" into v\n  put \"X\" into item 3 of v\n"
          "  put \"[\" & v & \"] \" & the number of items of v");

    puts("\n=== supprimer : le délimiteur part en entier, pas à moitié ===");
    essai("delete item 2",
          "set the itemDelimiter to \"é\"\n"
          "  put \"aébéc\" into v\n  delete item 2 of v\n"
          "  put \"[\" & v & \"] longueur \" & the length of v & \" (attendu 3)\"");
    essai("delete item 1",
          "set the itemDelimiter to \"é\"\n"
          "  put \"aébéc\" into v\n  delete item 1 of v\n  put \"[\" & v & \"]\"");
    essai("delete le dernier",
          "set the itemDelimiter to \"é\"\n"
          "  put \"aébéc\" into v\n  delete item 3 of v\n  put \"[\" & v & \"]\"");

    puts("\n=== un délimiteur de plusieurs caractères ===");
    /* HyperCard n'en acceptait qu'un ; rien n'oblige à s'y tenir, et le
     * découpage sur une chaîne ne coûte pas plus cher que sur un octet. */
    essai("« -- » comme séparateur",
          "set the itemDelimiter to \"--\"\n"
          "  put the number of items of \"a--b--c\" & \" [\""
          " & item 2 of \"a--b--c\" & \"]\"");

    puts("\n=== un délimiteur vide retombe sur la virgule ===");
    essai("vide",
          "set the itemDelimiter to empty\n"
          "  put the number of items of \"a,b,c\" & \" [\" & item 2 of \"a,b,c\" & \"]\"");

    hc_free(st);
    return 0;
}
