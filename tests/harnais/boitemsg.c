/* La boîte de messages se LIT, et pas seulement s'écrit.
 *
 * MESURÉ AVANT, sous cinq formes — « put msg », « put the message box »,
 * « the length of msg », une relecture après un « put » sans destination, et
 * « put X after msg ». Les cinq rendaient « objet introuvable ».
 *
 * LA MOITIÉ QUI MANQUAIT. L'analyseur savait reconnaître la boîte : hct_expr.c
 * la reconnaît sous « msg », « the message box » et « the message window »
 * depuis toujours. L'exécuteur savait y écrire : ecrit_dans() la détourne
 * avant resout(), parce qu'elle n'est pas un objet de la pile. Mais rien ne
 * RETENAIT ce qu'on y mettait — le noyau émettait HC_MSG et n'en gardait
 * rien — et le rappel de lecture n'existait pas.
 *
 * C'est la troisième fois qu'on rencontre cette forme : lockErrorDialogs
 * était une serrure sans clé, errorDialog un message que personne n'envoyait,
 * et la boîte un conteneur en écriture seule. Le point commun est qu'un
 * script qui ne fait qu'AFFICHER marche parfaitement, et que seul celui qui
 * RELIT tombe. HyperTalk se sert pourtant de la boîte comme d'un bloc-notes.
 *
 * CE QUE CE HARNAIS TIENT :
 *   — la lecture sous ses formes, y compris en expression et en condition ;
 *   — la PERSISTANCE d'un gestionnaire au suivant ;
 *   — le `mode` — « after » et « before » — que le rappel d'écriture
 *     ignorait, et qui faisait afficher une seconde ligne au lieu d'ajouter ;
 *   — les MORCEAUX, qui ont besoin des deux sens à la fois ;
 *   — et le piège dans lequel je suis tombé en mesurant, qui n'est pas un
 *     défaut mais une propriété : « put item 2 of msg » ÉCRIT dans la boîte
 *     avant qu'on ait fini de la lire.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   [boîte] %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}
static void mon_erreur(const char *t, Object *o)
{ (void)o; printf("   [DIALOGUE] %s\n", t); }

static Object *g_pile, *g_carte;

static void fais(const char *corps)
{
    char s[4096];
    snprintf(s, sizeof s, "on essai\n  %s\nend essai\n", corps);
    hc_set_script(g_pile, s);
    hc_send(g_carte, "essai");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.erreur = mon_erreur;
    hc_set_host(&h);

    g_pile  = hc_new_stack("Boite");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);

    printf("=== 1. écrire puis relire ===\n");
    printf("   (c'est le cas signalé : « objet introuvable » avant)\n");
    fais("put \"bonjour\" into msg\n  put msg");

    printf("=== 2. les trois noms désignent la même boîte ===\n");
    fais("put \"un\" into msg\n  put the message box");
    fais("put \"deux\" into the message window\n  put msg");

    printf("=== 3. « put » sans destination écrit dans la boîte ===\n");
    printf("   (et c'est pour cela qu'on peut le relire)\n");
    fais("put \"direct\"\n  put msg");

    printf("=== 4. elle SURVIT d'un gestionnaire au suivant ===\n");
    printf("   (sans quoi elle ne serait pas un bloc-notes)\n");
    fais("put \"memoire\" into msg");
    fais("put msg");

    printf("=== 5. en expression, comme n'importe quel conteneur ===\n");
    fais("put \"abc\" into msg\n  put the length of msg into n\n  put n");
    fais("put \"oui\" into msg\n"
         "  if msg is \"oui\" then put \"la condition lit la boîte\"");
    fais("put \"jeton\" into msg\n  put msg into x\n  put x & \"!\"");

    printf("=== 6. after et before AJOUTENT, ils n'affichent plus une ligne ===\n");
    printf("   (le `mode` que le rappel d'écriture ignorait : la boîte\n");
    printf("    d'HyperCard est un champ unique, pas un journal)\n");
    fais("put \"a\" into msg\n  put \"b\" after msg\n  put msg");
    fais("put \"monde\" into msg\n  put \"bonjour \" before msg\n  put msg");

    printf("=== 7. les morceaux, qui demandent les deux sens à la fois ===\n");
    fais("put \"un,deux,trois\" into msg\n"
         "  put item 2 of msg into lu\n"
         "  put \"DEUX\" into item 2 of msg\n"
         "  put lu && msg");

    printf("=== 8. LE PIÈGE, et ce n'en est pas un défaut ===\n");
    printf("   (« put item 2 of msg » sans destination ÉCRIT dans la boîte :\n");
    printf("    la ligne suivante ne travaille donc plus sur « un,deux,trois »\n");
    printf("    mais sur « deux ». Je m'y suis pris les pieds en mesurant ;\n");
    printf("    HyperCard fait exactement pareil, et un script qui relit la\n");
    printf("    boîte doit en tenir compte)\n");
    fais("put \"un,deux,trois\" into msg\n"
         "  put item 2 of msg\n"
         "  put \"DEUX\" into item 2 of msg\n"
         "  put msg");

    printf("=== 9. CE QUI RESTE À FAIRE : la boîte n'a pas de propriétés ===\n");
    printf("   (« the visible of msg » reste « objet introuvable ». La\n");
    printf("    fenêtre appartient à l'hôte ; seul son CONTENU est au noyau.\n");
    printf("    On l'inscrit pour ne pas croire le chantier terminé)\n");
    fais("put the visible of msg");
    return 0;
}
