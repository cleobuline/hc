/* LIRE UNE PROPRIÉTÉ SUR UN MORCEAU QUI N'EXISTE PAS.
 *
 * MESURÉ DANS HYPERCARD, et le résultat m'a démenti. J'avais annoncé un
 * « vrai défaut » :
 *
 *     the textStyle of word 99 of card field "T"
 *         -> textStyle of word 99 of card field "T"
 *
 * La lecture ratée rend SA PROPRE PHRASE comme valeur. C'est effectivement ce
 * que fait HC — et c'est EXACTEMENT ce que fait HyperCard, banc à l'appui,
 * pour le morceau absent comme pour le champ absent. HC est fidèle, et le
 * défaut n'existait que dans ma tête.
 *
 * CE HARNAIS EST DONC UN VERROU, pas un constat de défaut. Rien ne testait
 * cette valeur, et elle a tout l'air d'une bavure : quelqu'un — moi, demain —
 * la « corrigerait » en vide ou en erreur, et casserait une fidélité mesurée.
 * C'est la règle des littéraux nus d'HyperTalk qui joue jusqu'au bout : une
 * expression qui ne se résout pas vaut son propre texte.
 *
 * LA CONSÉQUENCE POUR LES PILES, elle, reste vraie et vaut d'être sue :
 *
 *     if the textStyle of word 99 of f is "bold" then …
 *
 * compare à une phrase, et la comparaison est fausse sans se plaindre. Mais
 * c'est vrai d'HyperCard aussi. Une pile écrite pour HyperCard en tient compte,
 * ou n'en tient pas compte, et notre affaire est de faire pareil.
 *
 * CE QUI DIVERGE ENCORE, et c'est la seule chose : LE NOMBRE DE MESSAGES.
 * HyperCard n'en montre AUCUN ; HC en émet TROIS pour une seule lecture, tous
 * identiques. Ils viennent du chemin de repli : la lecture de propriété sur un
 * morceau n'est pas portée sur l'arbre, si bien que la v3 la confie à
 * l'ancien évaluateur — deux fois depuis v3_recours, une fois par
 * l'analyseur d'expressions v1 —, et chaque tentative signale l'objet
 * introuvable avant que l'ensemble ne se replie sur le littéral.
 *
 * Le compte est donc INSCRIT ICI, pour qu'il se voie bouger. Le porter sur
 * l'arbre est le vrai remède — v3_chunk_cible existe déjà pour l'ÉCRITURE du
 * même genre de propriété, c'est son site jumeau qui manque — et ce n'est pas
 * un nettoyage de messages. */
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

    essai("1. le morceau EXISTE : une vraie propriété",
     "  put \"[\" & the textStyle of word 2 of card field \"T\" & \"]\"");

    essai("2. le MORCEAU manque : la phrase elle-même, comme HyperCard",
     "  put \"[\" & the textStyle of word 99 of card field \"T\" & \"]\"\n"
     "  put \"   (HyperCard rend la meme phrase — mesure du 27/09)\"");

    essai("3. le CHAMP manque : la phrase aussi, MAIS TROIS MESSAGES",
     "  put \"[\" & the textStyle of word 2 of card field \"Absent\" & \"]\"\n"
     "  put \"   (HyperCard rend la meme phrase et N'AFFICHE RIEN.\"\n"
     "  put \"    Les trois lignes ci-dessus sont la divergence, et la\"\n"
     "  put \"    seule : elles viennent du chemin de repli)\"");

    essai("4. ÉCRIRE dans un mot qui n'existe pas : on ajoute à la fin",
     "  put \"Sun Mon Tue\" into card field \"T\"\n"
     "  put \"X\" into word 99 of card field \"T\"\n"
     "  put the length of card field \"T\" into lg\n"
     "  put the number of words of card field \"T\" into nm\n"
     "  put \"[\" & card field \"T\" & \"]  \" & lg & \" octets, \" & nm & \" mots\"\n"
     "  put \"   (HyperCard : 13 octets, 4 mots — il ajoute, il ne remplit pas)\"");

    essai("5. et la propriété se relit au rang RÉEL du mot ajouté",
     "  set the textStyle of word 4 of card field \"T\" to bold\n"
     "  put \"word 4  : [\" & the textStyle of word 4 of card field \"T\" & \"]\"\n"
     "  put \"word 99 : [\" & the textStyle of word 99 of card field \"T\" & \"]\"");

    hc_free(st);
    return 0;
}
