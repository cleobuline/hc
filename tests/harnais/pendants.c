/* Le noyau gardait des adresses d'objets LIBÉRÉS.
 *
 * object_gone protège remarquablement bien les pointeurs de l'interface : il
 * est appelé au seul endroit où un objet meurt, et Cocoa oublie tout ce qui le
 * concerne. Le noyau ne s'appliquait pas le même principe à ses PROPRES
 * références, une couche plus bas. La sélection de texte, le résultat de la
 * dernière recherche, la pile de navigation, la carte courante — toutes
 * retenaient un Object* sans en être propriétaires, et personne ne les
 * prévenait.
 *
 * Trois lignes de HyperTalk suffisaient. Mesuré sous AddressSanitizer avant
 * correction :
 *
 *   select ... / delete le champ / put the selection
 *       heap-use-after-free dans field_is_percard, via hc_field_text.
 *   find ... / delete le champ / put the foundField
 *       heap-use-after-free dans hc_describe.
 *   push card / go next / delete la carte empilée / pop card
 *       déréférencement direct d'une carte libérée dans v3_cmd_pop.
 *
 * Ce harnais vaut dans les DEUX constructions. Sans sanitizer il vérifie le
 * comportement — une sélection sur un champ mort est vide, un « pop » d'une
 * carte morte ne va nulle part. Sous ASan, la même suite devient un détecteur
 * de lecture après libération : c'est là qu'il attrape une récidive.
 *
 * Chaque cas repart d'une pile NEUVE : un état laissé par le cas précédent
 * masquerait le suivant. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   [%s]\n", t ? t : ""); }

static Object *st;

static void pile_neuve(void)
{
    if (st) hc_free(st);
    st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *a  = hc_new_card(st, bg, "A");
    hc_new_card(st, bg, "B");
    hc_set_current_card(a);
    Object *f = hc_new_field(a, "x");
    hc_set_field_text(f, "toto tata");
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);
    hc_do("get 1");     /* la banniere v3 sort ici, pas au milieu d'un releve */

    puts("=== la selection ne survit pas a son champ ===");
    pile_neuve();
    hc_do("select char 1 to 2 of card field \"x\"");
    hc_do("put the selection");
    hc_do("put the selectedChunk");
    hc_do("delete card field \"x\"");
    puts("   -- champ supprime --");
    hc_do("put the selection");
    hc_do("put the selectedChunk");
    hc_do("put the selectedField");

    puts("\n=== le resultat de la recherche non plus ===");
    pile_neuve();
    hc_do("find \"toto\"");
    hc_do("put the foundText");
    hc_do("put the foundField");
    hc_do("delete card field \"x\"");
    puts("   -- champ supprime --");
    hc_do("put the foundText");
    hc_do("put the foundField");
    hc_do("put the foundChunk");
    hc_do("put the foundLine");

    puts("\n=== une carte empilee puis supprimee sort de la pile de navigation ===");
    pile_neuve();
    hc_do("push card");
    hc_do("go next card");
    hc_do("put the short name of this card");
    hc_do("delete card \"A\"");
    puts("   -- carte \"A\" supprimee --");
    hc_do("pop card");
    hc_do("put the short name of this card");

    puts("\n=== la meme carte empilee DEUX fois part deux fois ===");
    pile_neuve();
    hc_do("push card");
    hc_do("push card");
    hc_do("go next card");
    hc_do("delete card \"A\"");
    hc_do("pop card");
    hc_do("pop card");
    hc_do("put the short name of this card");

    puts("\n=== supprimer le champ ne trouble pas ce qui l'entoure ===");
    pile_neuve();
    hc_do("select char 1 to 2 of card field \"x\"");
    hc_do("find \"tata\"");
    hc_do("push card");
    hc_do("delete card field \"x\"");
    puts("   -- champ supprime, carte vivante --");
    hc_do("put the short name of this card");
    hc_do("put the number of card fields");
    hc_do("put the selection & \"|\" & the foundText");

    puts("\n=== supprimer la CARTE emporte la selection de son champ ===");
    pile_neuve();
    hc_do("select char 1 to 2 of card field \"x\"");
    hc_do("go next card");
    hc_do("delete card \"A\"");
    puts("   -- carte \"A\" supprimee, avec son champ --");
    hc_do("put the selection");
    hc_do("put the selectedField");
    hc_do("put the short name of this card");

    hc_free(st);
    return 0;
}
