/* notes — LES NOTES DE « play » : lues (hc_notes_lis), jouées (hc_notes_rends),
 * et passées à l'hôte par le noyau (v3_cmd_play).
 *
 * HC ne jouait que le nom du son : « play "harpsichord" "c d e" » faisait un
 * seul coup de clavecin. Les piles d'Apple jouent des mélodies — « Découvrir
 * HyperCard » : « play harpsichord tempo 300 a#2q c3w ».
 *
 * MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II) par l'utilisatrice le 12 octobre,
 * au chronomètre de « wait until the sound is "done" » (section 1) : le tempo
 * par défaut, la noire, la ronde, la durée qui se garde, le silence, le point
 * et le triolet. HyperCard rend 2 à 4 ticks de plus, le temps de démarrer.
 *
 * NON MESURÉ, et dit comme tel : l'octave par défaut et l'octave qui se garde,
 * les numéros MIDI (section 2, banc demandé) ; les mots qui ne sont pas des
 * notes, les modificateurs sans lettre, les bornes (sections 3 et 4). */
#include "hc_core.h"
#include "hc_notes.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void lis(const char *texte, double tempo)
{
    HcNote n[64];
    int k = hc_notes_lis(texte, tempo, n, 64);
    printf("   %-28s tempo %-5g -> %d note(s), %6.1f ticks :",
           texte, tempo, k, hc_notes_duree(n, k) * 60.0);
    for (int i = 0; i < k && i < 8; i++) {
        if (n[i].hauteur < 0) printf(" r/%.3g", n[i].duree);
        else printf(" %d/%.3g", n[i].hauteur, n[i].duree);
    }
    printf("\n");
}

/* Les passages par zéro d'un signal : deux par période. */
static long passages(const float *x, long n)
{
    long c = 0;
    for (long i = 1; i < n; i++)
        if ((x[i - 1] < 0) != (x[i] < 0)) c++;
    return c;
}

static long derniere_non_nulle(const float *x, long n)
{
    for (long i = n - 1; i >= 0; i--) if (x[i] != 0.0f) return i;
    return -1;
}

static void rends(const char *texte, const float *src, long nsrc, double fe)
{
    HcNote n[16];
    int k = hc_notes_lis(texte, 120, n, 16);
    long lon = (long)(hc_notes_duree(n, k) * fe + 0.5);
    float *dst = malloc(sizeof *dst * (size_t)(lon > 0 ? lon : 1));
    hc_notes_rends(src, nsrc, fe, n, k, dst, lon);
    printf("   %-14s %6ld trames, %4ld passages par zéro, dernière trame non nulle %ld\n",
           texte, lon, passages(dst, lon), derniere_non_nulle(dst, lon));
    if (lon > 0)
        printf("   %-14s amplitude de la dernière trame : %.4f\n", "", (double)fabsf(dst[lon - 1]));
    free(dst);
}

/* --- l'hôte, pour le chemin complet --- */
static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_ERR) printf("      [ERR] %s\n", t);
}
static void joue(const char *nom) { printf("      hôte : play_sound « %s »\n", nom); }
static void coupe(void)           { printf("      hôte : stop_sound\n"); }
static void joue_notes(const char *nom, const HcNote *n, int k)
{
    printf("      hôte : play_notes « %s », %d note(s), %.3f s :", nom, k, hc_notes_duree(n, k));
    for (int i = 0; i < k && i < 8; i++) {
        if (n[i].hauteur < 0) printf(" r/%.3g", n[i].duree);
        else printf(" %d/%.3g", n[i].hauteur, n[i].duree);
    }
    if (k > 8) printf(" …");
    printf("\n");
}
static void fais(const char *l) { printf("   %s\n", l); hc_do(l); }

static HcHost h;

int main(void)
{
    /* L'hôte d'abord : le noyau annonce son exécuteur sur la sortie d'erreur
     * au premier appel, et l'annonce doit tomber en tête, pas au milieu. */
    memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.play_sound = joue; h.stop_sound = coupe; h.play_notes = joue_notes;
    hc_set_host(&h);
    hc_do("put empty into rien");
    fflush(stdout);

    puts("== 1. les sept relevés d'HyperCard (ticks mesurés, démarrage compris) ==");
    puts("   mesuré : 124   124   121   183   92   42   480");
    lis("c c c c", 0);
    lis("c c c c", 120);
    lis("cw", 120);
    lis("ch c c", 120);
    lis("cq r c", 120);
    lis("ce. ce3 cs", 120);
    lis("cw", 30);

    puts("\n== 2. les hauteurs : do4 = 60 (octave par défaut et MIDI : NON MESURÉS) ==");
    lis("c c4 c#4 db4 b3 a4", 120);
    lis("c5 c", 120);
    lis("cb4 b#3 bb eb5 e", 120);
    lis("60 64 67", 120);
    lis("60q 72h", 120);
    lis("CQ E G", 120);
    lis("rw", 120);

    puts("\n== 3. les durées qui se gardent, et les modificateurs seuls (NON MESURÉ) ==");
    lis("ce. c c", 120);
    lis("ce c. c", 120);
    lis("cq3 c c", 120);
    lis("cw x c", 120);
    lis("ct cx", 120);

    puts("\n== 4. les bornes ==");
    lis("c", -5);
    lis("c", NAN);
    lis("c", 0.25);
    lis("c99 c-1 300", 120);
    lis("c5 flute c c4z c", 120);
    lis("zut c tempo 9 d", 120);
    lis("", 120);
    lis("cw..............................", 120);
    {
        HcNote n[2];
        printf("   max = 2 sur « c d e f » -> %d\n", hc_notes_lis("c d e f", 120, n, 2));
        printf("   hc_notes_compte(« c  d\\te\\n ») = %d, (NULL) = %d\n",
               hc_notes_compte("c  d\te\n "), hc_notes_compte(NULL));
    }

    puts("\n== 5. le rendu : un la de synthèse à 441 Hz, une seconde, 44 100 trames ==");
    {
        const double fe = 44100;
        const long ns = 44100;
        float *sin441 = malloc(sizeof *sin441 * (size_t)ns);
        for (long i = 0; i < ns; i++)
            sin441[i] = (float)sin(2 * M_PI * 441.0 * ((double)i + 0.5) / fe);
        printf("   l'échantillon seul : %ld passages par zéro\n", passages(sin441, ns));
        rends("c4q", sin441, ns, fe);     /* 0,5 s à vitesse 1 : 441 passages */
        rends("c5q", sin441, ns, fe);     /* deux fois plus vite : 882 */
        rends("c3q", sin441, ns, fe);     /* deux fois plus lent : 220 */
        rends("g4q", sin441, ns, fe);     /* une quinte : ×1,498 */
        rends("rq c4q", sin441, ns, fe);  /* une demi-seconde de silence d'abord */
        rends("c4w", sin441, ns, fe);     /* 2 s : l'échantillon finit à 1 s */
        rends("c6w", sin441, ns, fe);     /* ×4 : il finit à 0,25 s */
        float un = 1.0f;
        rends("c4q", &un, 1, fe);         /* un échantillon d'une trame : silence */
        hc_notes_rends(NULL, 0, fe, NULL, 0, NULL, 0);
        puts("   (NULL partout : rien, et pas de plantage)");
        free(sin441);
    }

    puts("\n== 6. le chemin complet : le noyau lit, l'hôte reçoit ==");
    Object *st = hc_new_stack("Sons");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    Object *c = hc_new_card(st, bg, "Carte");
    hc_set_current_card(c);
    fais("play \"boing\" tempo 120 \"c d e\"");
    fais("play harpsichord tempo 300 a#2q c3w");
    fais("play \"boing\" \"c4q\" & \" e g\"");
    fais("put \"c d e f g a b c5 d5 e5 f5 g5 a5 b5 c6 d6 e6 f6 g6 a6 b6 c7 d7 e7 f7 g7 a7 b7 c8 d8 e8 f8 g8 a8 b8 c4 d4 e4 f4 g4 a4 b4 c5 d5 e5 f5 g5 a5 b5 c6 d6 e6 f6 g6 a6 b6\" into melodie");
    fais("put the length of melodie");
    fais("play \"boing\" tempo 240 melodie");
    fais("play \"boing\"");
    fais("play \"boing\" tempo 200");
    fais("play \"boing\" \"zut flute\"");
    fais("play stop");

    puts("\n== 7. un hôte sans play_notes : le son une fois, comme avant ==");
    h.play_notes = NULL;
    fais("play \"boing\" tempo 120 \"c d e\"");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
