/* Une lettre accentuée dans un NOM : variable, paramètre, gestionnaire.
 *
 * MESURÉ AVANT LA CORRECTION : « on ayudar cómo » donnait deux
 * « caractère inattendu » par ligne — un sur le « ó » du nom du paramètre,
 * un sur celui de son usage — et le script entier était refusé pour un
 * accent. HyperCard nommait en Mac Roman et les piles d'époque s'en servent :
 * HypoGraph 0.91 passe son texte d'aide espagnol par ce paramètre.
 *
 * LA RACINE : isalpha() seul dans la branche des mots de hct_lex.c.
 *
 * CE QUE CE HARNAIS TIENT SURTOUT, c'est le PIÈGE de la correction. Accepter
 * tout octet ≥ 0x80 aurait été immédiat et faux : « ≠ » se serait collé au
 * mot qui le précède, et « if y≠"NAN(037)" » — une ligne de la MÊME pile —
 * serait devenue un seul identifiant « y≠ ». On vérifie donc dans les deux
 * sens : les lettres passent, les opérateurs restent des opérateurs, et les
 * symboles qui ne sont ni l'un ni l'autre restent refusés franchement.
 * Un silence y serait pire qu'une erreur.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *g_pile, *g_carte;
static void fais(const char *s)
{ hc_set_script(g_pile, s); hc_send(g_carte, "essai"); }

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    g_pile  = hc_new_stack("Accents");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);

    printf("=== 1. le gestionnaire accentué de HypoGraph, exécuté ===\n");
    fais("on ayudar cómo\n"
         "  put \"aide : \" & cómo\n"
         "end ayudar\n"
         "on essai\n"
         "  ayudar \"clique ici\"\n"
         "end essai\n");

    printf("=== 2. variables accentuées, et l'arithmétique par-dessus ===\n");
    fais("on essai\n"
         "  put 3 into é\n"
         "  put 4 into Ç\n"
         "  put \"é + Ç = \" & (é + Ç)\n"
         "  put \"français\" into prénom\n"
         "  put prénom\n"
         "  put \"Noël\" into fête\n"
         "  put fête & \" en \" & prénom\n"
         "end essai\n");

    printf("=== 3. une fonction au nom accentué, définie et appelée ===\n");
    fais("function côté n\n"
         "  return n * 4\n"
         "end côté\n"
         "on essai\n"
         "  put \"côté(3) = \" & côté(3)\n"
         "end essai\n");

    printf("=== 4. LE PIÈGE : « ≠ » ne se colle PAS au mot précédent ===\n");
    printf("   (la ligne exacte de la pile : if y≠\"NAN(037)\" then …)\n");
    fais("on essai\n"
         "  put \"INF\" into y\n"
         "  if y≠\"NAN(037)\" then put \"≠ reste un opérateur\"\n"
         "  if 2≤3 then put \"≤ aussi\"\n"
         "  if 4≥3 then put \"≥ aussi\"\n"
         "end essai\n");

    printf("=== 5. « × » et « ÷ » ne sont PAS des lettres ===\n");
    printf("   (ils vivent dans le MÊME bloc UTF-8 que À-ÿ ; les prendre pour\n");
    printf("    des lettres aurait fait de « a×3 » un seul nom, et le calcul\n");
    printf("    aurait rendu la chaîne « a×3 » sans rien dire)\n");
    fais("on essai\n"
         "  put 2 into a\n"
         "  put a×3\n"
         "end essai\n");

    printf("=== 6. les symboles à trois octets restent refusés franchement ===\n");
    printf("   (« ∞ » n'est pas un nom : on veut l'erreur, pas le silence)\n");
    fais("on essai\n"
         "  put 1 into ∞\n"
         "end essai\n");

    printf("=== 7. le témoin : un nom sans accent n'a pas bougé ===\n");
    fais("on essai\n"
         "  put 3 into cote\n"
         "  put cote * 4\n"
         "end essai\n");
    return 0;
}
