#ifndef HCview_h
#define HCview_h

#import <Cocoa/Cocoa.h>
#import "hc_core.h"
#import "HCglobals.h"   /* declare gView, une bonne fois pour toutes */

/* Propose un article de menu à la pile avant que l'action native n'agisse.
 * Rend YES si un « on doMenu » l'a pris : l'appelant sort alors sans rien
 * faire. Voir le commentaire de sa définition dans HCview.m. */
/* Les articles du menu Paint. L'étiquette d'un article de menu porte lequel,
 * et c'est ce qui les relie : AppDelegate pose les articles, HCview les
 * exécute. En commun ici, plutôt qu'en nombres recopiés de part et d'autre —
 * la première modification aurait fait diverger les deux copies sans que rien
 * ne le signale.
 *
 * Les huit premiers transforment la sélection et l'exigent donc. FILL la
 * remplit, et l'exige aussi. KEEP et REVERT, eux, portent sur la carte
 * entière : ils n'ont pas de sélection à demander, et paintOpTag les traite
 * avant d'en chercher une. */
enum {
    HCV_PAINT_INVERT = 1, HCV_PAINT_DARKEN, HCV_PAINT_LIGHTEN,
    HCV_PAINT_TRACE,      HCV_PAINT_FLIPH,  HCV_PAINT_FLIPV,
    HCV_PAINT_ROTL,       HCV_PAINT_ROTR,   HCV_PAINT_FILL,
    HCV_PAINT_KEEP,       HCV_PAINT_REVERT,
    /* Les quatre qui NE TRANSFORMENT PAS une sélection — ils la font, la
     * vident, ou changent le mode de dessin. Comme Keep et Revert, ils
     * passent avant le test de sélection dans paintOpTag:.
     *
     * « Select All » et « Clear Picture » existaient déjà, mais SEULEMENT
     * pour les scripts : leur code vivait dans cocoa_do_menu, qui n'est
     * appelé que par « doMenu ». Aucun article de menu ne les servait, si
     * bien qu'on ne pouvait ni tout sélectionner ni vider un calque à la
     * souris. « Transparent » n'était atteignable que par la palette des
     * trames, et pas du tout par script. */
    HCV_PAINT_SELECTALL,  HCV_PAINT_CLEAR,
    HCV_PAINT_OPAQUE,     HCV_PAINT_TRANSPARENT
};

BOOL hcv_menu_trappe(const char *article);

/* Redemander à la vue de carte quel curseur elle veut.
 *
 * À appeler dès que le choix change : outil, brosse, ou « set the cursor ».
 * Passe par les zones de curseur d'AppKit plutôt que par un [c set], pour que
 * le système le réapplique tout seul quand la souris revient sur la carte —
 * un curseur posé à la main serait écrasé par la première palette survolée et
 * ne reviendrait jamais. Voir la définition dans HCview.m. */
void hcv_curseur_maj(void);

/* Abandonner la sélection de peinture — rectangle, lasso, tracé libre — et
 * arrêter les fourmis.
 *
 * Un seul endroit pour un geste que trois chemins réclament : « choose … tool »
 * par script, le clic dans la palette d'outils, et le changement de carte.
 * Recopier le nettoyage à chacun est précisément ce qui avait laissé le clic
 * de palette incomplet. Voir la définition dans HCview.m. */
void hcv_abandonne_selection(void);

@interface HCView : NSView <NSTextViewDelegate>
- (void)installMessageBox;
/* La minuterie d'« idle ». Une seule pour toute l'application : elle envoie le
 * message à la carte de la fenêtre active, jamais aux autres. */
- (void)startIdleTimer;
- (void)stopIdleTimer;
- (void)idleTick:(NSTimer *)t;
/* L'état de document de cette vue, et la carte qu'elle affiche. Chaque fenêtre
 * a les siens ; HCDocument désigne l'actif par hc_set_active_doc. */
- (void *)docState;
- (Object *)documentCard;
/* La carte mémorisée par CETTE vue, sans passer par le noyau.
 * documentCard interroge hc_current_card() dès que la vue est active, ce qui
 * ne vaut rien au moment précis où l'on bascule de fenêtre : le noyau tient
 * encore la carte de la pile qu'on vient de quitter. */
- (Object *)rememberedCard;
- (void)dropFloating;
/* Lâcher l'édition et la sélection avant un changement de carte venu de
 * l'interface. Voir HCview.m. */
- (void)prepareForCardChange;
/* Un article d'un menu créé par script a été choisi ; l'étiquette de
 * l'article porte ses deux indices. Voir HCview.m. */
- (void)hcMenuScriptItem:(id)sender;
/* Les transformations du menu Paint. paintOp: lit l'étiquette de l'article ;
 * paintOpTag: sert aux scripts, qui n'en ont pas. Voir HCview.m. */
- (void)paintOp:(id)sender;
- (void)paintOpTag:(NSInteger)quoi;
- (void)findInStack:(id)sender;
- (void)messageBoxEntered:(id)sender;
- (void)installToolPalette;
- (void)toolChosen:(id)sender;
- (void)editScriptOf:(Object *)obj;
- (void)saveScript:(id)sender;
- (void)beginFieldEdit:(Object *)field;
- (void)endFieldEdit;
- (void)commitText;
- (void)testScribble;
- (void)flushPaintToKernel;
- (void)clearPaintCache;
- (void)resetForNewStack;
- (void)toggleBackground:(id)sender;
- (void)installPatternPalette;
- (void)applyStackSize;
- (void)installWidthPalette;
- (void)installBrushPalette;
- (void)startSprayTimer;
- (void)stopSprayTimer;
- (void)sprayTick:(NSTimer *)t;
- (void)showSprayPalette;
- (void)updateSprayLabels;
- (void)sprayRadiusChanged:(id)sender;
- (void)sprayDensityChanged:(id)sender;
- (void)widthChosen:(id)sender;
- (void)toggleFilled:(id)sender;
- (void)ditherSelection:(id)sender;
- (void)showPatternPalette;
- (void)showToolPalette;
- (void)togglePalette:(id)sender;
- (BOOL)paletteVisibleForTag:(NSInteger)tag;
- (void)showWidthPalette;
- (void)showBrushPalette;
- (void)newBackground:(id)sender;
- (void)updateWindowTitle;
- (void)showDrawColorPanel:(BOOL)ink;
- (void)eraseAll;
- (void)beginPaintUndo;
- (void)keepPaint;      /* menu Paint : Keep   — l'état courant fait référence */
- (void)revertPaint;    /* menu Paint : Revert — retour à cette référence */
- (void)undo:(id)sender;
/* Cartes : articles de menu distincts de Couper et Copier, comme dans
 * HyperCard. Coller reste commun — il pose ce que le presse-papiers contient. */
- (void)copyCard:(id)sender;
- (void)cutCard:(id)sender;
- (void)duplicateCard:(id)sender;
@end

#endif /* HCview_h */
