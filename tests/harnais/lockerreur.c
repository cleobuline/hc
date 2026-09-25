/* « the lockErrorDialogs » : l'erreur part en message, pas en dialogue.
 *
 * MESURÉ AVANT : « propriété inconnue : lockErrorDialogs », et le message
 * « errorDialog » n'était JAMAIS envoyé par quoi que ce soit.
 *
 * LA SERRURE ÉTAIT LÀ, SANS SA CLÉ. hct_verif.c liste « errordialog » parmi
 * les messages système légitimes depuis toujours — un « on errorDialog »
 * dans une pile ne produisait donc aucun avertissement, et avait l'air de
 * marcher. Mais rien dans le noyau ne l'envoyait, et la propriété qui
 * l'allume n'existait pas. Une moitié de mécanisme, invisible tant qu'aucune
 * pile ne s'en sert.
 *
 * HypoGraph 0.91 s'en sert aux deux bouts : « on errorDialog them / answer
 * them with "Cancel" » dans son script de pile, et « set the
 * lockErrorDialogs to true » en tête du gestionnaire de son bouton « draw ».
 * L'auteur traçait des milliers de points et ne voulait pas, à chacun, du
 * dialogue d'erreur de HyperCard — celui qui propose d'ouvrir l'éditeur de
 * script — mais du sien, avec un seul bouton.
 *
 * CE QUE CE HARNAIS TIENT, et surtout les deux pièges :
 *   — le détournement lui-même, et le TEXTE reçu en paramètre ;
 *   — la RÉCURSION : un gestionnaire errorDialog qui tombe lui-même en
 *     erreur s'enverrait à errorDialog, sans fin ;
 *   — le DÉVERROUILLAGE au repos : HypoGraph pose la serrure et ne la retire
 *     jamais. Sans remise à zéro, tout le reste de la session y passerait —
 *     exactement le défaut que lockScreen a déjà eu.
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
/* L'hôte : c'est LUI qui ouvrirait le dialogue. S'il parle, le détournement
 * n'a pas eu lieu. */
static void mon_erreur(const char *t, Object *o)
{ (void)o; printf("   [DIALOGUE HÔTE] %s\n", t); }

static Object *g_pile, *g_carte;

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.erreur = mon_erreur;
    hc_set_host(&h);

    g_pile  = hc_new_stack("Verrou");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);

    printf("=== 1. elle se LIT et elle s'ÉCRIT ===\n");
    printf("   (les deux sens : c'est le défaut de famille de ce noyau)\n");
    hc_set_script(g_pile,
        "on essai\n"
        "  put the lockErrorDialogs\n"
        "  set the lockErrorDialogs to true\n"
        "  put the lockErrorDialogs\n"
        "  set the lockErrorDialogs to false\n"
        "  put the lockErrorDialogs\n"
        "end essai\n");
    hc_send(g_carte, "essai");

    printf("=== 2. SANS la serrure : l'hôte ouvre son dialogue ===\n");
    hc_set_script(g_pile,
        "on errorDialog them\n"
        "  put \"MON gestionnaire a reçu : \" & them\n"
        "end errorDialog\n"
        "on essai\n"
        "  put the zorglub of this card\n"
        "end essai\n");
    hc_send(g_carte, "essai");

    printf("=== 3. AVEC la serrure : le gestionnaire prend l'erreur ===\n");
    printf("   (et l'hôte doit se taire — s'il parle, rien n'a été détourné)\n");
    hc_set_script(g_pile,
        "on errorDialog them\n"
        "  put \"MON gestionnaire a reçu : \" & them\n"
        "end errorDialog\n"
        "on essai\n"
        "  set the lockErrorDialogs to true\n"
        "  put the zorglub of this card\n"
        "end essai\n");
    hc_send(g_carte, "essai");

    printf("=== 4. la serrure s'ÉTEINT en retombant au repos ===\n");
    printf("   (HypoGraph la pose et ne la retire jamais ; sans cette remise\n");
    printf("    à zéro tout le reste de la session partirait en errorDialog,\n");
    printf("    exactement le défaut que lockScreen a déjà eu)\n");
    hc_set_script(g_pile, "on essai\n  put the lockErrorDialogs\nend essai\n");
    hc_send(g_carte, "essai");

    printf("=== 5. LE PIÈGE : errorDialog qui tombe lui-même en erreur ===\n");
    printf("   (sans garde-fou il s'enverrait à errorDialog, sans fin. On\n");
    printf("    repasse par le dialogue ordinaire pendant son exécution)\n");
    hc_set_script(g_pile,
        "on errorDialog them\n"
        "  put \"errorDialog a reçu : \" & them\n"
        "  put the patatras of this card\n"
        "end errorDialog\n"
        "on essai\n"
        "  set the lockErrorDialogs to true\n"
        "  put the zorglub of this card\n"
        "end essai\n");
    hc_send(g_carte, "essai");

    printf("=== 6. serrure posée, AUCUN gestionnaire : on ne remet pas le\n");
    printf("       dialogue, mais on ne se tait pas non plus ===\n");
    printf("   (la pile a demandé le silence ; le lui rendre contre son gré\n");
    printf("    viderait la propriété de son sens. Mais une pile qui pose la\n");
    printf("    serrure sans écrire le gestionnaire deviendrait aveugle, et\n");
    printf("    rien ne lui dirait pourquoi)\n");
    hc_set_script(g_pile,
        "on essai\n"
        "  set the lockErrorDialogs to true\n"
        "  put the zorglub of this card\n"
        "end essai\n");
    hc_send(g_carte, "essai");

    printf("=== 7. le cas de HypoGraph, bout à bout ===\n");
    printf("   (son errorDialog appelle « answer », qu'aucun hôte ne sert ici ;\n");
    printf("    on garde la forme et on remplace answer par put)\n");
    printf("\n");
    printf("   CE QUE CETTE SECTION AFFIRMAIT ÉTAIT FAUX, et c'est moi qui\n");
    printf("   l'avais écrit. Elle disait : « chez HyperCard une erreur ARRÊTE\n");
    printf("   le gestionnaire, la boucle de HC va au bout, c'est à corriger ».\n");
    printf("   Aucune mesure derrière — un raisonnement, et il était faux.\n");
    printf("\n");
    printf("   MESURÉ, sous Basilisk, avec une sonde qui ACCUMULE pour que la\n");
    printf("   boîte n'efface pas sa propre preuve :\n");
    printf("\n");
    printf("       on mouseUp\n");
    printf("         put \"1\"\n");
    printf("         set the zorglub of this card to 1\n");
    printf("         put msg & \"2\"\n");
    printf("       end mouseUp                         -> la boîte contient 12\n");
    printf("\n");
    printf("   HyperCard OUVRE le dialogue et CONTINUE quand même. Le « 2 »\n");
    printf("   est là. Une erreur de script n'interrompt donc pas.\n");
    printf("\n");
    printf("   La boucle de HC qui va au bout n'est pas un défaut : c'est le\n");
    printf("   comportement. Un chantier de cinquante sites — faire lever une\n");
    printf("   faute à chaque emit(HC_ERR) de commande — a été annulé par\n");
    printf("   cette mesure-là, avant d'avoir été commencé.\n");
    printf("\n");
    printf("   IL RESTE UNE DISSYMÉTRIE, et elle est réelle : dans le même\n");
    printf("   noyau, « put the zorglub » lève une faute et INTERROMPT — la\n");
    printf("   section 3 le montre — tandis que « drag » se contente d'un\n");
    printf("   emit(HC_ERR) et rend « traité ». C'est le PREMIER qui s'écarte\n");
    printf("   d'HyperCard, pas le second. Ce qu'on croyait être le sens de la\n");
    printf("   correction est exactement l'inverse.\n");
    printf("\n");
    printf("   CE QUI N'EST PAS MESURÉ, et qui reste donc ouvert : QUAND\n");
    printf("   errorDialog part. Ici il part à la sortie du gestionnaire le\n");
    printf("   plus extérieur. Puisque HyperCard continue, il devrait sans\n");
    printf("   doute partir AU POINT D'ERREUR — mais « sans doute » est\n");
    printf("   précisément le mot qui vient de me coûter un chantier, et on\n");
    printf("   ne touchera à rien sans l'avoir relevé.\n");
    printf("\n");
    printf("   LA FAUTE A CHANGÉ, PAS CE QU'ELLE MONTRE : la coordonnée non\n");
    printf("   finie de HypoGraph ne lève plus d'erreur du tout — mesuré,\n");
    printf("   HyperCard n'ouvre aucune alerte dans ce cas. On garde la forme\n");
    printf("   du gestionnaire et on prend l'autre faute que « drag » émet\n");
    printf("   sans arrêter : l'hôte de ce harnais ne gère pas la souris.\n");
    hc_set_script(g_pile,
        "on errorDialog them\n"
        "  put \"[boîte à un bouton] \" & them\n"
        "  choose browse tool\n"
        "end errorDialog\n"
        "on mouseUp\n"
        "  set the lockErrorDialogs to true\n"
        "  put 0 into cy\n"
        "  repeat with i = 1 to 3\n"
        "    put \"point \" & i\n"
        "    if i is 2 then drag from 10,20 to 30,40\n"
        "  end repeat\n"
        "  put \"la boucle est allée au bout\"\n"
        "end mouseUp\n");
    hc_send(g_carte, "mouseUp");

    printf("=== 8. le témoin : une coquille voisine reste refusée ===\n");
    hc_set_script(g_pile,
        "on essai\n  set the lockErrorDialog to true\nend essai\n");
    hc_send(g_carte, "essai");
    return 0;
}
