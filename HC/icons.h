#ifndef icons_h
#define icons_h

#import <Cocoa/Cocoa.h>

// tableaux d'icônes ASCII 32x32 (déclarés extern, définis dans icons.m)
extern const char *ICON_PENCIL32[32];
extern const char *ICON_ERASER32[32];
extern const char *ICON_BUCKET32[32];
extern const char *ICON_LASSO32[32];
extern const char *ICON_FREEFORM32[32];

// dessine une icône ASCII 32x32 dans le rect (centrée)
void draw_icon_ascii(const char **icon, NSRect r);

#endif /* icons_h */
