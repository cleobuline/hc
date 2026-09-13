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

/* Le curseur qui va avec un outil.
 *
 * Les curseurs système là où ils sont justes — croix, main, I-beam, flèche —
 * et des silhouettes dessinées pour le crayon, la gomme, le seau, le lasso et
 * l'aérographe. Le pinceau, lui, MONTRE SA FORME : il est construit depuis
 * brush_bit(), si bien que choisir la brosse oblique fait apparaître une
 * oblique sous la souris.
 *
 * Le point chaud de chacun a été relevé sur le code de dessin, pas choisi à
 * l'œil : un curseur qui pointe à côté de l'endroit où la peinture tombe est
 * pire que pas de curseur du tout. Voir le commentaire de la définition.
 *
 * L'objet rendu est mis en cache ; ne pas le libérer. */
NSCursor *hcv_curseur_outil(int outil);

/* Le nom HyperCard du curseur d'un outil — « hand », « cross », « ibeam »,
 * « arrow » — pour que « the cursor » reste honnête après que le repos a
 * repris la main au script. */
const char *hcv_curseur_nom_outil(int outil);

/* À appeler quand gBrush change : le curseur du pinceau est alors périmé. */
void hcv_curseur_pinceau_perime(void);

/* La montre de « set the cursor to watch ». macOS n'expose ni montre ni
 * sablier public — les curseurs d'attente du système sont privés — alors on
 * la dessine, comme le Macintosh d'origine. */
NSCursor *hcv_curseur_montre(void);

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
