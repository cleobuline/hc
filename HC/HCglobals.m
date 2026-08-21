#import "HCglobals.h"

/* Toutes les variables globales de l'interface vivent ici, et nulle part
 * ailleurs. gTextSize et gTransparentBg viennent de HCview.m, gFontTarget de
 * HCdialogs.m : penser a supprimer leurs definitions la-bas. */

/* ---- Etat des outils de dessin ---- */
HCTool  gTool         = TOOL_BROWSE;
HCInk   gInk          = INK_BLACK;
int     gPattern      = 19;
int     gLineWidth    = 2;
int     gBrush        = 5;
BOOL    gShapeFilled  = NO;
int     gTextSize     = 16;
BOOL    gTransparentBg = NO;
NSColor *gInkColor  = nil;   /* noir  — voir hc_colors_init() */
NSColor *gBackColor = nil;   /* blanc — idem */

/* ---- Selection et vue ---- */
Object *gSelected    = NULL;
Object *gFontTarget  = NULL;
HCView *gView        = nil;

/* ---- Presse-papiers peinture ---- */
NSBitmapImageRep *gClipboard = nil;
int     gClipW = 0, gClipH = 0;
NSPoint gClipPts[4096];
int     gClipPtsCount = 0;

/* ---- Cache des bitmaps de peinture ---- */
NSMutableDictionary *gPaintCache = nil;
/* Les couleurs ne peuvent pas s'initialiser a la declaration — un initialiseur
 * global doit etre une constante de compilation. Appele une fois au demarrage,
 * depuis le meme endroit que le branchement des callbacks du noyau. */
void hc_colors_init(void) {
    if (!gInkColor)  gInkColor  = [NSColor blackColor];
    if (!gBackColor) gBackColor = [NSColor whiteColor];
}
