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
    FORTH_VOISINAGES = 8
};

/* Les mots qui lisent le voisinage d'une cellule. */
enum {
    V_CENTER, V_CENTERP,
    V_NORTH, V_SOUTH, V_EAST, V_WEST,
    V_NEAST, V_NWEST, V_SEAST, V_SWEST,
    V_NORTHP, V_SOUTHP, V_EASTP, V_WESTP,
    V_CW, V_CCW, V_OPP, V_CWP, V_CCWP, V_OPPP,
    V_PHASE, V_PHASEP, V_HORZ, V_VERT,
    V_ACENTER, V_ACENTERP,
    V_NOMBRE
};

/* LA DISPOSITION : pour chaque voisinage, le mot que porte chaque bit de
 * l'index ; -1 pour un bit qu'aucun mot ne lit. */
extern signed char forth_disposition[FORTH_VOISINAGES][FORTH_BITS];

/* Le nom d'un mot de voisinage, pour les relevés. */
const char *forth_nom_voisin(int v);

typedef struct {
    char plans[FORTH_PLANS][FORTH_TABLE + 1];  /* « 0 » et « 1 », zéro final */
    int  ecrits;        /* bit k : une MAKE-TABLE a rempli le plan k */
    int  voisinage;     /* FORTH_MOORE… */
    char rendu[512];    /* ce que FORTH() rend : la pile, ou la faute */
} ForthSortie;

/* Compile `source` : rend 0, ou -1 sur une faute, que `rendu` dit. */
int forth_compile(const char *source, ForthSortie *s);

#endif
