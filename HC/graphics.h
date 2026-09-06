#ifndef graphics_h
#define graphics_h

#import <Cocoa/Cocoa.h>
#import "HCglobals.h"

#define NUM_PATTERNS 38

int pattern_bit(int pat, int x, int y);

NSBitmapImageRep *paint_bitmap(Object *o, int w, int h);

void paint_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, NSColor *color, CGFloat width);
void erase_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, CGFloat width);
void brush_stamp(NSBitmapImageRep *rep, int cx, int cy);
void brush_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to);

/* Aérographe. Sème `density` points isolés dans un disque de rayon `radius`,
 * sans jamais peindre le fond : repasser assombrit au lieu d'effacer.
 * L'appelant doit continuer à appeler ces fonctions tant que le bouton est
 * enfoncé, MÊME si la souris ne bouge pas — c'est la différence de nature
 * avec le pinceau, et ce qui permet de charger un point en insistant. */
/* Réglages de l'aérographe, ajustables par double-clic sur l'outil.
 * Le rayon donne la taille du nuage ; la densité, le nombre de points par
 * passe. Densité faible = nuage granuleux qu'on charge en insistant ;
 * densité forte = rond plein, autrement dit un pinceau. */
extern int gSprayRadius;    /* défaut 8  */
extern int gSprayDensity;   /* défaut 30 */

void spray_stamp(NSBitmapImageRep *rep, int cx, int cy, int radius, int density);
void spray_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to,
                  int radius, int density);

void paint_shape(NSBitmapImageRep *rep, HCTool tool, NSPoint a, NSPoint b, NSColor *color, CGFloat width);
void fill_shape(NSBitmapImageRep *rep, HCTool tool, NSPoint a, NSPoint b);
void flood_fill(NSBitmapImageRep *rep, int sx, int sy);

void paint_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n, CGFloat width);
void fill_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n);
void erase_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n);
void erase_rect(NSBitmapImageRep *rep, NSPoint a, NSPoint b);

void copy_rect(NSBitmapImageRep *rep, NSPoint a, NSPoint b);
void copy_freeform(NSBitmapImageRep *rep, NSPoint *pts, int n);
void stamp_clipboard(NSBitmapImageRep *rep, NSPoint pos);

void dither_region(NSBitmapImageRep *rep, int x0, int y0, int x1, int y1,
                   NSPoint *poly, int npoly);
/* ---- Les transformations du menu Paint ----
 *
 * Toutes travaillent sur une ZONE : un rectangle, éventuellement restreint au
 * polygone du lasso (poly/npoly, NULL sinon) — la convention de
 * dither_region, qui les a précédées.
 *
 * Flip et Rotate font exception et travaillent sur la boîte englobante :
 * retourner une forme quelconque « sur elle-même » n'a pas de sens
 * géométrique, et HyperCard ne le prétendait pas davantage.
 *
 * Le calcul est dans hc_pixels.h, en C pur et vérifiable hors du Mac. */
void paint_invert(NSBitmapImageRep *rep, int x0,int y0,int x1,int y1, NSPoint *poly,int npoly);
void paint_darken(NSBitmapImageRep *rep, int x0,int y0,int x1,int y1, NSPoint *poly,int npoly);
void paint_lighten(NSBitmapImageRep *rep, int x0,int y0,int x1,int y1, NSPoint *poly,int npoly);
void paint_trace_edges(NSBitmapImageRep *rep, int x0,int y0,int x1,int y1, NSPoint *poly,int npoly);
void paint_flip(NSBitmapImageRep *rep, int x0,int y0,int x1,int y1, int horizontal);
void paint_rotate(NSBitmapImageRep *rep, int x0,int y0,int x1,int y1, int sens, NSRect *nouveau);

NSBitmapImageRep *paint_copy(NSBitmapImageRep *src);
void paint_swap(NSBitmapImageRep *a, NSBitmapImageRep *b);
#endif /* graphics_h */
