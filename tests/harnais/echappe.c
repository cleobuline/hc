/* Un retour a la ligne dans un NOM coupait le fichier en deux.
 *
 * Les blocs script et contents sont solides depuis le format 2. Les CHAINES
 * COURTES ne l'etaient pas : put_quoted n'echappait que le guillemet et la
 * contre-oblique, et le style comme la police s'ecrivaient sans aucun
 * encodage. Or le langage pose n'importe quelle chaine comme nom :
 *
 *     set the name of card button 1 to "Bonjour" & return & "Monde"
 *     save this stack
 *
 * L'en-tete sortait physiquement sur DEUX lignes —
 *
 *     button "Bonjour
 *     Monde"
 *
 * — et la relecture rendait un bouton nomme « Bonjour », la ligne « Monde" »
 * etant avalee comme une ligne inconnue. Sans un mot. Mesure avant correction,
 * sur les trois : nom=[Bonjour] style=[round] police=[Ge].
 *
 * Le serialiseur ne doit pas dependre d'une restriction implicite de
 * l'interface : c'est le FORMAT qui sait encoder ce que le modele accepte.
 *
 * La propriete verifiee tient en une ligne, comme pour les textes :
 *
 *     chaine avant == chaine apres
 *
 * Elle porte sur les noms d'objets, les noms de PILE, de FOND et de CARTE, le
 * style, la police du champ et celle des plages. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FIC "/tmp/hc_echappe.stack"

static int nko;
static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   > [%s]\n", t ? t : ""); }

static void montre(const char *s)
{
    if (!s) { printf("(nul)"); return; }
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if      (*p == '\n') printf("\\n");
        else if (*p == '\r') printf("\\r");
        else if (*p == '\t') printf("\\t");
        else putchar(*p);
    }
}

static void compare(const char *quoi, const char *avant, const char *apres)
{
    int ok = (!avant && !apres) || (avant && apres && strcmp(avant, apres) == 0);
    if (!ok) nko++;
    printf("   %s %-16s [", ok ? "ok   " : "ECHEC", quoi);
    montre(avant); printf("] -> ["); montre(apres); printf("]\n");
}

static void pose(char **ou, const char *v) { free(*ou); *ou = v ? strdup(v) : NULL; }

/* Les chaines tordues, une par cas. Le « \c » est la pour verifier qu'un
 * echappement ECRIT PAR NOUS se relit comme du texte et non comme une virgule. */
static const char *CAS[] = {
    "simple",
    "avec\nun saut",
    "avec\r\nCRLF",
    "avec \"des guillemets\"",
    "avec\\une contre-oblique",
    "avec,une virgule",
    "avec\\nun faux echappement",
    "avec\\cun faux echappement de virgule",
    "tout\n\"a\\la,fois\r",
    "  des blancs au bord  ",
};
#define NCAS ((int)(sizeof CAS / sizeof *CAS))

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);
    hc_do("get 1");   /* la banniere v3 sort ici, pas au milieu d'un releve */

    for (int i = 0; i < NCAS; i++) {
        const char *v = CAS[i];
        printf("\n=== cas %d ===\n", i + 1);

        Object *st = hc_new_stack(v);
        Object *bg = hc_new_background(st, v);
        Object *c  = hc_new_card(st, bg, v);
        hc_set_current_card(c);
        Object *b  = hc_new_button(c, v);
        pose(&b->style, v);
        Object *f  = hc_new_field(c, "f");
        hc_set_field_text(f, "abcdef");
        pose(&f->textfont, v);
        /* une plage de style, dont la police est le cas lui-meme */
        hc_run_add_color(f, 0, 3, HC_BOLD, 12, v, HC_COLOR_INHERIT);

        remove(FIC);
        if (hc_save(st, FIC) != 0) { printf("   ECHEC sauvegarde\n"); nko++; hc_free(st); continue; }
        hc_free(st);

        Object *rl = hc_load(FIC);
        if (!rl) { printf("   *** relecture REFUSEE ***\n"); nko++; continue; }

        compare("nom de pile", v, rl->name);
        Object *rbg = NULL, *rc = NULL;
        for (int k = 0; k < rl->nparts; k++) {
            if (rl->parts[k]->type == OBJ_BACKGROUND) rbg = rl->parts[k];
            if (rl->parts[k]->type == OBJ_CARD)       rc  = rl->parts[k];
        }
        compare("nom de fond",  v, rbg ? rbg->name : NULL);
        compare("nom de carte", v, rc  ? rc->name  : NULL);

        Object *rb = NULL, *rf = NULL;
        if (rc) for (int k = 0; k < rc->nparts; k++) {
            if (rc->parts[k]->type == OBJ_BUTTON) rb = rc->parts[k];
            if (rc->parts[k]->type == OBJ_FIELD)  rf = rc->parts[k];
        }
        compare("nom de bouton", v, rb ? rb->name  : NULL);
        compare("style",         v, rb ? rb->style : NULL);
        compare("police champ",  v, rf ? rf->textfont : NULL);

        const char *police = NULL;
        if (rf && hc_run_count(rf) > 0) {
            int s0, l0, st0, sz0, co0; const char *fo0 = NULL;
            if (hc_run_attrs_color(rf, 0, &s0, &l0, &st0, &sz0, &fo0, &co0)) police = fo0;
        }
        compare("police plage",  v, police);

        hc_free(rl);
    }

    puts("\n=== par le CHEMIN REEL : set the name ... & return & ... ===");
    /* Les cas ci-dessus ecrivent les noms directement dans la structure. Celui
     * de l'utilisateur passe par le script : resolution de « button id N »,
     * puis le poseur de propriete. C'est ce chemin-la qu'il faut tenir, et
     * c'est exactement la ligne qui produisait
     *
     *     button "hello
     *     world"
     *
     * soit un en-tete coupe en deux et un bouton relu nomme « hello ». */
    {
        Object *st = hc_new_stack("P");
        hc_register_stack(st);
        Object *bg = hc_new_background(st, "F");
        Object *c  = hc_new_card(st, bg, "A");
        hc_set_current_card(c);
        Object *b  = hc_new_button(c, "Bouton");
        hc_set_id(b, 2915);

        hc_do("set the name of button id 2915 to \"hello\" & return & \"world\"");
        compare("nom apres set", "hello\nworld", b->name);

        remove(FIC);
        int sauve = hc_save(st, FIC);
        hc_unregister_stack(st);
        hc_free(st);
        if (sauve != 0) { puts("   ECHEC sauvegarde"); nko++; }
        else {
            Object *rl = hc_load(FIC);
            if (!rl) { printf("   *** relecture REFUSEE : %s ***\n",
                              hc_load_erreur() ? hc_load_erreur() : "?"); nko++; }
            else {
                hc_register_stack(rl);
                Object *rb = NULL;
                for (int k = 0; k < rl->nparts; k++) {
                    if (rl->parts[k]->type != OBJ_CARD) continue;
                    hc_set_current_card(rl->parts[k]);
                    for (int j = 0; j < rl->parts[k]->nparts; j++)
                        if (rl->parts[k]->parts[j]->type == OBJ_BUTTON)
                            rb = rl->parts[k]->parts[j];
                }
                compare("nom apres relecture", "hello\nworld", rb ? rb->name : NULL);
                printf("   id retrouve : %d\n", rb ? rb->id : 0);
                /* Et le nom reste DECOUPABLE en deux lignes, ce qui est bien
                 * ce que l'auteur du script a demande. */
                hc_do("put line 1 of the short name of button id 2915");
                hc_do("put line 2 of the short name of button id 2915");
                hc_unregister_stack(rl);
                hc_free(rl);
            }
        }
    }

    puts("\n=== une pile ECRITE AVANT ce changement se relit a l'identique ===");
    /* Contre-oblique brute dans une police : l'ancien ecrivain la posait telle
     * quelle. Un echappement inconnu doit rester tel quel a la lecture. */
    {
        FILE *g = fopen(FIC, "w");
        if (g) {
            fputs("-- pile HyperCard (format maison v1)\n\n"
                  "stack \"P\"\nsize 512,342\nend stack\n\n"
                  "background \"F\"\nid 2\nend background\n\n"
                  "card \"A\" background \"F\"\nid 3\n"
                  "field \"f\"\nrect 0,0,10,10\nid 9\n"
                  "textfont A\\Bc\n"
                  "end field\nend card\n", g);
            fclose(g);
        }
        Object *rl = hc_load(FIC);
        if (!rl) { puts("   *** relecture REFUSEE ***"); nko++; }
        else {
            Object *rf = NULL;
            for (int k = 0; k < rl->nparts; k++)
                if (rl->parts[k]->type == OBJ_CARD)
                    for (int j = 0; j < rl->parts[k]->nparts; j++)
                        if (rl->parts[k]->parts[j]->type == OBJ_FIELD)
                            rf = rl->parts[k]->parts[j];
            compare("police ancienne", "A\\Bc", rf ? rf->textfont : NULL);
            hc_free(rl);
        }
    }

    remove(FIC);
    printf("\n   %s\n", nko ? "DES ECARTS" : "tout revient a l'identique");
    return nko ? 1 : 0;
}
