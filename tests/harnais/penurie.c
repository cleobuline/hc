/* Une PÉNURIE DE MÉMOIRE pendant la lecture d'une pile ne doit rien perdre en
 * silence.
 *
 * Le lecteur avait déjà la bonne règle et le bon réflexe partout ailleurs :
 * une allocation qui échoue pose un drapeau COLLANT, et hc_load refuse le
 * fichier plutôt que de rendre une pile amputée. La raison est toujours la
 * même — l'utilisateur ne voit pas tout de suite ce qui manque, il enregistre,
 * et l'original est remplacé par la version appauvrie.
 *
 * Trois endroits y échappaient :
 *
 *   add_run                 « if (!v) return; » — une plage de style
 *                           disparaissait, le champ revenait avec le bon texte
 *                           et le mauvais style.
 *   la table des bgtexts    l'agrandissement raté faisait disparaître TOUT le
 *                           texte de fond de cette carte, et le free() suivant
 *                           en jetait les octets.
 *   var_set (hc_core.c)     libérait l'ancienne valeur AVANT de savoir si la
 *                           nouvelle tenait en mémoire.
 *
 * COMMENT ON LE MESURE. Ce harnais définit son propre `realloc` : un symbole
 * fort dans l'exécutable l'emporte sur celui de la bibliothèque C, et sur
 * l'intercepteur d'AddressSanitizer. Il fait échouer UN SEUL realloc — le
 * n-ième — puis balaie tous les n d'un chargement.
 *
 * Chaque essai se fait dans un FILS : certaines pénuries sont fatales par
 * dessein (hc_memoire_epuisee), et un exit() emporterait le balayage. Le fils
 * ne parle que par son code de sortie, ses deux sorties allant au néant : le
 * message de pénurie et le rapport de LeakSanitizer n'ont rien à faire dans
 * un fichier de référence.
 *
 * Ce qui est affiché est la seule chose STABLE d'un allocateur à l'autre : le
 * nombre de piles qui se chargent en ayant perdu quelque chose. Il valait 16
 * sur 52 points d'allocation avant correction. Il doit valoir zéro. */
#define _GNU_SOURCE
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>

#define FIC     "/tmp/hc_penurie.stack"
#define CARTES  8
#define PLAGES  (CARTES * 2)      /* deux « run » par champ de carte */

static int seuil = -1, compte;

void *realloc(void *p, size_t n)
{
    static void *(*vrai)(void *, size_t);
    if (!vrai) vrai = dlsym(RTLD_NEXT, "realloc");
    if (seuil >= 0 && compte == seuil) { compte++; return NULL; }
    compte++;
    return vrai(p, n);
}

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)k; (void)d; (void)t; }

static void ecris(void)
{
    FILE *g = fopen(FIC, "w");
    if (!g) return;
    fputs("-- pile HyperCard (format maison)\nformat 2\n\n"
          "stack \"P\"\nsize 512,342\nend stack\n\n"
          "background \"F\"\nid 2\n"
          "field \"bgc\"\nrect 0,0,100,20\nid 9\nsharedtext\nend field\n"
          "end background\n\n", g);
    for (int c = 0; c < CARTES; c++)
        fprintf(g, "card \"c%d\" background \"F\"\nid %d\n"
                   "bgtext 9\nbgtextdata\n| bonjour\n|\nend bgtextdata\n"
                   "bgrun 0,3,1,12,Geneva,0\n"
                   "field \"f%d\"\nrect 0,0,100,20\nid %d\n"
                   "contents\n| salut\n|\nend contents\n"
                   "run 0,2,1,12,Geneva,0\nrun 2,3,2,14,Times,255\n"
                   "end field\nend card\n\n", c, 100 + c, c, 200 + c);
    fputs("end hc-file\n", g);
    fclose(g);
}

/* Ce qu'on compte dans une pile relue : les deux choses que les trois défauts
 * faisaient disparaître. */
static void inventaire(Object *st, int *plages, int *fonds)
{
    *plages = *fonds = 0;
    for (int i = 0; i < st->nparts; i++) {
        Object *k = st->parts[i];
        if (k->type != OBJ_CARD) continue;
        *fonds += k->nbgtexts;
        for (int j = 0; j < k->nparts; j++) *plages += hc_run_count(k->parts[j]);
    }
}

enum { COMPLET = 0, PERTE = 2, FATAL = 3 };

/* 0 = refusée ou complète, 2 = chargée EN AYANT PERDU, 3 = pénurie fatale. */
static int essai(int n)
{
    fflush(NULL);
    pid_t f = fork();
    if (f < 0) return COMPLET;
    if (f == 0) {
        /* Le fils ne parle que par son code de sortie. */
        if (!freopen("/dev/null", "w", stdout)) _exit(COMPLET);
        if (!freopen("/dev/null", "w", stderr)) _exit(COMPLET);
        compte = 0; seuil = n;
        Object *st = hc_load(FIC);
        seuil = -1;
        int code = COMPLET;
        if (st) {
            int plages, fonds;
            inventaire(st, &plages, &fonds);
            if (plages != PLAGES || fonds != CARTES) code = PERTE;
            hc_free(st);
        }
        _exit(code);
    }
    int etat = 0;
    if (waitpid(f, &etat, 0) < 0) return COMPLET;
    if (!WIFEXITED(etat)) return FATAL;
    return WEXITSTATUS(etat) == PERTE ? PERTE
         : WEXITSTATUS(etat) == COMPLET ? COMPLET : FATAL;
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    ecris();

    /* Combien d'allocations réajustées le chargement fait-il ? Le nombre
     * dépend de l'allocateur ; il n'est donc pas affiché, seulement balayé. */
    compte = 0; seuil = -1;
    Object *st = hc_load(FIC);
    int points = compte;
    int plages = 0, fonds = 0;
    if (st) { inventaire(st, &plages, &fonds); hc_free(st); }

    printf("=== sans penurie ===\n");
    printf("   pile chargee : %s, plages %d/%d, textes de fond %d/%d\n",
           st ? "oui" : "NON", plages, PLAGES, fonds, CARTES);

    int perte = 0, refus = 0, fatal = 0, complet = 0;
    for (int n = 0; n < points; n++)
        switch (essai(n)) {
        case PERTE:   perte++;   break;
        case FATAL:   fatal++;   break;
        default:      complet++; break;   /* refusée OU chargée complète */
        }
    (void)refus;

    printf("\n=== un realloc en echec, a chaque point du chargement ===\n");
    printf("   piles chargees en ayant PERDU quelque chose : %d\n", perte);
    printf("   (avant correction : 16 points sur 52 le faisaient)\n");
    printf("   %s\n", (perte == 0 && points > 20 && fatal + complet == points)
                      ? "aucune perte silencieuse" : "DES PERTES SILENCIEUSES");

    remove(FIC);
    return perte ? 1 : 0;
}
