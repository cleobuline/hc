#ifndef HCpalettes_h
#define HCpalettes_h

#import "HCglobals.h"

#define NUM_PATTERNS 38
#define ICONGRID_COLS 6
#define ICONGRID_CELL 44

extern const int NUM_TOOLCELLS;

int pattern_bit(int pat, int x, int y);

@interface PatternPalette : NSView
@end

@interface ToolPalette : NSView
@end

@interface WidthPalette : NSView
@end
#define NUM_BRUSHES 12
int brush_bit(int brush, int x, int y);

@interface BrushPalette : NSView
@end

/* Grille de choix d'icone.
 *
 * Elle parcourt le CATALOGUE (hcicon_catalog_*), c'est-a-dire les icones de la
 * pile puis celles d'origine : elle voit donc les icones creees par
 * l'utilisateur, et une icone que la pile redefinit n'apparait qu'une fois.
 * Son nombre d'elements varie — appeler reload apres toute modification du
 * catalogue, elle ajuste sa hauteur et se redessine.
 *
 * target/action sont prevenus a chaque changement de selection, ce dont
 * l'editeur a besoin pour suivre. Facultatifs. */
@interface IconGrid : NSView
@property (assign) int selected;
@property (assign) id  target;
@property (assign) SEL action;
- (void)reload;
+ (CGFloat)heightForCount:(int)n;
@end
#endif /* HCpalettes_h */
