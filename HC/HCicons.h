#ifndef hcicons_h
#define hcicons_h

#import <Cocoa/Cocoa.h>

/* Icones ICON 32x32 d'origine HyperCard (Apple / FileMaker).
   bits : 128 octets, 32 lignes de 4 octets, bit de poids fort a gauche. */
typedef struct {
    int   id;
    const char *name;
    unsigned char bits[128];
} HCIcon;

extern const HCIcon HCICONS[];
extern const int NUM_HCICONS;

/* Retourne l'icone d'identifiant donne, ou NULL. */
const HCIcon *hcicon_find(int id);

/* Dessine une icone 32x32 centree dans le rect (px = taille d'un pixel). */
void hcicon_draw(const HCIcon *ic, NSRect r, CGFloat px);

#endif /* hcicons_h */
