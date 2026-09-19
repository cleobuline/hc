/* LA PORTEE DE L'EXTINCTION DOIT SUIVRE CELLE DE L'ALLUMAGE.
 *
 * SIGNALE PAR UN AUDIT EXTERIEUR, reproduit. Un bouton de FOND a deux facons
 * de s'allumer, et c'est tout le probleme :
 *
 *   sharedHilite VRAI  -> allume sur TOUTES les cartes du fond a la fois ;
 *   sharedHilite FAUX  -> un etat PAR CARTE, range dans la carte.
 *
 * L'extinction de famille, elle, ne connaissait qu'UNE carte. Mesure, avec R1
 * partage et R2 par carte, tous deux de famille 3 :
 *
 *     R2 allume sur la carte B
 *     on se place sur A, on allume R1
 *     -> sur A : R1=1 R2=0        juste
 *     -> sur B : R1=1 R2=1        DEUX de la famille 3 allumes
 *
 * R1 s'allume partout puisqu'il est partage ; R2 n'etait eteint que sur la
 * carte ou l'on se trouve. La famille etait violee sur toutes les autres, et
 * l'utilisateur ne s'en apercevait qu'en y allant — le pire moment, et le
 * plus difficile a rattacher a sa cause.
 *
 * LE MEME DEFAUT, UN CRAN PLUS LOIN : hc_set_family ne regardait lui aussi
 * que la carte courante —
 *
 *     if (famille > 0 && hc_hilite_of(btn, NULL)) eteint_la_famille(btn, NULL);
 *
 * — alors qu'un bouton a etat par carte peut etre ETEINT ici et ALLUME sur
 * cinq autres. Lui donner une famille le faisait entrer dans un groupe deja
 * pourvu, sur chacune de ces cinq cartes, sans rien eteindre. C'est
 * exactement l'etat « deux allumes » que le panneau Infos bouton avait ete
 * corrige pour ne pas produire — la correction d'alors ne voyait qu'une
 * carte.
 *
 * LA REGLE : si le bouton qu'on allume s'allume PARTOUT, ses freres a etat
 * par carte s'eteignent partout aussi. S'il ne s'allume que sur une carte,
 * eteindre sur cette carte suffit — et c'est le cas courant, qui ne coute
 * rien de plus qu'avant.
 *
 * Ce harnais tient les quatre combinaisons de partage, les deux portes
 * (l'allumage et la pose de famille), et le fait que les cartes d'un AUTRE
 * fond ne sont pas touchees.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *st, *fond, *cA, *cB, *cC, *r1, *r2;

static void etat(const char *quoi)
{
    printf("      %-30s A: R1=%d R2=%d   B: R1=%d R2=%d   C: R1=%d R2=%d\n",
           quoi,
           hc_hilite_of(r1, cA), hc_hilite_of(r2, cA),
           hc_hilite_of(r1, cB), hc_hilite_of(r2, cB),
           hc_hilite_of(r1, cC), hc_hilite_of(r2, cC));
}

/* Deux boutons de fond, dont on choisit le partage a chaque essai. */
static void monte(int partage1, int partage2)
{
    st   = hc_new_stack("Pile");
    fond = hc_new_background(st, "Fond");
    cA = hc_new_card(st, fond, "A");
    cB = hc_new_card(st, fond, "B");
    cC = hc_new_card(st, fond, "C");
    hc_register_stack(st);
    r1 = hc_new_button(fond, "R1"); r1->shared_hilite = partage1;
    r2 = hc_new_button(fond, "R2"); r2->shared_hilite = partage2;
    hc_set_family(r1, 3);
    hc_set_family(r2, 3);
    hc_set_current_card(cA);
}

static void demonte(void) { hc_unregister_stack(st); hc_free(st); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    puts("== 1. LE CAS SIGNALE : R1 partage, R2 par carte ==");
    monte(1, 0);
    hc_set_hilite(r2, cB, 1);
    etat("R2 allume sur B");
    hc_set_current_card(cA);
    hc_set_hilite(r1, cA, 1);
    etat("puis R1 allume depuis A");
    printf("      sur B, deux de la famille 3 ? %s\n",
           (hc_hilite_of(r1, cB) && hc_hilite_of(r2, cB)) ? "OUI" : "non");
    demonte();

    puts("\n== 2. les trois autres combinaisons de partage ==");
    monte(0, 0);           /* les deux par carte : chacun chez soi */
    hc_set_hilite(r2, cB, 1);
    hc_set_hilite(r1, cA, 1);
    etat("les deux par carte");
    demonte();

    monte(1, 1);           /* les deux partages : un seul etat chacun */
    hc_set_hilite(r2, cB, 1);
    hc_set_hilite(r1, cA, 1);
    etat("les deux partages");
    demonte();

    monte(0, 1);           /* R1 par carte, R2 partage */
    hc_set_hilite(r2, cB, 1);
    hc_set_hilite(r1, cA, 1);
    etat("R1 par carte, R2 partage");
    demonte();

    puts("\n== 3. L'AUTRE PORTE : poser la famille ==");
    /* R2 est allume sur B et sur C, eteint sur A ou l'on se trouve. R1 est
     * allume partout. Donner la famille 3 a R2 doit eteindre R1 — ou plutot
     * eteindre R2 sur les cartes ou R1 regne. */
    monte(1, 0);
    hc_set_family(r2, 0);              /* R2 sort de la famille */
    hc_set_hilite(r2, cB, 1);
    hc_set_hilite(r2, cC, 1);
    hc_set_current_card(cA);
    hc_set_hilite(r1, cA, 1);          /* R1, partage, s'allume partout */
    etat("avant la pose de famille");
    hc_set_family(r2, 3);              /* R2 entre dans la famille 3 */
    /* LE RESULTAT SURPREND, ET IL EST JUSTE. R1 s'eteint PARTOUT, y compris
     * sur la carte A ou R2 n'etait pas allume — parce que R1 est partage, et
     * qu'un bouton partage n'a qu'un seul etat pour tout le fond : on ne peut
     * pas l'eteindre sur B sans l'eteindre sur A. C'est la nature de
     * sharedHilite, pas un effet de bord de la correction.
     *
     * La regle appliquee est celle que hc_set_family portait deja : le
     * nouveau venu l'emporte. Ce qui change est qu'elle vaut maintenant sur
     * CHAQUE carte ou le nouveau venu est allume, et non plus sur la seule
     * carte courante. */
    etat("apres set the family de R2");
    printf("      une carte avec deux allumes ? %s\n",
           ((hc_hilite_of(r1,cA) && hc_hilite_of(r2,cA)) ||
            (hc_hilite_of(r1,cB) && hc_hilite_of(r2,cB)) ||
            (hc_hilite_of(r1,cC) && hc_hilite_of(r2,cC))) ? "OUI" : "non");
    demonte();

    puts("\n== 4. un AUTRE fond n'est pas touche ==");
    monte(1, 0);
    {
        Object *f2 = hc_new_background(st, "Autre");
        Object *d1 = hc_new_card(st, f2, "D");
        Object *e1 = hc_new_button(f2, "E1");
        hc_set_family(e1, 3);          /* meme numero, autre fond */
        hc_set_hilite(e1, d1, 1);
        hc_set_current_card(cA);
        hc_set_hilite(r1, cA, 1);      /* allume partout dans « Fond » */
        printf("      E1, sur l'autre fond, reste allume ? %s\n",
               hc_hilite_of(e1, d1) ? "oui" : "NON");
    }
    demonte();

    puts("\n== 5. et les familles DIFFERENTES ne se touchent pas ==");
    monte(1, 0);
    hc_set_family(r2, 4);
    hc_set_hilite(r2, cB, 1);
    hc_set_current_card(cA);
    hc_set_hilite(r1, cA, 1);
    etat("R1 famille 3, R2 famille 4");
    demonte();

    return 0;
}
