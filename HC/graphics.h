#ifndef graphics_h
#define graphics_h

#import <Cocoa/Cocoa.h>
#import "HCglobals.h"

#define NUM_PATTERNS 38

int pattern_bit(int pat, int x, int y);

NSBitmapImageRep *paint_bitmap(Object *o, int w, int h);

void paint_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, NSColor *color, CGFloat width);
void erase_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, CGFloat width);
/* CE PIXEL PORTE-T-IL DE L'ENCRE ?
 *
 * Deux façons de n'en pas porter : le VIDE — alpha nul, la convention que
 * suivent déjà le flot de remplissage et les transformations de zone —, et
 * le FOND OPAQUE, c'est-à-dire la couleur de fond posée franchement.
 *
 * Le second cas n'existait pas tant que le crayon n'effaçait qu'en
 * transparent. Il est né avec le mode opaque : sans lui, un pixel blanchi
 * resterait « encré », le clic suivant voudrait l'effacer encore, et la
 * bascule se coincerait d'un côté.
 *
 * x et y sont des coordonnées de VUE, donc de ligne : la vue de carte est
 * retournée, et les lignes de bitmapData se comptent depuis le haut. C'est
 * ce que font déjà brush_stamp et spray_stamp. */
int  paint_pixel_encre(NSBitmapImageRep *rep, int x, int y);

/* RETIRER L'ENCRE D'UN SEGMENT — la moitié « efface » du crayon.
 *
 * CE QUE « RETIRER » VEUT DIRE DÉPEND DU MODE DE FOND, et c'est la seule
 * question que pose cette fonction : transparent, on retire pour de bon et
 * le calque du dessous réapparaît ; opaque, on pose la couleur de fond, qui
 * couvre — le blanc d'HyperCard.
 *
 * La question est posée ICI et nulle part ailleurs : les deux chemins du
 * crayon, le clic et « drag » par script, l'appellent tous deux. */
void unink_stroke(NSBitmapImageRep *rep, NSPoint from, NSPoint to, CGFloat width);
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

/* LES SOMMETS D'UNE FORME POLYGONALE inscrite entre deux points.
 *
 * Le rectangle arrondi et le polygone régulier passent par ici, et par ici
 * SEULEMENT : le contour et le remplissage lisent la MÊME liste. Tracer un
 * chemin de Bézier et remplir par balayage aurait donné deux définitions de
 * la même forme, qui se seraient écartées d'un pixel dans les coins — un
 * liseré de fond entre le trait et l'intérieur, visible et inexplicable.
 *
 * Rend le nombre de points posés, 0 si la forme n'est pas de celles-là. */
#define HC_SOMMETS_MAX 256
int  shape_sommets(HCTool tool, NSPoint a, NSPoint b, NSPoint *out, int max);

/* LES SOMMETS D'UN POLYGONE RÉGULIER, côtés donnés plutôt que lus.
 *
 * Extrait de shape_sommets pour que la boîte « Polygon Sides » dessine ses
 * six choix avec LE MÊME calcul que l'outil. Une boîte qui montre un
 * hexagone pendant que l'outil en trace un autre est pire qu'une boîte
 * absente : elle a l'autorité d'un aperçu.
 *
 * `centre` est le centre, `vers` donne à la fois le rayon et l'angle du
 * premier sommet. Rend le nombre de points, 0 si le rayon est nul. */
int  poly_sommets(int cotes, NSPoint centre, NSPoint vers,
                  NSPoint *out, int max);

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
void paint_restore(NSBitmapImageRep *a, NSBitmapImageRep *b);
void paint_fill_zone(NSBitmapImageRep *rep, int x0, int y0, int x1, int y1,
                     NSPoint *poly, int npoly);
#endif /* graphics_h */
