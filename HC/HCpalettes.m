//
//  HCpalettes.m
//  HC
//
//  Created by Patricia Benedetto on 31/07/2026.
//

#import "HCpalettes.h"
#import "graphics.h"   /* poly_sommets : la boîte « Polygon Sides »
                             * dessine ses choix avec le calcul de l'outil */
#import "icons.h"
#import "HCview.h"
#import "HCicons.h"
// #import "HCview.h"
// ==================== palette d'épaisseur de trait (vue custom) ====================
 
// 12 brosses 16x16 (bit a 1 = pixel peint)
static const unsigned short BRUSHES[12][16] = {
    /* 0 : point 1px */
    {0,0,0,0,0,0,0,0x0080,0,0,0,0,0,0,0,0},
    /* 1 : carre 2 */
    {0,0,0,0,0,0,0x00C0,0x00C0,0,0,0,0,0,0,0,0},
    /* 2 : carre 4 */
    {0,0,0,0,0,0x03C0,0x03C0,0x03C0,0x03C0,0,0,0,0,0,0,0},
    /* 3 : carre 8 */
    {0,0,0,0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0,0,0,0},
    /* 4 : rond 4 */
    {0,0,0,0,0,0x0180,0x03C0,0x03C0,0x03C0,0x0180,0,0,0,0,0,0},
    /* 5 : rond 8 */
    {0,0,0,0x0180,0x07E0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x07E0,0x0180,0,0,0},
    /* 6 : rond 12 */
    {0,0x0180,0x07E0,0x0FF0,0x1FF8,0x3FFC,0x3FFC,0x3FFC,0x3FFC,0x3FFC,0x3FFC,0x1FF8,0x0FF0,0x07E0,0x0180,0},
    /* 7 : barre horizontale */
    {0,0,0,0,0,0,0,0x3FFC,0x3FFC,0,0,0,0,0,0,0},
    /* 8 : barre verticale */
    {0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180},
    /* 9 : oblique \ */
        {0xC000,0xE000,0x7000,0x3800,0x1C00,0x0E00,0x0700,0x0380,0x01C0,0x00E0,0x0070,0x0038,0x001C,0x000E,0x0007,0x0003},
        /* 10 : oblique / */
        {0x0003,0x0007,0x000E,0x001C,0x0038,0x0070,0x00E0,0x01C0,0x0380,0x0700,0x0E00,0x1C00,0x3800,0x7000,0xE000,0xC000},
    /* 11 : croix */
        {0,0,0,0x0180,0x0180,0x0180,0x0180,0x0180,0x0FF0,0x0FF0,0x0180,0x0180,0x0180,0x0180,0x0180,0},
};

int brush_bit(int brush, int x, int y) {
    if (brush < 0 || brush >= NUM_BRUSHES) brush = 5;
    if (x < 0 || x > 15 || y < 0 || y > 15) return 0;
    return (BRUSHES[brush][y] >> (15 - x)) & 1;
}

// ==================== les curseurs des outils ====================
/* LE POINTEUR NE SUIVAIT PAS L'OUTIL.
 *
 * Choisir le crayon, le lasso ou le seau changeait `gTool`, encadrait la case
 * dans la palette, redessinait la carte — et laissait la flèche. Il fallait
 * donc regarder la palette pour savoir avec quoi on dessinait, alors que
 * l'information doit être sous les yeux, au bout du bras.
 *
 * TROIS RÈGLES, et elles ne sont pas décoratives.
 *
 * 1. Le POINT CHAUD dit où la peinture tombe. Un crayon dont le point chaud
 *    serait au centre dessinerait huit pixels à côté de sa pointe. Chaque
 *    point chaud ci-dessous a donc été relevé sur le code de dessin, pas
 *    choisi à l'œil : la gomme efface un trait de seize pixels de large
 *    centré sur la souris (erase_stroke), l'aérographe pulvérise autour
 *    d'elle (spray_stamp), et le pinceau pose son bitmap à partir de
 *    (cx-8, cy-8) — d'où (8,8), et non le centre du dessin.
 *
 * 2. Le CONTOUR BLANC n'est pas un ornement. Une silhouette noire seule
 *    disparaît sur la peinture noire, et c'est précisément là qu'on dessine.
 *    HyperCard cerne tous ses curseurs de blanc pour cette raison.
 *
 * 3. On ne dessine QUE ce que le système ne sait pas dire. La croix, la main,
 *    le I-beam et la flèche existent déjà en curseurs système, nets sur tous
 *    les écrans et conformes aux réglages de l'utilisateur ; les redessiner
 *    serait s'inventer du travail et rendre un résultat moins bon.
 *
 * Le pinceau, lui, MONTRE SA FORME : il est construit depuis brush_bit(), le
 * même tableau que la palette des brosses et que brush_stamp(). Choisir la
 * brosse oblique fait donc apparaître une oblique sous la souris.
 */

/* '#' noir, '@' blanc, '.' transparent. Seize lignes de seize colonnes. */

/* Le crayon : la pointe en bas à gauche, et c'est elle le point chaud. */
static const char *CUR_PENCIL[16] = {
    ".........#####..",
    "........#@@@@#..",
    ".......#@@@@@#..",
    "......#@@@@@#...",
    ".....#@@@@@#....",
    "....#@@@@@#.....",
    "...#@@@@@#......",
    "..#@@@@@#.......",
    ".#@@@@@#........",
    "#@@@@@#.........",
    "#@@@@#..........",
    "#@@@#...........",
    "#@@#............",
    "#@#.............",
    "##..............",
    "#...............",
};

/* La gomme fait SEIZE pixels de large — c'est la largeur que passe
 * erase_stroke. Le curseur occupe donc tout son carré : ce qu'il couvre est
 * exactement ce qu'il effacera. */
static const char *CUR_ERASER[16] = {
    "################",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "#@@@@@@@@@@@@@@#",
    "################",
};

/* Le seau, et la goutte qui tombe : c'est la goutte qui marque le pixel d'où
 * part le remplissage.
 *
 * Dessiné à la main sur l'écran, après deux tentatives de ma part à l'aveugle
 * qui donnaient un cornet de glace puis une chaussette. Il y a des choses
 * qu'on ne peut pas faire sans voir le résultat.
 *
 * Les quatorze pixels blancs autour du filet et de la goutte, eux, sont
 * calculés : ces deux morceaux étaient du noir plein, sans un blanc nulle
 * part, et disparaissaient donc sur la peinture noire — précisément la partie
 * qu'il faut voir pour viser. Le corps du seau, lui, a son intérieur blanc et
 * se passait déjà de contour extérieur ; il n'est pas touché. */
static const char *CUR_BUCKET[16] = {
    "................",
    "......###.......",
    ".....#@@@##.....",
    "....#@@@@@@##...",
    "...#@@@@@@@@#...",
    "..#@@@@@@@@#....",
    ".#@@@@@@@@#.....",
    "#@@@@@@@@#......",
    ".#@@@@@@#.......",
    "..#@@@@#........",
    "...#@@#.........",
    "..@@##..........",
    ".@@##@@.........",
    ".@####@.........",
    ".@####@.........",
    ".@@##@@.........",
};

/* Le lasso : la boucle, et la corde dont la pointe est le point chaud. */
static const char *CUR_LASSO[16] = {
    "....######......",
    "...#@@@@@@#.....",
    "..#@#....#@#....",
    ".#@#......#@#...",
    ".#@#......#@#...",
    ".#@#......#@#...",
    "..#@#....#@#....",
    "...#@@@@@@#.....",
    "....##@@@#......",
    ".....#@@#.......",
    "....#@@#........",
    "...#@@#.........",
    "..#@@#..........",
    "..#@#...........",
    ".##.............",
    "#...............",
};

/* L'aérographe : la bombe en bas à droite, le nuage en haut à gauche. Le
 * point chaud est DANS le nuage — spray_stamp pulvérise autour de la souris,
 * pas devant une buse. */
static const char *CUR_SPRAY[16] = {
    ".#...#..........",
    "...#....#.......",
    ".#....#....#....",
    "....#...#.......",
    ".#...#.....#....",
    "...#...#........",
    ".....#....#.....",
    ".........####...",
    "........#@@@@#..",
    ".......#@@@@@@#.",
    ".......#@@@@@@#.",
    ".......#@@@@@@#.",
    ".......#@@@@@@#.",
    ".......#@@@@@@#.",
    ".......#@@@@@@#.",
    "........######..",
};

/* La montre de « set the cursor to watch ».
 *
 * Ce n'est pas un outil : c'est un script qui dit « je travaille, attends ».
 * Elle était rendue par operationNotAllowedCursor — le 🚫 d'interdiction —
 * puis par la flèche, faute de mieux : macOS n'expose ni montre ni sablier
 * public, et les curseurs d'attente du système sont privés.
 *
 * Alors on la dessine, comme le Macintosh d'origine : le cadran, ses deux
 * brins de bracelet, et les aiguilles sur midi et trois heures. Elles ne
 * tournent pas — une montre animée demanderait une minuterie, et une montre
 * arrêtée dit déjà ce qu'il faut. */
static const char *CUR_WATCH[16] = {
    ".....######.....",
    ".....#@@@@#.....",
    "...##########...",
    "..#@@@@@@@@@@#..",
    ".#@@@@@@@@@@@@#.",
    ".#@@@@@#@@@@@@#.",
    ".#@@@@@#@@@@@@#.",
    ".#@@@@@#@@@@@@#.",
    ".#@@@@@#####@@#.",
    ".#@@@@@@@@@@@@#.",
    ".#@@@@@@@@@@@@#.",
    "..#@@@@@@@@@@#..",
    "...##########...",
    ".....#@@@@#.....",
    ".....######.....",
    "................",
};

/* Fabrique une image 16x16 depuis une planche ASCII. Le blanc et le noir sont
 * OPAQUES, le point est transparent : c'est ce qui donne la silhouette. */
static NSImage *image_16_depuis_ascii(const char **plan)
{
    NSBitmapImageRep *rep =
        [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                pixelsWide:16 pixelsHigh:16
                                             bitsPerSample:8 samplesPerPixel:4
                                                  hasAlpha:YES isPlanar:NO
                                            colorSpaceName:NSDeviceRGBColorSpace
                                               bytesPerRow:0 bitsPerPixel:0];
    if (!rep) return nil;
    unsigned char *data = [rep bitmapData];
    NSInteger bpr = [rep bytesPerRow];
    memset(data, 0, (size_t)(bpr * 16));
    for (int y = 0; y < 16; y++) {
        const char *ligne = plan[y];
        if (!ligne) continue;
        for (int x = 0; x < 16 && ligne[x]; x++) {
            unsigned char *px = data + y*bpr + x*4;
            if (ligne[x] == '#')      { px[0]=0;   px[1]=0;   px[2]=0;   px[3]=255; }
            else if (ligne[x] == '@') { px[0]=255; px[1]=255; px[2]=255; px[3]=255; }
        }
    }
    NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(16, 16)];
    [img addRepresentation:rep];
    return img;
}

/* Le pinceau courant, cerné de blanc.
 *
 * La silhouette vient de brush_bit — le tableau que la palette affiche et que
 * brush_stamp imprime. Le halo est calculé : tout pixel vide qui touche un
 * pixel plein devient blanc. Écrire douze contours à la main aurait été douze
 * occasions de se tromper, et il aurait fallu recommencer à chaque brosse
 * ajoutée.
 *
 * L'image fait 18x18 pour que le halo d'une brosse qui touche le bord ait où
 * se poser ; le point chaud se décale d'autant. */
static NSImage *image_pinceau(int brosse)
{
    NSBitmapImageRep *rep =
        [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                pixelsWide:18 pixelsHigh:18
                                             bitsPerSample:8 samplesPerPixel:4
                                                  hasAlpha:YES isPlanar:NO
                                            colorSpaceName:NSDeviceRGBColorSpace
                                               bytesPerRow:0 bitsPerPixel:0];
    if (!rep) return nil;
    unsigned char *data = [rep bitmapData];
    NSInteger bpr = [rep bytesPerRow];
    memset(data, 0, (size_t)(bpr * 18));

    for (int y = 0; y < 18; y++) {
        for (int x = 0; x < 18; x++) {
            int bx = x - 1, by = y - 1;   /* la brosse est décalée de un */
            int plein = (bx >= 0 && bx < 16 && by >= 0 && by < 16)
                      ? brush_bit(brosse, bx, by) : 0;
            unsigned char *px = data + y*bpr + x*4;
            if (plein) { px[0]=0; px[1]=0; px[2]=0; px[3]=255; continue; }
            /* Vide : blanc s'il touche un plein, transparent sinon. */
            int touche = 0;
            for (int dy = -1; dy <= 1 && !touche; dy++)
                for (int dx = -1; dx <= 1 && !touche; dx++) {
                    int vx = bx + dx, vy = by + dy;
                    if (vx >= 0 && vx < 16 && vy >= 0 && vy < 16 &&
                        brush_bit(brosse, vx, vy)) touche = 1;
                }
            if (touche) { px[0]=255; px[1]=255; px[2]=255; px[3]=255; }
        }
    }
    NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(18, 18)];
    [img addRepresentation:rep];
    return img;
}

/* Les curseurs dessinés, construits une fois. Le pinceau ne peut pas être
 * gardé ainsi : il change avec la brosse, d'où son cache à part. */
static NSCursor *gCurPencil = nil, *gCurEraser = nil, *gCurBucket = nil;
static NSCursor *gCurLasso  = nil, *gCurSpray  = nil;
static NSCursor *gCurBrush  = nil;
static int       gCurBrushPour = -1;   /* la brosse que gCurBrush représente */

/* __strong explicite : sans lui, ARC prend « NSCursor ** » pour un paramètre
 * __autoreleasing et refuse qu'on lui passe l'adresse d'une variable statique,
 * dont la propriété est __strong. C'est une erreur de compilation, pas un
 * avertissement — et elle n'apparaît qu'au moment de construire. */
static NSCursor *curseur_cache(NSCursor * __strong *ou,
                               const char **plan, NSPoint chaud)
{
    if (!*ou) {
        NSImage *img = image_16_depuis_ascii(plan);
        if (!img) return [NSCursor arrowCursor];
        *ou = [[NSCursor alloc] initWithImage:img hotSpot:chaud];
    }
    return *ou;
}

void hcv_curseur_pinceau_perime(void) { gCurBrush = nil; gCurBrushPour = -1; }

/* Le point chaud est le centre du cadran, là où les aiguilles se rejoignent :
 * une montre n'est pas un outil de visée, mais un pointeur doit bien pointer
 * quelque part, et le centre est le seul endroit qui ne surprenne personne. */
static NSCursor *gCurWatch = nil;
NSCursor *hcv_curseur_montre(void)
{
    return curseur_cache(&gCurWatch, CUR_WATCH, NSMakePoint(7, 8));
}

/* Le nom HyperCard du curseur d'un outil, pour que « the cursor » ne mente
 * pas après que le repos a rendu la main.
 *
 * Les outils qui portent un dessin maison n'ont pas de nom dans le
 * vocabulaire d'HyperCard — il n'y a pas de mot pour « le curseur du
 * crayon ». On rend « arrow » pour ceux-là : c'est faux d'un cheveu, mais
 * moins faux que de continuer d'annoncer « watch » une fois la montre
 * partie. */
const char *hcv_curseur_nom_outil(int outil)
{
    switch ((HCTool)outil) {
        case TOOL_BROWSE:   return "hand";
        case TOOL_TEXT:     return "ibeam";
        case TOOL_SELRECT:
        case TOOL_LINE:
        case TOOL_RECT:
        case TOOL_ROUNDRECT:
        case TOOL_OVAL:
        case TOOL_REGPOLY:
        case TOOL_FREEFORM: return "cross";
        default:            return "arrow";
    }
}

NSCursor *hcv_curseur_outil(int outil)
{
    switch ((HCTool)outil) {

        /* La main : on feuillette, on presse les boutons. */
        case TOOL_BROWSE:  return [NSCursor pointingHandCursor];

        /* Poser et déplacer des objets se fait à la flèche, comme partout. */
        case TOOL_BUTTON:
        case TOOL_FIELD:   return [NSCursor arrowCursor];

        /* La croix, pour tout ce qui se vise : une sélection, un tracé dont
         * on pointe les deux bouts. Le curseur système est plus net que tout
         * ce que je dessinerais, et suit les réglages d'accessibilité. */
        case TOOL_SELRECT:
        case TOOL_LINE:
        case TOOL_RECT:
        case TOOL_ROUNDRECT:
        case TOOL_OVAL:
        case TOOL_REGPOLY:
        case TOOL_FREEFORM: return [NSCursor crosshairCursor];

        case TOOL_TEXT:     return [NSCursor IBeamCursor];

        case TOOL_PENCIL:
            return curseur_cache(&gCurPencil, CUR_PENCIL, NSMakePoint(0, 15));
        case TOOL_ERASER:
            /* Seize pixels de large, centrés : le carré du curseur EST la
               surface effacée. */
            return curseur_cache(&gCurEraser, CUR_ERASER, NSMakePoint(8, 8));
        case TOOL_FILL:
            /* La goutte, pas le seau. */
            return curseur_cache(&gCurBucket, CUR_BUCKET, NSMakePoint(3, 14));
        case TOOL_LASSO:
            return curseur_cache(&gCurLasso,  CUR_LASSO,  NSMakePoint(0, 15));
        case TOOL_SPRAY:
            /* Dans le nuage : spray_stamp pulvérise AUTOUR de la souris. */
            return curseur_cache(&gCurSpray,  CUR_SPRAY,  NSMakePoint(4, 3));

        case TOOL_BRUSH:
            if (!gCurBrush || gCurBrushPour != gBrush) {
                NSImage *img = image_pinceau(gBrush);
                if (!img) return [NSCursor crosshairCursor];
                /* brush_stamp pose le bitmap à partir de (cx-8, cy-8) : le
                 * pixel (8,8) de la brosse est donc celui de la souris. Le
                 * décalage de un vient de la marge du halo. */
                gCurBrush = [[NSCursor alloc] initWithImage:img
                                                    hotSpot:NSMakePoint(9, 9)];
                gCurBrushPour = gBrush;
            }
            return gCurBrush;
    }
    return [NSCursor arrowCursor];
}

@implementation WidthPalette

/* Agir dès le PREMIER clic, même si le panneau n'est pas la fenêtre active.
 * Par défaut AppKit consomme ce clic pour activer la fenêtre, ce qui oblige à
 * cliquer deux fois : une fois pour réveiller la palette, une fois pour
 * choisir. Les palettes d'HyperCard répondent au premier coup. */
- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
    NSRectFill(dirtyRect);

    int cols = 4;
    CGFloat cell = 40, gap = 3, margin = 6;

    for (int i = 0; i <= 10; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap),
                                margin + row*(cell+gap),
                                cell, cell);

        BOOL active = (gLineWidth == i);

        [(active ? [NSColor whiteColor] : [NSColor colorWithWhite:0.82 alpha:1.0]) setFill];
        NSRectFill(box);

        if (i == 0) {
            NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
            [ps setAlignment:NSTextAlignmentCenter];
            NSDictionary *attrs = @{
                NSFontAttributeName: [NSFont systemFontOfSize:14],
                NSParagraphStyleAttributeName: ps
            };
            NSRect tr = box; tr.origin.y += (box.size.height - 18)/2;
            [@"0" drawInRect:tr withAttributes:attrs];
        } else {
            [[NSColor blackColor] setStroke];
            NSBezierPath *line = [NSBezierPath bezierPath];
            CGFloat midY = box.origin.y + box.size.height/2;
            [line moveToPoint:NSMakePoint(box.origin.x + 6, midY)];
            [line lineToPoint:NSMakePoint(box.origin.x + box.size.width - 6, midY)];
            [line setLineWidth:i];
            [line stroke];
        }

        if (active) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr = [NSBezierPath bezierPathWithRect:NSInsetRect(box, 1, 1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    int cols = 4;
    CGFloat cell = 40, gap = 3, margin = 6;
    for (int i = 0; i <= 10; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap),
                                margin + row*(cell+gap),
                                cell, cell);
        if (NSPointInRect(p, box)) {
            gLineWidth = i;
            [self setNeedsDisplay:YES];
            break;
        }
    }
}

@end



/* ==================== Polygon Sides ====================
 *
 * LES SIX VALEURS, relevées sur la boîte de la vraie HyperCard : un
 * triangle, un carré, puis des polygones de plus en plus ronds — le dernier
 * ne se distingue plus d'un cercle à l'œil, ce qui est précisément son
 * intérêt. Douze est le choix de l'utilisatrice pour celui-là ; les quatre
 * premières valeurs sont lisibles sur la capture, la cinquième est une
 * interpolation et se corrige en changeant un chiffre ici.
 *
 * Une seule table, et la palette comme le menu la lisent : deux listes de
 * choix auraient fini par ne plus dire la même chose. */
const int POLYCHOIX[NUM_POLYCHOIX] = { 3, 4, 5, 6, 8, 12 };

#define POLY_CELL   46
#define POLY_GAP    3
#define POLY_MARGE  6

static NSRect poly_case(int i)
{
    return NSMakeRect(POLY_MARGE + i * (POLY_CELL + POLY_GAP),
                      POLY_MARGE, POLY_CELL, POLY_CELL);
}

@implementation PolySidesPalette

- (BOOL)acceptsFirstMouse:(NSEvent *)event { (void)event; return YES; }
- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
    NSRectFill(dirtyRect);

    for (int i = 0; i < NUM_POLYCHOIX; i++) {
        NSRect box = poly_case(i);
        BOOL active = (gPolySides == POLYCHOIX[i]);

        [(active ? [NSColor whiteColor]
                 : [NSColor colorWithWhite:0.82 alpha:1.0]) setFill];
        NSRectFill(box);

        /* LE POLYGONE, par le calcul de l'outil. Pointe en HAUT : la vue est
         * retournée, donc « vers le haut » s'écrit en y décroissant. */
        NSPoint centre = NSMakePoint(NSMidX(box), NSMidY(box));
        NSPoint vers   = NSMakePoint(centre.x, centre.y - (POLY_CELL/2 - 7));
        NSPoint som[HC_SOMMETS_MAX];
        int n = poly_sommets(POLYCHOIX[i], centre, vers, som, HC_SOMMETS_MAX);
        if (n >= 3) {
            NSBezierPath *pth = [NSBezierPath bezierPath];
            [pth moveToPoint:som[0]];
            for (int k = 1; k < n; k++) [pth lineToPoint:som[k]];
            [pth closePath];
            [[NSColor blackColor] setStroke];
            [pth setLineWidth:1];
            [pth stroke];
        }

        if (active) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr =
                [NSBezierPath bezierPathWithRect:NSInsetRect(box, 1, 1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    for (int i = 0; i < NUM_POLYCHOIX; i++) {
        if (NSPointInRect(p, poly_case(i))) {
            gPolySides = POLYCHOIX[i];
            /* Choisir un nombre de côtés CHOISIT AUSSI L'OUTIL, comme dans
             * HyperCard : on n'ouvre pas cette boîte pour régler un outil
             * qu'on ne va pas prendre. */
            gTool = TOOL_REGPOLY;
            hcv_palette_outils_maj();
            hcv_curseur_maj();
            [self setNeedsDisplay:YES];
            break;
        }
    }
}

@end

// ==================== palette d'outils custom (grille + sélection encadrée) ====================
typedef struct { const char *glyph; int kind; int value; } ToolCell;

static const ToolCell TOOLCELLS[] = {
    {"👆", 0, TOOL_BROWSE},
    {"B",  0, TOOL_BUTTON},
    {"F",  0, TOOL_FIELD},
    {"✏", 0, TOOL_PENCIL},
    {"P", 0, TOOL_BRUSH},
    {"⌫", 0, TOOL_ERASER},
    {"╱", 0, TOOL_LINE},
    {"▭", 0, TOOL_RECT},
    {"▢", 0, TOOL_ROUNDRECT},
    {"○", 0, TOOL_OVAL},
    {"⬠", 0, TOOL_REGPOLY},
    {"💧", 0, TOOL_FILL},
    {"✎", 0, TOOL_FREEFORM},
    {"⬚", 0, TOOL_LASSO},
    {"◰", 0, TOOL_SELRECT},
    {"A", 0, TOOL_TEXT},//
    /* Aérographe — icône dans ICON_SPRAY32. Le glyphe reste comme repli si
     * la chaîne de dessin ne le reconnaît pas. */
    {"❊", 0, TOOL_SPRAY},
    /* Encre et fond. Le glyphe ne sert plus : ces deux cases se peignent avec
     * leur couleur courante, et un double-clic dessus la change — comme le
     * double-clic sur le pinceau ouvre ses reglages. */
    {"⬛", 1, INK_BLACK},
    {"⬜", 1, INK_WHITE},
    {"▣", 2, 0},
    {"◫", 3, 0},
};


const int NUM_TOOLCELLS = (int)(sizeof(TOOLCELLS)/sizeof(TOOLCELLS[0]));
 
 

#define ICONGRID_COLS 6
#define ICONGRID_CELL 44

@implementation IconGrid

- (BOOL)isFlipped { return YES; }

+ (CGFloat)heightForCount:(int)n {
    int rows = (n + ICONGRID_COLS - 1) / ICONGRID_COLS;
    if (rows < 1) rows = 1;
    return rows * ICONGRID_CELL;
}

/* Le catalogue grandit quand on cree une icone : la vue doit s'agrandir avec
 * lui, sinon les dernieres tombent hors du document et le defilement ne les
 * atteint jamais. */
- (void)reload {
    NSRect f = [self frame];
    f.size.height = [IconGrid heightForCount:hcicon_catalog_count()];
    [self setFrame:f];
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
    NSRectFill([self bounds]);

    int n = hcicon_catalog_count();
    for (int i = 0; i < n; i++) {
        int col = i % ICONGRID_COLS, row = i / ICONGRID_COLS;
        NSRect box = NSMakeRect(col*ICONGRID_CELL, row*ICONGRID_CELL,
                                ICONGRID_CELL, ICONGRID_CELL);
        if (!NSIntersectsRect(box, dirtyRect)) continue;

        const HCIcon *ic = hcicon_catalog_at(i);
        if (!ic) continue;

        BOOL active = (ic->id == self.selected);
        [(active ? [NSColor whiteColor] : [NSColor colorWithWhite:0.82 alpha:1.0]) setFill];
        NSRectFill(box);

        [[NSColor blackColor] setFill];
        hcicon_draw(ic, box, 1.0);

        if (active) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr = [NSBezierPath bezierPathWithRect:NSInsetRect(box, 1, 1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    int col = (int)(p.x / ICONGRID_CELL);
    int row = (int)(p.y / ICONGRID_CELL);
    int i = row * ICONGRID_COLS + col;
    if (col < 0 || col >= ICONGRID_COLS || i < 0 || i >= hcicon_catalog_count()) return;

    const HCIcon *ic = hcicon_catalog_at(i);
    if (!ic) return;
    self.selected = ic->id;
    [self setNeedsDisplay:YES];

    if (self.target && self.action &&
        [self.target respondsToSelector:self.action])
        [NSApp sendAction:self.action to:self.target from:self];
}

@end
@implementation ToolPalette

/* Agir dès le PREMIER clic, même si le panneau n'est pas la fenêtre active.
 * Par défaut AppKit consomme ce clic pour activer la fenêtre, ce qui oblige à
 * cliquer deux fois : une fois pour réveiller la palette, une fois pour
 * choisir. Les palettes d'HyperCard répondent au premier coup. */
- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
    NSRectFill(dirtyRect);

    int cols = 4;
    CGFloat cell = 38, gap = 3, margin = 6;

    for (int i = 0; i < NUM_TOOLCELLS; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap),
                                margin + row*(cell+gap),
                                cell, cell);
        const ToolCell *tc = &TOOLCELLS[i];

        BOOL active = NO;
        if (tc->kind == 0) active = (gTool == (HCTool)tc->value);
        else if (tc->kind == 1) active = (gInk == (HCInk)tc->value);
        else if (tc->kind == 2) active = gShapeFilled;
        else if (tc->kind == 3) active = gTransparentBg;
        [(active ? [NSColor whiteColor] : [NSColor colorWithWhite:0.82 alpha:1.0]) setFill];
        NSRectFill(box);

        /* Les deux cases d'encre se peignent avec leur couleur courante. C'est
         * le seul endroit ou le reglage se voit sans qu'on l'ouvre, et ce qui
         * rend le double-clic devinable : un carre de couleur invite a etre
         * change, un glyphe non. Repli sur noir et blanc si hc_colors_init n'a
         * pas encore tourne — la palette peut se dessiner avant. */
        if (tc->kind == 1) {
            NSColor *sc = (tc->value == INK_BLACK) ? gInkColor : gBackColor;
            if (!sc) sc = (tc->value == INK_BLACK) ? [NSColor blackColor]
                                                   : [NSColor whiteColor];
            NSRect sw = NSInsetRect(box, 7, 7);
            [sc setFill];
            NSRectFill(sw);
            [[NSColor colorWithWhite:0.35 alpha:1.0] setStroke];
            NSFrameRect(sw);
        }
        // icône bitmap pour certains outils, glyphe pour les autres
        else if (tc->kind == 0 && tc->value == TOOL_PENCIL) {
             draw_icon_ascii(ICON_PENCIL32, box);
         } else if  (tc->kind == 0 && tc->value == TOOL_FILL) {
                    draw_icon_ascii(ICON_BUCKET32, box);
                } else if (tc->kind == 0 && tc->value == TOOL_LASSO) {
                    draw_icon_ascii(ICON_LASSO32, box);
                } else if (tc->kind == 0 && tc->value == TOOL_ERASER) {
                    draw_icon_ascii(ICON_ERASER32, box);
                } else if (tc->kind == 0 && tc->value == TOOL_FREEFORM) {
                    draw_icon_ascii(ICON_FREEFORM32, box);
                } else if (tc->kind == 0 && tc->value == TOOL_SELRECT) {
                            draw_icon_ascii(ICON_SELRECT32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_BUTTON) {
                            draw_icon_ascii(ICON_BUTTON32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_FIELD) {
                   draw_icon_ascii(ICON_FIELD32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_RECT) {
                   draw_icon_ascii(ICON_RECT32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_LINE) {
                   draw_icon_ascii(ICON_LINE32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_OVAL) {
                   draw_icon_ascii(ICON_OVAL32, box);
               } else   if (tc->kind == 0 && tc->value == TOOL_BROWSE) {
                       draw_icon_ascii(ICON_HAND32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_BRUSH) {
                           draw_icon_ascii(ICON_BRUSH32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_TEXT) {
                           draw_icon_ascii(ICON_TEXT32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_SPRAY) {
                           draw_icon_ascii(ICON_SPRAY32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_ROUNDRECT) {
                           draw_icon_ascii(ICON_ROUNDRECT32, box);
               } else if (tc->kind == 0 && tc->value == TOOL_REGPOLY) {
                           draw_icon_ascii(ICON_REGPOLY32, box);
                }else{
                    NSString *g = [NSString stringWithUTF8String:tc->glyph];
                    NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                    [ps setAlignment:NSTextAlignmentCenter];
                    NSDictionary *attrs = @{
                        NSFontAttributeName: [NSFont systemFontOfSize:18],
                        NSParagraphStyleAttributeName: ps
                    };
                    NSRect tr = box;
                    tr.origin.y += (box.size.height - 22) / 2;
                    [g drawInRect:tr withAttributes:attrs];
                }

        if (active) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr = [NSBezierPath bezierPathWithRect:NSInsetRect(box, 1, 1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    BOOL dbl = ([event clickCount] == 2);
    int cols = 4;
    CGFloat cell = 38, gap = 3, margin = 6;
    for (int i = 0; i < NUM_TOOLCELLS; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap), margin + row*(cell+gap), cell, cell);
        if (NSPointInRect(p, box)) {
            const ToolCell *tc = &TOOLCELLS[i];
            if (tc->kind == 0) {
                // quitter l'outil texte : graver la saisie en cours
                if (gTool == TOOL_TEXT && tc->value != TOOL_TEXT)
                    [gView commitText];

                /* La sélection de peinture s'abandonne quand on prend un
                 * outil qui ne sélectionne pas — exactement comme le fait
                 * « choose … tool » par script. Ce chemin-ci l'oubliait :
                 * cliquer sur la main dans la palette laissait les fourmis en
                 * place et la sélection vivante. */
                if (tc->value != TOOL_SELRECT && tc->value != TOOL_LASSO)
                    hcv_abandonne_selection();

                gTool = (HCTool)tc->value;
                hcv_selectionne(NULL);
                /* Le pointeur suit l'outil. Ce chemin-ci est celui du CLIC
                 * dans la palette ; « choose ... tool » passe par
                 * cocoa_choose_tool, qui fait le même geste. Les deux doivent
                 * s'accorder, sinon le curseur serait juste au script et faux
                 * à la souris. */
                hcv_curseur_maj();

                /* Déposer un collage en attente. Une image plus grande que la
                 * carte n'a pas d'extérieur où cliquer pour la valider :
                 * changer d'outil devient alors le seul moyen de la poser, et
                 * c'est ce que fait HyperCard. */
                [gView dropFloating];

                /* Une minuterie d'aérographe qui survit à son outil
                 * continuerait de pulvériser dans le vide. Le relâchement de
                 * la souris l'arrête déjà, mais pas un changement d'outil
                 * fait au clavier ou pendant que le bouton est enfoncé. */
                [gView stopSprayTimer];

                if (dbl) {
                    // double-clic : ouvrir la palette de reglage associee
                    switch (tc->value) {
                    case TOOL_FILL:  [gView showPatternPalette]; break;
                    case TOOL_BRUSH: [gView showBrushPalette];   break;
                        case TOOL_SPRAY: [gView showSprayPalette];   break;
                        /* La gomme n'ouvre pas la palette d'epaisseur : dans
                         * HyperCard, son double-clic efface toute la couche. */
                        case TOOL_ERASER: [gView eraseAll]; break;
                        case TOOL_PENCIL: case TOOL_LINE:
                    case TOOL_RECT:   case TOOL_OVAL:   case TOOL_FREEFORM:
                    case TOOL_ROUNDRECT:
                        [gView showWidthPalette]; break;
                        /* Le polygone régulier a SA boîte, comme le pinceau a
                         * la sienne : c'est le nombre de côtés qu'on vient
                         * régler, pas l'épaisseur du trait. */
                    case TOOL_REGPOLY:
                        [gView showPolySidesPalette]; break;
                    default: break;
                    }
                }
            }
            else if (tc->kind == 1) {
                gInk = (HCInk)tc->value;
                /* Double-clic : choisir la couleur de CETTE encre. Le panneau
                 * du systeme est partage avec l'outil texte et l'edition de
                 * champ ; showDrawColorPanel: pose le drapeau qui dit a qui
                 * la prochaine couleur revient. */
                if (dbl) [gView showDrawColorPanel:(tc->value == INK_BLACK)];
            }
            else if (tc->kind == 2) { gShapeFilled = !gShapeFilled; }
            else if (tc->kind == 3) { gTransparentBg = !gTransparentBg; }
            [self setNeedsDisplay:YES];
            [gView setNeedsDisplay:YES];
            break;
        }
    }
}

@end
// ---- petite vue-grille pour choisir un motif ----
 

@implementation PatternPalette

/* Agir dès le PREMIER clic, même si le panneau n'est pas la fenêtre active.
 * Par défaut AppKit consomme ce clic pour activer la fenêtre, ce qui oblige à
 * cliquer deux fois : une fois pour réveiller la palette, une fois pour
 * choisir. Les palettes d'HyperCard répondent au premier coup. */
- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithWhite:0.85 alpha:1.0] setFill];
    NSRectFill(dirtyRect);

    int cols = 4;
    CGFloat cell = 32, gap = 4, margin = 6;

    for (int i = 0; i < NUM_PATTERNS; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap),
                                margin + row*(cell+gap),
                                cell, cell);
        // fond blanc de la case
        [[NSColor whiteColor] setFill];
        NSRectFill(box);

        // dessiner le motif dans la case, pixel par pixel (agrandi)
        // dessiner le motif : 8x8 bits, chaque bit = 4x4 px -> remplit la case 32x32
        [[NSColor blackColor] setFill];
        CGFloat px = cell / 8.0;
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (pattern_bit(i, x, y)) {
                    NSRectFill(NSMakeRect(box.origin.x + x*px,
                                          box.origin.y + y*px,
                                          px, px));
                }
            }
        
        }

        // cadre : rouge épais si c'est le motif courant, gris sinon
        if (i == gPattern) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr = [NSBezierPath bezierPathWithRect:NSInsetRect(box, -1, -1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor grayColor] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    int cols = 4;
    CGFloat cell = 32, gap = 4, margin = 6;
    for (int i = 0; i < NUM_PATTERNS; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap),
                                margin + row*(cell+gap),
                                cell, cell);
        if (NSPointInRect(p, box)) {
            gPattern = i;
            [self setNeedsDisplay:YES];
            [gView setNeedsDisplay:YES];
            break;
        }
    }
}

@end
@implementation BrushPalette

/* Agir dès le PREMIER clic, même si le panneau n'est pas la fenêtre active.
 * Par défaut AppKit consomme ce clic pour activer la fenêtre, ce qui oblige à
 * cliquer deux fois : une fois pour réveiller la palette, une fois pour
 * choisir. Les palettes d'HyperCard répondent au premier coup. */
- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
    NSRectFill(dirtyRect);
    int cols = 4;
    CGFloat cell = 34, gap = 3, margin = 6;
    for (int i = 0; i < NUM_BRUSHES; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap), margin + row*(cell+gap), cell, cell);
        BOOL active = (gBrush == i);
        [(active ? [NSColor whiteColor] : [NSColor colorWithWhite:0.82 alpha:1.0]) setFill];
        NSRectFill(box);
        // apercu de la brosse, 1 bit = 2x2 px, centre
        [[NSColor blackColor] setFill];
        CGFloat px = 2.0;
        CGFloat ox = box.origin.x + (cell - 16*px)/2;
        CGFloat oy = box.origin.y + (cell - 16*px)/2;
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++)
                if (brush_bit(i, x, y))
                    NSRectFill(NSMakeRect(ox + x*px, oy + y*px, px, px));
        if (active) {
            [[NSColor redColor] setStroke];
            NSBezierPath *fr = [NSBezierPath bezierPathWithRect:NSInsetRect(box, 1, 1)];
            [fr setLineWidth:2];
            [fr stroke];
        } else {
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            NSFrameRect(box);
        }
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    int cols = 4;
    CGFloat cell = 34, gap = 3, margin = 6;
    for (int i = 0; i < NUM_BRUSHES; i++) {
        int col = i % cols, row = i / cols;
        NSRect box = NSMakeRect(margin + col*(cell+gap), margin + row*(cell+gap), cell, cell);
        if (NSPointInRect(p, box)) {
            gBrush = i;
            /* Le curseur du pinceau MONTRE la brosse : changer de brosse le
             * périme. Sans cela, on choisirait l'oblique et l'on continuerait
             * de voir le rond sous la souris. */
            hcv_curseur_pinceau_perime();
            hcv_curseur_maj();
            [self setNeedsDisplay:YES];
            break;
        }
    }
}

@end
