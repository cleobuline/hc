/* forth_coeur.h — le FORTH de MAC•CAM, sans HC autour.
 *
 * MAC•CAM Workshop, la pile de l'utilisatrice (début des années 90, THINK C),
 * compile ses règles par une XFCN FORTH : « put FORTH(bg fld "forth") into
 * bg fld "pile" », puis les tables partent dans les globales pln0 à pln3.
 * Ce fichier refait ce compilateur, pour HC ; forth.c en fait le module.
 *
 * Une TABLE est le texte de 4 096 caractères « 0 » ou « 1 » que la pile range
 * dans ses champs Pln0 à PLN3 : la nouvelle valeur d'une cellule pour chacun
 * des 4 096 états de son voisinage. L'ORDRE DES BITS de l'index dépend du
 * voisinage déclaré (N/MOORE, N/VONN, N/MARG-PH…). Il est LU dans le code du
 * FORTH d'origine, qui en porte les huit tables ; trois sont aussi MESURÉES
 * sur les tables que la pile garde, et le moteur de MACCAM fabrique ses
 * index de la même façon (docs/mesures/xcmd.txt). */

#ifndef FORTH_COEUR_H
#define FORTH_COEUR_H

#define FORTH_TABLE  4096
#define FORTH_BITS   12
#define FORTH_PLANS  4

/* LES VOISINS SUR MESURE, N/CUSTOM, l'ajout de HC décidé par l'utilisatrice
 * le 9 novembre, repris de son MacCam de bureau (cleobuline/maccam-6,
 * cam_forth.c) : jusqu'à douze voisins déclarés par leurs déplacements, plus
 * RAND, soit treize bits et une table de 8 192 cases au plus — deux fois
 * celles de l'original, pour ces règles-là seulement. */
#define FORTH_CUSTOM_MAX 12
#define FORTH_TABLE_MAX  8192

/* Les voisinages, et le numéro que les scripts de la pile donnent à chacun
 * (« set the major of window "CAM" to … », dans leur gestionnaire load). */
enum {
    FORTH_MOORE    = 0,
    FORTH_VONN     = 1,
    FORTH_MARG_PH  = 2,
    FORTH_MARG_HV  = 3,
    FORTH_MOORE_PH = 4,
    FORTH_MOORE_HV = 5,
    FORTH_VONN_PH  = 6,
    FORTH_VONN_HV  = 7,
    /* LE HASARD, l'ajout de HC décidé par l'utilisatrice le 7 novembre :
     * les mêmes voisinages, où un bit de l'index devient RAND, tiré au sort
     * à chaque pas (maccam_coeur.h). L'original n'en avait aucun ; ses huit
     * voisinages ne changent pas. */
    FORTH_MOORE_RAND = 8,
    FORTH_VONN_RAND  = 9,
    FORTH_MARG_RAND  = 10,
    /* LES VOISINS SUR MESURE, de HC aussi (9 novembre) : voir plus haut. */
    FORTH_CUSTOM     = 11,
    FORTH_VOISINAGES = 12
};

/* UNE DÉCLARATION N/CUSTOM, dans la syntaxe du MacCam de bureau :
 *
 *     N/CUSTOM  dx dy plan [NOM]  dx dy plan [NOM] …  END-CUSTOM
 *
 * dx > 0 vers l'est, dy > 0 vers le NORD (le haut de l'écran) — la boussole
 * du livre de Toffoli et Margolus, celle du bureau ; plan 0 ou 1, les plans
 * propres de la demi-machine : CAM-A lit les plans 0 et 1, CAM-B, aux mêmes
 * déplacements, les plans 2 et 3. Le bit i de l'index est le voisin i, dans
 * l'ordre de la déclaration ; le bit n, juste après le dernier, est RAND.
 * Comme au bureau, ce qui ne se comprend pas est ignoré : un triplet
 * incomplet, un plan autre que 0 ou 1, un déplacement au-delà de ±127, un
 * treizième voisin. */
typedef struct {
    int           n;                    /* les voisins retenus, 0 à 12 */
    signed char   dx[FORTH_CUSTOM_MAX];
    signed char   dy[FORTH_CUSTOM_MAX];
    unsigned char plan[FORTH_CUSTOM_MAX];
} ForthVoisins;

/* Les mots qui lisent le voisinage d'une cellule. */
enum {
    V_CENTER, V_CENTERP,
    V_NORTH, V_SOUTH, V_EAST, V_WEST,
    V_NEAST, V_NWEST, V_SEAST, V_SWEST,
    V_NORTHP, V_SOUTHP, V_EASTP, V_WESTP,
    V_CW, V_CCW, V_OPP, V_CWP, V_CCWP, V_OPPP,
    V_PHASE, V_PHASEP, V_HORZ, V_VERT,
    V_ACENTER, V_ACENTERP,
    V_RAND,
    V_NOMBRE
};

/* LA DISPOSITION : pour chaque voisinage, le mot que porte chaque bit de
 * l'index ; -1 pour un bit qu'aucun mot ne lit. */
extern signed char forth_disposition[FORTH_VOISINAGES][FORTH_BITS];

/* Le nom d'un mot de voisinage, pour les relevés. */
const char *forth_nom_voisin(int v);

typedef struct {
    /* « 0 » et « 1 » ; un zéro final, et un zéro là où rien n'a été écrit :
     * le texte s'arrête à la première entrée qu'aucun >PLNk n'a remplie. */
    char plans[FORTH_PLANS][FORTH_TABLE_MAX + 1];
    int  ecrits;        /* bit k : un >PLNk a écrit dans le plan k */
    int  voisinage;     /* FORTH_MOORE… */
    ForthVoisins voisins;   /* la dernière déclaration N/CUSTOM, n = 0 sinon */
    int  bips;          /* les BEEP : l'original appelait SysBeep */
    char faute[160];    /* la ligne qu'il envoyait à la carte, ou "" */
    char rendu[3000];   /* ce que FORTH() rend : la pile, sommet en tête */
} ForthSortie;

/* Compile et exécute `source` : rend 0, ou -1 sur une faute, que `faute`
 * dit dans la forme de l'original — une ligne de HyperTalk, faite pour être
 * envoyée à la carte. `rendu` porte la pile dans les deux cas. */
int forth_compile(const char *source, ForthSortie *s);

/* Les voisins de la DERNIÈRE déclaration N/CUSTOM du texte, lus comme le
 * compilateur les lit — mêmes mots, mêmes commentaires —, sans rien
 * exécuter : c'est ce que MACCAM demande au texte FORTH de la carte. Rend le
 * nombre de voisins retenus, ou -1 si le texte n'en déclare pas. */
int forth_voisins(const char *source, ForthVoisins *v);

#endif
