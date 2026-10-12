/* hc_notes.h — LES NOTES DE « play », lues puis jouées.
 *
 *     play "harpsichord" tempo 120 "c4q e g c5h"
 *
 * HC ne jouait que le nom du son, une fois : le reste demandait un
 * synthétiseur. Il n'en faut pas : HyperCard jouait chaque note en relisant
 * son échantillon plus vite ou plus lentement, un do5 deux fois plus vite
 * qu'un do4. C'est ce que fait ce module, en deux temps.
 *
 *   1. hc_notes_lis lit la chaîne en notes : une hauteur (numéro MIDI, do4 =
 *      60) et une durée en secondes. Le noyau s'en sert (v3_cmd_play), et le
 *      passe à l'hôte.
 *   2. hc_notes_rends fabrique la mélodie, un canal à la fois, à partir de
 *      l'échantillon du son. L'hôte s'en sert, et joue le tout comme un son.
 *
 * Tout est en C, sans rien d'Apple : les deux se vérifient dans les harnais
 * (notes), sous ASan.
 *
 * L'ÉCHANTILLON EST UN DO4. Un son joué à sa vitesse normale donne la note
 * 60 ; un instrument enregistré sur une autre note joue toutes ses mélodies
 * transposées d'autant. Les sons d'HyperCard portaient leur note de base ;
 * un fichier .wav n'en dit rien.
 *
 * Ce qui est MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II) et ce qui ne l'est pas
 * est écrit dans hc_notes.c, règle par règle, et dans docs/mesures/notes.txt. */
#ifndef HC_NOTES_H
#define HC_NOTES_H

typedef struct {
    int    hauteur;     /* numéro MIDI, 0 à 127 ; -1 pour un silence */
    double duree;       /* en secondes */
} HcNote;

/* Le tempo sans « tempo » : 120 noires par minute (mesuré). */
#define HC_NOTES_TEMPO 120.0

/* Au plus tant de notes par « play » : une mélodie plus longue est coupée.
 * Une borne de prudence, pas une mesure. */
#define HC_NOTES_MAX 10000

/* Combien de notes `texte` peut donner au plus : ses mots. De quoi allouer
 * le tableau de hc_notes_lis. */
int hc_notes_compte(const char *texte);

/* Lit `texte` au `tempo` donné (noires par minute ; 0 ou moins : le défaut),
 * range au plus `max` notes dans `notes`, et rend leur nombre. Un mot qui
 * n'est pas une note est sauté. */
int hc_notes_lis(const char *texte, double tempo, HcNote *notes, int max);

/* La durée de la mélodie, en secondes. */
double hc_notes_duree(const HcNote *notes, int n);

/* La mélodie, sur un canal : `src` est l'échantillon (nsrc trames), joué à
 * `frequence` trames par seconde ; `dst` reçoit ndst trames — le silence au
 * delà de la dernière note. Une note coupe la précédente, sur un court
 * fondu pour ne pas claquer. */
void hc_notes_rends(const float *src, long nsrc, double frequence,
                    const HcNote *notes, int n, float *dst, long ndst);

#endif
