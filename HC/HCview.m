#import "HCview.h"
#import "hc_core.h"
#import <objc/runtime.h>   // pour associer un bitmap à un Object
#include <stdlib.h>        // getenv, pour la trace HC_RUNS_DEBUG
#include <strings.h>       // strcasecmp, pour les noms de proprietes globales
#include <float.h>         // FLT_MAX, pour la taille libre de l'editeur de champ
#include <mach/mach.h>     // host_statistics64, pour « the heapSpace »
#import <QuartzCore/QuartzCore.h>  // CATransaction, pour pousser les pixels a l ecran
#import "icons.h"
#import "HCglobals.h"
#import "HCtext.h"
#import "HCvisual.h"
#import "HCprint.h"
#import "HCpalettes.h"
#import "HCicons.h"
#import "Hciconedit.h"
#import "graphics.h"
#import "hc_file.h"   /* hc_save, pour « save stack ... as ... » */
#import "HCdialogs.h"
#import "HCpaint.h"
#import "hct_verif.h"
#import "Hcdocument.h"   /* allDocuments : oublier un objet mort partout */

extern void hc_sync_size_field(Object *o);  // definie dans HCdialogs.m
extern Object *cocoa_open_stack(const char *nom);      // definies dans AppDelegate.m
extern Object *cocoa_load_stack(const char *nom);
extern void    cocoa_stack_changed(Object *stack);
static NSFont *text_font(void);

typedef struct {
    /* édition d'un champ */
    Object       *editingField;
    NSTextView   *fieldEditor;
    NSScrollView *fieldScroll;

    /* édition d'un script */
    Object       *editTarget;
    NSTextView   *editView;
    NSPanel      *editPanel;

    /* sélection et interaction */

    /* L'OBJET SÉLECTIONNÉ, PAR DOCUMENT — et il a mis du temps à arriver ici.
     *
     * Il est resté un global de processus pendant que tout ce qui l'entoure
     * déménageait dans cette structure. gTool étant global lui aussi, passer
     * de la pile A à la pile B avec l'outil Bouton en main laissait
     * object_selection_active() vrai sur un objet de A : Couper le supprimait
     * dans la fenêtre du dessous, Bring Closer y changeait l'ordre de
     * superposition, et drawRect peignait son cadre rouge par-dessus la carte
     * de B, aux coordonnées d'un objet absent.
     *
     * Le premier correctif l'ABANDONNAIT au changement de fenêtre, comme la
     * sélection de peinture. Le ranger ici vaut mieux et coûte moins : il n'y
     * a plus rien à effacer, puisque chaque fenêtre a la sienne. Revenir sur
     * la pile A y retrouve sa sélection, et la question « à qui appartient
     * l'objet sélectionné ? » n'a plus qu'une réponse possible. */
    Object       *selected;

    Object       *pressed;        /* objet sous le bouton de la souris */
    /* LA CARTE SUR LAQUELLE LE CLIC A EU LIEU.
     *
     * `pressed` était retenu avant l'envoi du message, son contexte de carte
     * ne l'était pas : la fin automatique du clic lisait la carte COURANTE
     * après le gestionnaire, qu'un simple « go next card » avait déjà changée.
     * Les deux voyagent maintenant ensemble. */
    Object       *pressedCard;
    /* LE MINUTEUR APPARTIENT AU GESTE, DONC AU DOCUMENT.
     *
     * Il était un global de processus alors que `pressed` est par document :
     * deux fenêtres ne pouvaient pas avoir un geste chacune, et surtout
     * startStillDownTimer arrêtait celui de l'autre fenêtre en passant. */
    NSTimer      *stillDownTimer;
    Object       *popupTarget;    /* menu popup ouvert */
    NSArray<NSString *> *popupItems;
    NSArray<NSNumber *> *popupItemLines; /* lignes HC, base 1 */
    NSRect        popupRect;
    CGFloat       popupRowHeight;
    NSInteger     popupKeyboardRow;
    NSInteger     popupChosenRow;
    BOOL          popupFlashInverted;
    NSInteger     popupFlashToggles;
    NSTimer      *popupFlashTimer;
    NSTimeInterval popupOpenedAt;
    Object       *scrollField;    /* champ dont on glisse la poignée */
    Object       *clickField;     /* champ du dernier clic */
    NSPoint       clickPoint;

    /* couche affichée */
    BOOL          editBackground; /* NO = carte, YES = fond */

    /* collage de peinture en attente de dépôt */
    BOOL          floating;
    NSPoint       floatPos;

    /* saisie de l'outil texte */
    BOOL             textActive;
    NSPoint          textPos;
    NSMutableString *textBuf;
    NSBitmapImageRep *paintUndo;
    Object           *paintUndoLayer;

    /* Le point de repère de Revert : l'état du calque avant le premier coup
     * de pinceau donné dessus. Distinct de paintUndo, qui ne remonte que d'un
     * pas — Revert défait toute la séance de peinture d'un coup. */
    NSBitmapImageRep *keepSnap;
    Object           *keepLayer;

    Object       *card;

    /* numérotation des cartes créées */
    int           newCount;
} HCDoc;

static HCDoc  gDoc0;
static HCDoc *gDoc = &gDoc0;
void hc_set_active_doc(void *d) { gDoc = d ? (HCDoc *)d : &gDoc0; }

#define gEditingField    (gDoc->editingField)
#define gFieldEditor     (gDoc->fieldEditor)
#define gFieldScroll     (gDoc->fieldScroll)
#define gEditTarget      (gDoc->editTarget)
#define gEditView        (gDoc->editView)
#define gEditPanel       (gDoc->editPanel)
#define gSelected        (gDoc->selected)
#define gPressed         (gDoc->pressed)
#define gPressedCard     (gDoc->pressedCard)
#define gPopupTarget     (gDoc->popupTarget)
#define gPopupItems      (gDoc->popupItems)
#define gPopupItemLines  (gDoc->popupItemLines)
#define gPopupRect       (gDoc->popupRect)
#define gPopupRowHeight  (gDoc->popupRowHeight)
#define gPopupKeyboardRow (gDoc->popupKeyboardRow)
#define gPopupChosenRow  (gDoc->popupChosenRow)
#define gPopupFlashInverted (gDoc->popupFlashInverted)
#define gPopupFlashToggles (gDoc->popupFlashToggles)
#define gPopupFlashTimer (gDoc->popupFlashTimer)
#define gPopupOpenedAt   (gDoc->popupOpenedAt)
#define gScrollField     (gDoc->scrollField)
#define gClickField      (gDoc->clickField)
#define gClickPoint      (gDoc->clickPoint)
#define gEditBackground  (gDoc->editBackground)
#define gFloating        (gDoc->floating)
#define gFloatPos        (gDoc->floatPos)
#define gTextActive      (gDoc->textActive)
#define gTextPos         (gDoc->textPos)
#define gTextBuf         (gDoc->textBuf)
#define gNewCount        (gDoc->newCount)
#define gDocCard         (gDoc->card)
#define gPaintUndo       (gDoc->paintUndo)
#define gPaintUndoLayer  (gDoc->paintUndoLayer)
#define gKeepSnap        (gDoc->keepSnap)
#define gKeepLayer       (gDoc->keepLayer)

static NSTextField *gMsgBox = nil;
static NSPanel *gMsgPanel = nil;

static NSPoint gDragStart;
static NSRect  gDragRect;
static BOOL    gDragging = NO;
static BOOL    gMoving = NO;
static NSPoint gMoveStart;
static int     gObjStartX, gObjStartY;
static int     gResizeHandle = 0;
static int     gObjStartW, gObjStartH;

static NSPoint gPenLast;
static BOOL    gPenDrawing = NO;

/* LE CRAYON BASCULE, IL NE NOIRCIT PAS.
 *
 * Chez HyperCard, un crayon posé sur un pixel déjà noir EFFACE, et continue
 * d'effacer tant qu'on ne relâche pas. C'est ce qui permet de retoucher un
 * dessin sans changer d'outil, et c'est le geste que tout le monde connaît.
 *
 * LA RÈGLE ÉTAIT DÉJÀ DANS LE PROGRAMME : l'éditeur d'icônes la tient depuis
 * toujours, dans son _drawValue. Le crayon de la carte ne l'appliquait pas —
 * il noircissait, toujours. Deux crayons dans le même logiciel, deux
 * comportements, et celui de la carte était le faux.
 *
 * Le sens est décidé au PREMIER point et vaut pour tout le trait. Le
 * redécider à chaque segment ferait clignoter le crayon le long de son
 * propre tracé : il effacerait ce qu'il vient de poser dès qu'il repasse
 * dessus, ce qui est exactement ce qu'on ne veut pas. C'est aussi ce que
 * fait l'éditeur d'icônes, qui pose _drawValue une fois. */
static BOOL    gPenEfface = NO;

typedef enum { AXIS_NONE, AXIS_HORIZONTAL, AXIS_VERTICAL } HCAxisLock;
static HCAxisLock gLockedAxis = AXIS_NONE;

static NSPoint gShapeStart;
static NSPoint gShapeEnd;
static BOOL    gShapeDrawing = NO;
static int       gTextHeight = 0;
static NSString *gTextStyleName = nil;
static NSString *gTextAlign = nil;
 
static NSPoint gLassoPts[4096];
static int gLassoCount = 0;
static BOOL gLassoDrawing = NO;
static BOOL gLassoActive = NO;

static NSPoint gSelStart, gSelEnd;
static BOOL gSelRectDrawing = NO;
static BOOL gSelRectActive = NO;

static NSPanel *gPatternPanel = nil;
static NSPanel *gToolPanel = nil;
static NSPanel *gWidthPanel = nil;
static NSPanel *gBrushPanel = nil;

/* ═══ Prévenir une palette qu'un script vient de changer son réglage ═════
 *
 * « set the pattern to 12 » posait bien le motif, mais la palette des motifs
 * continuait d'entourer l'ancien : l'écran disait une chose, le programme en
 * faisait une autre. Un état montré faux est pire qu'un état non montré —
 * l'utilisateur clique là où il croit être.
 *
 * Le clic de l'utilisateur, lui, redessinait sa palette depuis toujours : il
 * ne manquait que le chemin des scripts. On invalide plutôt qu'on force le
 * dessin — cent « set the pattern » dans une boucle ne doivent pas faire cent
 * redessins, mais un par image.
 *
 * Rien si la palette est fermée : elle se redessinera en s'ouvrant. */
static void hcv_palette_maj(NSPanel *p)
{
    if (p && [p isVisible]) [(NSView *)[p contentView] setNeedsDisplay:YES];
}

static BOOL gTextUnderline = NO;

static CGFloat gScrollGrab, gScrollGH, gScrollKH, gScrollGY, gScrollMax;

#define NUM_PATTERNS 38

static NSPoint gFreePts[4096];
static int gFreeCount = 0;
static BOOL gFreeDrawing = NO;

static BOOL gFloatDragging = NO;
static NSPoint gFloatGrab;
static NSFont *gTextFont = nil;

/* Fourmis de feu */
static NSTimer *gAntsTimer = nil;
static CGFloat  gAntsPhase = 0.0;

@interface HCView ()
- (void)popupFlashTick:(NSTimer *)timer;
- (void)startAntsTimer;
- (void)stopAntsTimer;
@end

/* ABANDONNER LA SÉLECTION DE PEINTURE, EN UN SEUL ENDROIT.
 *
 * Ce nettoyage existait, écrit à la main dans « choose … tool ». Le CLIC dans
 * la palette d'outils, lui, ne le faisait pas : prendre la main en cliquant
 * laissait les fourmis en place et la sélection vivante. Et un changement de
 * carte ne le faisait pas davantage — la sélection suivait d'une carte à
 * l'autre, portant sur des pixels qui n'étaient plus là.
 *
 * Deux chemins pour un même geste, dont un seul complet : c'est exactement la
 * divergence qui s'installe quand on recopie au lieu d'appeler. Une fonction,
 * et les trois chemins la partagent.
 *
 * La minuterie des fourmis s'arrête avec : sans elle, elle continuerait de
 * redessiner un cadre qui n'entoure plus rien, quinze fois par seconde. */
/* LA SÉLECTION D'OBJET, VUE DU DEHORS.
 *
 * gSelected est un champ de HCDoc depuis qu'il est par document, et gDoc est
 * privé à ce fichier. La palette des outils et le dialogue Infos en ont
 * pourtant besoin : deux verbes plutôt qu'une variable partagée, et le
 * document actif reste le seul à décider de quoi on parle.
 *
 * C'est la même raison qui avait fait de hc_pp_oublie un verbe plutôt que
 * deux globales sorties de leur fichier. */
Object *hcv_selection(void)          { return gSelected; }
void    hcv_selectionne(Object *o)   { gSelected = o; }

void hcv_abandonne_selection(void)
{
    gSelRectActive  = NO;
    gSelRectDrawing = NO;
    gLassoActive    = NO;
    gLassoDrawing   = NO;
    gLassoCount     = 0;
    gFreeDrawing    = NO;
    gFreeCount      = 0;
    [gView stopAntsTimer];
}

/* Une partie mérite-t-elle d'être redessinée ? La marge couvre le cadre
 * d'édition et le liseré de sélection, qui débordent un peu. */
static inline BOOL part_touche(Object *o, NSRect sale) {
    if (!o) return NO;
    return NSIntersectsRect(sale, NSMakeRect(o->x - 8, o->y - 8,
                                             o->w + 16, o->h + 16));
}
static NSPoint constrain_to_axis(NSPoint start, NSPoint current) {
    CGFloat dx = fabs(current.x - start.x);
    CGFloat dy = fabs(current.y - start.y);
    if (dx > dy) {
        return NSMakePoint(current.x, start.y);
    } else {
        return NSMakePoint(start.x, current.y);
    }
}

/* ═══ LA GRILLE ════════════════════════════════════════════════════════
 *
 * Huit pixels, le pas d'HyperCard. Ce n'est pas un chiffre rond gratuit :
 * c'est le côté d'une trame, si bien qu'une forme calée tombe sur les bords
 * du motif qui la remplit.
 *
 * CE QUI SE CALE, ET CE QUI NE SE CALE PAS. Les outils de FORME — ligne,
 * rectangle, ovale —, le rectangle de sélection, et les objets qu'on
 * déplace, redimensionne ou crée. PAS le crayon, le pinceau, l'aérographe,
 * la gomme, le tracé libre ni le lasso : caler une main levée sur huit
 * pixels ne la contraint pas, elle la rend inutilisable — on ne dessinerait
 * plus que des pointillés. La règle est donc « ce qui a des COINS se cale »,
 * et elle se tient sans exception à retenir.
 *
 * LES OBJETS SONT UNE EXTENSION À NOUS. Chez HyperCard la grille ne touche
 * que la peinture ; les boutons se traînent au pixel. Mais aligner deux
 * boutons est justement ce pour quoi on allume une grille, et s'en priver
 * par fidélité aurait été fidèle à la lettre contre l'usage. C'est écrit ici
 * pour qu'on sache lequel des deux on a choisi, et pourquoi.
 *
 * On cale le RÉSULTAT et non la souris : deux boutons posés grille allumée
 * ont exactement le même x, ce qui est le service rendu. Caler l'écart de
 * la souris n'aurait aligné que les gestes, pas les objets.
 *
 * Grille éteinte, les deux fonctions sont l'identité — les appelants n'ont
 * donc aucun test à faire, et aucun ne peut oublier d'en faire un. */
#define HCV_GRILLE 8

static int hcv_cale(int v)
{
    if (!gGrid) return v;
    return (int)lround((double)v / HCV_GRILLE) * HCV_GRILLE;
}

static NSPoint hcv_cale_pt(NSPoint p)
{
    if (!gGrid) return p;
    return NSMakePoint((CGFloat)hcv_cale((int)lround(p.x)),
                       (CGFloat)hcv_cale((int)lround(p.y)));
}

/* ═══ FATBITS ══════════════════════════════════════════════════════════
 *
 * Le calque grossi huit fois, pour poser les pixels un par un.
 *
 * TOUT TIENT DANS UNE TRANSFORMATION, ET C'EST LE SEUL MOYEN QUE CE SOIT SÛR.
 *
 * Le chemin de peinture traite quatre-vingt-six points comme des coordonnées
 * de calque — le crayon, les formes, le lasso, la sélection, le texte, la
 * sélection flottante. Les convertir un à un aurait demandé de tous les
 * trouver, et celui qu'on aurait oublié aurait dessiné à côté : une écriture
 * qui se pose ailleurs que là où on l'envoie, exactement la famille de défaut
 * qu'on passe ses soirées à traquer.
 *
 * Mais ces quatre-vingt-six points viennent tous de DEUX lignes — la
 * conversion en tête de mouseDown: et de mouseDragged:. Les cinq autres
 * conversions du fichier servent aux menus surgissants, au survol, à la
 * molette et au clic de l'outil main : toutes sous l'outil BROWSE, donc hors
 * de FatBits par construction. Transformer à la frontière laisse donc les
 * quatre-vingt-six sites inchangés, et il n'y a rien à oublier.
 *
 * Le dessin, lui, passe par une seule NSAffineTransform posée autour du
 * corps de drawRect:. D'où la séparation en drawCardContent: — le corps a
 * plusieurs `return`, et un état graphique sauvé avant eux se serait perdu
 * sur l'un des chemins. Le wrapper restaure toujours, quel que soit le
 * chemin qu'a pris le corps.
 *
 * FATBITS N'EST ACTIF QU'AVEC UN OUTIL DE PEINTURE, et cela règle trois
 * choses d'un coup : l'éditeur de champ est une vraie sous-vue d'AppKit et
 * ne suivrait pas la transformation ; les menus surgissants et le survol
 * gardent leurs coordonnées ; et choisir la main fait sortir de FatBits sans
 * qu'on ait à le débrancher — l'état reste, et revient avec le crayon.
 *
 * CE QUI EST DE NOUS : le déplacement à ⌘-glisser. HyperCard fait défiler
 * FatBits tout seul quand on dessine près du bord ; je ne sais pas le
 * reproduire de mémoire avec assez de certitude pour le prétendre, et une
 * fidélité inventée vaut moins qu'un choix assumé. ⌘ est libre — aucun outil
 * de peinture ne s'en sert. */
#define HCV_FATBITS 8

static NSPoint gFatOrigine  = {0, 0};  /* coin du calque montré, en pixels de calque */
static NSPoint gFatDernier  = {0, 0};  /* dernier point peint : on centre dessus */
/* LE DÉPLACEMENT SE MESURE CONTRE CE QUI NE BOUGE PAS.
 *
 * On garde le point de la VUE et l'origine, tous deux pris À LA SAISIE. La
 * première version soustrayait le point de CALQUE courant — lequel se
 * calcule à partir de l'origine, qui venait justement de changer : chaque
 * mouvement se mesurait contre une référence qu'il avait lui-même déplacée,
 * et le glissement s'emballait. Le point de vue, lui, ne dépend de rien. */
static BOOL    gFatGlisse   = NO;      /* ⌘-glisser en cours */
static NSPoint gFatSaisiVue = {0, 0};  /* le point de la VUE à la saisie */
static NSPoint gFatSaisiOrg = {0, 0};  /* l'origine à la saisie */

/* Un outil qui DESSINE, par opposition à ceux qui manipulent des objets.
 *
 * Écrit une fois, parce que la question se pose à trois endroits — le
 * dessin, la souris, la coche du menu — et que trois copies du même « tout
 * sauf BROWSE, BUTTON et FIELD » auraient divergé au premier outil ajouté. */
static BOOL hcv_outil_peint(void)
{
    return gTool != TOOL_BROWSE && gTool != TOOL_BUTTON && gTool != TOOL_FIELD;
}

/* FatBits est-il EN VIGUEUR ? Allumé ne suffit pas : il faut aussi un outil
 * qui peigne. Les deux questions sont ici, une fois. */
static BOOL hcv_fat(void)
{
    return gFatBits && hcv_outil_peint();
}

/* Le point de la vue → le point du calque. L'identité hors FatBits, donc les
 * appelants n'ont aucun test à faire — la même règle que hcv_cale_pt. */
static NSPoint hcv_vue_vers_calque(NSPoint p)
{
    if (!hcv_fat()) return p;
    return NSMakePoint(gFatOrigine.x + p.x / HCV_FATBITS,
                       gFatOrigine.y + p.y / HCV_FATBITS);
}

/* Le morceau de calque que la vue montre. En dehors de FatBits, la vue
 * entière — ce qui est exactement ce que le dessin attendait déjà. */
static NSRect hcv_fat_zone(NSRect vue)
{
    if (!hcv_fat()) return vue;
    return NSMakeRect(gFatOrigine.x, gFatOrigine.y,
                      vue.size.width  / HCV_FATBITS,
                      vue.size.height / HCV_FATBITS);
}

/* Centrer la fenêtre grossie sur un point du calque, sans sortir du calque.
 * Appelé quand on ALLUME FatBits : sans cela on tomberait sur le coin
 * supérieur gauche, qui n'est presque jamais ce qu'on regardait. */
static void hcv_fat_centre(NSPoint sur, NSRect vue)
{
    CGFloat w = vue.size.width  / HCV_FATBITS;
    CGFloat h = vue.size.height / HCV_FATBITS;
    CGFloat x = sur.x - w / 2, y = sur.y - h / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > vue.size.width  - w) x = vue.size.width  - w;
    if (y > vue.size.height - h) y = vue.size.height - h;

    /* L'ORIGINE EST UN NOMBRE ENTIER DE PIXELS DE CALQUE, et c'est ce qui
     * fait tenir la grille sur le dessin.
     *
     * Elle ne l'était pas : « sur − moitié de la vue » tombe sur un demi, un
     * quart, ce qu'on veut. Un pixel de calque L s'affiche en (L − origine)
     * × 8 ; avec une origine fractionnaire il tombe entre deux multiples de
     * huit, alors que les traits de la grille, eux, sont posés SUR les
     * multiples de huit. Les deux glissaient donc l'un par rapport à l'autre,
     * d'une fraction de case — visible à l'œil dès qu'on dessine.
     *
     * Arrondir ici et pas au moment de tracer : c'est l'origine qui doit être
     * entière, puisque c'est elle que partagent le dessin et la grille.
     * Arrondir à l'affichage aurait recalé les traits sur eux-mêmes et laissé
     * le décalage où il était.
     *
     * Après l'arrondi les bornes tiennent encore : les deux extrémités du
     * domaine, 0 et (taille − w), sont des multiples de huit dès que la carte
     * l'est, et floor ne fait que rapprocher de zéro. */
    gFatOrigine = NSMakePoint(floor(x), floor(y));
}

static NSRect compute_shape_rect(NSPoint start, NSPoint end, BOOL centered) {
    if (centered) {
        CGFloat dx = fabs(end.x - start.x);
        CGFloat dy = fabs(end.y - start.y);
        return NSMakeRect(start.x - dx, start.y - dy, dx * 2, dy * 2);
    } else {
        return NSMakeRect(MIN(start.x, end.x), MIN(start.y, end.y),
                          fabs(end.x - start.x), fabs(end.y - start.y));
    }
}


/* La couleur du nom d'un bouton.
 *
 * Un bouton désactivé écrit en gris, comme les contrôles grisés du Toolbox que
 * HyperCard a repris : c'est le seul signe visible qu'il est inerte, puisqu'il
 * garde sa place et son cadre. Le gris l'emporte sur l'allumage — un bouton
 * désactivé n'a de toute façon aucune raison d'être allumé. */
static NSColor *btn_label_color(Object *o, NSColor *normale) {
    if (o->type == OBJ_BUTTON && !o->enabled)
        return [NSColor colorWithWhite:0.55 alpha:1.0];
    return normale;
}

static void draw_btn_label(Object *o, NSString *s, NSRect r, BOOL on, CGFloat defSize) {
    if (!o->showname) return;
    CGFloat fs = o->textsize > 0 ? o->textsize : defSize;
    NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
    [ps setAlignment:NSTextAlignmentCenter];

    NSMutableDictionary *attrs =
        [obj_attrs(o, defSize, btn_label_color(o, on ? [NSColor whiteColor]
                                                     : [NSColor blackColor])) mutableCopy];
    attrs[NSParagraphStyleAttributeName] = ps;

    NSRect tr = NSInsetRect(r, 4, 0);
    tr.origin.y += (r.size.height - fs * 1.2) / 2;
    [s drawInRect:tr withAttributes:attrs];
}

static void draw_edit_outline(NSRect r) {
    if (gTool != TOOL_BUTTON && gTool != TOOL_FIELD) return;
    [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
    NSBezierPath *outline = [NSBezierPath bezierPathWithRect:r];
    [outline setLineWidth:1];
    CGFloat dash[] = {3, 2};
    [outline setLineDash:dash count:2 phase:0];
    [outline stroke];
}

static void draw_btn_frame(Object *o, NSRect r, BOOL on) {
    const char *st = o->style ? o->style : "rectangle";

    if (strcmp(st, "transparent") == 0) {
        /* Noir franc, et non un voile gris : HyperCard INVERSAIT la zone d'un
         * bouton transparent allumé, le noir passant au blanc et
         * réciproquement. Le reste du code suit déjà cette logique — l'icône
         * et le nom se dessinent en blanc quand `on`.
         *
         * Mais un bouton à ICÔNE ne s'inverse pas du tout par son fond : seule
         * l'encre de l'icône passe au blanc, ce dont draw_part se charge. On
         * ne touche donc à rien ici quand une icône est posée. */
        if (on && o->icon == 0) {
            [[NSColor blackColor] setFill];
            NSRectFill(r);
        }
        return;
    }
    if (strcmp(st, "shadow") == 0) {
        NSRect body = NSMakeRect(r.origin.x, r.origin.y,
                                 r.size.width - 3, r.size.height - 3);
        NSRect sh   = NSMakeRect(r.origin.x + 3, r.origin.y + 3,
                                 r.size.width - 3, r.size.height - 3);
        [[NSColor blackColor] setFill];
        NSRectFill(sh);
        [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
        NSRectFill(body);
        [[NSColor blackColor] setStroke];
        NSBezierPath *bp = [NSBezierPath bezierPathWithRect:NSInsetRect(body, 0.5, 0.5)];
        [bp setLineWidth:1];
        [bp stroke];
        return;
    }
    if (strcmp(st, "roundRect") == 0 || strcmp(st, "roundrect") == 0) {
        NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 0.5, 0.5)
                                                          xRadius:8 yRadius:8];
        [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
        [p fill];
        [[NSColor blackColor] setStroke];
        [p setLineWidth:1];
        [p stroke];
        return;
    }
    if (strcmp(st, "oval") == 0) {
        NSBezierPath *p = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(r, 0.5, 0.5)];
        [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
        [p fill];
        [[NSColor blackColor] setStroke];
        [p setLineWidth:1];
        [p stroke];
        return;
    }
    if (strcmp(st, "standard") == 0 || strcmp(st, "default") == 0) {
        NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 2.5, 2.5)
                                                          xRadius:6 yRadius:6];
        [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
        [p fill];
        [[NSColor blackColor] setStroke];
        [p setLineWidth:1];
        [p stroke];
        if (strcmp(st, "default") == 0) {
            NSBezierPath *o2 = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 1.5, 1.5)
                                                               xRadius:9 yRadius:9];
            [o2 setLineWidth:3];
            [o2 stroke];
        }
        return;
    }
    if (strcmp(st, "opaque") == 0) {
        [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
        NSRectFill(r);
        return;
    }
    [(on ? [NSColor blackColor] : [NSColor whiteColor]) setFill];
    NSRectFill(r);
    [[NSColor blackColor] setFill];
    NSFrameRect(r);
}

static void close_popup_menu(void) {
    [gPopupFlashTimer invalidate];
    gPopupFlashTimer = nil;
    gPopupTarget = NULL;
    gPopupItems = nil;
    gPopupItemLines = nil;
    gPopupRect = NSZeroRect;
    gPopupRowHeight = 0;
    gPopupKeyboardRow = -1;
    gPopupChosenRow = -1;
    gPopupFlashInverted = NO;
    gPopupFlashToggles = 0;
}

static BOOL popup_row_is_enabled(NSInteger row) {
    return row >= 0 && row < (NSInteger)gPopupItems.count;
}

static NSInteger popup_row_at_point(NSPoint p) {
    if (!NSPointInRect(p, gPopupRect) || gPopupRowHeight <= 0) return -1;
    NSInteger row = (NSInteger)floor((p.y - gPopupRect.origin.y - 1) /
                                     gPopupRowHeight);
    return popup_row_is_enabled(row) ? row : -1;
}

/* LARGEUR DE LA ZONE DE TITRE D'UN BOUTON POPUP.
 *
 * HyperCard écrit le titre d'un popup HORS du cadre, à gauche, et la boîte
 * encadrée n'occupe que le reste de la largeur ; c'est ce partage que sa
 * propriété « titleWidth » règle. Ici, le titre est le NOM du bouton et son
 * apparition suit « showName » — le réglage existait déjà et ne servait à
 * rien sur un popup, puisque le nom ne s'affichait jamais.
 *
 * La largeur se MESURE sur le texte plutôt que de se stocker. Une valeur
 * stockée serait une seconde source de vérité que le dessin devrait obéir, et
 * il faudrait la persister, la régler à la main, et la reprendre à chaque
 * changement de police. Mesurer ne coûte rien et ne peut pas se désaccorder.
 *
 * Ce calcul est partagé par le dessin et par la position du menu déroulant :
 * les deux doivent tomber au même endroit, sinon le menu s'ouvre décalé de la
 * boîte qu'on vient de cliquer. */
static CGFloat popup_title_width(Object *o)
{
    if (!o || o->type != OBJ_BUTTON) return 0;
    const char *st = o->style ? o->style : "rectangle";
    if (strcmp(st, "popup") != 0) return 0;
    if (!o->showname || !o->name || !*o->name) return 0;

    NSDictionary *attrs = obj_attrs(o, 12, nil);
    NSString *nom = [NSString stringWithUTF8String:o->name];
    CGFloat w = ceil([nom sizeWithAttributes:attrs].width) + 10;

    /* Il doit RESTER de quoi voir l'article choisi et la flèche : un nom plus
     * long que le bouton mangerait la boîte entière, et l'on cliquerait sur un
     * popup dont on ne voit plus la sélection. Quarante-quatre points, c'est
     * la flèche plus de quoi lire deux ou trois caractères. */
    CGFloat place = o->w - 44;
    if (place < 0) place = 0;
    return w > place ? place : w;
}

static void open_popup_menu(Object *o, HCView *view) {
    if (!o->contents || !*o->contents) return;
    NSArray<NSString *> *raw = [[NSString stringWithUTF8String:o->contents]
                                componentsSeparatedByString:@"\n"];
    NSMutableArray<NSString *> *items = [NSMutableArray array];
    NSMutableArray<NSNumber *> *lines = [NSMutableArray array];
    for (NSUInteger i = 0; i < raw.count; i++) {
        if (raw[i].length == 0) continue;
        [items addObject:raw[i]];
        [lines addObject:@(i + 1)];
    }
    if (items.count == 0) return;

    NSDictionary *attrs = obj_attrs(o, 12, nil);
    /* Le menu s'ouvre sous la BOÎTE, pas sous le rectangle entier : quand un
     * titre occupe la gauche, s'aligner sur o->x ouvrirait le menu décalé de
     * la boîte qu'on vient de cliquer. */
    CGFloat tw = popup_title_width(o);
    CGFloat width = o->w - tw;
    for (NSString *item in items)
        width = MAX(width, ceil([item sizeWithAttributes:attrs].width) + 24);
    gPopupRowHeight = MAX(16, ceil([@"Ag" sizeWithAttributes:attrs].height) + 4);
    CGFloat height = gPopupRowHeight * items.count;
    NSRect bounds = view.bounds;
    CGFloat x = MIN(MAX(0, o->x + tw), MAX(0, bounds.size.width - width - 3));
    CGFloat y = o->y + o->h;
    if (y + height + 3 > bounds.size.height) y = MAX(0, o->y - height);

    gPopupTarget = o;
    gPopupItems = items.copy;
    gPopupItemLines = lines.copy;
    gPopupRect = NSMakeRect(x, y, width, height);
    gPopupOpenedAt = [NSDate timeIntervalSinceReferenceDate];
    NSUInteger selected = [gPopupItemLines indexOfObject:@(o->selectedline)];
    gPopupKeyboardRow = selected == NSNotFound ? 0 : (NSInteger)selected;
    [view.window makeFirstResponder:view];
    [view.window setAcceptsMouseMovedEvents:YES];
    [view setNeedsDisplay:YES];
}

static void draw_popup_menu(void) {
    if (!gPopupTarget || gPopupItems.count == 0) return;
    [[NSColor blackColor] setFill];
    NSRectFill(NSOffsetRect(gPopupRect, 3, 3));
    [[NSColor whiteColor] setFill];
    NSRectFill(gPopupRect);
    [[NSColor blackColor] setStroke];
    [[NSBezierPath bezierPathWithRect:NSInsetRect(gPopupRect, .5, .5)] stroke];

    NSDictionary *attrs = obj_attrs(gPopupTarget, 12, nil);
    for (NSUInteger i = 0; i < gPopupItems.count; i++) {
        NSRect row = NSMakeRect(gPopupRect.origin.x + 1,
                                gPopupRect.origin.y + i * gPopupRowHeight + 1,
                                gPopupRect.size.width - 2, gPopupRowHeight);
        BOOL selected = (NSInteger)i == gPopupKeyboardRow && !gPopupFlashInverted;
        if (selected) { [[NSColor blackColor] setFill]; NSRectFill(row); }
        NSMutableDictionary *rowAttrs = attrs.mutableCopy;
        rowAttrs[NSForegroundColorAttributeName] = selected ? NSColor.whiteColor : NSColor.blackColor;
        NSString *title = gPopupItems[i];
        CGFloat textHeight = [title sizeWithAttributes:rowAttrs].height;
        [title drawAtPoint:NSMakePoint(row.origin.x + 14, row.origin.y + floor((row.size.height - textHeight) / 2))
            withAttributes:rowAttrs];
        if (gPopupItemLines[i].integerValue == gPopupTarget->selectedline) {
            [(selected ? NSColor.whiteColor : NSColor.blackColor) setStroke];
            NSBezierPath *check = NSBezierPath.bezierPath;
            CGFloat cx = row.origin.x + 2, cy = row.origin.y + row.size.height / 2;
            [check moveToPoint:NSMakePoint(cx, cy)];
            [check lineToPoint:NSMakePoint(cx + 2, cy + 3)];
            [check lineToPoint:NSMakePoint(cx + 6, cy - 3)];
            check.lineWidth = 1.5;
            [check stroke];
        }
    }
}

static void choose_popup_row(HCView *view, NSInteger row) {
    if (!gPopupTarget || !popup_row_is_enabled(row)) return;
    gPopupTarget->selectedline = gPopupItemLines[row].intValue;
    hc_send(gPopupTarget, "mouseUp");
    close_popup_menu();
    [view setNeedsDisplay:YES];
}

static void flash_popup_selection(HCView *view, NSInteger row) {
    if (!gPopupTarget || !popup_row_is_enabled(row)) return;
    gPopupKeyboardRow = row;
    gPopupChosenRow = row;
    gPopupFlashInverted = NO;
    gPopupFlashToggles = 6;
    [gPopupFlashTimer invalidate];
    gPopupFlashTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 15.0
                                                         target:view
                                                       selector:@selector(popupFlashTick:)
                                                       userInfo:nil repeats:YES];
    [view setNeedsDisplay:YES];
}

static void draw_part(Object *o) {
    if (!o->visible) return;

    NSRect r = NSMakeRect(o->x, o->y, o->w, o->h);

    if (o->type == OBJ_BUTTON) {
        const char *st = o->style ? o->style : "rectangle";
        BOOL isCheck  = (strcmp(st, "checkBox") == 0    || strcmp(st, "checkbox") == 0);
        BOOL isRadio  = (strcmp(st, "radioButton") == 0 || strcmp(st, "radiobutton") == 0);
        BOOL isTransp = (strcmp(st, "transparent") == 0);
        BOOL isPopup  = (strcmp(st, "popup") == 0);

        const char *nm = o->name ? o->name : "";
        NSString *s = [NSString stringWithUTF8String:nm];
        /* Pour un bouton de fond non partagé, l'allumage vit dans la carte :
         * lire o->hilite donnerait le même état sur toutes les cartes du fond. */
        BOOL on = hc_hilite_of(o, hc_current_card());

        const HCIcon *ic = (o->icon ? hcicon_find(o->icon) : NULL);

        if (ic) {
            /* L'icône se centre dans la place qui lui revient.
             *
             * Quand le nom s'affiche, il occupe la bande du bas : l'icône se
             * centre alors dans les 36 points du haut, ce qui la remet à
             * o->y + 2. Sans nom, elle se centre dans TOUT le bouton — la
             * caler à o->y + 2 la décalait de deux points vers le bas sur un
             * bouton de 32 ou 34 de haut, soit la taille habituelle d'un
             * bouton à icône. */
            BOOL withName = (o->showname && o->h > 36);
            CGFloat iarea = withName ? 36 : o->h;
            CGFloat iy    = o->y + floor((iarea - 32) / 2.0);
            NSRect ir = NSMakeRect(floor(o->x + (o->w - 32)/2.0), floor(iy), 32, 32);

            draw_btn_frame(o, r, on);

            /* Bouton transparent allumé : l'inversion se limite à la FORME de
             * l'icône — l'encre passe au blanc, le blanc enclos passe au noir,
             * et la carte autour n'est pas touchée. Ni carré noir, ni icône
             * qui disparaît sur fond blanc. */
            if (on && isTransp) {
                hcicon_draw_inverted(ic, ir, 1.0);
            } else {
                [(on ? [NSColor whiteColor] : [NSColor blackColor]) setFill];
                hcicon_draw(ic, ir, 1.0);
            }
            draw_edit_outline(r);

            if (withName) {
                NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                [ps setAlignment:NSTextAlignmentCenter];
                NSMutableDictionary *bat = [obj_attrs(o, 11,
                    btn_label_color(o, on ? [NSColor whiteColor]
                                          : [NSColor blackColor])) mutableCopy];
                bat[NSParagraphStyleAttributeName] = ps;
                /* Le texte ne suit plus iy : la bande du bas est fixe, elle
                 * commence sous les 36 points réservés à l'icône. */
                NSRect btr = NSMakeRect(o->x, o->y + 36, o->w, o->h - 36);

                /* Bouton transparent allumé : le nom s'inverse lui aussi.
                 *
                 * Pour du texte, inverser ne peut vouloir dire qu'une chose —
                 * fond noir, lettres blanches. On noircit donc DERRIÈRE, mais
                 * serré sur le texte plutôt qu'en bande large sur toute la
                 * largeur du bouton, dans le même esprit que l'icône dont
                 * l'inversion épouse la forme.
                 *
                 * Les autres styles ont déjà noirci tout leur corps dans
                 * draw_btn_frame : il n'y a rien à poser sous le texte. */
                if (on && isTransp) {
                    NSSize sz = [s sizeWithAttributes:bat];
                    NSRect tb = NSMakeRect(
                        floor(btr.origin.x + (btr.size.width - sz.width) / 2.0) - 1,
                        btr.origin.y,
                        ceil(sz.width) + 2,
                        ceil(sz.height));
                    [[NSColor blackColor] setFill];
                    NSRectFill(tb);
                }

                [s drawInRect:btr withAttributes:bat];
            }
        }
        else if (isCheck || isRadio) {
            CGFloat box = 14;
            CGFloat cy = o->y + o->h/2.0 - box/2.0;
            NSRect mark = NSMakeRect(o->x + 2, cy, box, box);

            [[NSColor whiteColor] setFill];
            [[NSColor blackColor] setStroke];

            if (isRadio) {
                NSBezierPath *circle = [NSBezierPath bezierPathWithOvalInRect:mark];
                [circle fill];
                [circle stroke];
                if (on) {
                    NSRect dot = NSInsetRect(mark, 4, 4);
                    [[NSColor blackColor] setFill];
                    [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
                }
            } else {
                [[NSColor whiteColor] setFill];
                NSRectFill(mark);
                [[NSColor blackColor] setStroke];
                NSBezierPath *bp = [NSBezierPath bezierPathWithRect:mark];
                [bp setLineWidth:1];
                [bp stroke];
                if (on) {
                    NSBezierPath *x = [NSBezierPath bezierPath];
                    [x moveToPoint:NSMakePoint(mark.origin.x+2, mark.origin.y+2)];
                    [x lineToPoint:NSMakePoint(mark.origin.x+box-2, mark.origin.y+box-2)];
                    [x moveToPoint:NSMakePoint(mark.origin.x+box-2, mark.origin.y+2)];
                    [x lineToPoint:NSMakePoint(mark.origin.x+2, mark.origin.y+box-2)];
                    [x setLineWidth:1.5];
                    [x stroke];
                }
            }

            if (o->showname) {
                CGFloat fs = o->textsize > 0 ? o->textsize : 13;
                [s drawAtPoint:NSMakePoint(o->x + box + 8, o->y + o->h/2 - fs*0.6)
                withAttributes:obj_attrs(o, 13, btn_label_color(o, nil))];
            }
        }
        else if (isTransp) {
            draw_btn_frame(o, r, on);
            draw_edit_outline(r);
            if (o->showname) {
                CGFloat fs = o->textsize > 0 ? o->textsize : 16;
                NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                [ps setAlignment:NSTextAlignmentCenter];
                NSMutableDictionary *bat = [obj_attrs(o, 16,
                    btn_label_color(o, [NSColor blackColor])) mutableCopy];
                bat[NSParagraphStyleAttributeName] = ps;
                NSRect btr = NSInsetRect(r, 2, 0);
                btr.origin.y += (r.size.height - fs * 1.2) / 2;
                [s drawInRect:btr withAttributes:bat];
            }
        }
        else if (isPopup) {
            /* LE TITRE, À GAUCHE ET HORS DU CADRE.
             *
             * C'est la disposition d'HyperCard : le nom du bouton s'écrit sur
             * le fond de la carte, et la boîte encadrée n'occupe que le reste.
             * Il apparaît quand « showName » est allumé — le réglage existait
             * déjà et ne servait à rien sur un popup, puisque le nom ne
             * s'affichait jamais, ni à droite ni à gauche.
             *
             * Sans titre (tw = 0), la boîte reprend toute la largeur : le
             * bouton est alors dessiné exactement comme avant. */
            CGFloat tw = popup_title_width(o);
            NSRect boite = NSMakeRect(r.origin.x + tw, r.origin.y,
                                      r.size.width - tw, r.size.height);
            NSRect body = NSMakeRect(boite.origin.x, boite.origin.y,
                                     boite.size.width - 3, boite.size.height - 3);
            NSRect sh   = NSMakeRect(boite.origin.x + 3, boite.origin.y + 3,
                                     boite.size.width - 3, boite.size.height - 3);
            [[NSColor blackColor] setFill];
            NSRectFill(sh);
            [[NSColor whiteColor] setFill];
            NSRectFill(body);
            [[NSColor blackColor] setStroke];
            NSBezierPath *bp = [NSBezierPath bezierPathWithRect:NSInsetRect(body, 0.5, 0.5)];
            [bp setLineWidth:1];
            [bp stroke];

            CGFloat cx = body.origin.x + body.size.width - 12;
            CGFloat cy = body.origin.y + body.size.height/2.0;
            [[NSColor blackColor] setFill];
            NSBezierPath *ar = [NSBezierPath bezierPath];
            [ar moveToPoint:NSMakePoint(cx - 4, cy - 2)];
            [ar lineToPoint:NSMakePoint(cx + 4, cy - 2)];
            [ar lineToPoint:NSMakePoint(cx,     cy + 3)];
            [ar closePath];
            [ar fill];

            /* Sans contenu, l'étiquette de la boîte retombe sur le nom du
             * bouton. Avec un titre, ce nom serait alors écrit DEUX FOIS —
             * une fois à gauche, une fois dans la boîte. Le titre le porte
             * déjà : la boîte reste vide en attendant un menu. */
            NSString *label = (tw > 0) ? @"" : s;
            if (o->contents && *o->contents) {
                NSArray *lines = [[NSString stringWithUTF8String:o->contents]
                                  componentsSeparatedByString:@"\n"];
                int sel = o->selectedline > 0 ? o->selectedline : 1;
                if (sel <= (int)[lines count] && [lines[sel-1] length] > 0)
                    label = lines[sel-1];
            }
            CGFloat fs = o->textsize > 0 ? o->textsize : 12;
            NSDictionary *pat = obj_attrs(o, 12, btn_label_color(o, nil));
            CGFloat ligne = body.origin.y + (body.size.height - fs*1.3)/2;

            if (tw > 0) {
                /* Le titre lui-même, sur le fond de la carte. Il garde la
                 * police et la taille du bouton : c'est le même objet, il
                 * n'aurait pas de raison de changer d'écriture en passant du
                 * cadre au fond. */
                [s drawAtPoint:NSMakePoint(r.origin.x + 2, ligne)
                withAttributes:pat];
            }

            /* L'ARTICLE CHOISI EST DÉTOURÉ. Il était écrit sans limite et
             * passait sous la flèche quand il était long — deux traits noirs
             * qui se chevauchent, et l'on ne lit plus ni l'un ni l'autre. La
             * boîte moins la flèche, c'est la place qui lui revient. */
            [NSGraphicsContext saveGraphicsState];
            NSRectClip(NSMakeRect(body.origin.x + 4, body.origin.y,
                                  body.size.width - 22, body.size.height));
            [label drawAtPoint:NSMakePoint(body.origin.x + 6, ligne)
                withAttributes:pat];
            [NSGraphicsContext restoreGraphicsState];
        }
        else {
            draw_btn_frame(o, r, on);
            NSRect lr = r;
            if (strcmp(st, "shadow") == 0)
                lr = NSMakeRect(r.origin.x, r.origin.y,
                                r.size.width - 3, r.size.height - 3);
            draw_btn_label(o, s, lr, on, 13);
        }

        if (gTool == TOOL_BUTTON) {
            draw_edit_outline(r);
        }
    }
    else if (o->type == OBJ_FIELD) {
        const char *st = o->style ? o->style : "rectangle";
        BOOL isTransp = (strcmp(st, "transparent") == 0);
        BOOL isOpaque = (strcmp(st, "opaque") == 0);
        BOOL isShadow = (strcmp(st, "shadow") == 0);
        BOOL isScroll = (strcmp(st, "scrolling") == 0);

        NSRect body = r;

        if (isTransp) {
        }
        else if (isOpaque) {
            [[NSColor whiteColor] setFill];
            NSRectFill(r);
        }
        else if (isShadow) {
            body = NSMakeRect(r.origin.x, r.origin.y,
                              r.size.width - 3, r.size.height - 3);
            NSRect sh = NSMakeRect(r.origin.x + 3, r.origin.y + 3,
                                   r.size.width - 3, r.size.height - 3);
            [[NSColor blackColor] setFill];
            NSRectFill(sh);
            [[NSColor whiteColor] setFill];
            NSRectFill(body);
            [[NSColor blackColor] setStroke];
            NSBezierPath *bp = [NSBezierPath bezierPathWithRect:NSInsetRect(body, 0.5, 0.5)];
            [bp setLineWidth:1];
            [bp stroke];
        }
        else if (isScroll) {
            field_clamp_scroll(o);

            [[NSColor whiteColor] setFill];
            NSRectFill(r);
            [[NSColor blackColor] setFill];
            NSFrameRect(r);

            CGFloat bw = 16;
            NSRect bar = NSMakeRect(r.origin.x + r.size.width - bw, r.origin.y,
                                    bw, r.size.height);
            [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
            NSRectFill(bar);
            [[NSColor blackColor] setFill];
            NSFrameRect(bar);

            [[NSColor blackColor] setFill];
            CGFloat cx = bar.origin.x + bw/2;
            NSBezierPath *up = [NSBezierPath bezierPath];
            [up moveToPoint:NSMakePoint(cx - 4, bar.origin.y + 11)];
            [up lineToPoint:NSMakePoint(cx + 4, bar.origin.y + 11)];
            [up lineToPoint:NSMakePoint(cx,     bar.origin.y + 5)];
            [up closePath]; [up fill];
            NSBezierPath *dn = [NSBezierPath bezierPath];
            CGFloat by = bar.origin.y + bar.size.height;
            [dn moveToPoint:NSMakePoint(cx - 4, by - 11)];
            [dn lineToPoint:NSMakePoint(cx + 4, by - 11)];
            [dn lineToPoint:NSMakePoint(cx,     by - 5)];
            [dn closePath]; [dn fill];

            NSRect tr0 = field_text_rect(o);
            CGFloat th = field_text_height(o, tr0);
            CGFloat vh = tr0.size.height;
            CGFloat gy = bar.origin.y + 16;
            CGFloat gh = bar.size.height - 32;
            if (th > vh && gh > 8) {
                CGFloat kh = gh * (vh / th);
                if (kh < 12) kh = 12;
                if (kh > gh) kh = gh;
                CGFloat maxs = th - vh;
                CGFloat pos = (maxs > 0) ? (o->scroll / maxs) : 0;
                if (pos < 0) pos = 0;
                if (pos > 1) pos = 1;
                NSRect knob = NSMakeRect(bar.origin.x + 1, gy + pos * (gh - kh),
                                         bw - 2, kh);
                [[NSColor colorWithWhite:0.75 alpha:1.0] setFill];
                NSRectFill(knob);
                [[NSColor blackColor] setFill];
                NSFrameRect(knob);
            }

            body = NSMakeRect(r.origin.x, r.origin.y, r.size.width - bw, r.size.height);
        }
        else {
            [[NSColor whiteColor] setFill];
            NSRectFill(r);
            [[NSColor blackColor] setFill];
            NSFrameRect(r);
        }

        NSDictionary *at = obj_attrs(o, 12, [NSColor blackColor]);
        NSRect tr = field_text_rect(o);

        if (o->show_lines) {
            CGFloat lh;
            if (o->fixed_lh) {
                lh = hc_text_height(o);
            } else {
                NSFont *fb = obj_base_font(o, 12);
                NSAttributedString *une =
                    [[NSAttributedString alloc] initWithString:@"Mg"
                                                    attributes:@{NSFontAttributeName: fb}];
                lh = [une boundingRectWithSize:NSMakeSize(10000, CGFLOAT_MAX)
                                       options:NSStringDrawingUsesLineFragmentOrigin
                      ].size.height;
            }
            if (lh < 4) lh = 12;
            [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
            for (CGFloat y = tr.origin.y + lh - o->scroll;
                 y < body.origin.y + body.size.height - 2; y += lh) {
                if (y < body.origin.y) continue;
                NSBezierPath *ln = [NSBezierPath bezierPath];
                [ln moveToPoint:NSMakePoint(tr.origin.x, floor(y) + 0.5)];
                [ln lineToPoint:NSMakePoint(tr.origin.x + tr.size.width, floor(y) + 0.5)];
                [ln setLineWidth:1];
                [ln stroke];
            }
        }

        const char *tx = hc_field_text(o);
        NSString *s = [NSString stringWithUTF8String:tx];

        NSAttributedString *as = field_attr_string(o, s, at);

        if (o != gEditingField) {
            /* On ne trace QUE la plage visible.
             *
             * -drawInRect: mettait en page tout le texte, quarante-huit
             * kilo-octets pour une vingtaine de lignes affichées : mesuré à
             * plus de quatre cents millisecondes par image sur un champ
             * chargé. NSLayoutManager, lui, compose paresseusement — en ne
             * lui demandant que les glyphes du rectangle visible, il ne
             * touchera jamais aux lignes du dessous. La mise en page est en
             * plus conservée d'une image à l'autre par field_layout.
             *
             * Le décalage vertical porte le défilement : le conteneur a son
             * origine au début du texte, et l'on pose cette origine plus haut
             * que le cadre pour montrer le passage voulu. */
            NSTextContainer *tc = nil;
            NSLayoutManager *lm = field_layout(o, s, at, tr.size.width, &tc);

            CGFloat dy = isScroll ? o->scroll : 0;
            NSRect visible = NSMakeRect(0, dy, tr.size.width, tr.size.height);
            NSRange plage = [lm glyphRangeForBoundingRect:visible
                                          inTextContainer:tc];
            NSPoint origine = NSMakePoint(tr.origin.x, tr.origin.y - dy);

            [NSGraphicsContext saveGraphicsState];
            NSRectClip(tr);
            [lm drawBackgroundForGlyphRange:plage atPoint:origine];
            [lm drawGlyphsForGlyphRange:plage atPoint:origine];
            [NSGraphicsContext restoreGraphicsState];
        }

        int fstart = 0, flen = 0;
        NSUInteger fdeb = 0, flg = 0;
        if (o != gEditingField &&
            hc_found_range(o, &fstart, &flen) && flen > 0) {
            /* OCTETS -> UTF-16, comme pour la sélection.
             *
             * hc_found_range rend le décalage du motif EN OCTETS — c'est ce
             * que le noyau calcule, « hit - tx ». On le passait tel quel à
             * glyphRangeForCharacterRange:, qui compte en unités UTF-16 : la
             * boîte noire glissait d'un cran par octet supplémentaire, donc
             * d'autant que le champ contient d'accents, de guillemets « » ou
             * de tirets longs AVANT le motif. Elle coupait les mots.
             *
             * Mesuré sur un texte français ordinaire : le motif commençait à
             * l'octet 52 et au caractère 47 — cinq de décalage, et cinq
             * lettres avalées au début du surlignage.
             *
             * La conversion existait déjà, six cents lignes plus haut, pour
             * « select char N of field X », avec un commentaire décrivant le
             * même défaut. Ce chemin-ci ne l'avait jamais reçue : un jumeau
             * corrigé, l'autre oublié. Le noyau, lui, est juste — « the
             * foundChunk » rend bien un rang de CARACTÈRE.
             *
             * Le bornage se fait APRÈS la conversion : comparer un décalage
             * en octets à [s length], qui est en UTF-16, laissait passer des
             * plages hors du texte sur un champ très accentué. */
            const char *tx = hc_field_text(o);
            NSUInteger u0 = utf16_from_byte(tx, fstart);
            NSUInteger u1 = utf16_from_byte(tx, fstart + flen);
            NSUInteger n  = [s length];
            if (u0 > n) u0 = n;
            if (u1 > n) u1 = n;
            fdeb = u0;
            flg  = u1 > u0 ? u1 - u0 : 0;
        }
        if (flg > 0) {

            /* La mise en page du TRACÉ, comme pour le test de clic.
             *
             * Celle qu'on montait ici gardait l'interligne de police par
             * défaut, alors que le tracé l'a désactivé : la boîte noire du
             * texte trouvé se décalait donc vers le bas à mesure qu'on
             * descendait dans le champ. Personne ne l'avait encore signalé,
             * mais c'est le même défaut que celui de la ligne surlignée, et
             * il se corrige au même endroit. */
            NSTextContainer *tc = nil;
            NSLayoutManager *lm = field_layout(o, s, at, tr.size.width, &tc);

            NSRange glyphs = [lm glyphRangeForCharacterRange:NSMakeRange(fdeb, flg)
                                       actualCharacterRange:NULL];
            NSRect box = [lm boundingRectForGlyphRange:glyphs inTextContainer:tc];
            box.origin.x += tr.origin.x;
            box.origin.y += tr.origin.y - (isScroll ? o->scroll : 0);

            [NSGraphicsContext saveGraphicsState];
            NSRectClip(tr);
            [[NSColor blackColor] setFill];
            NSRectFill(box);

            NSMutableAttributedString *sub =
                [[as attributedSubstringFromRange:NSMakeRange(fdeb, flg)] mutableCopy];
            [sub addAttribute:NSForegroundColorAttributeName
                        value:[NSColor whiteColor]
                        range:NSMakeRange(0, [sub length])];
            [sub drawAtPoint:box.origin];
            [NSGraphicsContext restoreGraphicsState];
        }

        if (gTool == TOOL_FIELD || isTransp) draw_edit_outline(r);
    }
}

static int handle_at(Object *o, NSPoint p) {
    if (!o) return 0;
    CGFloat s = 8;
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

/* Un bouton désactivé est-il transparent au clic ?
 *
 * Seulement en mode Browse. Avec l'outil Bouton on doit pouvoir le
 * sélectionner, le déplacer et rouvrir son Info — sans quoi on ne pourrait
 * plus jamais le réactiver autrement que par script. */
static BOOL part_inerte(Object *o) {
    return (gTool == TOOL_BROWSE && o->type == OBJ_BUTTON && !o->enabled);
}

/* Le part le plus haut d'un calque sous le point, ou NULL. */
static Object *part_at_layer(Object *layer, NSPoint p) {
    if (!layer) return NULL;
    for (int i = layer->nparts - 1; i >= 0; i--) {
        Object *o = layer->parts[i];
        if (o->visible && !part_inerte(o) &&
            p.x >= o->x && p.x <= o->x + o->w &&
            p.y >= o->y && p.y <= o->y + o->h)
            return o;
    }
    return NULL;
}

/* ═══ Ce qu'on peut attraper, et depuis où ═════════════════════════════
 *
 * Les outils Bouton et Champ atteignent LES DEUX CALQUES, que l'on soit en
 * édition de carte ou de fond. C'est ce que faisait la version précédente
 * pour le seul outil Browse ; les outils d'objet, eux, ne voyaient que le
 * calque courant.
 *
 * Ce n'était pas tenable, parce que le DESSIN, lui, ne faisait pas cette
 * différence : draw_edit_outline entoure de pointillés tous les boutons
 * visibles dès que l'outil Bouton est choisi, ceux du fond compris. On
 * montrait donc une poignée d'objets présentés comme modifiables, dont la
 * moitié ne répondait ni au clic ni au double-clic. Un état montré faux est
 * pire qu'un état non montré : l'utilisateur cherche l'erreur chez lui.
 *
 * L'ordre reste celui de l'affichage — la carte d'abord, le fond ensuite : un
 * bouton de carte posé par-dessus un bouton de fond garde la priorité, comme
 * au clic en mode Browse. Ce qui change, c'est qu'on ne s'arrête plus là.
 *
 * ET DANS L'AUTRE SENS AUSSI. En édition de fond, les mêmes outils atteignent
 * les objets de la CARTE. C'est ce que faisait HyperCard : ⌘B commande le
 * calque de PEINTURE et l'endroit où atterrissent les objets neufs, pas ce
 * que les outils d'objet ont le droit de toucher.
 *
 * Ce qui oblige à dessiner ce qu'on attrape — voir couche_carte_visible(),
 * qui répond pour les deux et que drawRect: consulte aussi. Une réponse par
 * fonction aurait donné, un jour ou l'autre, un bouton cliquable et invisible
 * ou l'inverse, sans que rien ne le signale.
 *
 * Le calque du part attrapé reste visible : la sélection change de couleur
 * (voir drawRect:), et l'Info dit « Background Button ». Déplacer un bouton
 * de fond le déplace sur TOUTES les cartes de ce fond — l'action est la même
 * qu'avant, il fallait seulement qu'on puisse la voir venir. */

/* Le calque CARTE participe-t-il — au dessin comme au clic ?
 *
 * Non en édition de fond : c'est tout le sens de ⌘B, on veut voir le fond
 * seul. Sauf avec les outils d'objet, qui travaillent sur les deux calques à
 * la fois et doivent donc les montrer tous les deux.
 *
 * Une seule fonction pour les deux questions, parce que ce sont les deux
 * moitiés d'une seule : on n'attrape que ce qui est dessiné, et on dessine
 * tout ce qu'on peut attraper. Les séparer, c'était se réserver un bouton
 * fantôme. */
static BOOL couche_carte_visible(void) {
    return !gEditBackground || gTool == TOOL_BUTTON || gTool == TOOL_FIELD;
}

static Object *part_at(Object *card, NSPoint p) {
    if (!card) return NULL;

    if (couche_carte_visible()) {
        Object *o = part_at_layer(card, p);
        if (o) return o;
    }
    return part_at_layer(card->bg, p);
}
static char gDlgBuf[512];
static char gFileBuf[2048];

/* Le rappel de « save stack "X" as "Y" ». Une COPIE, comme le dit le contrat
 * dans hc_core.h : la pile en mémoire garde son adresse, et « the long name of
 * this stack » répond la même chose avant et après. */
static int cocoa_save_stack(Object *stack, const char *path) {
    if (!stack || !path || !*path) return 0;
    [gView flushPaintToKernel];
    return hc_save_copie(stack, path) == 0;
}

static const char *cocoa_answer_file(const char *prompt) {
    NSOpenPanel *p = [NSOpenPanel openPanel];
    [p setCanChooseFiles:YES];
    [p setCanChooseDirectories:NO];
    [p setAllowsMultipleSelection:NO];
    if (prompt && *prompt)
        [p setMessage:[NSString stringWithUTF8String:prompt]];
    if ([p runModal] != NSModalResponseOK) return NULL;
    NSString *chemin = [[p URL] path];
    if (!chemin) return NULL;
    snprintf(gFileBuf, sizeof gFileBuf, "%s", [chemin UTF8String]);
    return gFileBuf;
}

static const char *cocoa_ask_file(const char *prompt, const char *deflt) {
    NSSavePanel *p = [NSSavePanel savePanel];
    if (prompt && *prompt)
        [p setMessage:[NSString stringWithUTF8String:prompt]];
    if (deflt && *deflt)
        [p setNameFieldStringValue:[NSString stringWithUTF8String:deflt]];
    if ([p runModal] != NSModalResponseOK) return NULL;
    NSString *chemin = [[p URL] path];
    if (!chemin) return NULL;
    snprintf(gFileBuf, sizeof gFileBuf, "%s", [chemin UTF8String]);
    return gFileBuf;
}

static const char *cocoa_ask(const char *prompt, const char *deflt) {
    NSAlert *a = [[NSAlert alloc] init];
    [a setMessageText:[NSString stringWithUTF8String:prompt ? prompt : ""]];
    [a addButtonWithTitle:@"OK"];
    [a addButtonWithTitle:@"Annuler"];

    NSTextField *tf = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 260, 24)];
    [tf setStringValue:[NSString stringWithUTF8String:deflt ? deflt : ""]];
    [a setAccessoryView:tf];
    [[a window] setInitialFirstResponder:tf];

    if ([a runModal] != NSAlertFirstButtonReturn) return NULL;
    snprintf(gDlgBuf, sizeof gDlgBuf, "%s", [[tf stringValue] UTF8String]);
    return gDlgBuf;
}

static const char *cocoa_answer(const char *prompt, const char *b1,
                                const char *b2, const char *b3) {
    NSAlert *a = [[NSAlert alloc] init];
    [a setMessageText:[NSString stringWithUTF8String:prompt ? prompt : ""]];
    const char *order[3] = { b3, b2, b1 };
    for (int i = 0; i < 3; i++)
        if (order[i]) [a addButtonWithTitle:[NSString stringWithUTF8String:order[i]]];

    NSModalResponse rep = [a runModal];
    int idx = (int)(rep - NSAlertFirstButtonReturn);
    const char *chosen = b1;
    int k = 0;
    for (int i = 0; i < 3; i++)
        if (order[i]) { if (k == idx) { chosen = order[i]; break; } k++; }
    snprintf(gDlgBuf, sizeof gDlgBuf, "%s", chosen ? chosen : "OK");
    return gDlgBuf;
}

static void cocoa_line(HcLineKind kind, int depth, const char *text) {
    (void)depth;
    if (kind == HC_MSG && gMsgBox) {
        if (gMsgPanel && ![gMsgPanel isVisible])
            [gMsgPanel orderFront:nil];
        [gMsgBox setStringValue:[NSString stringWithUTF8String:text]];
        return;
    }
    /* Le journal reste : il garde la trace ligne par ligne, avec le contexte,
     * et c'est ce qu'on veut sous le débogueur. Ce qui manquait n'était pas
     * une trace mais un AVERTISSEMENT — voir cocoa_erreur juste dessous. */
    if (kind == HC_ERR || getenv("HC_TRACE")) NSLog(@"%s", text);
}

/* UNE ERREUR DE SCRIPT S'ANNONCE À L'UTILISATEUR, COMME DANS HYPERCARD.
 *
 * Avant : NSLog, et rien d'autre. Toutes les erreurs de script étaient
 * invisibles pour qui n'avait pas lancé l'application depuis Xcode — pas
 * seulement les nouvelles : la syntaxe, l'objet introuvable, le verbe
 * inconnu. Depuis toujours. Un script qui échouait ne faisait rien, et rien
 * ne le disait.
 *
 * Le noyau appelle ce rappel UNE FOIS, à la fin du gestionnaire le plus
 * extérieur, avec toutes les lignes d'erreur accumulées : une erreur arrête
 * le gestionnaire, donc il y a au plus un dialogue par clic. C'est ce qui
 * rend le modal supportable — brancher NSAlert sur `line` en aurait ouvert
 * trois pour une seule erreur de syntaxe.
 *
 * « Script » ouvre l'éditeur sur l'objet fautif, comme le bouton du même nom
 * dans HyperCard. Le noyau nous donne cet objet ; il peut être NULL, et le
 * bouton n'apparaît alors pas plutôt que de ne rien faire. */
static void cocoa_erreur(const char *texte, Object *objet) {
    if (!texte || !*texte) return;

    NSAlert *a = [[NSAlert alloc] init];
    [a setAlertStyle:NSAlertStyleWarning];

    /* La PREMIÈRE ligne en titre, le reste en détail. Les messages du noyau
     * commencent par le fait — « objet introuvable » — et poursuivent par le
     * contexte : l'extrait du script, la ligne, l'objet. C'est exactement la
     * division que demande une alerte. */
    NSString *tout = [NSString stringWithUTF8String:texte];
    NSRange saut = [tout rangeOfString:@"\n"];
    NSString *titre = (saut.location == NSNotFound)
                      ? tout : [tout substringToIndex:saut.location];
    NSString *detail = (saut.location == NSNotFound)
                       ? @"" : [tout substringFromIndex:saut.location + 1];

    /* Les « !! » du noyau sont une marque de journal, pas de dialogue. */
    titre = [titre stringByTrimmingCharactersInSet:
             [NSCharacterSet whitespaceCharacterSet]];
    if ([titre hasPrefix:@"!! "]) titre = [titre substringFromIndex:3];

    [a setMessageText:titre];
    if ([detail length]) [a setInformativeText:detail];

    [a addButtonWithTitle:@"OK"];
    if (objet && gView) [a addButtonWithTitle:@"Script"];

    NSModalResponse rep = [a runModal];
    if (objet && gView && rep == NSAlertSecondButtonReturn)
        [gView editScriptOf:objet];
}

static BOOL gMouseClicked = NO;

/* Le dernier curseur posé, pour que « the cursor » se relise. Une propriété
 * qu'on peut poser et pas relire est une propriété à moitié — le même défaut
 * que hcv_quelle_couleur a supprimé pour les couleurs. */
static NSString *gCursorNom = @"arrow";

/* Le pointeur est-il caché ? « set the cursor to none » le cache, et il faut
 * bien que quelqu'un sache le remontrer. Cet état vivait deux mille lignes
 * plus bas, hors de portée du changement d'outil qui doit le lever. */
static BOOL gCursorHidden = NO;

/* « set the cursor » l'emporte sur le curseur de l'outil, jusqu'au prochain
 * changement d'outil.
 *
 * Les deux se disputent le même pointeur. Sans ce drapeau, les zones de
 * curseur de la vue reposeraient le curseur de l'outil au premier mouvement
 * de souris, et « set the cursor to watch » ne durerait pas le temps d'un
 * battement de cils — alors que « the cursor » continuerait de répondre
 * « watch ». La propriété décrirait un état qui n'existe plus. */
static BOOL gCursorScripte = NO;

/* ET LE CURSEUR QU'IL A DEMANDÉ.
 *
 * Le garder ne relevait pas de l'élégance : sans lui, resetCursorRects ne
 * posait AUCUNE zone tant qu'un script tenait le curseur, et AppKit retombait
 * alors sur la flèche par défaut de la vue. Comme « set the cursor » fait
 * rebâtir les zones juste après avoir posé le curseur, la montre était
 * détruite dans l'instant même où on la posait : « set cursor to watch » ne
 * montrait jamais rien. */
static NSCursor *gCursorScripteObj = nil;

/* Pose le bon curseur sur la vue de carte.
 *
 * On passe par les ZONES DE CURSEUR d'AppKit plutôt que par un [c set] : le
 * système les réapplique tout seul quand la souris entre dans la vue, et les
 * laisse tomber quand elle passe au-dessus d'une palette ou d'une autre
 * fenêtre. Un [c set] posé au changement d'outil, lui, serait écrasé par la
 * première palette survolée et ne reviendrait jamais. */
void hcv_curseur_maj(void)
{
    if (gView) [[gView window] invalidateCursorRectsForView:gView];
}

static void cocoa_choose_tool(const char *name) {
    if (!name) return;
    struct { const char *nom; HCTool t; } table[] = {
        { "browse",         TOOL_BROWSE   },
        { "button",         TOOL_BUTTON   },
        { "field",          TOOL_FIELD    },
        { "select",         TOOL_SELRECT  },
        { "lasso",          TOOL_LASSO    },
        { "pencil",         TOOL_PENCIL   },
        { "brush",          TOOL_BRUSH    },
        { "eraser",         TOOL_ERASER   },
        { "line",           TOOL_LINE     },
        { "spray",          TOOL_SPRAY    },
        { "spray can",      TOOL_SPRAY    },
        { "rectangle",      TOOL_RECT     },
        { "rect",           TOOL_RECT     },
        { "bucket",         TOOL_FILL     },
        { "oval",           TOOL_OVAL     },
        { "curve",          TOOL_FREEFORM },
        { "text",           TOOL_TEXT     },
        { "polygon",        TOOL_FREEFORM },
    };
    for (unsigned i = 0; i < sizeof table / sizeof *table; i++) {
        if (strcasecmp(name, table[i].nom) == 0) {
            [gView dropFloating];

            /* Sortie immédiate du mode édition et retrait du focus de l'éditeur Cocoa */
            if (gEditingField) {
                [gView endFieldEdit];
                [[gView window] makeFirstResponder:gView];
            }
            /* Changer pour un outil qui ne sélectionne pas abandonne la
             * sélection de peinture, comme dans HyperCard : on ne garde pas
             * son lasso en prenant la main ou le crayon.
             *
             * gSelected, juste en dessous, est la sélection d'un OBJET —
             * bouton ou champ. Ce sont deux choses distinctes, et seule la
             * première était remise à zéro : le rectangle en pointillés
             * survivait au changement d'outil, fourmis comprises. */
            HCTool neuf = table[i].t;
            if (neuf != TOOL_SELRECT && neuf != TOOL_LASSO)
                hcv_abandonne_selection();

            gTool = neuf;
            gSelected = NULL;
            [gView stopSprayTimer];
            /* Le pointeur suit l'outil. Et un script qui avait posé son propre
             * curseur — ou CACHÉ le pointeur — rend la main ici : choisir un
             * outil est un geste de l'utilisateur, qui doit toujours pouvoir
             * reprendre le contrôle de ce qu'il a sous le bras. */
            gCursorScripte = NO;
            gCursorScripteObj = nil;
            if (gCursorHidden) { [NSCursor unhide]; gCursorHidden = NO; }
            hcv_curseur_maj();
            /* La palette entoure l'outil courant : « choose brush tool »
             * depuis un script doit déplacer ce cadre, sinon la palette
             * désigne un outil dont on ne se sert plus. */
            hcv_palette_maj(gToolPanel);
            [gView setNeedsDisplay:YES];
            return;
        }
    }
    NSLog(@"choose : outil inconnu « %s »", name);
}

static BOOL mods_has(const char *mods, const char *k) {
    if (!mods || !k || !*k) return NO;
    size_t n = strlen(k);
    for (const char *p = mods; *p; p++)
        if (strncasecmp(p, k, n) == 0) return YES;
    return NO;
}
/* ═══ VERROU D'ÉCRAN, CÔTÉ PEINTURE ═════════════════════════════════════
 *
 * « lock screen » ne verrouillait rien du tout pour le dessin. Le noyau
 * prévient pourtant l'hôte par host_global_set("lockScreen", …), mais
 * personne ici n'écoutait : chaque segment tracé invalidait sa zone, et un
 * tracé de surface en produit plus de deux mille. Entre un « lock screen »
 * et son « unlock screen », l'écran se recomposait donc deux mille fois —
 * exactement ce que la commande existe pour empêcher.
 *
 * Le noyau fait déjà cela pour les CHAMPS : notify_field met de côté ceux
 * qui changent pendant le verrou, et verrou_reveille les réveille au
 * déverrouillage. Il manquait le pendant pour la peinture, qui ne passe pas
 * par le noyau mais va droit à l'hôte.
 *
 * On accumule donc l'union des zones sales et l'on n'invalide qu'une fois,
 * au déverrouillage. Le noyau déverrouille de lui-même en retombant au
 * repos, si bien qu'un script qui oublie son « unlock screen » ne laisse pas
 * l'écran figé.
 *
 * Première version : les deux fonctions ci-dessous testaient elles-mêmes le
 * verrou, et les sites de peinture les appelaient. C'était trop étroit — le
 * dessin n'est pas la seule chose qui salit la vue pendant un script :
 *
 *   - antsTick: fait marcher les fourmis d'une sélection QUINZE FOIS PAR
 *     SECONDE, et invalidait toute la vue à chaque pas. Il suffisait qu'une
 *     sélection reste active — un « doMenu Select All » non suivi d'un
 *     changement d'outil — pour que le tracé apparaisse au fur et à mesure,
 *     verrou ou pas ;
 *   - une douzaine de rappels de l'hôte (choose, doMenu, type, les propriétés
 *     de peinture, le changement de champ ou de sélection) marquaient la vue
 *     sale directement ;
 *   - eraseAll et les autres méthodes de la vue également.
 *
 * Recenser ces chemins un par un, c'est en oublier un. On intercepte donc au
 * seul endroit par lequel ils passent TOUS : setNeedsDisplay: et
 * setNeedsDisplayInRect: de la vue elle-même, redéfinies plus bas. Le verrou
 * y accumule, et ne relâche qu'une fois. */
static BOOL   gLockScreen = NO;
static BOOL   gSaleTout   = NO;   /* une invalidation totale est en attente */
static BOOL   gSaleUnPeu  = NO;   /* gSaleRect porte une zone en attente    */
static NSRect gSaleRect;

/* Ces deux-la ne testent plus rien : la vue s'en charge. Elles restent parce
 * qu'elles disent l'intention au lieu du moyen. */
static void hcv_invalide(NSRect r)   { [gView setNeedsDisplayInRect:r]; }
static void hcv_invalide_tout(void)  { [gView setNeedsDisplay:YES]; }

static void hcv_verrou_ecran(BOOL ferme)
{
    if (ferme == gLockScreen) return;
    gLockScreen = ferme;
    if (ferme) return;                    /* on ferme : rien à faire */

    if (gSaleTout)       [gView setNeedsDisplay:YES];
    else if (gSaleUnPeu) [gView setNeedsDisplayInRect:gSaleRect];
    gSaleTout = NO;
    gSaleUnPeu = NO;
}

static void cocoa_drag(int x1, int y1, int x2, int y2, const char *mods) {
    Object *card = hc_current_card();
    if (!card || !gView) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSRect b = [gView bounds];
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)b.size.width, (int)b.size.height);
    NSPoint a = NSMakePoint(x1, y1), z = NSMakePoint(x2, y2);
    if (mods_has(mods, "shift")) {
        z = constrain_to_axis(a, z);
    }
    switch (gTool) {
        /* LA MÊME RÈGLE PAR SCRIPT. « drag from x,y to x,y » avec le crayon
         * doit faire ce que ferait la souris, sans quoi un script et un
         * geste donneraient deux résultats — et c'est le script qui aurait
         * tort sans qu'on puisse le voir. Le sens se décide sur le point de
         * DÉPART, comme au mouseDown. */
        case TOOL_PENCIL:
            if (paint_pixel_pose(rep, (int)lround(a.x), (int)lround(a.y)))
                erase_stroke(rep, a, z, gLineWidth);
            else
                paint_stroke(rep, a, z, [NSColor blackColor], gLineWidth);
            break;
        case TOOL_BRUSH:  brush_stroke(rep, a, z); break;
        case TOOL_ERASER: erase_stroke(rep, a, z, 16); break;
        case TOOL_SPRAY:  spray_stroke(rep, a, z, gSprayRadius, gSprayDensity); break;
        case TOOL_LINE:   paint_shape(rep, TOOL_LINE, a, z, [NSColor blackColor], gLineWidth); break;
        case TOOL_RECT: case TOOL_OVAL: case TOOL_FREEFORM:
            if (gShapeFilled) fill_shape(rep, gTool, a, z);
            else paint_shape(rep, gTool, a, z, [NSColor blackColor], gLineWidth);
            break;

        case TOOL_SELRECT:
            /* Au lasso rectangulaire, un glissement ne dessine pas : il
             * SÉLECTIONNE. On pose directement l'état d'arrivée d'une
             * sélection faite à la souris, sans passer par gSelRectDrawing.
             *
             * Sans ce cas, le « drag from 0,0 to 169,341 » des scripts tombait
             * dans default, aucune sélection n'était établie, et le
             * « doMenu "Clear Picture" » qui suit effaçait TOUT le calque au
             * lieu de la seule bande visée — la courbe qu'on venait de tracer
             * disparaissait avec. */
            gSelStart = a;
            gSelEnd   = z;
            gSelRectDrawing = NO;
            gSelRectActive  = YES;
            [gView startAntsTimer];
            break;

        default:
            if (gSelected) {
                gSelected->x += x2 - x1;
                gSelected->y += y2 - y1;
            }
            break;
    }

    /* Ne rafraîchir que la zone touchée.
     *
     * Un tracé de script appelle cette fonction une fois par segment, et
     * v3_respire rend la main à AppKit à chaque tour de boucle : marquer TOUTE
     * la vue sale, c'était recomposer la carte entière — calque, champs,
     * boutons — une fois par segment. Sur une courbe de deux mille points, ça
     * se voit. HyperCard ne rafraîchissait que les pixels touchés, et c'est
     * pour cela que la pile d'origine paraît plus vive sous Basilisk.
     *
     * Deux exceptions gardent le rafraîchissement complet : la sélection, dont
     * le rectangle en pointillés bouge sur toute sa surface, et le déplacement
     * d'un objet, qui laisse un trou à son ancienne place. */
    if (gTool == TOOL_SELRECT || (gSelected && gTool != TOOL_BROWSE)) {
        hcv_invalide_tout();
        return;
    }

    NSRect sale = NSMakeRect(MIN(a.x, z.x), MIN(a.y, z.y),
                             fabs(z.x - a.x), fabs(z.y - a.y));

    /* De quoi couvrir l'épaisseur du trait, la largeur du pinceau et la
     * dispersion de l'aérographe. Large plutôt que juste : une marge de trop
     * ne coûte que quelques pixels, une marge manquante laisse une traînée. */
    CGFloat marge = gLineWidth + 2;
    if (gTool == TOOL_BRUSH)  marge = 24;
    if (gTool == TOOL_ERASER) marge = 20;
    if (gTool == TOOL_SPRAY)  marge = gSprayRadius + 4;

    hcv_invalide(NSInsetRect(sale, -marge, -marge));
}


static void cocoa_click_at(int x, int y, const char *mods) {
    (void)mods;
    if (!gView) return;
    NSPoint p = NSMakePoint(x, y);

    Object *hit = part_at(hc_current_card(), p);
    gClickPoint = p;
    gClickField = (hit && hit->type == OBJ_FIELD) ? hit : NULL;
    gMouseClicked = YES;

    if (gTool == TOOL_TEXT) {
        [gView commitText];
        gTextPos = p;
        gTextBuf = [NSMutableString string];
        gTextActive = YES;
        [gView setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_FILL) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer,
                                    (int)[gView bounds].size.width,
                                    (int)[gView bounds].size.height);
        flood_fill(rep, (int)p.x, (int)p.y);
        hcv_invalide_tout();           /* le remplissage peut aller partout */
        return;
    }

    /* Un clic avec un outil de peinture POSE UN POINT.
     *
     * C'est la primitive de tracé point par point d'HyperCard : les scripts
     * qui dessinent sans relier leurs points — case « Connect » décochée dans
     * la pile des équations paramétriques — n'emploient que « click at ».
     * Sans ce cas, ils ne dessinaient rien du tout, en silence. */
    if (gTool == TOOL_PENCIL || gTool == TOOL_BRUSH  ||
        gTool == TOOL_ERASER || gTool == TOOL_SPRAY  ||
        gTool == TOOL_LINE   || gTool == TOOL_RECT   ||
        gTool == TOOL_OVAL   || gTool == TOOL_FREEFORM) {

        Object *card = hc_current_card();
        if (!card) return;
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer,
                                    (int)[gView bounds].size.width,
                                    (int)[gView bounds].size.height);

        CGFloat marge;
        switch (gTool) {
            case TOOL_BRUSH:
                brush_stroke(rep, p, p);
                marge = 24;
                break;
            case TOOL_ERASER:
                erase_stroke(rep, p, p, 16);
                marge = 20;
                break;
            case TOOL_SPRAY:
                spray_stroke(rep, p, p, gSprayRadius, gSprayDensity);
                marge = gSprayRadius + 4;
                break;
            default:
                /* Crayon, ligne et formes. On passe par paint_stroke plutôt
                 * que de peindre à la main : elle travaille dans le repère de
                 * la CARTE, y vers le bas, alors qu'un contexte bitmap a son
                 * origine en bas à gauche — un NSRectFill direct posait le
                 * point à la hauteur inverse.
                 *
                 * Un trait d'un pixel de long tient lieu de point, et évite le
                 * cas dégénéré d'un segment de longueur nulle, dont le rendu
                 * dépend du moteur. */
                paint_stroke(rep, p, NSMakePoint(p.x + 1, p.y),
                             [NSColor blackColor], gLineWidth);
                marge = gLineWidth + 2;
                break;
        }

        /* Ne rafraîchir que le point posé. Marquer toute la vue sale
         * recomposait la carte entière — calque, champs, boutons — une fois
         * par point, et un maillage de surface en compte des milliers. */
        hcv_invalide(NSMakeRect(p.x - marge, p.y - marge, 2 * marge, 2 * marge));
        return;
    }

    if (hit && gTool == TOOL_BROWSE) {
        hc_send(hit, "mouseDown");
        /* `hit` est une variable locale : object_gone ne peut pas la mettre à
         * NULL. Si mouseDown a supprimé l'objet, le deuxième envoi lirait de
         * la mémoire rendue — on demande donc au noyau s'il est encore là. */
        if (hc_object_is_live(hit)) hc_send(hit, "mouseUp");
    }
    [gView setNeedsDisplay:YES];
}

/* ═══ Les articles de menu d'HyperCard ══════════════════════════════════
 *
 * Une pile d'époque écrit « doMenu "New Card" ». Jusqu'ici cet article
 * partait dans cocoa_do_menu, qui ne connaissait que « Clear Picture » et
 * « Select All », et n'en faisait rien. Sans un mot : c'est le pire des cas,
 * la pile croit avoir agi.
 *
 * Les articles de HC portent maintenant leurs noms d'HyperCard, si bien que
 * cette table ne traduit plus rien — elle dit quelle ACTION déclenche un
 * article donné. On pourrait la remplacer par une recherche du même titre
 * dans la barre de menus ; c'est justement ce qu'on ne veut pas, parce qu'il
 * y a des articles à ne PAS rendre atteignables depuis un script (voir plus
 * bas). Une table explicite dit lesquels, une recherche générique les
 * attraperait tous.
 *
 * L'action part par la chaîne des répondeurs, ces méthodes étant réparties
 * entre AppDelegate, HCView et sa catégorie Dialogs : les nommer une par une
 * supposerait de savoir où chacune habite, ce qui serait faux au premier
 * déménagement.
 *
 * DEUX ABSENTS VOLONTAIRES : « Delete Card » et « Cut Card ». Ils appellent
 * hc_delete_card, qui libère la carte. Si le script qui les demande tourne sur
 * un bouton DE CETTE CARTE — le cas le plus courant —, on libère l'objet dont
 * le gestionnaire est en cours d'exécution. Il faut d'abord différer la
 * libération jusqu'à la fin du gestionnaire ; tant que ce n'est pas fait, ces
 * deux articles restent hors de portée d'un script, et c'est un moindre mal
 * comparé à un plantage. */
/* ═══ Offrir un article à la pile avant de l'exécuter ═══════════════════
 *
 * HyperCard envoie doMenu pour TOUT choix de menu, celui de l'utilisateur
 * compris ; l'action native n'a lieu que si personne ne l'intercepte. Les
 * actions de HC savent déjà agir : il ne leur manquait que de demander
 * d'abord. D'où une ligne en tête de chacune :
 *
 *     if (hcv_menu_trappe("New Card")) return;
 *
 * Plutôt que de détourner les articles en leur réécrivant cible et action au
 * démarrage : validateMenuItem: se règle sur l'action d'un article, et la lui
 * changer sous les pieds griserait ou dégriserait les menus au hasard.
 *
 * gDansDoMenu ferme la boucle. Un script qui fait « doMenu "New Card" » passe
 * par hc_do_menu — message d'abord, puis cocoa_menu_hypercard, qui appelle
 * newCard:. Sans ce drapeau, newCard: reproposerait l'article à la pile, le
 * même « on doMenu » se déclencherait une seconde fois et la carte ne serait
 * jamais créée. Le drapeau dit : on est DÉJÀ dans le traitement d'un article,
 * la question a été posée. */
static BOOL gDansDoMenu = NO;

BOOL hcv_menu_trappe(const char *article)
{
    if (gDansDoMenu) return NO;
    return hc_menu_trappe(article) ? YES : NO;
}

static NSString *menu_normalise(NSString *s)
{
    /* Un script écrit aussi bien « Find… » que « Find... » ou « Find » : les
     * trois désignent le même article, et la casse n'y est pour rien. */
    NSCharacterSet *fin =
        [NSCharacterSet characterSetWithCharactersInString:@". …"];
    return [[s stringByTrimmingCharactersInSet:fin] lowercaseString];
}

static void cocoa_menu_hypercard(const char *item)
{
    static const struct { const char *article; const char *selecteur; } TABLE[] = {
        { "New Card",       "newCard:"           },
        { "Copy Card",      "copyCard:"          },
        { "Paste Card",     "paste:"             },
        { "New Background", "newBackground:"     },
        { "Background",     "toggleBackground:"  },
        /* L'ordre de superposition, atteignable par script comme par menu.
         * « Send Farther » est l'orthographe de HyperCard ; on n'invente pas
         * d'alias faute de pile d'époque qui en emploie un autre. */
        { "Bring Closer",   "bringCloser:"       },
        { "Send Farther",   "sendFarther:"       },
        { "Card Info",      "showCardInfo"       },
        { "Bkgnd Info",     "showBackgroundInfo" },
        { "Stack Info",     "showStackInfo"      },
        { "Find",           "findInStack:"       },
        { "New Stack",      "newStack:"          },
        { "Open Stack",     "openStack:"         },
        { "Save a Copy",    "saveStack:"         },
        { NULL, NULL }
    };

    NSString *voulu = menu_normalise([NSString stringWithUTF8String:item]);
    for (int i = 0; TABLE[i].article; i++) {
        NSString *nom = [NSString stringWithUTF8String:TABLE[i].article];
        if (![menu_normalise(nom) isEqualToString:voulu]) continue;
        SEL sel = NSSelectorFromString(
                      [NSString stringWithUTF8String:TABLE[i].selecteur]);
        if (sel) {
            /* La pile a déjà eu son mot à dire, dans hc_do_menu : que l'action
             * ne le lui redemande pas. Voir gDansDoMenu. */
            BOOL avant = gDansDoMenu;
            gDansDoMenu = YES;
            [NSApp sendAction:sel to:nil from:gView];
            gDansDoMenu = avant;
        }
        return;
    }
    NSLog(@"doMenu : article inconnu « %s »", item);
}

/* ═══ Le reflet Cocoa des menus de pile ═════════════════════════════════
 *
 * Le noyau tient le modèle ; on n'en construit ici que l'image. À chaque
 * menus_changed on refait toute la barre plutôt que de suivre des
 * modifications au coup par coup : le modèle est petit, et reconstruire est
 * la seule façon d'être sûr que l'écran dit la vérité.
 *
 * Les menus de pile se reconnaissent à leur ÉTIQUETTE. Sans marque, il
 * faudrait les distinguer des menus de l'application par leur titre, et une
 * pile qui s'appellerait « Edit » emporterait le menu Édition avec elle.
 *
 * L'étiquette d'un article encode ses deux indices — menu et rang — parce que
 * c'est tout ce dont hc_menu_choisi a besoin, et que la ranger là évite de
 * tenir un tableau parallèle qui se désaccorderait au premier oubli. */
#define HCV_MENU_SCRIPT   0x48435F4D     /* « HC_M », improbable ailleurs */

static void cocoa_menus_changed(void)
{
    NSMenu *barre = [NSApp mainMenu];
    if (!barre) return;

    /* D'abord retirer les anciens, en descendant : chaque retrait décale ce
     * qui suit. */
    for (NSInteger k = [barre numberOfItems] - 1; k >= 0; k--)
        if ([[barre itemAtIndex:k] tag] == HCV_MENU_SCRIPT)
            [barre removeItemAtIndex:k];

    for (int i = 0; i < hc_menu_nombre(); i++) {
        const char *nom = hc_menu_nom(i);
        if (!nom) continue;

        NSMenuItem *tete = [[NSMenuItem alloc] init];
        [tete setTag:HCV_MENU_SCRIPT];
        NSMenu *m = [[NSMenu alloc] initWithTitle:
                        [NSString stringWithUTF8String:nom]];

        /* Sans cela AppKit décide seul de ce qui est actif, en cherchant un
         * répondeur pour chaque action — et « disable menuItem 4 » n'aurait
         * aucun effet visible. */
        [m setAutoenablesItems:NO];

        for (int j = 0; j < hc_menu_nb_articles(i); j++) {
            const char *art = hc_menu_article(i, j);
            if (!art) continue;

            if (!strcmp(art, "-")) {
                [m addItem:[NSMenuItem separatorItem]];
                continue;
            }
            NSMenuItem *mi = [[NSMenuItem alloc]
                initWithTitle:[NSString stringWithUTF8String:art]
                       action:@selector(hcMenuScriptItem:)
                keyEquivalent:@""];
            [mi setTarget:gView];
            [mi setTag:(NSInteger)(i * 1000 + j)];
            /* Un menu désactivé grise TOUS ses articles.
             *
             * setEnabled: sur l'article de tête ne suffit pas : la barre de
             * menus, elle, décide seule de ce qui est actif, et l'on ne va pas
             * lui retirer ce pouvoir pour les menus de l'application. Griser
             * le contenu donne le même résultat visible, sans y toucher. */
            [mi setEnabled:(hc_menu_est_actif(i) && hc_menu_article_actif(i, j))
                            ? YES : NO];
            /* La marque à gauche du nom : « set the checkMark of menuItem 2
             * of menu "X" to true ». C'est l'état d'un article de menu au
             * sens d'AppKit, pas un caractère à ajouter au titre. */
            [mi setState:hc_menu_article_coche(i, j) ? NSControlStateValueOn
                                                     : NSControlStateValueOff];
            [m addItem:mi];
        }

        [tete setSubmenu:m];
        [tete setEnabled:hc_menu_est_actif(i) ? YES : NO];
        [barre addItem:tete];
    }
}

static void cocoa_do_menu(const char *item) {
    if (!item || !gView) return;
 
    /* « Clear Picture », « Select All », « Opaque » et « Transparent » ne
     * sont plus traités ici : leur corps est remonté dans paintOpTag:, avec
     * les autres articles du menu Paint, et la table ci-dessous les y
     * envoie.
     *
     * Ce n'était pas un rangement. Ces corps ne vivaient QUE dans cette
     * fonction, appelée par le seul « doMenu » : les scripts avaient Select
     * All et Clear Picture, la SOURIS ne les avait pas — aucun article de
     * menu ne les servait. Une mise en œuvre, trois portes, plus rien qui
     * puisse diverger. */

    /* Les articles du menu Paint, atteignables par script :
     * « doMenu "Invert" ». Avant la table des articles d'interface, qui
     * enverrait une action à gView — laquelle n'a pas d'étiquette à lire. */
    {
        static const struct { const char *nom; NSInteger tag; } PEINTURE[] = {
            { "Invert",          HCV_PAINT_INVERT  },
            { "Darken",          HCV_PAINT_DARKEN  },
            { "Lighten",         HCV_PAINT_LIGHTEN },
            { "Trace Edges",     HCV_PAINT_TRACE   },
            { "Flip Horizontal", HCV_PAINT_FLIPH   },
            { "Flip Vertical",   HCV_PAINT_FLIPV   },
            { "Rotate Left",     HCV_PAINT_ROTL    },
            { "Rotate Right",    HCV_PAINT_ROTR    },
            { "Fill",            HCV_PAINT_FILL    },
            { "Keep",            HCV_PAINT_KEEP    },
            { "Revert",          HCV_PAINT_REVERT  },
            { "Select All",      HCV_PAINT_SELECTALL   },
            { "Clear Picture",   HCV_PAINT_CLEAR       },
            /* « Clear » tout court : l'abréviation que des piles d'époque
             * emploient, et que l'ancien code acceptait déjà. La retirer
             * casserait des scripts qui marchent. */
            { "Clear",           HCV_PAINT_CLEAR       },
            { "Opaque",          HCV_PAINT_OPAQUE      },
            { "Transparent",     HCV_PAINT_TRANSPARENT },
            { "Grid",            HCV_PAINT_GRID        },
            { "FatBits",         HCV_PAINT_FATBITS     },
            { NULL, 0 }
        };
        for (int i = 0; PEINTURE[i].nom; i++)
            if (strcasecmp(PEINTURE[i].nom, item) == 0) {
                [gView paintOpTag:PEINTURE[i].tag];
                return;
            }
    }

    cocoa_menu_hypercard(item);
}

static void cocoa_type_text(const char *text, const char *mods) {
    (void)mods;
    if (!text || !gView) return;
    NSString *s = [NSString stringWithUTF8String:text];
    if (!s) return;

    if (gTool == TOOL_TEXT && gTextActive) {
        if (!gTextBuf) gTextBuf = [NSMutableString string];
        [gTextBuf appendString:s];
        [gView commitText];
        gView.needsDisplay = YES;
        return;
    }

    if (gEditingField && gFieldEditor) {
        [gFieldEditor insertText:s replacementRange:[gFieldEditor selectedRange]];
    } else if (gMsgBox) {
        [gMsgBox setStringValue:
            [[gMsgBox stringValue] stringByAppendingString:s]];
    }
    [gView setNeedsDisplay:YES];
}

static void cocoa_field_changed(Object *field)
{
    if (field && field == gEditingField && gFieldEditor) {
        const char *tx  = hc_field_text(field);
        NSString *noyau = [NSString stringWithUTF8String:tx ? tx : ""];

        if (![noyau isEqualToString:[gFieldEditor string]]) {
            NSRange sel = [gFieldEditor selectedRange];
            NSPoint org = [[gFieldScroll contentView] bounds].origin;

            NSDictionary *base = obj_attrs(field, 12, [NSColor blackColor]);
            gForEditor = YES;
            [[gFieldEditor textStorage]
                setAttributedString:field_attr_string(field, noyau, base)];
            gForEditor = NO;

            NSUInteger n = [[gFieldEditor string] length];
            if (sel.location > n) sel.location = n;
            if (sel.location + sel.length > n) sel.length = n - sel.location;
            [gFieldEditor setSelectedRange:sel];

            [[gFieldScroll contentView] scrollToPoint:org];
            [gFieldScroll reflectScrolledClipView:[gFieldScroll contentView]];
        }
    }
    [gView setNeedsDisplay:YES];
}

static BOOL gApplyingSelection = NO;

static void cocoa_selection_changed(Object *field, int start, int len) {
    if (gApplyingSelection) return;
    if (!gView) return;

    if (!field) {
        [gView setNeedsDisplay:YES];
        return;
    }

    gApplyingSelection = YES;

    if (field->locktext) {
        if (gEditingField && gEditingField != field) [gView endFieldEdit];
        gApplyingSelection = NO;
        [gView setNeedsDisplay:YES];
        return;
    }

    if (gEditingField != field) {
        [gView endFieldEdit];
        [gView beginFieldEdit:field];
    }

    if (gFieldEditor) {
        /* OCTETS -> UTF-16. Le noyau donne des décalages en octets ; les
         * passer tels quels à setSelectedRange: surlignait à côté dès le
         * premier accent. « select char 1 of card field "x" » sur « été »
         * demande les deux premiers octets, ce que Cocoa comprenait comme
         * les deux premiers CARACTÈRES. */
        const char *tx = hc_field_text(field);
        NSUInteger n = [[gFieldEditor string] length];
        NSUInteger s = utf16_from_byte(tx, start < 0 ? 0 : start);
        NSUInteger f = utf16_from_byte(tx, (start < 0 ? 0 : start) +
                                           (len   < 0 ? 0 : len));
        NSUInteger l = f > s ? f - s : 0;
        if (s > n)     s = n;
        if (s + l > n) l = n - s;
        [gFieldEditor setSelectedRange:NSMakeRange(s, l)];
        [gFieldEditor scrollRangeToVisible:NSMakeRange(s, l)];
    }

    gApplyingSelection = NO;
    [gView setNeedsDisplay:YES];
}

static char gGlobBuf[64];

static BOOL gSyncingEditorScroll = NO;

static CGFloat editor_course(void) {
    if (!gFieldScroll) return 0;
    CGFloat c = NSHeight([[gFieldScroll documentView] frame])
              - NSHeight([[gFieldScroll contentView] bounds]);
    return c > 0 ? c : 0;
}

static CGFloat editor_fraction(void) {
    CGFloat c = editor_course();
    if (c <= 0) return 0;
    CGFloat f = [[gFieldScroll contentView] bounds].origin.y / c;
    return f < 0 ? 0 : (f > 1 ? 1 : f);
}

static void sync_editor_scroll(Object *o) {
    if (!o || o != gEditingField || !gFieldScroll) return;

    NSView *doc = [gFieldScroll documentView];
    NSRect df = [doc frame];
    if (df.origin.y != 0 || df.origin.x != 0) {
        df.origin.x = 0;
        df.origin.y = 0;
        [doc setFrame:df];
    }

    CGFloat maxs = field_max_scroll(o);
    CGFloat pos  = maxs > 0 ? o->scroll / maxs : 0;
    if (pos < 0) pos = 0;
    if (pos > 1) pos = 1;

    gSyncingEditorScroll = YES;
    [[gFieldScroll contentView]
        scrollToPoint:NSMakePoint(0, floor(pos * editor_course()))];
    [gFieldScroll reflectScrolledClipView:[gFieldScroll contentView]];
    gSyncingEditorScroll = NO;
}

static int click_word_range(Object *f, NSPoint p, int *start, int *end) {
    if (!f || f->type != OBJ_FIELD) return 0;
    const char *tx = hc_field_text(f);
    if (!tx || !*tx) return 0;
    NSString *s = [NSString stringWithUTF8String:tx];
    if (!s) return 0;

    NSRect tr  = field_text_rect(f);
    NSRect off = field_text_draw_rect(f);

    /* Même mise en page que le tracé — voir click_line_number. */
    NSTextContainer *tc = nil;
    NSLayoutManager *lm = field_layout(f, s,
                                       obj_attrs(f, 12, [NSColor blackColor]),
                                       tr.size.width, &tc);

    NSPoint q = NSMakePoint(p.x - NSMinX(off), p.y - NSMinY(off));
    if (q.y < 0) return 0;
    NSRect used = [lm usedRectForTextContainer:tc];
    if (q.y > NSMaxY(used)) return 0;

    CGFloat frac = 0;
    NSUInteger gi = [lm glyphIndexForPoint:q inTextContainer:tc
                    fractionOfDistanceThroughGlyph:&frac];
    NSUInteger ci = [lm characterIndexForGlyphAtIndex:gi];
    NSUInteger n = [s length];
    if (ci >= n) ci = n ? n - 1 : 0;

    NSCharacterSet *blancs = [NSCharacterSet whitespaceAndNewlineCharacterSet];
    if ([blancs characterIsMember:[s characterAtIndex:ci]]) return 0;

    NSUInteger a = ci, b = ci;
    while (a > 0 && ![blancs characterIsMember:[s characterAtIndex:a - 1]]) a--;
    while (b + 1 < n && ![blancs characterIsMember:[s characterAtIndex:b + 1]]) b++;

    *start = (int)[[s substringToIndex:a] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    *end   = (int)[[s substringToIndex:b + 1] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    return 1;
}

static int click_line_number(Object *f, NSPoint p) {
    if (!f || f->type != OBJ_FIELD) return 0;

    const char *tx = hc_field_text(f);
    if (!tx || !*tx) return 0;
    NSString *s = [NSString stringWithUTF8String:tx];
    if (!s) return 0;

    NSRect tr  = field_text_rect(f);
    NSRect off = field_text_draw_rect(f);

    /* La MÊME mise en page que le tracé, et non une seconde montée pour
     * l'occasion.
     *
     * Deux raisons, et la seconde est celle qui se voit. La première : cette
     * fonction est appelée dans « repeat while the mouse is down », donc à
     * chaque tour de boucle, et remonter la mise en page d'un champ de
     * quarante-huit kilo-octets à chaque fois coûtait des centaines de
     * millisecondes par tour. La seconde : le tracé et le clic DOIVENT
     * s'accorder au pixel près, et deux mises en page montées séparément
     * finissent toujours par diverger — l'oubli de setUsesFontLeading:NO dans
     * field_layout a suffi à décaler la ligne surlignée. Une seule mise en
     * page, et la question ne se pose plus. */
    NSTextContainer *tc = nil;
    NSLayoutManager *lm = field_layout(f, s,
                                       obj_attrs(f, 12, [NSColor blackColor]),
                                       tr.size.width, &tc);

    NSPoint q = NSMakePoint(p.x - NSMinX(off), p.y - NSMinY(off));
    if (q.y < 0) return 0;

    CGFloat frac = 0;
    NSUInteger gi = [lm glyphIndexForPoint:q inTextContainer:tc
                    fractionOfDistanceThroughGlyph:&frac];
    NSUInteger ci = [lm characterIndexForGlyphAtIndex:gi];
    if (ci > [s length]) ci = [s length];

    NSRect used = [lm usedRectForTextContainer:tc];
    if (q.y > NSMaxY(used)) return 0;

    int line = 1;
    for (NSUInteger i = 0; i < ci; i++)
        if ([s characterAtIndex:i] == '\n') line++;
    return line;
}

/* ═══ Les deux couleurs de peinture, et leurs noms ══════════════════════
 *
 * « foreColor » et « backColor » sont les noms naturels, ceux qu'emploient
 * les descendants d'HyperCard. « paintColor » et « paintBackColor » ont été
 * écrits les premiers ici ; ils restent acceptés, parce qu'un nom qui a servi
 * une fois se retrouve dans une pile, et qu'une pile ne se corrige pas à
 * distance. « inkColor » suit, puisque c'est le nom de la variable.
 *
 * Une seule table, lue par la lecture ET par l'écriture : les faire diverger
 * donnerait une propriété qu'on peut poser et pas relire, ou l'inverse.
 *
 * Rend +1 pour l'encre, -1 pour le fond, 0 si ce n'est aucune des deux. */
static int hcv_quelle_couleur(const char *nom)
{
    static const char *ENCRE[] = { "foreColor", "foregroundColor",
                                   "paintColor", "inkColor", NULL };
    static const char *FOND[]  = { "backColor", "backgroundColor",
                                   "paintBackColor", NULL };
    for (int i = 0; ENCRE[i]; i++) if (!strcasecmp(nom, ENCRE[i])) return  1;
    for (int i = 0; FOND[i];  i++) if (!strcasecmp(nom, FOND[i]))  return -1;
    return 0;
}

static const char *cocoa_global_get(const char *name) {
    if (strcasecmp(name, "mouse") == 0)
        return ([NSEvent pressedMouseButtons] & 1) ? "down" : "up";

    if (strcasecmp(name, "optionKey") == 0)
        return ([NSEvent modifierFlags] & NSEventModifierFlagOption) ? "down" : "up";
    if (strcasecmp(name, "commandKey") == 0)
        return ([NSEvent modifierFlags] & NSEventModifierFlagCommand) ? "down" : "up";
    if (strcasecmp(name, "shiftKey") == 0)
        return ([NSEvent modifierFlags] & NSEventModifierFlagShift) ? "down" : "up";

    if (strcasecmp(name, "mouseH") == 0 || strcasecmp(name, "mouseV") == 0) {
        NSPoint s = [NSEvent mouseLocation];
        NSRect  w = [[gView window] convertRectFromScreen:
                        NSMakeRect(s.x, s.y, 0, 0)];
        NSPoint v = [gView convertPoint:w.origin fromView:nil];
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d",
                 (name[5] == 'H' || name[5] == 'h') ? (int)v.x : (int)v.y);
        return gGlobBuf;
    }

    /* L'état de la machine. Le noyau ne peut pas le connaître ; nous, si.
     *
     * On ne répond QUE ce qu'on sait vraiment — un chiffre décoratif serait
     * pire que pas de réponse, puisqu'un script s'en servirait pour décider
     * quelque chose. Les trois dernières (heapSpace, windows, programs)
     * restaient sans réponse faute d'équivalent ÉVIDENT sur macOS ; elles en
     * ont un, chacune expliquée à sa place. */
    if (strcasecmp(name, "diskSpace") == 0) {
        NSDictionary *a = [[NSFileManager defaultManager]
                            attributesOfFileSystemForPath:NSHomeDirectory()
                                                    error:nil];
        NSNumber *libre = a[NSFileSystemFreeSize];
        if (!libre) return NULL;
        snprintf(gGlobBuf, sizeof gGlobBuf, "%llu",
                 (unsigned long long)[libre unsignedLongLongValue]);
        return gGlobBuf;
    }

    if (strcasecmp(name, "systemVersion") == 0) {
        NSOperatingSystemVersion v =
            [[NSProcessInfo processInfo] operatingSystemVersion];
        snprintf(gGlobBuf, sizeof gGlobBuf, "%ld.%ld.%ld",
                 (long)v.majorVersion, (long)v.minorVersion,
                 (long)v.patchVersion);
        return gGlobBuf;
    }

    /* « the heapSpace » : sur le Macintosh d'origine, la place restante dans
     * le tas de l'application — un chiffre que les scripts consultaient avant
     * d'importer une grosse image ou d'ouvrir une pile.
     *
     * macOS n'a plus de tas borné par application, donc la question littérale
     * n'a pas de réponse. Ce qui répond à l'INTENTION, c'est la mémoire
     * physique encore disponible : pages libres plus pages inactives, que le
     * noyau rendra sans échanger sur disque. C'est le même chiffre qu'affiche
     * le Moniteur d'activité sous « mémoire disponible ». */
    if (strcasecmp(name, "heapSpace") == 0) {
        mach_port_t hote = mach_host_self();
        vm_statistics64_data_t st;
        mach_msg_type_number_t n = HOST_VM_INFO64_COUNT;
        if (host_statistics64(hote, HOST_VM_INFO64,
                              (host_info64_t)&st, &n) != KERN_SUCCESS)
            return NULL;
        vm_size_t page = 0;
        if (host_page_size(hote, &page) != KERN_SUCCESS || page == 0)
            return NULL;
        unsigned long long libre =
            ((unsigned long long)st.free_count +
             (unsigned long long)st.inactive_count) * (unsigned long long)page;
        snprintf(gGlobBuf, sizeof gGlobBuf, "%llu", libre);
        return gGlobBuf;
    }

    /* « the windows » : les fenêtres ouvertes, une par ligne. HyperCard y
     * comptait la pile, la boîte de messages, les palettes — exactement ce
     * que [NSApp windows] énumère chez nous.
     *
     * On saute celles qui n'ont pas de titre : ce sont les fenêtres de
     * service que Cocoa crée pour son compte (ombres, panneaux d'aide), et
     * une ligne vide dans la liste ne désignerait rien.
     *
     * Tampon PROPRE, pas gGlobBuf : celui-ci fait soixante-quatre octets, et
     * une liste de fenêtres les dépasse dès la troisième. */
    if (strcasecmp(name, "windows") == 0) {
        static char buf[4096];
        buf[0] = '\0';
        size_t len = 0;
        for (NSWindow *w in [NSApp windows]) {
            const char *t = [[w title] UTF8String];
            if (!t || !*t) continue;
            size_t besoin = strlen(t) + (len ? 1 : 0);
            if (len + besoin + 1 > sizeof buf) break;   /* tronqué proprement */
            if (len) buf[len++] = '\n';
            memcpy(buf + len, t, strlen(t));
            len += strlen(t);
            buf[len] = '\0';
        }
        return buf;
    }

    /* « the programs » : les applications en cours, une par ligne. C'était la
     * liste du menu Pomme sous MultiFinder ; c'est aujourd'hui celle de
     * NSWorkspace.
     *
     * On ne garde que les applications à interface — runningApplications
     * énumère aussi les agents et les démons, que HyperCard n'aurait jamais
     * listés et qui noieraient le résultat sous des dizaines de lignes. */
    if (strcasecmp(name, "programs") == 0) {
        static char buf[4096];
        buf[0] = '\0';
        size_t len = 0;
        for (NSRunningApplication *a in
                 [[NSWorkspace sharedWorkspace] runningApplications]) {
            if ([a activationPolicy] != NSApplicationActivationPolicyRegular)
                continue;
            const char *t = [[a localizedName] UTF8String];
            if (!t || !*t) continue;
            size_t besoin = strlen(t) + (len ? 1 : 0);
            if (len + besoin + 1 > sizeof buf) break;
            if (len) buf[len++] = '\n';
            memcpy(buf + len, t, strlen(t));
            len += strlen(t);
            buf[len] = '\0';
        }
        return buf;
    }

    if (strcasecmp(name, "screenRect") == 0) {
        NSRect r = [[NSScreen mainScreen] frame];
        snprintf(gGlobBuf, sizeof gGlobBuf, "0,0,%d,%d",
                 (int)r.size.width, (int)r.size.height);
        return gGlobBuf;
    }

    if (strcasecmp(name, "mouseClick") == 0) {
        BOOL eu = gMouseClicked;
        gMouseClicked = NO;
        return eu ? "true" : "false";
    }

    if (strcasecmp(name, "clickChunk") == 0) {
        static char buf[192];
        buf[0] = '\0';
        if (gClickField) {
            int s = 0, e = 0;
            if (click_word_range(gClickField, gClickPoint, &s, &e) && e > s) {
                char d[96];
                hc_describe(gClickField, d, sizeof d);
                snprintf(buf, sizeof buf, "char %d to %d of %s%s",
                         s + 1, e,
                         hc_owner_is_bg(gClickField) ? "bg " : "card ", d);
            }
        }
        return buf;
    }

    if (strcasecmp(name, "mouseLoc") == 0) {
        NSPoint s = [NSEvent mouseLocation];
        NSRect  w = [[gView window] convertRectFromScreen:
                        NSMakeRect(s.x, s.y, 0, 0)];
        NSPoint v = [gView convertPoint:w.origin fromView:nil];
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d,%d", (int)v.x, (int)v.y);
        return gGlobBuf;
    }

    if (strcasecmp(name, "ticks") == 0) {
        static NSTimeInterval t0 = 0;
        NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
        if (t0 == 0) t0 = now;
        snprintf(gGlobBuf, sizeof gGlobBuf, "%ld", (long)((now - t0) * 60.0));
        return gGlobBuf;
    }

    if (strcasecmp(name, "textHeight") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d",
                 gTextHeight > 0 ? gTextHeight
                                 : (int)[text_font() pointSize] + 3);
        return gGlobBuf;
    }
    if (strcasecmp(name, "textSize") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", (int)[text_font() pointSize]);
        return gGlobBuf;
    }
    if (strcasecmp(name, "textFont") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%s",
                 [[text_font() familyName] UTF8String]);
        return gGlobBuf;
    }
    if (strcasecmp(name, "textStyle") == 0)
        return gTextStyleName ? [gTextStyleName UTF8String] : "plain";
    if (strcasecmp(name, "textAlign") == 0)
        return gTextAlign ? [gTextAlign UTF8String] : "left";
    if (strcasecmp(name, "cursor") == 0)
        return gCursorNom ? [gCursorNom UTF8String] : "arrow";
    if (strcasecmp(name, "editBkgnd") == 0)
        return gEditBackground ? "true" : "false";

    /* Rendues en « r,v,b », le format que rend déjà « the textColor » : un
     * script qui relit une couleur pour la recalculer trouve trois nombres,
     * pas un nom qu'il faudrait retraduire. */
    if (hcv_quelle_couleur(name)) {
        NSColor *c = (hcv_quelle_couleur(name) < 0) ? gBackColor : gInkColor;
        NSColor *sr = [c colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
        if (!sr) return "0,0,0";
        CGFloat r = 0, v = 0, b = 0, a = 1;
        [sr getRed:&r green:&v blue:&b alpha:&a];
        int ia = (int)lround(a * 255);
        /* Trois nombres quand l'encre est opaque, quatre sinon. Rendre
         * toujours quatre casserait les scripts qui font « item 3 of the
         * paintColor » ou qui relisent pour réécrire ; n'en rendre que trois
         * perdrait l'opacité qu'on vient de poser. Le format dit donc ce
         * qu'il y a à dire, et pas davantage. */
        if (ia >= 255)
            snprintf(gGlobBuf, sizeof gGlobBuf, "%d,%d,%d",
                     (int)lround(r * 255), (int)lround(v * 255), (int)lround(b * 255));
        else
            snprintf(gGlobBuf, sizeof gGlobBuf, "%d,%d,%d,%d",
                     (int)lround(r * 255), (int)lround(v * 255),
                     (int)lround(b * 255), ia);
        return gGlobBuf;
    }

    if (strcasecmp(name, "filled") == 0)
        return gShapeFilled ? "true" : "false";
    if (strcasecmp(name, "grid") == 0)
        return gGrid ? "true" : "false";
    if (strcasecmp(name, "lineSize") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", gLineWidth);
        return gGlobBuf;
    }
    if (strcasecmp(name, "pattern") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", gPattern + 1);
        return gGlobBuf;
    }
    if (strcasecmp(name, "brush") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", gBrush + 1);
        return gGlobBuf;
    }

    if (strcasecmp(name, "tool") == 0) {
        const char *n = "browse";
        switch (gTool) {
            case TOOL_BROWSE:   n = "browse";    break;
            case TOOL_BUTTON:   n = "button";    break;
            case TOOL_FIELD:    n = "field";     break;
            case TOOL_SELRECT:  n = "select";    break;
            case TOOL_LASSO:    n = "lasso";     break;
            case TOOL_PENCIL:   n = "pencil";    break;
            case TOOL_BRUSH:    n = "brush";     break;
            case TOOL_ERASER:   n = "eraser";    break;
            case TOOL_LINE:     n = "line";      break;
            case TOOL_SPRAY:    n = "spray";     break;
            case TOOL_RECT:     n = "rectangle"; break;
            case TOOL_FILL:     n = "bucket";    break;
            case TOOL_OVAL:     n = "oval";      break;
            case TOOL_FREEFORM: n = "curve";     break;
            case TOOL_TEXT:     n = "text";      break;
                
            default: break;
        }
        snprintf(gGlobBuf, sizeof gGlobBuf, "%s tool", n);
        return gGlobBuf;
    }

    if (strcasecmp(name, "clickLoc") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d,%d",
                 (int)gClickPoint.x, (int)gClickPoint.y);
        return gGlobBuf;
    }
    if (strcasecmp(name, "clickH") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", (int)gClickPoint.x);
        return gGlobBuf;
    }
    if (strcasecmp(name, "clickV") == 0) {
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d", (int)gClickPoint.y);
        return gGlobBuf;
    }
    if (strcasecmp(name, "clickLine") == 0) {
        static char buf[160];
        buf[0] = '\0';
        int line = click_line_number(gClickField, gClickPoint);
        if (gClickField && line > 0) {
            char desc[96];
            hc_describe(gClickField, desc, sizeof desc);
            snprintf(buf, sizeof buf, "line %d of %s%s", line,
                     hc_owner_is_bg(gClickField) ? "bg " : "card ", desc);
        }
        return buf;
    }
    if (strcasecmp(name, "mouseLine") == 0) {
        static char buf[160];
        buf[0] = '\0';
        NSPoint w = [[gView window] mouseLocationOutsideOfEventStream];
        NSPoint p = [gView convertPoint:w fromView:nil];
        Object *f = part_at(hc_current_card(), p);
        if (f && f->type == OBJ_FIELD) {
            int line = click_line_number(f, p);
            if (line > 0) {
                char desc[96];
                hc_describe(f, desc, sizeof desc);
                snprintf(buf, sizeof buf, "line %d of %s%s", line,
                         hc_owner_is_bg(f) ? "bg " : "card ", desc);
            }
        }
        return buf;
    }

    if (strcasecmp(name, "clickText") == 0) {
        static char buf[512];
        buf[0] = '\0';
        int line = click_line_number(gClickField, gClickPoint);
        if (gClickField && line > 0) {
            const char *t = hc_field_text(gClickField);
            int n = 1;
            const char *deb = t;
            while (n < line && (deb = strchr(deb, '\n'))) { deb++; n++; }
            if (deb) {
                const char *fin = strchr(deb, '\n');
                int len = fin ? (int)(fin - deb) : (int)strlen(deb);
                if (len > (int)sizeof buf - 1) len = (int)sizeof buf - 1;
                snprintf(buf, sizeof buf, "%.*s", len, deb);
            }
        }
        return buf;
    }
    return NULL;
}

static void cocoa_global_set(const char *name, const char *value) {
    int vrai = (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0);

    /* Le verrou d'écran du noyau. Sans cette ligne, « lock screen » ne
     * retenait rien du dessin — voir hcv_verrou_ecran. */
    if (strcasecmp(name, "lockScreen") == 0) {
        hcv_verrou_ecran(vrai ? YES : NO);
        return;
    }

    /* ═══ La couleur de peinture, depuis un script ═══════════════════════
     *
     * « set the paintColor to "vert" », « set the paintColor to "64,255,30" ».
     *
     * HyperCard n'avait pas cela : il peignait en noir sur blanc. Mais HC a
     * déjà de la couleur — le panneau des couleurs pose gInkColor, et
     * « set the textColor » colore le texte —, et un script qui ne peut pas
     * atteindre ce que la souris atteint est un manque plutôt qu'une
     * fidélité. On se contente donc d'ouvrir une porte déjà construite : les
     * fonctions de dessin lisent gInkColor depuis toujours.
     *
     * Le vocabulaire est celui du noyau, hc_color_from_name — le même que
     * « set the textColor ». Deux tables de couleurs dans le même programme,
     * ce serait la garantie qu'un jour l'une saura dire « turquoise » et pas
     * l'autre.
     *
     * Une valeur incomprise ne change rien et se signale : peindre en noir
     * parce qu'on a mal orthographié « magenta » se remarque trop tard. */
    if (hcv_quelle_couleur(name)) {
        int alpha = 255;
        int rgb = hc_color_from_name_alpha(value, &alpha);
        if (rgb == HC_COLOR_INHERIT) {
            NSLog(@"set the %s : couleur incomprise « %s »", name, value);
            return;
        }
        /* Le fond reste opaque quoi qu'on demande : un fond à moitié
         * transparent n'est pas un fond, et l'admettre rendrait « Opaque »
         * et « Transparent » du menu Paint incompréhensibles le jour où on
         * les écrira. */
        BOOL estFond = (hcv_quelle_couleur(name) < 0);
        NSColor *c = [NSColor colorWithSRGBRed:((rgb >> 16) & 0xFF) / 255.0
                                         green:((rgb >>  8) & 0xFF) / 255.0
                                          blue:( rgb        & 0xFF) / 255.0
                                         alpha:estFond ? 1.0 : alpha / 255.0];
        if (estFond) gBackColor = c;
        else         gInkColor  = c;

        /* La palette d'outils montre les deux couleurs : sans ce rafraîchis-
         * sement, elle continuerait d'afficher les anciennes. */
        hcv_palette_maj(gToolPanel);
        [gView setNeedsDisplay:YES];
        return;
    }

    if (strcasecmp(name, "filled") == 0) {
        gShapeFilled = vrai ? YES : NO;
        hcv_palette_maj(gToolPanel);
        [gView setNeedsDisplay:YES];
        return;
    }
    if (strcasecmp(name, "grid") == 0) {
        gGrid = vrai ? YES : NO;
        return;
    }
    if (strcasecmp(name, "lineSize") == 0) {
        int v = hc_coord(value, gLineWidth);
        if (v < 1) v = 1;
        if (v > 8) v = 8;
        gLineWidth = v;
        hcv_palette_maj(gWidthPanel);
        [gView setNeedsDisplay:YES];
        return;
    }
    if (strcasecmp(name, "pattern") == 0) {
        int v = hc_coord(value, gPattern + 1);
        if (v < 1) v = 1;
        if (v > NUM_PATTERNS) v = NUM_PATTERNS;
        gPattern = v - 1;
        hcv_palette_maj(gPatternPanel);
        [gView setNeedsDisplay:YES];
        return;
    }
    if (strcasecmp(name, "brush") == 0) {
        int v = hc_coord(value, gBrush + 1);
        if (v < 1) v = 1;
        if (v > NUM_BRUSHES) v = NUM_BRUSHES;
        gBrush = v - 1;
        /* Même raison qu'au clic dans la palette des brosses : le curseur du
         * pinceau porte la forme de la brosse, il est périmé dès qu'elle
         * change. « set the brush to 10 » doit changer ce qu'on a sous la
         * souris, pas seulement ce qui sortira du trait. */
        hcv_curseur_pinceau_perime();
        hcv_curseur_maj();
        hcv_palette_maj(gBrushPanel);
        [gView setNeedsDisplay:YES];
        return;
    }
    if (strcasecmp(name, "textFont") == 0) {
        NSString *nom = [NSString stringWithUTF8String:value];
        NSFont *f = [NSFont fontWithName:nom size:gTextSize];
        if (f) gTextFont = f;
        return;
    }
    if (strcasecmp(name, "textSize") == 0) {
        int v = hc_coord(value, gTextSize);
        if (v < 4)  v = 4;
        if (v > 96) v = 96;
        gTextSize = v;
        NSFont *f = [NSFont fontWithName:[text_font() fontName] size:v];
        gTextFont = f ? f : [NSFont systemFontOfSize:v];
        return;
    }
    if (strcasecmp(name, "textHeight") == 0) {
        int v = hc_entier(value, 1, HC_TEXTE_MAX, 0);
        if (v > 0) gTextHeight = v;
        return;
    }
    if (strcasecmp(name, "textStyle") == 0) {
        gTextStyleName = [NSString stringWithUTF8String:value];
        return;
    }
    if (strcasecmp(name, "textAlign") == 0) {
        gTextAlign = [NSString stringWithUTF8String:value];
        return;
    }
    if (strcasecmp(name, "cursor") == 0) {
        /* On retient le curseur RÉELLEMENT POSÉ, pas le mot demandé : « set
         * the cursor to zorglub » donne la flèche, et « the cursor » doit
         * alors rendre « arrow ». Rendre « zorglub » décrirait un état qui
         * n'existe pas.
         *
         * LE VOCABULAIRE ÉTAIT INCOMPLET. HyperCard connaît arrow, busy,
         * cross, hand, iBeam, none, plus et watch ; il en manquait trois, et
         * « set the cursor to cross » — la forme la plus courante dans les
         * piles d'époque, juste avant un tracé — donnait la flèche.
         *
         * Le drapeau gCursorScripte dit aux zones de curseur de la vue de se
         * taire : sans lui, le premier mouvement de souris au-dessus de la
         * carte reposerait le curseur de l'outil, et la demande du script
         * n'aurait vécu qu'un instant. */
        if (strcasecmp(value, "none") == 0) {
            if (!gCursorHidden) { [NSCursor hide]; gCursorHidden = YES; }
            gCursorNom = @"none";
            gCursorScripte = YES;
            gCursorScripteObj = nil;      /* caché : rien à poser */
        } else {
            if (gCursorHidden) { [NSCursor unhide]; gCursorHidden = NO; }
            gCursorScripte = YES;
            if (strcasecmp(value, "watch") == 0 || strcasecmp(value, "busy") == 0) {
                /* PAS operationNotAllowedCursor : c'est le 🚫 d'interdiction.
                 * Le script disait « je travaille », l'utilisateur lisait
                 * « c'est interdit » — deux messages opposés.
                 *
                 * macOS n'expose ni montre ni sablier public : ses curseurs
                 * d'attente sont privés. La flèche, posée ici en attendant, ne
                 * mentait sur rien mais ne disait rien non plus. On dessine
                 * donc la montre, comme le Macintosh d'origine. */
                gCursorScripteObj = hcv_curseur_montre();
                [gCursorScripteObj set];
                gCursorNom = @"watch";
            } else if (strcasecmp(value, "ibeam") == 0) {
                gCursorScripteObj = [NSCursor IBeamCursor];
                [gCursorScripteObj set];
                gCursorNom = @"ibeam";
            } else if (strcasecmp(value, "cross") == 0 ||
                       strcasecmp(value, "plus")  == 0) {
                /* HyperCard distingue la croix fine (plus) de la grande
                 * croix (cross). macOS n'a qu'un réticule, et inventer une
                 * différence que le système ne rend pas tromperait plus
                 * qu'elle n'aiderait : on rend donc le mot demandé. */
                gCursorScripteObj = [NSCursor crosshairCursor];
                [gCursorScripteObj set];
                gCursorNom = (strcasecmp(value, "plus") == 0) ? @"plus" : @"cross";
            } else if (strcasecmp(value, "hand")   == 0 ||
                       strcasecmp(value, "browse") == 0) {
                /* « browse » N'EST PAS UN NOM DE CURSEUR D'HYPERCARD : c'est
                 * un nom d'OUTIL. Le curseur, lui, s'appelle « hand », et
                 * c'est le même objet. On accepte les deux mots — celui qu'on
                 * a sous la main quand on écrit un script de navigation est
                 * rarement le bon — et « the cursor » rend « hand », le nom du
                 * curseur RÉELLEMENT posé.
                 *
                 * C'est l'inverse de cross/plus, quelques lignes plus haut, et
                 * la différence se raisonne : là, HyperCard a DEUX curseurs
                 * distincts que macOS confond, et rendre le mot demandé garde
                 * l'intention du script. Ici il n'y en a qu'un, avec un nom,
                 * et l'inventer en second n'apporterait rien.
                 *
                 * Il passe par hcv_curseur_outil plutôt que par
                 * pointingHandCursor en dur : le curseur de l'outil browse et
                 * celui de « set the cursor to browse » sont la même chose, et
                 * doivent le rester si l'un des deux change un jour. */
                gCursorScripteObj = hcv_curseur_outil(TOOL_BROWSE);
                [gCursorScripteObj set];
                gCursorNom = @"hand";
            } else {
                [[NSCursor arrowCursor] set];
                gCursorNom = @"arrow";
                /* Un mot inconnu n'est pas une demande : on rend la flèche ET
                 * la main aux zones de curseur, sinon « set the cursor to
                 * zorglub » gèlerait le pointeur sur la flèche pour de bon. */
                gCursorScripte = NO;
                gCursorScripteObj = nil;
            }
        }
        hcv_curseur_maj();
        return;
    }

    /* editBkgnd : le pendant scriptable de ⌘B. HyperCard l'avait, et une pile
     * qui pose ses objets de fond en s'ouvrant en a besoin. Le passage par
     * gEditBackground est le même que celui de l'article de menu — un seul
     * chemin pour entrer dans le fond, quelle qu'en soit la demande. */
    if (strcasecmp(name, "editBkgnd") == 0) {
        BOOL v = (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0);
        if (v != gEditBackground) {
            gEditBackground = v;
            gSelected = NULL;
            [gView endFieldEdit];
            [gView setNeedsDisplay:YES];
        }
        return;
    }
}

void hc_restore_cursor(void) {
    if (gCursorHidden) { [NSCursor unhide]; gCursorHidden = NO; }
}

static NSMutableArray *gPlaying = nil;

@interface HCSoundKeeper : NSObject <NSSoundDelegate>
@end
@implementation HCSoundKeeper
- (void)sound:(NSSound *)s didFinishPlaying:(BOOL)ok {
    (void)ok;
    [gPlaying removeObject:s];
}
@end
static HCSoundKeeper *gSoundKeeper = nil;

static void cocoa_play(const char *name) {
    NSString *n = [NSString stringWithUTF8String:name ? name : ""];

    NSSound *s = [NSSound soundNamed:n];
    if (!s) {
        for (NSString *e in @[@"aiff", @"aif", @"wav"]) {
            NSString *p = [[NSBundle mainBundle] pathForResource:n ofType:e];
            if (p) { s = [[NSSound alloc] initWithContentsOfFile:p byReference:YES]; break; }
        }
    }
    if (!s) {
        NSFileManager *fm = [NSFileManager defaultManager];
        NSArray *dirs = @[[[NSBundle mainBundle] resourcePath],
                          [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Sounds"],
                          @"/Library/Sounds",
                          @"/System/Library/Sounds"];
        for (NSString *d in dirs) {
            if (!d) continue;
            for (NSString *f in [fm contentsOfDirectoryAtPath:d error:NULL]) {
                if ([[f stringByDeletingPathExtension] caseInsensitiveCompare:n] != NSOrderedSame)
                    continue;
                s = [[NSSound alloc] initWithContentsOfFile:
                        [d stringByAppendingPathComponent:f] byReference:YES];
                if (s) break;
            }
            if (s) break;
        }
    }
    if (!s) { NSBeep(); return; }

    if (!gPlaying)     gPlaying = [[NSMutableArray alloc] init];
    if (!gSoundKeeper) gSoundKeeper = [[HCSoundKeeper alloc] init];

    s = [s copy];
    [s setDelegate:gSoundKeeper];
    [gPlaying addObject:s];
    [s play];
}

static void cocoa_idle(void) {
    /* Le drapeau du noyau s'efface à la lecture : on le reporte tout de suite
     * sur la vue, sinon l'étranglement plus bas le consommerait sans rien
     * montrer. */
    if (hc_take_visual_dirty()) hcv_invalide_tout();

    /* Au plus soixante fois par seconde. Ce qui n'est pas montré maintenant
     * reste marqué sale et partira à l'image suivante : rien ne se perd, et la
     * boucle de script n'est pas ralentie par l'affichage. */
    static CFTimeInterval prochaine = 0;
    CFTimeInterval maintenant = CACurrentMediaTime();
    if (maintenant < prochaine) return;
    /* On avance d'un pas fixe, sans dériver ; si on a pris du retard, on
     * repart de maintenant plutôt que de rattraper en rafale. */
    prochaine += 1.0 / 60.0;
    if (prochaine < maintenant) prochaine = maintenant + 1.0 / 60.0;;

    /* Un tour de boucle d'événements, et non un simple display.
     *
     * Une partie de ce qui est visible ne vit pas dans drawRect: — le
     * surlignage d'un champ est dessiné par gFieldEditor, une NSTextView
     * adossée à un CALQUE. Marquer la vue sale ne suffit pas : le contenu d'un
     * calque n'atteint l'écran qu'à la validation de la transaction, que seule
     * la boucle d'événements déclenche.
     *
     * C'est aussi ce qui rend la fenêtre réactive pendant un long script :
     * les clics et les touches sont traités au passage. beforeDate:[NSDate
     * date] veut dire « ne dors pas si la file est vide ». */
    [[gView window] displayIfNeeded];
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate date]];
}

static NSFont *text_font(void) {
    if (!gTextFont) {
        gTextFont = [NSFont fontWithName:@"Helvetica" size:gTextSize];
        if (!gTextFont) gTextFont = [NSFont systemFontOfSize:gTextSize];
    }
 
    if (!gTextStyleName || [gTextStyleName length] == 0) return gTextFont;

    NSFontTraitMask traits = 0;
    NSString *s = [gTextStyleName lowercaseString];
    if ([s rangeOfString:@"bold"].location   != NSNotFound) traits |= NSBoldFontMask;
    if ([s rangeOfString:@"italic"].location != NSNotFound) traits |= NSItalicFontMask;

    if (traits) {
        NSFont *f = [[NSFontManager sharedFontManager]
                        convertFont:gTextFont toHaveTrait:traits];
        if (f) return f;
    }
    return gTextFont;
}

static NSDictionary *text_attrs(void) {
    NSColor *c = gInkColor;
    if (!c) c = (gInk == INK_WHITE) ? [NSColor whiteColor] : [NSColor blackColor];
    NSMutableDictionary *a = [NSMutableDictionary dictionary];
    a[NSFontAttributeName] = text_font();
    a[NSForegroundColorAttributeName] = c;
    if (gTextUnderline) a[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);
    return a;
}

static void stamp_text(NSBitmapImageRep *rep, NSString *s, NSPoint pos) {
    if (!rep || [s length] == 0) return;
    NSGraphicsContext *base = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!base) return;
    CGContextRef cg = [base CGContext];
    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithCGContext:cg flipped:YES];

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:YES];

    CGFloat H = [rep pixelsHigh];
    CGContextTranslateCTM(cg, 0, H);
    CGContextScaleCTM(cg, 1, -1);

    NSDictionary *attrs = text_attrs();
    NSFont *f = [attrs objectForKey:NSFontAttributeName];
    CGFloat montee = f ? [f ascender] : 0;

    CGFloat largeur = [s sizeWithAttributes:attrs].width;
    CGFloat x = pos.x;
    if (gTextAlign) {
        NSString *al = [gTextAlign lowercaseString];
        if      ([al isEqualToString:@"center"]) x -= largeur / 2;
        else if ([al isEqualToString:@"right"])  x -= largeur;
    }

    NSPoint haut = NSMakePoint(x, pos.y - montee);
    [s drawAtPoint:haut withAttributes:attrs];

    if (gTextStyleName &&
        [[gTextStyleName lowercaseString] rangeOfString:@"bold"].location
            != NSNotFound) {
        NSFont *f2 = [attrs objectForKey:NSFontAttributeName];
        NSString *nom = f2 ? [f2 fontName] : @"";
        if ([nom rangeOfString:@"Bold"].location == NSNotFound)
            [s drawAtPoint:NSMakePoint(haut.x + 1, haut.y) withAttributes:attrs];
    }
    [NSGraphicsContext restoreGraphicsState];
}

typedef struct { const char *glyph; int kind; int value; } ToolCell;

@interface SprayPreview : NSView
@end

@implementation SprayPreview
- (void)drawRect:(NSRect)dirty {
    [[NSColor whiteColor] setFill];
    NSRectFill([self bounds]);

    int w = (int)[self bounds].size.width, h = (int)[self bounds].size.height;
    if (w < 1 || h < 1) return;

    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL pixelsWide:w pixelsHigh:h
                    bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
                   colorSpaceName:NSCalibratedRGBColorSpace
                      bytesPerRow:0 bitsPerPixel:0];
    memset([rep bitmapData], 0, (size_t)[rep bytesPerRow] * h);

    for (int pass = 0; pass < 3; pass++) {
        NSPoint a = NSMakePoint(gSprayRadius + 4, h/2);
        NSPoint b = NSMakePoint(w - gSprayRadius - 4, h/2);
        spray_stroke(rep, a, b, gSprayRadius, gSprayDensity);
    }

    [rep drawInRect:[self bounds]];
    [[NSColor grayColor] setStroke];
    NSFrameRect([self bounds]);
}
@end

@implementation HCView {
    HCDoc _doc;
}

- (void *)docState { return &_doc; }

- (Object *)rememberedCard { return _doc.card; }

- (Object *)documentCard {
    if (gDoc == &_doc) {
        Object *c = hc_current_card();
        if (c) {
            /* Le changement de carte se constate ICI, et pas ailleurs : tout
             * ce qui a besoin de la carte courante passe par cette méthode,
             * qu'on y arrive par le menu Go, par « go next » dans un script,
             * par un bouton, ou par l'ouverture d'une pile. Poser le constat
             * dans prepareForCardChange n'aurait couvert que la moitié de ces
             * chemins — et la moitié manquante aurait donné un Revert qui
             * ramène à l'état d'une visite précédente.
             *
             * Quitter la carte vaut Keep, comme dans HyperCard : ce qu'on y
             * laisse en partant est gardé. */
            if (c != _doc.card) [self forgetKeepSnapshot];
            _doc.card = c;
        }
        return c;
    }
    return _doc.card;
}

/* ═══ Le verrou d'écran, appliqué ═══════════════════════════════════════
 *
 * Tout ce qui veut redessiner la carte finit ici, d'où qu'il vienne : un
 * rappel de l'hôte, une méthode de la vue, un minuteur, ou AppKit lui-même.
 * C'est donc ici, et nulle part ailleurs, que « lock screen » retient.
 *
 * Le garde `self == gView` limite l'effet à la vue active : les autres
 * documents ouverts continuent de se rafraîchir normalement.
 *
 * Au déverrouillage, hcv_verrou_ecran remet gLockScreen à NO AVANT de
 * réinvalider — ces deux méthodes laissent alors passer, et la carte se
 * recompose une seule fois. */
- (void)setNeedsDisplay:(BOOL)flag
{
    if (flag && gLockScreen && self == gView) { gSaleTout = YES; return; }
    [super setNeedsDisplay:flag];
}

- (void)setNeedsDisplayInRect:(NSRect)r
{
    if (gLockScreen && self == gView) {
        gSaleRect  = gSaleUnPeu ? NSUnionRect(gSaleRect, r) : r;
        gSaleUnPeu = YES;
        return;
    }
    [super setNeedsDisplayInRect:r];
}

- (void)startAntsTimer {
    if (gAntsTimer) return;
    gAntsTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/15.0
                                                  target:self
                                                selector:@selector(antsTick:)
                                                userInfo:nil
                                                 repeats:YES];
}

- (void)stopAntsTimer {
    if (gAntsTimer) { [gAntsTimer invalidate]; gAntsTimer = nil; }
}

- (void)antsTick:(NSTimer *)t {
    if (!gSelRectActive && !gLassoActive) {
        [self stopAntsTimer];
        return;
    }
    gAntsPhase += 1.0;
    if (gAntsPhase >= 8.0) gAntsPhase = 0.0;
    [self setNeedsDisplay:YES];
}

- (void)updateWindowTitle {
    Object *card = [self documentCard];
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;
    const char *nm = stack->name ? stack->name : "Sans titre";
    [[self window] setTitle:[NSString stringWithUTF8String:nm]];
}

- (void)newBackground:(id)sender {
    if (hcv_menu_trappe("New Background")) return;   /* la pile détourne l'article */
    Object *card = hc_current_card();
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;

    char name[64];
    static int bgCount = 0;
    snprintf(name, sizeof name, "Fond %d", ++bgCount);
    Object *bg = hc_new_background(stack, name);
    Object *nc = hc_new_card(stack, bg, "");
    hc_set_current_card(nc);
    gSelected = NULL;
    [gView setNeedsDisplay:YES];

    /* Les deux messages, dans l'ordre d'HyperCard : le fond naît, puis la
     * première carte qui s'y appuie. Envoyés une fois la carte devenue
     * courante, sans quoi « the target » et « me » désigneraient encore la
     * carte d'où l'on vient. */
    hc_send(bg, "newBackground");
    hc_send(nc, "newCard");
}

- (BOOL)acceptsFirstResponder { return YES; }

/* ═══ Le curseur de l'outil, sur la carte et nulle part ailleurs ═══════
 *
 * AppKit demande ses zones de curseur à la vue, et les réapplique tout seul
 * chaque fois que la souris y entre. C'est ce qui fait qu'on retrouve son
 * crayon en revenant d'une palette, sans que personne ait à s'en occuper.
 *
 * Un « set the cursor » venu d'un script l'emporte : le pointeur reste alors
 * celui qu'il a demandé, y compris au-dessus de la carte, jusqu'au prochain
 * changement d'outil. Et quand le script a caché le pointeur, on ne pose
 * aucune zone du tout — en poser une le ferait réapparaître au premier
 * mouvement. */
- (void)resetCursorRects
{
    [super resetCursorRects];
    /* Caché : on ne pose rien, et [NSCursor hide] tient tout seul. */
    if (gCursorHidden) return;
    /* Un script tient le curseur : c'est LE SIEN qu'on pose, pas rien. Ne rien
     * poser laissait AppKit revenir à la flèche par défaut. */
    NSCursor *c = (gCursorScripte && gCursorScripteObj)
                ? gCursorScripteObj
                : hcv_curseur_outil((int)gTool);
    if (c) [self addCursorRect:[self visibleRect] cursor:c];
}

- (void)showPopupMenuFor:(Object *)o atPoint:(NSPoint)p {
    if (!o->contents || !*o->contents) return;
    NSArray *lines = [[NSString stringWithUTF8String:o->contents]
                      componentsSeparatedByString:@"\n"];

    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

    NSMutableDictionary *iat = [obj_attrs(o, 12, nil) mutableCopy];
    [iat removeObjectForKey:NSForegroundColorAttributeName];

    for (NSUInteger i = 0; i < [lines count]; i++) {
        NSString *t = lines[i];
        if ([t length] == 0) continue;
        NSMenuItem *it = [[NSMenuItem alloc] initWithTitle:t
                                                    action:@selector(popupChosen:)
                                             keyEquivalent:@""];
        [it setAttributedTitle:
            [[NSAttributedString alloc] initWithString:t attributes:iat]];
        [it setTarget:self];
        [it setTag:(NSInteger)(i + 1)];
        if ((int)(i + 1) == o->selectedline) [it setState:NSControlStateValueOn];
        [menu addItem:it];
    }
    gPopupTarget = o;
    NSPoint origin = NSMakePoint(o->x, o->y + o->h);
    [menu popUpMenuPositioningItem:nil
                        atLocation:origin
                            inView:self];
}

- (void)popupChosen:(id)sender {
    if (gPopupTarget) {
        gPopupTarget->selectedline = (int)[sender tag];
        hc_send(gPopupTarget, "mouseUp");
    }
    gPopupTarget = NULL;
    [self setNeedsDisplay:YES];
}

static BOOL object_selection_active(void)
{
    return (gSelected != NULL && gTool != TOOL_BROWSE) ? YES : NO;
}

/* LE CALQUE DE PEINTURE, VU DU NOYAU — pour la VALIDATION des menus.
 *
 * Le pendant de paintLayer, sans passer par une vue. Il existe parce que
 * valider un article de menu et l'exécuter ne se font pas depuis le même
 * endroit :
 *
 *   - à l'EXÉCUTION, `self` est la vue à qui l'action a été envoyée, et
 *     paintLayer est ce qu'il faut : elle tient aussi la comptabilité du
 *     changement de carte ;
 *   - à la VALIDATION, `self` est la vue que le MENU A CAPTURÉE à sa
 *     construction — AppDelegate fait « [mi setTarget:view] » avec la vue du
 *     démarrage. documentCard ne répond vraiment que pour le document ACTIF
 *     (« if (gDoc == &_doc) ») et rend sinon la carte qu'elle gardait, ou
 *     rien. D'où quatre articles grisés pour de bon : Keep, Revert, et les
 *     deux qu'on venait d'ajouter.
 *
 * Tous les autres articles de ce menu se valident sur des globales —
 * paint_selection_active, juste en dessous, lit gTool et gSelRectActive — et
 * c'est exactement pour ça qu'ils n'ont jamais eu ce défaut.
 *
 * Elle ne touche à RIEN, et c'est voulu : documentCard oublie l'instantané
 * de Keep quand la carte a changé, ce qui n'a aucune raison d'arriver
 * pendant qu'on ouvre un menu pour regarder. */
static Object *hcv_calque_courant(void)
{
    Object *card = hc_current_card();
    if (!card) return NULL;
    Object *layer = gEditBackground ? card->bg : card;
    return layer ? layer : card;
}

static BOOL paint_selection_active(void)
{
    return ((gTool == TOOL_SELRECT && gSelRectActive) ||
            (gTool == TOOL_LASSO   && gLassoActive) ||
            gFloating) ? YES : NO;
}

/* Le nombre de parts du calque qui porte cet objet — boutons et champs
 * mêlés, comme les compte hc_part_number. Le noyau les compte par type ; les
 * additionner ici évite un symbole de plus pour une addition. */
static int parts_du_calque(Object *o)
{
    if (!o || !o->owner) return 0;
    return hc_part_count(o->owner, OBJ_BUTTON) +
           hc_part_count(o->owner, OBJ_FIELD);
}

/* ═══ BRING CLOSER / SEND FARTHER ═══════════════════════════════════════
 *
 * Il n'y avait AUCUN moyen de changer l'ordre de superposition : un bouton
 * posé sous un champ y restait pour toujours. Le noyau sait déplacer une
 * part depuis « set the partNumber » ; il ne manquait que ces deux portes.
 *
 * Le sens : la part 1 est DESSOUS. drawRect parcourt parts[] en croissant,
 * donc la dernière est dessinée par-dessus, et part_at_layer teste les clics
 * à rebours pour attraper celle du dessus. Rapprocher, c'est donc monter
 * d'un rang. Les deux lectures s'accordent, et c'est ce qui fait que le
 * déplacement se voit ET se clique.
 *
 * L'ÉCRÊTAGE EST DANS LE NOYAU, pas ici : hc_set_part_number borne le rang à
 * [1, nombre de parts]. Un article de menu grisé aux extrémités et un noyau
 * qui écrête disent la même chose, mais le grisage est un CONFORT — « doMenu
 * "Bring Closer" » ne passe pas par la validation, et doit rester sans
 * effet plutôt que de déborder. */
- (void)deplaceSelectionDe:(int)pas
{
    if (!object_selection_active()) { NSBeep(); return; }
    int r = hc_part_number(gSelected);
    if (r <= 0) { NSBeep(); return; }
    hc_set_part_number(gSelected, r + pas);
    [self setNeedsDisplay:YES];
}

- (void)bringCloser:(id)sender { (void)sender; [self deplaceSelectionDe:1]; }
- (void)sendFarther:(id)sender { (void)sender; [self deplaceSelectionDe:-1]; }

- (void)copy:(id)sender {
    if (object_selection_active()) {
        if (hc_copy_part(gSelected)) return;
    }

    if (gFloating && gClipboard) {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        NSImage *img = [[NSImage alloc] initWithCGImage:[gClipboard CGImage] size:NSMakeSize(gClipW, gClipH)];
        [pb writeObjects:@[img]];
        return;
    }

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
    if (object_selection_active()) {
        if (gSelected == gEditingField) [self endFieldEdit];
        if (hc_cut_part(gSelected)) {
            gSelected = NULL;
            [gView setNeedsDisplay:YES];
            return;
        }
    }

    if (gFloating && gClipboard) {
        [self copy:sender];
        gFloating = NO;
        gFloatDragging = NO;
        [gView setNeedsDisplay:YES];
        return;
    }

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
    [gView setNeedsDisplay:YES];
}

/* ==================== cartes : couper, copier, dupliquer ====================
 *
 * Articles de menu DISTINCTS de Couper et Copier, comme dans HyperCard. Les
 * partager rendrait Cmd-C imprévisible : copy: a déjà trois sens selon le
 * contexte — objet sélectionné, sélection flottante, région de peinture — et
 * la carte en ferait un quatrième dans l'état le plus courant, outil Browse et
 * rien de sélectionné.
 *
 * Coller, lui, reste commun : il se contente de poser ce que le presse-papiers
 * contient. */

- (void)copyCard:(id)sender {
    if (hcv_menu_trappe("Copy Card")) return;   /* la pile détourne l'article */
    (void)sender;
    Object *card = hc_current_card();
    if (!card) return;
    /* Même raison : on copie ce qui est à l'écran, pas ce qui dormait dans le
     * noyau depuis le dernier enregistrement. */
    [self flushPaintToKernel];
    hc_copy_card(card);
}

- (void)cutCard:(id)sender {
    if (hcv_menu_trappe("Cut Card")) return;   /* la pile détourne l'article */
    (void)sender;
    Object *card = hc_current_card();
    if (!card) return;

    /* Lâcher ce que la vue retient de cette carte AVANT qu'elle soit libérée :
     * champ en édition, objet sélectionné, ET le cache de peinture.
     *
     * Ce dernier est indexé par POINTEUR d'objet, et flushPaintToKernel appelle
     * hc_set_paint sur chacune de ses clés. Une carte coupée mais laissée dans
     * le cache faisait donc appeler free sur un objet rendu — plantage dans
     * free, loin de sa cause.
     *
     * On vide APRÈS le flush : ce que la carte avait de peint doit d'abord
     * redescendre dans le noyau, faute de quoi la copie au presse-papiers
     * emporterait un calque périmé. */
    if (gEditingField) [self endFieldEdit];
    gSelected = NULL;
    [self flushPaintToKernel];
    [self clearPaintCache];

    if (!hc_cut_card(card)) {
        /* hc_delete_card refuse la dernière carte d'une pile. */
        NSBeep();
        return;
    }
    [gView setNeedsDisplay:YES];
}

- (void)duplicateCard:(id)sender {
    if (hcv_menu_trappe("Duplicate Card")) return;   /* la pile détourne l'article */
    (void)sender;
    Object *card = hc_current_card();
    if (!card) return;

    /* Redescendre la peinture avant de cloner : sans cela le clone emporterait
     * le calque tel qu'il était au dernier enregistrement, et non ce que l'on
     * voit à l'écran. */
    [self flushPaintToKernel];

    Object *nouvelle = hc_duplicate_card(card);
    if (!nouvelle) { NSBeep(); return; }

    hc_set_current_card(nouvelle);
    gSelected = NULL;
    hc_send(nouvelle, "newCard");
    [gView setNeedsDisplay:YES];
}

- (void)paste:(id)sender {
    /* Une carte d'abord : c'est la nature du presse-papiers qui décide, pas
     * l'outil courant. Déduire du contexte se tromperait dès qu'une carte a
     * été copiée puis l'outil Bouton choisi. */
    if (hc_clipboard_has_card()) {
        Object *card = hc_current_card();
        Object *stack = card ? card->owner : NULL;
        Object *nouvelle = stack ? hc_paste_card(stack) : NULL;
        if (nouvelle) {
            /* Coller a pu apporter des icônes de l'autre pile. Le catalogue de
             * travail doit être refait ici : hcicon_edit_bind se contente de
             * comparer les pointeurs, et cette pile étant déjà liée, il ne
             * verrait rien changer — les boutons resteraient vides jusqu'au
             * prochain changement de fenêtre. */
            hcicon_edit_sync(stack);

            hc_set_current_card(nouvelle);
            gSelected = NULL;
            hc_send(nouvelle, "newCard");
            [gView setNeedsDisplay:YES];
        } else {
            /* Seul échec possible ici : le fond de la carte copiée n'existe
             * pas dans cette pile. On le dit, plutôt que de ne rien faire. */
            NSAlert *a = [[NSAlert alloc] init];
            [a setMessageText:@"Impossible de coller cette carte"];
            [a setInformativeText:@"Son fond n'existe pas dans cette pile."];
            [a runModal];
        }
        return;
    }

    if ((gTool == TOOL_BUTTON || gTool == TOOL_FIELD) && hc_clipboard_has_part()) {
        Object *card = hc_current_card();
        if (card) {
            Object *owner = (gEditBackground && card->bg) ? card->bg : card;
            Object *p = hc_paste_part(owner);
            if (p) {
                /* LE CATALOGUE DE TRAVAIL DOIT APPRENDRE CE QUI VIENT D'ARRIVER.
                 *
                 * Coller un bouton transplante son icône dans la pile — et lui
                 * donne un NUMÉRO NEUF quand l'ancien était déjà pris par un
                 * autre dessin. HCicons ne voit pas stack->icons : il en tient
                 * une copie, refaite par hcicon_edit_sync.
                 *
                 * Sans cet appel l'icône était bel et bien dans la pile, et
                 * invisible partout : hcicon_find ne connaissait pas son
                 * nouveau numéro, donc le bouton collé ne dessinait rien et le
                 * panneau ne la listait pas. On la croyait perdue alors
                 * qu'elle était seulement ignorée.
                 *
                 * Le chemin des CARTES faisait déjà cet appel, juste au-dessus.
                 * Celui des objets l'avait oublié : l'invariant est que toute
                 * pose qui touche stack->icons doit être suivie d'un sync. */
                if (card->owner) hcicon_edit_sync(card->owner);

                gSelected = p;
                hc_send(p, p->type == OBJ_BUTTON ? "newButton" : "newField");
                [gView setNeedsDisplay:YES];
                return;
            }
        }
    }

    if (gFloating) {
        [self dropFloating];
    }

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
        [gView setNeedsDisplay:YES];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    SEL a = [item action];

    if (a == @selector(togglePalette:)) {
        [item setState:[self paletteVisibleForTag:[item tag]]
                        ? NSControlStateValueOn : NSControlStateValueOff];
        return YES;
    }

    if (a == @selector(copy:) || a == @selector(cut:))
        return object_selection_active() || paint_selection_active();

    /* Bring Closer et Send Farther n'ont de sens que sur un objet
     * sélectionné, et chacun se grise À SON EXTRÉMITÉ : la part du dessus ne
     * peut pas monter, celle du dessous ne peut pas descendre. C'est la seule
     * information vraie qu'on puisse donner d'avance, et elle évite de
     * chercher pourquoi « ça ne fait rien ».
     *
     * Aucun appel à `self` ici, contrairement aux articles de peinture :
     * gSelected est une globale, donc la vue que le MENU a capturée à sa
     * construction n'entre pas en jeu — c'est ce qui avait gardé Keep et
     * Revert grisés pour de bon. */
    if (a == @selector(bringCloser:) || a == @selector(sendFarther:)) {
        if (!object_selection_active()) return NO;
        int r = hc_part_number(gSelected);
        if (r <= 0) return NO;
        return (a == @selector(bringCloser:))
               ? (r < parts_du_calque(gSelected)) : (r > 1);
    }

    /* Les articles du menu Paint n'ont de sens que sur une sélection de
     * peinture. HyperCard les grisait de même — et c'est plus sûr que de les
     * laisser cliquables pour ne rien faire.
     *
     * Quatre font exception, parce qu'ils ne transforment pas une sélection :
     * Keep et Revert portent sur la carte entière, Select All en crée une,
     * Clear Picture la vide. Les griser faute de sélection les rendrait
     * inaccessibles au moment précis où on les cherche — juste après un
     * dessin raté, quand plus rien n'est sélectionné. Revert reste grisé tant
     * qu'il n'y a nulle part où revenir : c'est la seule information vraie
     * qu'on puisse en donner d'avance. */
    if (a == @selector(paintOp:)) {
        NSInteger t = [item tag];

        /* LES QUATRE QUI NE DEMANDENT PAS DE SÉLECTION. Les griser faute de
         * sélection rendrait « Select All » impossible à atteindre — il est
         * fait pour en CRÉER une — et « Clear Picture » inutilisable au
         * moment où l'on veut vider un calque entier.
         *
         * TOUTES CES VALIDATIONS PASSENT PAR hcv_calque_courant, et pas par
         * [self paintLayer] : `self` est ici la vue que le MENU a capturée à
         * sa construction, pas la vue active. C'est ce qui gardait Keep,
         * Revert, Select All et Clear Picture grisés pour de bon. Le
         * pourquoi est en tête de hcv_calque_courant. */
        if (t == HCV_PAINT_SELECTALL || t == HCV_PAINT_CLEAR)
            return hcv_calque_courant() != NULL;

        /* Opaque et Transparent portent une COCHE : c'est un mode, pas une
         * action, et l'utilisateur doit voir lequel des deux est en cours.
         * La palette des outils montre déjà le même état ; les deux lisent
         * la même variable, donc elles ne peuvent pas se contredire. */
        if (t == HCV_PAINT_OPAQUE || t == HCV_PAINT_TRANSPARENT) {
            BOOL coche = (t == HCV_PAINT_TRANSPARENT) ? gTransparentBg
                                                      : !gTransparentBg;
            [item setState:coche ? NSControlStateValueOn
                                 : NSControlStateValueOff];
            return YES;
        }

        /* La grille porte une COCHE et ne demande AUCUNE sélection : c'est un
         * réglage de dessin, pas une transformation. La griser faute de
         * sélection la rendrait inatteignable au moment précis où on
         * l'allume — avant de dessiner. */
        if (t == HCV_PAINT_GRID) {
            [item setState:gGrid ? NSControlStateValueOn
                                 : NSControlStateValueOff];
            return YES;
        }

        /* FatBits porte sa coche comme la grille, mais se GRISE sous un outil
         * qui ne peint pas : là il n'a aucun effet, et un article cochable
         * qui ne fait rien est pire qu'un article grisé. C'est la même règle
         * que partout ailleurs dans ce menu — ne proposer que ce qui agira. */
        if (t == HCV_PAINT_FATBITS) {
            [item setState:gFatBits ? NSControlStateValueOn
                                    : NSControlStateValueOff];
            return hcv_outil_peint();
        }

        if (t == HCV_PAINT_KEEP) return hcv_calque_courant() != NULL;
        if (t == HCV_PAINT_REVERT) {
            /* paintLayer d'abord : il passe par documentCard, qui peut oublier
             * l'instantané si la carte a changé depuis. Lire gKeepSnap avant
             * lui donnerait un article actif pointant sur un instantané que le
             * clic suivant aurait déjà jeté. */
            Object *l = hcv_calque_courant();
            return l != NULL && gKeepSnap != nil && gKeepLayer == l;
        }
        return paint_selection_active();
    }
    /* « Icône… » reste TOUJOURS disponible, même sans bouton sélectionné : le
     * panneau gère les icônes de la pile, qui sont des ressources. On peut donc
     * en créer et en dessiner avec n'importe quel outil ; faute de bouton, OK
     * referme sans rien attribuer. */
    if (a == @selector(paste:)) {
        /* Une carte se colle quel que soit l'outil : c'est la nature du
         * presse-papiers qui commande, comme dans paste: lui-même. Sans cette
         * branche, l'article restait grisé après « Copier la carte » — la
         * validation n'autorisait que les outils Bouton et Champ. */
        if (hc_clipboard_has_card())
            return YES;
        if ((gTool == TOOL_BUTTON || gTool == TOOL_FIELD) && hc_clipboard_has_part())
            return YES;
        return gClipboard != nil ||
               [[NSPasteboard generalPasteboard] canReadObjectForClasses:@[[NSImage class]] options:nil];
    }
    return YES;
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self setNeedsDisplay:YES];
}

- (void)print:(id)sender {
    (void)sender;
    
    Object *card = [self documentCard];
     
    if (!card) return;
    Object *tab[1] = { card };
    cocoa_print_cards(tab, 1);
}

- (void)changeColor:(id)sender {
     
    NSColorPanel *panneau = (NSColorPanel *)sender;
    if (![panneau respondsToSelector:@selector(color)]) return;
    NSColor *c = [panneau color];

    if (gColorTarget) {
        if (gColorTarget == 1) gInkColor  = c;
        else                   gBackColor = c;
        [(NSView *)[gToolPanel contentView] display];
        [gView setNeedsDisplay:YES];
        return;
    }

    if (gEditingField && gFieldEditor) {
        NSRange r = [gFieldEditor selectedRange];
        if (r.length > 0) {
            [[gFieldEditor textStorage]
                addAttribute:NSForegroundColorAttributeName value:c range:r];
        } else {
            NSMutableDictionary *ta =
                [[gFieldEditor typingAttributes] mutableCopy];
            ta[NSForegroundColorAttributeName] = c;
            [gFieldEditor setTypingAttributes:ta];
        }
        [gView setNeedsDisplay:YES];
        return;
    }

    gInkColor = c;
    [gView setNeedsDisplay:YES];
}

- (void)underline:(id)sender {
    gTextUnderline = !gTextUnderline;
    [gView setNeedsDisplay:YES];
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
    Object *tgt = gFontTarget ? gFontTarget
                    : (gEditingField ? gEditingField
                    : ((gSelected && gTool != TOOL_BROWSE) ? gSelected : NULL));

    NSFont *nf = [sender convertFont:(tgt ? obj_font(tgt, 12) : text_font())];

    if (tgt) {
            NSFontManager *fm = [NSFontManager sharedFontManager];
            NSFontTraitMask tr = [fm traitsOfFont:nf];

            NSFontDescriptor *pd =
                [[nf fontDescriptor] fontDescriptorWithSymbolicTraits:
                    [[nf fontDescriptor] symbolicTraits] &
                    ~(NSFontDescriptorTraitBold | NSFontDescriptorTraitItalic)];
            NSFont *plain = [NSFont fontWithDescriptor:pd size:[nf pointSize]];
            if (!plain) {
                plain = nf;
                if (tr & NSBoldFontMask)
                    plain = [fm convertFont:plain toNotHaveTrait:NSBoldFontMask];
                if (tr & NSItalicFontMask)
                    plain = [fm convertFont:plain toNotHaveTrait:NSItalicFontMask];
            }
            if (!plain) plain = nf;

            free(tgt->textfont);
            NSString *fam = [plain familyName];
            if (!fam || [fam hasPrefix:@"."]) {
                tgt->textfont = NULL;
            } else {
                tgt->textfont = strdup([fam UTF8String]);
            }
            tgt->textsize = (int)[nf pointSize];

            int st = tgt->textstyle & ~(HC_BOLD | HC_ITALIC);
            if (tr & NSBoldFontMask)   st |= HC_BOLD;
            if (tr & NSItalicFontMask) st |= HC_ITALIC;
            tgt->textstyle = st;

            if (tgt == gEditingField && gFieldEditor)
                [gFieldEditor setFont:obj_font(tgt, 12)];

            [fm setSelectedFont:obj_font(tgt, 12) isMultiple:NO];
            hc_sync_size_field(tgt);
        } else {
        gTextFont = nf;
        gTextSize = (int)[nf pointSize];
    }
    [[NSFontManager sharedFontManager] setSelectedFont:nf isMultiple:NO];
    [gView setNeedsDisplay:YES];
}

static int gColorTarget = 0;

- (void)showDrawColorPanel:(BOOL)ink {
    gColorTarget = ink ? 1 : 2;
    NSColorPanel *p = [NSColorPanel sharedColorPanel];
    [p setColor:(ink ? gInkColor : gBackColor)];
    [[self window] makeFirstResponder:self];
    [p orderFront:nil];
}

- (void)changeAttributes:(id)sender {
    [gView setNeedsDisplay:YES];
}

- (NSFontPanelModeMask)validModesForFontPanel:(NSFontPanel *)fontPanel {
    return NSFontPanelModesMaskStandardModes;
}

- (void)ditherSelection:(id)sender {
    if (hcv_menu_trappe("Dither Selection")) return;   /* la pile détourne l'article */
    if (gFloating && gClipboard) {
        dither_region(gClipboard, 0, 0, gClipW-1, gClipH-1, NULL, 0);
        [gView setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (gSelRectActive) {
        dither_region(rep, (int)MIN(gSelStart.x,gSelEnd.x), (int)MIN(gSelStart.y,gSelEnd.y),
                           (int)MAX(gSelStart.x,gSelEnd.x), (int)MAX(gSelStart.y,gSelEnd.y),
                           NULL, 0);
        [gView setNeedsDisplay:YES];
        return;
    }
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
        [gView setNeedsDisplay:YES];
        return;
    }
    dither_region(rep, 0, 0, (int)[rep pixelsWide]-1, (int)[rep pixelsHigh]-1, NULL, 0);
    [gView setNeedsDisplay:YES];
}

- (void)popupFlashTick:(NSTimer *)timer {
    if (!gPopupTarget || gPopupChosenRow < 0) { close_popup_menu(); return; }
    gPopupFlashInverted = !gPopupFlashInverted;
    if (--gPopupFlashToggles <= 0) {
        NSInteger row = gPopupChosenRow;
        [timer invalidate];
        gPopupFlashTimer = nil;
        choose_popup_row(self, row);
        return;
    }
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)event {
    unichar key = [[event charactersIgnoringModifiers] characterAtIndex:0];
    NSUInteger mods = [event modifierFlags];
    BOOL cmd = (mods & NSEventModifierFlagCommand) != 0;

    if (gPopupTarget && !cmd) {
        if (gPopupFlashTimer) return;
        NSInteger count = gPopupItems.count;
        if (key == 27) { close_popup_menu(); [self setNeedsDisplay:YES]; return; }
        if (key == NSUpArrowFunctionKey) {
            NSInteger row = gPopupKeyboardRow < 0 ? count - 1 : gPopupKeyboardRow - 1;
            while (row >= 0 && !popup_row_is_enabled(row)) row--;
            if (row >= 0) { gPopupKeyboardRow = row; [self setNeedsDisplay:YES]; }
            return;
        }
        if (key == NSDownArrowFunctionKey) {
            NSInteger row = gPopupKeyboardRow < 0 ? 0 : gPopupKeyboardRow + 1;
            while (row < count && !popup_row_is_enabled(row)) row++;
            if (row < count) { gPopupKeyboardRow = row; [self setNeedsDisplay:YES]; }
            return;
        }
        if ((key == NSEnterCharacter || key == NSCarriageReturnCharacter) &&
            popup_row_is_enabled(gPopupKeyboardRow)) {
            flash_popup_selection(self, gPopupKeyboardRow); return;
        }
        return;
    }

    if (gFloating && !cmd) {
        if (key == NSEnterCharacter || key == NSCarriageReturnCharacter) {
            [self dropFloating];
            return;
        }
        /* Échap ou Suppression abandonnent la zone flottante.
         *
         * Elle n'est ni gSelRectActive ni gLassoActive — ces drapeaux ont été
         * baissés au soulèvement — donc les cas de suppression plus bas ne la
         * voyaient pas, et la touche restait sans effet.
         *
         * Le geste sert surtout après un Option-glisser : on duplique, on
         * change d'avis, et l'on jette la copie sans avoir à la déposer puis
         * à la resélectionner. Après un glissement ordinaire, le calque a
         * déjà été effacé au soulèvement : abandonner supprime alors vraiment
         * la zone, ce qui est bien ce qu'on attend de Suppression. */
        if (key == 27 || key == NSDeleteCharacter || key == NSDeleteFunctionKey) {
            gFloating = NO;
            gFloatDragging = NO;
            [self stopAntsTimer];
            [self setNeedsDisplay:YES];
            return;
        }
    }

    if (gTool == TOOL_TEXT && gTextActive && !cmd) {
        if (key == 27) {
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
        [self stopAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gTool == TOOL_SELRECT && gSelRectActive) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                    (int)[self bounds].size.height);
        erase_rect(rep, gSelStart, gSelEnd);
        gSelRectActive = NO;
        [self stopAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gSelected && gTool != TOOL_BROWSE) {
        hc_delete_part(gSelected);
        gSelected = NULL;
        [self setNeedsDisplay:YES];
        return;
    }

    /* En dernier, une fois écartés les modes propres à HC — menu déroulant,
     * zone flottante, outil Texte, suppressions. Ceux-là sont à nous et
     * gardent la main ; tout le reste appartient à la pile, qui peut
     * maintenant nommer la touche. */
    if ([self envoieToucheHyperCard:event]) return;

    [super keyDown:event];
}

- (void)inkChosen:(id)sender {
    gInk = (HCInk)[sender tag];
     
}

- (BOOL)isFlipped { return YES; }

- (void)applyStackSize {
    Object *card = [self documentCard];
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;
    int w = stack->w > 0 ? stack->w : 512;
    int h = stack->h > 0 ? stack->h : 342;

    NSWindow *win = [self window];
    if (!win) return;
    NSRect frame = [win frame];
    NSRect content = NSMakeRect(0, 0, w, h);
    NSRect newFrame = [win frameRectForContentRect:content];
    newFrame.origin = frame.origin;
    newFrame.origin.y = frame.origin.y + frame.size.height - newFrame.size.height;
    [win setFrame:newFrame display:YES animate:NO];
    [self updateWindowTitle];
    [self setNeedsDisplay:YES];
}

- (void)installWidthPalette {
    int cols = 4, rows = 3;
    CGFloat cell = 40, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;
    gWidthPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(200, 150, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered defer:NO];
    [gWidthPanel setTitle:@"Line Size"];
    [gWidthPanel setFloatingPanel:YES];
    [gWidthPanel setBecomesKeyOnlyIfNeeded:YES];
    [gWidthPanel setHidesOnDeactivate:YES];
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
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered defer:NO];
    [gBrushPanel setTitle:@"Brushes"];
    [gBrushPanel setFloatingPanel:YES];
    [gBrushPanel setBecomesKeyOnlyIfNeeded:YES];
    [gBrushPanel setHidesOnDeactivate:YES];
    [gBrushPanel setReleasedWhenClosed:NO];
    BrushPalette *grid = [[BrushPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gBrushPanel setContentView:grid];
    [gBrushPanel makeKeyAndOrderFront:nil];
}

/* ═══ Les articles du menu Paint ════════════════════════════════════════
 *
 * Une seule action pour les huit : l'étiquette de l'article dit laquelle.
 * Toutes suivent le même chemin — trouver la zone, prendre l'instantané
 * d'annulation, transformer, redessiner — et l'écrire huit fois serait huit
 * occasions d'en oublier un morceau.
 *
 * La ZONE, c'est la sélection : le polygone du lasso s'il est actif, sinon le
 * rectangle des fourmis. Sans sélection il ne se passe rien, et
 * validateMenuItem: grise les articles pour le dire d'avance — c'est ce que
 * faisait HyperCard, et c'est plus sûr que d'appliquer à toute la carte une
 * transformation qu'on ne pourrait plus distinguer d'une fausse manœuvre. */
/* Y a-t-il de quoi travailler ? Rend NO sans rien toucher si non. */
static BOOL hcv_zone_peinture(int *x0, int *y0, int *x1, int *y1,
                              NSPoint **poly, int *npoly)
{
    *poly = NULL; *npoly = 0;

    if (gLassoActive && gLassoCount >= 3) {
        double minx = gLassoPts[0].x, maxx = minx;
        double miny = gLassoPts[0].y, maxy = miny;
        for (int i = 1; i < gLassoCount; i++) {
            if (gLassoPts[i].x < minx) minx = gLassoPts[i].x;
            if (gLassoPts[i].x > maxx) maxx = gLassoPts[i].x;
            if (gLassoPts[i].y < miny) miny = gLassoPts[i].y;
            if (gLassoPts[i].y > maxy) maxy = gLassoPts[i].y;
        }
        *x0 = (int)floor(minx); *y0 = (int)floor(miny);
        *x1 = (int)ceil(maxx);  *y1 = (int)ceil(maxy);
        *poly = gLassoPts; *npoly = gLassoCount;
        return YES;
    }
    if (gSelRectActive) {
        *x0 = (int)MIN(gSelStart.x, gSelEnd.x);
        *x1 = (int)MAX(gSelStart.x, gSelEnd.x);
        *y0 = (int)MIN(gSelStart.y, gSelEnd.y);
        *y1 = (int)MAX(gSelStart.y, gSelEnd.y);
        return YES;
    }
    return NO;
}

- (void)paintOp:(id)sender
{
    /* Un script peut détourner l'article, comme pour les autres menus :
     *
     *     on doMenu what
     *       if what is "Revert" then answer "Pas sur cette carte."
     *       else pass doMenu
     *     end doMenu
     *
     * Le détournement se fait ICI et pas dans paintOpTag : celui-ci est aussi
     * le chemin qu'emprunte « doMenu "Invert" » venu d'un script, et l'y
     * mettre ferait rappeler le gestionnaire par lui-même. gDansDoMenu couvre
     * déjà ce cas, mais un seul garde vaut mieux que deux qui doivent
     * s'accorder. */
    if (hcv_menu_trappe([[sender title] UTF8String])) return;
    [self paintOpTag:[sender tag]];
}

/* Séparée de l'action pour que « doMenu "Invert" » y arrive aussi : un script
 * n'a pas d'article de menu à envoyer, donc pas d'étiquette à lire. */
- (void)paintOpTag:(NSInteger)quoi
{
    /* Keep et Revert portent sur la carte entière, pas sur une sélection : ils
     * passent donc AVANT qu'on en cherche une. Les exiger derrière le test de
     * zone les rendrait inutilisables au moment où on en a le plus besoin —
     * juste après un dessin raté, quand plus rien n'est sélectionné. */
    if (quoi == HCV_PAINT_KEEP)   { [self keepPaint];   return; }
    if (quoi == HCV_PAINT_REVERT) { [self revertPaint]; return; }

    /* SELECT ALL, CLEAR PICTURE, OPAQUE ET TRANSPARENT, pour la même raison :
     * aucun ne transforme une sélection. Les deux premiers en FONT une ou la
     * vident, les deux derniers changent le mode de dessin.
     *
     * Le corps de Select All et de Clear Picture vivait dans cocoa_do_menu,
     * qui n'est appelé que par « doMenu ». Les scripts les avaient donc, la
     * SOURIS non : aucun article de menu ne les servait. Les remonter ici
     * donne une seule mise en œuvre et trois portes — l'article de menu, le
     * script, et la palette pour la transparence — qui ne peuvent plus
     * diverger. C'est la règle qu'on s'applique partout ailleurs. */
    if (quoi == HCV_PAINT_SELECTALL) {
        NSRect b = [self bounds];
        gSelStart = NSMakePoint(0, 0);
        gSelEnd   = NSMakePoint(b.size.width, b.size.height);
        gSelRectActive = YES;
        gLassoActive   = NO;
        gLassoCount    = 0;
        [self startAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

    if (quoi == HCV_PAINT_CLEAR) {
        /* La SÉLECTION d'abord, tout le calque seulement s'il n'y en a pas.
         * C'est ce que fait HyperCard, et c'est la seule lecture qui ne
         * détruise pas plus que ce qu'on montrait à l'écran. */
        if (gTool == TOOL_SELRECT && gSelRectActive) {
            Object *card = [self documentCard];
            if (!card) return;
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep =
                paint_bitmap(layer, (int)[self bounds].size.width,
                                    (int)[self bounds].size.height);
            if (!rep) return;
            [self beginPaintUndo];
            erase_rect(rep, gSelStart, gSelEnd);
            gSelRectActive = NO;
            [self stopAntsTimer];
            [self setNeedsDisplay:YES];
            return;
        }
        [self eraseAll];
        return;
    }

    /* Le fond des formes et du texte : opaque le recouvre, transparent le
     * laisse voir. La palette des trames portait déjà cette bascule ; elle
     * n'était ni dans un menu ni atteignable par script. On POSE la valeur
     * au lieu de la basculer — « doMenu "Transparent" » doit rendre le
     * dessin transparent, pas l'inverser : un script qui l'appelle deux fois
     * ne doit pas défaire son propre travail. */
    /* LA GRILLE BASCULE, elle ne se pose pas : l'article de menu d'HyperCard
     * est une coche, et « doMenu "Grid" » fait exactement ce que ferait le
     * clic. Pour la POSER à une valeur précise il y a « set the grid to true »,
     * qui passe par cocoa_global_set — deux portes, deux gestes distincts, et
     * le même état derrière. */
    if (quoi == HCV_PAINT_GRID) {
        gGrid = !gGrid;
        return;
    }

    /* FATBITS. On CENTRE à l'allumage sur le dernier point peint : sans cela
     * on tomberait sur le coin supérieur gauche du calque, qui n'est presque
     * jamais ce qu'on était en train de regarder — et il faudrait ⌘-glisser
     * jusqu'à son ouvrage avant de pouvoir y toucher. */
    if (quoi == HCV_PAINT_FATBITS) {
        gFatBits = !gFatBits;
        if (gFatBits) hcv_fat_centre(gFatDernier, [gView bounds]);
        [gView setNeedsDisplay:YES];
        return;
    }

    if (quoi == HCV_PAINT_OPAQUE || quoi == HCV_PAINT_TRANSPARENT) {
        gTransparentBg = (quoi == HCV_PAINT_TRANSPARENT);
        [self setNeedsDisplay:YES];
        /* La palette des OUTILS montre cet état — c'est elle qui porte la
         * case, pas celle des trames. Sans ce rafraîchissement elle
         * resterait à l'ancienne valeur, et les deux se contrediraient sous
         * les yeux de l'utilisateur. */
        hcv_palette_maj(gToolPanel);
        return;
    }

    int x0, y0, x1, y1, npoly;
    NSPoint *poly;
    if (!hcv_zone_peinture(&x0, &y0, &x1, &y1, &poly, &npoly)) { NSBeep(); return; }

    Object *card = [self documentCard];
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;

    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (!rep) return;

    [self beginPaintUndo];

    switch (quoi) {
        case HCV_PAINT_INVERT:  paint_invert(rep, x0,y0,x1,y1, poly,npoly);      break;
        case HCV_PAINT_DARKEN:  paint_darken(rep, x0,y0,x1,y1, poly,npoly);      break;
        case HCV_PAINT_LIGHTEN: paint_lighten(rep, x0,y0,x1,y1, poly,npoly);     break;
        case HCV_PAINT_TRACE:   paint_trace_edges(rep, x0,y0,x1,y1, poly,npoly); break;
        case HCV_PAINT_FLIPH:   paint_flip(rep, x0,y0,x1,y1, 1);                 break;
        case HCV_PAINT_FLIPV:   paint_flip(rep, x0,y0,x1,y1, 0);                 break;

        /* Fill remplit la sélection de la trame courante, à l'encre courante,
         * et respecte « fond transparent » comme les formes pleines : c'est le
         * seul article du menu qui peint au lieu de transformer.
         *
         * L'encre EFFACE est écartée : remplir avec elle laisserait la zone
         * vide, ce que fait déjà la gomme, et on ne l'aurait su qu'après. */
        case HCV_PAINT_FILL:
            if (gInk == INK_ERASE) { NSBeep(); return; }
            paint_fill_zone(rep, x0,y0,x1,y1, poly,npoly);
            break;

        /* La rotation change la FORME de la sélection dès qu'elle n'est pas
         * carrée : ses côtés s'échangent. On y déplace les fourmis, sans quoi
         * elles entoureraient une zone qui n'a plus rien à voir avec l'image.
         *
         * Un lasso devient alors un simple rectangle : son polygone ne
         * survivrait pas à la rotation, et prétendre le contraire donnerait
         * une sélection qui ne recouvre plus ce qu'elle désigne. */
        case HCV_PAINT_ROTL:
        case HCV_PAINT_ROTR: {
            NSRect neuf = NSZeroRect;
            paint_rotate(rep, x0,y0,x1,y1,
                         quoi == HCV_PAINT_ROTR ? +1 : -1, &neuf);
            gLassoActive = NO;
            gLassoCount  = 0;
            gSelStart = NSMakePoint(NSMinX(neuf), NSMinY(neuf));
            gSelEnd   = NSMakePoint(NSMaxX(neuf), NSMaxY(neuf));
            gSelRectActive = YES;
            [self startAntsTimer];
            break;
        }
        default: return;
    }

    [self setNeedsDisplay:YES];
}

- (void)eraseAll {
    Object *card = [self documentCard];
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;

    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (!rep) return;

    [self beginPaintUndo];

    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    CGContextClearRect([ctx CGContext],
                       CGRectMake(0, 0, [rep pixelsWide], [rep pixelsHigh]));
    [NSGraphicsContext restoreGraphicsState];

    [self setNeedsDisplay:YES];
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
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [gPatternPanel setTitle:@"Patterns"];
    [gPatternPanel setFloatingPanel:YES];
    [gPatternPanel setBecomesKeyOnlyIfNeeded:YES];
    [gPatternPanel setHidesOnDeactivate:YES];
    PatternPalette *grid = [[PatternPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gPatternPanel setContentView:grid];
    [gPatternPanel makeKeyAndOrderFront:nil];
}

/* Ne composer que la portion du calque qui recoupe la zone sale.
 *
 * Composer le calque ENTIER à chaque redessin coûtait une vingtaine de
 * millisecondes, deux fois — celui du fond et celui de la carte. Sur un tracé
 * de plusieurs milliers de segments, c'est là que passait tout le temps, et
 * c'est pourquoi la pile d'origine paraissait plus vive sous Basilisk :
 * QuickDraw ne rafraîchissait que les pixels touchés.
 *
 * La vue est retournée, origine en haut ; un NSBitmapImageRep ne l'est pas.
 * Le rectangle source se déduit donc par symétrie verticale. */
static void draw_layer_dirty(NSBitmapImageRep *rep, NSRect sale) {
    if (!rep) return;
    CGFloat W = [rep pixelsWide], H = [rep pixelsHigh];
    NSRect dest = NSIntersectionRect(NSIntegralRect(sale), NSMakeRect(0, 0, W, H));
    if (NSIsEmptyRect(dest)) return;
    NSRect src = NSMakeRect(dest.origin.x,
                            H - (dest.origin.y + dest.size.height),
                            dest.size.width, dest.size.height);
    [rep drawInRect:dest fromRect:src
          operation:NSCompositingOperationSourceOver fraction:1.0
     respectFlipped:YES hints:nil];
}

/* Le dessin de la carte, en coordonnées de CALQUE.
 *
 * Séparé de drawRect: pour FatBits : ce corps a plusieurs `return`, et un
 * état graphique sauvé avant eux se perdrait sur l'un des chemins. Le
 * wrapper restaure toujours, quel que soit le chemin pris ici. */
- (void)drawCardContent:(NSRect)dirtyRect {
    /* Lier le catalogue d'icônes à la pile de CETTE fenêtre, avant tout dessin.
     *
     * HCicons ne retient qu'une copie de travail, alors que plusieurs piles
     * peuvent être ouvertes en même temps : c'est donc la fenêtre en train de
     * se dessiner qui doit imposer la sienne. Lier une fois au chargement ne
     * suffirait pas — la seconde pile ouverte écraserait le catalogue de la
     * première, qui afficherait alors de mauvaises icônes.
     *
     * Sans travail quand c'est déjà la bonne pile, donc gratuit au redessin. */
    {
        /* UN REDESSIN NE DOIT JAMAIS VIDER LE CATALOGUE.
         *
         * On passait NULL dès que documentCard ne rendait rien, et
         * hcicon_edit_bind(NULL) appelle hcicon_edit_sync(NULL), qui EFFACE la
         * copie de travail : il ne reste alors que les icônes d'origine.
         *
         * Le panneau Icônes ouvert par le menu le déclenchait. Il devient la
         * fenêtre active, la fenêtre de carte se redessine, et si ce redessin
         * trouve documentCard à NULL — vue qui n'est pas le document courant,
         * ou pile dont la carte n'est pas encore posée — les icônes de la pile
         * disparaissaient de la grille sous les yeux de l'utilisatrice. Par
         * l'autre porte, le bouton « Icon… » du panneau d'information, la
         * fenêtre de carte n'est pas réactivée de la même façon : le défaut ne
         * se voyait pas, ce qui est exactement ce qui le rendait difficile à
         * situer.
         *
         * Rompre le lien est une décision, pas un effet de bord d'un dessin :
         * Hcdocument le fait explicitement par hcicon_edit_sync(NULL) quand
         * une pile se ferme, et c'est le seul endroit qui doive le faire. Un
         * dessin qui ne sait pas quelle pile montrer ne sait rien — il ne
         * décide donc rien, et laisse le catalogue tel quel. */
        Object *dc = [self documentCard];
        if (dc && dc->owner) hcicon_edit_bind(dc->owner);
    }

    if (visual_pending()) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self runVisualTransition];
        });
    }

    if ([self drawVisualStep]) return;

    [[NSColor whiteColor] setFill];
    NSRectFill(dirtyRect);

    Object *card = [self documentCard];
    if (!card) return;

    NSRect b = [self bounds];

    if (card->bg) {
        draw_layer_dirty(paint_bitmap(card->bg, (int)b.size.width, (int)b.size.height),
                         dirtyRect);
        for (int i = 0; i < card->bg->nparts; i++)
            if (part_touche(card->bg->parts[i], dirtyRect))
                draw_part(card->bg->parts[i]);
    }

    /* La PEINTURE de la carte disparaît en édition de fond, toujours : c'est
     * ce qu'on demande à ⌘B, voir le fond seul et pouvoir y dessiner. */
    if (!gEditBackground)
        draw_layer_dirty(paint_bitmap(card, (int)b.size.width, (int)b.size.height),
                         dirtyRect);

    /* Ses OBJETS, eux, restent visibles quand un outil d'objet est en main,
     * puisque cet outil peut les attraper. Même question, même réponse que
     * part_at : couche_carte_visible() répond aux deux. */
    if (couche_carte_visible())
        for (int i = 0; i < card->nparts; i++)
            if (part_touche(card->parts[i], dirtyRect))
                draw_part(card->parts[i]);

    if (gSelected) {
        NSRect r = NSMakeRect(gSelected->x, gSelected->y, gSelected->w, gSelected->h);
        /* Ocre pour un objet du FOND, rouge pour un objet de la carte.
         *
         * Depuis que les outils Bouton et Champ atteignent les deux calques,
         * on peut tenir un objet de fond sans être en édition de fond — et le
         * déplacer, le redimensionner ou le supprimer le fait alors sur TOUTES
         * les cartes de ce fond. Cette portée-là doit se voir avant le geste,
         * pas se découvrir après. L'ocre est déjà la couleur du cadre
         * d'édition de fond, juste en dessous : c'est le même mot dans le même
         * vocabulaire, et pas un code de plus à retenir. */
        NSColor *teinte = hc_owner_is_bg(gSelected)
            ? [NSColor colorWithRed:0.6 green:0.4 blue:0.2 alpha:1.0]
            : [NSColor redColor];
        [teinte setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:NSInsetRect(r, -2, -2)];
        [path setLineWidth:2];
        [path stroke];
        [teinte setFill];
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

    if (gDragging) {
        [[NSColor blueColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:gDragRect];
        [path setLineWidth:1];
        CGFloat dash[] = {4, 3};
        [path setLineDash:dash count:2 phase:0];
        [path stroke];
    }

    if (gEditBackground) {
        [[NSColor colorWithRed:0.6 green:0.4 blue:0.2 alpha:1.0] setStroke];
        NSBezierPath *frame = [NSBezierPath bezierPathWithRect:NSInsetRect(b, 4, 4)];
        [frame setLineWidth:4];
        CGFloat dash[] = {10, 5};
        [frame setLineDash:dash count:2 phase:0];
        [frame stroke];
    }

    /* Aperçu dynamique des formes géométriques */
    if (gShapeDrawing) {
        BOOL optionDown = ([NSEvent modifierFlags] & NSEventModifierFlagOption) != 0;
        NSRect box = compute_shape_rect(gShapeStart, gShapeEnd, optionDown);

        [[NSColor blueColor] setStroke];
        NSBezierPath *preview = nil;
        if (gTool == TOOL_LINE) {
            preview = [NSBezierPath bezierPath];
            if (optionDown) {
                NSPoint opposite = NSMakePoint(2 * gShapeStart.x - gShapeEnd.x,
                                               2 * gShapeStart.y - gShapeEnd.y);
                [preview moveToPoint:opposite];
            } else {
                [preview moveToPoint:gShapeStart];
            }
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

    /* Lasso avec animation fourmis de feu */
    if ((gLassoDrawing || gLassoActive) && gLassoCount > 1) {
        [[NSColor blackColor] setStroke];
        NSBezierPath *pv = [NSBezierPath bezierPath];
        [pv moveToPoint:gLassoPts[0]];
        for (int i = 1; i < gLassoCount; i++) [pv lineToPoint:gLassoPts[i]];
        if (gLassoActive) [pv closePath];
        [pv setLineWidth:1];
        CGFloat dash[] = {4, 4};
        [pv setLineDash:dash count:2 phase:gAntsPhase];
        [pv stroke];
    }

    /* Sélection rectangulaire avec animation fourmis de feu */
    if (gSelRectDrawing || gSelRectActive) {
        NSRect sel = NSMakeRect(MIN(gSelStart.x,gSelEnd.x), MIN(gSelStart.y,gSelEnd.y),
                                fabs(gSelEnd.x-gSelStart.x), fabs(gSelEnd.y-gSelStart.y));
        [[NSColor blackColor] setStroke];
        NSBezierPath *pv = [NSBezierPath bezierPathWithRect:sel];
        [pv setLineWidth:1];
        CGFloat dash[] = {4, 4};
        [pv setLineDash:dash count:2 phase:gAntsPhase];
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
        CGFloat dash[] = {4, 4};
        [fp setLineDash:dash count:2 phase:gAntsPhase];
        [fp stroke];
    }

    if (gTextActive && gTextBuf) {
        NSDictionary *at = text_attrs();

        /* L'aperçu doit se placer comme la GRAVURE : à la ligne de base.
         * stamp_text a été corrigée pour poser le texte sur gTextPos, cet
         * aperçu non — d'où un texte affiché une ascendante trop haut pendant
         * la frappe, qui redescendait d'un coup à la validation. */
        NSFont *fp = [at objectForKey:NSFontAttributeName];
        CGFloat montee = fp ? [fp ascender] : 0;
        NSPoint haut = NSMakePoint(gTextPos.x, gTextPos.y - montee);

        [gTextBuf drawAtPoint:haut withAttributes:at];

        NSArray *lines = [gTextBuf componentsSeparatedByString:@"\n"];
        NSString *last = [lines lastObject];
        NSSize lastSz = [last sizeWithAttributes:at];
        NSSize oneLine = [@"Ag" sizeWithAttributes:at];

        /* Le curseur suit le même décalage, sans quoi il flotterait au-dessus
         * du texte qu'il est censé suivre. */
        CGFloat cy = haut.y + ([lines count] - 1) * oneLine.height;

        [[NSColor blackColor] setStroke];
        NSBezierPath *caret = [NSBezierPath bezierPath];
        [caret moveToPoint:NSMakePoint(gTextPos.x + lastSz.width, cy)];
        [caret lineToPoint:NSMakePoint(gTextPos.x + lastSz.width, cy + oneLine.height)];
        [caret setLineWidth:1];
        [caret stroke];
    }

    draw_popup_menu();
}

/* La grille de FatBits : un trait par pixel de calque, plus marqué tous les
 * huit. Sans repères on ne se situe pas dans un damier — c'est la leçon déjà
 * tirée dans l'éditeur d'icônes, et la seule chose qu'on lui emprunte : sa
 * grille à lui est câblée sur 32×32 et lit les bits d'une icône, pas un
 * calque, donc il n'y avait rien d'autre à reprendre.
 *
 * Dessinée APRÈS la restauration, donc en coordonnées de VUE : des traits
 * d'un pixel tracés sous la transformation en feraient huit. */
- (void)drawFatGrid
{
    NSRect b = [self bounds];

    [[NSColor colorWithWhite:0.80 alpha:1.0] setStroke];
    NSBezierPath *g = [NSBezierPath bezierPath];
    for (CGFloat x = 0; x <= b.size.width; x += HCV_FATBITS) {
        [g moveToPoint:NSMakePoint(x + 0.5, 0)];
        [g lineToPoint:NSMakePoint(x + 0.5, b.size.height)];
    }
    for (CGFloat y = 0; y <= b.size.height; y += HCV_FATBITS) {
        [g moveToPoint:NSMakePoint(0, y + 0.5)];
        [g lineToPoint:NSMakePoint(b.size.width, y + 0.5)];
    }
    [g setLineWidth:1];
    [g stroke];

    /* Tous les huit PIXELS DE CALQUE, soit soixante-quatre points à l'écran :
     * c'est le pas de la grille d'alignement, donc les deux modes se lisent
     * l'un dans l'autre au lieu de se contredire. Le décalage de l'origine
     * compte — sans lui les repères glisseraient au défilement et ne
     * repéreraient plus rien. */
    CGFloat dx = fmod(gFatOrigine.x, 8) * HCV_FATBITS;
    CGFloat dy = fmod(gFatOrigine.y, 8) * HCV_FATBITS;
    [[NSColor colorWithWhite:0.50 alpha:1.0] setStroke];
    NSBezierPath *q = [NSBezierPath bezierPath];
    for (CGFloat x = -dx; x <= b.size.width; x += 8 * HCV_FATBITS) {
        [q moveToPoint:NSMakePoint(x + 0.5, 0)];
        [q lineToPoint:NSMakePoint(x + 0.5, b.size.height)];
    }
    for (CGFloat y = -dy; y <= b.size.height; y += 8 * HCV_FATBITS) {
        [q moveToPoint:NSMakePoint(0, y + 0.5)];
        [q lineToPoint:NSMakePoint(b.size.width, y + 0.5)];
    }
    [q setLineWidth:1];
    [q stroke];
}

- (void)drawRect:(NSRect)dirtyRect
{
    if (!hcv_fat()) { [self drawCardContent:dirtyRect]; return; }

    /* Le rectangle sale d'AppKit ne veut plus rien dire sous la
     * transformation : on redessine la fenêtre grossie en entier, et on donne
     * au corps la zone de CALQUE qu'elle montre. Huit fois moins de surface à
     * couvrir, donc le coût reste celui d'un redessin ordinaire. */
    [[NSColor whiteColor] setFill];
    NSRectFill([self bounds]);

    [NSGraphicsContext saveGraphicsState];
    /* SANS INTERPOLATION : le grossissement doit montrer des CARRÉS. Lissé,
     * FatBits afficherait des pixels flous et ne servirait plus à rien — on y
     * vient justement pour voir où est le pixel. */
    [[NSGraphicsContext currentContext]
        setImageInterpolation:NSImageInterpolationNone];

    /* L'ordre des deux appels est celui du point, à l'envers : la DERNIÈRE
     * opération posée est la PREMIÈRE appliquée. On veut (calque − origine)
     * × 8, donc la translation en dernier. */
    NSAffineTransform *t = [NSAffineTransform transform];
    [t scaleXBy:HCV_FATBITS yBy:HCV_FATBITS];
    [t translateXBy:-gFatOrigine.x yBy:-gFatOrigine.y];
    [t concat];

    [self drawCardContent:hcv_fat_zone([self bounds])];

    [NSGraphicsContext restoreGraphicsState];
    [self drawFatGrid];
}

/* ═══ openField, closeField, exitField ══════════════════════════════════
 *
 * Les trois messages qu'HyperCard envoie autour de l'édition d'un champ, et
 * qui sont la seule façon pour une pile de CONTRÔLER une saisie :
 *
 *   openField   quand l'édition commence ;
 *   closeField  quand elle se termine ET que le texte a changé ;
 *   exitField   quand elle se termine sans que rien ait changé.
 *
 * C'est la distinction qui fait tout l'intérêt de la paire : un
 * « on closeField » qui valide et reformate ne doit pas se déclencher quand
 * l'utilisateur n'a fait que traverser le champ.
 *
 * D'où gTexteAuDebut, pris à l'ouverture et comparé à la fermeture. Le
 * comparer au texte du NOYAU serait faux : endFieldEdit vient justement d'y
 * recopier ce que contient l'éditeur, donc les deux seraient toujours égaux
 * et closeField ne partirait jamais.
 *
 * gSansMessageChamp couvre les fermetures TECHNIQUES — changer de pile, tout
 * remettre à zéro — où le champ s'en va sans que l'utilisateur ait quitté
 * quoi que ce soit. */
static NSString *gTexteAuDebut     = nil;
static BOOL      gSansMessageChamp = NO;

/* ═══ Les messages du clavier ═══════════════════════════════════════════
 *
 * HyperCard nomme les touches avant d'agir : arrowKey, tabKey, returnKey,
 * enterKey, functionKey, controlKey, keyDown. Une pile qui veut sa propre
 * navigation ou ses propres raccourcis n'a que ça, et HC n'en envoyait aucun.
 *
 * Rend YES si un gestionnaire a PRIS la touche : l'appelant s'arrête là. Un
 * gestionnaire qui fait « pass » rend 0, et la touche retrouve son effet
 * habituel — le même accord qu'avec returnInField.
 *
 * commandKeyDown n'y est pas : Cmd+lettre est intercepté par les équivalents
 * clavier des menus AVANT que keyDown: soit appelé. Il faudrait
 * performKeyEquivalent:, c'est-à-dire se placer devant la barre de menus, et
 * cela mérite d'être fait séparément plutôt qu'en passant. */
- (BOOL)envoieToucheHyperCard:(NSEvent *)event
{
    Object *carte = hc_current_card();
    if (!carte) return NO;

    NSString *nues = [event charactersIgnoringModifiers];
    if ([nues length] == 0) return NO;          /* touche morte, accent en cours */
    unichar key = [nues characterAtIndex:0];
    NSUInteger mods = [event modifierFlags];

    const char *msg = NULL;
    char arg[32];
    arg[0] = '\0';

    switch (key) {
        case NSLeftArrowFunctionKey:    msg = "arrowKey"; strcpy(arg, "left");  break;
        case NSRightArrowFunctionKey:   msg = "arrowKey"; strcpy(arg, "right"); break;
        case NSUpArrowFunctionKey:      msg = "arrowKey"; strcpy(arg, "up");    break;
        case NSDownArrowFunctionKey:    msg = "arrowKey"; strcpy(arg, "down");  break;
        case NSTabCharacter:            msg = "tabKey";                         break;
        case NSCarriageReturnCharacter: msg = "returnKey";                      break;
        case NSEnterCharacter:          msg = "enterKey";                       break;
        default: break;
    }

    if (!msg && key >= NSF1FunctionKey && key <= NSF15FunctionKey) {
        msg = "functionKey";
        snprintf(arg, sizeof arg, "%d", (int)(key - NSF1FunctionKey) + 1);
    }

    if (!msg && (mods & NSEventModifierFlagControl) && key >= 1 && key < 128) {
        msg = "controlKey";
        snprintf(arg, sizeof arg, "%d", (int)key);
    }

    if (!msg) {
        /* keyDown : toute touche qui produit un caractère, celui-ci en
         * argument. On prend [event characters] et non les touches nues :
         * « on keyDown k » attend le caractère TAPÉ, majuscule et accent
         * compris. */
        NSString *ch = [event characters];
        if ([ch length] == 0) return NO;
        unichar c = [ch characterAtIndex:0];
        if (c < 32 || c == 0x7F) return NO;             /* commande, pas caractère */
        if (c >= 0xF700 && c <= 0xF8FF) return NO;      /* touche de fonction */
        msg = "keyDown";
        snprintf(arg, sizeof arg, "%s", [ch UTF8String]);
    }

    return hc_send_arg(carte, msg, arg[0] ? arg : NULL) ? YES : NO;
}

/* ═══ returnInField ═════════════════════════════════════════════════════
 *
 * HyperCard envoie returnInField quand Retour est frappé dans un champ. Si
 * personne ne le traite, le retour à la ligne s'insère normalement — c'est
 * ce que fait « pass returnInField », et ici c'est un simple NO rendu à
 * NSTextView, qui poursuit son travail habituel.
 *
 * hc_send rend 1 seulement si un gestionnaire a VRAIMENT pris le message :
 * depuis que « pass » veut dire ce qu'il dit, un gestionnaire qui passe rend
 * la main, et la touche retrouve son effet normal. Les deux mécaniques se
 * répondent exactement.
 *
 * PAS d'enterInField. Sur un portable, Entrée n'existe que par Fn+Retour, et
 * Cocoa envoie les deux touches sur insertNewline: — les distinguer oblige à
 * fouiller l'événement courant. Se tromper ferait fermer le champ sur un
 * simple Retour : un risque réel sur la touche dont on se sert, pour servir
 * celle qu'on n'a pas. Le jour où enterInField manquera à quelqu'un, il sera
 * temps ; ce n'est pas aujourd'hui.
 *
 * Première méthode de délégué de texte de ce fichier. Les suivantes —
 * tabKey, arrowKey — passeront par ici. */
- (BOOL)textView:(NSTextView *)tv doCommandBySelector:(SEL)cmd
{
    if (tv != gFieldEditor || !gEditingField) return NO;

    if (cmd == @selector(insertNewline:))
        return hc_send(gEditingField, "returnInField") ? YES : NO;

    /* Tabulation dans un champ : HyperCard envoie tabKey. Sans preneur, la
     * tabulation s'insère comme d'habitude. */
    if (cmd == @selector(insertTab:))
        return hc_send_arg(hc_current_card(), "tabKey", NULL) ? YES : NO;

    return NO;
}

/* Ce qu'il faut lâcher avant de changer de carte depuis l'INTERFACE.
 *
 * Un clic sur un bouton passe par mouseDown:, qui referme déjà l'édition en
 * cours — sans quoi gEditingField et sa zone de texte flottante resteraient
 * affichés par-dessus la carte suivante. Un clic de MENU ne passe pas par là,
 * et n'avait donc personne pour faire ce ménage. */
/* Un article d'un menu de pile a été choisi. Le noyau décide de ce qui part
 * — le message de l'article, ou doMenu à défaut ; on ne fait que lui donner
 * les deux indices rangés dans l'étiquette.
 *
 * Même ménage et même rafraîchissement que le menu Go : ces messages
 * naviguent souvent (« goCurves » fait « go bg "Curves" »), et un clic de
 * menu ne fait tourner aucune boucle qui consommerait le drapeau visuel. */
- (void)hcMenuScriptItem:(id)sender
{
    NSInteger t = [sender tag];
    [self prepareForCardChange];
    hc_menu_choisi((int)(t / 1000), (int)(t % 1000));
    (void)hc_take_visual_dirty();
    [self updateWindowTitle];
    [self setNeedsDisplay:YES];
}

- (void)prepareForCardChange
{
    if (gEditingField) [self endFieldEdit];
    [self dropFloating];
    gSelected = NULL;
    /* La sélection de peinture porte sur les PIXELS DE CETTE CARTE. La garder
     * en changeant de carte laissait un cadre de fourmis sur la nouvelle,
     * autour de rien — et la première transformation du menu Paint se serait
     * appliquée à cette zone-là, sur une image qui n'a jamais été
     * sélectionnée. */
    hcv_abandonne_selection();
}

- (void)toggleBackground:(id)sender {
    if (hcv_menu_trappe("Background")) return;   /* la pile détourne l'article */
    gEditBackground = !gEditBackground;
    gSelected = NULL;
    [gView endFieldEdit];
    [gView setNeedsDisplay:YES];
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

- (Object *)paintLayer {
    Object *card = [self documentCard];
    if (!card) return NULL;
    Object *layer = gEditBackground ? card->bg : card;
    return layer ? layer : card;
}

- (void)beginPaintUndo {
    Object *layer = [self paintLayer];
    if (!layer) return;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (!rep) return;
    gPaintUndo      = paint_copy(rep);
    gPaintUndoLayer = layer;

    /* Et, du même coup, le point de repère de Revert s'il n'en existe pas
     * encore pour ce calque.
     *
     * Ici et nulle part ailleurs : beginPaintUndo est déjà le passage obligé
     * de tout ce qui modifie la peinture — outils, menu Paint, collage,
     * effacement. Recenser à la main les endroits où « la peinture commence »,
     * c'est en oublier un, et l'oubli donnerait un Revert qui ramène à un état
     * plus ancien que l'utilisateur ne croit. Ce qui est pire que pas de
     * Revert du tout. */
    if (!gKeepSnap || gKeepLayer != layer) {
        gKeepSnap  = paint_copy(rep);
        gKeepLayer = layer;
    }
}

/* Keep : l'état courant devient le nouveau point de repère. Ce qui précède
 * n'est plus rattrapable par Revert — c'est le sens du mot. */
- (void)keepPaint {
    Object *layer = [self paintLayer];
    if (!layer) { NSBeep(); return; }
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (!rep) { NSBeep(); return; }
    gKeepSnap  = paint_copy(rep);
    gKeepLayer = layer;
}

/* Revert : retour au dernier point de repère.
 *
 * L'instantané SURVIT au retour : on doit pouvoir repeindre et revenir encore,
 * autant de fois qu'on veut. C'est pourquoi c'est paint_restore et non
 * paint_swap — l'échange aurait fait du point de repère une bascule.
 *
 * Un beginPaintUndo d'abord, pour qu'un Revert donné par erreur se défasse
 * d'un Cmd-Z : c'est l'article le plus destructeur du menu. */
- (void)revertPaint {
    Object *layer = [self paintLayer];
    if (!gKeepSnap || !layer || gKeepLayer != layer) { NSBeep(); return; }
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    if (!rep) { NSBeep(); return; }
    gPaintUndo      = paint_copy(rep);
    gPaintUndoLayer = layer;
    paint_restore(rep, gKeepSnap);
    [self setNeedsDisplay:YES];
}

/* Quitter la carte vaut Keep, comme dans HyperCard : ce qu'on y laisse est
 * gardé, et Revert ne peut plus le reprendre. Sans cet oubli, revenir sur une
 * carte ferait pointer Revert vers un état d'il y a une heure. */
- (void)forgetKeepSnapshot {
    gKeepSnap  = nil;
    gKeepLayer = NULL;
}

- (void)undo:(id)sender {
    (void)sender;
    if (!gPaintUndo || !gPaintUndoLayer) { NSBeep(); return; }
    NSBitmapImageRep *rep = paint_bitmap(gPaintUndoLayer,
                                         (int)[self bounds].size.width,
                                         (int)[self bounds].size.height);
    if (!rep) return;
    paint_swap(rep, gPaintUndo);
    [self setNeedsDisplay:YES];
}

- (void)clearPaintCache {
    [gPaintCache removeAllObjects];
}

- (void)resetForNewStack {
    /* Fermeture technique : la pile s'en va, l'utilisateur n'a quitté aucun
     * champ. Un closeField ici ferait tourner le script d'un objet qui est
     * sur le point de disparaître. */
    if (gEditingField) {
        gSansMessageChamp = YES;
        [self endFieldEdit];
        gSansMessageChamp = NO;
    }
    [self dropFloating];
    close_popup_menu();
    hc_set_selection(NULL, 0, 0);
    [self stopSprayTimer];
    [self stopAntsTimer];

    HCDoc vide = {0};
    *gDoc = vide;

    gSelected       = NULL;
    gResizeHandle   = 0;
    gDragging       = NO;
    gPenDrawing     = NO;
    gLockedAxis     = AXIS_NONE;
    gSelRectActive  = NO;
    gSelRectDrawing = NO;
    gLassoActive    = NO;
    gLassoCount     = 0;
    gFloatDragging  = NO;
}

- (void)flushPaintToKernel {
    if (!gPaintCache) return;

    NSMutableArray *mortes = [NSMutableArray array];

    for (NSValue *key in gPaintCache) {
        Object *o = [key pointerValue];

        /* Le cache est indexé par POINTEUR et rien ne l'invalide : une carte
         * coupée, une pile fermée, et l'adresse y reste. hc_set_paint ferait
         * alors free sur de la mémoire rendue — d'où un plantage à
         * l'enregistrement, très loin de sa cause, puisque le flush n'a lieu
         * qu'à ce moment-là.
         *
         * On demande donc au noyau, qui tient le registre des piles ouvertes.
         * Il ne déréférence pas l'adresse douteuse, il la compare aux objets
         * vivants. */
        if (!hc_layer_is_live(o)) {
            NSLog(@"[flush] o=%p calque mort, ignore", o);
            [mortes addObject:key];
            continue;
        }

        NSBitmapImageRep *rep = [gPaintCache objectForKey:key];
        if (!rep) continue;

        NSInteger w = [rep pixelsWide], h = [rep pixelsHigh];

        /* On recopie les octets tels quels : `fresh` doit donc porter le meme
           espace colorimetrique et le meme format alpha que `rep`. Figer
           NSCalibratedRGBColorSpace ici re-etiquetait les pixels sans les
           convertir, alors que le chargement, lui, convertissait pour de bon.
           C'est cette asymetrie qui rongeait les couleurs cycle apres cycle.
           Le calque vit desormais en sRGB (cf. paint_bitmap) et l'on grave en
           sRGB : l'aller-retour est l'identite au bit pres. */
        NSBitmapImageRep *fresh = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL
                          pixelsWide:w pixelsHigh:h
                       bitsPerSample:8 samplesPerPixel:4
                            hasAlpha:YES isPlanar:NO
                      colorSpaceName:[rep colorSpaceName]
                        bitmapFormat:([rep bitmapFormat]
                                      & NSBitmapFormatAlphaNonpremultiplied)
                         bytesPerRow:w * 4 bitsPerPixel:32];

        unsigned char *src = [rep bitmapData];
        unsigned char *dst = [fresh bitmapData];
        NSInteger sbpr = [rep bytesPerRow], dbpr = [fresh bytesPerRow];
        NSInteger spp  = [rep samplesPerPixel];
        /* Pas en octets : samplesPerPixel n'est pas le pas des lors qu'une rep
           a 3 echantillons alignes sur 32 bits. */
        NSInteger sstride = [rep bitsPerPixel] / 8;
        NSInteger dstride = [fresh bitsPerPixel] / 8;
        for (NSInteger y = 0; y < h; y++) {
            for (NSInteger x = 0; x < w; x++) {
                unsigned char *sp = src + y*sbpr + x*sstride;
                unsigned char *dp = dst + y*dbpr + x*dstride;
                dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2];
                dp[3] = (spp >= 4) ? sp[3] : 255;
            }
        }

        /* colorSpaceName ne transporte qu'un nom generique : on rattache le
           profil ICC exact de la source. Retagging = on change l'etiquette,
           jamais les valeurs. A faire une fois les pixels ecrits, la methode
           renvoyant une nouvelle rep. */
        NSColorSpace *cs = [rep colorSpace];
        if (!cs) cs = [NSColorSpace sRGBColorSpace];
        NSBitmapImageRep *tagged =
            [fresh bitmapImageRepByRetaggingWithColorSpace:cs];
        if (tagged) fresh = tagged;

        NSString *b64 = hcp_encode(fresh);
        if (!b64) { NSLog(@"[flush] o=%p ECHEC encodage", o); continue; }
        NSLog(@"[flush] o=%p grave %ldx%ld, %lu car.", o, (long)w, (long)h,
              (unsigned long)[b64 length]);
        hc_set_paint(o, [b64 UTF8String]);
    }

    /* Hors de la boucle : on ne modifie pas une table qu'on parcourt. */
    for (NSValue *key in mortes) [gPaintCache removeObjectForKey:key];
}

- (void)mouseMoved:(NSEvent *)event {
    if (!gPopupTarget || gPopupFlashTimer) return;
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    NSInteger row = popup_row_at_point(p);
    if (row == gPopupKeyboardRow) return;
    gPopupKeyboardRow = row;
    [self setNeedsDisplay:YES];
}

- (NSView *)hitTest:(NSPoint)point {
    if (gTool != TOOL_BROWSE && gEditingField) {
        [self endFieldEdit];
        [[self window] makeFirstResponder:self];
    }
    return [super hitTest:point];
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    /* LA FRONTIÈRE DE FATBITS. Tout ce qui suit travaille en coordonnées de
     * CALQUE ; hors FatBits la conversion est l'identité, donc rien ne
     * change pour les quatre-vingt-six sites en aval. Voir l'en-tête de
     * hcv_vue_vers_calque. */
    NSPoint pvue = p;         /* gardé brut : le déplacement s'y mesure */
    p = hcv_vue_vers_calque(p);
    gFatDernier = p;          /* on centrera là-dessus si FatBits s'allume */

    /* ⌘-GLISSER DÉPLACE LA FENÊTRE GROSSIE. Avant tout le reste : sous
     * FatBits on ne voit qu'un huitième du calque, et sans moyen de bouger
     * le mode ne servirait qu'au coin où l'on est entré. ⌘ est libre —
     * aucun outil de peinture ne s'en sert. C'est NOTRE choix, pas celui
     * d'HyperCard : voir l'en-tête. */
    if (hcv_fat() && ([event modifierFlags] & NSEventModifierFlagCommand)) {
        gFatGlisse   = YES;
        gFatSaisiVue = pvue;
        gFatSaisiOrg = gFatOrigine;
        return;
    }

    /* 1. Menus Popup */
    if (gPopupTarget) {
        if (gPopupFlashTimer) return;
        NSInteger row = popup_row_at_point(p);
        if (row >= 0) {
            flash_popup_selection(self, row);
        } else {
            close_popup_menu();
            [self setNeedsDisplay:YES];
        }
        return;
    }

    /* L'ENCADRÉ DE « find » DISPARAÎT AU PREMIER CLIC, comme dans HyperCard.
     *
     * Rien ne l'effaçait : la boîte noire restait sur le champ tant qu'on ne
     * changeait pas de carte ou qu'on ne relançait pas une recherche. Cliquer
     * ailleurs — dans le champ ou en dehors — la laissait en place.
     *
     * hc_found_cache ne touche qu'à l'AFFICHAGE : « the foundChunk » et
     * « the foundText » continuent de répondre jusqu'à la recherche suivante.
     * C'est ce qui permet d'écrire « find "x" », de cliquer, puis
     * « select the foundChunk » — un idiome que ces piles emploient. */
    if (hc_found_cache()) [self setNeedsDisplay:YES];

    Object *hit = part_at(hc_current_card(), p);

    gClickPoint = p;
    gClickField = (hit && hit->type == OBJ_FIELD) ? hit : NULL;
    gMouseClicked = YES;

    /* 2. Fermeture prioritaire du mode édition de texte si l'outil n'est plus Browse */
    if (gTool != TOOL_BROWSE && gEditingField) {
        [self endFieldEdit];
    }

    /* 3. Double-clic sur un objet (Édition des infos ou du script) */
    if (gTool != TOOL_BROWSE && hit && [event clickCount] == 2) {
        if (hit->type == OBJ_BUTTON)      [self showButtonInfo:hit];
        else if (hit->type == OBJ_FIELD)  [self showFieldInfo:hit];
        else                              [self editScriptOf:hit];
        return;
    }

    /* 4. Priorité aux outils FIELD et BUTTON (sélection et déplacement direct) */
    if (gTool == TOOL_FIELD || gTool == TOOL_BUTTON) {
        if (hit && ((gTool == TOOL_FIELD  && hit->type == OBJ_FIELD) ||
                    (gTool == TOOL_BUTTON && hit->type == OBJ_BUTTON))) {
            
            gSelected = hit;
            gMoving = YES;
            [[self window] makeFirstResponder:self];
            gMoveStart = p;
            gObjStartX = hit->x;
            gObjStartY = hit->y;
            [self setNeedsDisplay:YES];
            return;
        }
    }

    /* 5. Gestion des sélections de peinture actives (Rectangulaire & Lasso) */
    if (gTool == TOOL_SELRECT && gSelRectActive) {
        NSRect selRect = NSMakeRect(MIN(gSelStart.x, gSelEnd.x), MIN(gSelStart.y, gSelEnd.y),
                                    fabs(gSelEnd.x - gSelStart.x), fabs(gSelEnd.y - gSelStart.y));
        if (NSPointInRect(p, selRect)) {
            [self beginPaintUndo];
            Object *layer = [self paintLayer];
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            
            copy_rect(rep, gSelStart, gSelEnd);
            
            BOOL optionDown = ([event modifierFlags] & NSEventModifierFlagOption) != 0;
            if (!optionDown) {
                erase_rect(rep, gSelStart, gSelEnd);
            }

            gFloating = YES;
            gFloatDragging = YES;
            gFloatPos = selRect.origin;
            gFloatGrab = NSMakePoint(p.x - gFloatPos.x, p.y - gFloatPos.y);
            gSelRectActive = NO;
            [self startAntsTimer];
            [self setNeedsDisplay:YES];
            return;
        } else if (gFloating) {
            [self dropFloating];
        }
    }

    if (gTool == TOOL_LASSO && gLassoActive && gLassoCount >= 3) {
        NSBezierPath *lassoPath = [NSBezierPath bezierPath];
        [lassoPath moveToPoint:gLassoPts[0]];
        for (int i = 1; i < gLassoCount; i++) [lassoPath lineToPoint:gLassoPts[i]];
        [lassoPath closePath];

        if ([lassoPath containsPoint:p]) {
            [self beginPaintUndo];
            Object *layer = [self paintLayer];
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            
            copy_freeform(rep, gLassoPts, gLassoCount);
            
            BOOL optionDown = ([event modifierFlags] & NSEventModifierFlagOption) != 0;
            if (!optionDown) {
                erase_freeform(rep, gLassoPts, gLassoCount);
            }
            
            double minx = gLassoPts[0].x, miny = gLassoPts[0].y;
            for (int i = 1; i < gLassoCount; i++) {
                if (gLassoPts[i].x < minx) minx = gLassoPts[i].x;
                if (gLassoPts[i].y < miny) miny = gLassoPts[i].y;
            }
            gFloating = YES;
            gFloatDragging = YES;
            gFloatPos = NSMakePoint(minx, miny);
            gFloatGrab = NSMakePoint(p.x - gFloatPos.x, p.y - gFloatPos.y);
            gLassoActive = NO;
            [self startAntsTimer];
            [self setNeedsDisplay:YES];
            return;
        } else if (gFloating) {
            [self dropFloating];
        }
    }

    if (gFloating) {
        NSRect fr = NSMakeRect(gFloatPos.x, gFloatPos.y, gClipW, gClipH);
        if (NSPointInRect(p, fr)) {
            gFloatDragging = YES;
            gFloatGrab = NSMakePoint(p.x - gFloatPos.x, p.y - gFloatPos.y);
        } else {
            [self dropFloating];
        }
        return;
    }

    /* 6. Préparation Undo pour les outils de dessin */
    if (gTool == TOOL_PENCIL || gTool == TOOL_BRUSH || gTool == TOOL_ERASER ||
        gTool == TOOL_SPRAY  || gTool == TOOL_LINE  || gTool == TOOL_RECT   ||
        gTool == TOOL_OVAL   || gTool == TOOL_FILL  || gTool == TOOL_FREEFORM)
        [self beginPaintUndo];

    /* 7. Traitement des outils continus */
    if (gTool == TOOL_PENCIL || gTool == TOOL_BRUSH || gTool == TOOL_ERASER ||
        gTool == TOOL_SPRAY) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        if (layer) {
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                        (int)[self bounds].size.height);
            gPenLast = p;
            gPenDrawing = YES;
            gLockedAxis = AXIS_NONE;
            /* Le sens du crayon se décide ICI, sur le pixel du premier
             * point, et ne rebascule plus jusqu'au relâchement. */
            gPenEfface = (gTool == TOOL_PENCIL) &&
                         paint_pixel_pose(rep, (int)lround(p.x),
                                               (int)lround(p.y));
            if (gTool == TOOL_PENCIL) {
                if (gPenEfface) erase_stroke(rep, p, p, gLineWidth);
                else paint_stroke(rep, p, p, [NSColor blackColor], gLineWidth);
            }
            else if (gTool == TOOL_BRUSH)  brush_stroke(rep, p, p);
            else if (gTool == TOOL_SPRAY) {
                spray_stamp(rep, (int)lround(p.x), (int)lround(p.y),
                            gSprayRadius, gSprayDensity);
                [self startSprayTimer];
            }
            else                           erase_stroke(rep, p, p, 16);
            [self setNeedsDisplay:YES];
        }
        return;
    }

    if (gTool == TOOL_LINE || gTool == TOOL_RECT || gTool == TOOL_OVAL) {
        gShapeStart = hcv_cale_pt(p);
        gShapeEnd = gShapeStart;
        gShapeDrawing = YES;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_FILL) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                    (int)[self bounds].size.height);
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

    if (gTool == TOOL_LASSO) {
        [[self window] makeFirstResponder:self];
        gLassoCount = 0;
        gLassoPts[gLassoCount++] = p;
        gLassoDrawing = YES;
        gLassoActive = NO;
        [self stopAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_SELRECT) {
        [[self window] makeFirstResponder:self];
        gSelStart = hcv_cale_pt(p); gSelEnd = gSelStart;
        gSelRectDrawing = YES;
        gSelRectActive = NO;
        [self stopAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_TEXT) {
        [self commitText];
        gTextPos = p;
        gTextBuf = [NSMutableString string];
        gTextActive = YES;
        [[self window] makeFirstResponder:self];
        [[NSFontManager sharedFontManager] setSelectedFont:text_font() isMultiple:NO];
        [self setNeedsDisplay:YES];
        return;
    }

    /* 8. Outil BROWSE (Interaction avec la carte) */
    if (gTool == TOOL_BROWSE) {
        /* Refermer l'édition en cours avant tout AUTRE clic, sinon un champ
         * en cours d'édition la garde ouverte pendant qu'un bouton clique à
         * côté exécute son script.
         *
         * C'est un vrai problème quand ce script change de carte (« go
         * next », par exemple) : gEditingField et sa zone de texte flottante
         * pointaient toujours sur le champ de l'ANCIENNE carte, et
         * continuaient de s'afficher par-dessus la nouvelle — en
         * surimpression, comme si les deux cartes se superposaient. Chaque
         * branche ci-dessous appelait déjà endFieldEdit pour SON propre cas
         * (changer de champ, cliquer la carte nue), sauf celle-ci — cliquer
         * un bouton, un champ verrouillé ou un menu popup pendant qu'un AUTRE
         * champ s'édite — qui ne l'appelait jamais.
         *
         * On épargne le champ qu'on re-clique lui-même : y cliquer ne fait
         * que replacer le curseur, pas recommencer l'édition, et lui
         * appliquer endFieldEdit ici la fermerait pour rien. */
        if (gEditingField && hit != gEditingField) [self endFieldEdit];

        if (hit && hit->type == OBJ_FIELD && hit->style &&
            strcmp(hit->style, "scrolling") == 0) {
            CGFloat bw = 16;
            NSRect bar = NSMakeRect(hit->x + hit->w - bw, hit->y, bw, hit->h);
            if (NSPointInRect(p, bar)) {
                NSRect tr0 = field_text_rect(hit);
                CGFloat th = field_text_height(hit, tr0);
                CGFloat vh = tr0.size.height;
                CGFloat gy = bar.origin.y + 16, gh = bar.size.height - 32;
                CGFloat lh = [@"Ag" sizeWithAttributes:
                    @{NSFontAttributeName: obj_font(hit, 12)}].height;
                if (lh < 4) lh = 12;

                if (p.y < bar.origin.y + 16) {
                    hit->scroll -= (int)lh;
                } else if (p.y > bar.origin.y + hit->h - 16) {
                    hit->scroll += (int)lh;
                } else if (th > vh && gh > 8) {
                    CGFloat kh = gh * (vh / th);
                    if (kh < 12) kh = 12;
                    if (kh > gh) kh = gh;
                    CGFloat maxs = th - vh;
                    CGFloat pos = (maxs > 0) ? (hit->scroll / maxs) : 0;
                    if (pos < 0) pos = 0;
                    if (pos > 1) pos = 1;
                    CGFloat ky = gy + pos * (gh - kh);
                    if (p.y >= ky && p.y <= ky + kh) {
                        gScrollField = hit;
                        gScrollGrab  = p.y - ky;
                        gScrollGH    = gh;
                        gScrollKH    = kh;
                        gScrollGY    = gy;
                        gScrollMax   = maxs;
                        return;
                    }
                    if (p.y < ky) hit->scroll -= hit->h;
                    else          hit->scroll += hit->h;
                }
                field_clamp_scroll(hit);
                sync_editor_scroll(hit);
                [self setNeedsDisplay:YES];
                return;
            }
        }

        if (hit && hit->type == OBJ_BUTTON && hit->style &&
            strcmp(hit->style, "popup") == 0) {
            open_popup_menu(hit, self);
            return;
        }

        if (hit && hit->type == OBJ_FIELD) {
            if (hit->locktext) {
                if (hit->auto_select) {
                    int ligne = click_line_number(hit, p);
                    if (ligne > 0) {
                        const char *tx = hc_field_text(hit);
                        int deb = 0, n = 1;
                        while (n < ligne && tx[deb]) {
                            if (tx[deb] == '\n') n++;
                            deb++;
                        }
                        int fin = deb;
                        while (tx[fin] && tx[fin] != '\n') fin++;
                        hc_set_selection(hit, deb, fin - deb);
                    }
                }
                gPressed = hit;
                gPressedCard = hc_current_card();
                hc_send(hit, "mouseDown");
                /* mouseStillDown part en continu tant que le bouton reste
                 * enfoncé, même immobile — c'est ce qui fait marcher les
                 * boutons à répétition et les flèches de défilement. Un
                 * minuteur plutôt que mouseDragged:, qui ne se déclenche
                 * qu'au mouvement. */
                [self startStillDownTimer];
                [self setNeedsDisplay:YES];
            } else {
                [self beginFieldEdit:hit];
            }
            return;
        }

        if (hit) {
            gPressed = hit;
            gPressedCard = hc_current_card();
            if (hit->type == OBJ_BUTTON && hit->autohilite &&
                (!hit->style ||
                 (strcmp(hit->style, "checkBox") != 0 && strcmp(hit->style, "checkbox") != 0 &&
                  strcmp(hit->style, "radioButton") != 0 && strcmp(hit->style, "radiobutton") != 0))) {
                hc_set_hilite(hit, hc_current_card(), 1);
            }
            hc_send(hit, "mouseDown");
            [self startStillDownTimer];
            [self setNeedsDisplay:YES];
        } else {
            /* Clic sur la carte nue : HyperCard lui envoie quand même
             * mouseDown, d'où il remonte au fond puis à la pile. C'est ce qui
             * permet à un script de carte de réagir à un clic n'importe où —
             * les piles de navigation s'en servent beaucoup. endFieldEdit
             * déjà fait en entrée du bloc TOOL_BROWSE, inutile de le refaire
             * ici. */
            gPressed = hc_current_card();
            gPressedCard = gPressed;
            hc_send(gPressed, "mouseDown");
            [self startStillDownTimer];
        }
        return;
    }

    /* 9. Redimensionnement via poignées */
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

    /* 10. Sélection générale ou tracé du rectangle de création */
    if (hit) {
        gSelected = hit;
        gMoving = YES;
        [[self window] makeFirstResponder:self];
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
    /* Le déplacement de la fenêtre grossie, AVANT la conversion : il se
     * mesure en points de vue, contre ce qui a été saisi et ne bouge plus.
     * La portion sous le curseur reste sous le curseur, comme une main qui
     * tient la feuille. */
    if (gFatGlisse) {
        NSRect vue = [self bounds];
        CGFloat w = vue.size.width  / HCV_FATBITS;
        CGFloat h = vue.size.height / HCV_FATBITS;
        NSPoint org = NSMakePoint(
            gFatSaisiOrg.x - (p.x - gFatSaisiVue.x) / HCV_FATBITS,
            gFatSaisiOrg.y - (p.y - gFatSaisiVue.y) / HCV_FATBITS);
        /* hcv_fat_centre borne au calque, et prend un CENTRE : on lui donne
         * celui qui correspond à l'origine voulue, plutôt que de recopier ici
         * le bornage — une seconde copie qui aurait divergé de la première. */
        hcv_fat_centre(NSMakePoint(org.x + w / 2, org.y + h / 2), vue);
        [self setNeedsDisplay:YES];
        return;
    }

    p = hcv_vue_vers_calque(p);          /* la même frontière qu'au mouseDown */
    BOOL shiftDown = ([event modifierFlags] & NSEventModifierFlagShift) != 0;

    if (gPopupTarget && !gPopupFlashTimer) {
        NSInteger row = popup_row_at_point(p);
        if (row != gPopupKeyboardRow) {
            gPopupKeyboardRow = row;
            [self setNeedsDisplay:YES];
        }
        return;
    }

    if (gPressed && gPressed->type == OBJ_FIELD && gPressed->auto_select) {
        int ligne = click_line_number(gPressed, p);
        if (ligne > 0) {
            const char *tx = hc_field_text(gPressed);
            int deb = 0, n = 1;
            while (n < ligne && tx[deb]) {
                if (tx[deb] == '\n') n++;
                deb++;
            }
            int fin = deb;
            while (tx[fin] && tx[fin] != '\n') fin++;
            hc_set_selection(gPressed, deb, fin - deb);
            [self setNeedsDisplay:YES];
        }
        return;
    }

    if (gScrollField) {
        CGFloat travel = gScrollGH - gScrollKH;
        if (travel > 0) {
            CGFloat pos = (p.y - gScrollGrab - gScrollGY) / travel;
            if (pos < 0) pos = 0;
            if (pos > 1) pos = 1;
            gScrollField->scroll = (int)(pos * gScrollMax);
            sync_editor_scroll(gScrollField);
        }
        [self setNeedsDisplay:YES];
        return;
    }

    if (gFloating && gFloatDragging) {
        NSPoint targetP = shiftDown ? constrain_to_axis(gMoveStart, p) : p;
        gFloatPos = NSMakePoint(targetP.x - gFloatGrab.x, targetP.y - gFloatGrab.y);
        [self setNeedsDisplay:YES];
        return;
    }

    /* Outils de dessin continu (Crayon, Pinceau, Aérographe, Gomme) avec verrou d'axe */
    if ((gTool == TOOL_PENCIL || gTool == TOOL_BRUSH || gTool == TOOL_ERASER || gTool == TOOL_SPRAY) && gPenDrawing) {
        if (shiftDown) {
            if (gLockedAxis == AXIS_NONE) {
                CGFloat dx = fabs(p.x - gPenLast.x);
                CGFloat dy = fabs(p.y - gPenLast.y);
                if (dx > 2 || dy > 2) {
                    gLockedAxis = (dx > dy) ? AXIS_HORIZONTAL : AXIS_VERTICAL;
                }
            }

            if (gLockedAxis == AXIS_HORIZONTAL) {
                p.y = gPenLast.y;
            } else if (gLockedAxis == AXIS_VERTICAL) {
                p.x = gPenLast.x;
            }
        } else {
            gLockedAxis = AXIS_NONE;
        }

        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);

        if (gTool == TOOL_PENCIL) {
            /* Le sens vient du mouseDown : voir gPenEfface. */
            if (gPenEfface) erase_stroke(rep, gPenLast, p, gLineWidth);
            else paint_stroke(rep, gPenLast, p, [NSColor blackColor], gLineWidth);
        } else if (gTool == TOOL_BRUSH) {
            brush_stroke(rep, gPenLast, p);
        } else if (gTool == TOOL_SPRAY) {
            spray_stroke(rep, gPenLast, p, gSprayRadius, gSprayDensity);
        } else if (gTool == TOOL_ERASER) {
            erase_stroke(rep, gPenLast, p, 16);
        }

        gPenLast = p;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_FREEFORM && gFreeDrawing) {
        if (shiftDown) p = constrain_to_axis(gFreePts[0], p);
        if (gFreeCount < 4096) gFreePts[gFreeCount++] = p;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_SELRECT && gSelRectDrawing) {
        gSelEnd = hcv_cale_pt(shiftDown ? constrain_to_axis(gSelStart, p) : p);
        [self setNeedsDisplay:YES];
        return;
    }

    if (gShapeDrawing) {
        /* Le calage APRÈS la contrainte d'axe : caler d'abord ferait dévier
         * le point de la ligne que shift venait de rendre droite, et la
         * droiture se perdrait sur un pixel. Dans cet ordre les deux tiennent
         * — un axe conservé, et les deux bouts sur la grille. */
        gShapeEnd = hcv_cale_pt(shiftDown ? constrain_to_axis(gShapeStart, p)
                                          : p);
        [self setNeedsDisplay:YES];
        return;
    }

    if (gResizeHandle && gSelected) {
        NSPoint currentP = shiftDown ? constrain_to_axis(gMoveStart, p) : p;
        int dx = (int)(currentP.x - gMoveStart.x);
        int dy = (int)(currentP.y - gMoveStart.y);
        int x = gObjStartX, y = gObjStartY, w = gObjStartW, h = gObjStartH;
        switch (gResizeHandle) {
            case 1: x += dx; y += dy; w -= dx; h -= dy; break;
            case 2: y += dy; w += dx; h -= dy; break;
            case 3: x += dx; w -= dx; h += dy; break;
            case 4: w += dx; h += dy; break;
        }
        if (w < 8) w = 8;
        if (h < 8) h = 8;
        /* Le calage APRÈS le plancher de huit : caler d'abord aurait pu
         * ramener une largeur à zéro, que le plancher aurait relevée à huit —
         * donc un coin qui ne bouge plus et une poignée qui semble coincée.
         * Dans cet ordre, huit est déjà sur la grille et rien ne se perd. */
        gSelected->x = hcv_cale(x); gSelected->y = hcv_cale(y);
        gSelected->w = hcv_cale(w); gSelected->h = hcv_cale(h);
        [self setNeedsDisplay:YES];
        return;
    }

    if (gTool == TOOL_LASSO && gLassoDrawing) {
        if (gLassoCount < 4096) gLassoPts[gLassoCount++] = p;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gMoving && gSelected) {
        NSPoint currentP = shiftDown ? constrain_to_axis(gMoveStart, p) : p;
        int dx = (int)(currentP.x - gMoveStart.x);
        int dy = (int)(currentP.y - gMoveStart.y);
        /* C'est la POSITION qui se cale, pas l'écart de la souris : deux
         * boutons traînés l'un après l'autre ont alors exactement le même x.
         * Caler l'écart n'aurait aligné que les gestes — chaque bouton
         * resterait décalé de là où il était parti. */
        gSelected->x = hcv_cale(gObjStartX + dx);
        gSelected->y = hcv_cale(gObjStartY + dy);
        [self setNeedsDisplay:YES];
        return;
    }

    if (!gDragging) return;
    NSPoint currentP = shiftDown ? constrain_to_axis(gDragStart, p) : p;
    CGFloat x = MIN(gDragStart.x, currentP.x);
    CGFloat y = MIN(gDragStart.y, currentP.y);
    CGFloat w = fabs(currentP.x - gDragStart.x);
    CGFloat h = fabs(currentP.y - gDragStart.y);
    /* Un objet NAÎT sur la grille, sans quoi il faudrait le recaler juste
     * après l'avoir tiré — et l'aperçu montrerait autre chose que ce qu'on
     * obtiendrait. */
    gDragRect = NSMakeRect((CGFloat)hcv_cale((int)lround(x)),
                           (CGFloat)hcv_cale((int)lround(y)),
                           (CGFloat)hcv_cale((int)lround(w)),
                           (CGFloat)hcv_cale((int)lround(h)));
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event {
    if (gFatGlisse) { gFatGlisse = NO; return; }

    if (gPopupTarget && !gPopupFlashTimer) {
        NSPoint pp = [self convertPoint:[event locationInWindow] fromView:nil];
        NSInteger row = popup_row_at_point(pp);
        if (row >= 0 && popup_row_is_enabled(row)) {
            flash_popup_selection(self, row);
        } else {
            if ([NSDate timeIntervalSinceReferenceDate] - gPopupOpenedAt > 0.25) {
                close_popup_menu();
                [self setNeedsDisplay:YES];
            }
        }
        return;
    }

    if (gScrollField) { gScrollField = NULL; return; }
    if (gFloating) {
        gFloatDragging = NO;
        return;
    }
    if (gResizeHandle) {
        gResizeHandle = 0;
        [self setNeedsDisplay:YES];
        return;
    }
    if (gTool == TOOL_PENCIL || gTool == TOOL_ERASER ||
        gTool == TOOL_BRUSH  || gTool == TOOL_SPRAY) {
        gPenDrawing = NO;
        gLockedAxis = AXIS_NONE;
        [self stopSprayTimer];
        return;
    }
    if (gTool == TOOL_SELRECT) {
        gSelRectDrawing = NO;
        gSelRectActive = (fabs(gSelEnd.x-gSelStart.x) > 3 && fabs(gSelEnd.y-gSelStart.y) > 3);
        if (gSelRectActive) [self startAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }
    if (gTool == TOOL_LASSO) {
        gLassoDrawing = NO;
        gLassoActive = (gLassoCount >= 3);
        if (gLassoActive) [self startAntsTimer];
        [self setNeedsDisplay:YES];
        return;
    }

    /* Gravure des formes géométriques */
    if (gShapeDrawing) {
        gShapeDrawing = NO;
        BOOL optionDown = ([event modifierFlags] & NSEventModifierFlagOption) != 0;
        NSRect box = compute_shape_rect(gShapeStart, gShapeEnd, optionDown);

        NSPoint finalStart = box.origin;
        NSPoint finalEnd = NSMakePoint(box.origin.x + box.size.width, box.origin.y + box.size.height);

        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);

        if (gTool == TOOL_LINE) {
            if (optionDown) {
                NSPoint opposite = NSMakePoint(2 * gShapeStart.x - gShapeEnd.x,
                                               2 * gShapeStart.y - gShapeEnd.y);
                paint_shape(rep, TOOL_LINE, opposite, gShapeEnd, [NSColor blackColor], gLineWidth);
            } else {
                paint_shape(rep, TOOL_LINE, gShapeStart, gShapeEnd, [NSColor blackColor], gLineWidth);
            }
        } else {
            if (gShapeFilled)
                fill_shape(rep, gTool, finalStart, finalEnd);
            paint_shape(rep, gTool, finalStart, finalEnd, [NSColor blackColor], gLineWidth);
        }

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

            /* TOUT CE QUI TOUCHE gDoc SE FAIT AVANT L'ENVOI.
             *
             * hc_send exécute du script, et le script peut tout changer sous
             * nos pieds : supprimer le bouton (« on mouseUp / delete me / end
             * mouseUp », l'idiome du bouton qui s'efface quand on s'en sert),
             * changer de carte, fermer la pile — donc congédier le document
             * dont gDoc est l'état. Lire ou écrire gDoc ensuite, c'est parier
             * sur ce qui aura survécu.
             *
             * On prend donc l'objet pressé et on remet l'ardoise à zéro
             * AVANT, puis on ne travaille plus que sur la variable locale.
             * Comme aucun rappel ne peut mettre une variable locale à NULL,
             * c'est hc_object_is_live qui dit si l'objet est encore là. */
            Object *presse = gPressed;
            Object *carteCliquee = gPressedCard;
            gPressed = NULL;
            gPressedCard = NULL;

            /* Pour la carte elle-même, part_at rend NULL : on compare
             * donc à la carte courante plutôt qu'au résultat du test. */
            if (hit == presse || (!hit && presse == hc_current_card()))
                hc_send(presse, "mouseUp");

            /* LA FIN DU CLIC S'APPLIQUE À LA CARTE DU CLIC.
             *
             * Elle lisait ici hc_current_card(), APRÈS le gestionnaire. Un
             * « go next card » dans mouseUp suffisait à éteindre le bouton sur
             * la carte d'arrivée ; avec un bouton de fond à sharedHilite faux
             * et un saut vers une autre pile, on inscrivait dans une carte de B
             * l'identifiant d'un bouton de A.
             *
             * La règle elle-même est descendue dans le noyau : c'est du modèle,
             * pas de l'affichage, et elle y devient vérifiable sans AppKit.
             * hc_fin_de_clic vérifie lui-même que les deux objets vivent. */
            hc_fin_de_clic(presse, carteCliquee);

            [self setNeedsDisplay:YES];
        }
        if (gMoving) {
            gMoving = NO;
            [self setNeedsDisplay:YES];
            return;
        }
        return;
    }

    if (!gDragging) return;
    gDragging = NO;
    if (gDragRect.size.width < 8 || gDragRect.size.height < 8) {
        [self setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
    if (!card) return;
    Object *owner = gEditBackground ? card->bg : card;
    if (!owner) owner = card;
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

- (void)findInStack:(id)sender {
    if (hcv_menu_trappe("Find…")) return;   /* la pile détourne l'article */
    if (!gMsgBox) return;

    NSString *amorce = @"find \"\"";
    [gMsgBox setStringValue:amorce];
    [[self window] makeFirstResponder:gMsgBox];

    NSText *ed = [[self window] fieldEditor:YES forObject:gMsgBox];
    if (ed) {
        NSUInteger pos = [amorce length] - 1;
        [ed setSelectedRange:NSMakeRange(pos, 0)];
    }
}

- (void)dropFloating {
    if (!gFloating || !gClipboard) { gFloating = NO; return; }
    Object *card = hc_current_card();
    if (!card) { gFloating = NO; return; }
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    stamp_clipboard(rep, gFloatPos);

    /* La sélection suit la zone déposée.
     *
     * Sans cela, gSelStart/gSelEnd et gLassoPts gardent leur position
     * d'origine : le tracé pointillé réapparaissait là d'où le dessin venait,
     * alors qu'il est maintenant ailleurs. La reprendre au même endroit
     * permet aussi de la redéplacer aussitôt, comme dans HyperCard. */
    if (gTool == TOOL_SELRECT) {
        gSelStart = gFloatPos;
        gSelEnd   = NSMakePoint(gFloatPos.x + gClipW, gFloatPos.y + gClipH);
        gSelRectActive = YES;
    } else if (gTool == TOOL_LASSO && gClipPtsCount >= 3) {
        for (int i = 0; i < gClipPtsCount && i < 4096; i++)
            gLassoPts[i] = NSMakePoint(gFloatPos.x + gClipPts[i].x,
                                       gFloatPos.y + gClipPts[i].y);
        gLassoCount = gClipPtsCount;
        gLassoActive = YES;
    }

    gFloating = NO;
    gFloatDragging = NO;
    [self setNeedsDisplay:YES];
}

static NSTimer *gIdleTimer = nil;
static BOOL     gInIdle = NO;

- (void)startIdleTimer {
    if (gIdleTimer) return;
    gIdleTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/10.0
                                                  target:self
                                                selector:@selector(idleTick:)
                                                userInfo:nil
                                                 repeats:YES];
}

- (void)stopIdleTimer {
    if (gIdleTimer) { [gIdleTimer invalidate]; gIdleTimer = nil; }
}

/* ═══ mouseEnter, mouseLeave, mouseWithin ═══════════════════════════════
 *
 * Le survol. Les boutons qui s'allument au passage du curseur sont partout
 * dans les piles des années 90, et HC n'envoyait aucun de ces trois messages.
 *
 * Traité au TEMPS MORT, avec « idle », et non sur les événements de
 * déplacement de souris. C'est l'architecture d'HyperCard, et elle nous
 * arrange doublement : rien à faire suivre à la fenêtre — pas de
 * setAcceptsMouseMovedEvents:, pas de mouseMoved: —, et surtout mouseWithin
 * est borné à dix envois par seconde au lieu d'un par pixel parcouru. Sur une
 * carte à cent boutons, la différence est exactement celle qu'on a passé la
 * journée à corriger ailleurs.
 *
 * Le prix : une transition est vue au dixième de seconde près. Pour un bouton
 * qui s'allume, personne ne le remarque.
 *
 * Trois gardes, chacun contre un vrai danger :
 *
 *   - seulement à l'outil doigt. Aux outils Bouton ou Champ, survoler un
 *     objet ne doit pas faire tourner son script : on l'AUTEURE, on ne s'en
 *     sert pas.
 *
 *   - rien pendant qu'on tient le bouton de la souris. Un glissement n'est
 *     pas un survol, et le script n'a pas à s'exécuter au milieu.
 *
 *   - à un changement de carte, on oublie l'objet survolé SANS lui envoyer
 *     mouseLeave. Il appartenait à l'ancienne carte et a pu être libéré :
 *     lui parler serait lire de la mémoire rendue. On compare les pointeurs
 *     de carte, on ne les déréférence jamais. */
static Object *gSurvole      = NULL;   /* l'objet sous le curseur          */
static Object *gSurvoleCarte = NULL;   /* et la carte où on l'a vu         */

/* ═══ UN OBJET MEURT ════════════════════════════════════════════════════
 *
 * L'interface garde des pointeurs sur des objets du noyau : l'objet survolé,
 * le sélectionné, le champ en cours d'édition, la cible d'un panneau, le
 * calque dont on tient l'instantané de Revert. Rien ne les prévenait qu'un
 * objet venait d'être libéré.
 *
 * « delete me » dans le gestionnaire d'un bouton laissait donc gSurvole sur
 * de la mémoire rendue, et le premier mouvement de souris ensuite envoyait
 * « mouseLeave » à un script qui n'existait plus : plantage dans
 * find_handler_k, en train de lire le script d'un objet mort. Le commentaire
 * juste au-dessus prévoyait déjà ce danger pour le CHANGEMENT DE CARTE — on
 * oublie l'objet survolé sans lui parler — mais pas pour sa suppression.
 *
 * Le noyau prévient maintenant, depuis hc_free, le seul endroit où un objet
 * meurt. Ici on oublie, sans jamais déréférencer : le pointeur ne sert qu'à
 * être comparé.
 *
 * La liste des emplacements est écrite UNE fois, ici. Tout Object * ajouté à
 * HCDoc doit y être ajouté aussi — c'est le prix d'un cache indexé par
 * adresse, et il vaut mieux le payer en un seul endroit qu'éparpillé. */
static void hcv_oublie_dans(HCDoc *d, Object *mort)
{
    if (!d) return;
    Object **emplacements[] = {
        &d->selected,
        &d->editingField, &d->editTarget, &d->pressed, &d->pressedCard,
        &d->popupTarget,
        &d->scrollField,  &d->clickField, &d->paintUndoLayer, &d->keepLayer,
        &d->card,
    };
    for (size_t i = 0; i < sizeof emplacements / sizeof *emplacements; i++)
        if (*emplacements[i] == mort) *emplacements[i] = NULL;
}

static void cocoa_object_gone(Object *mort)
{
    if (!mort) return;

    /* gSelected N'EST PLUS ICI, ET C'ÉTAIT LE PIÈGE DU DÉMÉNAGEMENT.
     *
     * Tant qu'il était global, l'effacer d'une ligne suffisait. Devenu un
     * champ de HCDoc, la même ligne n'aurait nettoyé QUE le document actif —
     * un objet peut mourir dans une pile qui n'est pas celle du dessus, et
     * la fenêtre d'à côté aurait gardé l'adresse d'un objet libéré. Il est
     * donc passé dans la liste de hcv_oublie_dans, qui est appelée pour
     * chaque document ; c'est exactement ce que le commentaire de cette
     * liste réclame de tout Object * ajouté à HCDoc. */
    if (gFontTarget   == mort) gFontTarget   = NULL;
    if (gSurvole      == mort) gSurvole      = NULL;
    if (gSurvoleCarte == mort) gSurvoleCarte = NULL;

    /* Le document sans fenêtre, puis chacun de ceux qui en ont une : un objet
     * peut mourir dans une pile qui n'est pas celle du dessus. */
    hcv_oublie_dans(&gDoc0, mort);
    for (HCDocument *doc in [HCDocument allDocuments]) {
        HCView *v = doc.view;
        if (v) hcv_oublie_dans((HCDoc *)[v docState], mort);
    }
}

static void hcv_survol(HCView *v, Object *carte)
{
    if (gTool != TOOL_BROWSE || gDragging || gPenDrawing || gFloatDragging) {
        gSurvole = NULL;
        gSurvoleCarte = carte;
        return;
    }

    if (carte != gSurvoleCarte) {          /* on a changé de carte */
        gSurvole = NULL;
        gSurvoleCarte = carte;
    }

    NSWindow *w = [v window];
    Object *sous = NULL;
    if (w && [w isKeyWindow]) {
        NSPoint p = [v convertPoint:[w mouseLocationOutsideOfEventStream]
                           fromView:nil];
        if (NSPointInRect(p, [v bounds])) sous = part_at(carte, p);
    }

    if (sous == gSurvole) {
        if (sous) hc_send(sous, "mouseWithin");
        return;
    }

    Object *ancien = gSurvole;
    gSurvole = sous;

    /* Un gestionnaire peut changer de carte sous nos pieds : on ne parle au
     * suivant que si l'on est toujours là où l'on croit être. */
    if (ancien) hc_send(ancien, "mouseLeave");
    /* On relit gSurvole plutôt que de réemployer `sous` : mouseLeave est du
     * script, il a pu supprimer l'objet qu'on s'apprêtait à saluer. Le noyau
     * remet alors gSurvole à NULL ; une variable locale, elle, ne l'apprend
     * jamais. La carte est vérifiée pour la même raison, depuis toujours. */
    if (gSurvole && hc_current_card() == carte) hc_send(gSurvole, "mouseEnter");
}

- (void)idleTick:(NSTimer *)t {
    (void)t;
    if (gInIdle || hc_is_running()) return;

    HCView *v = gView;
    if (!v) return;
    Object *card = [v documentCard];
    if (!card) return;

    /* LE CURSEUR D'UN SCRIPT NE DURE QUE JUSQU'AU REPOS.
     *
     * C'est la règle d'HyperCard, et sans elle « set cursor to watch » laisse
     * une montre à l'écran POUR TOUJOURS : le script qui l'a posée n'a aucune
     * raison de la retirer, puisqu'il annonçait seulement « attends-moi ».
     * Une application figée sur une montre alors qu'elle ne fait plus rien,
     * c'est pire que pas de montre du tout.
     *
     * Le moment est le bon : ce minuteur ne tourne pas tant qu'un gestionnaire
     * s'exécute (hc_is_running, juste au-dessus). La montre reste donc affichée
     * pendant toute la boucle, et s'en va quand le travail est fini — ce qui
     * est exactement ce qu'elle voulait dire. */
    if (gCursorScripte) {
        gCursorScripte    = NO;
        gCursorScripteObj = nil;
        if (gCursorHidden) { [NSCursor unhide]; gCursorHidden = NO; }
        gCursorNom = [NSString stringWithUTF8String:hcv_curseur_nom_outil((int)gTool)];
        hcv_curseur_maj();
    }

    gInIdle = YES;
    hcv_survol(v, card);
    hc_send(card, "idle");

    /* Et rafraîchir si l'un de ces gestionnaires a changé quelque chose.
     * Personne ne le faisait : un « on idle » qui bougeait un objet, ou un
     * mouseEnter qui allume un bouton, restaient invisibles jusqu'au prochain
     * redessin venu d'ailleurs. Le drapeau du noyau évite de tout repeindre
     * dix fois par seconde pour rien. */
    if (hc_take_visual_dirty()) [v setNeedsDisplay:YES];

    gInIdle = NO;
}

- (void)installMessageBox {
    gView = self;

    [self startIdleTimer];

    CGFloat w = 480, h = 30;
    gMsgPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(120, 120, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow |
                             NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered defer:NO];
    [gMsgPanel setTitle:@"Message"];
    [gMsgPanel setFloatingPanel:YES];
    [gMsgPanel setBecomesKeyOnlyIfNeeded:YES];
    [gMsgPanel setHidesOnDeactivate:YES];
    [gMsgPanel setReleasedWhenClosed:NO];

    gMsgBox = [[NSTextField alloc] initWithFrame:NSMakeRect(4, 3, w - 8, 24)];
    [gMsgBox setEditable:YES];
    [gMsgBox setSelectable:YES];
    [gMsgBox setBezeled:YES];
    [gMsgBox setDrawsBackground:YES];
    /* Fond et texte imposés ENSEMBLE.
     *
     * Le fond était figé à un blanc cassé, mais la couleur du texte restait
     * celle du système : en mode sombre elle passait au blanc, et la boîte
     * message devenait blanc sur blanc.
     *
     * La boîte message de HyperCard était noire sur blanc, et l'est restée
     * ici : c'est une fenêtre de PILE, pas un élément de l'interface de
     * macOS, et elle doit s'accorder à la carte qu'elle commande — laquelle
     * est blanche dans les deux modes. Imposer les deux couleurs plutôt que
     * suivre l'apparence est donc voulu. */
    [gMsgBox setBackgroundColor:[NSColor colorWithWhite:0.96 alpha:1.0]];
    [gMsgBox setTextColor:[NSColor blackColor]];
    [gMsgBox setAutoresizingMask:NSViewWidthSizable];
    [gMsgBox setStringValue:@""];
    [gMsgBox setTarget:self];
    [gMsgBox setAction:@selector(messageBoxEntered:)];
    [[gMsgPanel contentView] addSubview:gMsgBox];
    [gMsgPanel makeKeyAndOrderFront:nil];

    static HcHost host;
    host.line          = cocoa_line;
    host.erreur        = cocoa_erreur;
    host.field_changed = cocoa_field_changed;
    host.selection_changed = cocoa_selection_changed;
    host.ask           = cocoa_ask;
    host.answer_file   = cocoa_answer_file;
    host.ask_file      = cocoa_ask_file;
    host.save_stack    = cocoa_save_stack;
    host.open_stack    = cocoa_open_stack;
    host.load_stack    = cocoa_load_stack;
    hc_colors_init();
    host.print_cards   = cocoa_print_cards;
    host.stack_changed = cocoa_stack_changed;
    host.answer        = cocoa_answer;
    host.global_get    = cocoa_global_get;
    host.global_set    = cocoa_global_set;
    host.play_sound    = cocoa_play;
    host.choose_tool   = cocoa_choose_tool;
    host.drag          = cocoa_drag;
    host.click_at      = cocoa_click_at;
    host.type_text     = cocoa_type_text;
    host.visual_effect = cocoa_visual_effect;
    host.idle          = cocoa_idle;
    host.do_menu       = cocoa_do_menu;
    host.menus_changed = cocoa_menus_changed;
    host.object_gone   = cocoa_object_gone;
    hc_set_host(&host);

    /* Mettre la barre d'accord avec le modèle, une fois pour toutes.
     *
     * Une pile peut avoir été ouverte AVANT que l'hôte soit installé — un
     * double-clic sur un fichier au lancement appelle application:openFile:
     * sans garantie d'ordre. Son openStack aurait alors créé ses menus dans
     * le noyau, et menus_changed, encore nul, n'aurait prévenu personne : le
     * modèle serait juste, l'écran vide. Une ligne l'évite. */
    cocoa_menus_changed();
}

- (void)messageBoxEntered:(id)sender {
    NSString *cmd = [gMsgBox stringValue];
    if ([cmd length] == 0) return;
    hc_do([cmd UTF8String]);

    [gView applyStackSize];
    [gView updateWindowTitle];
    [gView setNeedsDisplay:YES];
    [gMsgBox selectText:nil];
}

static NSTimer *gSprayTimer = nil;
static NSView       *gSprayPreview = nil;
static NSTextField  *gSprayRadiusLabel = nil;
static NSTextField  *gSprayDensityLabel = nil;

- (void)startSprayTimer {
    [self stopSprayTimer];
    gSprayTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/24.0
                                                   target:self
                                                 selector:@selector(sprayTick:)
                                                 userInfo:nil
                                                  repeats:YES];
}

- (void)stopSprayTimer {
    if (gSprayTimer) { [gSprayTimer invalidate]; gSprayTimer = nil; }
}

- (void)sprayTick:(NSTimer *)t {
    if (!gPenDrawing || gTool != TOOL_SPRAY) { [self stopSprayTimer]; return; }
    Object *card = hc_current_card();
    if (!card) { [self stopSprayTimer]; return; }
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    spray_stamp(rep, (int)lround(gPenLast.x), (int)lround(gPenLast.y),
                gSprayRadius, gSprayDensity);
    [self setNeedsDisplay:YES];
}

- (void)showSprayPalette {
    NSAlert *a = [[NSAlert alloc] init];
    [a setMessageText:@"Aérographe"];
    [a setInformativeText:@"Taille du nuage et nombre de points par passe."];
    [a addButtonWithTitle:@"OK"];
    [a addButtonWithTitle:@"Annuler"];

    NSView *c = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 280, 142)];

    SprayPreview *prev = [[SprayPreview alloc] initWithFrame:NSMakeRect(0, 72, 280, 62)];
    [c addSubview:prev];
    gSprayPreview = prev;

    NSTextField *rl = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 42, 62, 18)];
    [rl setStringValue:@"Rayon"];
    [rl setBezeled:NO]; [rl setDrawsBackground:NO]; [rl setEditable:NO];
    [c addSubview:rl];

    NSSlider *rs = [[NSSlider alloc] initWithFrame:NSMakeRect(62, 40, 176, 20)];
    [rs setMinValue:1]; [rs setMaxValue:32];
    [rs setNumberOfTickMarks:32];
    [rs setAllowsTickMarkValuesOnly:YES];
    [rs setIntValue:gSprayRadius];
    [rs setContinuous:YES];
    [rs setTarget:self]; [rs setAction:@selector(sprayRadiusChanged:)];
    [c addSubview:rs];

    gSprayRadiusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(244, 42, 36, 18)];
    [gSprayRadiusLabel setBezeled:NO]; [gSprayRadiusLabel setDrawsBackground:NO];
    [gSprayRadiusLabel setEditable:NO];
    [gSprayRadiusLabel setAlignment:NSTextAlignmentRight];
    [c addSubview:gSprayRadiusLabel];

    NSTextField *dl = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 10, 62, 18)];
    [dl setStringValue:@"Densité"];
    [dl setBezeled:NO]; [dl setDrawsBackground:NO]; [dl setEditable:NO];
    [c addSubview:dl];

    NSSlider *ds = [[NSSlider alloc] initWithFrame:NSMakeRect(62, 8, 176, 20)];
    [ds setMinValue:1]; [ds setMaxValue:120];
    [ds setIntValue:gSprayDensity];
    [ds setContinuous:YES];
    [ds setTarget:self]; [ds setAction:@selector(sprayDensityChanged:)];
    [c addSubview:ds];

    gSprayDensityLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(244, 10, 36, 18)];
    [gSprayDensityLabel setBezeled:NO]; [gSprayDensityLabel setDrawsBackground:NO];
    [gSprayDensityLabel setEditable:NO];
    [gSprayDensityLabel setAlignment:NSTextAlignmentRight];
    [c addSubview:gSprayDensityLabel];

    [a setAccessoryView:c];
    [self updateSprayLabels];

    int oldR = gSprayRadius, oldD = gSprayDensity;

    if ([a runModal] != NSAlertFirstButtonReturn) {
        gSprayRadius  = oldR;
        gSprayDensity = oldD;
    }

    gSprayPreview       = nil;
    gSprayRadiusLabel   = nil;
    gSprayDensityLabel  = nil;
}

- (void)updateSprayLabels {
    [gSprayRadiusLabel  setStringValue:[NSString stringWithFormat:@"%d", gSprayRadius]];
    [gSprayDensityLabel setStringValue:[NSString stringWithFormat:@"%d", gSprayDensity]];
    [gSprayPreview setNeedsDisplay:YES];
}

- (void)sprayRadiusChanged:(id)sender {
    gSprayRadius = [sender intValue];
    [self updateSprayLabels];
}

- (void)sprayDensityChanged:(id)sender {
    gSprayDensity = [sender intValue];
    [self updateSprayLabels];
}

- (void)installToolPalette {
    int cols = 4, rows = (NUM_TOOLCELLS + cols - 1) / cols;
    CGFloat cell = 38, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;

    gToolPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(560, 350, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow |
                             NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered defer:NO];
    [gToolPanel setTitle:@"Tools"];
    [gToolPanel setFloatingPanel:YES];
    [gToolPanel setBecomesKeyOnlyIfNeeded:YES];
    [gToolPanel setHidesOnDeactivate:YES];
    [gToolPanel setReleasedWhenClosed:NO];

    ToolPalette *grid = [[ToolPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gToolPanel setContentView:grid];
    [gToolPanel makeKeyAndOrderFront:nil];
}

- (void)showToolPalette {
    if (!gToolPanel) [self installToolPalette];
    else [gToolPanel makeKeyAndOrderFront:nil];
}

- (void)togglePalette:(id)sender {
    NSInteger tag = [sender tag];
    NSPanel *p = nil;
    switch (tag) {
        case 1: if (!gToolPanel)    { [self installToolPalette];    return; } p = gToolPanel;    break;
        case 2: if (!gPatternPanel) { [self installPatternPalette]; return; } p = gPatternPanel; break;
        case 3: if (!gWidthPanel)   { [self installWidthPalette];   return; } p = gWidthPanel;   break;
        case 4: if (!gBrushPanel)   { [self installBrushPalette];   return; } p = gBrushPanel;   break;
        case 5: if (!gMsgPanel)     { [self installMessageBox];     return; } p = gMsgPanel;     break;
        default: return;
    }
    if ([p isVisible]) [p orderOut:nil];
    else               [p makeKeyAndOrderFront:nil];
}

- (BOOL)paletteVisibleForTag:(NSInteger)tag {
    switch (tag) {
        case 1: return gToolPanel    && [gToolPanel isVisible];
        case 2: return gPatternPanel && [gPatternPanel isVisible];
        case 3: return gWidthPanel   && [gWidthPanel isVisible];
        case 4: return gBrushPanel   && [gBrushPanel isVisible];
        case 5: return gMsgPanel     && [gMsgPanel isVisible];
    }
    return NO;
}

- (void)toggleFilled:(id)sender {
    gShapeFilled = !gShapeFilled;
     
}

- (void)toolChosen:(id)sender {
    [self dropFloating];
    
    HCTool newTool = (HCTool)[sender tag];
    
    if (gEditingField) {
        [self endFieldEdit];
    }
    
    [[self window] makeFirstResponder:self];
    
    gTool = newTool;
    gSelected = NULL;
    [self setNeedsDisplay:YES];
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

    NSButton *verif = [[NSButton alloc] initWithFrame:NSMakeRect(290, 8, 90, 30)];
    [verif setTitle:@"Vérifier"];
    [verif setBezelStyle:NSBezelStyleRounded];
    [verif setTarget:self];
    [verif setAction:@selector(checkScript:)];
    [verif setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];
    [[panel contentView] addSubview:verif];

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

- (void)selectLine:(int)ligne inTextView:(NSTextView *)tv {
    NSString *s = [tv string];
    NSUInteger deb = 0, len = [s length];
    int courante = 1;

    if (len == 0) return;

    while (deb < len && courante < ligne) {
        NSRange r = [s lineRangeForRange:NSMakeRange(deb, 0)];
        deb = NSMaxRange(r);
        courante++;
    }
    if (deb >= len) deb = len - 1;

    NSRange r = [s lineRangeForRange:NSMakeRange(deb, 0)];
    while (r.length > 0) {
        unichar c = [s characterAtIndex:NSMaxRange(r) - 1];
        if (c != '\n' && c != '\r') break;
        r.length--;
    }

    [tv setSelectedRange:r];
    [tv scrollRangeToVisible:r];
    [[tv window] makeFirstResponder:tv];
}

- (void)checkScript:(id)sender {
    (void)sender;
    if (!gEditView) return;

    const char *src = [[gEditView string] UTF8String];
    HctRapport rap;
    hct_verifie(src ? src : "", &rap, YES);

    if (rap.n == 0) {
        NSAlert *a = [[NSAlert alloc] init];
        [a setMessageText:@"Script correct"];
        [a setInformativeText:@"Aucune faute détectée."];
        [a runModal];
        hct_rapport_libere(&rap);
        return;
    }

    NSMutableString *txt = [NSMutableString string];
    for (int i = 0; i < rap.n; i++) {
        HctSignalement *s = &rap.liste[i];
        [txt appendFormat:@"%@ ligne %d : %s",
             s->niveau == HCT_V_ERREUR ? @"Erreur" : @"Attention",
             s->ligne, s->message];
        if (s->extrait[0]) [txt appendFormat:@"  [%s]", s->extrait];
        [txt appendString:@"\n"];
    }

    const HctSignalement *p = hct_premier(&rap);
    if (p) [self selectLine:p->ligne inTextView:gEditView];

    NSAlert *a = [[NSAlert alloc] init];
    [a setMessageText:rap.nerreurs
        ? [NSString stringWithFormat:@"%d faute%@ de syntaxe", rap.nerreurs,
                                     rap.nerreurs > 1 ? @"s" : @""]
        : @"Script correct, avec des remarques"];
    [a setInformativeText:txt];
    [a runModal];

    hct_rapport_libere(&rap);
}

- (void)saveScript:(id)sender {
    if (gEditTarget && gEditView) {
        hc_set_script(gEditTarget, [[gEditView string] UTF8String]);
    }
    [gEditPanel close];
    gEditPanel = nil; gEditView = nil; gEditTarget = nil;
    [gView setNeedsDisplay:YES];
}

- (void)beginFieldEdit:(Object *)field {
    if (!field) return;

    /* Interdiction d'éditer si l'outil n'est pas BROWSE ou si le champ est verrouillé */
    if (gTool != TOOL_BROWSE || field->locktext) {
        if (gEditingField) [self endFieldEdit];
        [[self window] makeFirstResponder:self];
        [self setNeedsDisplay:YES];
        return;
    }

    [self endFieldEdit];
    gEditingField = field;

    NSRect r = field_text_rect(field);
    BOOL isScroll = (field->style && strcmp(field->style, "scrolling") == 0);

    /* 1. Conteneur ScrollView ajusté et verrouillé au cadre */
    gFieldScroll = [[NSScrollView alloc] initWithFrame:r];
    [gFieldScroll setHasVerticalScroller:NO];
    [gFieldScroll setHasHorizontalScroller:NO];
    [gFieldScroll setAutohidesScrollers:YES];
    [gFieldScroll setBorderType:NSNoBorder];
    [gFieldScroll setDrawsBackground:NO];
    [gFieldScroll setWantsLayer:YES];
    [[gFieldScroll layer] setMasksToBounds:YES];

    NSSize sz = [gFieldScroll contentSize];

    /* 2. Instanciation et configuration du NSTextView */
    gFieldEditor = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, sz.width, sz.height)];
    [gFieldEditor setMinSize:NSMakeSize(0, 0)];
    [gFieldEditor setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
    [gFieldEditor setVerticallyResizable:YES];
    [gFieldEditor setHorizontallyResizable:NO];
    [gFieldEditor setAutoresizingMask:NSViewWidthSizable];

    /* 3. Métriques et suppression des marges internes */
    NSTextContainer *container = [gFieldEditor textContainer];
    [container setContainerSize:NSMakeSize(sz.width, FLT_MAX)];
    [container setWidthTracksTextView:YES];
    [container setLineFragmentPadding:0];

    [gFieldEditor setTextContainerInset:NSMakeSize(0, 0)];
    [gFieldEditor setDrawsBackground:NO];
    [gFieldEditor setDelegate:self];

    /* Alignement typographique classique Cocoa */
    NSLayoutManager *lm = [gFieldEditor layoutManager];
    [lm setUsesFontLeading:YES];
    [lm setTypesetterBehavior:NSTypesetterBehavior_10_2_WithCompatibility];

    /* Annule les variations de baseline de la première ligne selon les polices (Geneva vs Times) */
        [gFieldEditor setDisplaysLinkToolTips:NO];
        [lm setBackgroundLayoutEnabled:NO];
    
    [gFieldEditor setRichText:YES];
    [gFieldEditor setImportsGraphics:NO];
    [gFieldEditor setUsesFontPanel:YES];
    [gFieldEditor setAllowsUndo:YES];

    /* 4. Chargement du texte et synchronisation de l'interligne statique */
    const char *tx = hc_field_text(field);
    NSString *str = [NSString stringWithUTF8String:tx ? tx : ""];
    NSDictionary *base = obj_attrs(field, 12, [NSColor blackColor]);

    [gFieldEditor setEditable:!field->locktext];
    [gFieldEditor setSelectable:YES];

    gForEditor = YES;
    [[gFieldEditor textStorage] setAttributedString:field_attr_string(field, str, base)];
    gForEditor = NO;

    /* Application stricte de la hauteur de ligne pour correspondre au mode Browse */
    NSMutableDictionary *tattr = [base mutableCopy];
    NSMutableParagraphStyle *ps = [[tattr objectForKey:NSParagraphStyleAttributeName] mutableCopy];
    if (!ps) ps = [[NSMutableParagraphStyle alloc] init];

    CGFloat lh = hc_text_height(field);
    if (lh > 0) {
        [ps setMinimumLineHeight:lh];
        [ps setMaximumLineHeight:lh];
    }
    tattr[NSParagraphStyleAttributeName] = ps;
    [gFieldEditor setTypingAttributes:tattr];

    [gFieldScroll setDocumentView:gFieldEditor];

    /* Réinitialisation de l'origine du document */
    NSRect df = [gFieldEditor frame];
    if (df.origin.x != 0 || df.origin.y != 0) {
        df.origin = NSZeroPoint;
        [gFieldEditor setFrame:df];
    }

    [self addSubview:gFieldScroll];

    /* 5. Prise en charge du défilement pour les champs scrolling */
    if (isScroll) {
        field_clamp_scroll(field);
        if (field->scroll > 0) {
            [[gFieldEditor layoutManager] ensureLayoutForTextContainer:container];
            [[gFieldScroll contentView] scrollToPoint:NSMakePoint(0, field->scroll)];
            [gFieldScroll reflectScrolledClipView:[gFieldScroll contentView]];
        }

        [[gFieldScroll contentView] setPostsBoundsChangedNotifications:YES];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(fieldEditorDidScroll:)
                                                     name:NSViewBoundsDidChangeNotification
                                                   object:[gFieldScroll contentView]];
    }

    [[self window] makeFirstResponder:gFieldEditor];

    /* openField en DERNIER, quand l'éditeur est entièrement en place : le
     * gestionnaire est du script, il peut sélectionner du texte, changer de
     * carte, refermer le champ. Tout ce qu'il touche doit déjà exister. */
    gTexteAuDebut = [[gFieldEditor string] copy];
    hc_send(field, "openField");
}

- (void)scrollWheel:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    Object *hit = part_at(hc_current_card(), p);
    if (!hit || hit->type != OBJ_FIELD ||
        !(hit->style && strcmp(hit->style, "scrolling") == 0)) {
        [super scrollWheel:event];
        return;
    }

    CGFloat dy = [event scrollingDeltaY];
    if ([event hasPreciseScrollingDeltas]) hit->scroll -= (int)lround(dy);
    else                                   hit->scroll -= (int)lround(dy * 16);

    field_clamp_scroll(hit);
    sync_editor_scroll(hit);
    [self setNeedsDisplay:YES];
}

- (void)fieldEditorDidScroll:(NSNotification *)note {
    if (gSyncingEditorScroll) return;
    if (!gEditingField || !gFieldScroll) return;
    gEditingField->scroll =
        (int)lround(editor_fraction() * field_max_scroll(gEditingField));
    [self setNeedsDisplay:YES];
}

- (void)textViewDidChangeSelection:(NSNotification *)note {
    if (gApplyingSelection) return;
    if (!gEditingField || !gFieldEditor) return;
    if ([note object] != gFieldEditor) return;

    NSRange r = [gFieldEditor selectedRange];
    /* UTF-16 -> OCTETS. NSTextView compte en unités UTF-16, le noyau en
     * octets. Passer la NSRange telle quelle marchait tant que tout était en
     * ASCII, où les deux coïncident ; sur « été », sélectionner le premier é
     * donne une longueur Cocoa de 1, et le noyau retenait alors UN octet —
     * une demi-séquence UTF-8, que « the selection » ne pouvait plus rendre.
     *
     * byte_from_utf16 existait déjà et servait ailleurs dans ce fichier ; il
     * ne manquait qu'ici. */
    NSString *str = [gFieldEditor string];
    int b0 = byte_from_utf16(str, r.location);
    int b1 = byte_from_utf16(str, r.location + r.length);
    gApplyingSelection = YES;
    hc_set_selection(gEditingField, b0, b1 - b0);
    gApplyingSelection = NO;
}

- (void)endFieldEdit {
    if (gFieldEditor && gEditingField) {
        if ([[self window] firstResponder] == gFieldEditor) {
            [[self window] makeFirstResponder:self];
        }

        NSString *str = [gFieldEditor string];

        hc_set_field_text(gEditingField, [str UTF8String]);
        hc_runs_clear(gEditingField);

        NSTextStorage *ts = [gFieldEditor textStorage];

        NSFont *fbase = obj_base_font(gEditingField, 12);
        NSString *fbaseName = [fbase familyName];
        if (!fbaseName) fbaseName = [fbase fontName];
        int fbaseSize = (int)lround([fbase pointSize]);
        int fbaseStyle = gEditingField->textstyle;

        NSUInteger len = [ts length], i = 0;
        while (i < len) {
            NSRange eff;
            NSDictionary *a = [ts attributesAtIndex:i
                              longestEffectiveRange:&eff
                                            inRange:NSMakeRange(i, len - i)];
            int st = style_bits_from_attrs(a);

            NSFont *rf = a[NSFontAttributeName];
            const char *fname = NULL;
            int fsize = 0;
            if (rf) {
                NSFontDescriptor *fd = [[rf fontDescriptor]
                    fontDescriptorWithSymbolicTraits:
                        [[rf fontDescriptor] symbolicTraits]
                        & ~(NSFontDescriptorTraitBold | NSFontDescriptorTraitItalic)];
                NSFont *plain = [NSFont fontWithDescriptor:fd size:[rf pointSize]];
                NSFont *src = plain ? plain : rf;
                NSString *nm = [src familyName];
                if (!nm) nm = [src fontName];
                int sz = (int)lround([rf pointSize]);

                if (![nm isEqualToString:fbaseName]) fname = [nm UTF8String];
                if (sz != fbaseSize)                 fsize = sz;
            }

            int fcolor = HC_COLOR_INHERIT;
            {
                /* La couleur récoltée dans l'éditeur Cocoa.
                 *
                 * On rejetait ici le noir ET le blanc, pour ne pas enregistrer
                 * une couleur là où l'utilisateur n'en avait posé aucune. Mais
                 * le blanc est une couleur qu'on choisit délibérément — sur un
                 * bandeau sombre, c'est même la seule qui convienne — et le
                 * noir en est une aussi dès qu'un champ a une autre couleur par
                 * défaut. Les deux se perdaient donc en refermant l'éditeur,
                 * alors que « set the textColor to white » les gardait.
                 *
                 * On ne rejette plus rien : c'est le TEXTE NON COLORÉ qui doit
                 * se reconnaître, et l'absence d'attribut s'en charge déjà,
                 * `rc` valant alors nil. */
                NSColor *rc = a[NSForegroundColorAttributeName];
                if (rc) {
                    NSColor *rgb = [rc colorUsingColorSpace:
                                        [NSColorSpace sRGBColorSpace]];
                    if (rgb) {
                        CGFloat rc0 = [rgb redComponent];
                        CGFloat gc0 = [rgb greenComponent];
                        CGFloat bc0 = [rgb blueComponent];
                        if (rc0 < 0) rc0 = 0;
                        if (rc0 > 1) rc0 = 1;
                        if (gc0 < 0) gc0 = 0;
                        if (gc0 > 1) gc0 = 1;
                        if (bc0 < 0) bc0 = 0;
                        if (bc0 > 1) bc0 = 1;
                        int r = (int)lround(rc0 * 255);
                        int g = (int)lround(gc0 * 255);
                        int b = (int)lround(bc0 * 255);
                        fcolor = (r << 16) | (g << 8) | b;
                    }
                }
            }

            if (st == fbaseStyle && !fname && fsize == 0 &&
                fcolor == HC_COLOR_INHERIT) {
                i = NSMaxRange(eff);
                if (eff.length == 0) break;
                continue;
            }

            int b0 = byte_from_utf16(str, eff.location);
            int b1 = byte_from_utf16(str, eff.location + eff.length);
            hc_run_add_color(gEditingField, b0, b1 - b0, st, fsize, fname, fcolor);
            i = NSMaxRange(eff);
            if (eff.length == 0) break;
        }
    }
    if (gEditingField && gFieldScroll &&
        gEditingField->style && strcmp(gEditingField->style, "scrolling") == 0) {
        gEditingField->scroll =
            (int)lround(editor_fraction() * field_max_scroll(gEditingField));
        field_clamp_scroll(gEditingField);
    }

    if (gFieldScroll)
        [[NSNotificationCenter defaultCenter]
            removeObserver:self
                      name:NSViewBoundsDidChangeNotification
                    object:[gFieldScroll contentView]];

    [gFieldEditor setDelegate:nil];

    /* Ce qu'il faut retenir AVANT de tout effacer : à qui parler, et si le
     * texte a bougé. */
    Object   *champ   = gEditingField;
    NSString *final   = gFieldEditor ? [gFieldEditor string] : nil;
    BOOL      change  = (champ && gTexteAuDebut && final &&
                         ![final isEqualToString:gTexteAuDebut]);

    [gFieldScroll removeFromSuperview];
    gFieldScroll = nil;
    gFieldEditor = nil;
    gEditingField = NULL;
    gTexteAuDebut = nil;
    [self setNeedsDisplay:YES];

    /* Le message part une fois l'état rendu : un gestionnaire qui rappellerait
     * endFieldEdit — en changeant de carte, par exemple — ne trouve alors plus
     * rien à fermer, et ne peut pas boucler. */
    if (champ && !gSansMessageChamp)
        hc_send(champ, change ? "closeField" : "exitField");
}
/* LE GESTE DE LA FENÊTRE A DOIT GARDER L'ÉTAT DE A.
 *
 * Ces trois méthodes passaient par gDoc — le document ACTIF — alors que le
 * geste appartient à la vue qui l'a commencé. Le scénario qui casse :
 *
 *     souris enfoncée dans la pile A ;
 *     le mouseDown de A fait « go to stack "B" » ;
 *     B devient le document actif, donc gDoc pointe sur B ;
 *     le minuteur créé par A se déclenche et lit gPressed — celui de B.
 *
 * Résultat : mouseStillDown ne part plus, ou part au mauvais objet. Et comme
 * le minuteur était un GLOBAL, la fenêtre B écrasait en plus celui de A.
 *
 * On travaille donc sur _doc, l'état de CETTE vue, jamais sur gDoc. */
- (void)startStillDownTimer {
    [self stopStillDownTimer];
    _doc.stillDownTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0
                                                       target:self
                                                     selector:@selector(stillDownTick:)
                                                     userInfo:nil
                                                      repeats:YES];

    /* Le mode « common » fait continuer le minuteur pendant qu'un menu est
     * ouvert ou qu'on redimensionne la fenêtre ; sans lui il se fige, et le
     * script cesse de recevoir mouseStillDown au pire moment.
     *
     * Tolérance nulle : macOS regroupe volontiers les déclenchements pour
     * économiser l'énergie, ce qui produit exactement les à-coups qu'on
     * cherche à supprimer. */
    [[NSRunLoop currentRunLoop] addTimer:_doc.stillDownTimer
                                 forMode:NSRunLoopCommonModes];
    [_doc.stillDownTimer setTolerance:0];
}

- (void)stopStillDownTimer {
    [_doc.stillDownTimer invalidate];
    _doc.stillDownTimer = nil;
}

- (void)stillDownTick:(NSTimer *)t {
    (void)t;
    /* _doc, et non gPressed : voir startStillDownTimer. */
    Object *presse = _doc.pressed;
    /* Une vue sans fenêtre n'a plus de geste en cours. Un minuteur RETIENT sa
     * cible : sans cette sortie, fermer la fenêtre pendant que le bouton est
     * enfoncé laisserait la vue en vie et le minuteur battre dans le vide. */
    if (![self window] || !presse || !([NSEvent pressedMouseButtons] & 1)) {
        [self stopStillDownTimer];
        return;
    }
    hc_send(presse, "mouseStillDown");
}
@end
