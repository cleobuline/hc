/* hc_notes.c — les notes de « play », lues puis jouées. Voir hc_notes.h.
 *
 * LA GRAMMAIRE D'UNE NOTE, celle de HyperTalk :
 *
 *     nom [altération] [octave] [durée] [modificateurs]
 *
 *     nom           a à g, ou r pour un silence
 *     altération    # (dièse) ou b (bémol) : « c#4 », « eb »
 *     octave        un nombre ; do4 est le do du milieu, la note 60
 *     durée         w ronde, h blanche, q noire, e croche, s double croche,
 *                   t triple croche, x quadruple croche
 *     modificateurs . pointée (×1,5), 3 triolet (×2/3)
 *
 * Un numéro seul est une note MIDI : « 60 » est do4.
 *
 * MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II) par l'utilisatrice, le 12
 * octobre — une mélodie de clavecin chronométrée par « wait until the sound
 * is "done" », à 2 ou 4 ticks près, le temps de démarrer :
 *
 *     "c c c c"                 124 ticks   tempo 120 sans « tempo »
 *     tempo 120 "c c c c"       124         la noire, 30 ticks
 *     tempo 120 "cw"            121         la ronde, 4 noires
 *     tempo 120 "ch c c"        183         LA DURÉE SE GARDE d'une note à l'autre
 *     tempo 120 "cq r c"         92         le silence prend la durée courante
 *     tempo 120 "ce. ce3 cs"     42         22,5 + 10 + 7,5 : « . » ×1,5, « 3 » ×2/3
 *     tempo 30 "cw"             480         la ronde, 8 s
 *
 * NON MESURÉ, et choisi ici :
 *   · l'octave sans octave (4), et si elle se garde (oui) — banc demandé ;
 *   · les numéros MIDI — banc demandé ;
 *   · un mot qui n'est pas une note EN ENTIER (« flute », « c4z ») : sauté,
 *     sans rien changer à l'octave ni à la durée qui se gardent ;
 *   · des modificateurs sans lettre de durée (« c. ») : appliqués à la lettre
 *     d'avant ; la durée qui se garde est la DERNIÈRE ÉCRITE, modificateurs
 *     compris — « ce. c » donne deux croches pointées ;
 *   · une hauteur hors de 0 à 127, un tempo nul ou négatif : ramenés dans les
 *     bornes, le tempo au défaut ;
 *   · ce que fait une note plus longue que son échantillon : ici, le silence
 *     après lui ; une note plus courte coupe l'échantillon, sur un fondu. */
#include "hc_notes.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

/* Les demi-tons de a à g au-dessus du do de leur octave. */
static const int DEMI_TONS[7] = { 9, 11, 0, 2, 4, 5, 7 };

/* Une ronde vaut quatre noires ; chaque lettre suivante, la moitié. */
static const char LETTRES_DUREE[] = "whqestx";

/* Le fondu de fin d'une note coupée : 5 ms, assez court pour ne pas
 * s'entendre, assez long pour ne pas claquer. */
#define FONDU_SECONDES 0.005

/* Une note ne dure pas plus d'une heure : « cw...... » multiplie sans fin, et
 * une durée infinie ne se compte pas en trames. Une borne de prudence. */
#define DUREE_MAX 3600.0

int hc_notes_compte(const char *texte)
{
    int n = 0;
    const char *p = texte ? texte : "";
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        n++;
        while (*p && !isspace((unsigned char)*p)) p++;
    }
    return n;
}

static int borne_hauteur(long h)
{
    if (h < 0) return 0;
    if (h > 127) return 127;
    return (int)h;
}

int hc_notes_lis(const char *texte, double tempo, HcNote *notes, int max)
{
    if (!(tempo > 0)) tempo = HC_NOTES_TEMPO;     /* nul, négatif, NaN */
    if (tempo < 1) tempo = 1;
    const double noire = 60.0 / tempo;            /* une noire, en secondes */

    int octave = 4;
    double lettre = 1.0;                          /* en noires : q */
    double modif = 1.0;
    int n = 0;
    const char *p = texte ? texte : "";

    while (*p && n < max) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *mot = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        const char *finmot = p;
        const char *q = mot;

        /* Le mot se lit sur des copies : il ne change l'octave et la durée
         * qui se gardent que s'il est une note EN ENTIER. « flute » n'est
         * pas un fa suivi de « lute » : il est sauté. */
        int oct = octave;
        double let = lettre, mod = modif;
        int hauteur;
        if (isdigit((unsigned char)*q)) {
            long v = 0;
            while (q < finmot && isdigit((unsigned char)*q)) {
                if (v < 1000) v = v * 10 + (*q - '0');
                q++;
            }
            hauteur = borne_hauteur(v);
        } else {
            int c = tolower((unsigned char)*q);
            if (c == 'r') {
                hauteur = -1;
                q++;
            } else if (c >= 'a' && c <= 'g') {
                long h = DEMI_TONS[c - 'a'];
                q++;
                if (q < finmot && *q == '#')      { h++; q++; }
                else if (q < finmot && *q == 'b') { h--; q++; }
                if (q < finmot && isdigit((unsigned char)*q)) {
                    int o = 0;
                    while (q < finmot && isdigit((unsigned char)*q)) {
                        if (o < 100) o = o * 10 + (*q - '0');
                        q++;
                    }
                    oct = o;
                }
                hauteur = borne_hauteur(h + 12L * (oct + 1));
            } else {
                continue;                         /* pas une note : sauté */
            }
        }

        if (q < finmot) {
            const char *l = strchr(LETTRES_DUREE, tolower((unsigned char)*q));
            if (l && *l) {
                let = 4.0 / (double)(1 << (l - LETTRES_DUREE));
                mod = 1.0;
                q++;
            }
        }
        if (q < finmot && (*q == '.' || *q == '3')) {
            mod = 1.0;
            while (q < finmot && (*q == '.' || *q == '3')) {
                mod *= (*q == '.') ? 1.5 : 2.0 / 3.0;
                q++;
            }
        }
        if (q != finmot) continue;                /* un reste : pas une note */

        octave = oct;
        lettre = let;
        modif = mod;
        double d = lettre * modif * noire;
        notes[n].hauteur = hauteur;
        notes[n].duree = (d < DUREE_MAX) ? d : DUREE_MAX;
        n++;
    }
    return n;
}

double hc_notes_duree(const HcNote *notes, int n)
{
    double t = 0;
    for (int i = 0; i < n; i++) t += notes[i].duree;
    return t;
}

void hc_notes_rends(const float *src, long nsrc, double frequence,
                    const HcNote *notes, int n, float *dst, long ndst)
{
    if (!dst || ndst <= 0) return;
    memset(dst, 0, (size_t)ndst * sizeof *dst);
    if (!src || nsrc <= 0 || !(frequence > 0)) return;

    const long fondu = (long)(FONDU_SECONDES * frequence) + 1;
    double t = 0;
    for (int i = 0; i < n; i++) {
        long deb = (long)(t * frequence + 0.5);
        t += notes[i].duree;
        long fin = (long)(t * frequence + 0.5);
        if (deb >= ndst) break;
        if (fin > ndst) fin = ndst;
        if (notes[i].hauteur < 0 || fin <= deb) continue;

        /* La vitesse de relecture : un demi-ton, c'est 2^(1/12). */
        const double pas = pow(2.0, (notes[i].hauteur - 60) / 12.0);
        const long lon = fin - deb;
        for (long k = 0; k < lon; k++) {
            double pos = (double)k * pas;
            if (pos >= (double)(nsrc - 1)) break; /* l'échantillon est fini */
            long j = (long)pos;
            float fr = (float)(pos - (double)j);
            float v = src[j] + (src[j + 1] - src[j]) * fr;
            long reste = lon - k;
            if (reste < fondu) v *= (float)reste / (float)fondu;
            dst[deb + k] = v;
        }
    }
}
