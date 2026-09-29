/* L'INTERLIGNE PAR DEFAUT D'UN CHAMP : quatre tiers du corps, TRONQUES.
 *
 * hc_text_height rendait « (corps * 4 + 1) / 3 », c'est-a-dire l'arrondi au plus
 * proche, sous un commentaire qui annoncait « arrondi comme HyperCard ». Les
 * deux formules ne different que pour les corps dont le quadruple n'est pas
 * divisible par trois, ce qui est un corps sur trois : la faute pouvait vivre
 * longtemps sans se montrer, et AUCUN temoin ne l'exercait.
 *
 * LA MESURE VIENT DES PILES D'APPLE, corps par corps, sur les 140 parts de
 * « Decouvrir HyperCard », « 3D Parametric Equations » et TEST3 :
 *
 *     corps 9  -> 12 (x2), 13 (x6), 16 (x58)
 *     corps 10 -> 13 (x6), 16 (x2)
 *     corps 12 -> 16 (x54)
 *     corps 14 -> 18 (x12)
 *
 * Le corps 14 est le SEUL cas ou les deux formules se separent : troncature 18,
 * arrondi 19. Douze parts disent 18, aucune ne dit 19.
 *
 * Les valeurs qui s'ecartent du rapport de quatre tiers — 9 -> 16, cinquante-huit
 * fois — sont des choix d'auteur et non des defauts : un interligne genereux sur
 * un petit corps. Elles ne comptent pas contre la regle, et c'est pourquoi le
 * tableau ci-dessous ne retient que les corps dont la valeur mesuree SUIT le
 * rapport.
 *
 * LE CONTROLE POSITIF est dans l'affichage lui-meme : les deux formules sont
 * ecrites cote a cote pour chaque corps. Un temoin qui ne montrerait que le
 * resultat laisserait croire qu'il mesure quelque chose la ou les deux calculs
 * tombent d'accord — ce qui est le cas de deux corps sur trois.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG || k == HC_ERR) printf("      %s\n", t ? t : ""); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);

    /* Ce qu'Apple a stocke, pour les corps dont la valeur suit le rapport. */
    static const struct { int corps, mesure; const char *ou; } APPLE[] = {
        {  9, 12, "3D Parametric (2 parts)"        },
        { 10, 13, "Decouvrir HyperCard (6 parts)"  },
        { 12, 16, "les trois piles (54 parts)"     },
        { 14, 18, "Decouvrir HyperCard (12 parts)" },
    };

    puts("== l'interligne automatique, contre ce qu'Apple a stocke ==");
    puts("  corps   le notre   tronque   arrondi   mesure   ce qu'Apple ecrit");
    int fautes = 0;
    for (unsigned i = 0; i < sizeof APPLE / sizeof *APPLE; i++) {
        Object *f = hc_new_field(c, "x");
        f->textsize   = APPLE[i].corps;
        f->textheight = 0;                    /* automatique */
        int n = hc_text_height(f);
        int tronque = APPLE[i].corps * 4 / 3;
        int arrondi = (APPLE[i].corps * 4 + 1) / 3;
        printf("  %5d %10d %9d %9d %8d   %s%s\n",
               APPLE[i].corps, n, tronque, arrondi, APPLE[i].mesure,
               APPLE[i].ou,
               tronque == arrondi ? "   (les deux formules sont d'accord)" : "");
        if (n != APPLE[i].mesure) fautes++;
    }
    printf("  fautes : %d\n", fautes);

    /* UN INTERLIGNE POSE GAGNE TOUJOURS, et c'est la moitie de la propriete :
     * zero veut dire « automatique », tout le reste est une consigne. */
    puts("\n== un interligne pose l'emporte sur le calcul ==");
    {
        Object *f = hc_new_field(c, "pose");
        f->textsize = 14;
        f->textheight = 0;
        printf("  corps 14, interligne 0  -> %d  (automatique)\n", hc_text_height(f));
        f->textheight = 30;
        printf("  corps 14, interligne 30 -> %d  (pose)\n", hc_text_height(f));
        f->textheight = 0;
        printf("  remis a 0               -> %d  (de nouveau automatique)\n",
               hc_text_height(f));
    }

    /* Et par le SCRIPT, qui est la porte que voit l'utilisatrice. */
    puts("\n== « the textHeight of » rend la meme chose ==");
    {
        Object *f = hc_new_field(c, "Body");
        f->textsize = 14;
        f->textheight = 0;
        Object *b = hc_new_button(c, "pilote");
        /* 25 et non 18 : poser la valeur que le calcul rend deja ne prouverait
         * rien — c'est le genre de temoin qui passe quoi qu'il arrive. */
        hc_set_script(b, "on t\n  put the textHeight of card field \"Body\"\n"
                         "  set the textHeight of card field \"Body\" to 25\n"
                         "  put the textHeight of card field \"Body\"\n"
                         "  set the textHeight of card field \"Body\" to 0\n"
                         "  put the textHeight of card field \"Body\"\n"
                         "  put the textSize of card field \"Body\"\nend t\n");
        hc_send(b, "t");
    }

    hc_free(st);
    return 0;
}
