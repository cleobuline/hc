/* garde.c — RÉCOLTER LES GRAINES que la suite fabrique, puis efface.
 *
 * Les harnais qui enregistrent une pile — polygones, opacités, icônes en
 * couleur, textes de fond — l'effacent en finissant. Ce sont pourtant les
 * meilleures graines qui soient : chacune exerce une ligne du format qu'un
 * autre harnais a voulu tester.
 *
 * Chargée par LD_PRELOAD pendant que tourne tests/lance.sh, cette bibliothèque
 * intercepte unlink() et remove() : avant de laisser effacer un fichier qui
 * commence comme une pile de HC, elle le recopie dans $GARDE. Elle ne retient
 * RIEN d'autre, et ne change rien à ce que la suite observe.
 *
 * PAS sous --asan : le moteur d'ASan exige d'être chargé le premier, et
 * refuse toute bibliothèque préchargée avant lui. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

static const char ENTETE[] = "-- pile HyperCard (format maison)";

static int est_pile(const char *p)
{
    char b[sizeof ENTETE] = { 0 };
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    size_t n = fread(b, 1, sizeof ENTETE - 1, f);
    fclose(f);
    return n == sizeof ENTETE - 1 && memcmp(b, ENTETE, n) == 0;
}

static void recopie(const char *p)
{
    static int k;
    const char *dossier = getenv("GARDE");
    const char *base = strrchr(p, '/');
    char d[4096];
    snprintf(d, sizeof d, "%s/%d_%d_%s", dossier, (int)getpid(), k++, base ? base + 1 : p);
    FILE *a = fopen(p, "rb"), *b = fopen(d, "wb");
    if (a && b) {
        char t[8192];
        size_t n;
        while ((n = fread(t, 1, sizeof t, a)) > 0) fwrite(t, 1, n, b);
    }
    if (a) fclose(a);
    if (b) fclose(b);
}

int unlink(const char *p)
{
    static int (*vrai)(const char *);
    if (!vrai) vrai = (int (*)(const char *))dlsym(RTLD_NEXT, "unlink");
    if (getenv("GARDE") && est_pile(p)) recopie(p);
    return vrai(p);
}

int remove(const char *p)
{
    static int (*vrai)(const char *);
    if (!vrai) vrai = (int (*)(const char *))dlsym(RTLD_NEXT, "remove");
    if (getenv("GARDE") && est_pile(p)) recopie(p);
    return vrai(p);
}
