#ifndef HCglobals_h
#define HCglobals_h
#import <Cocoa/Cocoa.h>
#import "hc_core.h"

/* La vue principale est declaree en avant : ce fichier n'a besoin que du nom
 * de la classe, pas de son interface. Cela evite un cycle d'inclusion avec
 * HCview.h, qui lui importe HCglobals.h. */
@class HCView;

typedef enum { TOOL_BROWSE, TOOL_BUTTON, TOOL_FIELD, TOOL_PENCIL, TOOL_ERASER,
               TOOL_LINE, TOOL_RECT, TOOL_OVAL, TOOL_FILL, TOOL_FREEFORM,
               TOOL_LASSO, TOOL_SELRECT, TOOL_BRUSH , TOOL_TEXT,TOOL_SPRAY} HCTool;

typedef enum { INK_BLACK, INK_WHITE, INK_ERASE } HCInk;

/* ---- Etat des outils de dessin ---- */
extern HCTool   gTool;
extern HCInk    gInk;
extern int      gPattern;
extern int      gLineWidth;
extern int      gBrush;
extern BOOL     gShapeFilled;
extern int      gTextSize;
extern BOOL     gTransparentBg;
/* Couleur d'encre et couleur de fond du dessin.
 *
 * gInk reste ce qu'il etait : un MODE — peindre, peindre en fond, effacer.
 * Ces deux couleurs disent seulement AVEC QUOI. En noir et blanc elles valent
 * noir et blanc, et le comportement d'origine est alors le cas particulier,
 * sans branche supplementaire nulle part. */
extern NSColor *gInkColor;
extern NSColor *gBackColor;
/* ---- Selection et vue ---- */
extern Object  *gSelected;
extern Object  *gFontTarget;   /* objet vise par le panneau des polices */
extern HCView  *gView;

/* ---- Presse-papiers peinture ---- */
extern NSBitmapImageRep *gClipboard;
extern int      gClipW, gClipH;
extern NSPoint  gClipPts[4096];
extern int      gClipPtsCount;

/* ---- Cache des bitmaps de peinture ---- */
extern NSMutableDictionary *gPaintCache;
void hc_colors_init(void);
#endif
