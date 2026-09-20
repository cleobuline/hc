#ifndef HCglobals_h
#define HCglobals_h
#import <Cocoa/Cocoa.h>
#import "hc_core.h"

/* La vue principale est declaree en avant : ce fichier n'a besoin que du nom
 * de la classe, pas de son interface. Cela evite un cycle d'inclusion avec
 * HCview.h, qui lui importe HCglobals.h. */
@class HCView;

/* Les deux derniers sont AJOUTÉS EN QUEUE, et pas rangés près de leurs
 * cousins : la palette, les curseurs et les tables de noms se lisent par
 * valeur, et renuméroter l'énumération aurait déplacé tous les outils d'un
 * cran sans qu'aucun compilateur ne s'en plaigne. */
typedef enum { TOOL_BROWSE, TOOL_BUTTON, TOOL_FIELD, TOOL_PENCIL, TOOL_ERASER,
               TOOL_LINE, TOOL_RECT, TOOL_OVAL, TOOL_FILL, TOOL_FREEFORM,
               TOOL_LASSO, TOOL_SELRECT, TOOL_BRUSH , TOOL_TEXT,TOOL_SPRAY,
               TOOL_ROUNDRECT, TOOL_REGPOLY } HCTool;

/* UN OUTIL DE FORME : celui qui se tire au cerf-volant entre deux points, et
 * dont le tracé n'existe qu'au relâchement.
 *
 * Écrit UNE FOIS parce que la question se pose à quinze endroits — le
 * curseur, l'aperçu, l'annulation, la gravure, la palette d'épaisseur au
 * double-clic, le chemin des scripts. C'est en les comptant qu'on a compris
 * qu'ajouter deux noms à quinze listes revenait à choisir laquelle serait
 * oubliée ; et l'oubliée aurait donné un outil qui dessine mais qu'on ne
 * peut pas annuler, ou qui n'a pas d'aperçu — un défaut qu'on ne relie pas à
 * l'ajout d'un outil.
 *
 * La ligne en fait partie : elle se tire de la même façon. Ce qu'elle n'a
 * pas, c'est un intérieur — c'est « filled » qui le dit, pas cette
 * fonction. */
static inline int hcv_outil_forme(HCTool t)
{
    return t == TOOL_LINE      || t == TOOL_RECT ||
           t == TOOL_OVAL      || t == TOOL_ROUNDRECT ||
           t == TOOL_REGPOLY;
}

typedef enum { INK_BLACK, INK_WHITE, INK_ERASE } HCInk;

/* ---- Etat des outils de dessin ---- */
extern HCTool   gTool;
extern HCInk    gInk;
extern int      gPattern;
extern int      gLineWidth;
extern int      gBrush;
extern BOOL     gShapeFilled;
extern int      gTextSize;
extern BOOL     gTransparentBg;
/* LA GRILLE : le dessin et les objets se calent sur huit pixels.
 *
 * Huit, c'est le pas d'HyperCard, et ce n'est pas un chiffre rond par hasard :
 * c'est le côté d'une trame. Une forme calée sur la grille tombe donc sur les
 * bords du motif qui la remplit.
 *
 * Elle vit ici, avec les autres réglages d'outil, et non dans HCDoc : la
 * palette d'outils est unique pour toute l'application, et une grille active
 * dans une fenêtre et pas dans l'autre se lirait comme une panne. */
extern BOOL     gGrid;
/* FATBITS : le calque grossi huit fois, pour poser les pixels un par un.
 * Ici pour la même raison que gGrid — un réglage d'outil, donc unique pour
 * toute l'application. Le détail de son fonctionnement est en tête de
 * HCview.m, là où vit la transformation. */
extern BOOL     gFatBits;
/* Le nombre de côtés du polygone régulier. Quatre par défaut, comme
 * HyperCard. Les bornes vivent là où on l'écrit — trois est le minimum qui
 * enferme une surface, et au-delà d'une cinquantaine un polygone ne se
 * distingue plus d'un cercle à l'œil. */
extern int      gPolySides;
/* Couleur d'encre et couleur de fond du dessin.
 *
 * gInk reste ce qu'il etait : un MODE — peindre, peindre en fond, effacer.
 * Ces deux couleurs disent seulement AVEC QUOI. En noir et blanc elles valent
 * noir et blanc, et le comportement d'origine est alors le cas particulier,
 * sans branche supplementaire nulle part. */
extern NSColor *gInkColor;
extern NSColor *gBackColor;
/* ---- Selection et vue ----
 *
 * gSelected N'EST PLUS ICI : l'objet selectionne est un champ de HCDoc, donc
 * PAR FENETRE, comme la carte affichee et le champ en edition. Il se lit et
 * se pose par hcv_selection et hcv_selectionne, declares dans HCview.h.
 *
 * Tant qu'il etait global, passer d'une pile a l'autre laissait Couper,
 * Copier, Bring Closer et le panneau des polices braques sur un objet de la
 * pile qu'on venait de quitter. */
extern Object  *gFontTarget;   /* objet vise par le panneau des polices */
extern HCView  *gView;

/* ---- Presse-papiers peinture ---- */
extern NSBitmapImageRep *gClipboard;
extern int      gClipW, gClipH;
extern NSPoint  gClipPts[4096];
extern int      gClipPtsCount;

/* ---- Cache des bitmaps de peinture ---- */
extern NSMutableDictionary *gPaintCache;
void hc_colors_init(void);
#endif
