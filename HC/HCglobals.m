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
