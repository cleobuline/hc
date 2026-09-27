/* LIRE UNE PROPRIÉTÉ SUR UN MORCEAU QUI N'EXISTE PAS.
 *
 * DEUX CAS, DEUX RÉPONSES, et HC donnait la même mauvaise aux deux. Mesuré
 * dans HyperCard le 27/09/2026, sur un champ contenant « Sun Mon Tue » :
 *
 *     the textStyle of word 99 of card field "T"      le CHAMP existe
 *         HyperCard -> plain
 *         HC avant  -> textStyle of word 99 of card field "T"
 *
 *     the textStyle of word 2 of card field "Absent"  le champ MANQUE
 *         HyperCard -> une ERREUR, et le script S'ARRÊTE
 *                      (le dialogue Debug / Script / Cancel)
 *         HC avant  -> textStyle of word 2 of card field "Absent",
 *                      et le gestionnaire CONTINUE
 *
 * HC rendait sa propre phrase dans les deux cas. Un « if the textStyle of
 * word 99 of f is "bold" » comparait donc à une phrase, et la comparaison
 * était fausse sans jamais se plaindre ; et un nom de champ mal orthographié
 * laissait le gestionnaire travailler sur cette phrase pendant tout le reste
 * de son cours au lieu de s'arrêter net.
 *
 * LE POINT EXACT DU DÉFAUT : lire un MORCEAU d'un champ absent s'arrêtait
 * déjà correctement — « put word 2 of card field "Absent" » lève « objet
 * introuvable » et stoppe. Seule la propriété DU morceau avalait l'erreur,
 * parce que v3_recours annonçait un SUCCÈS là où il n'avait fait que se
 * replier sur le littéral nu. La règle « un mot nu peut valoir lui-même, une
 * référence d'objet jamais, un appel de fonction jamais » avait un troisième
 * cas manquant : une propriété de texte sur un morceau. C'est
 * v3_prop_sur_morceau, jumeau de v3_prop_sur_objet, qui le ferme.
 *
 * Et l'autre moitié est ailleurs, dans term_value_body : quand le champ EXISTE
 * et que seul le morceau manque, la propriété se lit sur une plage VIDE placée
 * à la FIN du texte. À la fin et non au début : un champ dont le premier mot
 * est en gras rendrait « bold » pour un mot qui n'existe pas. Les deux moitiés
 * se composent sans se connaître — la lecture rendant « plain », il n'y a plus
 * d'écho, donc plus rien à refuser.
 *
 * J'AI CRU HC FIDÈLE PENDANT UN COMMIT, ET IL FAUT DIRE POURQUOI. Un premier
 * relevé m'était arrivé sans être étiqueté ; j'y ai lu HyperCard alors que
 * c'était HC, j'ai retiré l'accusation et écrit ce harnais pour VERROUILLER la
 * phrase. La leçon n'est pas « vérifier l'étiquette », même si c'est vrai :
 * c'est qu'un relevé qui CONFIRME ce que fait déjà le code mérite plus de
 * méfiance qu'un relevé qui le contredit. Le second fait travailler, le
 * premier fait conclure.
 *
 * CE QUI RESTE, ET QUI EST INSCRIT : le champ absent produit TROIS messages
 * là où un seul suffirait — deux sondes du chemin de repli, puis la vraie
 * erreur qui arrête le gestionnaire. Ce n'est pas propre au morceau : « the
 * textStyle of card field "Absent" », sans morceau du tout, en produit
 * exactement trois aussi, et le faisait déjà. C'est le coût du repli, et il
 * tombera quand la lecture de propriété sur un morceau se lira dans l'ARBRE —
 * v3_chunk_cible le fait déjà pour l'ÉCRITURE, c'est son site jumeau qui
 * manque. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if      (k == HC_MSG) printf("   %s\n", t ? t : "");
    else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : "");
}

static Object *b;
static void essai(const char *titre, const char *corps)
{
    char s[600];
    snprintf(s, sizeof s, "on t\n%s\nend t\n", corps);
    hc_set_script(b, s);
    printf("== %s\n", titre);
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);
    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    Object *f  = hc_new_field(c, "T");
    hc_set_field_text(f, "Sun Mon Tue");
    b = hc_new_button(c, "B");
    hc_set_current_card(c);

    essai("1. le morceau EXISTE : juste des deux cotes",
     "  put \"[\" & the textStyle of word 2 of card field \"T\" & \"]\"\n"
     "  put \"   (HyperCard : plain)\"");

    essai("2. le MORCEAU manque, le champ est la : la valeur par DEFAUT",
     "  put \"[\" & the textStyle of word 99 of card field \"T\" & \"]\"\n"
     "  put \"   (HyperCard : plain — corrige, mesure du 27/09)\"");

    essai("2 bis. et le defaut est celui du CHAMP, pas du premier mot",
     "  set the textStyle of word 1 of card field \"T\" to bold\n"
     "  put \"word 1  : [\" & the textStyle of word 1 of card field \"T\" & \"]\"\n"
     "  put \"word 99 : [\" & the textStyle of word 99 of card field \"T\" & \"]\"\n"
     "  put \"   (la plage vide se lit a la FIN : « bold » ici voudrait dire\"\n"
     "  put \"    qu'un mot inexistant herite du premier)\"\n"
     "  set the textStyle of word 1 of card field \"T\" to plain");

    /* §3 — LE GESTIONNAIRE DOIT S'ARRÊTER ICI, SUR UN SEUL MESSAGE.
     *
     * La ligne qui suit la lecture ne doit JAMAIS paraître : si elle
     * reparaît un jour, c'est que l'erreur est redevenue un littéral, et le
     * harnais le dira sans qu'on le cherche.
     *
     * Et le NOMBRE de messages compte autant. Il y en avait TROIS — deux
     * sondes du chemin de repli, puis la vraie erreur — parce que la lecture
     * repartait à l'ancien évaluateur. Il n'y en a plus qu'UN depuis qu'elle
     * se fait dans l'arbre. Une seconde ligne qui réapparaîtrait ici voudrait
     * dire que le repli est revenu. */
    essai("3. le CHAMP manque : erreur, et le gestionnaire S'ARRETE",
     "  put \"[\" & the textStyle of word 2 of card field \"Absent\" & \"]\"\n"
     "  put \"   >>> CETTE LIGNE NE DOIT PAS PARAITRE\"");

    essai("4. ECRIRE au-dela de la fin : on ajoute SANS espace",
     "  put \"Sun Mon Tue\" into card field \"T\"\n"
     "  put \"X\" into word 99 of card field \"T\"\n"
     "  put the length of card field \"T\" into lg\n"
     "  put the number of words of card field \"T\" into nm\n"
     "  put \"[\" & card field \"T\" & \"]  \" & lg & \" octets, \" & nm & \" mots\"\n"
     "  put \"   (HyperCard : 12 octets, 3 mots — corrige et fidele)\"");

    essai("5. et le mot ainsi allonge garde son rang",
     "  put \"[\" & word 3 of card field \"T\" & \"]\"\n"
     "  set the textStyle of word 3 of card field \"T\" to bold\n"
     "  put \"word 3  : [\" & the textStyle of word 3 of card field \"T\" & \"]\"\n"
     "  put \"word 99 : [\" & the textStyle of word 99 of card field \"T\" & \"]\"");

    hc_free(st);
    return 0;
}
