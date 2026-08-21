//
//  graphics.m — fonctions de dessin bitmap
//

#import "graphics.h"
#import "HCpalettes.h"    // brush_bit


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

// modifié #define NUM_PATTERNS (int)(sizeof(PATTERNS)/sizeof(PATTERNS[0]))



int pattern_bit(int pat, int x, int y) {
    unsigned char row = PATTERNS[pat][y & 7];
    return (row >> (7 - (x & 7))) & 1;
}

/* ---- Couleur d'encre et de fond, pour les fonctions qui ecrivent le pixel ----
 *
 * Les fonctions a contexte (paint_stroke, paint_shape) posent simplement
 * [gInkColor setStroke]. Celles qui touchent le pixel directement ont besoin
 * des composantes, et il serait absurde de les recalculer a chaque point : on
 * les extrait une fois par appel, dans des variables locales.
 *
 * Le passage par sRGB est obligatoire : une couleur nommee ([NSColor
 * blackColor]) ou issue d'un catalogue n'a pas de composantes tant qu'on ne
 * l'a pas convertie, et getRed:… leve sur elle.
 *
 * Le repli sur noir et blanc couvre le cas ou hc_colors_init() n'a pas encore
 * tourne — le dessin retombe alors exactement sur le comportement d'avant. */
static void color_rgb(NSColor *c, NSColor *repli,
                      unsigned char *r, unsigned char *g, unsigned char *b)
{
    if (!c) c = repli;
    NSColor *s = [c colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    if (!s) s = [repli colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    if (!s) { *r = *g = *b = 0; return; }
    CGFloat rr = 0, gg = 0, bb = 0, aa = 1;
    [s getRed:&rr green:&gg blue:&bb alpha:&aa];
    *r = (unsigned char)(rr * 255.0 + 0.5);
    *g = (unsigned char)(gg * 255.0 + 0.5);
    *b = (unsigned char)(bb * 255.0 + 0.5);
}

/* Les six composantes, a declarer en tete des fonctions a pixel. Un macro
 * plutot que six lignes recopiees cinq fois : la moindre divergence entre
 * deux copies donnerait un outil qui ne peint pas comme les autres. */
#define INK_RGB_LOCALS \
    unsigned char ir_, ig_, ib_, br_, bg_, bb_; \
    color_rgb(gInkColor,  [NSColor blackColor], &ir_, &ig_, &ib_); \
    color_rgb(gBackColor, [NSColor whiteColor], &br_, &bg_, &bb_)

/* Poser l'encre / poser le fond, alpha opaque. */
#define PUT_INK(px)  do { (px)[0]=ir_; (px)[1]=ig_; (px)[2]=ib_; \
                          if (spp>=4) (px)[3]=255; } while (0)
#define PUT_BACK(px) do { (px)[0]=br_; (px)[1]=bg_; (px)[2]=bb_; \
                          if (spp>=4) (px)[3]=255; } while (0)

void dither_region(NSBitmapImageRep *rep, int x0, int y0, int x1, int y1,
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

void fill_shape(NSBitmapImageRep *rep, HCTool tool, NSPoint a, NSPoint b) {
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

    INK_RGB_LOCALS;

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
                PUT_INK(px);                   // trait du motif : l'encre
            } else {
                if (gInk == INK_ERASE) {
                    px[0]=0; px[1]=0; px[2]=0;
                    if (spp>=4) px[3]=0;       // efface
                } else if (gTransparentBg) {
                    continue;                  // laisser intact
                } else {
                    PUT_BACK(px);              // fond opaque
                }
            }
        }
    }
}
// dessine une forme libre (contour fermé) à partir d'une liste de points

void paint_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n, CGFloat width) {
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
        [(gBackColor ? gBackColor : [NSColor whiteColor]) setStroke];
    } else {
        [(gInkColor ? gInkColor : [NSColor blackColor]) setStroke];
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


void paint_shape(NSBitmapImageRep *rep, HCTool tool, NSPoint a, NSPoint b, NSColor *color, CGFloat width) {
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

    // encre : voir paint_stroke, meme logique
    if (gInk == INK_ERASE) {
        CGContextSetBlendMode([ctx CGContext], kCGBlendModeClear);
        [[NSColor blackColor] setStroke];
    } else if (gInk == INK_WHITE) {
        [(gBackColor ? gBackColor : [NSColor whiteColor]) setStroke];
    } else {
        [(gInkColor ? gInkColor : [NSColor blackColor]) setStroke];
    }
    (void)color;   /* le parametre reste pour la signature ; ce sont les
                    * globales qui gouvernent, comme pour l'outil texte */

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

void flood_fill(NSBitmapImageRep *rep, int sx, int sy) {
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

    INK_RGB_LOCALS;

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
            PUT_INK(px);                          // trait du motif : l'encre
        } else {
            if (gInk == INK_ERASE) {
                px[0]=0; px[1]=0; px[2]=0;
                if (spp>=4) px[3]=0;              // efface tout
            } else if (gTransparentBg) {
                /* fond : laisser intact */
            } else {
                PUT_BACK(px);                     // fond opaque
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

void brush_stamp(NSBitmapImageRep *rep, int cx, int cy) {
    int W = (int)[rep pixelsWide], H = (int)[rep pixelsHigh];
    unsigned char *data = [rep bitmapData];
    if (!data) return;
    NSInteger bpr = [rep bytesPerRow], spp = [rep samplesPerPixel];
    INK_RGB_LOCALS;
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
                PUT_INK(px);                     // trait du motif : l'encre
            } else if (gTransparentBg) {
                continue;                        // fond : laisser intact
            } else {
                PUT_BACK(px);                    // fond opaque
            }
        }
    }
}

// trace un segment au pinceau

/* Réglages courants de l'aérographe, modifiés par son panneau de réglage. */
int gSprayRadius  = 8;
int gSprayDensity = 30;

/* ---- aérographe ----
 * Le spray ne pose pas une forme pleine comme le pinceau : il sème des points
 * isolés dans un disque, avec une densité faible. Repasser au même endroit
 * assombrit progressivement — c'est ce qui fait tout le caractère de l'outil,
 * et pourquoi il doit continuer à pulvériser quand la souris ne bouge PAS.
 *
 * La densité est volontairement basse (une trentaine de points par passe pour
 * un rayon de 8) : à forte densité on obtient un rond plein, c'est-à-dire un
 * pinceau, et l'effet de nuage disparaît.
 *
 * Le tirage est uniforme dans le DISQUE, pas dans le carré : tirer x et y
 * indépendamment concentrerait les points aux quatre coins. On tire donc un
 * angle et un rayon, ce dernier en racine pour que la surface soit couverte
 * uniformément — sans la racine, le centre serait bien plus dense que le bord. */
void spray_stamp(NSBitmapImageRep *rep, int cx, int cy, int radius, int density) {
    if (!rep) return;
    int W = (int)[rep pixelsWide], H = (int)[rep pixelsHigh];
    unsigned char *data = [rep bitmapData];
    if (!data) return;
    NSInteger bpr = [rep bytesPerRow], spp = [rep samplesPerPixel];
    if (radius < 1) radius = 1;

    /* L'aerographe ne peint jamais le fond (voir plus bas) : seule l'encre
     * lui sert. Le macro en declare deux, le compilateur ecarte l'inutile. */
    INK_RGB_LOCALS;

    for (int i = 0; i < density; i++) {
        double ang = ((double)arc4random_uniform(100000) / 100000.0) * 2.0 * M_PI;
        double rr  = sqrt((double)arc4random_uniform(100000) / 100000.0) * radius;
        int x = cx + (int)lround(cos(ang) * rr);
        int y = cy + (int)lround(sin(ang) * rr);
        if (x < 0 || x >= W || y < 0 || y >= H) continue;

        unsigned char *px = data + y*bpr + x*spp;
        if (gInk == INK_ERASE) {
            px[0]=0; px[1]=0; px[2]=0;
            if (spp>=4) px[3]=0;
        } else if (pattern_bit(gPattern, x, y)) {
            PUT_INK(px);
        }
        /* Hors motif on ne pose RIEN : contrairement au pinceau, l'aérographe
         * ne peint jamais le fond. Un nuage doit laisser voir ce qu'il y a
         * dessous, sinon repasser dessus effacerait le dessin au lieu de
         * l'assombrir. */
    }
}

/* Pulvérise le long d'un segment. Le pas de 2 pixels évite de recalculer un
 * nuage complet à chaque pixel d'une diagonale, ce qui saturerait le trait. */
void spray_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to,
                  int radius, int density) {
    if (!rep) return;
    double dx = to.x - from.x, dy = to.y - from.y;
    double len = sqrt(dx*dx + dy*dy);
    int steps = (int)(len / 2.0);
    if (steps < 1) {
        spray_stamp(rep, (int)lround(from.x), (int)lround(from.y), radius, density);
        return;
    }
    for (int i = 0; i <= steps; i++) {
        double t = (double)i / steps;
        spray_stamp(rep, (int)lround(from.x + dx*t),
                         (int)lround(from.y + dy*t), radius, density);
    }
}

void brush_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to) {
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

void paint_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, NSColor *color, CGFloat width) {
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

    /* gInk reste un MODE — peindre, peindre en fond, effacer. Les deux couleurs
     * disent seulement AVEC QUOI. En noir et blanc elles valent noir et blanc,
     * et le comportement d'origine est alors le cas particulier, sans branche
     * supplementaire nulle part. */
    if (gInk == INK_ERASE) {
        CGContextSetBlendMode([ctx CGContext], kCGBlendModeClear);
        [[NSColor blackColor] setStroke];   // couleur ignorée en mode clear
    } else if (gInk == INK_WHITE) {
        [(gBackColor ? gBackColor : [NSColor whiteColor]) setStroke];
    } else {
        [(gInkColor ? gInkColor : [NSColor blackColor]) setStroke];
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

void erase_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, CGFloat width) {
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


NSBitmapImageRep *paint_bitmap(Object *o, int w, int h) {
    if (!gPaintCache) gPaintCache = [NSMutableDictionary dictionary];
    NSValue *key = [NSValue valueWithPointer:o];
    NSBitmapImageRep *rep = [gPaintCache objectForKey:key];
    if (rep && ((int)[rep pixelsWide] != w || (int)[rep pixelsHigh] != h)) {
        [gPaintCache removeObjectForKey:key];
        rep = nil;
    }
    if (rep) return rep;
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

void erase_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n) {
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

void stamp_clipboard(NSBitmapImageRep *rep, NSPoint pos) {
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

void copy_rect(NSBitmapImageRep *rep, NSPoint a, NSPoint b) {
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

void copy_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n) {
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

void erase_rect(NSBitmapImageRep *rep, NSPoint a, NSPoint b) {
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
/* ---- dessine le nom d'un bouton, centre dans le rect ---- */

void fill_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n) {
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

    INK_RGB_LOCALS;

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
                PUT_INK(px);                   // trait du motif : l'encre
            } else {
                if (gInk == INK_ERASE) {
                    px[0]=0; px[1]=0; px[2]=0;
                    if (spp>=4) px[3]=0;       // efface
                } else if (gTransparentBg) {
                    continue;                  // laisser intact
                } else {
                    PUT_BACK(px);              // fond opaque
                }
            }
        }
    }
}
