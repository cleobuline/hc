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
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    /* ─── CHAQUE ECHEC DANS SON PROPRE GESTIONNAIRE ───────────────────────
     *
     * Ces quatre cas LEVENT desormais une erreur, et une erreur ARRETE le
     * gestionnaire. Les enchainer dans un seul, comme avant, faisait
     * disparaitre tout ce qui suit la premiere : mesure, quatre verifications
     * perdues d'un coup, dont celle de la variable qui se designe elle-meme.
     * Un harnais qui se coupe ne mesure plus rien.
     *
     * CE QUI A CHANGE, ET POURQUOI. « the short name of z », ou z contient du
     * texte ordinaire, rendait la chaine « short name of z ». Pas d'erreur :
     * la question rendue comme reponse. Le meme echo que « nExistePas(3) » et
     * que « owner of me » avant sa correction.
     *
     * La regle est desormais portee par la PROPRIETE, pas par la tete de la
     * cible : « short name » n'a de sens que sur un objet, donc l'absence
     * d'objet est une erreur ; « number of chars » compte du texte et continue
     * de le compter — c'est le bloc « ce qui ne doit PAS changer » ci-dessus
     * qui le tient. */
    puts("\n== une propriete d'OBJET sans objet : chacune dans son gestionnaire ==");
    {
        static const char *cas[] = {
            "un mot inconnu",
                "  put \"inconnu\" into z\n  put the short name of z\n",
            "une variable jamais posee",
                "  put the short name of jamaisPosee\n",
            "une variable qui se designe elle-meme",
                "  put \"o\" into o\n  put the short name of o\n",
            /* Celui-ci RESOUT, et c'est le point : le bouton id 2915 existe
             * dans cette pile. Il est ici pour montrer qu'une variable
             * contenant un vrai descripteur n'est pas emportee par la
             * correction — elle repond, comme avant. */
            "un VRAI descripteur dans une variable (doit repondre)",
                "  put \"card button id 2915\" into cycle\n"
                "  put the short name of cycle\n",
            NULL
        };
        for (int i = 0; cas[i]; i += 2) {
            char sc[400];
            snprintf(sc, sizeof sc, "on mouseUp\n%send mouseUp\n", cas[i + 1]);
            printf("   %s\n", cas[i]);
            hc_set_script(b, sc);
            hc_send(b, "mouseUp");
        }
    }

    /* Le garde-fou, isole lui aussi : la MEME cible, la MEME variable, une
     * propriete de TEXTE. Elle doit repondre, sinon la correction aurait
     * invente une erreur au lieu d'en reveler une. */
    puts("\n== la meme cible, mais une propriete de TEXTE : elle repond ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  put \"inconnu\" into z\n"
        "  put the number of chars of z\n"
        "  put the length of z\n"
        "  put the number of words of z\n"
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
