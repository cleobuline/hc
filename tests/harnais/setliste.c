/* « set the rect of X to 1,2,30,40 » ne faisait RIEN, et sans un mot.
 *
 * Le motif de `set` est « e to * », et l'etoile vaut « zero a N expressions
 * separees par des virgules » : l'analyseur rend donc un enfant PAR ELEMENT.
 * v3_cmd_set ne lisait le sous-arbre que lorsqu'il y en avait exactement UN,
 * et retombait sinon sur eval_checked, qui relit le texte brut « 1,2,30,40 »
 * comme UNE expression — et hct_expression s'arrete a la premiere virgule.
 *
 * parse_ints n'en comptait donc qu'un au lieu de quatre, l'ecriture etait
 * abandonnee, et rien ne bougeait :
 *
 *     set the rect of button "Ok" to 1,2,30,40     ne faisait rien
 *     set the rect of button "Ok" to "1,2,30,40"   marchait
 *     set the loc of button "Ok" to 200,200        ne faisait rien
 *
 * Or c'est l'idiome courant : personne n'ecrit les guillemets.
 *
 * Verifie sur 1db2a7b, bien avant l'audit : le defaut est ANCIEN, il n'a
 * jamais marche. Il a ete trouve par une sonde ecrite pour tout autre chose —
 * verifier que le durcissement de hc_entier n'avait pas casse d'autres
 * lecteurs. C'est la troisieme fois qu'un defaut tombe comme ca, et c'est un
 * argument pour sonder large.
 *
 * Ce harnais tient les deux bouts : la liste nue marche, et les formes qui
 * marchaient DEJA n'ont pas change — la valeur citee, la variable, et la
 * liste de NOMS NUS de textStyle, qui n'est pas une expression et garde
 * volontairement le chemin par le texte.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    Object *st = hc_new_stack("P"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    Object *b  = hc_new_button(c, "Ok"); hc_set_id(b, 7);
    Object *f  = hc_new_field(c, "Ch");
    hc_set_current_card(c);
    Object *d  = hc_new_button(c, "d");

    hc_set_script(d,
        "on mouseUp\n"
        "  put \"== la liste NUE, celle qui ne marchait pas ==\"\n"
        "  set the rect of card button id 7 to 1,2,30,40\n"
        "  put the rect of card button id 7\n"
        "  set the rect of button \"Ok\" to 5,6,50,60\n"
        "  put the rect of button \"Ok\"\n"
        "  set the rectangle of button \"Ok\" to 7,8,70,80\n"
        "  put the rect of button \"Ok\"\n"
        "  set the loc of button \"Ok\" to 200,150\n"
        "  put the loc of button \"Ok\"\n"
        "  set the topleft of button \"Ok\" to 10,20\n"
        "  put the topleft of button \"Ok\"\n"
        "  set the botright of button \"Ok\" to 90,100\n"
        "  put the botright of button \"Ok\"\n"

        "  put \"== chaque element est une EXPRESSION ==\"\n"
        "  put 11 into a\n"
        "  put 22 into bb\n"
        "  set the loc of button \"Ok\" to a,bb\n"
        "  put the loc of button \"Ok\"\n"
        "  set the loc of button \"Ok\" to a*2,bb+1\n"
        "  put the loc of button \"Ok\"\n"
        "  put \"33,44\" into p\n"
        "  set the loc of button \"Ok\" to item 1 of p, item 2 of p\n"
        "  put the loc of button \"Ok\"\n"

        "  put \"== ce qui marchait DEJA et ne doit pas bouger ==\"\n"
        "  set the rect of button \"Ok\" to \"3,4,33,44\"\n"
        "  put the rect of button \"Ok\"\n"
        "  put \"9,9,99,99\" into r\n"
        "  set the rect of button \"Ok\" to r\n"
        "  put the rect of button \"Ok\"\n"
        "  set the width of button \"Ok\" to 60\n"
        "  put the width of button \"Ok\"\n"

        "  put \"== une liste de NOMS NUS reste litterale ==\"\n"
        "  set the textStyle of card field \"Ch\" to bold,condense\n"
        "  put the textStyle of card field \"Ch\"\n"
        "  set the textStyle of card field \"Ch\" to plain\n"
        "  put the textStyle of card field \"Ch\"\n"
        "end mouseUp\n");
    hc_send(d, "mouseUp");

    printf("\n   bouton, en direct : %d,%d,%d,%d\n", b->x, b->y, b->w, b->h);
    printf("   champ,  en direct : style %d\n", f->textstyle);
    hc_free(st);
    return 0;
}
