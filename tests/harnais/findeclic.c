/* La fin automatique d'un clic s'applique a la carte DU CLIC.
 *
 * Le pointeur du bouton etait bien sauvegarde avant l'envoi de mouseUp — la
 * vue s'en garde tres bien — mais son CONTEXTE DE CARTE ne l'etait pas. Elle
 * lisait hc_current_card() APRES le gestionnaire, et un gestionnaire n'a rien
 * d'exceptionnel a changer de carte :
 *
 *     on mouseUp
 *       go next card
 *     end mouseUp
 *
 * Pour un bouton de fond a sharedHilite faux, l'allumage se range dans la
 * table de la carte : l'extinction partait donc dans celle de la carte
 * D'ARRIVEE. Avec « go to card 1 of stack "B" », on inscrivait dans une carte
 * de B l'identifiant d'un bouton de A.
 *
 * La regle est descendue dans le noyau — c'est du modele, pas de l'affichage —
 * ou elle devient verifiable sans AppKit. C'est tout l'interet : la vue ne se
 * compile pas ici, mais la decision, si. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Le style d'un bouton est un champ public : on l'ecrit directement, comme
 * le fait l'Info de bouton. */
static void style(Object *o, const char *s)
{ free(o->style); o->style = s ? strdup(s) : NULL; }

static void ligne(HcLineKind k, int d, const char *t)
{ (void)k; (void)d; (void)t; }

static Object *stA, *bgA, *cA1, *cA2, *btn;
static Object *stB, *cB1;

/* Ce qu'on observe : l'allumage du bouton VU DEPUIS chaque carte. Pour un
 * bouton de fond non partage, c'est bien deux reponses differentes. */
static void etat(const char *quand)
{
    printf("   %-34s A1=%d  A2=%d  B1=%d\n", quand,
           hc_hilite_of(btn, cA1), hc_hilite_of(btn, cA2), hc_hilite_of(btn, cB1));
}

static void monte(int partage)
{
    stA = hc_new_stack("A");  hc_register_stack(stA);
    bgA = hc_new_background(stA, "F");
    cA1 = hc_new_card(stA, bgA, "A1");
    cA2 = hc_new_card(stA, bgA, "A2");
    stB = hc_new_stack("B");  hc_register_stack(stB);
    Object *bgB = hc_new_background(stB, "G");
    cB1 = hc_new_card(stB, bgB, "B1");

    btn = hc_new_button(bgA, "b");     /* bouton de FOND */
    btn->autohilite = 1;
    btn->shared_hilite = partage;
    hc_set_current_card(cA1);
}

static void demonte(void)
{ hc_unregister_stack(stA); hc_unregister_stack(stB); hc_free(stA); hc_free(stB); }

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    puts("=== bouton de fond, sharedHilite FAUX : l'allumage vit dans la carte ===");
    monte(0);
    printf("   hc_hilite_par_carte : %d (1 = oui)\n", hc_hilite_par_carte(btn));
    hc_set_hilite(btn, cA1, 1);            /* ce que fait mouseDown */
    etat("apres le mouseDown sur A1");

    /* Le gestionnaire est alle ailleurs. C'est TOUT le defaut. */
    hc_set_current_card(cA2);
    hc_fin_de_clic(btn, cA1);              /* la carte du CLIC, pas la courante */
    etat("fin de clic, carte du clic = A1");

    puts("\n=== et vers une AUTRE PILE ===");
    hc_set_hilite(btn, cA1, 1);
    hc_set_current_card(cB1);
    hc_fin_de_clic(btn, cA1);
    etat("fin de clic depuis B1");
    printf("   la carte B1 porte-t-elle un etat de bouton ? %s\n",
           cB1->nbghilites ? "OUI (defaut)" : "non");
    demonte();

    puts("\n=== sharedHilite VRAI : l'allumage vit sur le bouton ===");
    monte(1);
    printf("   hc_hilite_par_carte : %d (0 = non)\n", hc_hilite_par_carte(btn));
    hc_set_hilite(btn, cA1, 1);
    etat("apres le mouseDown sur A1");
    hc_set_current_card(cA2);
    hc_fin_de_clic(btn, cA1);
    etat("fin de clic, carte du clic = A1");
    demonte();

    puts("\n=== la carte du clic a disparu : on ne retombe PAS sur la courante ===");
    monte(0);
    hc_set_hilite(btn, cA1, 1);
    etat("avant");
    hc_set_current_card(cA2);
    hc_delete_card(cA1);
    cA1 = NULL;
    hc_fin_de_clic(btn, NULL);             /* ce que la vue passe apres l'oubli */
    printf("   %-34s A2=%d  (doit rester 0)\n", "fin de clic sans carte",
           hc_hilite_of(btn, cA2));
    demonte();

    puts("\n=== la case a cocher bascule, sur la carte du clic ===");
    monte(0);
    style(btn, "checkBox");
    hc_set_current_card(cA2);
    hc_fin_de_clic(btn, cA1);
    etat("premier clic sur A1");
    hc_fin_de_clic(btn, cA1);
    etat("deuxieme clic sur A1");
    demonte();

    /* CETTE SECTION A CHANGE DE SUJET SANS CHANGER DE QUESTION.
     *
     * Elle s'appelait « le radio eteint ses voisins, carte ET fond ». Les
     * deux moities de ce titre sont tombees le jour ou famille 0 a cesse de
     * grouper :
     *
     *   - sans famille, un radio n'eteint plus personne — c'est la regle
     *     d'HyperCard, et c'etait le choix a faire ;
     *   - la famille NE TRAVERSE PAS LES COUCHES : deux boutons de meme
     *     numero, l'un sur la carte et l'autre sur le fond, sont deux
     *     groupes distincts. Egalement HyperCard, et deja documente dans
     *     eteint_la_famille.
     *
     * Ce que cette section verifie reste EXACTEMENT le meme : que
     * l'extinction s'inscrit dans la table de la carte CLIQUEE et non dans
     * celle de la carte courante. On lui redonne donc un sujet — une
     * famille — plutot que de la supprimer : la question qu'elle pose
     * n'avait rien a voir avec le mecanisme qui a change.
     *
     * b et r2 sont tous deux sur le FOND, donc du meme groupe. r3 est sur la
     * CARTE avec le meme numero : il ne doit pas bouger, et c'est lui qui
     * tient la regle des couches. */
    puts("\n=== le radio eteint ses freres de FAMILLE, sur la carte du clic ===");
    monte(0);
    style(btn, "radioButton");
    Object *r2 = hc_new_button(bgA, "r2");   style(r2, "radioButton");
    Object *r3 = hc_new_button(cA1, "r3");   style(r3, "radioButton");
    hc_set_family(btn, 1);
    hc_set_family(r2, 1);
    hc_set_family(r3, 1);          /* meme numero, AUTRE couche */
    hc_set_hilite(r2, cA1, 1);
    hc_set_hilite(r3, cA1, 1);
    printf("   avant : b=%d r2=%d r3=%d\n", hc_hilite_of(btn, cA1),
           hc_hilite_of(r2, cA1), hc_hilite_of(r3, cA1));
    hc_set_current_card(cA2);
    hc_fin_de_clic(btn, cA1);
    printf("   apres : b=%d r2=%d r3=%d   (sur A1)\n", hc_hilite_of(btn, cA1),
           hc_hilite_of(r2, cA1), hc_hilite_of(r3, cA1));
    printf("   (r3 est sur la CARTE : meme numero, autre groupe)\n");
    printf("   et sur A2, ou l'on se trouve : b=%d r2=%d\n",
           hc_hilite_of(btn, cA2), hc_hilite_of(r2, cA2));
    demonte();

    puts("\n=== un RADIO dont le gestionnaire saute dans une autre pile ===");
    /* Le cas le plus net : un allumage, donc une ENTREE creee. Une extinction
     * n'en cree pas, si bien qu'elle passait inapercue meme quand elle partait
     * dans la mauvaise carte. */
    monte(0);
    style(btn, "radioButton");
    hc_set_current_card(cB1);              /* le gestionnaire a fait « go to stack B » */
    hc_fin_de_clic(btn, cA1);
    etat("fin de clic, carte du clic = A1");
    printf("   la carte B1 de l'autre pile porte-t-elle un etat ? %s\n",
           cB1->nbghilites ? "OUI (defaut)" : "non");
    printf("   la carte A1, celle du clic, en porte-t-elle un ? %s\n",
           cA1->nbghilites ? "oui" : "NON (defaut)");
    demonte();

    puts("\n=== un bouton mort ne fait rien, et ne plante pas ===");
    monte(0);
    Object *mort = hc_new_button(cA1, "mort");
    hc_delete_part(mort);
    hc_fin_de_clic(mort, cA1);
    puts("   (aucun effet, aucun plantage)");
    hc_fin_de_clic(NULL, NULL);
    demonte();

    return 0;
}
