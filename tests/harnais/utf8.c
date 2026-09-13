/* Les caractères accentués comptent pour UN.
 *
 * « the number of chars of "été" » rendait CINQ : le découpage comptait des
 * octets, et un é en fait deux en UTF-8. Pire que le compte faux, « char 1 of
 * "été" » rendait la MOITIÉ d'un é — une demi-séquence, que rien ne sait
 * afficher, et qu'on ne pouvait ni recoller ni comparer.
 *
 * HyperCard était en MacRoman, un octet par caractère, et la question ne se
 * posait pas. Nos fichiers sont en UTF-8, et c'est le caractère que
 * l'utilisateur compte.
 *
 * Les mots, les éléments et les lignes allaient déjà bien : ils se repèrent à
 * des séparateurs qui sont tous en ASCII, et un octet de continuation n'en
 * est jamais un. Seul « char » indexait de l'octet. Ce harnais couvre les
 * deux sens — lire un morceau et en écrire un — plus les fonctions qui
 * comptent ou convertissent des caractères. */
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

static void essai(const char *corps)
{
    char s[700];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", corps);
    printf("── %-46s", corps);
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
    hc_new_field(c, "f");
    b = hc_new_button(c, "B");
    hc_set_current_card(c);

    puts("=== compter ===");
    essai("put the number of chars of \"ete\"");
    essai("put the number of chars of \"été\"");
    essai("put the length of \"été\"");
    essai("put length(\"été\")");
    essai("put the number of chars of \"日本語\"");
    essai("put the number of chars of \"\"");

    puts("\n=== lire un caractère ===");
    essai("put char 1 of \"été\"");
    essai("put char 2 of \"été\"");
    essai("put char 3 of \"été\"");
    essai("put char 4 of \"été\"");
    essai("put last char of \"été\"");
    essai("put char 1 to 2 of \"été\"");
    essai("put char 2 to 3 of \"été\"");
    essai("put middle char of \"été\"");

    puts("\n=== écrire un caractère ===");
    essai("put \"été\" into v\n  put \"X\" into char 1 of v\n  put v");
    essai("put \"été\" into v\n  put \"X\" into char 2 of v\n  put v");
    essai("put \"été\" into v\n  put \"X\" into char 3 of v\n  put v");
    essai("put \"ete\" into v\n  put \"é\" into char 2 of v\n  put v");
    essai("put \"été\" into v\n  delete char 2 of v\n  put v");
    essai("put \"été\" into v\n  delete char 1 of v\n  put v & \"|\" & the number of chars of v");

    puts("\n=== mots, éléments, lignes : déjà justes, et ils le restent ===");
    essai("put the number of words of \"été où\"");
    essai("put word 2 of \"été où\"");
    essai("put the number of items of \"a,é,b\"");
    essai("put item 2 of \"a,é,b\"");
    essai("put the number of lines of \"é\" & return & \"à\"");
    essai("put line 2 of (\"é\" & return & \"à\")");

    puts("\n=== offset, en caractères ===");
    essai("put offset(\"t\", \"été\")");
    essai("put offset(\"é\", \"été\")");
    essai("put offset(\"b\", \"aéb\")");
    essai("put offset(\"z\", \"été\")");

    puts("\n=== charToNum et numToChar font l'aller-retour ===");
    essai("put charToNum(\"A\")");
    essai("put charToNum(\"é\")");
    essai("put numToChar(233)");
    essai("put numToChar(charToNum(\"é\"))");
    essai("put the number of chars of numToChar(233)");
    essai("put charToNum(\"日\")");
    essai("put numToChar(charToNum(\"日\"))");
    /* Un code hors d'Unicode, et un demi-codet : ni l'un ni l'autre n'encode
     * quoi que ce soit, et il vaut mieux rendre vide qu'un octet invalide. */
    essai("put the number of chars of numToChar(1114112)");
    essai("put the number of chars of numToChar(55296)");

    puts("\n=== dans un champ, le chemin complet ===");
    essai("put \"été\" into card field \"f\"\n"
          "  put the number of chars of card field \"f\"");
    essai("put \"X\" into char 2 of card field \"f\"\n"
          "  put card field \"f\"");

    hc_free(st);
    return 0;
}
