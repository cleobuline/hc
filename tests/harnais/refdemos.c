/* refdemos — LES DÉMONSTRATIONS DE « HYPERTALK REFERENCE », UNE PAR UNE.
 *
 * La pile d'Apple montre chaque mot du langage par un script que joue le
 * bouton « Run the Script » (voir runscript.c pour le bouton lui-même).
 * L'utilisatrice l'a renvoyée le 9 octobre ; ses cent trente-six
 * démonstrations ont été jouées dans le noyau, par un faux hôte, et ce qui
 * échouait du côté du noyau est repris ici.
 *
 * Les gestionnaires sont recopiés dans leur FORME, pas dans leur texte : la
 * pile d'Apple n'entre pas dans le dépôt.
 *
 * Ailleurs, pour ne pas disperser une même correction :
 *   · « set lockMessages to true »        lockmsg.c
 *   · « the number of this bkgnd »        rang.c
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

static void menu_hote(const char *item) { printf("      -> hôte : doMenu « %s »\n", item ? item : ""); }

static void joue(const char *corps)
{
    char s[2048];
    printf("   %s\n", corps);
    snprintf(s, sizeof s, "on mouseUp\n  %s\nend mouseUp\n", corps);
    hc_set_script(b, s);
    hc_send(b, "mouseUp");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("Reference");
    Object *bg = hc_new_background(st, "Content");
    Object *c  = hc_new_card(st, bg, "selectedField");
    hc_register_stack(st);
    hc_set_current_card(c);
    b = hc_new_button(c, "Run the Script");
    Object *f1 = hc_new_field(c, "Titre");
    hc_set_field_text(f1, "Le titre");
    Object *f2 = hc_new_field(c, "Demo Script");
    hc_set_field_text(f2, "on foundFieldDemo\n  find \"foundFieldDemo\"\nend foundFieldDemo");

    /* « the name of the selectedField », « the name of the foundField » :
     * « objet introuvable ». L'arbre en fait un mot nu précédé de « the »,
     * et seule une variable était relue comme un descripteur. Pire, « the
     * textFont of the selectedField » rendait la chaîne « textFont of the
     * selectedField », sans erreur. */
    puts("== 1. the selectedField et the foundField désignent un champ ==");
    joue("put the name of the selectedField");          /* rien de sélectionné */
    joue("select line 1 of card field 1\n"
         "  put the selectedField\n"
         "  put the name of the selectedField\n"
         "  put the short name of the selectedField\n"
         "  put the number of the selectedField\n"
         "  put the name of selectedField()");
    joue("select line 2 of card field \"Demo Script\"\n"
         "  put \"The field containing the selection is:\" && the name of the selectedField");
    joue("find \"foundFieldDemo\"\n"
         "  put \"The foundField is\" && the foundField & \".\"\n"
         "  put \"The name of the foundField is\" && the name of the foundField & \".\"");
    joue("find \"absent de partout\"\n  put the name of the foundField");

    /* « doMenu "Background","Edit" » : « ne sait pas faire ». Les deux
     * formes longues renvoyaient à l'ancien interpréteur, qui a disparu. */
    puts("\n== 2. doMenu article, menu — et without dialog ==");
    h.do_menu = menu_hote;
    hc_set_host(&h);
    joue("doMenu \"Background\",\"Edit\" -- view the background layer\n"
         "  doMenu \"Background\",\"Edit\" -- return to the card layer");
    joue("doMenu \"Card Info\" without dialog");
    joue("doMenu \"Bkgnd Info\", \"Objects\" without dialog");
    joue("put \"Edit\" into m\n  doMenu \"Background\", m");
    joue("doMenu \"Background\", zorglub()\n  put \"pas ici\"");
    /* Le message part avec l'article : la pile peut toujours l'intercepter. */
    hc_set_script(st, "on doMenu quoi\n  put \"pile : \" & quoi\nend doMenu\n");
    joue("doMenu \"Background\",\"Edit\"");
    hc_set_script(st, "");

    /* « show card picture » : « objet introuvable » — l'arbre y voyait la
     * carte de rang « picture ». Les quatre formes de la carte « show »
     * d'Apple, et « pict » pour « picture ». */
    puts("\n== 3. show / hide card picture, picture of bkgnd ==");
    Object *c2 = hc_new_card(st, bg, "Deuxième");
    (void)c2;
    joue("hide card picture\n  put \"carte : \" & the showPict of this card");
    joue("show card picture\n  put \"carte : \" & the showPict of this card");
    joue("hide bkgnd pict\n  put \"fond : \" & the showPict of this bkgnd");
    joue("show background picture\n  put \"fond : \" & the showPict of this bkgnd");
    joue("hide picture of card 2\n  put \"carte 2 : \" & the showPict of card 2"
         " && \"/ carte 1 : \" & the showPict of card 1");
    joue("show pict of last cd\n  put \"carte 2 : \" & the showPict of card 2");
    joue("hide picture of bkgnd 1\n  put \"fond 1 : \" & the showPict of bkgnd 1");
    joue("show picture of bkgnd 1\n  put \"fond 1 : \" & the showPict of bkgnd 1");
    joue("hide picture of card \"Absente\"\n  put \"pas ici\"");
    /* Un champ ou un bouton n'a pas de peinture : on ne s'y trompe pas. */
    joue("hide card field \"Titre\"\n  put \"Titre : \" & the visible of card field \"Titre\"");
    joue("show card field \"Titre\"");

    /* « char 1 to 0 » rendait le premier caractère : n2 = 0 voulait dire
     * « pas de fin ». MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II), 9 octobre :
     *
     *     char 1 to 0 of "abc"                                    []
     *     char 1 to -1 of "abc"                                   []
     *     word 1 to 0 of "a b c"                                  a
     *     the number of lines of return                           1
     *     the number of lines of char 1 to 0 of (return & "abc" & return)  0
     *
     * Les mots gardent leur lecture. La fonction textToLineNum de la pile
     * « HyperCard Help » (Ken Laws, selon son commentaire), recopiée dans sa
     * forme, doit rendre 0 quand le texte manque : elle rendait 1. */
    puts("\n== 4. char 1 to 0 est vide ==");
    joue("put \"[\" & char 1 to 0 of \"abc\" & \"]\"\n"
         "  put \"[\" & char 1 to -1 of \"abc\" & \"]\"\n"
         "  put \"[\" & word 1 to 0 of \"a b c\" & \"]\"\n"
         "  put the number of lines of return\n"
         "  put the number of lines of char 1 to 0 of (return & \"abc\" & return)");
    joue("put \"[\" & char 3 to 1 of \"abc\" & \"]\"\n"
         "  put \"[\" & char 2 to 3 of \"abc\" & \"]\"\n"
         "  put 0 into z\n  put \"[\" & char 1 to z of \"abc\" & \"]\"");
    hc_set_script(st,
        "function textToLineNum theText,theContainer\n"
        "  put return & theContainer & return into theContainer\n"
        "  return the number of lines of char 1 to  offset(return & theText & return,theContainer) of theContainer\n"
        "end textToLineNum\n");
    joue("put \"HyperCard Help\" & return & \"HyperTalk Reference\" into suite\n"
         "  put textToLineNum(\"HyperTalk Reference\", suite)\n"
         "  put textToLineNum(\"HyperCard Help\", suite)\n"
         "  put textToLineNum(\"Mon Calendrier\", suite)");
    hc_set_script(st, "");
    /* NON MESURÉ DANS HYPERCARD : écrire, effacer et sélectionner dans une
     * telle plage. L'écriture refusait déjà toute fin inférieure à 1 (« char
     * 1 to -1 ») ; « to 0 » suit maintenant. Effacer n'ôte rien ; select pose
     * un point d'insertion au début. */
    joue("put \"abc\" into v\n  put \"X\" into char 1 to 0 of v\n  put v");
    joue("put \"abc\" into v\n  delete char 1 to 0 of v\n  put v");
    joue("put \"abc\" into v\n  delete char 1 to -1 of v\n  put v");
    joue("select char 1 to 0 of card field \"Titre\"\n"
         "  put the selectedChunk\n  put \"[\" & the selectedText & \"]\"");

    hc_free(st);
    return 0;
}
