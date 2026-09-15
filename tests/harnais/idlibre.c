/* Le plus petit identifiant LIBRE, quand le compteur est epuise.
 *
 * Le compteur monte ; quand il touche HC_ID_MAX, la creation suivante doit
 * trouver un trou dans les numeros deja pris. La version d'avant essayait 1,
 * parcourait toute la pile, essayait 2, reparcourait toute la pile — N essais
 * fois N objets. Mesure, avec le compteur epuise :
 *
 *       500 objets   0,04 ms par creation
 *      1000 objets   0,2  ms
 *      2000 objets   1,1  ms
 *      4000 objets   4,3  ms        x4 quand N double : du N carre
 *
 * A 10 000 objets on etait a ~27 ms par objet, a 50 000 l'application se fige.
 * Aucun fichier malveillant n'est necessaire — une pile assez grosse suffit.
 * Apres correction, une seule passe et un tri : 4000 objets passent a 0,1 ms,
 * et 20 000 tiennent en 0,4 ms.
 *
 * La VITESSE ne se versionne pas — elle depend de la machine. Ce harnais tient
 * donc la JUSTESSE, qui est ce que la correction risquait de perdre : le
 * numero rendu doit etre le plus petit libre, et il doit etre libre. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

static int occupe(Object *pile, int id)
{
    if (pile->id == id) return 1;
    for (int i = 0; i < pile->nparts; i++) {
        if (pile->parts[i]->id == id) return 1;
        for (int j = 0; j < pile->parts[i]->nparts; j++)
            if (pile->parts[i]->parts[j]->id == id) return 1;
    }
    return 0;
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "U");
    hc_set_current_card(c);

    /* Des numeros bas occupes, puis des TROUS qu'on creuse expres. */
    Object *b[12];
    for (int i = 0; i < 12; i++) { b[i] = hc_new_button(c, "b"); hc_set_id(b[i], 100 + i); }
    hc_set_id(b[3], 7);          /* 7 pris, 8..99 libres */
    hc_set_id(b[4], 8);

    printf("== les numeros en place ==\n   ");
    for (int id = 1; id <= 12; id++) printf("%d%s ", id, occupe(st, id) ? "*" : "");
    printf("\n   (* = pris)\n");

    /* On epuise le compteur : la creation suivante devra chercher. */
    Object *x = hc_new_button(c, "x");
    hc_set_id(x, HC_ID_MAX - 1);

    puts("\n== les six creations suivantes prennent les plus petits trous ==");
    for (int k = 0; k < 6; k++) {
        Object *n = hc_new_button(c, "n");
        printf("   creation %d -> id %d\n", k + 1, n->id);
    }

    /* Aucun doublon ne doit etre ne en chemin. */
    puts("\n== et aucun numero n'est porte deux fois ==");
    {
        int doublons = 0;
        for (int i = 0; i < c->nparts; i++)
            for (int j = i + 1; j < c->nparts; j++)
                if (c->parts[i]->id == c->parts[j]->id) doublons++;
        printf("   doublons : %d\n", doublons);
    }

    hc_free(st);
    return 0;
}
