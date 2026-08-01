#import "HCview.h"
#import "hc_core.h"
#import <objc/runtime.h>   // pour associer un bitmap à un Object
#import "icons.h"
#import "HCglobals.h"
#import "HCpalettes.h"
#import "HCicons.h"
static NSTextField *gMsgBox = nil; // la message box

static NSPoint gDragStart;
static NSRect  gDragRect;
static BOOL    gDragging = NO;
static int     gNewCount = 0;
static Object *gEditTarget = NULL;
static NSTextView *gEditView = nil;
static NSPanel *gEditPanel = nil;
static Object *gPressed = NULL;
static BOOL    gMoving = NO;        // on déplace l'objet sélectionné
static NSPoint gMoveStart;         // point de départ du déplacement
static int     gObjStartX, gObjStartY;  // position de l'objet au départ
static int     gResizeHandle = 0;  // 0 = pas de resize, 1..4 = coin saisi
static int     gObjStartW, gObjStartH;
static NSTextView *gFieldEditor = nil;
static Object     *gEditingField = NULL;
static NSPoint gPenLast;
static BOOL    gPenDrawing = NO;
static BOOL gEditBackground = NO;   // NO = couche carte, YES = couche fond
// peint un segment de ligne dans le bitmap de la carte courante
static NSPoint gShapeStart;
static NSPoint gShapeEnd;
static BOOL    gShapeDrawing = NO;
 
 
static NSPoint gLassoPts[4096];
static int gLassoCount = 0;
static BOOL gLassoDrawing = NO;
static BOOL gLassoActive = NO;   // une sélection existe-t-elle ?


static NSPoint gSelStart, gSelEnd; // selection rectabgle
static BOOL gSelRectDrawing = NO;
static BOOL gSelRectActive = NO;

static NSPoint gClipPts[4096];   // contour de la selection collee (relatif au coin)
static int gClipPtsCount = 0;    // 0 = selection rectangulaire
static NSPanel *gPatternPanel = nil;
static NSPanel *gWidthPanel = nil;
static NSPanel *gBrushPanel = nil;

BOOL gTransparentBg = NO;

static BOOL gTextActive = NO;
static NSPoint gTextPos;                 // coin haut-gauche de la saisie
static NSMutableString *gTextBuf = nil;

int gTextSize = 16;
static NSColor *gTextColor = nil;
static BOOL gTextUnderline = NO;
static void paint_shape(NSBitmapImageRep *rep, HCTool tool, NSPoint a, NSPoint b, NSColor *color, CGFloat width);
// ==================== motifs de remplissage (8x8, façon QuickDraw) ====================

static const unsigned char PATTERNS[38][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},   /* 19  blanc     */
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},   /*  0  noir      */
    {0xDD,0xFF,0x77,0xFF,0xDD,0xFF,0x77,0xFF},   /*  1  87.5%     */
    {0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77},   /*  2  75%       */
    {0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55},   /*  3  50%       */
    {0x55,0xFF,0x55,0xFF,0x55,0xFF,0x55,0xFF},   /*  4            */
    {0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA},   /*  5  lignes V  */
    {0xEE,0xDD,0xBB,0x77,0xEE,0xDD,0xBB,0x77},   /*  6  diagonale */
    {0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88},   /*  7            */
    {0xB1,0x30,0x03,0x1B,0xD8,0xC0,0x0C,0x8D},   /*  8            */
    {0x80,0x10,0x02,0x20,0x01,0x08,0x40,0x04},   /*  9            */
    {0xFF,0x88,0x88,0x88,0xFF,0x88,0x88,0x88},   /* 10  grille    */
    {0xFF,0x80,0x80,0x80,0xFF,0x08,0x08,0x08},   /* 11  briques   */
    {0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00},   /* 12  1.5%      */
    {0x80,0x40,0x20,0x00,0x02,0x04,0x08,0x00},   /* 13            */
    {0x82,0x44,0x39,0x44,0x82,0x01,0x01,0x01},   /* 14            */
    {0xF8,0x74,0x22,0x47,0x8F,0x17,0x22,0x71},   /* 15  vannerie  */
    {0x55,0xA0,0x40,0x40,0x55,0x0A,0x04,0x04},   /* 16            */
    {0x20,0x50,0x88,0x88,0x88,0x88,0x05,0x02},   /* 17            */
    {0xBF,0x00,0xBF,0xBF,0xB0,0xB0,0xB0,0xB0},   /* 18            */
    {0x80,0x00,0x08,0x00,0x80,0x00,0x08,0x00},   /* 20  6%        */
    {0x88,0x00,0x22,0x00,0x88,0x00,0x22,0x00},   /* 21  12%       */
    {0x88,0x22,0x88,0x22,0x88,0x22,0x88,0x22},   /* 22  25%       */
    {0xAA,0x00,0xAA,0x00,0xAA,0x00,0xAA,0x00},   /* 23            */
    {0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00},   /* 24  lignes H  */
    {0x11,0x22,0x44,0x88,0x11,0x22,0x44,0x88},   /* 25  diagonale */
    {0xFF,0x00,0x00,0x00,0xFF,0x00,0x00,0x00},   /* 26            */
    {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80},   /* 27  diagonale */
    {0xAA,0x00,0x80,0x00,0x88,0x00,0x80,0x00},   /* 28            */
    {0xFF,0x80,0x80,0x80,0x80,0x80,0x80,0x80},   /* 29            */
    {0x08,0x1C,0x22,0xC1,0x80,0x01,0x02,0x04},   /* 30            */
    {0x88,0x14,0x22,0x41,0x88,0x00,0xAA,0x00},   /* 31            */
    {0x40,0xA0,0x00,0x00,0x04,0x0A,0x00,0x00},   /* 32            */
    {0x03,0x84,0x48,0x30,0x0C,0x02,0x01,0x01},   /* 33            */
    {0x80,0x80,0x41,0x3E,0x08,0x08,0x14,0xE3},   /* 34  poisson   */
    {0x10,0x20,0x54,0xAA,0xFF,0x02,0x04,0x08},   /* 35  fleche    */
    {0x77,0x89,0x8F,0x8F,0x77,0x98,0xF8,0xF8},   /* 36  tissage   */
    {0x00,0x08,0x14,0x2A,0x55,0x2A,0x14,0x08},   /* 37  losanges  */
};
#define NUM_PATTERNS 38

// #define NUM_PATTERNS (int)(sizeof(PATTERNS)/sizeof(PATTERNS[0]))


 // int gPattern = 2;   // motif courant (1 = noir plein)
static NSPoint gFreePts[4096];
static int gFreeCount = 0;
static BOOL gFreeDrawing = NO;


static NSBitmapImageRep *gClipboard = nil;   // presse-papier (zone copiée)
static int gClipW = 0, gClipH = 0;           // dimensions de la zone copiée


static BOOL gFloating = NO;        // un collage flotte-t-il ?
static NSPoint gFloatPos;          // position (coin haut-gauche) du flottant
static BOOL gFloatDragging = NO;   // en train de le déplacer ?
static NSPoint gFloatGrab;         // décalage entre le clic et le coin
static NSFont *gTextFont = nil;

// ==================== icônes bitmap 16x16 (1 = pixel noir) ====================
// dessine chaque ligne en binaire : le motif est lisible directement dans le code




  inline int pattern_bit(int pat, int x, int y) {
    unsigned char row = PATTERNS[pat][y & 7];
    return (row >> (7 - (x & 7))) & 1;
}
// convertit une zone en noir et blanc trame (Floyd-Steinberg)
// poly != NULL : ne trame que l'interieur du polygone
static void dither_region(NSBitmapImageRep *rep, int x0, int y0, int x1, int y1,
                          NSPoint *poly, int npoly)
{
    if (!rep) return;
    if ([rep bitsPerSample] != 8 || [rep samplesPerPixel] < 3) return;
    int W = (int)[rep pixelsWide], H = (int)[rep pixelsHigh];
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= W) x1 = W-1;
    if (y1 >= H) y1 = H-1;
    int w = x1-x0+1, h = y1-y0+1;
    if (w < 1 || h < 1) return;

    unsigned char *data = [rep bitmapData];
    if (!data) return;
    NSInteger bpr = [rep bytesPerRow], spp = [rep samplesPerPixel];

    double *lum = calloc((size_t)w*h, sizeof(double));
    unsigned char *use = calloc((size_t)w*h, 1);
    if (!lum || !use) { free(lum); free(use); return; }

    // luminance des pixels concernes
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int gx = x0+x, gy = y0+y;
            unsigned char *px = data + gy*bpr + gx*spp;
            unsigned char a = (spp>=4) ? px[3] : 255;
            if (a == 0) continue;                    // transparent : intact
            if (poly && npoly >= 3) {
                int inside = 0;
                for (int i=0, j=npoly-1; i<npoly; j=i++) {
                    double yi=poly[i].y, yj=poly[j].y, xi=poly[i].x, xj=poly[j].x;
                    if (((yi>gy)!=(yj>gy)) && (gx < (xj-xi)*(gy-yi)/(yj-yi)+xi))
                        inside = !inside;
                }
                if (!inside) continue;               // hors polygone : intact
            }
            use[y*w+x] = 1;
            lum[y*w+x] = 0.299*px[0] + 0.587*px[1] + 0.114*px[2];
        }
    }

    // diffusion d'erreur
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!use[y*w+x]) continue;
            double old = lum[y*w+x];
            double nv  = (old < 128.0) ? 0.0 : 255.0;
            double err = old - nv;
            lum[y*w+x] = nv;
            if (x+1 < w && use[y*w+x+1])          lum[y*w+x+1]     += err * 7.0/16.0;
            if (y+1 < h) {
                if (x > 0 && use[(y+1)*w+x-1])    lum[(y+1)*w+x-1] += err * 3.0/16.0;
                if (use[(y+1)*w+x])               lum[(y+1)*w+x]   += err * 5.0/16.0;
                if (x+1 < w && use[(y+1)*w+x+1])  lum[(y+1)*w+x+1] += err * 1.0/16.0;
            }
        }
    }

    // ecriture en noir ou blanc pur
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!use[y*w+x]) continue;
            unsigned char *px = data + (y0+y)*bpr + (x0+x)*spp;
            unsigned char v = (lum[y*w+x] < 128.0) ? 0 : 255;
            px[0]=v; px[1]=v; px[2]=v;
            if (spp>=4) px[3]=255;
        }
    }
    free(lum); free(use);
}
// remplit l'intérieur d'une forme (rect ou ovale) avec le motif + encre courants
static void fill_shape(NSBitmapImageRep *rep, HCTool tool, NSPoint a, NSPoint b) {
    if (!rep) return;
    int W = (int)[rep pixelsWide];
    int H = (int)[rep pixelsHigh];
    unsigned char *data = [rep bitmapData];
    if (!data) return;
    NSInteger bpr = [rep bytesPerRow];
    NSInteger spp = [rep samplesPerPixel];

    int x0 = (int)MIN(a.x, b.x), x1 = (int)MAX(a.x, b.x);
    int y0 = (int)MIN(a.y, b.y), y1 = (int)MAX(a.y, b.y);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 >= W) x1 = W-1; if (y1 >= H) y1 = H-1;

    // centre et demi-axes pour l'ovale
    double cx = (x0 + x1) / 2.0, cy = (y0 + y1) / 2.0;
    double rx = (x1 - x0) / 2.0, ry = (y1 - y0) / 2.0;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            // pour l'ovale, ne remplir que l'intérieur de l'ellipse
            if (tool == TOOL_OVAL) {
                if (rx < 1 || ry < 1) continue;
                double dx = (x - cx) / rx, dy = (y - cy) / ry;
                if (dx*dx + dy*dy > 1.0) continue;   // hors de l'ellipse
            }
            // (pour TOOL_RECT, tout le rectangle est rempli)

            unsigned char *px = data + y*bpr + x*spp;
            if (pattern_bit(gPattern, x, y)) {
                // trait du motif : toujours noir
                px[0]=0; px[1]=0; px[2]=0;
                if (spp>=4) px[3]=255;
            } else {
                                if (gInk == INK_ERASE) {
                                    px[0]=0; px[1]=0; px[2]=0;
                                    if (spp>=4) px[3]=0;          // efface
                                } else if (gTransparentBg) {
                                    continue;                      // laisser intact
                                } else {
                                    px[0]=255; px[1]=255; px[2]=255;
                                    if (spp>=4) px[3]=255;        // fond blanc opaque
                                }
                            }
        }
    }
}
// dessine une forme libre (contour fermé) à partir d'une liste de points
static void paint_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n, CGFloat width) {
    if (!rep || n < 2) return;
    if (width <= 0) return;   // épaisseur 0 : pas de contour

    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!ctx) return;
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:NO];

    CGFloat H = [rep pixelsHigh];
    NSAffineTransform *flip = [NSAffineTransform transform];
    [flip translateXBy:0 yBy:H];
    [flip scaleXBy:1 yBy:-1];
    [flip concat];

    if (gInk == INK_ERASE) {
        CGContextSetBlendMode([ctx CGContext], kCGBlendModeClear);
        [[NSColor blackColor] setStroke];
    } else if (gInk == INK_WHITE) {
        [[NSColor whiteColor] setStroke];
    } else {
        [[NSColor blackColor] setStroke];
    }

    NSBezierPath *path = [NSBezierPath bezierPath];
    [path moveToPoint:pts[0]];
    for (int i = 1; i < n; i++)
        [path lineToPoint:pts[i]];
    [path closePath];              // ferme le contour (relie au premier point)
    [path setLineWidth:width];
    [path stroke];

    [NSGraphicsContext restoreGraphicsState];
}

static void paint_shape(NSBitmapImageRep *rep, HCTool tool, NSPoint a, NSPoint b, NSColor *color, CGFloat width) {
    if (!rep) return;
    if (width <= 0 && tool != TOOL_LINE) return;   // épaisseur 0 : pas de contour (sauf ligne)
    if (width <= 0) width = 1;                       // une ligne garde au moins 1

    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!ctx) return;
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:NO];

    CGFloat H = [rep pixelsHigh];
    NSAffineTransform *flip = [NSAffineTransform transform];
    [flip translateXBy:0 yBy:H];
    [flip scaleXBy:1 yBy:-1];
    [flip concat];

    // encre : blanc, noir, ou effacement (comme paint_stroke)
    if (gInk == INK_ERASE) {
        CGContextSetBlendMode([ctx CGContext], kCGBlendModeClear);
        [[NSColor blackColor] setStroke];
    } else if (gInk == INK_WHITE) {
        [[NSColor whiteColor] setStroke];
    } else {
        [[NSColor blackColor] setStroke];
    }
    (void)color;   // l'encre gouverne la couleur désormais

    NSBezierPath *path = [NSBezierPath bezierPath];
    NSRect box = NSMakeRect(MIN(a.x,b.x), MIN(a.y,b.y), fabs(b.x-a.x), fabs(b.y-a.y));
    if (tool == TOOL_LINE) {
        [path moveToPoint:a];
        [path lineToPoint:b];
    } else if (tool == TOOL_RECT) {
        path = [NSBezierPath bezierPathWithRect:box];
    } else if (tool == TOOL_OVAL) {
        path = [NSBezierPath bezierPathWithOvalInRect:box];
    }
    [path setLineWidth:width];
    [path stroke];

    [NSGraphicsContext restoreGraphicsState];
}
// remplit la zone connexe autour de (sx,sy) avec du noir.
// Approche itérative avec une pile explicite (pas de récursion : robuste sur grandes zones).
static void flood_fill(NSBitmapImageRep *rep, int sx, int sy) {
    if (!rep) return;
    int W = (int)[rep pixelsWide];
    int H = (int)[rep pixelsHigh];
    if (sx < 0 || sx >= W || sy < 0 || sy >= H) return;

    unsigned char *data = [rep bitmapData];
    if (!data) return;
    NSInteger bpr = [rep bytesPerRow];
    NSInteger spp = [rep samplesPerPixel];

    #define PIX(x,y) (data + (y)*bpr + (x)*spp)

    unsigned char *sp = PIX(sx, sy);
    unsigned char sr = sp[0], sg = sp[1], sb = sp[2];
    unsigned char sa = (spp >= 4) ? sp[3] : 255;

    unsigned char *done = calloc((size_t)W * H, 1);
    if (!done) return;

    int cap = 4096, top = 0;
    int *xs = malloc(sizeof(int)*cap);
    int *ys = malloc(sizeof(int)*cap);
    xs[top]=sx; ys[top]=sy; top++;

    while (top > 0) {
        top--;
        int x = xs[top], y = ys[top];
        if (x < 0 || x >= W || y < 0 || y >= H) continue;
        if (done[y*W + x]) continue;
        done[y*W + x] = 1;

        unsigned char *px = PIX(x, y);
        unsigned char a = (spp >= 4) ? px[3] : 255;

        if (abs(px[0]-sr) > 40 || abs(px[1]-sg) > 40 ||
            abs(px[2]-sb) > 40 || abs(a-sa) > 40)
            continue;   // frontière

        if (pattern_bit(gPattern, x, y)) {
                    // trait du motif : TOUJOURS noir
                    px[0] = 0; px[1] = 0; px[2] = 0;
                    if (spp >= 4) px[3] = 255;
        } else {
            if (gInk == INK_ERASE) {
                        px[0]=0; px[1]=0; px[2]=0;
                        if (spp>=4) px[3]=0;              // efface tout
                    } else if (pattern_bit(gPattern, x, y)) {
                        px[0]=0; px[1]=0; px[2]=0;        // trait du motif : noir
                        if (spp>=4) px[3]=255;
                    } else if (gTransparentBg) {
                        /* fond : laisser intact */
                    } else {
                        px[0]=255; px[1]=255; px[2]=255;  // fond blanc opaque
                        if (spp>=4) px[3]=255;
                    }
                        }

        if (top + 4 >= cap) {
            cap *= 2;
            xs = realloc(xs, sizeof(int)*cap);
            ys = realloc(ys, sizeof(int)*cap);
        }
        xs[top]=x+1; ys[top]=y; top++;
        xs[top]=x-1; ys[top]=y; top++;
        xs[top]=x; ys[top]=y+1; top++;
        xs[top]=x; ys[top]=y-1; top++;
    }
    #undef PIX

    free(done);
    free(xs);
    free(ys);
}
// applique la brosse en un point (coin haut-gauche centre sur cx,cy)
static void brush_stamp(NSBitmapImageRep *rep, int cx, int cy) {
    int W = (int)[rep pixelsWide], H = (int)[rep pixelsHigh];
    unsigned char *data = [rep bitmapData];
    if (!data) return;
    NSInteger bpr = [rep bytesPerRow], spp = [rep samplesPerPixel];
    for (int by = 0; by < 16; by++) {
        for (int bx = 0; bx < 16; bx++) {
            if (!brush_bit(gBrush, bx, by)) continue;
            int x = cx - 8 + bx, y = cy - 8 + by;
            if (x < 0 || x >= W || y < 0 || y >= H) continue;
            unsigned char *px = data + y*bpr + x*spp;
                        if (gInk == INK_ERASE) {
                            px[0]=0; px[1]=0; px[2]=0;
                            if (spp>=4) px[3]=0;
                        } else if (pattern_bit(gPattern, x, y)) {
                            px[0]=0; px[1]=0; px[2]=0;          // trait du motif : noir
                            if (spp>=4) px[3]=255;
                        } else if (gTransparentBg) {
                            continue;                            // fond : laisser intact
                        } else {
                            px[0]=255; px[1]=255; px[2]=255;    // fond blanc opaque
                            if (spp>=4) px[3]=255;
                        }
        }
    }
}

// trace un segment au pinceau
static void brush_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to) {
    if (!rep) return;
    int x0=(int)from.x, y0=(int)from.y, x1=(int)to.x, y1=(int)to.y;
    int dx = abs(x1-x0), dy = abs(y1-y0);
    int sx = x0<x1 ? 1 : -1, sy = y0<y1 ? 1 : -1;
    int err = dx-dy;
    while (1) {
        brush_stamp(rep, x0, y0);
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}
static void paint_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, NSColor *color, CGFloat width) {
    if (!rep) return;
    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!ctx) return;
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:NO];

    CGFloat H = [rep pixelsHigh];
    NSAffineTransform *flip = [NSAffineTransform transform];
    [flip translateXBy:0 yBy:H];
    [flip scaleXBy:1 yBy:-1];
    [flip concat];

    // encre : blanc, noir, ou effacement (transparent)
    if (gInk == INK_ERASE) {
        CGContextSetBlendMode([ctx CGContext], kCGBlendModeClear);
        [[NSColor blackColor] setStroke];   // couleur ignorée en mode clear
    } else if (gInk == INK_WHITE) {
        [[NSColor whiteColor] setStroke];
    } else {
        [[NSColor blackColor] setStroke];
    }

    NSBezierPath *path = [NSBezierPath bezierPath];
    [path moveToPoint:from];
    [path lineToPoint:to];
    [path setLineWidth:width > 0 ? width : 1];
    [path setLineCapStyle:NSLineCapStyleRound];
    [path stroke];

    [NSGraphicsContext restoreGraphicsState];
}
// efface un segment (remet à transparent) au lieu de peindre
static void erase_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, CGFloat width) {
    if (!rep) return;

    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!ctx) return;
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:NO];
    CGFloat H = [rep pixelsHigh];
    NSAffineTransform *flip = [NSAffineTransform transform];
    [flip translateXBy:0 yBy:H];
    [flip scaleXBy:1 yBy:-1];
    [flip concat];

    CGContextSetBlendMode([ctx CGContext], kCGBlendModeClear);
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path moveToPoint:from];
    [path lineToPoint:to];
    [path setLineWidth:width];
    [path setLineCapStyle:NSLineCapStyleRound];
    [path stroke];

    [NSGraphicsContext restoreGraphicsState];
}
// récupère (ou crée) le bitmap de peinture d'une carte/fond
static NSMutableDictionary *gPaintCache = nil;  // clé = pointeur objet, valeur = NSBitmapImageRep

static NSBitmapImageRep *paint_bitmap(Object *o, int w, int h) {
    if (!gPaintCache) gPaintCache = [NSMutableDictionary dictionary];
    NSValue *key = [NSValue valueWithPointer:o];
    NSBitmapImageRep *rep = [gPaintCache objectForKey:key];
    if (rep && ((int)[rep pixelsWide] != w || (int)[rep pixelsHigh] != h)) {
        [gPaintCache removeObjectForKey:key];
        rep = nil;
    }
    if (rep) return rep;
    NSLog(@"paint_bitmap crée canvas %dx%d (bounds vue: %.0fx%.0f)",
              w, h, [gView bounds].size.width, [gView bounds].size.height);
    NSBitmapImageRep *canvas = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL
                          pixelsWide:w pixelsHigh:h
                       bitsPerSample:8 samplesPerPixel:4
                            hasAlpha:YES isPlanar:NO
                      colorSpaceName:NSCalibratedRGBColorSpace
                         bytesPerRow:0 bitsPerPixel:0];

    // effacer explicitement vers le transparent
    {
        NSGraphicsContext *cctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:canvas];
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:cctx];
        CGContextClearRect([cctx CGContext], CGRectMake(0, 0, w, h));
        [NSGraphicsContext restoreGraphicsState];
    }

    const char *b64 = hc_paint_of(o);
        if (b64 && *b64) {
            NSData *data = [[NSData alloc] initWithBase64EncodedString:
                             [NSString stringWithUTF8String:b64]
                             options:NSDataBase64DecodingIgnoreUnknownCharacters];
            NSBitmapImageRep *loaded = data ? [NSBitmapImageRep imageRepWithData:data] : nil;
            if (loaded) {
                NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:canvas];
                [NSGraphicsContext saveGraphicsState];
                [NSGraphicsContext setCurrentContext:ctx];
                [ctx setShouldAntialias:NO];
                // dessiner à taille réelle en HAUT-gauche (pas d'étirement)
                CGFloat lh = [loaded pixelsHigh];
                [loaded drawAtPoint:NSMakePoint(0, h - lh)];
                [NSGraphicsContext restoreGraphicsState];
            }
        }

    [gPaintCache setObject:canvas forKey:key];
    return canvas;
}
// éteint tous les radioButtons de la carte sauf 'keep'
static void radio_exclusive(Object *card, Object *keep) {
    if (!card) return;
    for (int i = 0; i < card->nparts; i++) {
        Object *o = card->parts[i];
        if (o->type == OBJ_BUTTON && o != keep && o->style &&
            (strcmp(o->style, "radioButton") == 0 || strcmp(o->style, "radiobutton") == 0))
            o->hilite = 0;
    }
    if (card->bg)
        for (int i = 0; i < card->bg->nparts; i++) {
            Object *o = card->bg->parts[i];
            if (o->type == OBJ_BUTTON && o != keep && o->style &&
                (strcmp(o->style, "radioButton") == 0 || strcmp(o->style, "radiobutton") == 0))
                o->hilite = 0;
        }
}
// efface (rend transparent) l'intérieur d'un polygone
static void erase_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n) {
    if (!rep || n < 3) return;
    int W = (int)[rep pixelsWide];
    int H = (int)[rep pixelsHigh];
    unsigned char *data = [rep bitmapData];
    if (!data) return;
    NSInteger bpr = [rep bytesPerRow];
    NSInteger spp = [rep samplesPerPixel];

    double minx=pts[0].x,maxx=pts[0].x,miny=pts[0].y,maxy=pts[0].y;
    for (int i=1;i<n;i++){
        if(pts[i].x<minx)minx=pts[i].x; if(pts[i].x>maxx)maxx=pts[i].x;
        if(pts[i].y<miny)miny=pts[i].y; if(pts[i].y>maxy)maxy=pts[i].y;
    }
    int y0=(int)floor(miny),y1=(int)ceil(maxy),x0=(int)floor(minx),x1=(int)ceil(maxx);
    if(y0<0)y0=0; if(x0<0)x0=0; if(y1>=H)y1=H-1; if(x1>=W)x1=W-1;

    for (int y=y0;y<=y1;y++){
        for (int x=x0;x<=x1;x++){
            int inside=0;
            for (int i=0,j=n-1;i<n;j=i++){
                double yi=pts[i].y,yj=pts[j].y,xi=pts[i].x,xj=pts[j].x;
                if(((yi>y)!=(yj>y)) && (x < (xj-xi)*(y-yi)/(yj-yi)+xi)) inside=!inside;
            }
            if(!inside) continue;
            unsigned char *px = data + y*bpr + x*spp;
            px[0]=0; px[1]=0; px[2]=0;
            if(spp>=4) px[3]=0;   // transparent
        }
    }
}
// scotche le presse-papier dans le bitmap à la position (haut-gauche) donnée
static void stamp_clipboard(NSBitmapImageRep *rep, NSPoint pos) {
    if (!rep || !gClipboard) return;
    int W = (int)[rep pixelsWide], H = (int)[rep pixelsHigh];
    int px0 = (int)pos.x, py0 = (int)pos.y;

    unsigned char *dst = [rep bitmapData];
    unsigned char *src = [gClipboard bitmapData];
    NSInteger dbpr = [rep bytesPerRow], dspp = [rep samplesPerPixel];
    NSInteger sbpr = [gClipboard bytesPerRow], sspp = [gClipboard samplesPerPixel];

    for (int y = 0; y < gClipH; y++) {
        for (int x = 0; x < gClipW; x++) {
            int dx = px0 + x, dy = py0 + y;
            if (dx < 0 || dx >= W || dy < 0 || dy >= H) continue;
            unsigned char *sp = src + y*sbpr + x*sspp;
            unsigned char sa = (sspp>=4) ? sp[3] : 255;
            if (sa == 0) continue;   // pixel transparent du presse-papier : ne pas écraser
            unsigned char *dp = dst + dy*dbpr + dx*dspp;
            dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2];
            if (dspp>=4) dp[3]=sa;
        }
    }
}
// copie une zone rectangulaire du bitmap dans le presse-papier
static void copy_rect(NSBitmapImageRep *rep, NSPoint a, NSPoint b) {
    if (!rep) return;
    
    int W = (int)[rep pixelsWide], H = (int)[rep pixelsHigh];
    int x0 = (int)MIN(a.x,b.x), x1 = (int)MAX(a.x,b.x);
    int y0 = (int)MIN(a.y,b.y), y1 = (int)MAX(a.y,b.y);
    if (x0<0)x0=0; if(y0<0)y0=0; if(x1>=W)x1=W-1; if(y1>=H)y1=H-1;
    int w = x1-x0+1, h = y1-y0+1;
    if (w < 1 || h < 1) return;

    // créer le bitmap presse-papier
    NSBitmapImageRep *clip = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL pixelsWide:w pixelsHigh:h
        bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
        colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:0 bitsPerPixel:0];

    unsigned char *src = [rep bitmapData];
    unsigned char *dst = [clip bitmapData];
    NSInteger sbpr = [rep bytesPerRow], sspp = [rep samplesPerPixel];
    NSInteger dbpr = [clip bytesPerRow], dspp = [clip samplesPerPixel];

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned char *sp = src + (y0+y)*sbpr + (x0+x)*sspp;
            unsigned char *dp = dst + y*dbpr + x*dspp;
            dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2];
            dp[3] = (sspp>=4) ? sp[3] : 255;
        }
    }

    gClipboard = clip;
        gClipW = w; gClipH = h;
        gClipPtsCount = 0;        // selection rectangulaire : pas de contour libre
    // aussi vers le presse-papier système
        NSData *tiff = [clip TIFFRepresentation];
        if (tiff) {
            NSImage *img = [[NSImage alloc] initWithData:tiff];
            NSPasteboard *pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            [pb writeObjects:@[img]];
        }
}
// copie l'interieur d'un polygone dans le presse-papier (hors polygone = transparent)
static void copy_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n) {
    if (!rep || n < 3) return;
    int W = (int)[rep pixelsWide], H = (int)[rep pixelsHigh];
    unsigned char *src = [rep bitmapData];
    if (!src) return;
    NSInteger sbpr = [rep bytesPerRow], sspp = [rep samplesPerPixel];

    // boite englobante
    double minx=pts[0].x, maxx=pts[0].x, miny=pts[0].y, maxy=pts[0].y;
    for (int i=1;i<n;i++){
        if(pts[i].x<minx)minx=pts[i].x; if(pts[i].x>maxx)maxx=pts[i].x;
        if(pts[i].y<miny)miny=pts[i].y; if(pts[i].y>maxy)maxy=pts[i].y;
    }
    int x0=(int)floor(minx), x1=(int)ceil(maxx);
    int y0=(int)floor(miny), y1=(int)ceil(maxy);
    if(x0<0)x0=0; if(y0<0)y0=0; if(x1>=W)x1=W-1; if(y1>=H)y1=H-1;
    int w = x1-x0+1, h = y1-y0+1;
    if (w < 1 || h < 1) return;
    gClipPtsCount = (n > 4096) ? 4096 : n;
     for (int i = 0; i < gClipPtsCount; i++)
         gClipPts[i] = NSMakePoint(pts[i].x - x0, pts[i].y - y0);
    NSBitmapImageRep *clip = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL pixelsWide:w pixelsHigh:h
        bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
        colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
    unsigned char *dst = [clip bitmapData];
    NSInteger dbpr = [clip bytesPerRow], dspp = [clip samplesPerPixel];

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            unsigned char *dp = dst + (y-y0)*dbpr + (x-x0)*dspp;
            // dedans ou dehors du polygone ?
            int inside = 0;
            for (int i=0, j=n-1; i<n; j=i++) {
                double yi=pts[i].y, yj=pts[j].y, xi=pts[i].x, xj=pts[j].x;
                if (((yi>y)!=(yj>y)) && (x < (xj-xi)*(y-yi)/(yj-yi)+xi))
                    inside = !inside;
            }
            if (!inside) {                       // dehors : transparent
                dp[0]=0; dp[1]=0; dp[2]=0; dp[3]=0;
                continue;
            }
            unsigned char *sp = src + y*sbpr + x*sspp;
            dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2];
            dp[3] = (sspp>=4) ? sp[3] : 255;
        }
    }

    gClipboard = clip;
    gClipW = w; gClipH = h;

    // presse-papier systeme
    NSData *tiff = [clip TIFFRepresentation];
    if (tiff) {
        NSImage *img = [[NSImage alloc] initWithData:tiff];
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb writeObjects:@[img]];
    }
}
static void erase_rect(NSBitmapImageRep *rep, NSPoint a, NSPoint b) {
if (!rep) return;
int W = (int)[rep pixelsWide], H = (int)[rep pixelsHigh];
unsigned char *data = [rep bitmapData];
if (!data) return;
NSInteger bpr = [rep bytesPerRow], spp = [rep samplesPerPixel];
int x0 = (int)MIN(a.x,b.x), x1 = (int)MAX(a.x,b.x);
int y0 = (int)MIN(a.y,b.y), y1 = (int)MAX(a.y,b.y);
if (x0<0)x0=0; if(y0<0)y0=0; if(x1>=W)x1=W-1; if(y1>=H)y1=H-1;
for (int y=y0;y<=y1;y++)
    for (int x=x0;x<=x1;x++){
        unsigned char *px = data + y*bpr + x*spp;
        px[0]=0; px[1]=0; px[2]=0;
        if(spp>=4) px[3]=0;
    }
}
// dessine un objet (bouton ou champ) à son rectangle
static void draw_part(Object *o) {
    if (!o->visible) return;

    NSRect r = NSMakeRect(o->x, o->y, o->w, o->h);

    if (o->type == OBJ_BUTTON) {
        const char *st = o->style ? o->style : "rectangle";
        BOOL isCheck = (strcmp(st, "checkBox") == 0 || strcmp(st, "checkbox") == 0);
        BOOL isRadio = (strcmp(st, "radioButton") == 0 || strcmp(st, "radiobutton") == 0);
        BOOL isTransparent = (strcmp(st, "transparent") == 0);
        const char *nm = o->name ? o->name : "";
        NSString *s = [NSString stringWithUTF8String:nm];

        const HCIcon *ic = (o->icon ? hcicon_find(o->icon) : NULL);

        if (ic) {
            // ---- bouton a icone : l'icone remplace l'apparence ----
            BOOL on = o->hilite;
            if (on) {                       // hilite : fond noir, icone en blanc
                [[NSColor blackColor] setFill];
                NSRectFill(r);
            }
            // icone centree horizontalement, en haut
            CGFloat iy = o->y + 2;
            NSRect ir = NSMakeRect(o->x + (o->w - 32)/2.0, iy, 32, 32);
            if (on) [[NSColor whiteColor] setFill];
            else    [[NSColor blackColor] setFill];
            hcicon_draw(ic, ir, 1.0);

            // en mode edition : contour pointille pour pouvoir la saisir
            if (gTool == TOOL_BUTTON || gTool == TOOL_FIELD) {
                [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
                NSBezierPath *outline = [NSBezierPath bezierPathWithRect:r];
                [outline setLineWidth:1];
                CGFloat dash[] = {3, 2};
                [outline setLineDash:dash count:2 phase:0];
                [outline stroke];
            }

            // le nom, sous l'icone
            if (o->showname && o->h > 36) {
                CGFloat fs = o->textsize > 0 ? o->textsize : 11;
                NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                [ps setAlignment:NSTextAlignmentCenter];
                NSDictionary *attrs = @{
                    NSFontAttributeName: [NSFont systemFontOfSize:fs],
                    NSForegroundColorAttributeName: (on ? [NSColor whiteColor] : [NSColor blackColor]),
                    NSParagraphStyleAttributeName: ps
                };
                NSRect tr = NSMakeRect(o->x, iy + 34, o->w, o->h - 36);
                [s drawInRect:tr withAttributes:attrs];
            }
        }
        else if (isCheck || isRadio) {
            // case ou rond à gauche, nom à droite
            CGFloat box = 14;
            CGFloat cy = o->y + o->h/2.0 - box/2.0;
            NSRect mark = NSMakeRect(o->x + 2, cy, box, box);

            [[NSColor whiteColor] setFill];
            [[NSColor blackColor] setStroke];

            if (isRadio) {
                NSBezierPath *circle = [NSBezierPath bezierPathWithOvalInRect:mark];
                [circle fill];
                [circle stroke];
                if (o->hilite) {
                    NSRect dot = NSInsetRect(mark, 4, 4);
                    [[NSColor blackColor] setFill];
                    [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
                }
            } else {
                // case à cocher
                [[NSColor whiteColor] setFill];
                NSRectFill(mark);
                [[NSColor blackColor] setStroke];
                NSBezierPath *box_path = [NSBezierPath bezierPathWithRect:mark];
                [box_path setLineWidth:1];
                [box_path stroke];
                if (o->hilite) {
                    NSBezierPath *x = [NSBezierPath bezierPath];
                    [x moveToPoint:NSMakePoint(mark.origin.x+2, mark.origin.y+2)];
                    [x lineToPoint:NSMakePoint(mark.origin.x+box-2, mark.origin.y+box-2)];
                    [x moveToPoint:NSMakePoint(mark.origin.x+box-2, mark.origin.y+2)];
                    [x lineToPoint:NSMakePoint(mark.origin.x+2, mark.origin.y+box-2)];
                    [x setLineWidth:1.5];
                    [x stroke];
                }
            }

            // le nom, à droite de la case
            if (o->showname) {
                NSDictionary *attrs = @{ NSFontAttributeName: [NSFont systemFontOfSize:13] };
                [s drawAtPoint:NSMakePoint(o->x + box + 8, o->y + o->h/2 - 8) withAttributes:attrs];
            }
        }
        else if (isTransparent) {
            BOOL on = o->hilite;
            if (on) {
                [[NSColor colorWithWhite:0.0 alpha:0.15] setFill];
                NSRectFill(r);
            }
            // en mode édition : montrer le contour pour pouvoir le saisir
            if (gTool == TOOL_BUTTON || gTool == TOOL_FIELD) {
                [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
                NSBezierPath *outline = [NSBezierPath bezierPathWithRect:r];
                [outline setLineWidth:1];
                CGFloat dash[] = {3, 2};
                [outline setLineDash:dash count:2 phase:0];
                [outline stroke];
            }
            CGFloat fs = o->textsize > 0 ? o->textsize : 16;
            NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
            [ps setAlignment:NSTextAlignmentCenter];
            NSDictionary *attrs = @{
                NSFontAttributeName: [NSFont boldSystemFontOfSize:fs],
                NSForegroundColorAttributeName: [NSColor blackColor],
                NSParagraphStyleAttributeName: ps
            };
            NSRect tr = NSInsetRect(r, 2, 0);
            tr.origin.y += (r.size.height - fs * 1.2) / 2;
            if (o->showname)
                [s drawInRect:tr withAttributes:attrs];
        }
        else {
            // bouton rectangle classique
            BOOL on = o->hilite;
            [(on ? [NSColor blackColor] : [NSColor colorWithWhite:0.9 alpha:1.0]) setFill];
            NSRectFill(r);
            [[NSColor blackColor] setStroke];
            NSFrameRect(r);

            CGFloat fs = o->textsize > 0 ? o->textsize : 13;
            NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
            [ps setAlignment:NSTextAlignmentCenter];
            NSDictionary *attrs = @{
                NSFontAttributeName: [NSFont boldSystemFontOfSize:fs],
                NSForegroundColorAttributeName: (on ? [NSColor whiteColor] : [NSColor blackColor]),
                NSParagraphStyleAttributeName: ps
            };
            NSRect tr = NSInsetRect(r, 4, 0);
            tr.origin.y += (r.size.height - fs * 1.2) / 2;
            if (o->showname)
                [s drawInRect:tr withAttributes:attrs];
        }
    }
    else if (o->type == OBJ_FIELD) {
        [[NSColor colorWithWhite:0.97 alpha:1.0] setFill];
        NSRectFill(r);
        [[NSColor colorWithWhite:0.4 alpha:1.0] setStroke];
        NSFrameRect(r);
        const char *tx = o->contents ? o->contents : "";
        NSString *s = [NSString stringWithUTF8String:tx];
        [s drawInRect:NSInsetRect(r, 4, 4) withAttributes:nil];
    }
}
static int handle_at(Object *o, NSPoint p) {
    if (!o) return 0;
    CGFloat s = 8;  // tolérance de clic
    NSPoint corners[4] = {
        {o->x, o->y},
        {o->x + o->w, o->y},
        {o->x, o->y + o->h},
        {o->x + o->w, o->y + o->h}
    };
    for (int i = 0; i < 4; i++)
        if (fabs(p.x - corners[i].x) <= s && fabs(p.y - corners[i].y) <= s)
            return i + 1;
    return 0;
}
// trouve la part (bouton/champ) contenant le point, carte puis fond
// trouve la part contenant le point.
// En navigation (browse), cherche partout (carte + fond).
// En édition, ne cherche que dans la couche active.
static Object *part_at(Object *card, NSPoint p) {
    if (!card) return NULL;

    // en mode édition, restreindre à la couche active
    if (gTool != TOOL_BROWSE) {
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) return NULL;
        for (int i = layer->nparts - 1; i >= 0; i--) {
            Object *o = layer->parts[i];
            if (o->visible &&
                p.x >= o->x && p.x <= o->x + o->w &&
                p.y >= o->y && p.y <= o->y + o->h)
                return o;
        }
        return NULL;
    }

    // en navigation : carte d'abord, puis fond
    for (int i = card->nparts - 1; i >= 0; i--) {
        Object *o = card->parts[i];
        if (o->visible &&
            p.x >= o->x && p.x <= o->x + o->w &&
            p.y >= o->y && p.y <= o->y + o->h)
            return o;
    }
    if (card->bg)
        for (int i = card->bg->nparts - 1; i >= 0; i--) {
            Object *o = card->bg->parts[i];
            if (o->visible &&
                p.x >= o->x && p.x <= o->x + o->w &&
                p.y >= o->y && p.y <= o->y + o->h)
                return o;
        }
    return NULL;
}

// reçoit toute sortie du noyau
static void cocoa_line(HcLineKind kind, int depth, const char *text) {
    (void)depth;
    if (kind == HC_MSG && gMsgBox) {
        [gMsgBox setStringValue:[NSString stringWithUTF8String:text]];
    } else {
        NSLog(@"%s", text);
    }
}

static void cocoa_field_changed(Object *field) {
    (void)field;
    [gView setNeedsDisplay:YES];
}
static NSFont *text_font(void) {
    if (!gTextFont) {
        gTextFont = [NSFont fontWithName:@"Helvetica" size:gTextSize];
        if (!gTextFont) gTextFont = [NSFont systemFontOfSize:gTextSize];
    }
    return gTextFont;
}

static NSDictionary *text_attrs(void) {
    NSColor *c = gTextColor;
    if (!c) c = (gInk == INK_WHITE) ? [NSColor whiteColor] : [NSColor blackColor];
    NSMutableDictionary *a = [NSMutableDictionary dictionary];
    a[NSFontAttributeName] = text_font();
    a[NSForegroundColorAttributeName] = c;
    if (gTextUnderline) a[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);
    return a;
}
// grave le texte dans le bitmap a la position donnee
static void stamp_text(NSBitmapImageRep *rep, NSString *s, NSPoint pos) {
    if (!rep || [s length] == 0) return;
    NSGraphicsContext *base = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!base) return;
    CGContextRef cg = [base CGContext];
    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithCGContext:cg flipped:YES];

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:NO];

    CGFloat H = [rep pixelsHigh];
    CGContextTranslateCTM(cg, 0, H);
    CGContextScaleCTM(cg, 1, -1);

    [s drawAtPoint:pos withAttributes:text_attrs()];

    [NSGraphicsContext restoreGraphicsState];
}


// ==================== palette d'épaisseur de trait (vue custom) ====================




// ==================== palette d'outils custom (grille + sélection encadrée) ====================
typedef struct { const char *glyph; int kind; int value; } ToolCell;

static const ToolCell TOOLCELLS[] = {
    {"👆", 0, TOOL_BROWSE},
    {"B",  0, TOOL_BUTTON},
    {"F",  0, TOOL_FIELD},
    {"✏", 0, TOOL_PENCIL},
    {"⌫", 0, TOOL_ERASER},
    {"╱", 0, TOOL_LINE},
    {"▭", 0, TOOL_RECT},
    {"○", 0, TOOL_OVAL},
    {"💧", 0, TOOL_FILL},
    {"✎", 0, TOOL_FREEFORM},
    {"⬚", 0, TOOL_LASSO},
    {"◰", 0, TOOL_SELRECT},     // ou ⬛⃞ ou ◰ — un rectangle de sélection
    {"⬛", 1, INK_BLACK},
    {"⬜", 1, INK_WHITE},
    {"◌", 1, INK_ERASE},
    {"▣", 2, 0},
    {"P", 0, TOOL_BRUSH},
    {"A", 0, TOOL_TEXT},
};


#define NUM_TOOLCELLS (int)(sizeof(TOOLCELLS)/sizeof(TOOLCELLS[0]))

@implementation HCView
- (BOOL)acceptsFirstResponder { return YES; }
- (void)copy:(id)sender {
    Object *card = hc_current_card();
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (gTool == TOOL_SELRECT && gSelRectActive)
        copy_rect(rep, gSelStart, gSelEnd);
    else if (gTool == TOOL_LASSO && gLassoActive)
        copy_freeform(rep, gLassoPts, gLassoCount);
}

- (void)cut:(id)sender {
    Object *card = hc_current_card();
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (gTool == TOOL_SELRECT && gSelRectActive) {
        copy_rect(rep, gSelStart, gSelEnd);
        erase_rect(rep, gSelStart, gSelEnd);
        gSelRectActive = NO;
    } else if (gTool == TOOL_LASSO && gLassoActive) {
        copy_freeform(rep, gLassoPts, gLassoCount);
        erase_freeform(rep, gLassoPts, gLassoCount);
        gLassoActive = NO;
        gLassoCount = 0;
    }
    [self setNeedsDisplay:YES];
}

- (void)paste:(id)sender {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSArray *imgs = [pb readObjectsForClasses:@[[NSImage class]] options:nil];
    if (imgs.count > 0) {
        NSData *tiff = [imgs[0] TIFFRepresentation];
        NSBitmapImageRep *ext = [NSBitmapImageRep imageRepWithData:tiff];
        if (ext) {
            gClipboard = ext;
            gClipW = (int)[ext pixelsWide];
            gClipH = (int)[ext pixelsHigh];
            gClipPtsCount = 0;
        }
    }
    if (gClipboard) {
        gFloating = YES;
        NSRect b = [self bounds];
        gFloatPos = NSMakePoint((b.size.width - gClipW)/2, (b.size.height - gClipH)/2);
        [self setNeedsDisplay:YES];
    }
}
- (BOOL)validateMenuItem:(NSMenuItem *)item {
    SEL a = [item action];
    if (a == @selector(copy:) || a == @selector(cut:))
        return (gTool == TOOL_SELRECT && gSelRectActive) ||
               (gTool == TOOL_LASSO && gLassoActive);
    if (a == @selector(paste:))
        return gClipboard != nil ||
               [[NSPasteboard generalPasteboard] canReadObjectForClasses:@[[NSImage class]] options:nil];
    return YES;
}
- (void)setFrameSize:(NSSize)newSize {
    [self flushPaintToKernel];   // encoder les dessins avant que le cache ne soit invalide
    [super setFrameSize:newSize];
    [self clearPaintCache];
    [self setNeedsDisplay:YES];
}
- (void)changeColor:(id)sender {
    gTextColor = [sender color];
    [self setNeedsDisplay:YES];
}
- (void)underline:(id)sender {
    gTextUnderline = !gTextUnderline;
    [self setNeedsDisplay:YES];
}
- (void)commitText {
    if (!gTextActive) return;
    if ([gTextBuf length] > 0) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        if (layer) {
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                        (int)[self bounds].size.height);
            stamp_text(rep, gTextBuf, gTextPos);
        }
    }
    gTextActive = NO;
    gTextBuf = nil;
    [self setNeedsDisplay:YES];
}
- (void)changeFont:(id)sender {
    gTextFont = [sender convertFont:text_font()];
    gTextSize = (int)[gTextFont pointSize];
    [[NSFontManager sharedFontManager] setSelectedFont:gTextFont isMultiple:NO];
    [self setNeedsDisplay:YES];
}
// gras / italique / souligne passent par la
- (void)changeAttributes:(id)sender {
    [self setNeedsDisplay:YES];
}

- (NSFontPanelModeMask)validModesForFontPanel:(NSFontPanel *)fontPanel {
    return NSFontPanelModesMaskStandardModes;
}
- (void)ditherSelection:(id)sender {
    // collage flottant : tramer l'image qui flotte
    if (gFloating && gClipboard) {
        dither_region(gClipboard, 0, 0, gClipW-1, gClipH-1, NULL, 0);
        [self setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    // selection rectangulaire
    if (gSelRectActive) {
        dither_region(rep, (int)MIN(gSelStart.x,gSelEnd.x), (int)MIN(gSelStart.y,gSelEnd.y),
                           (int)MAX(gSelStart.x,gSelEnd.x), (int)MAX(gSelStart.y,gSelEnd.y),
                           NULL, 0);
        [self setNeedsDisplay:YES];
        return;
    }
    // selection lasso
    if (gLassoActive && gLassoCount >= 3) {
        double minx=gLassoPts[0].x, maxx=minx, miny=gLassoPts[0].y, maxy=miny;
        for (int i=1;i<gLassoCount;i++){
            if(gLassoPts[i].x<minx)minx=gLassoPts[i].x;
            if(gLassoPts[i].x>maxx)maxx=gLassoPts[i].x;
            if(gLassoPts[i].y<miny)miny=gLassoPts[i].y;
            if(gLassoPts[i].y>maxy)maxy=gLassoPts[i].y;
        }
        dither_region(rep, (int)floor(minx), (int)floor(miny),
                           (int)ceil(maxx),  (int)ceil(maxy),
                           gLassoPts, gLassoCount);
        [self setNeedsDisplay:YES];
        return;
    }
    // rien de selectionne : toute la couche
    dither_region(rep, 0, 0, (int)[rep pixelsWide]-1, (int)[rep pixelsHigh]-1, NULL, 0);
    [self setNeedsDisplay:YES];
}
- (void)keyDown:(NSEvent *)event {
    unichar key = [[event charactersIgnoringModifiers] characterAtIndex:0];
    NSUInteger mods = [event modifierFlags];
    BOOL cmd = (mods & NSEventModifierFlagCommand) != 0;

    // saisie de texte en cours : tout va au tampon
    if (gTool == TOOL_TEXT && gTextActive && !cmd) {
        if (key == 27) {                          // Echap : annuler
            gTextActive = NO;
            [self setNeedsDisplay:YES];
            return;
        }
        if (key == NSDeleteCharacter || key == NSBackspaceCharacter) {
            NSUInteger n = [gTextBuf length];
            if (n > 0) [gTextBuf deleteCharactersInRange:NSMakeRange(n-1, 1)];
            [self setNeedsDisplay:YES];
            return;
        }
        NSString *chars = [event characters];
        if ([chars length] > 0) {
            if ([chars isEqualToString:@"\r"]) chars = @"\n";
            [gTextBuf appendString:chars];
            [self setNeedsDisplay:YES];
        }
        return;
    }

    // Delete : lasso actif -> effacer la zone
    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gTool == TOOL_LASSO && gLassoActive) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                    (int)[self bounds].size.height);
        erase_freeform(rep, gLassoPts, gLassoCount);
        gLassoActive = NO;
        gLassoCount = 0;
        [self setNeedsDisplay:YES];
        return;
    }

    // Delete : selection rectangulaire -> effacer la zone
    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gTool == TOOL_SELRECT && gSelRectActive) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                    (int)[self bounds].size.height);
        erase_rect(rep, gSelStart, gSelEnd);
        gSelRectActive = NO;
        [self setNeedsDisplay:YES];
        return;
    }

    // Delete : objet selectionne -> le supprimer
    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gSelected && gTool != TOOL_BROWSE) {
        hc_delete_part(gSelected);
        gSelected = NULL;
        [self setNeedsDisplay:YES];
        return;
    }

    [super keyDown:event];
}
- (void)inkChosen:(id)sender {
    gInk = (HCInk)[sender tag];
    NSLog(@"encre : %d", (int)gInk);
}
- (BOOL)isFlipped { return YES; }
- (void)applyStackSize {
    // 1. D'ABORD encoder les dessins actuels (avant tout redimensionnement)
    [self flushPaintToKernel];

    // 2. Trouver la pile et sa taille
    Object *card = hc_current_card();
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;
    int w = stack->w > 0 ? stack->w : 512;
    int h = stack->h > 0 ? stack->h : 342;

    // 3. Vider le cache (les bitmaps seront recréés à la nouvelle taille, en rechargeant le PNG)
    [self clearPaintCache];

    // 4. Redimensionner la fenêtre
    NSWindow *win = [self window];
    if (!win) return;
    NSRect frame = [win frame];
    NSRect content = NSMakeRect(0, 0, w, h);
    NSRect newFrame = [win frameRectForContentRect:content];
    newFrame.origin = frame.origin;
    newFrame.origin.y = frame.origin.y + frame.size.height - newFrame.size.height;
    [win setFrame:newFrame display:YES animate:NO];

    [self setNeedsDisplay:YES];
}
- (void)installWidthPalette {
    int cols = 4, rows = 3;   // 11 valeurs sur 3 rangées
    CGFloat cell = 40, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;
    gWidthPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(200, 150, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gWidthPanel setTitle:@"Épaisseur"];
    [gWidthPanel setFloatingPanel:YES];
    [gWidthPanel setReleasedWhenClosed:NO];
    WidthPalette *grid = [[WidthPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gWidthPanel setContentView:grid];
    [gWidthPanel makeKeyAndOrderFront:nil];
}
- (void)installBrushPalette {
    int cols = 4, rows = (NUM_BRUSHES + cols - 1) / cols;
    CGFloat cell = 34, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;
    gBrushPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(250, 300, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gBrushPanel setTitle:@"Pinceaux"];
    [gBrushPanel setFloatingPanel:YES];
    [gBrushPanel setReleasedWhenClosed:NO];
    BrushPalette *grid = [[BrushPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gBrushPanel setContentView:grid];
    [gBrushPanel makeKeyAndOrderFront:nil];
}

- (void)showBrushPalette {
    if (!gBrushPanel) [self installBrushPalette];
    else [gBrushPanel makeKeyAndOrderFront:nil];
}
- (void)widthChosen:(id)sender {
    gLineWidth = (int)[sender tag];
}
- (void)showPatternPalette {
   if (!gPatternPanel) [self installPatternPalette];
   else [gPatternPanel makeKeyAndOrderFront:nil];
    [gPatternPanel setReleasedWhenClosed:NO];
}
- (void)showWidthPalette {
    NSLog(@"showWidthPalette: gWidthPanel=%@", gWidthPanel);
   if (!gWidthPanel) [self installWidthPalette];
   else [gWidthPanel makeKeyAndOrderFront:nil];
    [gWidthPanel setReleasedWhenClosed:NO];
}
- (void)installPatternPalette {
    int cols = 4, rows = (NUM_PATTERNS + cols - 1) / cols;
    CGFloat cell = 32, gap = 4, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;
    gPatternPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(560, 200, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [gPatternPanel setTitle:@"Motifs"];
    [gPatternPanel setFloatingPanel:YES];
    PatternPalette *grid = [[PatternPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gPatternPanel setContentView:grid];
    [gPatternPanel makeKeyAndOrderFront:nil];
}
// remplit l'intérieur d'un polygone (forme libre) avec le motif + encre courants
static void fill_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n) {
    if (!rep || n < 3) return;
    int W = (int)[rep pixelsWide];
    int H = (int)[rep pixelsHigh];
    unsigned char *data = [rep bitmapData];
    if (!data) return;
    NSInteger bpr = [rep bytesPerRow];
    NSInteger spp = [rep samplesPerPixel];

    // boîte englobante du polygone
    double minx = pts[0].x, maxx = pts[0].x, miny = pts[0].y, maxy = pts[0].y;
    for (int i = 1; i < n; i++) {
        if (pts[i].x < minx) minx = pts[i].x;
        if (pts[i].x > maxx) maxx = pts[i].x;
        if (pts[i].y < miny) miny = pts[i].y;
        if (pts[i].y > maxy) maxy = pts[i].y;
    }
    int y0 = (int)floor(miny), y1 = (int)ceil(maxy);
    int x0 = (int)floor(minx), x1 = (int)ceil(maxx);
    if (y0 < 0) y0 = 0; if (x0 < 0) x0 = 0;
    if (y1 >= H) y1 = H-1; if (x1 >= W) x1 = W-1;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            // test point-dans-polygone (ray casting horizontal)
            int inside = 0;
            for (int i = 0, j = n-1; i < n; j = i++) {
                double yi = pts[i].y, yj = pts[j].y;
                double xi = pts[i].x, xj = pts[j].x;
                if (((yi > y) != (yj > y)) &&
                    (x < (xj - xi) * (y - yi) / (yj - yi) + xi))
                    inside = !inside;
            }
            if (!inside) continue;

            unsigned char *px = data + y*bpr + x*spp;
            if (pattern_bit(gPattern, x, y)) {
                px[0]=0; px[1]=0; px[2]=0;
                if (spp>=4) px[3]=255;
            } else {
                                if (gInk == INK_ERASE) {
                                    px[0]=0; px[1]=0; px[2]=0;
                                    if (spp>=4) px[3]=0;          // efface
                                } else if (gTransparentBg) {
                                    continue;                      // laisser intact
                                } else {
                                    px[0]=255; px[1]=255; px[2]=255;
                                    if (spp>=4) px[3]=255;        // fond blanc opaque
                                }
                            }
        }
    }
}
- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor whiteColor] setFill];
    NSRectFill(dirtyRect);

    Object *card = hc_current_card();
    if (!card) return;

    NSRect b = [self bounds];

    // ===== empilement façon HyperCard, de bas en haut =====

    // 1. peinture du fond
    if (card->bg) {
        NSBitmapImageRep *bgpaint = paint_bitmap(card->bg, (int)b.size.width, (int)b.size.height);
        [bgpaint drawInRect:NSMakeRect(0, 0, [bgpaint pixelsWide], [bgpaint pixelsHigh])
                   fromRect:NSZeroRect
                  operation:NSCompositingOperationSourceOver fraction:1.0
             respectFlipped:YES hints:nil];
    }

    // 2. objets du fond
    if (card->bg)
        for (int i = 0; i < card->bg->nparts; i++)
            draw_part(card->bg->parts[i]);

    // 3. peinture de la carte (PAR-DESSUS les objets du fond)
    NSBitmapImageRep *cardpaint = paint_bitmap(card, (int)b.size.width, (int)b.size.height);
    [cardpaint drawInRect:NSMakeRect(0, 0, [cardpaint pixelsWide], [cardpaint pixelsHigh])
                 fromRect:NSZeroRect
                operation:NSCompositingOperationSourceOver fraction:1.0
           respectFlipped:YES hints:nil];

    // 4. objets de la carte (au-dessus de tout)
    for (int i = 0; i < card->nparts; i++)
        draw_part(card->parts[i]);

    // ===== surcouches d'édition (toujours au-dessus) =====

    // sélection
    if (gSelected) {
        NSRect r = NSMakeRect(gSelected->x, gSelected->y, gSelected->w, gSelected->h);
        [[NSColor redColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:NSInsetRect(r, -2, -2)];
        [path setLineWidth:2];
        [path stroke];
        [[NSColor redColor] setFill];
        CGFloat s = 6;
        NSPoint corners[4] = {
            {r.origin.x, r.origin.y},
            {r.origin.x + r.size.width, r.origin.y},
            {r.origin.x, r.origin.y + r.size.height},
            {r.origin.x + r.size.width, r.origin.y + r.size.height}
        };
        for (int i = 0; i < 4; i++)
            NSRectFill(NSMakeRect(corners[i].x - s/2, corners[i].y - s/2, s, s));
    }

    // aperçu de création d'objet (drag rectangle)
    if (gDragging) {
        [[NSColor blueColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:gDragRect];
        [path setLineWidth:1];
        CGFloat dash[] = {4, 3};
        [path setLineDash:dash count:2 phase:0];
        [path stroke];
    }

    // cadre marron : mode édition du fond
    if (gEditBackground) {
        [[NSColor colorWithRed:0.6 green:0.4 blue:0.2 alpha:1.0] setStroke];
        NSBezierPath *frame = [NSBezierPath bezierPathWithRect:NSInsetRect(b, 4, 4)];
        [frame setLineWidth:4];
        CGFloat dash[] = {10, 5};
        [frame setLineDash:dash count:2 phase:0];
        [frame stroke];
    }

    // aperçu élastique des formes (ligne / rect / ovale)
    if (gShapeDrawing) {
        [[NSColor blueColor] setStroke];
        NSBezierPath *preview = [NSBezierPath bezierPath];
        NSRect box = NSMakeRect(MIN(gShapeStart.x,gShapeEnd.x), MIN(gShapeStart.y,gShapeEnd.y),
                                fabs(gShapeEnd.x-gShapeStart.x), fabs(gShapeEnd.y-gShapeStart.y));
        if (gTool == TOOL_LINE) {
            [preview moveToPoint:gShapeStart];
            [preview lineToPoint:gShapeEnd];
        } else if (gTool == TOOL_RECT) {
            preview = [NSBezierPath bezierPathWithRect:box];
        } else if (gTool == TOOL_OVAL) {
            preview = [NSBezierPath bezierPathWithOvalInRect:box];
        }
        [preview setLineWidth:1];
        [preview stroke];
    }
    if (gFreeDrawing && gFreeCount > 1) {
            [[NSColor blueColor] setStroke];
            NSBezierPath *pv = [NSBezierPath bezierPath];
            [pv moveToPoint:gFreePts[0]];
            for (int i = 1; i < gFreeCount; i++) [pv lineToPoint:gFreePts[i]];
            [pv setLineWidth:1];
            [pv stroke];
        }
    if ((gLassoDrawing || gLassoActive) && gLassoCount > 1) {
            [[NSColor blackColor] setStroke];
            NSBezierPath *pv = [NSBezierPath bezierPath];
            [pv moveToPoint:gLassoPts[0]];
            for (int i = 1; i < gLassoCount; i++) [pv lineToPoint:gLassoPts[i]];
            if (gLassoActive) [pv closePath];
            [pv setLineWidth:1];
            CGFloat dash[] = {4, 3};
            [pv setLineDash:dash count:2 phase:0];
            [pv stroke];
        }
    if (gSelRectDrawing || gSelRectActive) {
            NSRect sel = NSMakeRect(MIN(gSelStart.x,gSelEnd.x), MIN(gSelStart.y,gSelEnd.y),
                                    fabs(gSelEnd.x-gSelStart.x), fabs(gSelEnd.y-gSelStart.y));
            [[NSColor blackColor] setStroke];
            NSBezierPath *pv = [NSBezierPath bezierPathWithRect:sel];
            [pv setLineWidth:1];
            CGFloat dash[] = {4, 3};
            [pv setLineDash:dash count:2 phase:0];
            [pv stroke];
        }
    if (gFloating && gClipboard) {
            NSRect fr = NSMakeRect(gFloatPos.x, gFloatPos.y, gClipW, gClipH);
            [gClipboard drawInRect:fr fromRect:NSZeroRect
                         operation:NSCompositingOperationSourceOver fraction:1.0
                    respectFlipped:YES hints:nil];

            [[NSColor blackColor] setStroke];
            NSBezierPath *fp = [NSBezierPath bezierPath];
            if (gClipPtsCount >= 3) {
                [fp moveToPoint:NSMakePoint(gFloatPos.x + gClipPts[0].x,
                                            gFloatPos.y + gClipPts[0].y)];
                for (int i = 1; i < gClipPtsCount; i++)
                    [fp lineToPoint:NSMakePoint(gFloatPos.x + gClipPts[i].x,
                                                gFloatPos.y + gClipPts[i].y)];
                [fp closePath];
            } else {
                fp = [NSBezierPath bezierPathWithRect:fr];
            }
            [fp setLineWidth:1];
            CGFloat dash[] = {4, 3};
            [fp setLineDash:dash count:2 phase:0];
            [fp stroke];
        }
    if (gTextActive && gTextBuf) {
            NSDictionary *at = text_attrs();
            [gTextBuf drawAtPoint:gTextPos withAttributes:at];

            NSArray *lines = [gTextBuf componentsSeparatedByString:@"\n"];
            NSString *last = [lines lastObject];
            NSSize lastSz = [last sizeWithAttributes:at];
            // hauteur d'une ligne telle que la mesure le moteur de rendu
            NSSize oneLine = [@"Ag" sizeWithAttributes:at];
            CGFloat cy = gTextPos.y + ([lines count] - 1) * oneLine.height;

            [[NSColor blackColor] setStroke];
            NSBezierPath *caret = [NSBezierPath bezierPath];
            [caret moveToPoint:NSMakePoint(gTextPos.x + lastSz.width, cy)];
            [caret lineToPoint:NSMakePoint(gTextPos.x + lastSz.width, cy + oneLine.height)];
            [caret setLineWidth:1];
            [caret stroke];
        }
}
- (void)toggleBackground:(id)sender {
    gEditBackground = !gEditBackground;
    gSelected = NULL;
    [self endFieldEdit];
    [self setNeedsDisplay:YES];
    NSLog(@"couche : %@", gEditBackground ? @"FOND" : @"CARTE");
}
- (void)testScribble {
    Object *card = hc_current_card();
    if (!card) return;
    NSBitmapImageRep *rep = paint_bitmap(card, 500, 400);

    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:NO];
    [[NSColor blackColor] setStroke];
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path moveToPoint:NSMakePoint(50, 50)];
    [path curveToPoint:NSMakePoint(250, 150)
         controlPoint1:NSMakePoint(100, 200)
         controlPoint2:NSMakePoint(200, 20)];
    [path setLineWidth:3];
    [path stroke];

    [NSGraphicsContext restoreGraphicsState];
    [self setNeedsDisplay:YES];
}
- (void)clearPaintCache {
    [gPaintCache removeAllObjects];
}
- (void)flushPaintToKernel {
    if (!gPaintCache) return;
    for (NSValue *key in gPaintCache) {
        Object *o = [key pointerValue];
        NSBitmapImageRep *rep = [gPaintCache objectForKey:key];
        NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        NSString *b64 = [png base64EncodedStringWithOptions:0];
        hc_set_paint(o, [b64 UTF8String]);
    }
}
- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    Object *hit = part_at(hc_current_card(), p);
    if (gFloating) {
            NSRect fr = NSMakeRect(gFloatPos.x, gFloatPos.y, gClipW, gClipH);
            if (NSPointInRect(p, fr)) {
                // clic DANS le flottant : commencer à le déplacer
                gFloatDragging = YES;
                gFloatGrab = NSMakePoint(p.x - gFloatPos.x, p.y - gFloatPos.y);
            } else {
                // clic DEHORS : scotcher le flottant dans le bitmap
                Object *card = hc_current_card();
                Object *layer = gEditBackground ? card->bg : card;
                if (!layer) layer = card;
                NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
                stamp_clipboard(rep, gFloatPos);
                gFloating = NO;
                [self setNeedsDisplay:YES];
            }
            return;
        }
    if (gTool == TOOL_PENCIL) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;   // ← couche active
        if (!layer) layer = card;
        NSLog(@"crayon: gEditBackground=%d card=%p bg=%p layer=%p type=%d",
              gEditBackground, (void*)card, (void*)(card?card->bg:NULL),
              (void*)layer, layer?layer->type:-1);
            if (layer) {
                
                NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
                gPenLast = p;
                gPenDrawing = YES;
                paint_stroke(rep, p, p, [NSColor blackColor], gLineWidth);
                [self setNeedsDisplay:YES];
            }
            return;
        }
    if (gTool == TOOL_SELRECT) {
        gSelStart = p; gSelEnd = p;
        gSelRectDrawing = YES;
        gSelRectActive = NO;
        [self setNeedsDisplay:YES];
        return;
    }
    if (gTool == TOOL_BRUSH) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            if (layer) {
                NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
                gPenLast = p;
                gPenDrawing = YES;
                brush_stroke(rep, p, p);
                [self setNeedsDisplay:YES];
            }
            return;
        }
    if (gTool == TOOL_ERASER) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
        NSLog(@"gomme: gEditBackground=%d layer type=%d", gEditBackground, layer?layer->type:-1);
            if (layer) {
                NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
                gPenLast = p;
                gPenDrawing = YES;
                erase_stroke(rep, p, p, 16);   // gomme large de 16
                [self setNeedsDisplay:YES];
            }
            return;
        }
    if (gTool == TOOL_LINE || gTool == TOOL_RECT || gTool == TOOL_OVAL) {
            gShapeStart = p;
            gShapeEnd = p;
            gShapeDrawing = YES;
            [self setNeedsDisplay:YES];
            return;
        }
    // double-clic en mode édition : éditer le script
    if (gTool != TOOL_BROWSE && hit && [event clickCount] == 2) {
        [self editScriptOf:hit];
        return;
    }
    if (gTool == TOOL_FILL) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            flood_fill(rep, (int)p.x, (int)p.y);
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_FREEFORM) {
            gFreeCount = 0;
            gFreePts[gFreeCount++] = p;
            gFreeDrawing = YES;
            [self setNeedsDisplay:YES];
            return;
        }
    // tool lasso
        if (gTool == TOOL_LASSO) {
            gLassoCount = 0;
            gLassoPts[gLassoCount++] = p;
            gLassoDrawing = YES;
            gLassoActive = NO;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_BROWSE) {
            if (hit && hit->type == OBJ_FIELD) {
                [self beginFieldEdit:hit];
                return;
            }
        if (hit) {
                    gPressed = hit;
                    // flash uniquement pour les boutons rectangle avec autohilite
                    if (hit->type == OBJ_BUTTON && hit->autohilite &&
                        (!hit->style ||
                         (strcmp(hit->style, "checkBox") != 0 && strcmp(hit->style, "checkbox") != 0 &&
                          strcmp(hit->style, "radioButton") != 0 && strcmp(hit->style, "radiobutton") != 0))) {
                        hit->hilite = 1;
                    }
                    hc_send(hit, "mouseDown");
                    [self setNeedsDisplay:YES];
                } else {
                    [self endFieldEdit];
                }
            return;
        }
    if (gTool == TOOL_TEXT) {
            [self commitText];          // graver la saisie en cours
            gTextPos = p;
            gTextBuf = [NSMutableString string];
            gTextActive = YES;
            [[self window] makeFirstResponder:self];
            [self setNeedsDisplay:YES];
            return;
        }
 
    // mode bouton/champ
        // d'abord : saisit-on une poignée de l'objet déjà sélectionné ?
        if (gSelected) {
            int h = handle_at(gSelected, p);
            if (h) {
                gResizeHandle = h;
                gMoveStart = p;
                gObjStartX = gSelected->x;
                gObjStartY = gSelected->y;
                gObjStartW = gSelected->w;
                gObjStartH = gSelected->h;
                gMoving = NO;
                gDragging = NO;
                return;
            }
        }

        if (hit) {
            gSelected = hit;
            gMoving = YES;
            [[self window] makeFirstResponder:self];   // ← focus clavier pour recevoir Delete
            gMoveStart = p;
            gObjStartX = hit->x;
            gObjStartY = hit->y;
            gDragging = NO;
        } else {
            gSelected = NULL;
            gDragStart = p;
            gDragRect = NSMakeRect(p.x, p.y, 0, 0);
            gDragging = YES;
        }
        [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    if (gFloating && gFloatDragging) {
            gFloatPos = NSMakePoint(p.x - gFloatGrab.x, p.y - gFloatGrab.y);
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_PENCIL && gPenDrawing) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;   // ← couche active
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
        paint_stroke(rep, gPenLast, p, [NSColor blackColor], gLineWidth);            gPenLast = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_BRUSH && gPenDrawing) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            brush_stroke(rep, gPenLast, p);
            gPenLast = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_ERASER && gPenDrawing) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            erase_stroke(rep, gPenLast, p, 16);
            gPenLast = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_FREEFORM && gFreeDrawing) {
            if (gFreeCount < 4096) gFreePts[gFreeCount++] = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_SELRECT && gSelRectDrawing) {
        gSelEnd = p;
        [self setNeedsDisplay:YES];
        return;
    }





    if (gShapeDrawing) {
            gShapeEnd = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gResizeHandle && gSelected) {
        int dx = (int)(p.x - gMoveStart.x);
        int dy = (int)(p.y - gMoveStart.y);
        int x = gObjStartX, y = gObjStartY, w = gObjStartW, h = gObjStartH;
        switch (gResizeHandle) {
            case 1: x += dx; y += dy; w -= dx; h -= dy; break;  // HG
            case 2: y += dy; w += dx; h -= dy; break;            // HD
            case 3: x += dx; w -= dx; h += dy; break;            // BG
            case 4: w += dx; h += dy; break;                     // BD
        }
        if (w < 8) w = 8;
        if (h < 8) h = 8;
        gSelected->x = x; gSelected->y = y;
        gSelected->w = w; gSelected->h = h;
        [self setNeedsDisplay:YES];
        return;
    }
    // mouseDragged
        if (gTool == TOOL_LASSO && gLassoDrawing) {
            if (gLassoCount < 4096) gLassoPts[gLassoCount++] = p;
            [self setNeedsDisplay:YES];
            return;
        }
    // déplacement d'un objet sélectionné
    if (gMoving && gSelected) {
        int dx = (int)(p.x - gMoveStart.x);
        int dy = (int)(p.y - gMoveStart.y);
        gSelected->x = gObjStartX + dx;
        gSelected->y = gObjStartY + dy;
        [self setNeedsDisplay:YES];
        return;
    }

    // création par tracé (inchangé)
    if (!gDragging) return;
    CGFloat x = MIN(gDragStart.x, p.x);
    CGFloat y = MIN(gDragStart.y, p.y);
    CGFloat w = fabs(p.x - gDragStart.x);
    CGFloat h = fabs(p.y - gDragStart.y);
    gDragRect = NSMakeRect(x, y, w, h);
    [self setNeedsDisplay:YES];
}
- (void)mouseUp:(NSEvent *)event {
    // --- mode flèche : envoyer mouseUp au script ---
    if (gFloating) {
            gFloatDragging = NO;
            return;
        }
    if (gResizeHandle) {
            gResizeHandle = 0;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_PENCIL || gTool == TOOL_ERASER) {
            gPenDrawing = NO;
            return;
        }
    if (gTool == TOOL_SELRECT) {
            gSelRectDrawing = NO;
            gSelRectActive = (fabs(gSelEnd.x-gSelStart.x) > 3 && fabs(gSelEnd.y-gSelStart.y) > 3);
            [self setNeedsDisplay:YES];
            return;
        }
    // tool lasso
        if (gTool == TOOL_LASSO) {
            gLassoDrawing = NO;
            gLassoActive = (gLassoCount >= 3);   // sélection valide si au moins un triangle
            [self setNeedsDisplay:YES];
            return;
        }
    if (gShapeDrawing) {
            gShapeDrawing = NO;
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
                    if (gShapeFilled && gTool != TOOL_LINE)
                        fill_shape(rep, gTool, gShapeStart, gShapeEnd);
                    paint_shape(rep, gTool, gShapeStart, gShapeEnd, [NSColor blackColor], gLineWidth);;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_FREEFORM) {
            gFreeDrawing = NO;
            if (gFreeCount >= 2) {
                Object *card = hc_current_card();
                Object *layer = gEditBackground ? card->bg : card;
                if (!layer) layer = card;
                NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
                if (gShapeFilled && gFreeCount >= 3)
                    fill_freeform(rep, gFreePts, gFreeCount);
                paint_freeform(rep, gFreePts, gFreeCount, gLineWidth);
            }
            gFreeCount = 0;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_BROWSE) {
            if (gPressed) {
                NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
                Object *hit = part_at(hc_current_card(), p);
                if (hit == gPressed)
                    hc_send(gPressed, "mouseUp");

                // comportement checkBox / radioButton : bascule persistante
                if (gPressed->type == OBJ_BUTTON && gPressed->style) {
                    const char *st = gPressed->style;
                    if (strcmp(st, "checkBox") == 0 || strcmp(st, "checkbox") == 0) {
                        gPressed->hilite = !gPressed->hilite;   // bascule et reste
                    }
                    else if (strcmp(st, "radioButton") == 0 || strcmp(st, "radiobutton") == 0) {
                        gPressed->hilite = 1;                    // allume
                        radio_exclusive(hc_current_card(), gPressed);  // éteint les autres
                    }
                    else if (gPressed->autohilite) {
                        gPressed->hilite = 0;   // bouton normal : éteindre le flash
                    }
                } else if (gPressed->type == OBJ_BUTTON && gPressed->autohilite) {
                    gPressed->hilite = 0;
                }

                gPressed = NULL;
                [self setNeedsDisplay:YES];
            }
            if (gMoving) {
                gMoving = NO;
                [self setNeedsDisplay:YES];
                return;
            }
            return;
        }

    // --- mode édition : finir la création par drag ---
    if (!gDragging) return;
    gDragging = NO;
    if (gDragRect.size.width < 8 || gDragRect.size.height < 8) {
        [self setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
        if (!card) return;
        Object *owner = gEditBackground ? card->bg : card;   // couche cible
        if (!owner) owner = card;                            // sécurité si pas de fond
        char name[64];
        Object *o;
        if (gTool == TOOL_BUTTON) {
            snprintf(name, sizeof name, "Bouton %d", ++gNewCount);
            o = hc_new_button(owner, name);
        } else {
            snprintf(name, sizeof name, "Champ %d", ++gNewCount);
            o = hc_new_field(owner, name);
        }
    o->x = (int)gDragRect.origin.x;
    o->y = (int)gDragRect.origin.y;
    o->w = (int)gDragRect.size.width;
    o->h = (int)gDragRect.size.height;
    gSelected = o;
    [self setNeedsDisplay:YES];
}
- (void)installMessageBox {
    gView = self;
    NSRect b = [self bounds];
    gMsgBox = [[NSTextField alloc] initWithFrame:NSMakeRect(10, b.size.height - 34, b.size.width - 20, 24)];
    [gMsgBox setEditable:YES];
    [gMsgBox setSelectable:YES];
    [gMsgBox setBezeled:YES];
    [gMsgBox setDrawsBackground:YES];
    [gMsgBox setBackgroundColor:[NSColor colorWithWhite:0.96 alpha:1.0]];
    [gMsgBox setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
    [gMsgBox setStringValue:@""];
    [gMsgBox setTarget:self];
    [gMsgBox setAction:@selector(messageBoxEntered:)];
    [self addSubview:gMsgBox];

    static HcHost host;
    host.line = cocoa_line;
    host.field_changed = cocoa_field_changed;
    hc_set_host(&host);
}

- (void)messageBoxEntered:(id)sender {
    NSString *cmd = [gMsgBox stringValue];
    if ([cmd length] == 0) return;
    hc_do([cmd UTF8String]);
    [self applyStackSize];          // ← applique un éventuel changement de taille
    [self setNeedsDisplay:YES];
    [gMsgBox selectText:nil];
}

- (void)installToolPalette {
    int cols = 4, rows = (NUM_TOOLCELLS + cols - 1) / cols;
    CGFloat cell = 38, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;

    NSPanel *palette = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(560, 350, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow)
                    backing:NSBackingStoreBuffered defer:NO];
    [palette setTitle:@"Outils"];
    [palette setFloatingPanel:YES];

    ToolPalette *grid = [[ToolPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [palette setContentView:grid];
    [palette makeKeyAndOrderFront:nil];
}
- (void)toggleFilled:(id)sender {
    gShapeFilled = !gShapeFilled;
    NSLog(@"formes : %@", gShapeFilled ? @"PLEINES" : @"VIDES");
}
- (void)toolChosen:(id)sender {
    gTool = (HCTool)[sender tag];
    gSelected = NULL;
    [self setNeedsDisplay:YES];
    NSLog(@"outil : %d", (int)gTool);
}
- (void)editScriptOf:(Object *)obj {
    gEditTarget = obj;

    NSPanel *panel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(300, 200, 480, 340)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    char d[64]; hc_describe(obj, d, sizeof d);
    [panel setTitle:[NSString stringWithFormat:@"Script de %s", d]];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(10, 44, 460, 286)];
    [scroll setHasVerticalScroller:YES];
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    NSTextView *tv = [[NSTextView alloc] initWithFrame:[[scroll contentView] bounds]];
    [tv setFont:[NSFont fontWithName:@"Monaco" size:12]];
    [tv setAutoresizingMask:NSViewWidthSizable];
    const char *cur = hc_script_of(obj);
    [tv setString:cur ? [NSString stringWithUTF8String:cur] : @""];
    [scroll setDocumentView:tv];
    [[panel contentView] addSubview:scroll];

    gEditView = tv;

    NSButton *ok = [[NSButton alloc] initWithFrame:NSMakeRect(390, 8, 80, 30)];
    [ok setTitle:@"OK"];
    [ok setBezelStyle:NSBezelStyleRounded];
    [ok setTarget:self];
    [ok setAction:@selector(saveScript:)];
    [ok setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];
    [[panel contentView] addSubview:ok];

    gEditPanel = panel;
    [panel makeKeyAndOrderFront:nil];
}

- (void)saveScript:(id)sender {
    if (gEditTarget && gEditView) {
        hc_set_script(gEditTarget, [[gEditView string] UTF8String]);
    }
    [gEditPanel close];
    gEditPanel = nil; gEditView = nil; gEditTarget = nil;
    [self setNeedsDisplay:YES];
}
- (void)beginFieldEdit:(Object *)field {
    [self endFieldEdit];
    gEditingField = field;
    NSRect r = NSMakeRect(field->x, field->y, field->w, field->h);
    gFieldEditor = [[NSTextView alloc] initWithFrame:NSInsetRect(r, 2, 2)];
    [gFieldEditor setFont:[NSFont systemFontOfSize:13]];
    const char *tx = field->contents ? field->contents : "";
    [gFieldEditor setString:[NSString stringWithUTF8String:tx]];
    [self addSubview:gFieldEditor];
    [[self window] makeFirstResponder:gFieldEditor];
}

- (void)endFieldEdit {
    if (gFieldEditor && gEditingField) {
        hc_set_field_text(gEditingField, [[gFieldEditor string] UTF8String]);
    }
    [gFieldEditor removeFromSuperview];
    gFieldEditor = nil;
    gEditingField = NULL;
    [self setNeedsDisplay:YES];
}
 
@end
