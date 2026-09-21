#ifndef icons_h
#define icons_h

#import <Cocoa/Cocoa.h>

// tableaux d'icônes ASCII 32x32 (déclarés extern, définis dans icons.m)
extern const char *ICON_PENCIL32[32];
extern const char *ICON_ERASER32[32];
extern const char *ICON_BUCKET32[32];
extern const char *ICON_LASSO32[32];
extern const char *ICON_FREEFORM32[32];
extern const char *ICON_SELRECT32[32];
extern const char *ICON_BUTTON32[32] ;
extern const char *ICON_FIELD32[32] ;
/* Le rectangle arrondi et le polygone régulier. Dessinés par rastérisation
 * de la MÊME construction que shape_sommets, ce qui a servi deux fois : ces
 * icônes sont fidèles à ce que l'outil trace, et c'est en les regardant
 * qu'on a vu que la construction était fausse. */
/* La ligne brisée, avec ses sommets marqués. OUVERTE : c'est ce que
 * l'outil trace par défaut ; « filled » la referme, mais l'icône montre
 * le cas sans réglage, comme les autres. */
extern const char *ICON_POLY32[32];
extern const char *ICON_ROUNDRECT32[32];
extern const char *ICON_REGPOLY32[32];
extern const char *ICON_RECT32[32] ;
extern const char *ICON_LINE32[32];
extern const char *ICON_OVAL32[32];
extern const char *ICON_HAND32[32] ;
extern const char *ICON_BRUSH32[32];
extern const char *ICON_TEXT32[32];
extern const char *ICON_SPRAY32[32];
// dessine une icône ASCII 32x32 dans le rect (centrée)
void draw_icon_ascii(const char **icon, NSRect r);

#endif /* icons_h */
