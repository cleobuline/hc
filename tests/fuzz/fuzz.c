/* fuzz.c — LE FUZZER DES DEUX PORTES DE FICHIERS DE HC.
 *
 * Une pile arrive d'Internet : c'est la seule chose que HC lit sans l'avoir
 * écrite. Ce pilote en fabrique des versions abîmées et regarde si le noyau
 * plante, fuit ou gèle en les ouvrant.
 *
 *   fuzz hc     <graines> <n> <sortie> <germe>   notre format, le texte
 *   fuzz orig   <graines> <n> <sortie> <germe>   celui d'Apple, nu ou MacBinary
 *   fuzz temoin hc|orig <graines> <sortie>       chaque graine telle quelle
 *   fuzz un     hc|orig <fichier>                rejoue UN fichier, sans fork
 *
 * CHAQUE ESSAI SE FAIT DANS UN FILS, qui fait ce que fait l'application à
 * l'ouverture — lire, convertir s'il le faut, poser les icônes de la
 * ressource — puis l'aller-retour par le disque : enregistrer, relire. Le
 * fils meurt sous ASan, UBSan, LeakSanitizer ou l'alarme ; le père garde le
 * fichier et le rapport dans <sortie>.
 *
 * DEUX DÉFAUTS DE CE PILOTE ONT FAIT MENTIR SA PREMIÈRE CAMPAGNE, le 4
 * octobre (docs/mesures/fuzzing.txt). Le fils sortait par _exit(), qui saute
 * les routines de fin de programme — et c'est là que LeakSanitizer fait son
 * contrôle : 400 000 essais, et aucune fuite n'aurait jamais pu être vue. Puis
 * le fils effaçait son fichier avant de sortir, si bien qu'une fuite, enfin
 * signalée, perdait le cas qui la produisait. D'où le mode TÉMOIN, compilé
 * avec -DCANARI : trois graines piégées — un débordement, une fuite, une
 * boucle sans fin — que tests/fuzz/lance.sh exige de voir attrapées AVANT
 * de croire un « zéro signalement ». Un instrument sourd rend des zéros.
 */
#define _GNU_SOURCE
#include "hc_core.h"
#include "hc_file.h"
#include "hc_origine.h"
#include "hc_importe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

typedef struct { unsigned char *o; size_t n; } Buf;

static Buf *graines;
static int  ngraines;
static unsigned long long rs;

static unsigned r32(void)
{
    rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17;
    return (unsigned)(rs >> 11);
}
static size_t rn(size_t n) { return n ? r32() % n : 0; }

static int lit_fichier(const char *p, Buf *b)
{
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    if (n <= 0 || n > (4L << 20)) { fclose(f); return 0; }
    b->o = malloc((size_t)n + 1);
    b->n = (size_t)n;
    if (!b->o || fread(b->o, 1, b->n, f) != b->n) { fclose(f); free(b->o); return 0; }
    fclose(f);
    return 1;
}

static void charge_graines(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    if (!d) { perror(dir); exit(2); }
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[4096];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        Buf b;
        if (!lit_fichier(p, &b)) continue;
        Buf *t = realloc(graines, sizeof *graines * (size_t)(ngraines + 1));
        if (!t) exit(2);
        graines = t;
        graines[ngraines++] = b;
    }
    closedir(d);
}

/* ═══ LES MUTATIONS ═══ */

static const char *NOMBRES[] = {
    "-1", "0", "1", "-2147483648", "2147483647", "2147483648", "4294967295",
    "99999999999999999999", "-99999", "65535", "65536", "32767", "-32768",
    "256", "255", "1e308", "nan", "inf", "0x7fffffff", "", " ", "-0", "3.5",
    "1000000" };

/* Des lignes du format, justes ou fausses, et des octets qui fâchent. */
static const char *MOTS[] = {
    "end card\n", "end stack\n", "end button\n", "end field\n",
    "end background\n", "card\n", "button\n", "field\n", "background\n",
    "stack \"x\"\n", "points 0,0 1,1\n", "points 1,1\n",
    "points 0,0 5,5 -3,7 99999,1 2147483647,-2147483648\n", "points 0,0",
    "backalpha 300\n", "backalpha -5\n", "forealpha 0\n", "backcolor -1\n",
    "backcolor 99999999\n", "iconres 1 ", "ciconres 1 ", "bgtext 1 ",
    "bgtextdata 1 ", "bghilite 99999\n", "run 0 5 ", "bgrun 1 0 5 ", "paint ",
    "rect 0,0,-5,-5\n", "rect 2147483647,0,-2147483648,1\n", "size 0,0\n",
    "size -1,-1\n", "size 100000,100000\n", "format 99\n", "format 1\n",
    "scroll -1\n", "selectedline 99999\n", "textalign 7\n", "textsize 0\n",
    "textsize -3\n", "textheight 100000\n", "family 99\n", "id 0\n", "id -1\n",
    "id 2147483647\n", "style polygon\n", "style ", "contents\n", "script\n",
    "| ", "|\n", "\"", "\\", "\\x", "\r", "\xff\xfe", "\xc3", "\xe2\x80",
    "titlewidth -9\n" };

#define NB(t) (sizeof t / sizeof *t)

static void insere(Buf *b, size_t at, const void *s, size_t n)
{
    unsigned char *t = realloc(b->o, b->n + n + 1);
    if (!t) exit(2);
    b->o = t;
    memmove(b->o + at + n, b->o + at, b->n - at);
    memcpy(b->o + at, s, n);
    b->n += n;
}

static void retire(Buf *b, size_t at, size_t n)
{
    if (at >= b->n) return;
    if (at + n > b->n) n = b->n - at;
    memmove(b->o + at, b->o + at + n, b->n - at - n);
    b->n -= n;
}

static size_t debut_ligne(const Buf *b, size_t at)
{
    while (at > 0 && b->o[at - 1] != '\n') at--;
    return at;
}

static size_t fin_ligne(const Buf *b, size_t at)
{
    while (at < b->n && b->o[at] != '\n') at++;
    return at < b->n ? at + 1 : at;
}

/* Recopie [d, f) de `b` à la position `ou` : le morceau est copié AVANT
 * l'insertion, qui peut déplacer le tampon. */
static void recopie(Buf *b, size_t d, size_t f, size_t ou)
{
    if (f <= d) return;
    unsigned char *t = malloc(f - d);
    if (!t) exit(2);
    memcpy(t, b->o + d, f - d);
    insere(b, ou, t, f - d);
    free(t);
}

static void mute_texte(Buf *b)
{
    for (int k = 1 + (int)rn(4); k > 0; k--) {
        size_t at = rn(b->n + 1);
        switch (rn(11)) {
        case 0:                                         /* un bit */
            if (b->n) b->o[rn(b->n)] ^= (unsigned char)(1u << rn(8));
            break;
        case 1:                                         /* un octet */
            if (b->n) b->o[rn(b->n)] = (unsigned char)r32();
            break;
        case 2: {                                       /* un nombre remplacé */
            size_t i = rn(b->n + 1), j;
            while (i < b->n && !(b->o[i] >= '0' && b->o[i] <= '9')) i++;
            if (i >= b->n) break;
            if (i > 0 && b->o[i - 1] == '-') i--;
            j = i + 1;
            while (j < b->n && b->o[j] >= '0' && b->o[j] <= '9') j++;
            const char *s = NOMBRES[rn(NB(NOMBRES))];
            retire(b, i, j - i);
            insere(b, i, s, strlen(s));
            break;
        }
        case 3: {                                       /* une ligne du format */
            const char *s = MOTS[rn(NB(MOTS))];
            insere(b, debut_ligne(b, at), s, strlen(s));
            break;
        }
        case 4: {                                       /* une ligne en moins */
            size_t d = debut_ligne(b, at);
            retire(b, d, fin_ligne(b, at) - d);
            break;
        }
        case 5: {                                       /* une ligne en double */
            size_t d = debut_ligne(b, at);
            recopie(b, d, fin_ligne(b, at), d);
            break;
        }
        case 6:                                         /* tronqué */
            b->n = rn(b->n + 1);
            break;
        case 7: {                                       /* un mot en pleine ligne */
            const char *s = MOTS[rn(NB(MOTS))];
            insere(b, at, s, strlen(s));
            break;
        }
        case 8: {                                       /* une ligne d'une autre graine */
            const Buf *g = &graines[rn((size_t)ngraines)];
            if (!g->n) break;
            size_t p = rn(g->n);
            size_t d = debut_ligne(g, p), f = fin_ligne(g, p);
            insere(b, debut_ligne(b, at), g->o + d, f - d);
            break;
        }
        case 9: {                                       /* huit lignes recopiées ailleurs */
            size_t d = debut_ligne(b, at), f = d;
            for (int i = 0; i < 8; i++) f = fin_ligne(b, f);
            recopie(b, d, f, fin_ligne(b, rn(b->n + 1)));
            break;
        }
        case 10: {                                      /* une ligne interminable */
            size_t L = 1 + rn(70000);
            unsigned char *t = malloc(L);
            if (!t) exit(2);
            memset(t, "x9,|-"[rn(5)], L);
            insere(b, at, t, L);
            free(t);
            break;
        }
        }
    }
}

static void mute_binaire(Buf *b)
{
    static const unsigned V[] = { 0, 1, 0xFFFFFFFFu, 0x7FFFFFFFu, 0x80000000u,
                                  0xFFFF, 0x8000, 0x7FFF, 0x10000, 4, 8, 0x600,
                                  0x10, 0xFFFFFFF0u };
    for (int k = 1 + (int)rn(4); k > 0; k--) {
        if (!b->n) return;
        size_t at = rn(b->n);
        switch (rn(7)) {
        case 0: b->o[at] ^= (unsigned char)(1u << rn(8)); break;
        case 1: b->o[at] = (unsigned char)r32(); break;
        case 2: {               /* un entier gros-boutiste de 16 ou 32 bits, aligné */
            unsigned v = V[rn(NB(V))];
            size_t a = at & ~(size_t)1;
            int w = rn(2) ? 4 : 2;
            if (a + (size_t)w > b->n) break;
            for (int i = 0; i < w; i++)
                b->o[a + (size_t)i] = (unsigned char)(v >> (8 * (w - 1 - i)));
            break;
        }
        case 3: b->n = at; break;
        case 4: {               /* une rafale d'octets */
            size_t L = 1 + rn(64);
            for (size_t i = 0; i < L && at + i < b->n; i++) b->o[at + i] = (unsigned char)r32();
            break;
        }
        case 5: {               /* un morceau d'une autre graine, par-dessus */
            const Buf *g = &graines[rn((size_t)ngraines)];
            if (!g->n) break;
            size_t p = rn(g->n), L = 1 + rn(256);
            if (p + L > g->n) L = g->n - p;
            if (at + L > b->n) L = b->n - at;
            memcpy(b->o + at, g->o + p, L);
            break;
        }
        case 6: {               /* un bloc dupliqué */
            size_t L = 1 + rn(512);
            if (at + L > b->n) L = b->n - at;
            recopie(b, at, at + L, rn(b->n + 1));
            break;
        }
        }
    }
}

/* ═══ CE QUE FAIT LE FILS — ce que fait l'application à l'ouverture ═══ */

static void note(const char *c)
{
    const char *p = getenv("FZ_STATS");
    if (!p) return;
    FILE *f = fopen(p, "a");
    if (f) { fputs(c, f); fclose(f); }
}

/* L'aller-retour par le disque : ce qui s'est lu doit s'écrire et se relire. */
static void aller_retour(Object *st, const char *chemin)
{
    char deux[512];
    snprintf(deux, sizeof deux, "%s.bis", chemin);
    if (hc_save(st, deux) == 0) {
        Object *re = hc_load(deux);
        if (re) hc_free(re);
    }
    unlink(deux);
}

#ifdef CANARI
/* LES TROIS PIÈGES DU TÉMOIN, déclenchés par la première ligne de la graine.
 * Ils ne servent qu'à prouver que l'instrument entend : jamais compilés dans
 * le pilote de campagne. */
static void piege(const char *chemin)
{
    char t[64] = { 0 };
    FILE *f = fopen(chemin, "rb");
    if (f) { if (fread(t, 1, sizeof t - 1, f)) { } fclose(f); }
    if (strstr(t, "CANARI_DEBORDE")) { volatile char *p = malloc(8); p[8] = 1; free((void *)p); }
    if (strstr(t, "CANARI_FUIT"))    { volatile char *p = malloc(100); p[0] = 1; p = NULL; }
    if (strstr(t, "CANARI_BOUCLE"))  { for (volatile int i = 0; ; i++) { } }
}
#endif

static void tourne_hc(const char *chemin)
{
#ifdef CANARI
    piege(chemin);
#endif
    Object *st = hc_load(chemin);
    note(st ? "L" : "R");
    if (!st) return;
    aller_retour(st, chemin);
    hc_free(st);
}

/* Le même chemin qu'importeAuFormatHyperCard: dans AppDelegate.m. */
static void tourne_orig(const unsigned char *octets, size_t n, const char *chemin)
{
    const unsigned char *ress = NULL;
    size_t nress = 0;
    char pourquoi[200];
    HcMacBinaire mb;
    if (hc_macbinaire(octets, n, &mb)) {
        octets = mb.donnees;  n = mb.ndonnees;
        ress = mb.ressources; nress = mb.nressources;
        free(hc_origine_utf8((const unsigned char *)mb.nom, strlen(mb.nom)));
    }
    HcOrigPile orig;
    if (hc_origine_lit(octets, n, &orig, pourquoi, sizeof pourquoi) != 0) {
        note("R");
        if (ress) {             /* la ressource se lit même quand la pile est refusée */
            HcOrigRessources r;
            hc_origine_ressources(ress, nress, &r, pourquoi, sizeof pourquoi);
            hc_origine_ressources_libere(&r);
        }
        return;
    }
    Object *st = hc_importe_pile(&orig, "Pile");
    hc_origine_libere(&orig);
    note(st ? "L" : "I");
    if (!st) return;
    if (ress) {
        HcOrigRessources r;
        if (hc_origine_ressources(ress, nress, &r, pourquoi, sizeof pourquoi) == 0)
            hc_importe_icones(&r, st);
        hc_origine_ressources_libere(&r);
    }
    aller_retour(st, chemin);
    hc_free(st);
}

static void ligne_muette(HcLineKind k, int d, const char *t) { (void)k; (void)d; (void)t; }

static void monte_hote(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ligne_muette;
    hc_set_host(&h);
}

/* Un essai. Rend 1 si le fils est mort autrement que par une sortie propre. */
static int essai(int mode_orig, const Buf *b, const char *sortie, const char *nom)
{
    char chemin[512], journal[512];
    snprintf(chemin, sizeof chemin, "/tmp/hcfuzz_%d_%s.stack", (int)getpid(), nom);
    snprintf(journal, sizeof journal, "/tmp/hcfuzz_%d_%s.log", (int)getpid(), nom);
    FILE *f = fopen(chemin, "wb");
    if (!f) { perror(chemin); exit(2); }
    fwrite(b->o, 1, b->n, f);
    fclose(f);

    const char *al = getenv("FZ_ALARME");
    unsigned alarme = al ? (unsigned)atoi(al) : 10;

    pid_t p = fork();
    if (p == 0) {
        int fd = open(journal, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        int nul = open("/dev/null", O_WRONLY);
        if (fd >= 0) dup2(fd, 2);
        if (nul >= 0) dup2(nul, 1);
        alarm(alarme);
        monte_hote();
        if (mode_orig) tourne_orig(b->o, b->n, chemin); else tourne_hc(chemin);
        /* exit et PAS _exit : LeakSanitizer fait son contrôle à la sortie. Le
         * fichier reste : c'est le PÈRE qui l'efface ou le garde. */
        exit(0);
    }
    int st;
    waitpid(p, &st, 0);
    int mauvais = !(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    if (mauvais) {
        char d[1024];
        snprintf(d, sizeof d, "%s/cas_%s_%s%d.bin", sortie, nom,
                 WIFSIGNALED(st) ? "signal" : "code",
                 WIFSIGNALED(st) ? WTERMSIG(st) : WEXITSTATUS(st));
        rename(chemin, d);
        snprintf(d, sizeof d, "%s/cas_%s.log", sortie, nom);
        rename(journal, d);
    } else {
        unlink(chemin);
        unlink(journal);
    }
    return mauvais;
}

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "un")) {
        monte_hote();
        Buf b;
        if (!lit_fichier(argv[3], &b)) return 2;
        if (!strcmp(argv[2], "orig")) tourne_orig(b.o, b.n, "/tmp/hcfuzz_un");
        else tourne_hc(argv[3]);
        free(b.o);
        return 0;
    }

    if (argc == 5 && !strcmp(argv[1], "temoin")) {
        int mode_orig = !strcmp(argv[2], "orig");
        DIR *d = opendir(argv[3]);
        struct dirent *e;
        if (!d) { perror(argv[3]); return 2; }
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char p[4096];
            snprintf(p, sizeof p, "%s/%s", argv[3], e->d_name);
            Buf b;
            if (!lit_fichier(p, &b)) continue;
            printf("%-28s %s\n", e->d_name,
                   essai(mode_orig, &b, argv[4], e->d_name) ? "SIGNALÉ" : "propre");
            free(b.o);
        }
        closedir(d);
        return 0;
    }

    if (argc != 6) {
        fprintf(stderr, "fuzz hc|orig <graines> <n> <sortie> <germe>\n"
                        "fuzz temoin hc|orig <graines> <sortie>\n"
                        "fuzz un hc|orig <fichier>\n");
        return 2;
    }
    int mode_orig = !strcmp(argv[1], "orig");
    charge_graines(argv[2]);
    long n = atol(argv[3]);
    const char *sortie = argv[4];
    rs = 0x9E3779B97F4A7C15ull ^ (unsigned long long)atoll(argv[5]);
    if (!ngraines) { fprintf(stderr, "aucune graine dans %s\n", argv[2]); return 2; }

    long mauvais = 0;
    for (long i = 0; i < n; i++) {
        const Buf *g = &graines[rn((size_t)ngraines)];
        Buf b = { malloc(g->n + 1), g->n };
        if (!b.o) return 2;
        memcpy(b.o, g->o, g->n);
        if (mode_orig) mute_binaire(&b); else mute_texte(&b);
        char nom[32];
        snprintf(nom, sizeof nom, "%ld", i);
        mauvais += essai(mode_orig, &b, sortie, nom);
        free(b.o);
        if ((i + 1) % 5000 == 0)
            fprintf(stderr, "[%s %s] %ld essais, %ld signalés\n", argv[1], argv[5], i + 1, mauvais);
    }
    printf("%s %ld %ld\n", argv[1], n, mauvais);
    return 0;
}
