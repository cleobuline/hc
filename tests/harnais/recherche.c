/* « FIND », TEL QU'IL A ÉTÉ MESURÉ DANS HYPERCARD.
 *
 * Onze lignes tapées dans Basilisk II ont donné le modèle entier, et aucune
 * des réponses n'était devinable. La troisième surtout.
 *
 * SUR UN CHAMP CONTENANT « alpha beta gamma delta » :
 *
 *     find "bet gam"          trouvé    le motif est DÉCOUPÉ EN MOTS
 *     find chars "ph be"      trouvé    découpé aussi
 *     find string "a bet"     trouvé    la phrase ENTIÈRE, à cheval sur deux
 *                                       mots : « alph[a bet]a »
 *     find whole "a bet"      NON       la phrase, mais aux frontières de mots
 *     find whole "beta gamma" trouvé
 *     find word "bet"         NON       le mot entier seulement
 *     find word "beta"        trouvé
 *     find "gamma alpha"      trouvé    L'ORDRE NE COMPTE PAS
 *
 * ET SUR DEUX CARTES, deux champs sur la première :
 *
 *     find "alph gam"         trouvé    un mot par champ, MÊME CARTE
 *     find "bet alph" -> foundText = « beta »   LE MOT, pas le motif
 *     find "alpha" trois fois -> carte 1, carte 2, carte 1   ÇA BOUCLE
 *
 * CE QUE HC FAISAIT AVANT, et qui est corrigé :
 *
 *   · « find chars » et « find word » NE PASSAIENT PAS L'ANALYSEUR. « char »
 *     et « word » sont des mots de morceau en HyperTalk — « word 2 of f » —,
 *     et la grammaire réclamait « of ». Deux formes sur cinq étaient donc
 *     inaccessibles avant qu'on parle de sémantique, et l'erreur ne nommait
 *     même pas « find ».
 *
 *   · LE MOTIF N'ÉTAIT PAS DÉCOUPÉ. « find "bet gam" » cherchait la suite
 *     littérale « bet gam » et ne trouvait rien. C'est pourtant la forme la
 *     plus courante : on tape deux mots pour retrouver une fiche.
 *
 *   · « string » et « whole » faisaient la même chose que « chars ». Trois
 *     formes pour un seul comportement, alors que la mesure en distingue
 *     trois différents.
 *
 *   · RÉPÉTER « find » N'AVANÇAIT PAS. Trois fois la même carte. C'est TOUT
 *     l'usage de la commande : on tape une fois, puis on appuie sur Retour
 *     pour faire défiler. En l'état elle était à peu près inutile en
 *     interactif, et aucun harnais ne s'en plaignait.
 *
 * CE QUI N'EST PAS FAIT, ET QUI EST DIT : « find international » est accepté
 * par la grammaire mais ne replie pas encore les accents ; « of marked
 * cards » est accepté et ignoré. Les sections 9 et 10 les enregistrent tels
 * quels. Une porte annoncée et non percée est ce qu'on a passé la journée à
 * débusquer — il n'est pas question d'en ouvrir une de plus en silence. */
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
static void essai(const char *titre, const char *corps)
{
    char s[3072];
    snprintf(s, sizeof s, "on t\n%s\nend t\n", corps);
    hc_set_script(b, s);
    printf("── %s\n", titre);
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *c1 = hc_new_card(st, bg, "un");
    Object *c2 = hc_new_card(st, bg, "deux");
    Object *c3 = hc_new_card(st, bg, "trois");

    Object *f1 = hc_new_field(c1, "A");
    hc_set_field_text(f1, "alpha beta gamma delta");
    Object *f1b = hc_new_field(c1, "B");
    hc_set_field_text(f1b, "omega");
    Object *f2 = hc_new_field(c2, "A");
    hc_set_field_text(f2, "alpha zeta");
    Object *f3 = hc_new_field(c3, "A");
    hc_set_field_text(f3, "rien ici");
    (void)f3;

    hc_set_current_card(c1);
    b = hc_new_button(c1, "Z");

    essai("1. LES HUIT MESURES, rejouées telles quelles",
     "  go to card \"un\"\n"
     "  find \"bet gam\"\n"
     "  put \"1 find deux mots      : [\" & the result & \"]   HyperCard : trouvé\"\n"
     "  find chars \"ph be\"\n"
     "  put \"2 find chars deux mots: [\" & the result & \"]   HyperCard : trouvé\"\n"
     "  find string \"a bet\"\n"
     "  put \"3 find string         : [\" & the result & \"]   HyperCard : trouvé\"\n"
     "  find whole \"a bet\"\n"
     "  put \"4 find whole          : [\" & the result & \"]   HyperCard : not found\"\n"
     "  find whole \"beta gamma\"\n"
     "  put \"5 find whole exact    : [\" & the result & \"]   HyperCard : trouvé\"\n"
     "  find word \"bet\"\n"
     "  put \"6 find word partiel   : [\" & the result & \"]   HyperCard : not found\"\n"
     "  find word \"beta\"\n"
     "  put \"7 find word entier    : [\" & the result & \"]   HyperCard : trouvé\"\n"
     "  find \"gamma alpha\"\n"
     "  put \"8 find ordre inverse  : [\" & the result & \"]   HyperCard : trouvé\"");

    essai("2. LA RECHERCHE EST PAR CARTE, pas par champ",
     "  go to card \"un\"\n"
     "  find \"bet omeg\"\n"
     "  put \"un mot par champ : [\" & the result & \"]   HyperCard : trouvé\"");

    essai("3. « the foundText » rend LE MOT, pas le motif",
     "  go to card \"un\"\n"
     "  find \"bet alph\"\n"
     "  put \"foundText  : \" & the foundText & \"    HyperCard : beta\"\n"
     "  put \"foundChunk : \" & the foundChunk\n"
     "  put \"foundField : \" & the foundField\n"
     "  put \"   (HyperCard ecrit « card field 1 » la ou l'on ecrit le nom :\"\n"
     "  put \"    c'est le format du designateur, pas la recherche. Inscrit\"\n"
     "  put \"    ici, pas corrige au juge)\"");

    essai("4. RÉPÉTER AVANCE, ET ÇA BOUCLE",
     "  go to card \"un\"\n"
     "  find \"alpha\"\n"
     "  put \"1er : \" & the short name of this card\n"
     "  find \"alpha\"\n"
     "  put \"2e  : \" & the short name of this card\n"
     "  find \"alpha\"\n"
     "  put \"3e  : \" & the short name of this card\n"
     "  put \"   (HyperCard : carte 1, carte 2, carte 1)\"");

    essai("5. UN MOTIF DIFFÉRENT REPART DE LA CARTE COURANTE",
     "  go to card \"un\"\n"
     "  find \"delta\"\n"
     "  put \"find delta (neuf)    : \" & the short name of this card\n"
     "  find \"alpha\"\n"
     "  put \"puis alpha (neuf)    : \" & the short name of this card\n"
     "  find \"alpha\"\n"
     "  put \"puis la meme         : \" & the short name of this card\n"
     "  put \"   (une recherche NEUVE ne doit pas heriter du compteur de la\"\n"
     "  put \"    precedente : on chercherait a partir d'un endroit que rien\"\n"
     "  put \"    n'explique. Seule la REPETITION avance)\"");

    /* §6 — LA QUESTION QUI N'EST PAS MESURÉE, et qui est posée plutôt que
     * tranchée en silence.
     *
     * Deux règles collent également aux relevés d'HyperCard :
     *
     *   (a) on avance si le motif est le même ET qu'on est resté sur la carte
     *       trouvée. Naviguer ailleurs repart de là.
     *   (b) on avance dès que le motif est le même, où qu'on soit allé.
     *
     * La mesure d'origine — trois « find "alpha" » d'affilée sans bouger — ne
     * les distingue pas. Celle-ci les distingue : sous (a) on repart de
     * « trois » et l'on tombe sur « un » ; sous (b) on continue après « un »
     * et l'on tombe sur « deux ».
     *
     * HC applique (a), parce qu'une recherche qui reprend un compteur alors
     * qu'on a changé de carte à la main surprendrait — mais c'est un
     * RAISONNEMENT, pas un relevé, et il est écrit ici comme tel. */
    essai("6. NAVIGUER AILLEURS : (a) ou (b) ? — À MESURER",
     "  go to card \"un\"\n"
     "  find \"alpha\"\n"
     "  put \"trouve en   : \" & the short name of this card\n"
     "  go to card \"trois\"\n"
     "  find \"alpha\"\n"
     "  put \"depuis trois: \" & the short name of this card\n"
     "  put \"   (« un » = HC repart de la carte courante ; « deux » aurait\"\n"
     "  put \"    voulu dire qu'il continue son balayage. HyperCard : PAS\"\n"
     "  put \"    ENCORE MESURE — voir le commentaire de ce harnais)\"");

    essai("7. RIEN TROUVÉ : le résultat le dit, et foundText se vide",
     "  go to card \"un\"\n"
     "  find \"zorglub\"\n"
     "  put \"result    : [\" & the result & \"]\"\n"
     "  put \"foundText : [\" & the foundText & \"]\"");

    essai("8. « in <champ> » restreint la recherche",
     "  go to card \"un\"\n"
     "  find \"omeg\" in field \"A\"\n"
     "  put \"omeg dans le champ A : [\" & the result & \"]   (omeg est dans B)\"\n"
     "  find \"omeg\" in field \"B\"\n"
     "  put \"omeg dans le champ B : [\" & the result & \"]\"");

    essai("9. « international » : ACCEPTÉ, pas encore replié",
     "  go to card \"un\"\n"
     "  find international \"alph\"\n"
     "  put \"result : [\" & the result & \"]   (il s'analyse, et se comporte\"\n"
     "  put \"   comme la forme de base — le repli des accents reste A FAIRE)\"");

    essai("10. « of marked cards » : ACCEPTÉ, IGNORÉ",
     "  go to card \"un\"\n"
     "  find \"alph\" of marked cards\n"
     "  put \"result : [\" & the result & \"]   (aucune carte n'est marquee,\"\n"
     "  put \"   donc HyperCard ne trouverait RIEN. On trouve quand meme :\"\n"
     "  put \"   la restriction est ignoree, et c'est inscrit ici pour ne pas\"\n"
     "  put \"   croire le chantier fini)\"");

    hc_free(st);
    return 0;
}
