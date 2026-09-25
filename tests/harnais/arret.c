/* DEUX FAÇONS DE SIGNALER DANS LE MÊME NOYAU, DONT UNE SEULE ARRÊTE.
 *
 * MESURÉ : 83 emit(HC_ERR) dans hc_core.c, contre 13 hct_ctx_faute. Les
 * premiers écrivent la ligne d'erreur et rendent « traité » : le gestionnaire
 * continue. Les seconds posent ctx->erreur, que l'exécuteur teste à vingt
 * endroits, et le gestionnaire s'arrête.
 *
 * LE SYMPTÔME OBSERVÉ. HypoGraph pose « set the lockErrorDialogs to true »
 * puis trace mille points. Une erreur en cours de boucle devrait, chez
 * HyperCard, arrêter le gestionnaire et envoyer errorDialog AUSSITÔT. Chez
 * nous la boucle allait au bout et errorDialog ne partait qu'à la sortie du
 * gestionnaire le plus extérieur. La différence ne vient pas du verrou, elle
 * vient d'ici.
 *
 * CE HARNAIS N'EST PAS UNE CORRECTION, C'EST UN ÉTAT DES LIEUX. Il relève,
 * cas par cas, si « après » s'affiche — donc si le gestionnaire a continué.
 * Il servira de preuve avant/après quand on tranchera site par site.
 *
 * LES TROIS GENRES, qu'il faut séparer avant de toucher à quoi que ce soit :
 *
 *   1. LES VRAIES ERREURS HYPERTALK — propriété inconnue, morceau hors
 *      limites, fichier non ouvert. Je les croyais bloquantes chez
 *      HyperCard, et c'est POUR ELLES que le chantier existait.
 *
 *      MESURÉ, et l'inverse : HyperCard ouvre son dialogue et CONTINUE.
 *      La sonde accumule, pour que la boîte n'efface pas sa preuve —
 *      « put "1" / set the zorglub of this card to 1 / put msg & "2" »
 *      laisse « 12 » dans la boîte. Le « 2 » est là.
 *
 *      Ces cinquante sites sont donc FIDÈLES TELS QUELS, et ce harnais,
 *      écrit pour préparer leur correction, sert maintenant à prouver
 *      qu'il ne faut pas les corriger.
 *
 *   2. « L'HÔTE NE SAIT PAS FAIRE » — sept sites qui ne disent rien sur
 *      HyperTalk et tout sur le HÔTE COURANT : pas de souris, pas
 *      d'imprimante, pas de menus. Sous Cocoa ils ne se déclenchent jamais.
 *      Les transformer en fautes ferait planter des scripts parfaitement
 *      valides dans des harnais qui n'ont pas d'écran. Ils doivent RESTER
 *      non bloquants.
 *
 *   3. CEUX QUI POSENT « THE RESULT » — delete, go, save. « the result » EST
 *      la convention d'HyperTalk pour ce qui n'est pas une erreur : le script
 *      teste « if the result is not empty ». Les faire arrêter contredirait
 *      la convention, et leur emit(HC_ERR) est déjà douteux — il ouvre un
 *      dialogue pour quelque chose que le langage veut silencieux.
 *
 * Les trois genres se ressemblent dans le code : un emit(HC_ERR) et un
 * return 1. C'est justement pourquoi personne ne les avait séparés.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
    /* Le moniteur compte ici : c'est LUI qui montre les deux silences de la
     * §6 — une commande qui annonce « → supprimé » sans rien supprimer ne
     * dit rien sur HC_ERR, et sans cette ligne le harnais ne verrait pas
     * la réussite en trompe-l'œil. */
    else if (k == HC_INFO) printf("   [moniteur] %s\n", t);
}
static void mon_erreur(const char *t, Object *o) { (void)t; (void)o; }

static Object *g_pile, *g_carte;

/* Encadrer la ligne à l'essai : si « après » sort, le gestionnaire a
 * continué. C'est tout ce que ce harnais mesure. */
static void essaie(const char *titre, const char *ligne)
{
    char s[2048];
    printf("   — %s\n", titre);
    printf("     %s\n", ligne);
    snprintf(s, sizeof s,
             "on essai\n  put \"avant\"\n  %s\n  put \"APRÈS\"\nend essai\n",
             ligne);
    hc_set_script(g_pile, s);
    hc_send(g_carte, "essai");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.erreur = mon_erreur;
    hc_set_host(&h);

    g_pile  = hc_new_stack("Arret");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);

    printf("=== 1. LA FAUTE, QUI ARRÊTE — ET QUI S'ÉCARTE D'HYPERCARD ===\n");
    printf("   (hct_ctx_faute. « APRÈS » ne sort pas. Je l'avais mise ici\n");
    printf("    comme TÉMOIN du bon comportement ; la mesure en fait le seul\n");
    printf("    cas douteux de la page, puisque HyperCard, lui, continue)\n");
    essaie("lecture d'une propriété inconnue",
           "put the zorglub of this card");

    printf("=== 2. GENRE 1 : vraies erreurs HyperTalk ===\n");
    printf("   (« APRÈS » sort, et il DOIT sortir : mesuré sous Basilisk,\n");
    printf("    HyperCard ouvre son dialogue et continue. Ces lignes-là\n");
    printf("    étaient la cible du chantier ; elles en sont la réfutation)\n");
    essaie("écriture d'une propriété inconnue",
           "set the zorglub of this card to 1");
    essaie("convert sans « to »",       "convert \"hier\"");
    essaie("date incomprise",           "convert \"pas une date\" to seconds");
    essaie("read d'un fichier non ouvert",
           "read from file \"nexistepas\" for 4");
    essaie("write vers un fichier non ouvert",
           "write \"x\" to file \"nexistepas\"");
    essaie("reset mal formé",           "reset zorglub");
    essaie("show … at mal formé",       "show card at zorglub");

    printf("=== 3. GENRE 2 : « l'hôte ne sait pas faire » ===\n");
    printf("   (ils ne disent rien sur HyperTalk et tout sur CET hôte :\n");
    printf("    sous Cocoa ils ne se déclenchent jamais. Ils doivent\n");
    printf("    RESTER non bloquants, et « APRÈS » doit continuer de sortir)\n");
    essaie("pas de souris",             "drag from 1,2 to 3,4");
    essaie("pas d'outils",              "choose line tool");
    essaie("pas de clavier",            "type \"a\"");
    essaie("pas d'imprimante",          "print this card");

    printf("=== 4. GENRE 3 : ceux qui posent « the result » ===\n");
    printf("   (la convention d'HyperTalk pour ce qui n'est PAS une erreur.\n");
    printf("    Les faire arrêter la contredirait — et leur emit(HC_ERR)\n");
    printf("    est déjà douteux, puisqu'il ouvre un dialogue pour ce que\n");
    printf("    le langage veut silencieux)\n");
    essaie("rien à supprimer",          "delete zorglub");
    essaie("pile introuvable",          "go to stack \"nexistepas\"");

    printf("=== 5. ET CE QUE « THE RESULT » DIT DANS CES CAS-LÀ ===\n");
    printf("   (s'il est posé, le script a le moyen de se garder lui-même,\n");
    printf("    et c'est exactement pour cela qu'il ne faut pas arrêter)\n");
    hc_set_script(g_pile,
        "on essai\n"
        "  delete zorglub\n"
        "  put \"result après delete : [\" & the result & \"]\"\n"
        "  go to stack \"nexistepas\"\n"
        "  put \"result après go : [\" & the result & \"]\"\n"
        "end essai\n");
    hc_send(g_carte, "essai");

    printf("=== 6. DEUX SILENCES TROUVÉS EN INVENTORIANT ===\n");
    printf("   (je ne les cherchais pas. Ils ne sont pas du chantier, mais\n");
    printf("    ils en sont la forme extrême : au lieu de signaler sans\n");
    printf("    arrêter, ceux-là ne signalent RIEN et annoncent une réussite)\n");
    printf("\n");
    printf("   6a. « delete <n'importe quel mot> » RÉUSSIT toujours.\n");
    printf("   container_set(d, \"\", 3) rend vrai pour un nom inconnu — il\n");
    printf("   crée puis vide une variable de ce nom. La branche « rien à\n");
    printf("   supprimer » est donc INATTEIGNABLE pour un mot seul, et le\n");
    printf("   script reçoit « → supprimé » et un result vide.\n");
    hc_set_script(g_pile,
        "on essai\n"
        "  delete zorglub\n"
        "  put \"result : [\" & the result & \"]\"\n"
        "end essai\n");
    hc_send(g_carte, "essai");
    printf("\n");
    printf("   6b. « show card at <pas une coordonnée> » l'accepte.\n");
    printf("   Le emit(HC_ERR) « point mal formé » existe et n'est pas\n");
    printf("   atteint : la position n'est pas vérifiée du tout. C'est la\n");
    printf("   MÊME famille que la coordonnée non finie de « click at » —\n");
    printf("   une valeur illisible qui passe pour une position valide.\n");
    hc_set_script(g_pile,
        "on essai\n  show card at zorglub\nend essai\n");
    hc_send(g_carte, "essai");
    printf("\n");
    printf("   CE QUE JE NE SAIS PAS : ce que fait HyperCard dans ces deux\n");
    printf("   cas. Ce n'est pas mesuré, donc ce n'est pas décidé. On les\n");
    printf("   inscrit pour ne pas les reperdre, et rien de plus.\n");
    return 0;
}
