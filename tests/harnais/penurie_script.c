/* Une pénurie de mémoire pendant l'ANALYSE d'un script ne fait pas planter.
 *
 * Un nœud ou un tableau de fils qui ne s'allouait pas laissait un arbre
 * amputé, sans un mot, et l'évaluateur y lisait un fils absent ; le lexeur,
 * lui, rendait un lot sans jeton de fin. Mesuré en faisant échouer UNE
 * allocation à la fois pendant un script ordinaire : trois plantages sur 255
 * points, dans unaire (hct_eval.c) et hct_bloc_script.
 *
 * Depuis : l'arbre dit ses pannes par le drapeau collant des valeurs
 * (hct_arbre.c), le lexeur rend un lot de secours (hct_lex.c), et ceux qui
 * exécutent un arbre le refusent entier.
 *
 * COMMENT ON LE MESURE, comme penurie_atomique.c : ce harnais définit malloc,
 * calloc et realloc ; le n-ième appel échoue, et l'on balaie n jusqu'à ce que
 * le script passe sans échec provoqué. Chaque essai se fait dans un FILS, qui
 * ne parle que par son code de sortie. Seul le nombre de plantages s'affiche :
 * le nombre de points dépend de la bibliothèque C. Il doit valoir zéro.
 * L'arrêt voulu du noyau — hc_memoire_epuisee, qui prévient l'hôte avant de
 * sortir — n'est pas un plantage, et ne se compte pas.
 * Vérifié en retirant les corrections : il ne vaut pas zéro. */
#define _GNU_SOURCE
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <fcntl.h>

static int seuil = -1, compte, provoque;
static void *(*vrai_malloc)(size_t);
static void *(*vrai_calloc)(size_t, size_t);
static void *(*vrai_realloc)(void *, size_t);
static unsigned char amorce[65536];
static size_t amorce_pos;
static int cherche;

static void *depuis_amorce(size_t n)
{
    n = (n + 15) & ~(size_t)15;
    if (amorce_pos + n > sizeof amorce) return NULL;
    void *p = amorce + amorce_pos;
    amorce_pos += n;
    memset(p, 0, n);
    return p;
}

static void resous(void)
{
    if (vrai_malloc || cherche) return;
    cherche = 1;
    vrai_malloc  = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
    vrai_calloc  = (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
    vrai_realloc = (void *(*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");
    cherche = 0;
}

static int panne(void)
{
    if (seuil < 0) return 0;
    if (compte++ == seuil) { provoque = 1; return 1; }
    return 0;
}

void *malloc(size_t n)
{
    resous();
    if (!vrai_malloc) return depuis_amorce(n);
    if (panne()) return NULL;
    return vrai_malloc(n);
}

void *calloc(size_t a, size_t b)
{
    resous();
    if (!vrai_calloc) return depuis_amorce(a * b);
    if (panne()) return NULL;
    return vrai_calloc(a, b);
}

void *realloc(void *p, size_t n)
{
    resous();
    if (!vrai_realloc) return NULL;
    if (panne()) return NULL;
    return vrai_realloc(p, n);
}

static void ma_ligne(HcLineKind k, int d, const char *t) { (void)k; (void)d; (void)t; }

/* L'arrêt VOULU du noyau (hc_memoire_epuisee) n'est pas un plantage : il
 * prévient l'hôte, qui sauve ce qu'il peut. Le fils le dit par son code. */
static void panique(const char *quoi) { (void)quoi; _exit(9); }

static const char *SCRIPT =
    "on t\n"
    "  put 1 + 2 * 3 & \"a\" into x\n"
    "  if x contains \"a\" and not (x is empty) then put char 2 of x & item 2 of line 2 of cd fld 1 into y\n"
    "  repeat with i = 1 to 2\n"
    "    put max(i, 3) + the length of y after x\n"
    "  end repeat\n"
    "  put f2(x) into z\n"
    "  do \"put 1 into w\" & return & \"add 2 to w\"\n"
    "  put the value of \"3 * (w + 1)\" into w\n"
    "end t\n"
    "function f2 v\n"
    "  return v && \"ok\"\n"
    "end f2\n";

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    h.panic = panique;
    hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "U");
    Object *b = hc_new_button(c, "B");
    Object *f = hc_new_field(c, "F1");
    hc_set_field_text(f, "un deux\ntrois,quatre");
    hc_set_current_card(c);

    int plantages = 0, fini = 0;
    for (int n = 0; n < 5000 && !fini; n++) {
        fflush(stdout);
        pid_t p = fork();
        if (p == 0) {
            /* Le fils ne parle que par son code de sortie. */
            int nul = open("/dev/null", 1);
            if (nul >= 0) { dup2(nul, 1); dup2(nul, 2); }
            seuil = n; compte = 0; provoque = 0;
            hc_set_script(b, SCRIPT);
            hc_send(b, "t");
            seuil = -1;
            _exit(provoque ? 0 : 7);
        }
        int s = 0;
        waitpid(p, &s, 0);
        if (WIFSIGNALED(s)) plantages++;
        else if (WEXITSTATUS(s) == 7) fini = 1;
        else if (WEXITSTATUS(s) != 0 && WEXITSTATUS(s) != 9)
            plantages++;                                /* ASan sort en 1 */
    }
    printf("=== une allocation en échec, à chaque point d'un script ===\n");
    printf("   balayage mené au bout : %s\n", fini ? "oui" : "NON");
    printf("   plantages : %d\n", plantages);
    printf("   (avant correction : 3)\n");
    hc_free(st);
    return 0;
}
