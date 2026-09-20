/* LES ERREURS DE LA BOÎTE DE MESSAGE N'ARRIVAIENT QU'À LA CONSOLE.
 *
 * SIGNALE A L'USAGE : « put the zorglub of this card envoie un message dans
 * la console Xcode, mais pas pour l'user ».
 *
 * Le noyau accumule les lignes d'erreur pour les remettre a l'hote EN UNE
 * FOIS a la fin du gestionnaire le plus exterieur — une seule faute produit
 * le message, l'extrait du script et le resume, et les donner une par une
 * ouvrirait trois dialogues.
 *
 * Mais la condition d'accumulation etait « g_depth > 0 », et hc_do — la
 * boite de message, la commande « do », les articles de menu crees par
 * script — travaille a g_depth == 0. Rien ne s'accumulait, rien n'etait
 * remis, et le commentaire du code expliquait pourquoi :
 *
 *     « Hors gestionnaire, rien a accumuler : personne n'attend derriere,
 *       et l'appelant a deja eu la ligne. »
 *
 * « L'appelant a deja eu la ligne » voulait dire : elle est partie par le
 * rappel `line`, que l'hote Cocoa ecrit dans la console de Xcode. Ce qui
 * revient, pour qui se sert de l'application, a ne RIEN recevoir. Et c'est
 * justement la qu'on a le plus besoin du dialogue : on vient de taper une
 * ligne et on attend une reponse.
 *
 * LES QUATRE SORTES D'ERREUR ETAIENT CONCERNEES, pas seulement celle qui a
 * ete signalee : propriete inconnue, objet introuvable, verbe que personne ne
 * sert, et ligne que l'analyseur refuse.
 *
 * CE QUE CE HARNAIS TIENT SURTOUT, c'est le COMPTE des dialogues. Une
 * correction naive — vider a chaque emit — en ouvrirait un par ligne
 * d'erreur ; une autre — poser un booleen — en ouvrirait deux quand la boite
 * appelle un gestionnaire qui echoue, le gestionnaire vidant le sien en
 * redescendant. On compte donc, au lieu de regarder si « il y en a eu un ».
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static int g_dialogues;

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [console] %s\n", t ? t : ""); }

/* L'hote qui montre le dialogue. On retient le NOMBRE d'appels : c'est lui
 * que la correction devait preserver, pas seulement le fait qu'il y en ait. */
static void erreur(const char *texte, Object *objet)
{
    g_dialogues++;
    printf("      >>> DIALOGUE : %s\n", texte ? texte : "");
    printf("          objet fautif : %s\n", objet ? "oui" : "aucun");
}

static void essai(const char *titre, const char *l)
{
    printf("   %s\n", titre);
    g_dialogues = 0;
    hc_do(l);
    printf("      [dialogues ouverts : %d]\n", g_dialogues);
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne; h.erreur = erreur;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);
    hc_new_field(c, "Present");
    hc_set_script(c,
        "on fautif\n  put the zorglub of me\nend fautif\n"
        "on sain\n  put \"tout va bien\"\nend sain\n");

    puts("== 1. les quatre sortes d'erreur, depuis la boite ==");
    essai("propriete inconnue",  "put the zorglub of this card");
    essai("objet introuvable",   "put field \"Absent\"");
    essai("verbe que personne ne sert", "zorglub");
    essai("ligne refusee par l'analyseur", "repeat with i = 1 up to 5");

    puts("\n== 2. UNE LIGNE QUI REUSSIT N'OUVRE RIEN ==");
    essai("un calcul",            "put 2+2");
    essai("une lecture de champ", "put \"[\" & field \"Present\" & \"]\"");
    essai("un gestionnaire sain", "sain");

    puts("\n== 3. UN SEUL DIALOGUE, JAMAIS DEUX ==");
    /* Le gestionnaire vide le sien en redescendant a g_depth == 0 ; il ne
     * doit rien rester pour la boite. */
    essai("un gestionnaire fautif appele depuis la boite", "fautif");
    /* Deux fautes sur la meme ligne : un seul dialogue, la premiere faute
     * arretant l'evaluation. */
    essai("deux fautes sur une ligne",
          "put the zorglub of this card & the machin of this card");

    puts("\n== 4. et le gestionnaire ordinaire n'a pas bouge ==");
    {
        Object *b = hc_new_button(c, "B");
        hc_set_script(b, "on mouseUp\n  put the zorglub of me\nend mouseUp\n");
        printf("   un clic sur un bouton fautif\n");
        g_dialogues = 0;
        hc_send(b, "mouseUp");
        printf("      [dialogues ouverts : %d]\n", g_dialogues);
    }

    hc_free(st);
    return 0;
}
