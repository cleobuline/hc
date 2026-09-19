/* « THE TARGET » NOMME L'OBJET ; « TARGET » EN DONNE LE CONTENU.
 *
 * HyperTalk fait cette distinction expres, et elle est reelle : « target » est
 * un CONTENEUR — on peut y lire et y ecrire comme dans un champ —, tandis que
 * « the target » NOMME l'objet qui a recu le message d'origine.
 *
 * Chez nous les deux rendaient la meme chose. Dans un gestionnaire de champ,
 * « put the target » donnait le TEXTE du champ au lieu d'un descripteur :
 *
 *   the target             -> contenu du champ      FAUX
 *   target                 -> contenu du champ      juste
 *   the name of the target -> card field "Champ"    juste
 *
 * Le contournement existait, mais un script ecrit pour HyperCard ne l'emploie
 * pas : il ecrit « the target » et attend un nom. C'est une incompatibilite au
 * centre de la semantique des messages, pas un detail d'affichage.
 *
 * L'ANALYSEUR GARDAIT DEJA LA DISTINCTION. n->article vaut 1 quand « the »
 * etait la — seule l'evaluation la perdait. La correction ne touche donc ni a
 * la grammaire ni au modele : elle lit un drapeau qui etait pose et qu'on
 * ignorait. C'est ce qui la rend sure.
 *
 * Ce harnais tient :
 *
 *   1. sur un CHAMP, les deux formes different — c'est le cas qui revelait le
 *      defaut, le contenu ne ressemblant pas a un descripteur ;
 *   2. sur un BOUTON aussi : « B » n'est pas « card button "B" ». Sans ce
 *      point, un bouton dont le nom est vide passerait le test par hasard ;
 *   3. le descripteur rendu SE RE-RESOUT — c'est tout ce qu'on demande a un
 *      nom d'objet, et c'est ce qui distingue un vrai descripteur d'une
 *      chaine qui y ressemble ;
 *   4. « target » reste un CONTENEUR : on peut y ecrire ;
 *   5. « me » n'est pas touche — il s'ecrit toujours nu, « the me » n'existe
 *      pas, et la regle ne doit pas deborder sur lui.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);

    Object *f = hc_new_field(c, "Champ");
    hc_set_field_text(f, "contenu du champ");
    Object *b = hc_new_button(c, "B");

    /* Le gestionnaire est sur la CARTE : le message part vers le champ, ne
     * trouve pas preneur, et remonte. « the target » doit donc nommer le
     * CHAMP — celui qui a recu le message d'origine — et non la carte qui le
     * traite. C'est toute la raison d'etre de target face a me. */
    hc_set_script(c,
        "on essai\n"
        "  put \"the target : \" & the target\n"
        "  put \"target      : \" & target\n"
        "  put \"me          : \" & the name of me\n"
        "end essai\n");

    puts("== 1. message envoye au CHAMP, gestionnaire sur la CARTE ==");
    hc_send(f, "essai");

    puts("\n== 2. message envoye au BOUTON ==");
    hc_send(b, "essai");

    puts("\n== 3. le descripteur rendu se RE-RESOUT ==");
    /* Un nom d'objet qui ne se relit pas n'est pas un nom d'objet : c'est une
     * chaine qui en a l'air. On le repasse donc au resolveur. */
    hc_set_script(c,
        "on essai2\n"
        "  put the target into d\n"
        /* Dans une variable, le descripteur reste du TEXTE — « put d » rend la
         * chaine, et c'est juste : HyperTalk ne resout pas une variable par
         * surprise. Ce qui compte est qu'il se re-resolve la ou l'on ATTEND
         * une reference d'objet, c'est-a-dire derriere une propriete. Les deux
         * lignes suivantes disent chacune l'une des deux moities. */
        "  put \"la variable contient : \" & d\n"
        "  put \"et elle designe l'objet d'id \" & the id of d\n"
        "  put \"dont le contenu est : \" & the text of d\n"
        "end essai2\n");
    hc_send(f, "essai2");

    puts("\n== 4. « target » reste un CONTENEUR : on peut y ecrire ==");
    hc_set_script(c,
        "on essai3\n"
        "  put \"ecrit par le script\" into target\n"
        "end essai3\n");
    hc_send(f, "essai3");
    printf("      le champ contient : [%s]\n", hc_field_text(f));

    puts("\n== 5. « me » n'est pas touche ==");
    /* Le gestionnaire est sur le CHAMP : me et target designent alors le meme
     * objet, mais « me » garde sa regle — il rend le CONTENU, sans article
     * possible. */
    hc_set_field_text(f, "contenu du champ");
    hc_set_script(f,
        "on essai4\n"
        "  put \"me          : \" & me\n"
        "  put \"the name of me : \" & the name of me\n"
        "  put \"the target  : \" & the target\n"
        "end essai4\n");
    hc_send(f, "essai4");

    hc_free(st);
    return 0;
}
