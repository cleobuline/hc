/* « OF X » OU X N'EXISTE PAS N'EST PAS « SANS OF ».
 *
 * LE DEFAUT LE PLUS GRAVE RENCONTRE DANS CE DEPOT, et il tient en une
 * confusion de deux NULL. v3_cible() repondait NULL a deux questions
 * differentes :
 *
 *     y a-t-il un « of X » ?          -> NULL si non
 *     ce « of X » designe-t-il quoi ? -> NULL si l'objet n'existe pas
 *
 * L'appelant, ne pouvant pas les distinguer, continuait dans les DEUX cas
 * avec g_current_card. Mesure, avec une carte « Une » portant un champ « X »
 * valant BONJOUR, et aucune carte « Absente » :
 *
 *     put field "X" of card "Absente"            -> BONJOUR
 *     put "OUPS" into field "X" of card "Absente"
 *                     -> ecrit OUPS dans le champ de la carte COURANTE
 *     put there is a field "X" of card "Absente" -> true
 *     put the name of card 1 of stack "PileAbsente" -> card "Une"
 *
 * La lecture ment. L'ECRITURE, elle, modifie des donnees dans un objet que
 * le script n'a jamais nomme, et parait avoir reussi. Un script qui range
 * des donnees dans une carte disparue les ecrase silencieusement ailleurs.
 *
 * ET LE JUMEAU, QUI A MORDU DANS LA MEME HEURE. La v3 corrigee, la LECTURE
 * mentait toujours : l'echec de la v3 passe le relais au recours, qui tombe
 * dans le resolve() de l'ancien moteur, dont la derniere ligne etait
 *
 *     return r ? r : resolve_local(ref);
 *
 * et resolve_local IGNORE le « of X » — elle lit la tete et la cherche la ou
 * l'on se trouve. Deux portes, une seule reparee, rien de repare. C'est le
 * motif qu'on traque depuis des semaines, et il s'est presente le jour meme
 * ou on le nommait.
 *
 * DEUX CRANS, VERIFIES SEPAREMENT :
 *
 *   - la PORTEE n'existe pas (card "Absente") ;
 *   - la portee existe mais l'objet n'y est pas (field "X" of card "Deux",
 *     ou « X » n'est que sur la carte Une). Celui-la est plus fin et tout
 *     aussi trompeur : le script nomme une carte et obtient le contenu
 *     d'une autre.
 *
 * CE QUE CE HARNAIS TIENT SURTOUT, c'est que les portees LEGITIMES marchent
 * encore — c'est la moitie qui coute, puisqu'un refus general passerait le
 * reste du test. Deux piles ouvertes, deux niveaux de portee, la lecture et
 * l'ecriture.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

static void execute(const char *corps)
{
    char s[2048];
    printf("   %s\n", corps);
    snprintf(s, sizeof s, "on essaie\n%s\nend essaie\n", corps);
    hc_set_script(b, s);
    hc_send(b, "essaie");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c1 = hc_new_card(st, bg, "Une");
    Object *c2 = hc_new_card(st, bg, "Deux");
    hc_set_current_card(c1);
    hc_register_stack(st);
    hc_new_field(bg, "Partage");
    Object *f1 = hc_new_field(c1, "X"); hc_set_field_text(f1, "SUR UNE");
    Object *f2 = hc_new_field(c2, "Y"); hc_set_field_text(f2, "SUR DEUX");
    (void)f2;
    b = hc_new_button(c1, "B");

    /* Une SECONDE pile ouverte : sans elle, « of stack "Autre" » ne
     * prouverait rien — tout echouerait pour la mauvaise raison. */
    Object *st2 = hc_new_stack("Autre");
    Object *bg2 = hc_new_background(st2, "F2");
    Object *d1  = hc_new_card(st2, bg2, "Alpha");
    Object *f3  = hc_new_field(d1, "Z"); hc_set_field_text(f3, "AILLEURS");
    hc_register_stack(st2);

    puts("== 1. la PORTEE n'existe pas : refus, pas de repli ==");
    execute("  put field \"X\" of card \"Absente\"");
    execute("  put there is a field \"X\" of card \"Absente\"");
    execute("  put the name of card 1 of stack \"PileAbsente\"");
    execute("  put bg field \"Partage\" of card \"Absente\"");

    puts("\n== 2. et surtout : L'ECRITURE ne va nulle part ==");
    execute("  put \"OUPS\" into field \"X\" of card \"Absente\"");
    execute("  put \"le champ de la carte courante : \" & field \"X\"");

    puts("\n== 3. la portee existe, l'objet n'y est pas ==");
    /* « X » n'est que sur la carte Une ; on le demande sur la carte Deux. */
    execute("  put field \"X\" of card \"Deux\"");
    execute("  put \"OUPS\" into field \"X\" of card \"Deux\"");
    execute("  put \"le champ de la carte courante : \" & field \"X\"");

    puts("\n== 4. LES PORTEES LEGITIMES MARCHENT TOUJOURS ==");
    execute("  put field \"Y\" of card \"Deux\"");
    execute("  put field \"Y\" of card 2");
    execute("  put field \"X\" of this card");
    execute("  put the name of card 2 of stack \"Pile\"");
    execute("  put the short name of bg field \"Partage\" of card \"Deux\"");
    /* Deux niveaux de portee, et une autre pile. */
    execute("  put field \"Z\" of card \"Alpha\" of stack \"Autre\"");
    execute("  put the number of cards of stack \"Autre\"");

    puts("\n== 5. et l'ecriture dans une portee legitime aboutit ==");
    execute("  put \"ECRIT AILLEURS\" into field \"Y\" of card \"Deux\"\n"
            "  put field \"Y\" of card \"Deux\"");
    execute("  put \"le champ X de la carte courante n'a pas bouge : \" & field \"X\"");

    hc_free(st2);
    hc_free(st);
    return 0;
}
