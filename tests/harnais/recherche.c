/* « FIND », TEL QU'IL A ÉTÉ MESURÉ DANS HYPERCARD.
 *
 * Deux campagnes. La première, onze lignes tapées dans Basilisk II, a donné
 * le modèle de base. La seconde, un banc de mesure complet joué des deux
 * côtés — docs/mesures/find.txt —, a donné cinq divergences de plus ET
 * DÉMOLI TROIS DES ANNOTATIONS DE CE HARNAIS. Elles sont corrigées ci-dessous
 * et le démenti est écrit, parce qu'une supposition qu'on garde après l'avoir
 * mesurée fausse est pire qu'un trou.
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
 * CE QUE LE SECOND BANC A AJOUTÉ, mesure par mesure :
 *
 *   · LE CURSEUR NE SE REMET JAMAIS À ZÉRO. Ni la navigation ni le changement
 *     de motif ne le ramènent au début. C'est la mesure qui a démoli la §5 et
 *     la §6 de ce harnais (voir leurs commentaires).
 *
 *   · LE DÉSIGNATEUR s'écrit « bkgnd field 1 » ou « card field 1 » — la
 *     couche et le NUMÉRO, jamais le nom.
 *
 *   · « Not found » PORTE UNE MAJUSCULE. HC écrivait « not found ». Une pile
 *     qui compare « the result » à la chaîne exacte se tromperait.
 *
 *   · « of marked cards » RESTREINT POUR DE VRAI : sans aucune carte marquée,
 *     « Not found ».
 *
 *   · LES ACCENTS : c'est la FORME DE BASE qui replie — « find "eleve" »
 *     trouve « élève » —, et « find international » qui NE replie PAS. Soit
 *     l'inverse exact de ce qu'on avait supposé, et de ce que la §9 écrivait.
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
 *     pour faire défiler.
 *
 *   · LE BALAYAGE NE REBOUCLAIT PAS DANS LA CARTE DE DÉPART. Il la visitait
 *     une seule fois, et sous la restriction du curseur : son début n'était
 *     donc jamais revu. Une recherche relancée juste après une trouvaille
 *     dans la même carte rendait « Not found » là où HyperCard reboucle. Ce
 *     défaut-là ne s'est vu qu'en jouant le banc — aucune des onze mesures
 *     d'origine ne le touchait. */
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
    hc_set_field_text(f3, "\xc3\xa9l\xc3\xa8ve \xc3\xa0 No\xc3\xabl");
    (void)f3;
    /* un champ de FOND, vide sauf quand une section l'écrit : c'est lui qui
     * montre que le désignateur distingue les deux couches. */
    hc_new_field(bg, "T");

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
     "  put \"4 find whole          : [\" & the result & \"]   HyperCard : Not found\"\n"
     "  find whole \"beta gamma\"\n"
     "  put \"5 find whole exact    : [\" & the result & \"]   HyperCard : trouvé\"\n"
     "  find word \"bet\"\n"
     "  put \"6 find word partiel   : [\" & the result & \"]   HyperCard : Not found\"\n"
     "  find word \"beta\"\n"
     "  put \"7 find word entier    : [\" & the result & \"]   HyperCard : trouvé\"\n"
     "  find \"gamma alpha\"\n"
     "  put \"8 find ordre inverse  : [\" & the result & \"]   HyperCard : trouvé\"");

    essai("2. LA RECHERCHE EST PAR CARTE, pas par champ",
     "  go to card \"un\"\n"
     "  find \"bet omeg\"\n"
     "  put \"un mot par champ : [\" & the result & \"]   HyperCard : trouvé\"");

    /* §3 — LE DÉSIGNATEUR, corrigé et non plus seulement inscrit.
     *
     * L'ancienne version de ce harnais notait la divergence puis la laissait
     * en place : « HyperCard écrit card field 1 là où l'on écrit le nom ».
     * Le banc l'a confirmée sur la couche de FOND aussi — « bkgnd field 1 » —,
     * et une pile qui relit « the foundField » pour y remettre du texte a
     * besoin d'un désignateur qui se résout. Un nom ne dit pas la couche ;
     * deux champs du même nom, l'un sur la carte l'autre sur le fond, se
     * confondaient. */
    essai("3. « the foundText » rend LE MOT, et le désignateur dit la COUCHE",
     "  go to card \"un\"\n"
     "  find \"bet alph\"\n"
     "  put \"foundText  : \" & the foundText & \"    HyperCard : beta\"\n"
     "  put \"foundChunk : \" & the foundChunk\n"
     "  put \"foundField : \" & the foundField & \"   (couche + numéro)\"\n"
     "  put \"foundLine  : \" & the foundLine");

    /* §4 — LE « find "zorglub" » DU DÉBUT N'EST PAS DU DÉCOR.
     *
     * Le curseur est GLOBAL et survit d'une section à l'autre : sans le
     * remettre à zéro, cette section repartirait de la trouvaille de la §3 et
     * ne mesurerait plus le tour de piste depuis la carte 1. Une recherche qui
     * échoue vide « the foundText » — ça, c'est mesuré — et donc le curseur
     * avec, puisqu'il n'est rien d'autre que la dernière trouvaille. C'est le
     * seul moyen d'obtenir un curseur neuf depuis un script, et les §5, §6 et
     * §11 s'en servent pareil.
     *
     * Ce qu'on ne sait PAS, et qui n'est pas prétendu : si HyperCard remet
     * vraiment son curseur à zéro sur un échec, ou s'il le laisse là où il
     * était. La question ne se posait pas avant que le curseur existe. */
    essai("4. RÉPÉTER AVANCE, ET ÇA BOUCLE",
     "  go to card \"un\"\n"
     "  find \"zorglub\"\n"
     "  find \"alpha\"\n"
     "  put \"1er : \" & the short name of this card\n"
     "  find \"alpha\"\n"
     "  put \"2e  : \" & the short name of this card\n"
     "  find \"alpha\"\n"
     "  put \"3e  : \" & the short name of this card\n"
     "  put \"   (HyperCard : carte 1, carte 2, carte 1 — le tour se referme)\"");

    /* §5 — CE QUE CE HARNAIS AFFIRMAIT, ET QUE LA MESURE A DÉMENTI.
     *
     * Il était écrit ici : « un motif différent repart de la carte courante,
     * parce qu'hériter du compteur d'une autre recherche ferait chercher à
     * partir d'un endroit que rien n'explique ». C'était un raisonnement, pas
     * un relevé, et le banc l'a démoli d'une ligne :
     *
     *     find "bet alph"   -> trouve « beta », char 7 à 10 de la carte 1
     *     find "alpha"      -> HyperCard rend la CARTE 2
     *
     * Motif tout neuf, et pourtant il ne repart pas du début : il reprend
     * après le caractère 10, ne trouve plus d'« alpha » dans la carte 1, et
     * passe à la suivante. Le curseur est UNIQUE et INDIFFÉRENT au motif. */
    essai("5. UN MOTIF NEUF HÉRITE DU CURSEUR — mesuré, contre l'intuition",
     "  go to card \"un\"\n"
     "  find \"zorglub\"\n"
     "  find \"bet alph\"\n"
     "  put \"trouvé en            : \" & the short name of this card\n"
     "  find \"alpha\"\n"
     "  put \"motif neuf, ensuite  : \" & the short name of this card\n"
     "  put \"   (HyperCard : « deux ». Il reprend APRÈS la trouvaille\"\n"
     "  put \"    précédente, alors que le motif a changé)\"");

    /* §6 — LA MÊME QUESTION POUR LA NAVIGATION, tranchée elle aussi.
     *
     * Deux règles collaient aux onze mesures d'origine : (a) on n'avance que
     * si l'on est resté sur la carte trouvée, (b) on avance où qu'on soit
     * allé. Ce harnais posait la question au lieu de l'inventer — et la
     * réponse est (b) :
     *
     *     find "alpha"   -> carte 1
     *     go to card 3
     *     find "alpha"   -> HyperCard rend la carte 2, pas la carte 1
     *
     * Aller ailleurs à la main ne remet rien à zéro. HC appliquait (a). */
    essai("6. NAVIGUER AILLEURS NE REMET PAS LE CURSEUR À ZÉRO",
     "  go to card \"un\"\n"
     "  find \"zorglub\"\n"
     "  find \"alpha\"\n"
     "  put \"trouvé en   : \" & the short name of this card\n"
     "  go to card \"trois\"\n"
     "  find \"alpha\"\n"
     "  put \"depuis trois: \" & the short name of this card\n"
     "  put \"   (HyperCard : « deux » — il continue son balayage. « un »\"\n"
     "  put \"    aurait voulu dire qu'il repart de la carte courante)\"");

    essai("7. RIEN TROUVÉ : le résultat le dit, et foundText se vide",
     "  go to card \"un\"\n"
     "  find \"zorglub\"\n"
     "  put \"result    : [\" & the result & \"]   (majuscule : « Not found »)\"\n"
     "  put \"foundText : [\" & the foundText & \"]\"");

    essai("8. « in <champ> » restreint la recherche",
     "  go to card \"un\"\n"
     "  find \"omeg\" in field \"A\"\n"
     "  put \"omeg dans le champ A : [\" & the result & \"]   (omeg est dans B)\"\n"
     "  find \"omeg\" in field \"B\"\n"
     "  put \"omeg dans le champ B : [\" & the result & \"]\"");

    /* §9 — L'ANNOTATION LA PLUS FAUSSE DES TROIS.
     *
     * Il était écrit ici que « international » était accepté sans replier les
     * accents, et que le repli restait à faire. Le banc dit le contraire :
     *
     *     find "eleve"                -> TROUVE « élève »
     *     find international "eleve"  -> Not found
     *     find international "noel"   -> Not found
     *
     * C'est donc la forme DE BASE qui replie, et « international » qui compare
     * les octets tels quels. Le nom trompe : il ne veut pas dire « sois plus
     * tolérant », il veut dire « compare selon l'ordre international », donc
     * sans le repli de la recherche ordinaire. */
    essai("9. LES ACCENTS : la forme de BASE replie, « international » NON",
     "  go to card \"un\"\n"
     "  find \"eleve\"\n"
     "  put \"1 base, sans accent      : [\" & the result & \"]   HyperCard : trouvé\"\n"
     "  go to card \"un\"\n"
     "  find international \"eleve\"\n"
     "  put \"2 international           : [\" & the result & \"]   HyperCard : Not found\"\n"
     "  go to card \"un\"\n"
     "  find international \"noel\"\n"
     "  put \"3 international, tréma    : [\" & the result & \"]   HyperCard : Not found\"\n"
     "  go to card \"un\"\n"
     "  find \"\xc3\xa9l\xc3\xa8ve\"\n"
     "  put \"4 base, avec accent       : [\" & the result & \"]   HyperCard : trouvé\"");

    /* §10 — « of marked cards » : accepté et IGNORÉ, c'était écrit ici comme
     * un chantier ouvert. Le banc a chiffré les deux cas, donc il est fermé :
     * sans carte marquée « Not found », et avec la carte 2 marquée c'est elle
     * qu'on atteint même en partant de la carte 1. */
    essai("10. « of marked cards » RESTREINT POUR DE VRAI",
     "  go to card \"un\"\n"
     "  unmark all cards\n"
     "  find \"alph\" of marked cards\n"
     "  put \"aucune marquée   : [\" & the result & \"]   HyperCard : Not found\"\n"
     "  go to card \"deux\"\n"
     "  mark this card\n"
     "  go to card \"un\"\n"
     "  find \"alph\" of marked cards\n"
     "  put \"carte deux seule : [\" & the result & \"]   HyperCard : trouvé\"\n"
     "  put \"carte atteinte   : \" & the short name of this card & \"   HyperCard : deux\"\n"
     "  unmark all cards");

    /* §11 — LE DÉSIGNATEUR SUR LES DEUX COUCHES. Deux champs, un par couche,
     * et le même mot dans chacun : c'est le seul essai qui distingue
     * « card field » de « bkgnd field », et donc le seul qui aurait vu
     * l'ancien format à nom nu confondre les deux. */
    essai("11. « card field » ou « bkgnd field » selon la couche",
     "  go to card \"trois\"\n"
     "  put \"sigma\" into bg field \"T\"\n"
     "  find \"zorglub\"\n"
     "  find \"sigma\"\n"
     "  put \"trouvé dans le fond  : \" & the foundField\n"
     "  put \"le morceau           : \" & the foundChunk\n"
     "  put empty into bg field \"T\"\n"
     "  go to card \"un\"\n"
     "  find \"zorglub\"\n"
     "  find \"omeg\"\n"
     "  put \"trouvé dans la carte : \" & the foundField");

    hc_free(st);
    return 0;
}
