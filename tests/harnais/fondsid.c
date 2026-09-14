/* Une carte retrouve SON fond, même si deux fonds portent le même nom.
 *
 * Les fonds ont de vrais identifiants depuis longtemps, mais une carte les
 * référençait par leur NOM :
 *
 *     card "B" background "commun"
 *
 * et la lecture prenait le PREMIER fond de ce nom. Rien n'interdit deux fonds
 * homonymes — le modèle ne l'a jamais interdit —, si bien que deux cartes
 * attachées à deux fonds différents se retrouvaient toutes deux sur le
 * premier après un aller-retour.
 *
 * Mesuré avant correction : la seconde carte perdait son fond ET tout son
 * contenu de fond, sans un mot. C'est une perte de données, pas un détail de
 * format.
 *
 * L'id est maintenant écrit sur la ligne de carte et lu en premier. Le nom
 * RESTE écrit : un binaire plus ancien continue de lire ces fichiers, et il
 * retrouvera le bon fond dans le cas courant où les noms sont distincts. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

#define FIC "/tmp/hc_fondsid.stack"

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)k; (void)d; (void)t; }

static Object *carte(Object *st, const char *nom)
{
    for (int i = 0; i < st->nparts; i++) {
        Object *c = st->parts[i];
        if (c->type == OBJ_CARD && c->name && strcmp(c->name, nom) == 0) return c;
    }
    return NULL;
}

/* Ce que la carte VOIT de son fond : c'est la conséquence observable, et
 * c'est elle qui compte pour l'utilisateur — pas le numéro d'identifiant. */
static void ce_que_voit(Object *st, const char *nom)
{
    Object *c = carte(st, nom);
    printf("   carte %-3s voit :", nom);
    if (!c || !c->bg) { printf(" (aucun fond)\n"); return; }
    for (int j = 0; j < c->bg->nparts; j++) printf(" %s", c->bg->parts[j]->name);
    printf("\n");
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);

    puts("=== deux fonds du MÊME nom ===");
    {
        Object *st = hc_new_stack("P");
        Object *b1 = hc_new_background(st, "commun");
        Object *b2 = hc_new_background(st, "commun");
        Object *cA = hc_new_card(st, b1, "A");
        hc_new_card(st, b2, "B");
        hc_set_current_card(cA);
        hc_new_field(b1, "champDuPremier");
        hc_new_field(b2, "champDuSecond");

        puts("   avant la sauvegarde :");
        ce_que_voit(st, "A");
        ce_que_voit(st, "B");

        remove(FIC);
        printf("   sauvegarde : %s\n", hc_save(st, FIC) == 0 ? "faite" : "ÉCHEC");
        hc_free(st);

        Object *rl = hc_load(FIC);
        if (!rl) puts("   *** relecture REFUSÉE ***");
        else {
            puts("   après la relecture :");
            ce_que_voit(rl, "A");
            ce_que_voit(rl, "B");
            hc_free(rl);
        }
    }

    puts("\n=== une carte nommée « backgroundid 7 » ne trompe pas le lecteur ===");
    /* Le mot-clé se cherche APRÈS le dernier guillemet : dans la ligne
     * entière, il se trouverait aussi dans le nom de la carte. */
    {
        Object *st = hc_new_stack("P");
        Object *b1 = hc_new_background(st, "un");
        Object *b2 = hc_new_background(st, "deux");
        hc_new_field(b1, "champDuUn");
        hc_new_field(b2, "champDuDeux");
        Object *c = hc_new_card(st, b2, "backgroundid 7");
        hc_set_current_card(c);
        remove(FIC);
        hc_save(st, FIC);
        hc_free(st);
        Object *rl = hc_load(FIC);
        if (!rl) puts("   *** relecture REFUSÉE ***");
        else { ce_que_voit(rl, "backgroundid 7"); hc_free(rl); }
    }

    puts("\n=== un fichier SANS backgroundid se lit par le nom, comme avant ===");
    {
        FILE *g = fopen(FIC, "w");
        if (g) {
            fputs("-- pile HyperCard (format maison v1)\n\n"
                  "stack \"P\"\nsize 512,342\nend stack\n\n"
                  "background \"seul\"\nid 2\n"
                  "field \"champDuSeul\"\nrect 0,0,10,10\nid 9\n"
                  "end background\n\n"
                  "card \"A\" background \"seul\"\nid 3\nend card\n", g);
            fclose(g);
        }
        Object *rl = hc_load(FIC);
        if (!rl) puts("   *** l'ancien fichier est REFUSÉ ***");
        else { ce_que_voit(rl, "A"); hc_free(rl); }
    }

    remove(FIC);
    return 0;
}
