/* LIRE UNE PROPRIÉTÉ SUR UN MORCEAU QUI N'EXISTE PAS.
 *
 * HYPERCARD REND « plain ». HC rend la PHRASE ELLE-MÊME :
 *
 *     the textStyle of word 99 of card field "T"
 *         HyperCard -> plain
 *         HC        -> textStyle of word 99 of card field "T"
 *
 * C'est un défaut, et la conséquence pour les piles est directe :
 *
 *     if the textStyle of word 99 of f is "bold" then …
 *
 * compare à une phrase, et la comparaison est fausse sans se plaindre.
 *
 * J'AI CRU LE CONTRAIRE PENDANT UN COMMIT, ET IL FAUT DIRE POURQUOI. Un
 * premier relevé m'était arrivé sans être étiqueté ; j'y ai lu HyperCard
 * alors que c'était HC, j'ai conclu que HC était fidèle, retiré l'accusation
 * et écrit un harnais pour VERROUILLER la phrase. Le relevé suivant, marqué
 * « dans hypercard », donne « plain ».
 *
 * La leçon n'est pas « vérifier d'où vient un relevé », même si c'est vrai :
 * c'est qu'un relevé qui CONFIRME ce que fait déjà le code mérite plus de
 * méfiance qu'un relevé qui le contredit. Le second fait travailler, le
 * premier fait conclure — et j'ai conclu.
 *
 * CE QUI SE MESURE ICI, en attendant la correction :
 *
 *   1. le morceau qui existe : « plain », juste des deux côtés ;
 *   2. le morceau absent : la phrase, là où HyperCard dit « plain » ;
 *   3. le champ absent : la phrase ET TROIS MESSAGES, là où HyperCard
 *      n'affiche rien ;
 *   4. l'écriture au-delà de la fin : « Sun Mon TueX », 12 octets, 3 mots —
 *      corrigé, et fidèle depuis la mesure du 27/09.
 *
 * D'OÙ VIENT LE DÉFAUT, relevé sous backtrace, trois fois la même pile :
 *
 *     noeud_of -> recours_pont -> v3_recours -> term_value -> call_function
 *              -> eval_expr
 *
 * La lecture d'une propriété sur un MORCEAU n'est pas portée sur l'arbre :
 * noeud_of résout sa cible par hote.resout, qui ne sait résoudre qu'un OBJET,
 * échoue sur un nœud de morceau, et confie tout à l'ancien évaluateur — deux
 * fois depuis v3_recours, une troisième par l'analyseur v1. Chaque tentative
 * signale l'objet introuvable, puis l'ensemble se replie sur le littéral nu,
 * d'où la phrase.
 *
 * Le remède est le SITE JUMEAU MANQUANT de v3_chunk_cible, qui fait déjà ce
 * travail pour l'ÉCRITURE — « set the textStyle of word 3 of line 3 of me to
 * bold » lit ses rangs dans l'arbre depuis des jours. La lecture, elle, n'a
 * jamais eu son chemin. */
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

    essai("2. le MORCEAU manque : HC rend la phrase, HyperCard dit « plain »",
     "  put \"[\" & the textStyle of word 99 of card field \"T\" & \"]\"\n"
     "  put \"   (HyperCard : plain — DEFAUT, mesure du 27/09)\"");

    essai("3. le CHAMP manque : la phrase, ET TROIS MESSAGES",
     "  put \"[\" & the textStyle of word 2 of card field \"Absent\" & \"]\"\n"
     "  put \"   (HyperCard n'affiche RIEN, et n'ecrit meme pas la ligne :\"\n"
     "  put \"    a confirmer, voir docs/mesures/morceaux.txt)\"");

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
