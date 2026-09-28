#import "HCprint.h"
#import "HCview.h"      /* l'interface complete : on appelle drawRect: sur gView */
#import "HCglobals.h"   /* gView */
#include <stdlib.h>     /* malloc, free */
#include <string.h>     /* memcpy */

/* ─── Impression ─────────────────────────────────────────────────────────────
 *
 * Une vue jetable qui dessine les cartes les unes après les autres, une par
 * page. NSPrintOperation s'occupe du panneau, de l'aperçu et du PDF — « Enre-
 * gistrer au format PDF » y est offert sans une ligne de plus, ce qui est
 * commode quand on n'a pas d'imprimante sous la main.
 *
 * Le dessin passe par la vue existante plutôt que d'être réécrit : on lui
 * demande de se dessiner pour chaque carte, en déplaçant la carte courante le
 * temps de la page. C'est le même procédé que le tri, qui se déplace pour
 * évaluer ses clés — et il évite d'avoir deux codes de rendu qui divergeraient.
 */
@interface HCPrintView : NSView
@property (assign) Object **cards;
@property (assign) int       count;
@property (assign) NSSize    cardSize;
@property (assign) int       pageEnCours;
/* Le decoupage de « print card from x,y to x,y ». decoupeVue vaut NO pour la
 * carte entiere. */
@property (assign) BOOL      decoupeVue;
@property (assign) NSRect    decoupe;
@end

@implementation HCPrintView

- (BOOL)isFlipped { return YES; }

- (BOOL)knowsPageRange:(NSRangePointer)range {
    range->location = 1;
    range->length   = (NSUInteger)self.count;
    return YES;
}

- (NSRect)rectForPage:(NSInteger)page {
    /* La bande de la carte demandée. On retient AUSSI son numéro : avec la
     * pagination désactivée, AppKit ne décale pas le dessin d'une page à
     * l'autre — il rappelle drawRect: avec la même origine, et sans ce
     * repère toutes les pages porteraient la première carte. */
    self.pageEnCours = (int)page - 1;
    return NSMakeRect(0, (page - 1) * self.cardSize.height,
                      self.cardSize.width, self.cardSize.height);
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    if (!self.cards || self.count <= 0 || !gView) return;

    int i = self.pageEnCours;
    if (i < 0 || i >= self.count) i = 0;

    /* Se placer SUR la carte à imprimer : la vue dessine celle du noyau, et
     * sans ce déplacement toutes les pages porteraient la même. */
    Object *avant = hc_current_card();
    hc_set_current_card(self.cards[i]);

    [NSGraphicsContext saveGraphicsState];
    /* Ramener l'origine sur la bande de CETTE page : le contexte est placé au
     * début de la bande demandée par rectForPage:, mais la vue dessine
     * toujours en 0,0. */
    NSAffineTransform *t = [NSAffineTransform transform];
    [t translateXBy:0 yBy:i * self.cardSize.height];
    [t concat];

    NSRect r = NSMakeRect(0, 0, self.cardSize.width, self.cardSize.height);
    [[NSColor whiteColor] setFill];
    NSRectFill(r);
    /* « print card from 0,0 to 512,304 » n'imprime qu'une PARTIE de la carte :
     * trois fonds de « Stack Templates » l'ecrivent, et leur auteur dit
     * pourquoi — « does not print the buttons along the bottom ». On DECOUPE
     * apres avoir blanchi la page, de sorte que ce qui est exclu reste blanc.
     *
     * CE QUI N'EST PAS MESURE, et qui s'ecrit comme non mesure : la page garde
     * la bande de la carte ENTIERE. HyperCard recadrait peut-etre la page sur le
     * rectangle ; on n'a pas de banc pour le dire, et inventer un recadrage
     * serait decider seul de ce qu'il faisait. Ce qui est mesure est l'intention
     * de l'auteur, ecrite dans son propre commentaire. */
    if (self.decoupeVue) NSRectClip(self.decoupe);
    [gView drawRect:r];
    [NSGraphicsContext restoreGraphicsState];

    hc_set_current_card(avant);
}

@end

void cocoa_print_cards(Object **cards, int n, const int *decoupe) {
    if (!cards || n <= 0 || !gView) return;

    /* La taille de la première carte : une pile a une taille unique, et
     * mélanger des piles dans une même impression n'a pas de sens. */
    Object *pile = cards[0]->owner;
    while (pile && pile->type != OBJ_STACK) pile = pile->owner;
    CGFloat w = (pile && pile->w > 0) ? pile->w : 512;
    CGFloat h = (pile && pile->h > 0) ? pile->h : 342;

    /* Copier la liste : le noyau nous passe un tableau local, qui disparaît
     * dès que la commande rend la main. runOperation est modal, donc il
     * survivrait — mais compter là-dessus rendrait la fonction fragile au
     * moindre changement d'AppKit. */
    Object **copie = malloc(sizeof(Object *) * (size_t)n);
    if (!copie) return;
    memcpy(copie, cards, sizeof(Object *) * (size_t)n);

    HCPrintView *pv = [[HCPrintView alloc]
        initWithFrame:NSMakeRect(0, 0, w, h * n)];
    pv.cards    = copie;
    pv.count    = n;
    pv.cardSize = NSMakeSize(w, h);
    if (decoupe) {
        pv.decoupeVue = YES;
        pv.decoupe = NSMakeRect(decoupe[0], decoupe[1],
                                decoupe[2] - decoupe[0], decoupe[3] - decoupe[1]);
    }

    NSPrintInfo *info = [[NSPrintInfo sharedPrintInfo] copy];
    [info setTopMargin:36];  [info setBottomMargin:36];
    [info setLeftMargin:36]; [info setRightMargin:36];

    /* Paysage si la carte est plus large que haute : une pile 512×342 y perd
     * beaucoup moins qu'en portrait. */
    [info setOrientation:(w > h) ? NSPaperOrientationLandscape
                                 : NSPaperOrientationPortrait];

    /* Faire tenir chaque carte ENTIÈREMENT dans sa page.
     *
     * La pagination automatique découpe ce qui déborde : une carte plus haute
     * que la page sortait sur deux feuilles, coupée au milieu. On calcule donc
     * l'échelle nous-mêmes — le plus petit des deux rapports, pour que ni la
     * largeur ni la hauteur ne dépasse — et l'on n'agrandit jamais : une carte
     * qui tient déjà garde sa taille réelle, ce qui préserve le rendu des
     * trames en noir et blanc. */
    NSSize papier = [info paperSize];
    CGFloat dispoW = papier.width  - [info leftMargin] - [info rightMargin];
    CGFloat dispoH = papier.height - [info topMargin]  - [info bottomMargin];
    CGFloat ech = 1.0;
    if (w > dispoW || h > dispoH) {
        CGFloat ex = dispoW / w, ey = dispoH / h;
        ech = ex < ey ? ex : ey;
    }
    [info setScalingFactor:ech];

    /* Pagination désactivée dans les deux sens : l'échelle garantit déjà que
     * chaque carte tient, et rectForPage: place la bonne bande. Laisser
     * AppKit paginer par-dessus ajouterait des pages vides. */
    [info setHorizontalPagination:NSPrintingPaginationModeClip];
    [info setVerticalPagination:NSPrintingPaginationModeClip];

    NSPrintOperation *op = [NSPrintOperation printOperationWithView:pv
                                                          printInfo:info];
    [op setShowsPrintPanel:YES];
    [op setShowsProgressPanel:YES];
    [op runOperation];

    free(copie);
}
