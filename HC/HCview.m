#import "HCview.h"
#import "hc_core.h"
#import <objc/runtime.h>   // pour associer un bitmap à un Object
#include <stdlib.h>        // getenv, pour la trace HC_RUNS_DEBUG
#include <strings.h>       // strcasecmp, pour les noms de proprietes globales
#include <float.h>         // FLT_MAX, pour la taille libre de l'editeur de champ
#import <QuartzCore/QuartzCore.h>  // CATransaction, pour pousser les pixels a l ecran
#import "icons.h"
#import "HCglobals.h"
#import "HCpalettes.h"
#import "HCicons.h"
#import "graphics.h"
#import "HCdialogs.h"
extern void hc_sync_size_field(Object *o);  // definie dans HCdialogs.m
static NSTextField *gMsgBox = nil; // la message box

static NSPoint gDragStart;
static NSRect  gDragRect;
static BOOL    gDragging = NO;
static int     gNewCount = 0;
static Object *gEditTarget = NULL;
static NSTextView *gEditView = nil;
static NSPanel *gEditPanel = nil;
static Object *gPressed = NULL;
static BOOL    gMoving = NO;        // on déplace l'objet sélectionné
static NSPoint gMoveStart;         // point de départ du déplacement
static int     gObjStartX, gObjStartY;  // position de l'objet au départ
static int     gResizeHandle = 0;  // 0 = pas de resize, 1..4 = coin saisi
static int     gObjStartW, gObjStartH;
static NSTextView *gFieldEditor = nil;
/* La NSTextView vit désormais dans une NSScrollView, qui est la sous-vue
 * réellement montée : c'est elle qu'il faut retirer à la fermeture. */
static NSScrollView *gFieldScroll = nil;
static Object     *gEditingField = NULL;
static NSPoint gPenLast;
static BOOL    gPenDrawing = NO;
static BOOL gEditBackground = NO;   // NO = couche carte, YES = couche fond
// peint un segment de ligne dans le bitmap de la carte courante
static NSPoint gShapeStart;
static NSPoint gShapeEnd;
static BOOL    gShapeDrawing = NO;
 
 
static NSPoint gLassoPts[4096];
static int gLassoCount = 0;
static BOOL gLassoDrawing = NO;
static BOOL gLassoActive = NO;   // une sélection existe-t-elle ?


static NSPoint gSelStart, gSelEnd; // selection rectabgle
static BOOL gSelRectDrawing = NO;
static BOOL gSelRectActive = NO;

static NSPanel *gPatternPanel = nil;
static NSPanel *gToolPanel = nil;
static NSPanel *gWidthPanel = nil;
static NSPanel *gBrushPanel = nil;

// BOOL gTransparentBg = NO;

static BOOL gTextActive = NO;
static NSPoint gTextPos;                 // coin haut-gauche de la saisie
static NSMutableString *gTextBuf = nil;

// int gTextSize = 16;
static NSColor *gTextColor = nil;
static BOOL gTextUnderline = NO;

static Object *gPopupTarget = NULL;


static Object *gScrollField = NULL;
static CGFloat gScrollGrab, gScrollGH, gScrollKH, gScrollGY, gScrollMax;



 #define NUM_PATTERNS 38

// #define NUM_PATTERNS (int)(sizeof(PATTERNS)/sizeof(PATTERNS[0]))


 // int gPattern = 2;   // motif courant (1 = noir plein)
static NSPoint gFreePts[4096];
static int gFreeCount = 0;
static BOOL gFreeDrawing = NO;




static BOOL gFloating = NO;        // un collage flotte-t-il ?
static NSPoint gFloatPos;          // position (coin haut-gauche) du flottant
static BOOL gFloatDragging = NO;   // en train de le déplacer ?
static NSPoint gFloatGrab;         // décalage entre le clic et le coin
static NSFont *gTextFont = nil;




// éteint tous les radioButtons de la carte sauf 'keep'
static void radio_exclusive(Object *card, Object *keep) {
    if (!card) return;
    for (int i = 0; i < card->nparts; i++) {
        Object *o = card->parts[i];
        if (o->type == OBJ_BUTTON && o != keep && o->style &&
            (strcmp(o->style, "radioButton") == 0 || strcmp(o->style, "radiobutton") == 0))
            o->hilite = 0;
    }
    if (card->bg)
        for (int i = 0; i < card->bg->nparts; i++) {
            Object *o = card->bg->parts[i];
            if (o->type == OBJ_BUTTON && o != keep && o->style &&
                (strcmp(o->style, "radioButton") == 0 || strcmp(o->style, "radiobutton") == 0))
                o->hilite = 0;
        }
}


// dessine un objet (bouton ou champ) à son rectangle
static NSDictionary *obj_attrs(Object *o, CGFloat defSize, NSColor *color);

/* ---- dessine le nom d'un bouton, centre dans le rect ---- */
static void draw_btn_label(Object *o, NSString *s, NSRect r, BOOL on, CGFloat defSize) {
    if (!o->showname) return;
    CGFloat fs = o->textsize > 0 ? o->textsize : defSize;
    NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
    [ps setAlignment:NSTextAlignmentCenter];

    /* Ce chemin fabriquait son dictionnaire à la main, avec
     * boldSystemFontOfSize: en dur : la police ET le style de l'objet étaient
     * ignorés, seul le corps passait. D'où « seul textSize fonctionne sur les
     * boutons ». Il partage désormais obj_attrs avec les autres chemins. */
    NSMutableDictionary *attrs =
        [obj_attrs(o, defSize, on ? [NSColor whiteColor]
                                  : [NSColor blackColor]) mutableCopy];
    attrs[NSParagraphStyleAttributeName] = ps;

    NSRect tr = NSInsetRect(r, 4, 0);
    tr.origin.y += (r.size.height - fs * 1.2) / 2;
    [s drawInRect:tr withAttributes:attrs];
}

/* ---- contour pointille montre en mode edition ---- */
static void draw_edit_outline(NSRect r) {
    if (gTool != TOOL_BUTTON && gTool != TOOL_FIELD) return;
    [[NSColor colorWithWhite:0.6 alpha:1.0] setStroke];
    NSBezierPath *outline = [NSBezierPath bezierPathWithRect:r];
    [outline setLineWidth:1];
    CGFloat dash[] = {3, 2};
    [outline setLineDash:dash count:2 phase:0];
    [outline stroke];
}
/* ---- dessine le fond et le cadre d'un bouton selon son style ---- */
static void draw_btn_frame(Object *o, NSRect r, BOOL on) {
    const char *st = o->style ? o->style : "rectangle";

    if (strcmp(st, "transparent") == 0) {
        if (on) {
            [[NSColor colorWithWhite:0.0 alpha:0.15] setFill];
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
        [(on ? [NSColor blackColor] : [NSColor colorWithWhite:0.9 alpha:1.0]) setFill];
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
    // rectangle par defaut
    [(on ? [NSColor blackColor] : [NSColor colorWithWhite:0.9 alpha:1.0]) setFill];
    NSRectFill(r);
    [[NSColor blackColor] setStroke];
    NSFrameRect(r);
}
static NSFont *font_with_traits(NSFont *f, BOOL bold, BOOL italic, BOOL *faux);
static NSFont *font_by_loose_name(NSString *want, CGFloat sz);
static NSMutableDictionary *style_attrs(int style, NSFont *base, NSColor *color);

/* La police d'un objet SANS ses traits : famille et corps seulement. Les
 * traits sont posés ensuite par style_attrs, qui sait aussi les simuler.
 * Les garder séparés évite la double source de vérité entre le nom de fonte
 * et les bits de style, qui nous a déjà coûté cher. */
static NSFont *obj_base_font(Object *o, CGFloat defSize) {
    CGFloat sz = o->textsize > 0 ? o->textsize : defSize;
    NSFont *f = nil;
    if (o->textfont && *o->textfont)
        f = font_by_loose_name([NSString stringWithUTF8String:o->textfont], sz);
    if (!f) f = [NSFont systemFontOfSize:sz];
    return f;
}

static NSFont *obj_font(Object *o, CGFloat defSize) {
    /* Même chemin que les plages de style : convertFont:toHaveTrait: rend la
     * fonte système inchangée et sans rien dire, si bien qu'un champ resté
     * dans la police par défaut ne pouvait pas passer au gras — et que le
     * panneau de polices, en recevant une fonte sans trait, croyait le gras
     * éteint et l'écrasait au coup suivant. */
    return font_with_traits(obj_base_font(o, defSize),
                            (o->textstyle & HC_BOLD)   ? YES : NO,
                            (o->textstyle & HC_ITALIC) ? YES : NO, NULL);
}





 

/* ---- attributs de texte d'un objet ---- */
/* Attributs de dessin d'un objet — nom de bouton, étiquette de case à cocher,
 * ligne courante d'un popup. Passe désormais par style_attrs, si bien qu'un
 * bouton rend le creux, l'ombré, l'approche et les traits simulés exactement
 * comme un champ. Auparavant il ne connaissait que le souligné, et « set the
 * textStyle of button "x" to outline » restait sans effet visible. */
static NSDictionary *obj_attrs(Object *o, CGFloat defSize, NSColor *color) {
    return style_attrs(o->textstyle, obj_base_font(o, defSize), color);
}
/* `group` n'a aucun equivalent Cocoa : c'est une semantique HyperCard, pas un
 * rendu. On le porte comme attribut personnalise, sinon il disparaitrait au
 * premier aller-retour par l'editeur. */
static NSString * const kHCGroupAttribute = @"HCGroup";

/* Le gras synthétique et le contour se disputaient NSStrokeWidth : poser l'un
 * effaçait l'autre, et « gras + creux » revenait creux tout court. On garde
 * donc une trace explicite du gras simulé, au lieu de la déduire du signe du
 * trait. */
static NSString * const kHCFauxBoldAttribute = @"HCFauxBold";

/* ---- texte attribué d'un champ : les plages de style ----
 *
 * Le noyau tient des plages de caractères (hc_run_count / hc_run_at). On part
 * des attributs du champ entier, puis on surcharge chaque plage. Les plages
 * arrivent triées et sans recouvrement, il n'y a donc aucune imbrication à
 * démêler ici.
 */

/* Les plages du noyau sont des décalages en OCTETS dans le texte UTF-8, alors
 * que NSString compte en unités UTF-16. C'est identique tant que le texte est
 * en ASCII — le calendrier l'est — et faux dès le premier accent. */
static NSUInteger utf16_from_byte(const char *utf8, int byteoff)
{
    if (byteoff <= 0) return 0;
    NSString *pre = [[NSString alloc] initWithBytes:utf8
                                             length:(NSUInteger)byteoff
                                           encoding:NSUTF8StringEncoding];
    return pre ? [pre length] : (NSUInteger)byteoff;   /* coupe au milieu d'un
                                                        * caractère : repli */
}

/* Applique gras/italique de façon fiable.
 *
 * NSFontManager convertFont:toHaveTrait: échoue silencieusement sur la police
 * système : San Francisco n'est pas exposée par nom de famille, et on récupère
 * la même fonte sans le moindre avertissement. On passe donc par le
 * descripteur et ses traits symboliques, qui, eux, la connaissent. Si même
 * cela échoue — police sans variante grasse — on grossit le trait, ce que le
 * Macintosh appelait le gras synthétique. */
static NSFont *font_with_traits(NSFont *f, BOOL bold, BOOL italic, BOOL *faux)
{
    if (faux) *faux = NO;
    if (!f || (!bold && !italic)) return f;

    NSFontDescriptorSymbolicTraits want = 0;
    if (bold)   want |= NSFontDescriptorTraitBold;
    if (italic) want |= NSFontDescriptorTraitItalic;

    NSFontDescriptor *fd =
        [[f fontDescriptor] fontDescriptorWithSymbolicTraits:
            [[f fontDescriptor] symbolicTraits] | want];
    NSFont *nf = [NSFont fontWithDescriptor:fd size:[f pointSize]];

    if (!nf) {                              /* repli : l'ancienne méthode */
        NSFontManager *fm = [NSFontManager sharedFontManager];
        nf = f;
        if (bold)   nf = [fm convertFont:nf toHaveTrait:NSBoldFontMask];
        if (italic) nf = [fm convertFont:nf toHaveTrait:NSItalicFontMask];
    }
    if (!nf) nf = f;

    /* A-t-on vraiment obtenu le gras ? Sinon on le simulera au trait.
     * L'italique n'a pas d'équivalent : sur une famille sans variante penchée,
     * « to italic » reste stocké dans le noyau mais ne se voit pas — c'est le
     * choix retenu, plutôt qu'une inclinaison synthétique. */
    if (bold && faux) {
        NSFontDescriptorSymbolicTraits got = [[nf fontDescriptor] symbolicTraits];
        if (!(got & NSFontDescriptorTraitBold)) *faux = YES;
    }
    return nf;
}

/* Cocoa veut le nom exact : « monaco » ne rend rien, « Monaco » rend la
 * police. HyperCard, lui, se moquait de la casse — les scripts d'origine
 * écrivent « geneva » aussi souvent que « Geneva ». On tente donc le nom tel
 * quel, puis on le cherche parmi les familles installées en ignorant la
 * casse. Sans cela un « set the textFont … to "monaco" » était accepté par le
 * noyau, stocké, relu — et restait invisible à l'écran. */
static NSFont *font_by_loose_name(NSString *want, CGFloat sz)
{
    if (![want length]) return nil;

    NSFont *f = [NSFont fontWithName:want size:sz];
    if (f) return f;

    NSFontManager *fm = [NSFontManager sharedFontManager];

    /* Par famille : « Monaco », « Times New Roman »… C'est ce qu'écrivent les
     * scripts HyperCard. fontWithName: accepte un nom de famille, mais exige
     * la casse exacte, d'où la comparaison souple. */
    for (NSString *fam in [fm availableFontFamilies])
        if ([fam caseInsensitiveCompare:want] == NSOrderedSame) {
            if ((f = [NSFont fontWithName:fam size:sz])) return f;
            /* Famille reconnue mais sans membre au nom de la famille : on
             * prend son premier membre (« Helvetica Neue » -> « HelveticaNeue-
             * Regular »). availableMembersOfFontFamily rend des tableaux dont
             * le premier élément est le nom de fonte. */
            for (NSArray *m in [fm availableMembersOfFontFamily:fam]) {
                if ([m count] < 1) continue;
                if ((f = [NSFont fontWithName:m[0] size:sz])) return f;
            }
        }

    /* Dernier recours : un nom de FONTE et non de famille — « Courier-Bold »,
     * que certains scripts écrivent tel quel. */
    for (NSString *nm in [fm availableFonts])
        if ([nm caseInsensitiveCompare:want] == NSOrderedSame)
            if ((f = [NSFont fontWithName:nm size:sz])) return f;
    return nil;
}

/* La police d'une plage : son nom et son corps à elle, sur lesquels viendront
 * se poser les traits. `fallback` est la police du champ, utilisée si le nom
 * demandé n'existe pas sur cette machine — HyperCard faisait de même plutôt
 * que de rendre du vide. */
static NSFont *run_base_font(const char *name, int size, NSFont *fallback)
{
    CGFloat sz = size > 0 ? (CGFloat)size
                          : (fallback ? [fallback pointSize] : 12);
    if (name && *name) {
        NSFont *f = font_by_loose_name([NSString stringWithUTF8String:name], sz);
        if (f) return f;
        if (getenv("HC_RUNS_DEBUG"))
            NSLog(@"[runs]   police introuvable : \"%s\" -> repli sur le champ",
                  name);
    }
    if (!fallback) return [NSFont systemFontOfSize:sz];
    if (sz == [fallback pointSize]) return fallback;
    return [NSFont fontWithDescriptor:[fallback fontDescriptor] size:sz];
}

/* Traduit un masque de style HyperCard en attributs Cocoa. C'est LE seul
 * endroit où cette traduction est écrite : le champ y passe par plage, le
 * bouton par son nom entier. Les avoir en double, c'était garantir qu'un
 * effet ajouté d'un côté manquerait de l'autre — ce qui était le cas du
 * creux et de l'ombré, absents des boutons. */
static NSMutableDictionary *style_attrs(int style, NSFont *base, NSColor *color)
{
    NSMutableDictionary *at = [NSMutableDictionary dictionary];
    if (!color) color = [NSColor blackColor];
    at[NSForegroundColorAttributeName] = color;

    BOOL faux = NO;
    NSFont *f = font_with_traits(base,
                                 (style & HC_BOLD)   ? YES : NO,
                                 (style & HC_ITALIC) ? YES : NO, &faux);
    if (f) at[NSFontAttributeName] = f;

    /* Un seul NSStrokeWidth pour deux effets, il faut donc trancher une fois :
     *   creux            -> trait positif (contour seul)
     *   creux ET gras    -> trait positif plus épais, comme le faisait le Mac
     *   gras simulé seul -> trait négatif (remplir ET contourner)
     * Le gras simulé est en plus marqué par son propre attribut : le signe du
     * trait ne suffit plus à le retrouver quand le creux l'a emporté. */
    if (faux) at[kHCFauxBoldAttribute] = @(1);

    double stroke = 0.0;
    if (style & HC_OUTLINE)  stroke = (faux || (style & HC_BOLD)) ? 5.0 : 3.0;
    else if (faux)           stroke = -3.0;
    if (stroke != 0.0) {
        at[NSStrokeWidthAttributeName] = @(stroke);
        at[NSStrokeColorAttributeName] = color;
    }

    if (style & HC_UNDERLINE)
        at[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);

    if (style & HC_SHADOW) {
        NSShadow *sh = [[NSShadow alloc] init];
        [sh setShadowOffset:NSMakeSize(1, -1)];
        [sh setShadowBlurRadius:0];
        [sh setShadowColor:[NSColor grayColor]];
        at[NSShadowAttributeName] = sh;
    }

    /* Condense et extend se rendent par l'approche : c'est ce que faisait le
     * Macintosh, qui rapprochait ou écartait les glyphes sans changer de fonte. */
    if (style & HC_CONDENSE) at[NSKernAttributeName] = @(-1.0);
    if (style & HC_EXTEND)   at[NSKernAttributeName] = @(1.5);

    /* HC_GROUP ne se voit pas, mais il doit survivre a un aller-retour par
     * l'editeur : on le porte comme attribut personnalise. */
    if (style & HC_GROUP) at[kHCGroupAttribute] = @(1);

    return at;
}

static void apply_run_style(NSMutableAttributedString *as, NSRange r,
                            int style, NSFont *base, NSColor *color)
{
    [as addAttributes:style_attrs(style, base, color) range:r];
}

static NSAttributedString *field_attr_string(Object *o, NSString *s,
                                             NSDictionary *at)
{
    NSMutableAttributedString *as =
        [[NSMutableAttributedString alloc] initWithString:s attributes:at];

    int n = hc_run_count(o);
    if (getenv("HC_RUNS_DEBUG"))
        NSLog(@"[runs] champ %s : %d plage(s)", o->name ? o->name : "?", n);
    if (n <= 0) return as;

    const char *tx  = hc_field_text(o);
    NSFont  *base   = at[NSFontAttributeName];
    NSColor *color  = at[NSForegroundColorAttributeName];
    NSUInteger len  = [s length];

    for (int i = 0; i < n; i++) {
        int a = 0, l = 0, st = 0, sz = 0;
        const char *fn = NULL;
        if (!hc_run_attrs(o, i, &a, &l, &st, &sz, &fn) || l <= 0) continue;
        NSUInteger u0 = utf16_from_byte(tx, a);
        NSUInteger u1 = utf16_from_byte(tx, a + l);
        if (u0 >= len) continue;
        if (u1 > len)  u1 = len;
        if (u1 <= u0)  continue;
        if (getenv("HC_RUNS_DEBUG"))
            NSLog(@"[runs]   [%d..%d[ style %d corps %d police %s"
                   " -> UTF16 [%lu..%lu[",
                  a, a + l, st, sz, fn ? fn : "(champ)",
                  (unsigned long)u0, (unsigned long)u1);
        /* La plage porte sa propre police : les traits se posent dessus, pas
         * sur celle du champ. Sans cela « Geneva » restait invisible tant que
         * le champ était en Helvetica. */
        apply_run_style(as, NSMakeRange(u0, u1 - u0), st,
                        run_base_font(fn, sz, base), color);
    }
    return as;
}

/* ---- Traduction retour : attributs Cocoa -> bits du noyau ----
 *
 * Pendant la saisie, c'est la NSTextStorage qui détient le style : elle sait
 * déjà tout faire, y compris Cmd-B. À la fermeture on relit ses attributs et
 * on reconstruit les plages du noyau. Le Toolbox faisait exactement cela avec
 * son TEStyleRec — une table de plages pointant vers des styles ; Cocoa en est
 * la descendante directe, les deux modèles ne sont pas rivaux.
 *
 * `group` n'a aucun équivalent Cocoa : c'est une sémantique HyperCard, pas un
 * rendu. On le porte comme attribut personnalisé, sinon il disparaîtrait au
 * premier aller-retour. */
static int style_bits_from_attrs(NSDictionary *a)
{
    int st = 0;

    NSFont *f = a[NSFontAttributeName];
    if (f) {
        NSFontDescriptorSymbolicTraits t = [[f fontDescriptor] symbolicTraits];
        if (t & NSFontDescriptorTraitBold)   st |= HC_BOLD;
        if (t & NSFontDescriptorTraitItalic) st |= HC_ITALIC;
    }

    NSNumber *u = a[NSUnderlineStyleAttributeName];
    if (u && [u intValue] != 0) st |= HC_UNDERLINE;

    /* Le trait NÉGATIF est notre gras synthétique, pas un contour : seul un
     * trait positif signifie « lettres creuses ». Confondre les deux
     * transformerait chaque gras en outline à la première sauvegarde.
     * Quand les deux styles coexistent, le trait est positif et ne dit plus
     * rien du gras : c'est l'attribut dédié qui le rapporte. */
    NSNumber *sw = a[NSStrokeWidthAttributeName];
    if (sw) {
        double v = [sw doubleValue];
        if (v > 0)      st |= HC_OUTLINE;
        else if (v < 0) st |= HC_BOLD;
    }
    if (a[kHCFauxBoldAttribute]) st |= HC_BOLD;

    if (a[NSShadowAttributeName]) st |= HC_SHADOW;

    NSNumber *k = a[NSKernAttributeName];
    if (k) {
        double v = [k doubleValue];
        if (v < 0)      st |= HC_CONDENSE;
        else if (v > 0) st |= HC_EXTEND;
    }

    if (a[kHCGroupAttribute]) st |= HC_GROUP;
    return st;
}

/* Décalage en OCTETS correspondant à un index UTF-16 : l'inverse de
 * utf16_from_byte. Le noyau raisonne en octets, NSString en unités UTF-16. */
static int byte_from_utf16(NSString *s, NSUInteger u16)
{
    if (u16 == 0) return 0;
    if (u16 > [s length]) u16 = [s length];
    NSString *pre = [s substringToIndex:u16];
    return (int)[pre lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
}

/* hauteur totale du texte d'un champ, dans sa largeur utile */
static CGFloat field_text_height(Object *o, NSRect tr) {
    /* hc_field_text et non o->contents : un champ de fond non partagé a un
     * texte par carte, et c'est celui-là qu'on affiche. */
    const char *tx = hc_field_text(o);
    NSString *s = [NSString stringWithUTF8String:tx ? tx : ""];
    if ([s length] == 0) return 0;
    NSDictionary *at = obj_attrs(o, 12, [NSColor blackColor]);
    /* Mesurer sur le texte attribué : du gras occupe plus de place, et un
     * champ défilant se tromperait de hauteur. */
    NSAttributedString *as = field_attr_string(o, s, at);
    NSRect b = [as boundingRectWithSize:NSMakeSize(tr.size.width, CGFLOAT_MAX)
                                options:NSStringDrawingUsesLineFragmentOrigin];
    return b.size.height;
}

/* ---------------------------------------------------------------------------
 * Géométrie du texte d'un champ : UNE SEULE source de vérité, partagée par le
 * dessin, l'éditeur et le suivi de la barre de défilement.
 *
 * Auparavant chaque endroit refaisait son propre calcul — le dessin retirait
 * la barre puis les marges, l'éditeur se contentait d'un NSInsetRect(…, 2, 2)
 * — et les deux divergeaient de quelques pixels. Le texte sautait donc en
 * passant du mode édition au mode navigation, et la plage de défilement
 * n'était pas la même de part et d'autre.
 * ------------------------------------------------------------------------- */
static NSRect field_text_rect(Object *o) {
    NSRect r = NSMakeRect(o->x, o->y, o->w, o->h);
    const char *st = o->style ? o->style : "rectangle";
    if (strcmp(st, "scrolling") == 0) {
        r.size.width -= 16;                    /* place prise par la barre */
    } else if (strcmp(st, "shadow") == 0) {
        r.size.width  -= 3;                    /* l'ombre portée */
        r.size.height -= 3;
    }
    CGFloat m = o->wide_margins ? 8 : 4;
    return NSInsetRect(r, m, m);
}

/* Défilement maximal : ce qui dépasse de la partie visible, jamais négatif. */
static CGFloat field_max_scroll(Object *o) {
    NSRect tr = field_text_rect(o);
    CGFloat maxs = field_text_height(o, tr) - tr.size.height;
    return maxs > 0 ? maxs : 0;
}

/* Ramener o->scroll dans ses bornes. Il n'était borné qu'en bas : les flèches
 * et le clic-page pouvaient le pousser au-delà de la fin du texte, et le champ
 * finissait sur du blanc pendant que la poignée, elle, restait bloquée en bas. */
static void field_clamp_scroll(Object *o) {
    CGFloat maxs = field_max_scroll(o);
    if (o->scroll > maxs) o->scroll = (int)lround(maxs);
    if (o->scroll < 0)    o->scroll = 0;
}

static void draw_part(Object *o) {
    if (!o->visible) return;

    NSRect r = NSMakeRect(o->x, o->y, o->w, o->h);

    /* ==================== BOUTON ==================== */
    if (o->type == OBJ_BUTTON) {
        const char *st = o->style ? o->style : "rectangle";
        BOOL isCheck  = (strcmp(st, "checkBox") == 0    || strcmp(st, "checkbox") == 0);
        BOOL isRadio  = (strcmp(st, "radioButton") == 0 || strcmp(st, "radiobutton") == 0);
        BOOL isTransp = (strcmp(st, "transparent") == 0);
        BOOL isPopup  = (strcmp(st, "popup") == 0);

        const char *nm = o->name ? o->name : "";
        NSString *s = [NSString stringWithUTF8String:nm];
        BOOL on = o->hilite;

        const HCIcon *ic = (o->icon ? hcicon_find(o->icon) : NULL);

        /* ---- bouton a icone : habillage du style + icone par-dessus ---- */
        if (ic) {
            CGFloat iy = o->y + 2;
            NSRect ir = NSMakeRect(floor(o->x + (o->w - 32)/2.0), floor(iy), 32, 32);

            draw_btn_frame(o, r, on);
            [(on ? [NSColor whiteColor] : [NSColor blackColor]) setFill];
            hcicon_draw(ic, ir, 1.0);
            draw_edit_outline(r);

            if (o->showname && o->h > 36) {
                NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                [ps setAlignment:NSTextAlignmentCenter];
                NSMutableDictionary *bat = [obj_attrs(o, 11,
                    on ? [NSColor whiteColor] : [NSColor blackColor]) mutableCopy];
                bat[NSParagraphStyleAttributeName] = ps;
                NSRect btr = NSMakeRect(o->x, iy + 34, o->w, o->h - 36);
                [s drawInRect:btr withAttributes:bat];
            }
        }
        /* ---- case a cocher / bouton radio ---- */
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
                withAttributes:obj_attrs(o, 13, nil)];
            }
        }
        /* ---- transparent ---- */
        else if (isTransp) {
            draw_btn_frame(o, r, on);
            draw_edit_outline(r);
            if (o->showname) {
                CGFloat fs = o->textsize > 0 ? o->textsize : 16;
                NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
                [ps setAlignment:NSTextAlignmentCenter];
                NSMutableDictionary *bat = [obj_attrs(o, 16, [NSColor blackColor]) mutableCopy];
                bat[NSParagraphStyleAttributeName] = ps;
                NSRect btr = NSInsetRect(r, 2, 0);
                btr.origin.y += (r.size.height - fs * 1.2) / 2;
                [s drawInRect:btr withAttributes:bat];
            }
        }
        /* ---- popup : rectangle ombre + chevron + ligne courante ---- */
        else if (isPopup) {
            NSRect body = NSMakeRect(r.origin.x, r.origin.y,
                                     r.size.width - 3, r.size.height - 3);
            NSRect sh   = NSMakeRect(r.origin.x + 3, r.origin.y + 3,
                                     r.size.width - 3, r.size.height - 3);
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

            NSString *label = s;
            if (o->contents && *o->contents) {
                NSArray *lines = [[NSString stringWithUTF8String:o->contents]
                                  componentsSeparatedByString:@"\n"];
                int sel = o->selectedline > 0 ? o->selectedline : 1;
                if (sel <= (int)[lines count] && [lines[sel-1] length] > 0)
                    label = lines[sel-1];
            }
            CGFloat fs = o->textsize > 0 ? o->textsize : 12;
            [label drawAtPoint:NSMakePoint(body.origin.x + 6,
                                           body.origin.y + (body.size.height - fs*1.3)/2)
                withAttributes:obj_attrs(o, 12, nil)];
        }
        /* ---- tous les autres styles ---- */
        else {
            draw_btn_frame(o, r, on);
            NSRect lr = r;
            if (strcmp(st, "shadow") == 0)
                lr = NSMakeRect(r.origin.x, r.origin.y,
                                r.size.width - 3, r.size.height - 3);
            draw_btn_label(o, s, lr, on, 13);
        }
    }
    /* ==================== CHAMP ==================== */
    else if (o->type == OBJ_FIELD) {
        const char *st = o->style ? o->style : "rectangle";
        BOOL isTransp = (strcmp(st, "transparent") == 0);
        BOOL isOpaque = (strcmp(st, "opaque") == 0);
        BOOL isShadow = (strcmp(st, "shadow") == 0);
        BOOL isScroll = (strcmp(st, "scrolling") == 0);

        NSRect body = r;

        /* ---- fond et cadre selon le style ---- */
        if (isTransp) {
            /* rien : le fond transparait */
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
            /* Borner avant de dessiner : au retour du mode édition, scroll peut
             * porter une valeur venue d'une géométrie qui n'est plus la nôtre. */
            field_clamp_scroll(o);

            [[NSColor whiteColor] setFill];
            NSRectFill(r);
            [[NSColor blackColor] setStroke];
            NSFrameRect(r);

            CGFloat bw = 16;
            NSRect bar = NSMakeRect(r.origin.x + r.size.width - bw, r.origin.y,
                                    bw, r.size.height);
            [[NSColor colorWithWhite:0.9 alpha:1.0] setFill];
            NSRectFill(bar);
            [[NSColor blackColor] setStroke];
            NSFrameRect(bar);

            /* les deux fleches */
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

            /* l'ascenseur, proportionnel a la part visible du texte */
            NSRect tr0 = field_text_rect(o);
            CGFloat th = field_text_height(o, tr0);
            CGFloat vh = tr0.size.height;
            CGFloat gy = bar.origin.y + 16;
            CGFloat gh = bar.size.height - 32;
            if (th > vh && gh > 8) {
                CGFloat kh = gh * (vh / th);
                if (kh < 12) kh = 12;
                /* Sur un champ court, gh tombe entre 8 et 12 : le garde-fou
                 * ci-dessus laissait poser une poignée de 12 dans une piste de
                 * 9, qui débordait alors sur les flèches. */
                if (kh > gh) kh = gh;
                CGFloat maxs = th - vh;
                CGFloat pos = (maxs > 0) ? (o->scroll / maxs) : 0;
                if (pos < 0) pos = 0;
                if (pos > 1) pos = 1;
                NSRect knob = NSMakeRect(bar.origin.x + 1, gy + pos * (gh - kh),
                                         bw - 2, kh);
                [[NSColor colorWithWhite:0.75 alpha:1.0] setFill];
                NSRectFill(knob);
                [[NSColor blackColor] setStroke];
                NSFrameRect(knob);
            }

            body = NSMakeRect(r.origin.x, r.origin.y, r.size.width - bw, r.size.height);
        }
        else {   /* rectangle */
            [[NSColor whiteColor] setFill];
            NSRectFill(r);
            [[NSColor blackColor] setStroke];
            NSFrameRect(r);
        }

        NSDictionary *at = obj_attrs(o, 12, [NSColor blackColor]);
        NSRect tr = field_text_rect(o);

        /* ---- lignes de guidage ---- */
        if (o->show_lines) {
            CGFloat lh = [@"Ag" sizeWithAttributes:at].height;
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

        /* ---- le texte ---- */
        const char *tx = hc_field_text(o);
        NSString *s = [NSString stringWithUTF8String:tx];

        /* Texte attribué plutôt que chaîne nue : c'est lui qui porte les
         * plages de style posées par « set the textStyle of word 3 of … ». */
        NSAttributedString *as = field_attr_string(o, s, at);

        /* Sauté quand l'éditeur est ouvert sur ce champ : la NSTextView est
         * transparente, pour laisser voir le cadre et le fond dessinés ici,
         * mais du coup l'ancien texte se voyait AUSSI, en surimpression sous
         * celui qu'on est en train de taper. Le cadre reste dessiné, le
         * contenu revient à l'éditeur. */
        if (o != gEditingField) {
            if (isScroll) {
                [NSGraphicsContext saveGraphicsState];
                NSRectClip(tr);
                NSRect off = tr;
                off.origin.y   -= o->scroll;
                /* La hauteur réelle du texte, mesurée dans la même largeur.
                 * La marge forfaitaire de 4000 px qui tenait lieu de hauteur
                 * ne servait qu'à masquer le fait qu'on ne la connaissait
                 * pas ; elle est désormais celle qui a servi à dimensionner
                 * la poignée, donc les deux ne peuvent plus diverger. */
                CGFloat full = field_text_height(o, tr);
                if (full < tr.size.height) full = tr.size.height;
                off.size.height = full;
                [as drawInRect:off];
                [NSGraphicsContext restoreGraphicsState];
            } else {
                [as drawInRect:tr];
            }
        }

        /* ---- surlignage du dernier « find » ----
         * Sauté lui aussi pendant l'édition : il se poserait sur un texte que
         * l'on ne dessine plus, donc en surimpression du contenu de
         * l'éditeur, et à une position calculée sans tenir compte des
         * modifications en cours de frappe. */
        int fstart = 0, flen = 0;
        if (o != gEditingField &&
            hc_found_range(o, &fstart, &flen) && flen > 0 &&
            fstart + flen <= (int)[s length]) {

            /* Mesurer sur le texte ATTRIBUÉ : un mot en gras est plus large,
             * et le rectangle de surlignage tomberait à côté. */
            NSTextStorage   *ts = [[NSTextStorage alloc] initWithAttributedString:as];
            NSLayoutManager *lm = [[NSLayoutManager alloc] init];
            NSTextContainer *tc = [[NSTextContainer alloc]
                                    initWithContainerSize:NSMakeSize(tr.size.width, 1e6)];
            [tc setLineFragmentPadding:0];
            [lm addTextContainer:tc];
            [ts addLayoutManager:lm];

            NSRange glyphs = [lm glyphRangeForCharacterRange:NSMakeRange(fstart, flen)
                                       actualCharacterRange:NULL];
            NSRect box = [lm boundingRectForGlyphRange:glyphs inTextContainer:tc];
            box.origin.x += tr.origin.x;
            box.origin.y += tr.origin.y - (isScroll ? o->scroll : 0);

            [NSGraphicsContext saveGraphicsState];
            NSRectClip(tr);
            [[NSColor blackColor] setFill];
            NSRectFill(box);
            /* Redessiner en blanc la portion trouvée, en conservant son style :
             * on part du texte attribué et on n'échange que la couleur. */
            NSMutableAttributedString *sub =
                [[as attributedSubstringFromRange:NSMakeRange(fstart, flen)] mutableCopy];
            [sub addAttribute:NSForegroundColorAttributeName
                        value:[NSColor whiteColor]
                        range:NSMakeRange(0, [sub length])];
            [sub drawAtPoint:box.origin];
            [NSGraphicsContext restoreGraphicsState];
        }

        if (isTransp) draw_edit_outline(r);
    }
}
static int handle_at(Object *o, NSPoint p) {
    if (!o) return 0;
    CGFloat s = 8;  // tolérance de clic
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
// trouve la part (bouton/champ) contenant le point, carte puis fond
// trouve la part contenant le point.
// En navigation (browse), cherche partout (carte + fond).
// En édition, ne cherche que dans la couche active.
static Object *part_at(Object *card, NSPoint p) {
    if (!card) return NULL;

    // en mode édition, restreindre à la couche active
    if (gTool != TOOL_BROWSE) {
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) return NULL;
        for (int i = layer->nparts - 1; i >= 0; i--) {
            Object *o = layer->parts[i];
            if (o->visible &&
                p.x >= o->x && p.x <= o->x + o->w &&
                p.y >= o->y && p.y <= o->y + o->h)
                return o;
        }
        return NULL;
    }

    // en navigation : carte d'abord, puis fond
    for (int i = card->nparts - 1; i >= 0; i--) {
        Object *o = card->parts[i];
        if (o->visible &&
            p.x >= o->x && p.x <= o->x + o->w &&
            p.y >= o->y && p.y <= o->y + o->h)
            return o;
    }
    if (card->bg)
        for (int i = card->bg->nparts - 1; i >= 0; i--) {
            Object *o = card->bg->parts[i];
            if (o->visible &&
                p.x >= o->x && p.x <= o->x + o->w &&
                p.y >= o->y && p.y <= o->y + o->h)
                return o;
        }
    return NULL;
}
static char gDlgBuf[512];

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
    // HyperCard met le dernier bouton en position par defaut :
    // on les ajoute a l'envers pour que le dernier soit a droite
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
// reçoit toute sortie du noyau
static void cocoa_line(HcLineKind kind, int depth, const char *text) {
    (void)depth;
    if (kind == HC_MSG && gMsgBox) {
        [gMsgBox setStringValue:[NSString stringWithUTF8String:text]];
    } else {
        NSLog(@"%s", text);
    }
}

static void cocoa_field_changed(Object *field) {
    (void)field;
    [gView setNeedsDisplay:YES];
}

/* ---------- proprietes globales : ce que le noyau ne peut pas savoir ----------
 * On ne pompe PAS la boucle d'evenements : NSEvent expose l'etat du materiel
 * en methodes de classe, ce qui marche depuis une boucle HyperTalk bloquante
 * du genre « repeat until the mouse is up ».
 * La vue etant isFlipped, ses coordonnees sont deja celles de la carte. */
static char gGlobBuf[64];

static const char *cocoa_global_get(const char *name) {
    if (strcasecmp(name, "mouse") == 0)
        return ([NSEvent pressedMouseButtons] & 1) ? "down" : "up";

    if (strcasecmp(name, "optionKey") == 0)
        return ([NSEvent modifierFlags] & NSEventModifierFlagOption) ? "down" : "up";
    if (strcasecmp(name, "commandKey") == 0)
        return ([NSEvent modifierFlags] & NSEventModifierFlagCommand) ? "down" : "up";
    if (strcasecmp(name, "shiftKey") == 0)
        return ([NSEvent modifierFlags] & NSEventModifierFlagShift) ? "down" : "up";

    if (strcasecmp(name, "mouseLoc") == 0) {
        NSPoint s = [NSEvent mouseLocation];               // ecran
        NSRect  w = [[gView window] convertRectFromScreen:
                        NSMakeRect(s.x, s.y, 0, 0)];       // fenetre
        NSPoint v = [gView convertPoint:w.origin fromView:nil];
        snprintf(gGlobBuf, sizeof gGlobBuf, "%d,%d", (int)v.x, (int)v.y);
        return gGlobBuf;
    }

    if (strcasecmp(name, "ticks") == 0) {                  // 1/60 s
        snprintf(gGlobBuf, sizeof gGlobBuf, "%ld",
                 (long)([NSDate timeIntervalSinceReferenceDate] * 60.0));
        return gGlobBuf;
    }
    return NULL;   // nom inconnu : le noyau se rabat sur un litteral
}

/* hide/unhide sont comptes par NSCursor : sans ce drapeau, un script qui
 * fait « set cursor to none » deux fois laisserait le curseur cache. */
static BOOL gCursorHidden = NO;

static void cocoa_global_set(const char *name, const char *value) {
    if (strcasecmp(name, "cursor") == 0) {
        if (strcasecmp(value, "none") == 0) {
            if (!gCursorHidden) { [NSCursor hide]; gCursorHidden = YES; }
        } else {
            if (gCursorHidden) { [NSCursor unhide]; gCursorHidden = NO; }
            if (strcasecmp(value, "watch") == 0 || strcasecmp(value, "busy") == 0)
                [[NSCursor operationNotAllowedCursor] set];
            else if (strcasecmp(value, "ibeam") == 0)
                [[NSCursor IBeamCursor] set];
            else
                [[NSCursor arrowCursor] set];
        }
    }
}

/* Filet de securite : si un script se termine sur « set cursor to none »
 * sans jamais le remettre, l'utilisateur perd son pointeur. */
void hc_restore_cursor(void) {
    if (gCursorHidden) { [NSCursor unhide]; gCursorHidden = NO; }
}

/* Les sons en cours de lecture. INDISPENSABLE : [NSSound play] est
 * asynchrone et rend la main aussitot. Sans cette reference forte, ARC
 * desalloue le NSSound des la sortie de la fonction et la lecture est
 * coupee avant d'avoir produit le moindre son : le son est bien trouve
 * (donc pas de NSBeep de repli), mais on n'entend rien du tout. */
static NSMutableArray *gPlaying = nil;

@interface HCSoundKeeper : NSObject <NSSoundDelegate>
@end
@implementation HCSoundKeeper
- (void)sound:(NSSound *)s didFinishPlaying:(BOOL)ok {
    (void)ok;
    [gPlaying removeObject:s];      // relache une fois la lecture finie
}
@end
static HCSoundKeeper *gSoundKeeper = nil;

static void cocoa_play(const char *name) {
    NSString *n = [NSString stringWithUTF8String:name ? name : ""];

    NSSound *s = [NSSound soundNamed:n];               // sons systeme
    if (!s) {                                          // puis les ressources
        for (NSString *e in @[@"aiff", @"aif", @"wav"]) {
            NSString *p = [[NSBundle mainBundle] pathForResource:n ofType:e];
            if (p) { s = [[NSSound alloc] initWithContentsOfFile:p byReference:YES]; break; }
        }
    }
    if (!s) {
        /* Ni l'un ni l'autre : soundNamed: et pathForResource: comparent le
         * nom a la lettre pres, alors que HyperTalk ignore la casse partout.
         * La pile d'origine ecrivait d'ailleurs « boing » a un endroit et
         * « Boing » a un autre. On refait donc la recherche a la main. */
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

    /* soundNamed: rend une instance partagee : on la copie, sinon deux
     * rebonds rapproches se coupent l'un l'autre. */
    s = [s copy];
    [s setDelegate:gSoundKeeper];
    [gPlaying addObject:s];
    [s play];             // asynchrone : l'animation continue pendant ce temps
}

/* Appele a chaque tour de « repeat ». Sans lui, une boucle d'animation
 * monopolise le fil principal : rien ne s'affiche avant la fin du
 * gestionnaire, et le de apparait directement en bas de la carte. */
static void cocoa_idle(void) {
    [gView display];              // drawRect: tout de suite, dans le backing store
    [[gView window] displayIfNeeded];

    /* Et LA, le point qui fait toute la difference : la vue est layer-backed,
     * donc le contenu du calque n'est envoye au serveur de fenetres qu'a la
     * fin du cycle de run loop. Comme dieFall ne rend jamais la main, les
     * soixante images s'empileraient dans le calque et seule la derniere
     * serait visible : on verrait le de partir, puis arriver, sans la chute.
     * CATransaction flush valide la transaction immediatement. */
    [CATransaction flush];

    [NSThread sleepForTimeInterval:1.0 / 60.0]; // sinon la chute est instantanee
}
static NSFont *text_font(void) {
    if (!gTextFont) {
        gTextFont = [NSFont fontWithName:@"Helvetica" size:gTextSize];
        if (!gTextFont) gTextFont = [NSFont systemFontOfSize:gTextSize];
    }
    return gTextFont;
}

static NSDictionary *text_attrs(void) {
    NSColor *c = gTextColor;
    if (!c) c = (gInk == INK_WHITE) ? [NSColor whiteColor] : [NSColor blackColor];
    NSMutableDictionary *a = [NSMutableDictionary dictionary];
    a[NSFontAttributeName] = text_font();
    a[NSForegroundColorAttributeName] = c;
    if (gTextUnderline) a[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);
    return a;
}
// grave le texte dans le bitmap a la position donnee
static void stamp_text(NSBitmapImageRep *rep, NSString *s, NSPoint pos) {
    if (!rep || [s length] == 0) return;
    NSGraphicsContext *base = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!base) return;
    CGContextRef cg = [base CGContext];
    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithCGContext:cg flipped:YES];

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [ctx setShouldAntialias:NO];

    CGFloat H = [rep pixelsHigh];
    CGContextTranslateCTM(cg, 0, H);
    CGContextScaleCTM(cg, 1, -1);

    [s drawAtPoint:pos withAttributes:text_attrs()];

    [NSGraphicsContext restoreGraphicsState];
}


// ==================== palette d'épaisseur de trait (vue custom) ====================




// ==================== palette d'outils custom (grille + sélection encadrée) ====================
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

    /* Un trait horizontal en trois passes : la première montre le grain d'un
     * geste rapide, les suivantes l'assombrissement obtenu en repassant. */
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

@implementation HCView

// static NSPanel     *gStackPanel = nil;
//static Object      *gStackTarget = NULL;
//static NSTextField *gStackName = nil;

- (void)updateWindowTitle {
    Object *card = hc_current_card();
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;
    const char *nm = stack->name ? stack->name : "Sans titre";
    [[self window] setTitle:[NSString stringWithUTF8String:nm]];
}
- (void)newBackground:(id)sender {
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
    [self setNeedsDisplay:YES];
}




  
- (BOOL)acceptsFirstResponder { return YES; }

- (void)showPopupMenuFor:(Object *)o atPoint:(NSPoint)p {
    if (!o->contents || !*o->contents) return;
    NSArray *lines = [[NSString stringWithUTF8String:o->contents]
                      componentsSeparatedByString:@"\n"];

    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

    /* Les items d'un NSMenu prennent la police système si on ne leur donne
     * qu'un titre nu : le popup s'affichait donc dans une autre police que
     * son propre titre. On leur pose un titre attribué construit par
     * obj_attrs, comme tout le reste.
     *
     * La couleur est retirée du dictionnaire : c'est au menu de la choisir,
     * pour que l'item survolé reste lisible sur son fond de surbrillance.
     * Un noir imposé y deviendrait illisible. */
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
        [it setTag:(NSInteger)(i + 1)];        // numero de ligne, base 1
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
        hc_send(gPopupTarget, "mouseUp");     // le script reagit au choix
    }
    gPopupTarget = NULL;
    [self setNeedsDisplay:YES];
}








 

 
 

/* ---- Couper / Copier / Coller ----------------------------------------
 * Ces trois commandes ne veulent pas dire la même chose selon le contexte,
 * et c'est l'OUTIL COURANT qui tranche — pas le contenu du presse-papiers.
 * C'est le modèle d'HyperCard, et c'est déjà celui qu'applique la touche
 * Delete plus bas dans keyDown:.
 *
 * Ordre d'arbitrage, du plus prioritaire au moins :
 *   1. un champ en cours d'édition  -> TEXTE. Rien à faire ici : le
 *      NSTextView est premier répondant et intercepte avant nous.
 *   2. un objet sélectionné, hors outil main -> OBJET (noyau)
 *   3. une sélection de peinture             -> IMAGE (presse-papiers pixels)
 *   4. rien                                  -> on laisse passer
 *
 * L'objet passe AVANT l'image : avec l'outil bouton, une sélection de
 * peinture peut traîner d'un usage précédent, et copier des pixels alors
 * qu'un bouton est visiblement sélectionné surprendrait. */

/* La sélection d'objet est-elle celle qui doit primer ? */
static BOOL object_selection_active(void)
{
    return (gSelected != NULL && gTool != TOOL_BROWSE) ? YES : NO;
}

static BOOL paint_selection_active(void)
{
    return ((gTool == TOOL_SELRECT && gSelRectActive) ||
            (gTool == TOOL_LASSO   && gLassoActive)) ? YES : NO;
}

- (void)copy:(id)sender {
    if (object_selection_active()) {
        if (hc_copy_part(gSelected)) return;
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
        /* Le champ éventuellement ouvert sur cet objet doit être fermé AVANT
         * la suppression : son éditeur pointe l'objet, et écrirait dans de la
         * mémoire libérée à la fermeture. */
        if (gSelected == gEditingField) [self endFieldEdit];
        if (hc_cut_part(gSelected)) {
            gSelected = NULL;
            [self setNeedsDisplay:YES];
            return;
        }
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
    [self setNeedsDisplay:YES];
}

- (void)paste:(id)sender {
    /* Coller un OBJET, si l'outil bouton ou champ est actif et que le noyau
     * en tient un. Avec l'outil main, Coller garde son sens de peinture :
     * poser un bouton alors qu'on navigue serait surprenant. */
    if ((gTool == TOOL_BUTTON || gTool == TOOL_FIELD) && hc_clipboard_part()) {
        Object *card = hc_current_card();
        if (card) {
            /* On colle là où l'on édite : couche fond si le mode fond est
             * actif, couche carte sinon. Le noyau ajustera la nature de
             * l'objet en conséquence. */
            Object *owner = (gEditBackground && card->bg) ? card->bg : card;
            Object *p = hc_paste_part(owner);
            if (p) {
                gSelected = p;
                /* Certains scripts s'initialisent à la pose — le champ
                 * calendrier d'origine écrit son gestionnaire openCard dans
                 * le script de la pile depuis newField. Sans ce message, il
                 * serait collé mais inerte. */
                hc_send(p, p->type == OBJ_BUTTON ? "newButton" : "newField");
                [self setNeedsDisplay:YES];
                return;
            }
        }
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
        [self setNeedsDisplay:YES];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    SEL a = [item action];

    /* Coche devant les palettes visibles, comme HyperCard. On la pose ici
     * plutôt qu'au moment du basculement : l'utilisateur peut aussi fermer une
     * palette par sa case de fermeture, et le menu doit le refléter. */
    if (a == @selector(togglePalette:)) {
        [item setState:[self paletteVisibleForTag:[item tag]]
                        ? NSControlStateValueOn : NSControlStateValueOff];
        return YES;
    }

    if (a == @selector(copy:) || a == @selector(cut:))
        return object_selection_active() || paint_selection_active();
    if (a == @selector(paste:)) {
        if ((gTool == TOOL_BUTTON || gTool == TOOL_FIELD) && hc_clipboard_part())
            return YES;
        return gClipboard != nil ||
               [[NSPasteboard generalPasteboard] canReadObjectForClasses:@[[NSImage class]] options:nil];
    }
    return YES;
}
- (void)setFrameSize:(NSSize)newSize {
    [self flushPaintToKernel];   // encoder les dessins avant que le cache ne soit invalide
    [super setFrameSize:newSize];
    [self clearPaintCache];
    [self setNeedsDisplay:YES];
}
- (void)changeColor:(id)sender {
    gTextColor = [sender color];
    [self setNeedsDisplay:YES];
}
- (void)underline:(id)sender {
    gTextUnderline = !gTextUnderline;
    [self setNeedsDisplay:YES];
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

    /* Le gestionnaire de polices applique sa modification à la fonte qu'on lui
     * tend : c'est donc CELLE DE LA CIBLE qu'il faut lui donner. En partant de
     * text_font() — la fonte de l'outil peinture, toujours Helvetica normal —
     * chaque Cmd-B repartait de zéro : le gras posé juste avant n'était pas
     * dans la fonte soumise, donc pas dans la fonte rendue, donc perdu. Les
     * styles s'écrasaient au lieu de s'ajouter. */
    NSFont *nf = [sender convertFont:(tgt ? obj_font(tgt, 12) : text_font())];

    if (tgt) {
            NSFontManager *fm = [NSFontManager sharedFontManager];
            NSFontTraitMask tr = [fm traitsOfFont:nf];

            /* Une seule source de vérité pour le gras et l'italique : les bits
             * de textstyle. On range donc le NOM DE FONTE SANS SES TRAITS,
             * sinon « Helvetica-Bold » resterait gras même après avoir éteint
             * le bit, et rien ne pourrait plus le dégraisser. */
            NSFontDescriptor *pd =
                [[nf fontDescriptor] fontDescriptorWithSymbolicTraits:
                    [[nf fontDescriptor] symbolicTraits] &
                    ~(NSFontDescriptorTraitBold | NSFontDescriptorTraitItalic)];
            NSFont *plain = [NSFont fontWithDescriptor:pd size:[nf pointSize]];
            if (!plain) {                       /* repli : l'ancienne méthode */
                plain = nf;
                if (tr & NSBoldFontMask)
                    plain = [fm convertFont:plain toNotHaveTrait:NSBoldFontMask];
                if (tr & NSItalicFontMask)
                    plain = [fm convertFont:plain toNotHaveTrait:NSItalicFontMask];
            }
            if (!plain) plain = nf;

            free(tgt->textfont);
            tgt->textfont = strdup([[plain fontName] UTF8String]);
            tgt->textsize = (int)[nf pointSize];

            /* Le panneau de polices ne parle que de gras et d'italique : on ne
             * touche qu'à ces deux bits et on garde les autres (souligné,
             * creux, ombré, condensé, étendu, group), qu'il ignore et qu'il
             * effaçait jusqu'ici au passage. */
            int st = tgt->textstyle & ~(HC_BOLD | HC_ITALIC);
            if (tr & NSBoldFontMask)   st |= HC_BOLD;
            if (tr & NSItalicFontMask) st |= HC_ITALIC;
            tgt->textstyle = st;

            if (tgt == gEditingField && gFieldEditor)
                [gFieldEditor setFont:obj_font(tgt, 12)];

            /* Le panneau doit refléter ce que porte vraiment l'objet, sinon la
             * bascule suivante repart d'un état faux. */
            [fm setSelectedFont:obj_font(tgt, 12) isMultiple:NO];

            /* Garder la case « taille » du dialogue d'info en accord :
               sans ca, valider le dialogue apres avoir choisi une police
               reecrit l'ancienne taille par-dessus la nouvelle. */
            hc_sync_size_field(tgt);
        } else {
        gTextFont = nf;
        gTextSize = (int)[nf pointSize];
    }
    [[NSFontManager sharedFontManager] setSelectedFont:nf isMultiple:NO];
    [self setNeedsDisplay:YES];
}
// gras / italique / souligne passent par la
- (void)changeAttributes:(id)sender {
    [self setNeedsDisplay:YES];
}

- (NSFontPanelModeMask)validModesForFontPanel:(NSFontPanel *)fontPanel {
    return NSFontPanelModesMaskStandardModes;
}
- (void)ditherSelection:(id)sender {
    // collage flottant : tramer l'image qui flotte
    if (gFloating && gClipboard) {
        dither_region(gClipboard, 0, 0, gClipW-1, gClipH-1, NULL, 0);
        [self setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
    if (!card) return;
    Object *layer = gEditBackground ? card->bg : card;
    if (!layer) layer = card;
    NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                (int)[self bounds].size.height);
    // selection rectangulaire
    if (gSelRectActive) {
        dither_region(rep, (int)MIN(gSelStart.x,gSelEnd.x), (int)MIN(gSelStart.y,gSelEnd.y),
                           (int)MAX(gSelStart.x,gSelEnd.x), (int)MAX(gSelStart.y,gSelEnd.y),
                           NULL, 0);
        [self setNeedsDisplay:YES];
        return;
    }
    // selection lasso
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
        [self setNeedsDisplay:YES];
        return;
    }
    // rien de selectionne : toute la couche
    dither_region(rep, 0, 0, (int)[rep pixelsWide]-1, (int)[rep pixelsHigh]-1, NULL, 0);
    [self setNeedsDisplay:YES];
}
- (void)keyDown:(NSEvent *)event {
    unichar key = [[event charactersIgnoringModifiers] characterAtIndex:0];
    NSUInteger mods = [event modifierFlags];
    BOOL cmd = (mods & NSEventModifierFlagCommand) != 0;

    // saisie de texte en cours : tout va au tampon
    if (gTool == TOOL_TEXT && gTextActive && !cmd) {
        if (key == 27) {                          // Echap : annuler
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

    // Delete : lasso actif -> effacer la zone
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
        [self setNeedsDisplay:YES];
        return;
    }

    // Delete : selection rectangulaire -> effacer la zone
    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gTool == TOOL_SELRECT && gSelRectActive) {
        Object *card = hc_current_card();
        Object *layer = gEditBackground ? card->bg : card;
        if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                    (int)[self bounds].size.height);
        erase_rect(rep, gSelStart, gSelEnd);
        gSelRectActive = NO;
        [self setNeedsDisplay:YES];
        return;
    }

    // Delete : objet selectionne -> le supprimer
    if ((key == NSDeleteCharacter || key == NSDeleteFunctionKey) &&
        gSelected && gTool != TOOL_BROWSE) {
        hc_delete_part(gSelected);
        gSelected = NULL;
        [self setNeedsDisplay:YES];
        return;
    }

    [super keyDown:event];
}
- (void)inkChosen:(id)sender {
    gInk = (HCInk)[sender tag];
    NSLog(@"encre : %d", (int)gInk);
}
- (BOOL)isFlipped { return YES; }
- (void)applyStackSize {
    // 1. D'ABORD encoder les dessins actuels (avant tout redimensionnement)
    [self flushPaintToKernel];

    // 2. Trouver la pile et sa taille
    Object *card = hc_current_card();
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;
    int w = stack->w > 0 ? stack->w : 512;
    int h = stack->h > 0 ? stack->h : 342;

    // 3. Vider le cache (les bitmaps seront recréés à la nouvelle taille, en rechargeant le PNG)
    [self clearPaintCache];

    // 4. Redimensionner la fenêtre
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
    int cols = 4, rows = 3;   // 11 valeurs sur 3 rangées
    CGFloat cell = 40, gap = 3, margin = 6;
    CGFloat w = margin*2 + cols*cell + (cols-1)*gap;
    CGFloat h = margin*2 + rows*cell + (rows-1)*gap;
    gWidthPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(200, 150, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered defer:NO];
    [gWidthPanel setTitle:@"Épaisseur"];
    [gWidthPanel setFloatingPanel:YES];
    /* Les palettes d'HyperCard ne prennent JAMAIS le clavier : on clique un
     * outil et il est choisi, sans clic préalable pour activer la fenêtre.
     * Sans ces deux réglages, AppKit avale le premier clic pour activer le
     * panneau, puis la carte perd son premier répondant — d'où le second clic
     * pour choisir, et encore un autre pour éditer ensuite. */
    [gWidthPanel setBecomesKeyOnlyIfNeeded:YES];
    /* Les palettes s'effacent quand on passe à une autre application, et
     * reviennent au retour. setFloatingPanel: les place au-dessus de TOUTES
     * les fenêtres du système, y compris celles des autres programmes : sans
     * cela elles resteraient plantées par-dessus Xcode ou l'émulateur. */
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
    [gBrushPanel setTitle:@"Pinceaux"];
    [gBrushPanel setFloatingPanel:YES];
    /* Les palettes d'HyperCard ne prennent JAMAIS le clavier : on clique un
     * outil et il est choisi, sans clic préalable pour activer la fenêtre.
     * Sans ces deux réglages, AppKit avale le premier clic pour activer le
     * panneau, puis la carte perd son premier répondant — d'où le second clic
     * pour choisir, et encore un autre pour éditer ensuite. */
    [gBrushPanel setBecomesKeyOnlyIfNeeded:YES];
    /* Les palettes s'effacent quand on passe à une autre application, et
     * reviennent au retour. setFloatingPanel: les place au-dessus de TOUTES
     * les fenêtres du système, y compris celles des autres programmes : sans
     * cela elles resteraient plantées par-dessus Xcode ou l'émulateur. */
    [gBrushPanel setHidesOnDeactivate:YES];
    [gBrushPanel setReleasedWhenClosed:NO];
    BrushPalette *grid = [[BrushPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gBrushPanel setContentView:grid];
    [gBrushPanel makeKeyAndOrderFront:nil];
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
    [gPatternPanel setTitle:@"Motifs"];
    [gPatternPanel setFloatingPanel:YES];
    /* Les palettes d'HyperCard ne prennent JAMAIS le clavier : on clique un
     * outil et il est choisi, sans clic préalable pour activer la fenêtre.
     * Sans ces deux réglages, AppKit avale le premier clic pour activer le
     * panneau, puis la carte perd son premier répondant — d'où le second clic
     * pour choisir, et encore un autre pour éditer ensuite. */
    [gPatternPanel setBecomesKeyOnlyIfNeeded:YES];
    /* Les palettes s'effacent quand on passe à une autre application, et
     * reviennent au retour. setFloatingPanel: les place au-dessus de TOUTES
     * les fenêtres du système, y compris celles des autres programmes : sans
     * cela elles resteraient plantées par-dessus Xcode ou l'émulateur. */
    [gPatternPanel setHidesOnDeactivate:YES];
    PatternPalette *grid = [[PatternPalette alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [gPatternPanel setContentView:grid];
    [gPatternPanel makeKeyAndOrderFront:nil];
}
 

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor whiteColor] setFill];
    NSRectFill(dirtyRect);

    Object *card = hc_current_card();
    if (!card) return;

    NSRect b = [self bounds];

    // ===== empilement façon HyperCard, de bas en haut =====

    // 1. peinture du fond
    if (card->bg) {
        NSBitmapImageRep *bgpaint = paint_bitmap(card->bg, (int)b.size.width, (int)b.size.height);
        [bgpaint drawInRect:NSMakeRect(0, 0, [bgpaint pixelsWide], [bgpaint pixelsHigh])
                   fromRect:NSZeroRect
                  operation:NSCompositingOperationSourceOver fraction:1.0
             respectFlipped:YES hints:nil];
    }

    // 2. objets du fond
    if (card->bg)
        for (int i = 0; i < card->bg->nparts; i++)
            draw_part(card->bg->parts[i]);

    // 3. peinture de la carte (PAR-DESSUS les objets du fond)
    NSBitmapImageRep *cardpaint = paint_bitmap(card, (int)b.size.width, (int)b.size.height);
    [cardpaint drawInRect:NSMakeRect(0, 0, [cardpaint pixelsWide], [cardpaint pixelsHigh])
                 fromRect:NSZeroRect
                operation:NSCompositingOperationSourceOver fraction:1.0
           respectFlipped:YES hints:nil];

    // 4. objets de la carte (au-dessus de tout)
    for (int i = 0; i < card->nparts; i++)
        draw_part(card->parts[i]);

    // ===== surcouches d'édition (toujours au-dessus) =====

    // sélection
    if (gSelected) {
        NSRect r = NSMakeRect(gSelected->x, gSelected->y, gSelected->w, gSelected->h);
        [[NSColor redColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:NSInsetRect(r, -2, -2)];
        [path setLineWidth:2];
        [path stroke];
        [[NSColor redColor] setFill];
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

    // aperçu de création d'objet (drag rectangle)
    if (gDragging) {
        [[NSColor blueColor] setStroke];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:gDragRect];
        [path setLineWidth:1];
        CGFloat dash[] = {4, 3};
        [path setLineDash:dash count:2 phase:0];
        [path stroke];
    }

    // cadre marron : mode édition du fond
    if (gEditBackground) {
        [[NSColor colorWithRed:0.6 green:0.4 blue:0.2 alpha:1.0] setStroke];
        NSBezierPath *frame = [NSBezierPath bezierPathWithRect:NSInsetRect(b, 4, 4)];
        [frame setLineWidth:4];
        CGFloat dash[] = {10, 5};
        [frame setLineDash:dash count:2 phase:0];
        [frame stroke];
    }

    // aperçu élastique des formes (ligne / rect / ovale)
    if (gShapeDrawing) {
        [[NSColor blueColor] setStroke];
        NSBezierPath *preview = [NSBezierPath bezierPath];
        NSRect box = NSMakeRect(MIN(gShapeStart.x,gShapeEnd.x), MIN(gShapeStart.y,gShapeEnd.y),
                                fabs(gShapeEnd.x-gShapeStart.x), fabs(gShapeEnd.y-gShapeStart.y));
        if (gTool == TOOL_LINE) {
            [preview moveToPoint:gShapeStart];
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
    if ((gLassoDrawing || gLassoActive) && gLassoCount > 1) {
            [[NSColor blackColor] setStroke];
            NSBezierPath *pv = [NSBezierPath bezierPath];
            [pv moveToPoint:gLassoPts[0]];
            for (int i = 1; i < gLassoCount; i++) [pv lineToPoint:gLassoPts[i]];
            if (gLassoActive) [pv closePath];
            [pv setLineWidth:1];
            CGFloat dash[] = {4, 3};
            [pv setLineDash:dash count:2 phase:0];
            [pv stroke];
        }
    if (gSelRectDrawing || gSelRectActive) {
            NSRect sel = NSMakeRect(MIN(gSelStart.x,gSelEnd.x), MIN(gSelStart.y,gSelEnd.y),
                                    fabs(gSelEnd.x-gSelStart.x), fabs(gSelEnd.y-gSelStart.y));
            [[NSColor blackColor] setStroke];
            NSBezierPath *pv = [NSBezierPath bezierPathWithRect:sel];
            [pv setLineWidth:1];
            CGFloat dash[] = {4, 3};
            [pv setLineDash:dash count:2 phase:0];
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
            CGFloat dash[] = {4, 3};
            [fp setLineDash:dash count:2 phase:0];
            [fp stroke];
        }
    if (gTextActive && gTextBuf) {
            NSDictionary *at = text_attrs();
            [gTextBuf drawAtPoint:gTextPos withAttributes:at];

            NSArray *lines = [gTextBuf componentsSeparatedByString:@"\n"];
            NSString *last = [lines lastObject];
            NSSize lastSz = [last sizeWithAttributes:at];
            // hauteur d'une ligne telle que la mesure le moteur de rendu
            NSSize oneLine = [@"Ag" sizeWithAttributes:at];
            CGFloat cy = gTextPos.y + ([lines count] - 1) * oneLine.height;

            [[NSColor blackColor] setStroke];
            NSBezierPath *caret = [NSBezierPath bezierPath];
            [caret moveToPoint:NSMakePoint(gTextPos.x + lastSz.width, cy)];
            [caret lineToPoint:NSMakePoint(gTextPos.x + lastSz.width, cy + oneLine.height)];
            [caret setLineWidth:1];
            [caret stroke];
        }
}
- (void)toggleBackground:(id)sender {
    gEditBackground = !gEditBackground;
    gSelected = NULL;
    [self endFieldEdit];
    [self setNeedsDisplay:YES];
    NSLog(@"couche : %@", gEditBackground ? @"FOND" : @"CARTE");
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
- (void)clearPaintCache {
    [gPaintCache removeAllObjects];
}

/* Lâche tout ce que la vue retient de la pile courante, AVANT que le noyau ne
 * la libère. Sans cela, gSelected et gEditingField pointeraient dans de la
 * mémoire rendue, et le premier redessin — ou le premier clic — planterait.
 * Le défaut existait déjà à l'ouverture d'un fichier : charger une pile avec
 * un bouton sélectionné suffisait à le déclencher. */
- (void)resetForNewStack {
    if (gEditingField) [self endFieldEdit];
    gSelected      = NULL;
    gEditingField  = NULL;
    gResizeHandle  = 0;
    gDragging      = NO;
    gPenDrawing    = NO;
    [self stopSprayTimer];

    /* Sélections de peinture et objet flottant : ils décrivent une carte qui
     * n'existera plus. */
    gSelRectActive  = NO;
    gSelRectDrawing = NO;
    gLassoActive    = NO;
    gLassoCount     = 0;
    gFloating       = NO;
    gFloatDragging  = NO;

    /* Le presse-papiers d'objets survit volontairement : il est détaché de
     * toute pile, et coller d'une pile vers une autre est justement l'usage. */
}
- (void)flushPaintToKernel {
    if (!gPaintCache) return;
    for (NSValue *key in gPaintCache) {
        Object *o = [key pointerValue];
        NSBitmapImageRep *rep = [gPaintCache objectForKey:key];
        if (!rep) continue;

        NSInteger w = [rep pixelsWide], h = [rep pixelsHigh];

        /* NSBitmapImageRep garde en cache la representation compressee : apres
         * une ecriture directe dans bitmapData, elle serait perimee. On encode
         * donc une copie fraiche. */
        NSBitmapImageRep *fresh = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL
                          pixelsWide:w pixelsHigh:h
                       bitsPerSample:8 samplesPerPixel:4
                            hasAlpha:YES isPlanar:NO
                      colorSpaceName:NSCalibratedRGBColorSpace
                         bytesPerRow:0 bitsPerPixel:0];

        unsigned char *src = [rep bitmapData];
        unsigned char *dst = [fresh bitmapData];
        NSInteger sbpr = [rep bytesPerRow], dbpr = [fresh bytesPerRow];
        NSInteger spp  = [rep samplesPerPixel], dspp = [fresh samplesPerPixel];
        for (NSInteger y = 0; y < h; y++) {
            for (NSInteger x = 0; x < w; x++) {
                unsigned char *sp = src + y*sbpr + x*spp;
                unsigned char *dp = dst + y*dbpr + x*dspp;
                dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2];
                dp[3] = (spp >= 4) ? sp[3] : 255;
            }
        }

        NSData *png = [fresh representationUsingType:NSBitmapImageFileTypePNG
                                          properties:@{}];
        NSString *b64 = [png base64EncodedStringWithOptions:0];
        hc_set_paint(o, [b64 UTF8String]);
    }
}
/* a placer avec les autres globales de HCview.m */
// static Object *gPopupTarget = NULL;

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    Object *hit = part_at(hc_current_card(), p);

    /* ---------- collage flottant : deplacer ou scotcher ---------- */
    if (gFloating) {
        NSRect fr = NSMakeRect(gFloatPos.x, gFloatPos.y, gClipW, gClipH);
        if (NSPointInRect(p, fr)) {
            gFloatDragging = YES;
            gFloatGrab = NSMakePoint(p.x - gFloatPos.x, p.y - gFloatPos.y);
        } else {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                        (int)[self bounds].size.height);
            stamp_clipboard(rep, gFloatPos);
            gFloating = NO;
            [self setNeedsDisplay:YES];
        }
        return;
    }

    /* ---------- outils de trace libre ---------- */
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
            if (gTool == TOOL_PENCIL)      paint_stroke(rep, p, p, [NSColor blackColor], gLineWidth);
            else if (gTool == TOOL_BRUSH)  brush_stroke(rep, p, p);
            else if (gTool == TOOL_SPRAY) {
                spray_stamp(rep, (int)lround(p.x), (int)lround(p.y),
                            gSprayRadius, gSprayDensity);
                /* L'aérographe continue de pulvériser sur place : c'est ce qui
                 * permet de charger un point en insistant, et c'est la seule
                 * différence de nature avec le pinceau. Sans minuterie, un clic
                 * maintenu ne déposerait qu'un seul nuage. */
                [self startSprayTimer];
            }
            else                           erase_stroke(rep, p, p, 16);
            [self setNeedsDisplay:YES];
        }
        return;
    }

    /* ---------- formes elastiques ---------- */
    if (gTool == TOOL_LINE || gTool == TOOL_RECT || gTool == TOOL_OVAL) {
        gShapeStart = p;
        gShapeEnd = p;
        gShapeDrawing = YES;
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- pot de peinture ---------- */
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

    /* ---------- forme libre ---------- */
    if (gTool == TOOL_FREEFORM) {
        gFreeCount = 0;
        gFreePts[gFreeCount++] = p;
        gFreeDrawing = YES;
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- lasso ---------- */
    if (gTool == TOOL_LASSO) {
        [[self window] makeFirstResponder:self];
        gLassoCount = 0;
        gLassoPts[gLassoCount++] = p;
        gLassoDrawing = YES;
        gLassoActive = NO;
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- selection rectangulaire ---------- */
    if (gTool == TOOL_SELRECT) {
        [[self window] makeFirstResponder:self];
        gSelStart = p; gSelEnd = p;
        gSelRectDrawing = YES;
        gSelRectActive = NO;
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- outil texte ---------- */
    if (gTool == TOOL_TEXT) {
        [self commitText];                 // graver la saisie precedente
        gTextPos = p;
        gTextBuf = [NSMutableString string];
        gTextActive = YES;
        [[self window] makeFirstResponder:self];
        [[NSFontManager sharedFontManager] setSelectedFont:text_font() isMultiple:NO];
        [self setNeedsDisplay:YES];
        return;
    }

    /* ---------- double-clic en mode edition : dialogue Info ---------- */
    if (gTool != TOOL_BROWSE && hit && [event clickCount] == 2) {
        if (hit->type == OBJ_BUTTON)      [self showButtonInfo:hit];
        else if (hit->type == OBJ_FIELD)  [self showFieldInfo:hit];
        else                              [self editScriptOf:hit];
        return;
    }

    /* ---------- navigation ---------- */
    if (gTool == TOOL_BROWSE) {

        // barre de defilement d'un champ scrolling : avant toute autre chose
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
                            /* Exactement le même calcul que dans draw_part :
                             * une poignée dessinée ici et testée là serait
                             * insaisissable sur ses derniers pixels. */
                            CGFloat kh = gh * (vh / th);
                            if (kh < 12) kh = 12;
                            if (kh > gh) kh = gh;
                            CGFloat maxs = th - vh;
                            CGFloat pos = (maxs > 0) ? (hit->scroll / maxs) : 0;
                            if (pos < 0) pos = 0;
                            if (pos > 1) pos = 1;
                            CGFloat ky = gy + pos * (gh - kh);
                            if (p.y >= ky && p.y <= ky + kh) {
                                gScrollField = hit;          // debut du glissement
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
                        [self setNeedsDisplay:YES];
                        return;
                    }
                }

        // bouton popup : derouler le menu
        if (hit && hit->type == OBJ_BUTTON && hit->style &&
            strcmp(hit->style, "popup") == 0) {
            [self showPopupMenuFor:hit atPoint:p];
            return;
        }

        // champ : passer en saisie
        if (hit && hit->type == OBJ_FIELD) {
                    if (hit->locktext) {
                        // champ verrouille : le clic va au script, comme un bouton
                        gPressed = hit;
                        hc_send(hit, "mouseDown");
                        [self setNeedsDisplay:YES];
                    } else {
                        [self beginFieldEdit:hit];
                    }
                    return;
                }

        if (hit) {
            gPressed = hit;
            // flash uniquement pour les boutons sans etat persistant
            if (hit->type == OBJ_BUTTON && hit->autohilite &&
                (!hit->style ||
                 (strcmp(hit->style, "checkBox") != 0 && strcmp(hit->style, "checkbox") != 0 &&
                  strcmp(hit->style, "radioButton") != 0 && strcmp(hit->style, "radiobutton") != 0))) {
                hit->hilite = 1;
            }
            hc_send(hit, "mouseDown");
            [self setNeedsDisplay:YES];
        } else {
            [self endFieldEdit];
        }
        return;
    }

    /* ---------- mode bouton / champ ---------- */
    // saisit-on une poignee de l'objet deja selectionne ?
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

    if (hit) {
        gSelected = hit;
        gMoving = YES;
        [[self window] makeFirstResponder:self];   // focus clavier pour Delete
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
    if (gScrollField) {
            CGFloat travel = gScrollGH - gScrollKH;
            if (travel > 0) {
                CGFloat pos = (p.y - gScrollGrab - gScrollGY) / travel;
                if (pos < 0) pos = 0;
                if (pos > 1) pos = 1;
                gScrollField->scroll = (int)(pos * gScrollMax);
            }
            [self setNeedsDisplay:YES];
            return;
        }
    if (gFloating && gFloatDragging) {
            gFloatPos = NSMakePoint(p.x - gFloatGrab.x, p.y - gFloatGrab.y);
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_PENCIL && gPenDrawing) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;   // ← couche active
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
        paint_stroke(rep, gPenLast, p, [NSColor blackColor], gLineWidth);            gPenLast = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_BRUSH && gPenDrawing) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            brush_stroke(rep, gPenLast, p);
            gPenLast = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_SPRAY && gPenDrawing) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width,
                                                        (int)[self bounds].size.height);
            spray_stroke(rep, gPenLast, p, gSprayRadius, gSprayDensity);
            gPenLast = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_ERASER && gPenDrawing) {
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
            NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
            erase_stroke(rep, gPenLast, p, 16);
            gPenLast = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_FREEFORM && gFreeDrawing) {
            if (gFreeCount < 4096) gFreePts[gFreeCount++] = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gTool == TOOL_SELRECT && gSelRectDrawing) {
        gSelEnd = p;
        [self setNeedsDisplay:YES];
        return;
    }

    if (gShapeDrawing) {
            gShapeEnd = p;
            [self setNeedsDisplay:YES];
            return;
        }
    if (gResizeHandle && gSelected) {
        int dx = (int)(p.x - gMoveStart.x);
        int dy = (int)(p.y - gMoveStart.y);
        int x = gObjStartX, y = gObjStartY, w = gObjStartW, h = gObjStartH;
        switch (gResizeHandle) {
            case 1: x += dx; y += dy; w -= dx; h -= dy; break;  // HG
            case 2: y += dy; w += dx; h -= dy; break;            // HD
            case 3: x += dx; w -= dx; h += dy; break;            // BG
            case 4: w += dx; h += dy; break;                     // BD
        }
        if (w < 8) w = 8;
        if (h < 8) h = 8;
        gSelected->x = x; gSelected->y = y;
        gSelected->w = w; gSelected->h = h;
        [self setNeedsDisplay:YES];
        return;
    }
    // mouseDragged
        if (gTool == TOOL_LASSO && gLassoDrawing) {
            if (gLassoCount < 4096) gLassoPts[gLassoCount++] = p;
            [self setNeedsDisplay:YES];
            return;
        }
    // déplacement d'un objet sélectionné
    if (gMoving && gSelected) {
        int dx = (int)(p.x - gMoveStart.x);
        int dy = (int)(p.y - gMoveStart.y);
        gSelected->x = gObjStartX + dx;
        gSelected->y = gObjStartY + dy;
        [self setNeedsDisplay:YES];
        return;
    }

    // création par tracé (inchangé)
    if (!gDragging) return;
    CGFloat x = MIN(gDragStart.x, p.x);
    CGFloat y = MIN(gDragStart.y, p.y);
    CGFloat w = fabs(p.x - gDragStart.x);
    CGFloat h = fabs(p.y - gDragStart.y);
    gDragRect = NSMakeRect(x, y, w, h);
    [self setNeedsDisplay:YES];
}
- (void)mouseUp:(NSEvent *)event {
    // --- mode flèche : envoyer mouseUp au script ---
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
            /* TOOL_BRUSH manquait ici : gPenDrawing restait à YES après le
             * relâchement. Sans effet visible — mouseDragged: ne part que
             * bouton enfoncé — mais l'état mentait. */
            gPenDrawing = NO;
            [self stopSprayTimer];
            return;
        }
    if (gTool == TOOL_SELRECT) {
            gSelRectDrawing = NO;
            gSelRectActive = (fabs(gSelEnd.x-gSelStart.x) > 3 && fabs(gSelEnd.y-gSelStart.y) > 3);
            [self setNeedsDisplay:YES];
            return;
        }
    // tool lasso
        if (gTool == TOOL_LASSO) {
            gLassoDrawing = NO;
            gLassoActive = (gLassoCount >= 3);   // sélection valide si au moins un triangle
            [self setNeedsDisplay:YES];
            return;
        }
    if (gShapeDrawing) {
            gShapeDrawing = NO;
            Object *card = hc_current_card();
            Object *layer = gEditBackground ? card->bg : card;
            if (!layer) layer = card;
        NSBitmapImageRep *rep = paint_bitmap(layer, (int)[self bounds].size.width, (int)[self bounds].size.height);
                    if (gShapeFilled && gTool != TOOL_LINE)
                        fill_shape(rep, gTool, gShapeStart, gShapeEnd);
                    paint_shape(rep, gTool, gShapeStart, gShapeEnd, [NSColor blackColor], gLineWidth);;
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
                if (hit == gPressed)
                    hc_send(gPressed, "mouseUp");

                // comportement checkBox / radioButton : bascule persistante
                if (gPressed->type == OBJ_BUTTON && gPressed->style) {
                    const char *st = gPressed->style;
                    if (strcmp(st, "checkBox") == 0 || strcmp(st, "checkbox") == 0) {
                        gPressed->hilite = !gPressed->hilite;   // bascule et reste
                    }
                    else if (strcmp(st, "radioButton") == 0 || strcmp(st, "radiobutton") == 0) {
                        gPressed->hilite = 1;                    // allume
                        radio_exclusive(hc_current_card(), gPressed);  // éteint les autres
                    }
                    else if (gPressed->autohilite) {
                        gPressed->hilite = 0;   // bouton normal : éteindre le flash
                    }
                } else if (gPressed->type == OBJ_BUTTON && gPressed->autohilite) {
                    gPressed->hilite = 0;
                }

                gPressed = NULL;
                [self setNeedsDisplay:YES];
            }
            if (gMoving) {
                gMoving = NO;
                [self setNeedsDisplay:YES];
                return;
            }
            return;
        }

    // --- mode édition : finir la création par drag ---
    if (!gDragging) return;
    gDragging = NO;
    if (gDragRect.size.width < 8 || gDragRect.size.height < 8) {
        [self setNeedsDisplay:YES];
        return;
    }
    Object *card = hc_current_card();
        if (!card) return;
        Object *owner = gEditBackground ? card->bg : card;   // couche cible
        if (!owner) owner = card;                            // sécurité si pas de fond
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
/* --- Find, à la manière d'HyperCard ---
 * Pas de panneau de recherche : la commande prépare simplement « find "" »
 * dans la boîte de message et pose le point d'insertion ENTRE les guillemets.
 * L'utilisateur tape son mot et valide — c'est la commande HyperTalk find qui
 * fait le travail, la même que dans un script.
 *
 * Il faut passer par l'éditeur de champ (le NSText partagé de la fenêtre) et
 * non par le NSTextField : setStringValue: ne donne pas le focus, et un champ
 * qui ne l'a pas n'a pas de sélection où poser un point d'insertion. */
- (void)findInStack:(id)sender {
    if (!gMsgBox) return;

    NSString *amorce = @"find \"\"";
    [gMsgBox setStringValue:amorce];
    [[self window] makeFirstResponder:gMsgBox];

    NSText *ed = [[self window] fieldEditor:YES forObject:gMsgBox];
    if (ed) {
        /* Entre les deux guillemets : longueur - 1. */
        NSUInteger pos = [amorce length] - 1;
        [ed setSelectedRange:NSMakeRange(pos, 0)];
    }
}

- (void)installMessageBox {
    gView = self;
    NSRect b = [self bounds];
    gMsgBox = [[NSTextField alloc] initWithFrame:NSMakeRect(10, b.size.height - 34, b.size.width - 20, 24)];
    [gMsgBox setEditable:YES];
    [gMsgBox setSelectable:YES];
    [gMsgBox setBezeled:YES];
    [gMsgBox setDrawsBackground:YES];
    [gMsgBox setBackgroundColor:[NSColor colorWithWhite:0.96 alpha:1.0]];
    [gMsgBox setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
    [gMsgBox setStringValue:@""];
    [gMsgBox setTarget:self];
    [gMsgBox setAction:@selector(messageBoxEntered:)];
    [self addSubview:gMsgBox];

    static HcHost host;
    host.line          = cocoa_line;
    host.field_changed = cocoa_field_changed;
    host.ask           = cocoa_ask;
    host.answer        = cocoa_answer;
    host.global_get    = cocoa_global_get;
    host.global_set    = cocoa_global_set;
    host.play_sound    = cocoa_play;
    host.idle          = cocoa_idle;
    hc_set_host(&host);
}

- (void)messageBoxEntered:(id)sender {
    NSString *cmd = [gMsgBox stringValue];
    if ([cmd length] == 0) return;
    hc_do([cmd UTF8String]);
    [self applyStackSize];          // ← applique un éventuel changement de taille
    [self setNeedsDisplay:YES];
    [gMsgBox selectText:nil];
}

/* ---- minuterie de l'aérographe ----
 * Le spray doit continuer à déposer tant que le bouton est enfoncé, même
 * immobile. mouseDragged: ne se déclenchant qu'au mouvement, il faut une
 * horloge. 24 pulsations par seconde donnent une montée en densité proche de
 * l'original sans saturer d'un coup.
 *
 * La minuterie est arrêtée à trois endroits : au relâchement, au changement
 * d'outil et à la disparition de la vue. Une minuterie qui survit à son outil
 * continuerait de peindre dans le vide — ou pire, dans la carte suivante. */
static NSTimer *gSprayTimer = nil;
/* Valables uniquement pendant que le dialogue de réglage est ouvert : mis à
 * nil à sa fermeture, les vues étant détruites avec lui. */
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
    /* gPenLast, et non la position courante de la souris : si elle bouge,
     * c'est mouseDragged: qui pulvérise le long du trajet, et pulvériser
     * deux fois doublerait la densité du trait. */
    spray_stamp(rep, (int)lround(gPenLast.x), (int)lround(gPenLast.y),
                gSprayRadius, gSprayDensity);
    [self setNeedsDisplay:YES];
}

/* ---- réglage de l'aérographe ----
 * Ouvert par double-clic sur l'outil. Modal : on règle, on valide, on dessine.
 *
 * Tirettes plutôt que champs de saisie, parce que personne ne sait ce que vaut
 * « densité 45 » — on le découvre en essayant. Avec un champ, chaque essai
 * coûte quatre gestes ; avec une tirette on balaie et l'aperçu suit, si bien
 * qu'on trouve la bonne valeur sans jamais lire un chiffre. C'est le cas où le
 * geste continu bat la saisie : il n'y a pas de valeur juste, seulement une
 * valeur qui rend bien.
 *
 * Le chiffre reste affiché à côté, pour ceux qui veulent retrouver un réglage.
 * setContinuous: est indispensable — sans lui l'action n'arrive qu'au
 * relâchement, et l'aperçu ne suivrait pas le geste. */
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

    /* --- rayon --- */
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

    /* --- densité --- */
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

    /* Mémoriser pour rétablir sur Annuler : les tirettes modifient les
     * globales en direct, donc renoncer doit défaire tous ces essais. */
    int oldR = gSprayRadius, oldD = gSprayDensity;

    if ([a runModal] != NSAlertFirstButtonReturn) {
        gSprayRadius  = oldR;
        gSprayDensity = oldD;
    }

    /* Les vues meurent avec le dialogue : laisser les pointeurs en ferait des
     * cibles pendantes au prochain rafraîchissement de l'aperçu. */
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

    /* Retenue dans une globale, et refermable : sans les deux, la palette
     * n'existait que le temps de cette méthode et rien ne pouvait la rappeler.
     * setReleasedWhenClosed:NO est indispensable — par défaut un NSPanel se
     * détruit à la fermeture, et la globale pointerait dans le vide. */
    gToolPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(560, 350, w, h)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskUtilityWindow |
                             NSWindowStyleMaskClosable |
                             NSWindowStyleMaskNonactivatingPanel)
                    backing:NSBackingStoreBuffered defer:NO];
    [gToolPanel setTitle:@"Outils"];
    [gToolPanel setFloatingPanel:YES];
    /* Les palettes d'HyperCard ne prennent JAMAIS le clavier : on clique un
     * outil et il est choisi, sans clic préalable pour activer la fenêtre.
     * Sans ces deux réglages, AppKit avale le premier clic pour activer le
     * panneau, puis la carte perd son premier répondant — d'où le second clic
     * pour choisir, et encore un autre pour éditer ensuite. */
    [gToolPanel setBecomesKeyOnlyIfNeeded:YES];
    /* Les palettes s'effacent quand on passe à une autre application, et
     * reviennent au retour. setFloatingPanel: les place au-dessus de TOUTES
     * les fenêtres du système, y compris celles des autres programmes : sans
     * cela elles resteraient plantées par-dessus Xcode ou l'émulateur. */
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

/* Une seule commande par palette, qui montre ou cache selon l'état. Le menu
 * porte une coche, mise à jour par validateMenuItem: — c'est ce que faisait
 * HyperCard, et ça évite d'avoir deux entrées « Afficher » et « Masquer »
 * dont l'une est toujours inutile. */
- (void)togglePalette:(id)sender {
    NSInteger tag = [sender tag];
    NSPanel *p = nil;
    switch (tag) {
        case 1: if (!gToolPanel)    { [self installToolPalette];    return; } p = gToolPanel;    break;
        case 2: if (!gPatternPanel) { [self installPatternPalette]; return; } p = gPatternPanel; break;
        case 3: if (!gWidthPanel)   { [self installWidthPalette];   return; } p = gWidthPanel;   break;
        case 4: if (!gBrushPanel)   { [self installBrushPalette];   return; } p = gBrushPanel;   break;
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
    }
    return NO;
}

- (void)toggleFilled:(id)sender {
    gShapeFilled = !gShapeFilled;
    NSLog(@"formes : %@", gShapeFilled ? @"PLEINES" : @"VIDES");
}
- (void)toolChosen:(id)sender {
    gTool = (HCTool)[sender tag];
    gSelected = NULL;
    [self setNeedsDisplay:YES];
    NSLog(@"outil : %d", (int)gTool);
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

- (void)saveScript:(id)sender {
    if (gEditTarget && gEditView) {
        hc_set_script(gEditTarget, [[gEditView string] UTF8String]);
    }
    [gEditPanel close];
    gEditPanel = nil; gEditView = nil; gEditTarget = nil;
    [self setNeedsDisplay:YES];
}
- (void)beginFieldEdit:(Object *)field {
    [self endFieldEdit];
    gEditingField = field;

    /* MÊME rectangle que celui du dessin, au pixel près. Un NSInsetRect(…,2,2)
     * posait le texte 2 px plus haut que draw_part, et sur une largeur
     * différente : le contenu sautait à l'ouverture de l'éditeur, se
     * réenroulait autrement, et ressortait décalé. */
    NSRect r = field_text_rect(field);

    /* Une NSTextView créée par initWithFrame: est verticalement
     * redimensionnable : elle GRANDIT au-delà de son cadre dès que le texte
     * dépasse. Posée directement comme sous-vue, elle débordait donc du
     * champ, sans rien pour la rogner ni la faire défiler.
     *
     * On l'enferme dans une NSScrollView, qui apporte les deux : le clip par
     * sa vue de contenu, et le défilement. La barre n'apparaît que pour les
     * champs de style « scrolling », comme dans HyperCard — les autres se
     * contentent d'être rognés, mais restent parcourables au clavier et à la
     * molette, sans quoi un texte trop long deviendrait inatteignable. */
    BOOL isScroll = (field->style && strcmp(field->style, "scrolling") == 0);

    gFieldScroll = [[NSScrollView alloc] initWithFrame:r];
    /* Pas de barre Cocoa : draw_part dessine déjà la nôtre, avec ses deux
     * flèches de 16 px. Les deux se superposaient, et la poignée de Cocoa,
     * qui court sur toute la hauteur du cadre, débordait par-dessus les
     * flèches. Elle rognait en outre la largeur utile, d'où un enroulement
     * du texte différent de celui du dessin. */
    [gFieldScroll setHasVerticalScroller:NO];
    [gFieldScroll setHasHorizontalScroller:NO];
    [gFieldScroll setAutohidesScrollers:YES];
    [gFieldScroll setBorderType:NSNoBorder];
    [gFieldScroll setDrawsBackground:NO];

    NSSize sz = [gFieldScroll contentSize];
    gFieldEditor = [[NSTextView alloc]
        initWithFrame:NSMakeRect(0, 0, sz.width, sz.height)];
    [gFieldEditor setMinSize:NSMakeSize(0, 0)];
    [gFieldEditor setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
    [gFieldEditor setVerticallyResizable:YES];
    [gFieldEditor setHorizontallyResizable:NO];
    [gFieldEditor setAutoresizingMask:NSViewWidthSizable];
    [[gFieldEditor textContainer] setContainerSize:NSMakeSize(sz.width, FLT_MAX)];
    [[gFieldEditor textContainer] setWidthTracksTextView:YES];
    /* NSTextView réserve 5 px de chaque côté par défaut, là où -drawInRect:
     * n'en réserve aucun : sans cela le texte se décalait à droite en édition
     * et se réenroulait 10 px trop tôt. */
    [[gFieldEditor textContainer] setLineFragmentPadding:0];
    [gFieldEditor setTextContainerInset:NSZeroSize];
    [gFieldEditor setDrawsBackground:NO];

    [gFieldEditor setFont:obj_font(field, 12)];

    /* Texte riche, sans quoi la NSTextStorage aplatirait tout a la premiere
     * frappe et Cmd-B resterait sans effet. */
    [gFieldEditor setRichText:YES];
    [gFieldEditor setImportsGraphics:NO];
    [gFieldEditor setAllowsUndo:YES];

    const char *tx = hc_field_text(field);
    NSString *str = [NSString stringWithUTF8String:tx ? tx : ""];
    NSDictionary *base = obj_attrs(field, 12, [NSColor blackColor]);

    [gFieldEditor setEditable:!field->locktext];
    [gFieldEditor setSelectable:YES];

    /* On confie a l'editeur le texte AVEC ses plages : pendant la saisie,
     * c'est lui qui detient la verite du style. */
    [[gFieldEditor textStorage]
        setAttributedString:field_attr_string(field, str, base)];
    [gFieldEditor setTypingAttributes:base];

    [gFieldScroll setDocumentView:gFieldEditor];
    [self addSubview:gFieldScroll];

    /* Reprendre le décalage que le dessin utilisait : sinon un champ
     * scrolling sautait en haut à l'ouverture de l'éditeur, puis y restait
     * après fermeture. La vue est retournée, donc l'origine du document
     * correspond directement à field->scroll. */
    if (isScroll) {
        field_clamp_scroll(field);
        if (field->scroll > 0) {
            /* Après que la mise en page a eu lieu, sinon la vue de contenu ne
             * connaît pas encore sa hauteur de document et refuse d'aller
             * au-delà de zéro. */
            [[gFieldEditor layoutManager]
                ensureLayoutForTextContainer:[gFieldEditor textContainer]];
            [[gFieldScroll contentView]
                scrollToPoint:NSMakePoint(0, field->scroll)];
            [gFieldScroll reflectScrolledClipView:[gFieldScroll contentView]];
        }

        /* Pendant la saisie, c'est la NSScrollView qui défile : sans écoute,
         * la poignée que nous dessinons resterait figée sur la valeur d'avant
         * l'ouverture, puis sauterait d'un coup à la fermeture. */
        [[gFieldScroll contentView] setPostsBoundsChangedNotifications:YES];
        [[NSNotificationCenter defaultCenter]
            addObserver:self
               selector:@selector(fieldEditorDidScroll:)
                   name:NSViewBoundsDidChangeNotification
                 object:[gFieldScroll contentView]];
    }

    [[self window] makeFirstResponder:gFieldEditor];
}

/* Recopier le défilement de l'éditeur dans le modèle, à chaque mouvement. */
- (void)fieldEditorDidScroll:(NSNotification *)note {
    if (!gEditingField || !gFieldScroll) return;
    gEditingField->scroll =
        (int)lround([[gFieldScroll contentView] bounds].origin.y);
    [self setNeedsDisplay:YES];
}

- (void)endFieldEdit {
    if (gFieldEditor && gEditingField) {
        NSString *str = [gFieldEditor string];

        /* Le texte d'abord : hc_set_field_text detruit les plages, puisqu'il
         * voit un remplacement complet. On les reconstruit ensuite depuis la
         * NSTextStorage, qui a suivi la saisie. */
        hc_set_field_text(gEditingField, [str UTF8String]);
        hc_runs_clear(gEditingField);

        NSTextStorage *ts = [gFieldEditor textStorage];

        /* Repère de comparaison : la police que le champ RENDRAIT sans aucune
         * plage. hc_run_add_full compare aux valeurs brutes du champ —
         * textfont à NULL, textsize à 0 — alors que l'éditeur travaille avec
         * la police résolue (système, 12 points). Rien ne coïncidait, donc
         * aucune plage n'était neutralisée : après une seule session
         * d'édition, chaque caractère portait une police, un corps et un
         * style explicites, et le dialogue du champ n'avait plus prise sur
         * rien. On neutralise donc ici, contre l'effectif. */
        NSFont *fbase = obj_base_font(gEditingField, 12);
        NSString *fbaseName = [fbase fontName];
        int fbaseSize = (int)lround([fbase pointSize]);
        int fbaseStyle = gEditingField->textstyle;

        NSUInteger len = [ts length], i = 0;
        while (i < len) {
            NSRange eff;
            NSDictionary *a = [ts attributesAtIndex:i
                              longestEffectiveRange:&eff
                                            inRange:NSMakeRange(i, len - i)];
            int st = style_bits_from_attrs(a);

            /* Le nom de police est rangé SANS ses traits : le gras vit dans
             * les bits de style, et le laisser aussi dans le nom de la fonte
             * donnerait deux sources de vérité qui finiraient par diverger. */
            NSFont *rf = a[NSFontAttributeName];
            const char *fname = NULL;
            int fsize = 0;
            if (rf) {
                NSFontDescriptor *fd = [[rf fontDescriptor]
                    fontDescriptorWithSymbolicTraits:
                        [[rf fontDescriptor] symbolicTraits]
                        & ~(NSFontDescriptorTraitBold | NSFontDescriptorTraitItalic)];
                NSFont *plain = [NSFont fontWithDescriptor:fd size:[rf pointSize]];
                NSString *nm = [(plain ? plain : rf) fontName];
                int sz = (int)lround([rf pointSize]);

                /* Identique à ce que le champ rendrait tout seul : on ne dit
                 * rien, pour que la plage reste sensible aux changements
                 * ultérieurs du champ. */
                if (![nm isEqualToString:fbaseName]) fname = [nm UTF8String];
                if (sz != fbaseSize)                 fsize = sz;
            }

            /* Même raisonnement pour le style : une plage qui ne fait que
             * répéter celui du champ n'a pas à le figer. Sans quoi mettre le
             * champ en gras laissait tout le texte en clair. */
            if (st == fbaseStyle && !fname && fsize == 0) {
                i = NSMaxRange(eff);
                if (eff.length == 0) break;
                continue;
            }

            int b0 = byte_from_utf16(str, eff.location);
            int b1 = byte_from_utf16(str, eff.location + eff.length);
            hc_run_add_full(gEditingField, b0, b1 - b0, st, fsize, fname);
            i = NSMaxRange(eff);
            if (eff.length == 0) break;      /* garde-fou : jamais de boucle */
        }
    }
    /* Reporter le décalage dans le modèle, pour que le dessin reprenne là où
     * l'éditeur s'est arrêté. Sans cela le texte sautait en haut dès qu'on
     * cliquait ailleurs. */
    if (gEditingField && gFieldScroll &&
        gEditingField->style && strcmp(gEditingField->style, "scrolling") == 0) {
        gEditingField->scroll =
            (int)lround([[gFieldScroll contentView] bounds].origin.y);
        /* Le texte vient peut-être de raccourcir sous la frappe : ce que
         * l'éditeur pouvait atteindre n'est plus forcément atteignable. */
        field_clamp_scroll(gEditingField);
    }

    if (gFieldScroll)
        [[NSNotificationCenter defaultCenter]
            removeObserver:self
                      name:NSViewBoundsDidChangeNotification
                    object:[gFieldScroll contentView]];

    [gFieldScroll removeFromSuperview];
    gFieldScroll = nil;
    gFieldEditor = nil;
    gEditingField = NULL;
    [self setNeedsDisplay:YES];
}
 
@end
