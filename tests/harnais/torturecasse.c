/* LE GARDE-FOU DE LA PILE DE TORTURE MORD-IL ?
 *
 * Ce harnais monte la pile EXPRÈS DE TRAVERS et vérifie que le banc refuse de
 * tourner en le disant, au lieu de produire des échecs qui n'en sont pas.
 *
 * Il existe parce que ces échecs-là ont eu lieu. La pile de torture, refaite à
 * la main dans HyperCard, a rendu quatre ÉCHECS qui contredisaient tous quatre
 * des mesures déjà prises du même côté : les cartes n'étaient pas nommées, et
 * « go to card "Deux" » sur une carte absente NE LÈVE PAS chez HyperCard — il
 * reste sur place, en silence. Toute la préparation écrivait donc dans la même
 * carte, chaque ligne écrasant la précédente.
 *
 * UNE AUTO-VÉRIFICATION NE SE CROIT PAS, ELLE SE JOUE SUR UNE PILE CASSÉE.
 * La première version du garde-fou ne mordait pas du tout, et trois causes s'y
 * étaient empilées :
 *
 *   · son appel n'avait jamais été inséré dans « on mouseUp » — le remplacement
 *     de texte n'avait rien trouvé, et personne ne l'avait vérifié ;
 *   · il testait l'existence d'une carte par « go to card », qui LÈVE dans HC :
 *     l'erreur emportait la vérification avant qu'elle note le manque. C'est
 *     « there is a card » qu'il faut, qui répond sans supposer que oui ;
 *   · une fonction qui lève rend VIDE, et « vide is false » ne mord pas. D'où
 *     « is not true » chez l'appelant : on n'accepte que le vrai explicite.
 *
 * Trois façons de ne pas mordre, pour un garde-fou de dix lignes. Aucune ne se
 * voyait sur la pile BIEN montée — elle passait, forcément.
 *
 * La pile montée ici est fausse de trois façons à la fois : cinq cartes dont
 * aucune n'est nommée, les champs de carte dans le mauvais ordre (B avant A),
 * et pas de dontSearch sur le rapport. Le garde-fou doit les nommer toutes.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("%s\n", t);
    else if (k == HC_ERR) printf("[ERR] %s\n", t);
}

static char *lire(const char *chemin)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) { perror(chemin); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *t = malloc((size_t)n + 1);
    if (!t) { fclose(f); return NULL; }
    if (fread(t, 1, (size_t)n, f) != (size_t)n) { free(t); fclose(f); return NULL; }
    t[n] = 0; fclose(f);
    return t;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s bouton.txt pile.txt\n", argv[0]);
        return 2;
    }
    char *src_bouton = lire(argv[1]);
    char *src_pile   = lire(argv[2]);
    if (!src_bouton || !src_pile) { free(src_bouton); free(src_pile); return 2; }

    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Torture");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    hc_new_field(bg, "T");

    /* CINQ cartes, et AUCUNE nommée — c'est l'état exact de la pile qui a
     * produit les quatre faux échecs. */
    Object *c1 = hc_new_card(st, bg, NULL);
    for (int i = 0; i < 4; i++) hc_new_card(st, bg, NULL);

    /* B AVANT A : l'ordre de création est le numéro, et les sections 3, 4 et 7
     * en dépendent. Le garde-fou doit le dire. */
    hc_new_field(c1, "B");
    hc_new_field(c1, "A");
    hc_new_field(c1, "R");          /* et pas de dontSearch */

    Object *b = hc_new_button(c1, "TORTURE");
    hc_set_script(st, src_pile);
    hc_set_script(b, src_bouton);
    hc_set_current_card(c1);

    hc_send(b, "mouseUp");

    hc_free(st); free(src_bouton); free(src_pile);
    return 0;
}
