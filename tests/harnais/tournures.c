/* tournures — « does not contain » et « replace … with … in », DEUX
 * TOLÉRANCES DE HC.
 *
 * Ni l'une ni l'autre n'est d'HyperCard : mesuré le 26 octobre DANS
 * HYPERCARD (Basilisk II), qui répond « Can't understand this keyword » et
 * « Can't understand arguments of replace » (docs/mesures/mots_absents.txt).
 * Elles viennent de LiveCode et de SuperCard, et HC les accepte à la demande
 * de l'utilisatrice, comme « chars » — HC accepte plus que l'original, sans
 * qu'aucune pile d'HyperCard change de comportement.
 *
 * CE QUE CE HARNAIS TIENT :
 *   1. « does not contain » est l'exact contraire de « contains » : casse
 *      ignorée, chaîne vide, priorité, morceaux, champs, boucles ;
 *   2. « replace » remplace toutes les occurrences, de gauche à droite et
 *      sans recouvrement, la casse ignorée, dans tout ce que « put … into »
 *      sait écrire ; il ne boucle pas quand le neuf contient le vieux ; il
 *      vide the result ; il refuse ce qui ne s'écrit pas ;
 *   3. LES VOISINS N'ONT PAS BOUGÉ : une pile qui définit SON « on replace »
 *      ou SA « function replace » les garde, et « replace » ou « does »
 *      restent des noms de variable. C'est la promesse faite à toute pile
 *      d'HyperCard, et c'est elle qui compte le plus ici. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("      -> %s\n", t ? t : "");
    else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : "");
}

static Object *g_carte;

static void essai(const char *ligne)
{
    printf("   %s\n", ligne);
    hc_do(ligne);
}

static const char *SCRIPT =
    "on replace a, b, c\n"
    "  put \"le gestionnaire de la pile :\" && a & \"|\" & b & \"|\" & c\n"
    "end replace\n"
    "\n"
    "function remplace s, a, b\n"
    "  replace a with b in s\n"
    "  return s\n"
    "end remplace\n"
    "\n"
    "on epluche\n"
    "  put \"abracadabra\" into s\n"
    "  put 0 into n\n"
    "  repeat until s does not contain \"a\"\n"
    "    delete char (offset(\"a\", s)) of s\n"
    "    add 1 to n\n"
    "  end repeat\n"
    "  put s && n\n"
    "end epluche\n";

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    Object *pile = hc_new_stack("Tournures");
    hc_register_stack(pile);
    Object *fond = hc_new_background(pile, "Fond");
    g_carte = hc_new_card(pile, fond, "Une");
    hc_set_current_card(g_carte);
    Object *f = hc_new_field(g_carte, "f");
    f->x = 10; f->y = 10; f->w = 200; f->h = 100;
    hc_set_script(pile, SCRIPT);

    puts("=== 1. « does not contain », le contraire exact de « contains » ===");
    essai("put \"abc\" into x");
    essai("if x does not contain \"z\" then put \"oui\" else put \"non\"");
    essai("if x does not contain \"b\" then put \"oui\" else put \"non\"");
    essai("put \"ABC\" does not contain \"b\"");
    essai("put \"abc\" does not contain empty");
    essai("put \"abc\" contains empty");
    essai("put \"abc\" does not contain \"d\" and 1 is 1");
    essai("put not (\"abc\" contains \"d\") and 1 is 1");
    essai("put word 2 of \"un deux trois\" does not contain \"eu\"");
    essai("put \"hello\" into card field \"f\"");
    essai("put card field \"f\" does not contain \"ELL\"");
    essai("put card field \"f\" does not contain \"xyz\"");
    puts("   (dans une boucle, ôter les « a » un à un :)");
    hc_send(g_carte, "epluche");

    puts("\n=== 2. « replace … with … in » ===");
    essai("put \"banana\" into x");
    essai("replace \"an\" with \"AN\" in x");
    essai("put x");
    puts("   (la casse ignorée, comme dans « contains » :)");
    essai("put \"BaNaNa\" into y");
    essai("replace \"A\" with \"o\" in y");
    essai("put y");
    puts("   (de gauche à droite, sans recouvrement :)");
    essai("put \"aaa\" into z");
    essai("replace \"aa\" with \"b\" in z");
    essai("put z");
    puts("   (le neuf contient le vieux : un seul passage, pas de boucle :)");
    essai("put \"aba\" into w");
    essai("replace \"a\" with \"aa\" in w");
    essai("put w");
    puts("   (un vieux vide ne remplace rien ; un neuf vide efface :)");
    essai("replace empty with \"x\" in w");
    essai("put w");
    essai("replace \"a\" with empty in w");
    essai("put w");
    puts("   (des expressions de part et d'autre :)");
    essai("put \"banana\" into x");
    essai("replace \"a\" & \"n\" with \"-\" & \"-\" in x");
    essai("put x");
    puts("   (dans un champ :)");
    essai("put \"Il pleut, il mouille\" into card field \"f\"");
    essai("replace \"il\" with \"elle\" in card field \"f\"");
    essai("put card field \"f\"");
    puts("   (« mouille » contient « il » : on remplace des chaînes, pas des mots)");
    puts("   (dans un morceau, et seulement là :)");
    essai("put \"a,b,a\" & return & \"a,b,a\" into t");
    essai("replace \"a\" with \"z\" in line 2 of t");
    essai("put t");
    essai("put \"un,deux,trois\" into card field \"f\"");
    essai("replace \"e\" with \"E\" in item 2 of card field \"f\"");
    essai("put card field \"f\"");
    puts("   (une variable devant « in » : « in » reste à la commande ;");
    puts("    « of » garde son sens dans le neuf :)");
    essai("put \"Z\" into neuf");
    essai("put \"abc\" into s");
    essai("replace \"b\" with neuf in s");
    essai("replace \"c\" with the short name of this card in s");
    essai("put s");
    essai("replace \"Z\" with (neuf & neuf) in s");
    essai("put s");
    puts("   (une variable jamais affectée est vide, pas son propre nom :)");
    essai("replace \"j\" with \"J\" in jamais");
    essai("put \"[\" & jamais & \"]\"");
    puts("   (the result est vidé — un find raté l'a rempli juste avant :)");
    essai("put \"vieux\" into x");
    essai("find \"qqqq\"");
    essai("put \"[\" & the result & \"]\"");
    essai("replace \"v\" with \"n\" in x");
    essai("put \"[\" & the result & \"]\" && x");
    puts("   (ce qui ne s'écrit pas est refusé :)");
    essai("replace \"U\" with \"V\" in the name of this card");
    essai("put the name of this card");
    puts("   (dans une fonction de la pile :)");
    essai("put remplace(\"Mississippi\", \"ss\", \"s\")");

    puts("\n=== 3. les voisins n'ont pas bougé ===");
    puts("   (le « on replace » d'une pile, appelé avec des virgules :)");
    essai("replace \"un\", \"deux\", \"trois\"");
    puts("   (« replace » et « does », noms de variable :)");
    essai("put 3 into replace");
    essai("put replace + 1");
    essai("put 5 into does");
    essai("put does * 2");
    puts("   (contains, is in, is not in :)");
    essai("put \"abc\" contains \"B\"");
    essai("put \"b\" is in \"abc\"");
    essai("put \"z\" is not in \"abc\"");

    hc_free(pile);
    return 0;
}
