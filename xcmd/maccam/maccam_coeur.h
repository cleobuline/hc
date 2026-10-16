/* maccam_coeur.h — la machine de MAC•CAM, sans HC autour.
 *
 * La fenêtre « CAM » de la pile MAC•CAM Workshop est une XFCN, MACCAM, écrite
 * au début des années 90 avec THINK C. Ce fichier en refait la MACHINE — la
 * grille, les tables, le pas —, traduite de son code 68000, lu et désassemblé
 * (docs/mesures/xcmd.txt) ; maccam.c en fera le module, avec sa fenêtre.
 *
 * LA GRILLE est une image de w × h pixels de 8 bits, la fenêtre entière. Le
 * plan 0 est le bit 7, le plan 3 le bit 4 ; les bits 3 à 0 retombent à zéro
 * au premier pas. Seul l'intérieur se calcule : la bordure d'un pixel est
 * refaite après chaque pas par la ligne et la colonne opposées — un TORE de
 * (w - 2) × (h - 2) cellules.
 *
 * DEUX DEMI-MACHINES. CAM-A calcule les plans 0 et 1 par la table lutA,
 * CAM-B les plans 2 et 3 par lutB ; le nouveau pixel est
 * lutA[indexA] + lutB[indexB]. Chacune lit ses deux plans comme siens et le
 * centre de l'autre comme « & ». L'ordre des bits de l'index est celui des
 * tables du FORTH (forth_coeur.c) : les deux programmes s'accordent. */

#ifndef MACCAM_COEUR_H
#define MACCAM_COEUR_H

#define CAM_TABLE 4096

typedef struct {
    int w, h;                   /* la grille, sa bordure comprise */
    unsigned char *pix;         /* l'état : (h + 2) × w, comme l'original */
    unsigned char *neuf;        /* le second tampon, de même taille */
    unsigned char lutA[CAM_TABLE];  /* bits 0x80 (plan 0) et 0x40 (plan 1) */
    unsigned char lutB[CAM_TABLE];  /* bits 0x20 (plan 2) et 0x10 (plan 3) */
    int major, minor, alternate, phaseA, phaseB;
} Cam;

/* Le voisinage que suit un pas : « major », ou l'un de ceux qu'imposent
 * stepmfast (Margolus) et stepvfast (von Neumann). */
enum { CAM_SELON_MAJOR = -1, CAM_MARGOLUS = 2, CAM_VONN = 1 };

int  cam_ouvre(Cam *c, int w, int h);   /* 0, ou -1 (mémoire, taille) */
void cam_ferme(Cam *c);

/* « set the pln7 of window "CAM" to … » : pln7 à pln4, un bit de lutA ou de
 * lutB ; les autres ne font rien, comme dans l'original. */
void cam_pose_pln(Cam *c, int n, const char *texte);
/* La table d'un plan, en 4 096 « 0 » ou « 1 » et un zéro final. */
void cam_lit_pln(const Cam *c, int n, char *texte);

/* Un pas : le balayage, puis les deux phases basculent. */
void cam_pas(Cam *c, int voisinage);

/* fill1 à fill8 (bit 0 à 7), erasea, erase1 et erase2 (bit 0 et 1). */
void cam_remplit(Cam *c, int bit);
void cam_efface(Cam *c);
void cam_efface_bit(Cam *c, int bit);

/* « set the image7 of window "CAM" to <rect> » : un dessin de rw × rh
 * points, 1 pour noir, dont chaque point noir ALLUME le bit du plan. */
void cam_image(Cam *c, int bit, const unsigned char *noir, int rw, int rh);

/* La couleur d'un pixel : la palette officielle du moteur MacCam moderne,
 * plan 0 blanc, plan 1 cyan, CAM-B jaune, orange, vert, le vide noir. */
void cam_couleur(unsigned char pixel, unsigned char rgb[3]);

#endif
