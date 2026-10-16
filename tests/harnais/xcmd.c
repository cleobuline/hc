/* xcmd — LES MODULES : des XCMD et des XFCN en WebAssembly, rangés dans la
 * pile (hc_wasm.c ; décidé le 15 octobre, docs/mesures/xcmd.txt).
 *
 * Le premier maillon que l'utilisatrice a demandé : « une XCMD "hello
 * world" dans la message box, pour valider toute la chaîne de production ».
 * bonjour.wasm est l'exemple du kit (xcmd/exemples/bonjour.c), tel quel.
 * Les autres modules (donnees/modules/, refais.sh) se conduisent mal exprès :
 * chacun éprouve un garde-fou.
 *
 * Rien n'est mesuré dans HyperCard ici : il ne connaissait pas le
 * WebAssembly. Ce qui en vient — le bloc de paramètres, SendCardMessage,
 * passFlag — est noté dans docs/mesures/xcmd.txt. */
#include "hc_core.h"
#include "hc_file.h"
#include "hc_wasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if      (k == HC_ERR)  printf("      [ERR] %s\n", t);
    else if (k == HC_MSG)  printf("      = %s\n", t);
    else if (k == HC_INFO && strstr(t, "XCMD")) printf("      %s\n", t);
}

static void fais(const char *l)
{
    printf("   %s\n", l);
    hc_do(l);
}

static unsigned char *lis(const char *dossier, const char *nom, size_t *n)
{
    char chemin[512];
    snprintf(chemin, sizeof chemin, "%s/%s", dossier, nom);
    FILE *f = fopen(chemin, "rb");
    if (!f) { perror(chemin); exit(2); }
    fseek(f, 0, SEEK_END);
    long t = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)t + 1);
    if (!b || fread(b, 1, (size_t)t, f) != (size_t)t) { perror(chemin); exit(2); }
    fclose(f);
    *n = (size_t)t;
    return b;
}

static void ajoute(Object *st, const char *dossier, const char *fichier,
                   const char *nom)
{
    size_t n = 0;
    unsigned char *o = lis(dossier, fichier, &n);
    char pourquoi[300] = "";
    int r = hc_module_ajoute(st, nom, o, n, pourquoi, sizeof pourquoi);
    printf("   ajoute %s (%zu octets) : %s%s\n", fichier, n,
           r == 0 ? "accepté" : "REFUSÉ — ", r == 0 ? "" : pourquoi);
    free(o);
}

static void externes(Object *st)
{
    for (int i = 0; i < hc_module_nombre(st); i++) {
        char liste[1000];
        int k = hc_module_externes(st, i, liste, sizeof liste);
        printf("   module « %s » : %d externe(s)\n", hc_module_nom(st, i), k);
        for (char *p = strtok(liste, "\n"); p; p = strtok(NULL, "\n"))
            printf("      %s\n", p);
    }
}

/* Le texte d'un fichier, pour en montrer un bout ou le réécrire abîmé. */
static char *texte(const char *chemin)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long t = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)t + 1);
    if (!b || fread(b, 1, (size_t)t, f) != (size_t)t) { fclose(f); free(b); return NULL; }
    b[t] = '\0';
    fclose(f);
    return b;
}

static void ecris(const char *chemin, const char *t)
{
    FILE *f = fopen(chemin, "wb");
    if (!f) return;
    fputs(t, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage : xcmd <dossier des modules>\n"); return 2; }
    const char *D = argv[1];
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("S");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "C");
    hc_set_current_card(c);

    puts("== 1. bonjour, le premier module : la chaîne entière ==");
    ajoute(st, D, "bonjour.wasm", "bonjour");
    externes(st);
    fais("bonjour \"Patricia\"");
    fais("bonjour");
    fais("put bonjour(\"Patricia\")");
    fais("put bonjour() into x");
    fais("put x");

    puts("\n== 2. le nom ne distingue pas les capitales, comme HyperTalk ==");
    fais("BONJOUR \"en capitales\"");
    fais("put BonJour(\"aussi\")");

    puts("\n== 3. un script du même nom passe AVANT le module ==");
    hc_set_script(st, "on bonjour qui\n  put \"le script répond à \" & qui\nend bonjour");
    fais("bonjour \"Patricia\"");
    fais("put bonjour(\"la fonction\")");     /* la fonction, elle, reste au module */
    hc_set_script(st, "");
    fais("bonjour \"de nouveau le module\"");

    puts("\n== 4. refusés à l'ajout, et la pile n'en garde rien ==");
    {
        char pourquoi[300] = "";
        static const unsigned char pas_un[] = "on mouseUp beep end mouseUp";
        int r = hc_module_ajoute(st, "texte", pas_un, sizeof pas_un - 1,
                                 pourquoi, sizeof pourquoi);
        printf("   ajoute du texte : %s %s\n", r ? "REFUSÉ —" : "accepté", pourquoi);
        r = hc_module_ajoute(st, "vide", pas_un, 0, pourquoi, sizeof pourquoi);
        printf("   ajoute un module vide : %s %s\n", r ? "REFUSÉ —" : "accepté", pourquoi);
    }
    ajoute(st, D, "inconnu.wasm", "inconnu");
    ajoute(st, D, "sansboite.wasm", "sansboite");
    printf("   la pile garde %d module(s)\n", hc_module_nombre(st));

    puts("\n== 5. les épreuves : un module qui se conduit mal ==");
    ajoute(st, D, "epreuves.wasm", "epreuves");
    externes(st);
    fais("put compte(\"a\", \"b\", \"c\")");
    fais("put compte()");
    fais("faute");
    fais("put \"le script suivant tourne\"");
    fais("put compte(\"repart\")");            /* une instance neuve */
    fais("hors");
    fais("put hors()");
    fais("put \"[\" & the result & \"]\"");
    fais("resultat");
    fais("put \"[\" & the result & \"]\"");
    fais("reentre");
    fais("mauvaise");
    fais("passe");
    fais("put compte(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)");
    fais("put compte(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17)");
    fais("passe 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17");

    puts("\n== 6. la boucle sans fin : arrêtée par la sentinelle ==");
    hc_module_delai(0.3);
    {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        fais("boucle");
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double s = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
        printf("   rendue en %s\n", s < 2.0 ? "moins de deux secondes" : "PLUS DE DEUX SECONDES");
    }
    hc_module_delai(0);
    fais("bonjour \"après la boucle\"");

    puts("\n== 7. enregistrer, relire : le module voyage dans la pile ==");
    {
        char chemin[] = "/tmp/xcmd_XXXXXX";
        int fd = mkstemp(chemin);
        if (fd < 0) { perror("mkstemp"); return 1; }
        close(fd);
        printf("   hc_save : %d\n", hc_save(st, chemin));
        char *t = texte(chemin);
        const char *m = t ? strstr(t, "module \"bonjour\"") : NULL;
        if (m) {
            const char *fin = strchr(m, '\n');
            const char *l2 = fin ? strchr(fin + 1, '\n') : NULL;
            printf("   le fichier : %.*s\n", (int)(fin - m), m);
            if (fin && l2) printf("                %.24s…\n", fin + 1);
        } else {
            puts("   le fichier n'a PAS de bloc « module »");
        }
        Object *relue = hc_load(chemin);
        printf("   hc_load : %s, %d module(s)\n", relue ? "relue" : hc_load_erreur(),
               relue ? hc_module_nombre(relue) : 0);
        if (relue) {
            size_t n1 = 0, n2 = 0;
            const unsigned char *a = hc_module_octets(st, 0, &n1);
            const unsigned char *b = hc_module_octets(relue, 0, &n2);
            printf("   les octets de bonjour : %s\n",
                   n1 == n2 && !memcmp(a, b, n1) ? "identiques" : "DIFFÉRENTS");
            hc_unregister_stack(st);
            hc_register_stack(relue);
            Object *cr = NULL;
            for (int i = 0; i < relue->nparts && !cr; i++)
                if (relue->parts[i]->type == OBJ_CARD) cr = relue->parts[i];
            hc_set_current_card(cr);
            fais("bonjour \"depuis la pile relue\"");
            hc_unregister_stack(relue);
            hc_register_stack(st);
            hc_set_current_card(c);
            hc_free(relue);
        }

        /* Un chiffre hexadécimal abîmé : la pile est refusée, et le dit. */
        if (t && m) {
            char *z = strstr(t, "module \"bonjour\"");
            char *l = strchr(z, '\n');
            if (l && l[1] == '|' && l[3]) l[3] = 'Z';
            ecris(chemin, t);
            Object *x = hc_load(chemin);
            printf("   un octet abîmé : %s\n", x ? "RELUE" : hc_load_erreur());
            if (x) hc_free(x);
        }
        /* Un bloc sans sa fin : une troncature. */
        if (t) {
            char *z = strstr(t, "end module");
            if (z) {
                *z = '\0';
                ecris(chemin, t);
                Object *x = hc_load(chemin);
                printf("   un bloc sans sa fin : %s\n", x ? "RELUE" : hc_load_erreur());
                if (x) hc_free(x);
            }
        }
        free(t);
        unlink(chemin);
    }

    puts("\n== 8. retirer un module ==");
    printf("   hc_module_retire(bonjour) : %d\n", hc_module_retire(st, "bonjour"));
    fais("bonjour \"Patricia\"");
    printf("   la pile garde %d module(s)\n", hc_module_nombre(st));

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
