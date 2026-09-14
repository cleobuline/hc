/* Un nombre venu d'un SCRIPT ou d'un FICHIER se lit d'une seule façon.
 *
 * atoi ne dit jamais non. Il s'arrête au premier caractère qu'il ne comprend
 * pas, rend zéro sur du texte, et sur ce qui dépasse un int son comportement
 * est INDÉFINI — pas seulement faux. Trois conséquences mesurées avant
 * correction :
 *
 *   set the width of … to 1e2      donnait 1, alors que « put 1e3 + 0 » vaut
 *                                  1000 : le même texte, deux réponses, selon
 *                                  le chemin qui l'avait lu.
 *   id 2147483647 dans un .stack   faisait passer le compteur d'identifiants
 *                                  à -2147483648, et la carte créée ensuite
 *                                  recevait un id NÉGATIF.
 *   rect -2147483648,0,2147483647  donnait une largeur de -1, par débordement
 *                                  de la soustraction c - a.
 *
 * Tout passe maintenant par hc_entier : lecture à la mode du langage, bornes
 * explicites, et valeur de repli choisie par l'appelant quand le texte est
 * illisible ou hors bornes. Ce harnais tient les trois cas, plus les deux
 * comportements qui en découlent : un texte illisible ne remet pas la
 * propriété à zéro, et une valeur absurde est refusée au lieu d'être subie. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

#define FIC "/tmp/hc_entiers.stack"

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   > %s\n", t); }

static Object *champ;

static void geom(const char *quoi)
{ printf("   %-34s x=%d y=%d w=%d h=%d\n", quoi, champ->x, champ->y, champ->w, champ->h); }

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);

    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "A");
    hc_set_current_card(c);
    champ = hc_new_field(c, "x");

    puts("=== la notation scientifique vaut la même chose partout ===");
    hc_do("put 1e3 + 0");
    hc_do("set the width of card field \"x\" to 1e2");
    hc_do("set the top of card field \"x\" to 1e2");
    geom("apres 1e2");

    puts("\n=== un texte illisible NE remet PAS la propriete a zero ===");
    hc_do("set the width of card field \"x\" to \"patate\"");
    geom("apres \"patate\"");

    puts("\n=== une valeur hors bornes est refusee, pas subie ===");
    hc_do("set the width of card field \"x\" to 3000000");
    geom("apres 3 000 000");
    hc_do("set the left of card field \"x\" to -2147483648");
    geom("apres -2147483648");

    puts("\n=== ce qui est lisible ET dans les bornes passe, evidemment ===");
    hc_do("set the width of card field \"x\" to 220");
    hc_do("set the height of card field \"x\" to 55");
    geom("apres 220 et 55");

    hc_free(st);

    puts("\n=== un .stack porteur d'un id extreme ne fait pas deborder le compteur ===");
    {
        FILE *g = fopen(FIC, "w");
        if (g) {
            fputs("-- pile HyperCard (format maison)\n\n"
                  "stack \"P\"\nsize 512,342\nend stack\n\n"
                  "background \"F\"\nid 2\nend background\n\n"
                  "card \"A\" background \"F\"\nid 2147483647\nend card\n", g);
            fclose(g);
        }
        Object *rl = hc_load(FIC);
        if (!rl) puts("   *** relecture REFUSEE ***");
        else {
            Object *bg2 = NULL;
            for (int i = 0; i < rl->nparts; i++)
                if (rl->parts[i]->type == OBJ_BACKGROUND) { bg2 = rl->parts[i]; break; }
            for (int i = 0; i < rl->nparts; i++)
                if (rl->parts[i]->type == OBJ_CARD)
                    printf("   id relu de la carte A        : %d  (l'id du fichier "
                           "est hors bornes, il est REFUSE)\n", rl->parts[i]->id);
            Object *neuve = hc_new_card(rl, bg2, "B");
            printf("   id de la carte creee ensuite : %d  (%s)\n",
                   neuve ? neuve->id : 0,
                   (neuve && neuve->id > 0) ? "positif" : "NEGATIF");
            hc_free(rl);
        }
    }

    puts("\n=== un rect extreme ne deborde plus a la soustraction ===");
    {
        FILE *g = fopen(FIC, "w");
        if (g) {
            fputs("-- pile HyperCard (format maison)\n\n"
                  "stack \"P\"\nsize 512,342\nend stack\n\n"
                  "background \"F\"\nid 2\nend background\n\n"
                  "card \"A\" background \"F\"\nid 3\n"
                  "field \"x\"\nrect -2147483648,0,2147483647,10\nid 9\n"
                  "textsize 99999999999\nscroll -4000000000\n"
                  "end card\n", g);
            fclose(g);
        }
        Object *rl = hc_load(FIC);
        if (!rl) puts("   *** relecture REFUSEE ***");
        else {
            Object *f = NULL;
            for (int i = 0; i < rl->nparts; i++) {
                Object *k = rl->parts[i];
                if (k->type != OBJ_CARD) continue;
                for (int j = 0; j < k->nparts; j++)
                    if (k->parts[j]->type == OBJ_FIELD) f = k->parts[j];
            }
            if (!f) puts("   *** champ introuvable ***");
            else printf("   champ : x=%d y=%d w=%d h=%d textsize=%d scroll=%d\n",
                        f->x, f->y, f->w, f->h, f->textsize, f->scroll);
            hc_free(rl);
        }
    }

    puts("\n=== une taille de pile absurde ne passe pas non plus ===");
    {
        FILE *g = fopen(FIC, "w");
        if (g) {
            fputs("-- pile HyperCard (format maison)\n\n"
                  "stack \"P\"\nsize 99999999999,-99999999999\nend stack\n\n"
                  "background \"F\"\nid 2\nend background\n\n"
                  "card \"A\" background \"F\"\nid 3\nend card\n", g);
            fclose(g);
        }
        Object *rl = hc_load(FIC);
        if (!rl) puts("   *** relecture REFUSEE ***");
        else { printf("   taille relue : %d x %d\n", rl->w, rl->h); hc_free(rl); }
    }

    puts("\n=== les couleurs se lisent bornees, elles aussi ===");
    {
        const char *cas[] = { "#FF8000", "#FFFFFFFFFF", "255,128,0",
                              "99999999999,128,0", "-5,-5,-5", "patate" };
        for (unsigned i = 0; i < sizeof cas / sizeof *cas; i++)
            printf("   hc_color_from_name(\"%s\") = %d\n",
                   cas[i], hc_color_from_name(cas[i]));
    }

    puts("\n=== hc_entier lui-meme, aux bords ===");
    {
        struct { const char *t; int mini, maxi, def; } cas[] = {
            { "42",          0, 100, -1 },
            { "  42  ",      0, 100, -1 },
            { "1e2",         0, 100, -1 },
            { "42abc",       0, 100, -1 },
            { "abc",         0, 100, -1 },
            { "",            0, 100, -1 },
            { "101",         0, 100, -1 },
            { "-1",          0, 100, -1 },
            { "2147483648",  0, 100, -1 },
            { "1e400",       0, 100, -1 },
            { "42.9",        0, 100, -1 },
        };
        for (unsigned i = 0; i < sizeof cas / sizeof *cas; i++)
            printf("   hc_entier(\"%s\", %d, %d, %d) = %d\n",
                   cas[i].t, cas[i].mini, cas[i].maxi, cas[i].def,
                   hc_entier(cas[i].t, cas[i].mini, cas[i].maxi, cas[i].def));
        printf("   hc_entier(NULL, ...) = %d\n", hc_entier(NULL, 0, 100, -1));
        printf("   hc_id(\"0\") = %d   hc_id(\"7\") = %d   hc_id(\"abc\") = %d\n",
               hc_id("0"), hc_id("7"), hc_id("abc"));
        printf("   hc_rang(\"0\") = %d  hc_rang(\"3\") = %d  hc_rang(\"9e9\") = %d\n",
               hc_rang("0"), hc_rang("3"), hc_rang("9e9"));
    }

    remove(FIC);
    return 0;
}
