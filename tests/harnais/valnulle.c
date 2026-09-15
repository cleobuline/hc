/* Le texte d'une HctValeur n'est JAMAIS nul.
 *
 * Il l'était. Quand une allocation echouait, les constructeurs rendaient
 * txt = NULL et len = 0, indiscernable d'une chaine vide valide, et l'en-tete
 * l'avouait : « tout consommateur doit donc traiter txt == NULL ». C'est une
 * promesse que personne ne peut tenir — il y a des centaines de lectures de
 * .txt dans le noyau, et il suffit d'en oublier une.
 *
 * MESURE, en faisant echouer UN SEUL malloc et en balayant tous les points
 * d'allocation de deux scripts ordinaires : sept plantages sur quatre cents,
 * puis deux sur six cents. Zero apres correction.
 *
 * POURQUOI CE HARNAIS NE BALAIE PAS, LUI. L'interposition de malloc ne prend
 * pas sous AddressSanitizer — verifie : le programme ne produit plus rien du
 * tout. Un harnais qui balaierait quand meme rendrait un ZERO FAUX dans la
 * moitie des passages de la suite, ce qui est la pire sorte de vert. Il
 * verifie donc le CONTRAT directement, ce qui vaut identiquement dans les deux
 * constructions — et sous ASan chaque liberation est en plus controlee pour de
 * bon.
 *
 * (penurie.c, lui, interpose realloc, qui passe tres bien sous ASan. C'est une
 * particularite de malloc, pas une regle.) */
#include "hc_core.h"
#include "hct_val.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int nko;
static void ok(const char *quoi, int vrai)
{
    if (!vrai) nko++;
    printf("   %s %s\n", vrai ? "ok   " : "ECHEC", quoi);
}

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : "");
  else if (k == HC_MSG) printf("   [msg] %s\n", t ? t : ""); }

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);
    hc_do("get 1");   /* la banniere v3 sort ici, pas au milieu d'un releve */

    puts("=== une valeur vide porte une chaine LISIBLE, pas un pointeur nul ===");
    {
        HctValeur v = hct_val_vide();
        ok("hct_val_vide().txt n'est pas nul", v.txt != NULL);
        ok("  et vaut la chaine vide",         v.txt && v.txt[0] == '\0');
        ok("  et sa longueur est nulle",       v.len == 0);
        hct_val_libere(&v);           /* ne doit pas liberer la sentinelle */
        ok("liberer une valeur vide ne plante pas", 1);
    }

    puts("\n=== une valeur en ECHEC aussi, et elle leve un drapeau ===");
    {
        hct_val_manque_efface();
        ok("le drapeau part a zero", !hct_val_manque());
        HctValeur v = hct_val_echec();
        ok("hct_val_echec().txt n'est pas nul", v.txt != NULL);
        ok("  et vaut la chaine vide",          v.txt && v.txt[0] == '\0');
        ok("le drapeau est leve",               hct_val_manque());
        ok("  et il est COLLANT",               hct_val_manque());
        hct_val_manque_efface();
        ok("  jusqu'a ce qu'on l'efface",       !hct_val_manque());
        hct_val_libere(&v);
    }

    puts("\n=== une chaine vide construite normalement suit la meme route ===");
    {
        HctValeur v = hct_val_texte("");
        ok("hct_val_texte(\"\").txt n'est pas nul", v.txt != NULL);
        HctValeur w = hct_val_texte(NULL);
        ok("hct_val_texte(NULL).txt non plus",     w.txt != NULL);
        HctValeur c = hct_val_copie(v);
        ok("une copie de valeur vide non plus",    c.txt != NULL);
        hct_val_libere(&v); hct_val_libere(&w); hct_val_libere(&c);
        ok("les trois se liberent sans broncher", 1);
    }

    puts("\n=== hct_val_prend : la SEULE porte de sortie d'un texte ===");
    {
        /* Sur une valeur vide, elle doit rendre un bloc du TAS : reprendre la
         * sentinelle a la main puis la liberer donnait « free(): invalid
         * pointer », mesure dans le tri. */
        HctValeur v = hct_val_vide();
        char *p = hct_val_prend(&v);
        ok("prend() sur une valeur vide rend non-nul", p != NULL);
        ok("  et une chaine vide",                     p && p[0] == '\0');
        ok("  et la source est videe",                 v.txt == NULL && v.len == 0);
        free(p);                       /* ASan crie si ce n'est pas du tas */
        ok("  et free() l'accepte", 1);

        HctValeur w = hct_val_texte("bonjour");
        char *q = hct_val_prend(&w);
        ok("prend() sur un vrai texte rend le texte", q && !strcmp(q, "bonjour"));
        ok("  et la source est videe",                w.txt == NULL && w.len == 0);
        free(q);
        ok("  et free() l'accepte aussi", 1);
    }

    puts("\n=== et l'executeur ne CONTINUE PAS sur une valeur inventee ===");
    /* Sans la lecture du drapeau, une penurie se deguiserait en chaine vide et
     * le script rendrait un resultat FAUX en silence. On leve le drapeau a la
     * main, puis on fait tourner un script : il doit s'arreter proprement. */
    {
        Object *st = hc_new_stack("P"); hc_register_stack(st);
        Object *bg = hc_new_background(st, "F");
        Object *c  = hc_new_card(st, bg, "A");
        hc_set_current_card(c);
        Object *b  = hc_new_button(c, "b");
        hc_set_script(b,
            "on mouseUp\n"
            "  put \"un\" into a\n"
            "  put \"deux\" into b\n"
            "  put a & b\n"
            "end mouseUp\n");

        puts("   -- sans drapeau : le script va au bout --");
        hct_val_manque_efface();
        hc_send(b, "mouseUp");

        puts("   -- drapeau leve avant l'envoi : il s'arrete et le dit --");
        HctValeur bidon = hct_val_echec();
        hct_val_libere(&bidon);
        hc_send(b, "mouseUp");
        ok("le drapeau a ete consomme", !hct_val_manque());

        hct_val_manque_efface();
        hc_unregister_stack(st);
        hc_free(st);
    }

    printf("\n   %s\n", nko ? "DES ECARTS" : "le contrat est tenu");
    return nko ? 1 : 0;
}
