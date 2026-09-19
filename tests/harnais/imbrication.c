/* UN SCRIPT POUVAIT TUER L'APPLICATION EN EMPILANT DES « NOT ».
 *
 * SIGNALE PAR UN AUDIT EXTERIEUR, reproduit par un segfault. L'analyseur a
 * un garde-fou, HCT_PROF_MAX, qui repond « expression trop imbriquee » avant
 * que la recursion ne creuse la pile C trop loin. Il n'etait surveille QUE
 * par rang_ou — entre une fois par expression, et une fois de plus par
 * parenthese ouvrante, ce qui explique que « ((((((… » se fasse bien
 * arreter.
 *
 * Mais deux niveaux de priorite se rappellent DIRECTEMENT sans toucher au
 * compteur :
 *
 *     rang_puissance : return binaire(a, "^", j, g, rang_puissance(a));
 *     rang_unaire    : return unaire(a, "not", j, rang_unaire(a));
 *                      return unaire(a, "neg", j, rang_unaire(a));
 *
 * (Le moins unaire s'ecrit un tiret SUIVI D'UN ESPACE dans ce harnais : deux
 * tirets colles ouvrent un COMMENTAIRE en HyperTalk, et ma premiere version
 * empilait donc mille tirets que le lexeur avalait sans rien analyser du
 * tout. Le test annoncait « pas de garde-fou » pour la seule raison qu'il ne
 * testait rien.)
 *
 * Un script n'avait donc qu'a empiler des « not », des moins unaires ou des
 * « ^ ». MESURE : 20 000 « not » passent, 60 000 font un SEGFAULT — sans
 * jamais produire le message. L'application meurt sur un script, et c'est
 * exactement ce que le garde-fou existait pour empecher.
 *
 * UN SEUL COMPTEUR POUR LES TROIS, parce que c'est une seule pile C qu'on
 * protege. Le budget de 400 est donc partage : une parenthese en coute
 * desormais deux, soit 200 niveaux reels — tres au-dela de ce qu'un script
 * ecrit a la main atteint, et sans commune mesure avec les dizaines de
 * milliers qu'il fallait pour tuer le processus.
 *
 * CE HARNAIS NE MONTE PAS A 60 000. Il n'a pas besoin de reproduire le
 * segfault pour le tenir : il suffit de verifier qu'au-dela de la borne le
 * message SORT, et qu'en deca les expressions normales passent. Un test qui
 * plante quand il echoue n'apprend rien de plus qu'un test qui parle, et il
 * coute trois secondes de pile a chaque execution de la suite.
 *
 * Ce qu'il tient :
 *
 *   1. les trois formes recursives sont arretees, et par un MESSAGE ;
 *   2. les parentheses le sont toujours — le compteur partage ne doit pas
 *      avoir desarme le seul garde qui marchait. Leur message reste
 *      « parenthese fermante attendue » plutot que « trop imbriquee » : le
 *      garde-fou coupe bien la descente, mais c'est le controle de la
 *      fermante qui parle le premier. Moins precis, et sans consequence —
 *      la pile est protegee, ce qui est l'objet de ce test ;
 *   3. les imbrications NORMALES passent : c'est la moitie qui coute, une
 *      borne trop basse passerait le point 1 sans rien valoir.
 */
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* On retient le PREMIER message d'erreur, pas un simple oui/non.
 *
 * Premiere version : je comptais les « trop imbriquee » et j'affichais
 * « pas de garde-fou » sinon. Deux cas s'en trouvaient mal decrits — les
 * parentheses, arretees mais sous le message « parenthese fermante
 * attendue », et le moins unaire, dont le motif etait faux (voir plus bas).
 * Un instrument qui reduit tout a un booleen ne sait pas dire ce qu'il a vu. */
static char g_faute[160];
static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if (t && k == HC_ERR && !g_faute[0]) {
      const char *p = t; while (*p == ' ') p++;
      snprintf(g_faute, sizeof g_faute, "%s", p);
      /* On coupe au rappel de position : il est le meme partout et noie la
       * seule chose qui distingue les cas. */
      char *c = strstr(g_faute, " (v3,"); if (c) *c = '\0';
      c = strstr(g_faute, " (colonne"); if (c) *c = '\0';
  }
  if (t && k == HC_MSG) printf("      %s\n", t); }

static Object *b;

/* `motif` repete `n` fois, puis `queue`. On ne montre pas le script : a mille
 * repetitions il ferait mille lignes de reference. */
static void profond(const char *titre, const char *motif, int n,
                    const char *queue)
{
    size_t lm = strlen(motif);
    char *s = malloc(lm * (size_t)n + strlen(queue) + 64);
    if (!s) { printf("   %s : pas de memoire\n", titre); return; }
    strcpy(s, "on essaie\n  put ");
    char *p = s + strlen(s);
    for (int i = 0; i < n; i++) { memcpy(p, motif, lm); p += lm; }
    *p = '\0';
    strcat(s, queue);
    strcat(s, "\nend essaie\n");

    g_faute[0] = '\0';
    hc_set_script(b, s);
    hc_send(b, "essaie");
    printf("   %-24s x%-5d -> %s\n", titre, n,
           g_faute[0] ? g_faute : "accepte, aucune faute");
    free(s);
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);
    b = hc_new_button(c, "B");

    puts("== 1. au-dela de la borne : un MESSAGE, pas un plantage ==");
    profond("not empiles",        "not ", 1000, "true");
    profond("moins unaires",      "- ",   1000, "1");
    profond("puissances a droite", "1^",  1000, "1");

    puts("\n== 2. les parentheses restent gardees ==");
    profond("parentheses ouvrantes", "(", 1000, "1");

    puts("\n== 3. les imbrications NORMALES passent ==");
    /* Le budget etant partage, ces nombres sont volontairement au ras de ce
     * qu'un script humain produit — et bien en deca de la borne. */
    profond("not empiles",         "not ", 40, "true");
    profond("moins unaires",       "- ",   40, "1");
    profond("puissances a droite", "1^",   40, "1");
    {   /* Les fermantes sont FABRIQUEES, pas ecrites a la main : ma premiere
         * version en avait trente-neuf pour quarante ouvrantes, et le test
         * annoncait « parenthese fermante attendue » — ce qui etait vrai, et
         * n'avait rien a voir avec ce qu'il pretendait mesurer. */
        char queue[64];
        queue[0] = '1';
        for (int i = 0; i < 40; i++) queue[1 + i] = ')';
        queue[41] = '\0';
        profond("parentheses",     "(",    40, queue);
    }

    puts("\n== 4. et les expressions ordinaires ne bougent pas ==");
    hc_set_script(b, "on essaie\n"
                     "  put not (1 = 2)\n"
                     "  put - (3 + 4)\n"
                     "  put 2 ^ 3 ^ 2\n"
                     "  put not not true\n"
                     "  put ((1 + 2) * (3 + 4))\n"
                     "end essaie\n");
    hc_send(b, "essaie");

    hc_free(st);
    return 0;
}
