#import "HCvisual.h"
#import "HCview.h"
#import "HCglobals.h"
#import <QuartzCore/QuartzCore.h>   /* CATransaction, pour pousser les pixels a l ecran */
#include <stdlib.h>
#include <strings.h>                /* strcasecmp, pour les noms d'effets */

/* ═══ Effets de transition ═══════════════════════════════════════
 *
 * Extrait de HCview.m sans changement de comportement. Les huit variables
 * d'etat restent privees a ce fichier : seules deux choses en sortent, l'appel
 * que le noyau nous adresse (cocoa_visual_effect) et le predicat que drawRect:
 * consulte (visual_pending).
 *
 * Les deux methodes de HCView qui jouent l'animation viennent avec, sous forme
 * de categorie : les separer de leur etat aurait oblige a exporter les huit
 * variables, ce qui aurait rendu le decoupage plus couteux que le desordre. */


/* Transition en attente : image de départ, nom de l'effet, et nombre
 * d'images restant à jouer. Armés par cocoa_visual_effect, consommés au
 * premier redessin qui suit. */
static NSBitmapImageRep *gVisualBefore = nil;
static char     gVisualName[64]  = "";
static char     gVisualImage[16] = "";
static int      gVisualSteps = 0;
static NSBitmapImageRep *gVisualAfter = nil;
static CGFloat  gVisualStep  = 0;
static BOOL     gVisualRunning = NO;   /* une animation est en cours */
static BOOL     gVisualCapturing = NO; /* on photographie : ne rien animer */

/* ---------- effets de transition ----------
 * Le noyau nous prévient juste AVANT de changer de carte. On photographie
 * l'écran de départ, on laisse la carte changer, puis on anime le passage.
 *
 * Le principe : dessiner l'image d'arrivée par-dessus celle de départ, en la
 * dévoilant progressivement. Chaque effet ne diffère que par la FORME du
 * dévoilement — un rectangle qui grandit pour « iris », une bande qui avance
 * pour « wipe », des cases éparses pour « dissolve ». Une seule boucle
 * d'animation, donc, et une fonction qui donne la zone à découvrir.
 *
 * Cocoa ne rend pas la main pendant qu'un script tourne : l'animation doit
 * donc pousser ses images elle-même, comme le fait cocoa_idle. */

/* Photographie de la vue, en NSBitmapImageRep et non en NSImage.
 *
 * La vue est isFlipped ; NSImage ne l'est pas, et -drawInRect: y dessinait
 * l'image la tête en bas. NSBitmapImageRep sait respecter l'orientation de sa
 * destination, à condition de le lui demander par respectFlipped: — c'est
 * exactement ce que fait déjà le dessin de la peinture des cartes. */
static NSBitmapImageRep *snapshot_view(void) {
    if (!gView) return nil;
    NSRect b = [gView bounds];
    NSBitmapImageRep *rep = [gView bitmapImageRepForCachingDisplayInRect:b];
    if (!rep) return nil;
    [gView cacheDisplayInRect:b toBitmapImageRep:rep];
    return rep;
}

/* Dessine une photographie en respectant le retournement de la vue. */
static void draw_snapshot(NSBitmapImageRep *rep, NSRect b,
                          NSCompositingOperation op, CGFloat frac) {
    if (!rep) return;
    [rep drawInRect:b fromRect:NSZeroRect operation:op fraction:frac
     respectFlipped:YES hints:nil];
}

/* Nombre d'images de l'animation, selon la vitesse demandée. */
static int visual_frames(const char *speed) {
    if (!speed || !*speed)                    return 14;
    if (strcasecmp(speed, "very fast") == 0)  return 4;
    if (strcasecmp(speed, "fast") == 0)       return 8;
    if (strcasecmp(speed, "slow") == 0 ||
        strcasecmp(speed, "slowly") == 0)     return 24;
    if (strcasecmp(speed, "very slow") == 0 ||
        strcasecmp(speed, "very slowly") == 0) return 40;
    return 14;
}

/* Couleur de l'image d'arrivée quand ce n'est pas la carte : « to black »,
 * « to white », « to gray ». nil veut dire « la carte elle-même ». */
static NSColor *visual_image_color(const char *image) {
    if (!image || !*image) return nil;
    if (strcasecmp(image, "black") == 0)   return [NSColor blackColor];
    if (strcasecmp(image, "white") == 0)   return [NSColor whiteColor];
    if (strcasecmp(image, "gray") == 0 ||
        strcasecmp(image, "grey") == 0)    return [NSColor grayColor];
    return nil;                            /* « card » et « inverse » */
}

void cocoa_visual_effect(const char *effect, const char *speed,
                                const char *image) {
    if (!gView || !effect) return;

    gVisualCapturing = YES;
    NSBitmapImageRep *avant = snapshot_view();
    gVisualCapturing = NO;
    if (!avant) return;

    /* Le noyau change de carte juste après notre retour ; on doit donc
     * animer depuis un état qui n'existe pas encore. La solution : rendre la
     * main tout de suite en mémorisant l'image de départ, et laisser
     * l'animation se jouer au premier redessin qui suit. */
    gVisualBefore = avant;
    snprintf(gVisualName,  sizeof gVisualName,  "%s", effect);
    snprintf(gVisualImage, sizeof gVisualImage, "%s", image ? image : "");
    gVisualSteps = visual_frames(speed);
}

/* Zone dévoilée à l'étape t (0 → 1) pour l'effet nommé. Renvoie NO si l'effet
 * n'est pas géométrique — dissolve et checkerboard se traitent à part. */
static BOOL visual_reveal_rect(const char *nom, CGFloat t, NSRect b, NSRect *out) {
    CGFloat W = b.size.width, H = b.size.height;

    /* La vue est isFlipped : l'origine est en HAUT à gauche, et y croît vers
     * le bas. « wipe up » découvre donc depuis le bas de l'écran, ce qui
     * s'écrit ici avec une origine y de H*(1-t) — l'inverse de ce qu'on
     * écrirait dans une vue ordinaire. */

    if (strcasecmp(nom, "wipe right") == 0) { *out = NSMakeRect(0, 0, W*t, H); return YES; }
    if (strcasecmp(nom, "wipe left") == 0)  { *out = NSMakeRect(W*(1-t), 0, W*t, H); return YES; }
    if (strcasecmp(nom, "wipe up") == 0)    { *out = NSMakeRect(0, H*(1-t), W, H*t); return YES; }
    if (strcasecmp(nom, "wipe down") == 0)  { *out = NSMakeRect(0, 0, W, H*t); return YES; }

    if (strcasecmp(nom, "iris open") == 0) {
        CGFloat w = W*t, h = H*t;
        *out = NSMakeRect((W-w)/2, (H-h)/2, w, h); return YES;
    }
    if (strcasecmp(nom, "iris close") == 0 ||
        strcasecmp(nom, "zoom close") == 0 ||
        strcasecmp(nom, "zoom in") == 0) {
        /* Fermeture : la zone d'arrivée est tout SAUF un rectangle central qui
         * rétrécit — donc pas une zone rectangulaire simple. Faute de pouvoir
         * la décrire ainsi, on la traite en fondu ; l'effet reste lisible. */
        (void)out;
        return NO;
    }
    if (strcasecmp(nom, "zoom open") == 0 || strcasecmp(nom, "zoom out") == 0) {
        CGFloat w = W*t, h = H*t;
        *out = NSMakeRect((W-w)/2, (H-h)/2, w, h); return YES;
    }
    if (strcasecmp(nom, "barn door open") == 0) {
        *out = NSMakeRect(W*(1-t)/2, 0, W*t, H); return YES;
    }
    if (strcasecmp(nom, "scroll up") == 0)   { *out = NSMakeRect(0, H*(1-t), W, H*t); return YES; }
    if (strcasecmp(nom, "scroll down") == 0) { *out = NSMakeRect(0, 0, W, H*t); return YES; }
    if (strcasecmp(nom, "scroll left") == 0) { *out = NSMakeRect(W*(1-t), 0, W*t, H); return YES; }
    if (strcasecmp(nom, "scroll right") == 0){ *out = NSMakeRect(0, 0, W*t, H); return YES; }
    if (strcasecmp(nom, "push left") == 0)   { *out = NSMakeRect(W*(1-t), 0, W*t, H); return YES; }
    if (strcasecmp(nom, "push right") == 0)  { *out = NSMakeRect(0, 0, W*t, H); return YES; }
    if (strcasecmp(nom, "push up") == 0)     { *out = NSMakeRect(0, H*(1-t), W, H*t); return YES; }
    if (strcasecmp(nom, "push down") == 0)   { *out = NSMakeRect(0, 0, W, H*t); return YES; }
    if (strcasecmp(nom, "shrink to top") == 0)    { *out = NSMakeRect(0, 0, W, H*t); return YES; }
    if (strcasecmp(nom, "shrink to bottom") == 0) { *out = NSMakeRect(0, H*(1-t), W, H*t); return YES; }
    if (strcasecmp(nom, "stretch from top") == 0) { *out = NSMakeRect(0, 0, W, H*t); return YES; }
    if (strcasecmp(nom, "stretch from bottom")==0){ *out = NSMakeRect(0, H*(1-t), W, H*t); return YES; }
    if (strcasecmp(nom, "plain") == 0)       { *out = NSMakeRect(0, 0, W, H); return YES; }

    return NO;
}
/* Une transition attend-elle d'etre jouee ? drawRect: lisait les quatre
 * drapeaux directement ; maintenant qu'ils sont prives, il pose la question. */
BOOL visual_pending(void) {
    return gVisualBefore && gVisualSteps > 0 && !gVisualRunning && !gVisualCapturing;
}

@implementation HCView (Visual)

/* Joue l'animation. Appelée au premier redessin après un « visual », donc à un
 * moment où la carte d'arrivée est déjà en place. */
- (void)runVisualTransition {
    if (!gVisualBefore || gVisualSteps <= 0 || gVisualRunning) return;
    gVisualRunning = YES;

    int steps = gVisualSteps;

    /* Capturer l'écran d'ARRIVÉE avant de commencer : le recalculer à chaque
     * image multiplierait le coût par le nombre d'étapes. La carte est déjà
     * en place — le noyau a changé de carte juste après nous avoir prévenus. */
    /* gVisualStep à zéro pendant la capture : sinon drawVisualStep prendrait
     * la main et l'on photographierait l'animation au lieu de la carte. */
    gVisualCapturing = YES;
    gVisualStep = 0;
    gVisualAfter = snapshot_view();
    gVisualCapturing = NO;

    /* L'animation passe par drawRect:, et non par lockFocus.
     *
     * La première version dessinait hors du cycle de dessin, avec lockFocus et
     * flushWindow — deux méthodes obsolètes qui ne poussent plus rien quand la
     * vue est adossée à un calque. L'animation tournait, les traces le
     * montraient, mais l'écran ne bougeait pas.
     *
     * On avance donc une étape à la fois, en demandant un redessin et en
     * validant la transaction du calque immédiatement, comme le fait déjà
     * cocoa_idle pour les boucles de script. */
    /* Borne de sûreté : une transition ne doit jamais durer plus d'une
     * seconde. Sans elle, une erreur de drapeau fige l'application sans
     * recours — c'est arrivé, et le seul moyen d'en sortir était de tuer le
     * programme. */
    NSTimeInterval debut = [NSDate timeIntervalSinceReferenceDate];
    if (steps > 60) steps = 60;

    for (int i = 1; i <= steps; i++) {
        gVisualStep = (CGFloat)i / steps;
        [self display];
        [CATransaction flush];
        [NSThread sleepForTimeInterval:1.0/60.0];
        if ([NSDate timeIntervalSinceReferenceDate] - debut > 1.5) break;
    }

    gVisualBefore  = nil;
    gVisualAfter   = nil;
    gVisualSteps   = 0;
    gVisualStep    = 0;
    gVisualRunning = NO;
    [self setNeedsDisplay:YES];
}

/* Dessine l'étape courante de la transition. Appelée par drawRect: quand une
 * animation est en cours ; renvoie NO s'il n'y en a pas, et le dessin normal
 * de la carte reprend alors ses droits. */
- (BOOL)drawVisualStep {
    if (gVisualCapturing) return NO;
    if (!gVisualBefore || gVisualSteps <= 0) return NO;

    /* Transition armée mais pas encore lancée : afficher l'image de DÉPART.
     *
     * Entre le changement de carte et le premier pas de l'animation, un
     * redessin ordinaire a lieu — et montrait la carte d'arrivée en entier,
     * d'où un éclair avant que le wipe ne commence. Redonner l'image de départ
     * pendant cet intervalle rend la transition continue. */
    if (!gVisualRunning) {
        draw_snapshot(gVisualBefore, [self bounds],
                      NSCompositingOperationCopy, 1.0);
        return YES;
    }

    NSRect b = [self bounds];
    CGFloat t = gVisualStep;
    NSColor *teinte = visual_image_color(gVisualImage);

    draw_snapshot(gVisualBefore, b, NSCompositingOperationCopy, 1.0);

    /* ---- damier ----
     * Une grille de cases qui se découvrent en DEUX vagues : les cases de
     * parité paire d'abord, les impaires ensuite. C'est ce décalage qui donne
     * le motif en échiquier ; révéler toutes les cases ensemble ne ferait
     * qu'un agrandissement uniforme.
     *
     * La zone à découvrir n'étant pas un rectangle mais une réunion de
     * rectangles, elle ne peut pas passer par visual_reveal_rect — d'où ce
     * traitement à part, avec un chemin de découpe. */
    if (strcasecmp(gVisualName, "checkerboard") == 0 && gVisualAfter) {
        const CGFloat cote = 32;
        int cols = (int)ceil(b.size.width  / cote);
        int rows = (int)ceil(b.size.height / cote);

        NSBezierPath *masque = [NSBezierPath bezierPath];
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                /* Chaque case a son propre instant d'apparition : sa vague
                 * (0 ou 1) donne la moitié de course, et sa position dans la
                 * grille répartit le reste — sans quoi une vague entière
                 * surgirait d'un coup. */
                int vague = (r + c) & 1;
                CGFloat rang = (CGFloat)(r * cols + c) / (cols * rows);
                CGFloat seuil = vague * 0.5 + rang * 0.5;
                if (t < seuil) continue;
                [masque appendBezierPathWithRect:
                    NSMakeRect(c * cote, r * cote, cote, cote)];
            }
        }
        if (![masque isEmpty]) {
            [NSGraphicsContext saveGraphicsState];
            [masque addClip];
            if (teinte) { [teinte setFill]; NSRectFill(b); }
            else draw_snapshot(gVisualAfter, b, NSCompositingOperationCopy, 1.0);
            [NSGraphicsContext restoreGraphicsState];
        }
        return YES;
    }

    NSRect z;
    if (visual_reveal_rect(gVisualName, t, b, &z)) {
        [NSGraphicsContext saveGraphicsState];
        NSRectClip(z);
        if (teinte) { [teinte setFill]; NSRectFill(z); }
        else draw_snapshot(gVisualAfter, b, NSCompositingOperationCopy, 1.0);
        [NSGraphicsContext restoreGraphicsState];
    } else if (teinte) {
        [[teinte colorWithAlphaComponent:t] setFill];
        NSRectFillUsingOperation(b, NSCompositingOperationSourceOver);
    } else if (gVisualAfter) {
        /* dissolve, checkerboard, venetian blinds : un fondu, qui rend
         * honorablement ce que la trame faisait en noir et blanc. */
        draw_snapshot(gVisualAfter, b, NSCompositingOperationSourceOver, t);
    }
    return YES;
}

@end
