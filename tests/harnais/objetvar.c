/* Une VARIABLE qui contient un descripteur d'objet en designe un.
 *
 * C'est l'idiome de toute fonction utilisateur qui prend un objet :
 *
 *     function nomme o
 *       return the short name of o
 *     end nomme
 *     put nomme(the name of me)
 *
 * hct_resout ne connait que les noeuds OBJET. Devant un identificateur nu, il
 * rendait NULL ; le recours reconstituait le texte « the short name of o » ;
 * l'ancien evaluateur cherchait un objet NOMME « o », n'en trouvait pas ; et
 * la regle « identificateur inconnu = son propre nom » rendait le litteral
 * « short name of o ». SANS ERREUR — le script avait l'air de marcher, et
 * c'est le pire des cas.
 *
 * Mesure avant correction, avec un descripteur parfaitement forme dans la
 * variable (« button "Bouton" ») : le resultat etait le meme.
 *
 * Et ce chemin-la etait le SEUL, parmi toutes les formes d'appel de fonction
 * utilisateur, qui repartait vraiment dans l'ancien moteur — et a chaque
 * appel, pas une fois. Mesure sur vingt appels de nomme(the name of me) :
 *
 *     avant :  recours of short name 20, v1 terme 60, v1 fonction 60
 *     apres :  rien
 *
 * Ce harnais tient les deux choses : la resolution, et ce qui ne doit PAS
 * changer — un mot inconnu garde le chemin d'avant, « the length of s » reste
 * une longueur, et une variable qui se designe elle-meme ne boucle pas. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   > %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    Object *st = hc_new_stack("P"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "A");
    hc_set_current_card(c);
    Object *b  = hc_new_button(c, "Bouton"); hc_set_id(b, 2915);
    Object *f  = hc_new_field(c, "Champ");   hc_set_field_text(f, "contenu");
    hc_new_button(bg, "DuFond");

    hc_set_script(st,
        "function nomme o\n  return the short name of o\nend nomme\n"
        "function largeur o\n  return the width of o\nend largeur\n"
        "function idde o\n  return the id of o\nend idde\n");

    hc_set_script(b,
        "on mouseUp\n"
        "  put \"== un descripteur dans une variable ==\"\n"
        "  put the name of me into d\n"
        "  put d\n"
        "  put the short name of d\n"
        "  put the id of d\n"
        "  put the width of d\n"
        "  put \"card button id 2915\" into d2\n"
        "  put the short name of d2\n"
        "  put \"card field \" & quote & \"Champ\" & quote into d3\n"
        "  put the short name of d3\n"
        "  put \"bg button \" & quote & \"DuFond\" & quote into d4\n"
        "  put the short name of d4\n"

        "  put \"== par une fonction utilisateur ==\"\n"
        "  put nomme(the name of me)\n"
        "  put largeur(the name of me)\n"
        "  put idde(the name of me)\n"
        "  put nomme(\"card field \" & quote & \"Champ\" & quote)\n"

        "  put \"== ce qui ne doit PAS changer ==\"\n"
        "  put \"abc\" into s\n"
        "  put the length of s\n"
        "  put the number of chars of s\n"
        "  put \"a,b,c\" into v\n"
        "  put the number of items of v\n"
        "  put item 2 of v\n"
        "  put \"== un mot inconnu garde le chemin d'avant ==\"\n"
        "  put \"inconnu\" into z\n"
        "  put the short name of z\n"
        "  put the short name of jamaisPosee\n"
        "  put \"== une variable qui se designe elle-meme ne boucle pas ==\"\n"
        "  put \"o\" into o\n"
        "  put the short name of o\n"
        "  put \"card button id 2915\" into cycle\n"
        "  put the short name of cycle\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== et le relevé : plus rien ne repart dans l'ancien moteur ==");
    hc_set_script(b,
        "on mouseUp\n  repeat with i = 1 to 20\n"
        "    put nomme(the name of me) into r\n  end repeat\n"
        "  put r\nend mouseUp\n");
    hc_v3_bilan_remise_a_zero();
    hc_send(b, "mouseUp");
    hc_v3_bilan();

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
