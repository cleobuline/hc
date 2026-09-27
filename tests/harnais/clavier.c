/* LES MESSAGES DU CLAVIER, PAR LEURS DEUX PORTES.
 *
 * Ils entrent dans le noyau de deux façons, et jusqu'ici UNE SEULE marchait :
 *
 *   1. l'INTERFACE les envoie quand on tape. HCview.m nomme la touche et
 *      appelle hc_send_arg. C'est la §1 ci-dessous, et elle tenait déjà.
 *
 *   2. un SCRIPT les écrit comme des commandes — « arrowKey "right" » pour
 *      naviguer, « tabKey » pour passer au champ suivant. C'est la §2, et
 *      AUCUN des dix ne marchait, même avec le gestionnaire sous la main :
 *
 *          on arrowKey d        -- dans le script de pile
 *            ...
 *          end arrowKey
 *
 *          arrowKey "right"     -- dans un bouton : « ne sait pas faire »
 *
 * LE DÉFAUT TIENT EN UNE PHRASE : être dans la table des commandes empêche
 * d'être un message. hct_cmd.c leur donne un motif — c'est ce qui fait que
 * « arrowKey "left" » s'analyse —, donc le nœud est une COMMANDE ; le
 * répartiteur cherche le verbe parmi les verbes portés, ne le trouve pas, et
 * la branche qui aurait proposé le nom à la pile ne s'applique qu'aux nœuds
 * MESSAGE. Les dix tombaient entre les deux.
 *
 * POURQUOI ÇA N'AVAIT PAS ÉTÉ VU : le relevé du corpus les comptait UNE fois
 * chacun, tout en bas du tableau, et je les avais rangés en queue de liste
 * d'après ce chiffre. Or le relevé compte le corpus de TEST, et un harnais
 * n'appuie jamais sur une touche. Une pile réelle ne fait que ça. Un compteur
 * mesure ce qu'on lui montre, pas ce qui compte.
 *
 * CE QUI RESTE À MESURER, et qui est donc REFUSÉ plutôt qu'inventé :
 * l'ACTION PAR DÉFAUT, celle d'une touche que nul n'intercepte. Une flèche
 * change-t-elle de carte, et dans quel sens ? tabKey choisit-il le champ
 * suivant ? Sans gestionnaire, les dix rendent toujours « ne sait pas faire »,
 * exactement comme avant — voir docs/mesures/clavier.txt. Une action inventée
 * serait pire qu'un refus franc : elle se ferait passer pour de la fidélité.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if      (k == HC_MSG) printf("   [msg] %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static const char *GESTIONNAIRES =
    "on arrowKey d\n  put \"flèche : \" & d\n"
    "  if d is \"right\" then pass arrowKey\nend arrowKey\n"
    "on keyDown k\n  put \"touche : \" & k\nend keyDown\n"
    "on functionKey n\n  put \"F\" & n\nend functionKey\n"
    "on tabKey\n  put \"tab\"\nend tabKey\n"
    "on controlKey n\n  put \"ctrl \" & n\nend controlKey\n"
    "on commandKeyDown c\n  put \"cmd \" & c\nend commandKeyDown\n"
    "on enterKey\n  put \"entrée\"\nend enterKey\n"
    "on enterInField\n  put \"entrée dans le champ\"\nend enterInField\n"
    "on returnInField\n  put \"retour dans le champ\"\nend returnInField\n";

static Object *b;
static void depuis_script(const char *ligne)
{
    char s[256];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", ligne);
    hc_set_script(b, s);
    printf("   %-24s ", ligne);
    fflush(stdout);
    printf("\n");
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_new_card(st, bg, "Deux");
    hc_set_current_card(c);
    b = hc_new_button(c, "B");
    hc_set_script(st, GESTIONNAIRES);

    puts("== 1. L'INTERFACE ENVOIE : hc_send_arg, comme quand on tape ==");
    printf("arrowKey left   pris=%d\n", hc_send_arg(hc_current_card(), "arrowKey", "left"));
    printf("arrowKey right  pris=%d  (le gestionnaire fait « pass »)\n",
           hc_send_arg(hc_current_card(), "arrowKey", "right"));
    printf("keyDown a       pris=%d\n", hc_send_arg(hc_current_card(), "keyDown", "a"));
    printf("functionKey 3   pris=%d\n", hc_send_arg(hc_current_card(), "functionKey", "3"));
    printf("tabKey          pris=%d\n", hc_send_arg(hc_current_card(), "tabKey", NULL));
    printf("returnKey       pris=%d  (aucun gestionnaire)\n",
           hc_send_arg(hc_current_card(), "returnKey", NULL));

    puts("\n== 2. UN SCRIPT LES ÉCRIT : les dix atteignent le gestionnaire ==");
    depuis_script("arrowKey \"left\"");
    depuis_script("keyDown \"x\"");
    depuis_script("controlKey 1");
    depuis_script("functionKey 3");
    depuis_script("commandKeyDown \"b\"");
    depuis_script("enterKey");
    depuis_script("tabKey");
    depuis_script("enterInField");
    depuis_script("returnInField");

    puts("\n== 3. « pass » vaut DEPUIS UN SCRIPT AUSSI ==");
    /* Le gestionnaire voit la flèche, puis passe. Personne d'autre ne la
     * prend, et la commande se termine EN SILENCE.
     *
     * LA §4, elle, REFUSE — « ne sait pas faire ». Deux réponses pour la même
     * situation : dans les deux cas personne n'a pris la touche. La
     * différence vient du « pass », qui remonte et court-circuite le vieux
     * chemin avant qu'il ne se plaigne.
     *
     * C'est inscrit et non maquillé, parce que la vraie réponse n'est ni le
     * silence ni le refus : c'est l'ACTION PAR DÉFAUT, qui n'est pas mesurée.
     * Le jour où elle le sera, les deux cas la feront et l'écart disparaîtra
     * de lui-même. Aligner les deux aujourd'hui reviendrait à choisir entre
     * deux mauvaises réponses et à faire croire la question réglée. */
    depuis_script("arrowKey \"right\"");

    puts("\n== 4. SANS GESTIONNAIRE : le refus, faute d'action par défaut ==");
    /* Ce que HC ne sait pas faire est DIT. Ces lignes doivent changer le jour
     * où l'action par défaut sera mesurée — et c'est bien le but qu'elles
     * bougent alors, plutôt que de rester vraies en silence. */
    hc_set_script(st, "");
    depuis_script("arrowKey \"left\"");
    depuis_script("tabKey");
    depuis_script("returnKey");

    /* ── 5. SUPPRIMER UN RACCOURCI DE MENU, TOUCHE PAR TOUCHE ──────────────
     *
     * C'est l'usage que l'utilisatrice a trouvé au message dès qu'il a marché,
     * et il vaut mieux que l'avertissement que j'avais écrit à côté : je
     * décrivais une pile qui prend TOUT comme un piège — Cmd+Q compris —, sans
     * voir qu'une pile qui prend CE QU'ELLE VEUT est la raison d'être du
     * message. Le piège et la fonction sont le même mécanisme vu de deux
     * côtés ; il n'y avait qu'un côté dans le commentaire.
     *
     * TOUT REPOSE SUR LA VALEUR RENDUE, et elle est mesurée ici :
     *
     *     exit commandKeyDown  -> pris  -> l'interface rend YES, le menu se tait
     *     pass commandKeyDown  -> passe -> l'interface rend NO,  le menu agit
     *
     * Un gestionnaire qui finit normalement compte pour une prise, comme
     * « exit ». La sélectivité vient donc du « pass » : on ne passe que ce
     * qu'on veut laisser au menu. */
    puts("\n== 5. SUPPRIMER UN RACCOURCI DE MENU, touche par touche ==");
    hc_set_script(st,
        "on commandKeyDown k\n"
        "  if k is \"n\" then\n"
        "    put \"Cmd+N : supprime par la pile\"\n"
        "    exit commandKeyDown\n"
        "  end if\n"
        "  if k is \"z\" then\n"
        "    put \"Cmd+Z : traite par la pile\"\n"
        "  else\n"
        "    pass commandKeyDown\n"
        "  end if\n"
        "end commandKeyDown\n");
    {
        static const char *touches[] = { "n", "z", "q", NULL };
        for (int i = 0; touches[i]; i++) {
            int pris = hc_send_arg(hc_current_card(), "commandKeyDown", touches[i]);
            printf("Cmd+%s  pris=%d  -> le menu %s\n",
                   touches[i], pris, pris ? "NE FAIT RIEN" : "agit");
        }
        puts("   (Cmd+Q doit rester a 0 : une pile ordinaire ne doit pas");
        puts("    empecher de quitter sans l'avoir voulu)");
    }

    hc_free(st);
    return 0;
}
