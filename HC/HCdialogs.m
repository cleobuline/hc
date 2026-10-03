//
//  HCdialogs.m — boites de dialogue Info (bouton, carte, fond, pile, icones, contenu)
//

#import "HCview.h"
#import "HCglobals.h"
#import "HCpalettes.h"
#import "HCicons.h"
#import "Hciconedit.h"
#import "graphics.h"
#import <objc/runtime.h>
#include <math.h>       /* lround, pour les couleurs du bouton */

/* Les actions du panneau « Text Style » sont définies dans la catégorie
 * HCView (Dialogs), plus bas dans ce fichier. Mais show_style_panel, qui les
 * pose comme cibles, est une fonction C au niveau fichier, donc AVANT cette
 * @implementation : le compilateur n'en connaît pas encore les sélecteurs et
 * refuse @selector(styleOK:) — « Undeclared selector ».
 *
 * Les autres @selector du fichier ne posent pas ce problème parce qu'ils sont
 * tous écrits À L'INTÉRIEUR de l'@implementation, où les méthodes définies
 * plus bas restent visibles.
 *
 * Une catégorie déclarée sans @implementation correspondante dans la même
 * unité de compilation ne provoque aucun avertissement : c'est la manière
 * habituelle d'annoncer des méthodes privées. Un nom distinct de (Dialogs)
 * évite tout risque de doublon avec ce que HCview.h déclare déjà. */
@interface HCView (DialogsPrivate)
/* iconColler: et paste: servent à HCPanelColler, juste en dessous, qui est
 * déclarée AVANT l'@implementation de la catégorie : sans ces deux lignes le
 * compilateur n'en connaît pas encore les sélecteurs. paste: est bien celle de
 * HCview.m — NSResponder ne la déclare pas, elle n'est visible qu'annoncée. */
- (void)iconColler:(id)sender;
- (void)paste:(id)sender;
- (void)styleOK:(id)sender;
- (void)styleFont:(id)sender;
- (void)styleAlign:(id)sender;
- (void)autoSelectToggled:(id)sender;
@end

/* État de l'édition d'icône, pour la validation de « Coller ». Défini avec le
 * panneau Icônes, plus bas, où vivent ses variables ; déclaré ici parce que
 * HCPanelColler s'en sert et que le panneau Text Style, qui lui est antérieur
 * dans ce fichier, réclame déjà la classe. */
static BOOL icone_prete_a_coller(void);

/* ═══ « Coller » PENDANT QU'UN PANNEAU EST OUVERT ═══════════════════
 *
 * RELEVÉ À L'USAGE : « le coller ne fonctionne plus dans l'éditeur d'icône, ça
 * colle dans la carte ». Le panneau était bien la fenêtre CLÉ, et Cmd-V
 * atterrissait quand même dans la peinture de la carte, derrière lui.
 *
 * LA CAUSE EST LA RECHERCHE DE CIBLE D'APPKIT, et non le collage d'icône, qui
 * marchait — par le bouton « Coller » du panneau. L'article Coller du menu
 * Édition a une cible NULLE ; NSApplication la cherche alors dans cet ordre :
 * la chaîne des répondants de la fenêtre CLÉ, le DÉLÉGUÉ de cette fenêtre,
 * puis, si la fenêtre PRINCIPALE en est une autre, la chaîne de celle-là, son
 * délégué, l'application, son délégué. Or un NSPanel ne devient JAMAIS fenêtre
 * principale : la fenêtre du document le reste tant qu'elle est ouverte. Aucune
 * vue du panneau ne répondant à paste:, la recherche traversait donc le panneau
 * entier et trouvait HCView — qui a fait exactement son travail.
 *
 * CORRIGER HCVIEW N'Y SUFFIRAIT PAS : tant que le panneau n'offre personne, la
 * recherche continue au-delà, et refuser dans HCView casserait le collage
 * légitime de la carte. C'est au panneau de répondre. Sa place dans l'ordre
 * ci-dessus est deux fois juste : APRÈS la chaîne des répondants du panneau,
 * donc un champ de texte en cours d'édition garde son coller de TEXTE, qui est
 * le bon ; AVANT la fenêtre du document, donc la carte ne reçoit plus rien
 * pendant que le panneau est là.
 *
 * LES SEPT AUTRES PANNEAUX DE CE FICHIER ONT LE MÊME DÉFAUT — c'est le site
 * jumeau, et il se corrige ici même plutôt que d'être annoncé en commentaire.
 * Info bouton, Info champ, Text Style, Info carte, Info fond, Info pile,
 * Contenu : aucun n'a de collage à lui, et tous laissaient passer Cmd-V dans
 * la peinture de la carte cachée derrière eux. Ils reçoivent donc le même
 * délégué, en refus : l'article se grise, ce qui le DIT, au lieu de coller
 * ailleurs sans rien dire. Les panneaux d'HyperCard étaient modaux et leurs
 * articles Édition inertes ; griser est donc aussi le comportement fidèle.
 *
 * LES PALETTES DE HCVIEW.M SONT HORS DE CETTE RÈGLE, ET IL FAUT LES Y LAISSER.
 * Outils, motifs, pinceaux, épaisseurs, boîte de message : celles-là sont
 * posées NSWindowStyleMaskNonactivatingPanel avec setBecomesKeyOnlyIfNeeded:YES,
 * donc elles ne PRENNENT PAS la fenêtre clé — la fenêtre du document la garde,
 * et Cmd-V y colle dans la carte, ce qui est justement ce qu'on veut d'une
 * palette flottante. Leur donner ce délégué casserait le collage de peinture.
 * Les dialogues de ce fichier, eux, ont des champs à remplir : il leur faut le
 * clavier, donc la fenêtre clé, et c'est de là que vient le défaut.
 */
@interface HCPanelColler : NSObject <NSWindowDelegate>
@property (weak)   HCView *vue;
@property (assign) BOOL    versIcone;   /* NO : le panneau refuse le collage */
@end

@implementation HCPanelColler

- (void)paste:(id)sender
{
    /* « doMenu "Paste Card" » ARRIVE ICI AUSSI, et ne doit pas y rester.
     *
     * cocoa_menu_hypercard envoie l'action avec to:nil, donc par la même
     * recherche de cible : un script qui colle une carte pendant qu'un de ces
     * panneaux est ouvert tomberait sur nous. Le sender les sépare — l'article
     * de menu envoie son NSMenuItem, cocoa_menu_hypercard envoie la vue — et ce
     * qui ne vient pas d'un menu retourne à HCView, dont c'est le travail. */
    if (![sender isKindOfClass:[NSMenuItem class]]) {
        [self.vue paste:sender];
        return;
    }
    if (self.versIcone) [self.vue iconColler:sender];
}

/* Le délégué étant la cible, c'est à lui que la validation est demandée : sans
 * cette méthode l'article resterait actif faute d'icône choisie ou d'image, et
 * Cmd-V ne ferait rien sans le dire. Le griser le dit. */
- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if ([item action] != @selector(paste:)) return YES;
    if (!self.versIcone) return NO;
    return icone_prete_a_coller();
}

@end

/* Le délégué d'une NSWindow est FAIBLE : sans propriétaire, ARC le libère
 * aussitôt posé et le panneau se retrouve avec un délégué nul — le défaut
 * serait exactement le même, sans que rien ne le montre. Le panneau le retient
 * donc par association, ce qui lui donne précisément sa durée de vie : un
 * panneau recréé à chaque ouverture emporte l'ancien avec lui. */
static void panneau_donne_le_coller(NSPanel *panneau, HCView *vue, BOOL versIcone)
{
    static char cle;
    HCPanelColler *d = [[HCPanelColler alloc] init];
    d.vue       = vue;
    d.versIcone = versIcone;
    objc_setAssociatedObject(panneau, &cle, d, OBJC_ASSOCIATION_RETAIN);
    [panneau setDelegate:d];
}

/* --- etat des dialogues, prive a ce fichier --- */
static NSPanel      *gInfoPanel = nil;
static Object       *gInfoTarget = NULL;
static NSTextField  *gInfoName = nil;
static NSPopUpButton *gInfoStyle = nil;
static NSButton     *gInfoShowName = nil;
static NSButton     *gInfoAutoHilite = nil;
static NSButton     *gInfoEnabled = nil;
static NSButton     *gInfoSharedHilite = nil;
static NSTextField  *gInfoIconField = nil;
/* Le groupement des boutons radio. Sans ce contrôle, la famille n'était
 * accessible qu'au script, et depuis que famille 0 ne groupe plus rien —
 * comme dans HyperCard — c'est ce menu qui rend les boutons radio exclusifs.
 * Il n'est donc plus un confort : c'est la seule porte de l'interface. */
static NSPopUpButton *gInfoFamily = nil;
static NSTextField  *gInfoTextSize = nil;

/* LES TROIS COULEURS DU BOUTON — Back, Fore, Hilite —, dans l'Info.
 *
 * Les couleurs de bouton ne se posaient que par script ; l'utilisatrice a
 * demandé, le 3 octobre, de pouvoir les choisir à la souris. Ce n'est pas
 * une extension de plus : c'est l'interface de celle qui existe.
 *
 * Une case et un puits par couleur. Case décochée : RIEN DE POSÉ, le bouton
 * garde le dessin d'HyperCard — c'est ce que vaut un bouton d'une pile
 * ancienne, et ouvrir puis valider l'Info ne doit rien lui ajouter. Choisir
 * une couleur dans le puits coche la case. Le sélecteur montre l'opacité,
 * qui va dans backalpha, forealpha, hilitealpha. */
static NSButton     *gInfoCoulCase[3];
static NSColorWell  *gInfoCoulPuits[3];

/* Une couleur et son opacité, telles que le noyau les range, en NSColor. */
static NSColor *info_couleur_de(int c, int alpha, NSColor *defaut)
{
    if (!c) return defaut;
    int rvb = HC_COUL_RVB(c);
    return [NSColor colorWithDeviceRed:((rvb >> 16) & 255) / 255.0
                                 green:((rvb >> 8) & 255) / 255.0
                                  blue:(rvb & 255) / 255.0
                                 alpha:HC_ALPHA(alpha) / 255.0];
}

/* Et dans l'autre sens : un NSColor du sélecteur, en couleur et opacité du
 * noyau. Opaque : l'opacité reste à zéro, rien de posé. */
static void info_couleur_vers(NSColor *nc, int *c, int *alpha)
{
    NSColor *d = [nc colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
    if (!d) { *c = 0; *alpha = 0; return; }
    CGFloat r = 0, g = 0, b = 0, a = 1;
    [d getRed:&r green:&g blue:&b alpha:&a];
    int R = (int)lround(r * 255), G = (int)lround(g * 255), B = (int)lround(b * 255);
    int A = (int)lround(a * 255);
    *c = HC_COUL_POSEE | (R << 16) | (G << 8) | B;
    *alpha = (A >= 255) ? 0 : (HC_ALPHA_POSE | (A & 0xFF));
}

/* ---------- panneau « Text Style » ----------
 * Partagé par le dialogue de bouton et celui de champ : les deux visent le
 * même Object, seule l'ouverture diffère. Le panneau de polices du système
 * ne sait poser que le gras et l'italique ; les six autres bits d'HyperCard
 * (souligné, creux, ombré, condensé, étendu, group) n'avaient aucune
 * interface, alors même que le rendu les gère. */
static NSPanel  *gStylePanel  = nil;
static Object   *gStyleTarget = NULL;
static NSButton *gStyleBox[8] = {nil};
static NSPopUpButton *gStyleFont = nil;
static NSTextField   *gStyleSize = nil;
static NSButton      *gStyleAlign[3] = {nil};
static NSTextField   *gStyleLH = nil;

/* Dans l'ordre du codage du noyau : 0 gauche, 1 centre, 2 droite. Comme pour
 * les cases de style, l'indice EST la valeur — pas de table à tenir à jour. */
static const char *ALIGN_LABELS[3] = { "Left", "Center", "Right" };

/* Même ordre que les bits, pour que l'indice de la case soit son décalage. */
static const char *STYLE_LABELS[8] = {
    "Bold", "Italic", "Underline", "Outline",
    "Shadow", "Condense", "Extend", "Group"
};

/* Définie plus bas ; remet la case « taille » du dialogue parent en accord. */
void hc_sync_size_field(Object *o);

/* Range les trois attributs dans l'objet. Appelée par styleOK: comme par
 * styleFont: : sans cela, changeFont: écrirait dans l'objet puis styleOK:
 * rendrait par-dessus l'état figé des contrôles, et la police choisie au
 * panneau système repartirait toute seule en arrière. */
static void commit_style_panel(void)
{
    Object *o = gStyleTarget;
    if (!o) return;

    int st = 0;
    for (int i = 0; i < 8; i++)
        if ([gStyleBox[i] state] == NSControlStateValueOn) st |= (1 << i);
    o->textstyle = st;

    NSString *fam = [gStyleFont titleOfSelectedItem];
    if ([fam length]) {
        free(o->textfont);
        o->textfont = strdup([fam UTF8String]);
    }

    int sz = [gStyleSize intValue];
    if (sz > 0) o->textsize = sz;

    /* L'ALIGNEMENT. Le noyau le connaît depuis toujours — « the textAlign of
     * field X » se lit, s'écrit et se relit du fichier — mais ce panneau ne
     * l'offrait pas, alors que le dialogue de HyperCard le pose juste sous
     * les cases de style. Une propriété qu'un script pouvait poser et que la
     * souris ne pouvait pas : le défaut de famille, à l'envers. */
    for (int i = 0; i < 3; i++)
        if ([gStyleAlign[i] state] == NSControlStateValueOn) {
            o->text_align = i;
            break;
        }

    /* L'INTERLIGNE. Vide — ou zéro, ou négatif — remet l'automatique : c'est
     * la seule façon de REVENIR en arrière après avoir posé une valeur, et
     * sans elle le réglage serait à sens unique. gStyleLH est nil pour un
     * bouton, et [nil stringValue] rend nil, d'où le test explicite. */
    if (gStyleLH) {
        NSString *t = [gStyleLH stringValue];
        int lh = [t intValue];
        o->textheight = ([t length] && lh > 0) ? lh : 0;
    }

    /* Garder la case « taille » du dialogue parent en accord : sinon son OK
     * réécrira l'ancienne valeur par-dessus celle qu'on vient de poser. */
    hc_sync_size_field(o);
}

/* Fermer un dialogue d'info doit fermer le panneau de styles qu'il a ouvert :
 * sinon il reste à l'écran, braqué sur un objet dont le dialogue parent a
 * disparu — et son OK réécrirait le style d'une cible que l'utilisateur croit
 * ne plus éditer. */
static void close_style_panel(void)
{
    if (gStylePanel) [gStylePanel close];
    gStyleTarget = NULL;
}

/* Ouvre le panneau de styles sur un objet quelconque — bouton ou champ.
 * Les cases sont posées dans l'ordre des bits : l'indice i vaut 1 << i, ce
 * qui évite une table de correspondance qu'il faudrait tenir à jour. */
static void show_style_panel(id owner, Object *o)
{
    if (!o) return;
    gStyleTarget = o;

    /* ALIGNEMENT ET INTERLIGNE NE SONT OFFERTS QUE POUR UN CHAMP.
     *
     * field_attr_string est le seul à lire text_align et hc_text_height : le
     * titre d'un bouton est toujours centré et tient sur une ligne, et les
     * deux propriétés n'y changent rien. HyperCard pose bien ces contrôles
     * dans les deux dialogues, mais ce sont chez lui des contrôles inertes,
     * et ce n'est pas une raison de les copier — un réglage qui ne fait rien
     * coûte plus cher que son absence.
     *
     * Le panneau du bouton garde donc EXACTEMENT sa géométrie d'avant : dy
     * vaut zéro, et toutes les ordonnées retrouvent leur valeur d'origine. */
    BOOL estChamp = (o->type == OBJ_FIELD);
    CGFloat dy = estChamp ? 130 : 0;

    if (gStylePanel) [gStylePanel close];
    gStylePanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(340, 300, 240, 300 + dy)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gStylePanel setTitle:@"Text Style"];
    [gStylePanel setReleasedWhenClosed:NO];
    NSView *c = [gStylePanel contentView];

    /* --- police --- */
    NSTextField *fl = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 262 + dy, 70, 18)];
    [fl setStringValue:@"Text font:"];
    [fl setBezeled:NO]; [fl setDrawsBackground:NO]; [fl setEditable:NO];
    [c addSubview:fl];

    gStyleFont = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(90, 258 + dy, 134, 24)];
    [gStyleFont addItemsWithTitles:
        [[[NSFontManager sharedFontManager] availableFontFamilies]
            sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]];

    /* Sélectionner la police courante sans tenir compte de la casse : le
     * noyau stocke ce que le script a écrit (« monaco »), la liste système
     * porte le nom canonique (« Monaco »), et selectItemWithTitle: compare à
     * la lettre près. Même piège que select_style plus haut. */
    if (o->textfont && *o->textfont) {
        NSString *want = hcv_texte(o->textfont);
        for (NSMenuItem *it in [gStyleFont itemArray])
            if ([[it title] caseInsensitiveCompare:want] == NSOrderedSame) {
                [gStyleFont selectItem:it];
                break;
            }
    }
    [c addSubview:gStyleFont];

    /* --- corps --- */
    NSTextField *zl = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 234 + dy, 70, 18)];
    [zl setStringValue:@"Text size:"];
    [zl setBezeled:NO]; [zl setDrawsBackground:NO]; [zl setEditable:NO];
    [c addSubview:zl];

    gStyleSize = [[NSTextField alloc] initWithFrame:NSMakeRect(90, 232 + dy, 60, 22)];
    [gStyleSize setIntValue:o->textsize];
    [c addSubview:gStyleSize];

    for (int i = 0; i < 8; i++) {
        gStyleBox[i] = [[NSButton alloc]
            initWithFrame:NSMakeRect(20, 202 + dy - i * 22, 150, 20)];
        [gStyleBox[i] setButtonType:NSButtonTypeSwitch];
        [gStyleBox[i] setTitle:[NSString stringWithUTF8String:STYLE_LABELS[i]]];
        [gStyleBox[i] setState:(o->textstyle & (1 << i)) ? NSControlStateValueOn
                                                         : NSControlStateValueOff];
        [c addSubview:gStyleBox[i]];
    }

    /* --- alignement ---
     *
     * Trois boutons radio, sous les cases de style, comme chez HyperCard.
     * L'EXCLUSION EST FAITE À LA MAIN dans styleAlign:. AppKit groupe bien
     * tout seul les radios qui partagent vue et action, mais s'en remettre à
     * cette règle rendrait le panneau dépendant d'un détail d'implémentation
     * qu'aucun harnais ici ne peut mesurer — rien de ce fichier ne se
     * compile hors de Xcode. Trois lignes explicites coûtent moins cher
     * qu'une exclusivité qui cesse de fonctionner sans qu'on sache pourquoi.
     *
     * Remis à nil pour un bouton : commit_style_panel parcourt le tableau, et
     * des pointeurs restés d'une ouverture précédente lui feraient poser un
     * alignement que personne n'a choisi. [nil state] rend bien zéro, mais ce
     * n'est pas au hasard d'une règle du langage de tenir la correction. */
    for (int i = 0; i < 3; i++) gStyleAlign[i] = nil;
    gStyleLH = nil;

    if (estChamp) {
        NSTextField *al = [[NSTextField alloc]
            initWithFrame:NSMakeRect(16, 150, 70, 18)];
        [al setStringValue:@"Align:"];
        [al setBezeled:NO]; [al setDrawsBackground:NO]; [al setEditable:NO];
        [c addSubview:al];

        int aln = (o->text_align >= 0 && o->text_align <= 2) ? o->text_align : 0;
        for (int i = 0; i < 3; i++) {
            gStyleAlign[i] = [[NSButton alloc]
                initWithFrame:NSMakeRect(24, 126 - i * 22, 150, 20)];
            [gStyleAlign[i] setButtonType:NSButtonTypeRadio];
            [gStyleAlign[i] setTitle:
                [NSString stringWithUTF8String:ALIGN_LABELS[i]]];
            [gStyleAlign[i] setState:(i == aln) ? NSControlStateValueOn
                                                : NSControlStateValueOff];
            [gStyleAlign[i] setTarget:owner];
            [gStyleAlign[i] setAction:@selector(styleAlign:)];
            [c addSubview:gStyleAlign[i]];
        }

        /* --- interligne ---
         *
         * ZÉRO VEUT DIRE « AUTOMATIQUE », et c'est ce qui décide de la forme
         * de ce contrôle. hc_text_height rend alors les quatre tiers du corps,
         * arrondis comme HyperCard : un champ dont on grossit le texte suit
         * tout seul.
         *
         * Afficher cette valeur calculée dans la case aurait été le piège :
         * il aurait suffi d'ouvrir le panneau et de faire OK pour FIGER
         * l'interligne, et le champ aurait cessé de suivre son corps sans que
         * personne ait rien demandé. La case reste donc VIDE tant que rien
         * n'est posé, et la valeur calculée s'affiche en filigrane — on voit
         * ce qu'on aurait, sans l'avoir écrit.
         *
         * L'interligne ne se VOIT que si « Fixed Line Height » est coché dans
         * le dialogue du champ : c'est le sens de cette propriété chez
         * HyperCard, et HCtext.m ne l'impose que là. On ne duplique pas la
         * case ici — un même réglage à deux endroits finit par diverger. */
        NSTextField *hl = [[NSTextField alloc]
            initWithFrame:NSMakeRect(16, 52, 80, 18)];
        [hl setStringValue:@"Line height:"];
        [hl setBezeled:NO]; [hl setDrawsBackground:NO]; [hl setEditable:NO];
        [c addSubview:hl];

        gStyleLH = [[NSTextField alloc] initWithFrame:NSMakeRect(100, 50, 60, 22)];
        if (o->textheight > 0) [gStyleLH setIntValue:o->textheight];
        else [gStyleLH setStringValue:@""];
        [gStyleLH setPlaceholderString:
            [NSString stringWithFormat:@"%d", hc_text_height(o)]];
        [c addSubview:gStyleLH];
    }

    NSButton *fnt = [[NSButton alloc] initWithFrame:NSMakeRect(16, 8, 80, 28)];
    [fnt setTitle:@"Font…"]; [fnt setBezelStyle:NSBezelStyleRounded];
    [fnt setTarget:owner]; [fnt setAction:@selector(styleFont:)];
    [c addSubview:fnt];

    NSButton *ok = [[NSButton alloc] initWithFrame:NSMakeRect(104, 8, 80, 28)];
    [ok setTitle:@"OK"]; [ok setBezelStyle:NSBezelStyleRounded];
    [ok setTarget:owner]; [ok setAction:@selector(styleOK:)];
    [ok setKeyEquivalent:@"\r"];
    [c addSubview:ok];

    panneau_donne_le_coller(gStylePanel, owner, NO);
    [gStylePanel makeKeyAndOrderFront:nil];
}

 
static NSPanel     *gBgPanel = nil;
static Object      *gBgTarget = NULL;
static NSTextField *gBgName = nil;

static NSPanel     *gCardPanel = nil;
static Object      *gCardTarget = NULL;
static NSTextField *gCardName = nil;
static NSButton    *gCardMarked = nil;
static NSButton    *gCardDontSearch = nil;
static NSButton    *gCardCantDelete = nil;
static NSButton    *gBgDontSearch = nil;
static NSButton    *gBgCantDelete = nil;

static NSPanel     *gStackPanel = nil;

static NSPanel     *gIconPanel = nil;
static IconGrid    *gIconGrid = nil;
static NSTextField *gIconLabel = nil;
/* moitie edition du panneau Icones */
static HCFatBits   *gIconBits = nil;
static HCIconPalette *gIconPal  = nil;    /* la bande de couleurs */
static NSButton      *gIconCoul = nil;    /* la case « Couleur » */
static NSTextField *gIconName = nil;
/* Quelle icône le champ Nom est en train de nommer. Indispensable : quand on
 * clique une autre icône, la sélection a déjà changé au moment où l'on valide,
 * et sans ce témoin on renommerait la nouvelle avec le texte de l'ancienne. */
static int          gIconNameId = 0;
static NSTextField *gIconInfo = nil;
static Object      *gIconStack = NULL;

/* Y a-t-il de quoi coller ? Une icône choisie, et une image au presse-papiers.
 * Déclarée en tête du fichier, pour HCPanelColler. */
static BOOL icone_prete_a_coller(void)
{
    if (!gIconGrid || gIconGrid.selected == 0) return NO;
    return [[NSPasteboard generalPasteboard]
              canReadObjectForClasses:@[[NSImage class]] options:nil];
}

static Object      *gStackTarget = NULL;
static NSTextField *gStackName = nil;
static NSPanel    *gContentsPanel = nil;
static NSTextView *gContentsView = nil;
static Object     *gContentsTarget = NULL;

/* ---------- popup de style : la casse ----------
 * HyperTalk ignore la casse, NSPopUpButton non. Le noyau accepte aussi bien
 * « radioButton » que « radiobutton » (cf. HCview.m et hc_core.c), mais
 * selectItemWithTitle: compare a la lettre pres : un bouton dont le style
 * est en minuscules ne trouve aucun element, le popup se retrouve SANS
 * selection, et titleOfSelectedItem rend nil a la validation.
 * strdup(NULL) plantait alors dans infoOK: / fldOK:. */
static void select_style(NSPopUpButton *pop, const char *style)
{
    NSString *want = hcv_texte(style && *style ? style : "rectangle");
    for (NSMenuItem *it in [pop itemArray]) {
        if ([[it title] caseInsensitiveCompare:want] == NSOrderedSame) {
            [pop selectItem:it];
            return;
        }
    }
    /* style inconnu du menu : on retombe sur le premier plutot que de
       laisser le popup sans selection. */
    if ([pop numberOfItems] > 0) [pop selectItemAtIndex:0];
}

/* Remplace une chaine du noyau par le contenu d'un controle Cocoa.
 * Ne touche a rien si la source est nil : c'est le filet qui manquait. */
static void set_cstr(char **dst, NSString *s)
{
    if (!s) return;
    const char *u = [s UTF8String];
    if (!u) return;
    char *n = strdup(u);
    if (!n) return;          /* plus de memoire : on garde l'ancienne valeur */
    free(*dst);
    *dst = n;
}

@implementation HCView (Dialogs)

static NSPanel      *gFldPanel = nil;
static Object       *gFldTarget = NULL;
static NSTextField  *gFldName = nil;
static NSPopUpButton *gFldStyle = nil;
static NSButton     *gFldLock = nil;
static NSButton     *gFldWide = nil;
static NSButton     *gFldFixed = nil;
static NSButton     *gFldLines = nil;
static NSButton     *gFldTab = nil;
static NSButton     *gFldNoSearch = nil;
static NSButton     *gFldShared = nil;
static NSButton     *gFldNoWrap = nil;
static NSButton     *gFldAutoSel = nil;
static NSButton     *gFldMultiple = nil;
static NSTextField  *gFldTextSize = nil;
//extern Object *gFontTarget;

//Object *gFontTarget = NULL;    // objet vise par le panneau des polices

- (void)showFieldInfo:(Object *)obj {
    if (!obj) return;
    gFldTarget = obj;

    if (gFldPanel) [gFldPanel close];
    gFldPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(300, 200, 380, 360)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gFldPanel setTitle:@"Field Info"];
    [gFldPanel setReleasedWhenClosed:NO];
    NSView *c = [gFldPanel contentView];

    // --- nom ---
    NSTextField *lb = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 282, 80, 18)];
    [lb setStringValue:@"Field Name:"];
    [lb setBezeled:NO]; [lb setDrawsBackground:NO]; [lb setEditable:NO];
    [c addSubview:lb];

    gFldName = [[NSTextField alloc] initWithFrame:NSMakeRect(100, 280, 262, 22)];
    [gFldName setStringValue:hcv_texte(obj->name)];
    [c addSubview:gFldName];

    // --- identifiants ---
    /* Le rang vient du noyau. Recompté ici à partir de l'index brut dans
     * parts[], il mélangeait boutons et champs : un champ posé après cinq
     * boutons s'affichait « Field number: 6 », et « card field 6 » recopié
     * dans un script ne désignait rien. Les deux compteurs sont par ailleurs
     * distincts — numéro de champ et numéro de part — là où l'ancien code
     * imprimait deux fois la même valeur. */
    int fnum  = hc_object_number(obj);
    int pnum  = hc_part_number(obj);
    BOOL isBg = hc_owner_is_bg(obj) ? YES : NO;

    NSTextField *ids = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 218, 180, 52)];
    [ids setStringValue:[NSString stringWithFormat:
        @"%@ number: %d\nPart number: %d\nField ID: %d",
        isBg ? @"Bg field" : @"Card field", fnum, pnum, obj->id]];
    [ids setBezeled:NO]; [ids setDrawsBackground:NO]; [ids setEditable:NO];
    [c addSubview:ids];

    // --- style ---
    NSTextField *sl = [[NSTextField alloc] initWithFrame:NSMakeRect(210, 250, 40, 18)];
    [sl setStringValue:@"Style:"];
    [sl setBezeled:NO]; [sl setDrawsBackground:NO]; [sl setEditable:NO];
    [c addSubview:sl];

    gFldStyle = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(250, 246, 112, 24)];
    [gFldStyle addItemsWithTitles:@[@"transparent", @"opaque", @"rectangle",
                                    @"shadow", @"scrolling"]];
    select_style(gFldStyle, obj->style);
    [c addSubview:gFldStyle];

    // --- cases a cocher ---
    NSButton *(^mkChk)(NSString*, BOOL, CGFloat) = ^NSButton*(NSString *t, BOOL on, CGFloat y) {
        NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(210, y, 152, 20)];
        [b setButtonType:NSButtonTypeSwitch];
        [b setTitle:t];
        [b setState:on ? NSControlStateValueOn : NSControlStateValueOff];
        [c addSubview:b];
        return b;
    };
    /* Ordre repris de HyperCard 2.4 : Lock Text, Don't Wrap, Auto Select,
     * Multiple Lines, Wide Margins, Fixed Line Height, Show Lines, Auto Tab,
     * Don't Search. Les habitués retrouvent chaque case à sa place. */
    /* Sous le menu Style, qui occupe 246 à 270 : la première case chevauchait
     * le menu déroulant, les deux se dessinant l'un sur l'autre. */
    gFldLock     = mkChk(@"Lock Text",         obj->locktext,      218);
    gFldNoWrap   = mkChk(@"Don't Wrap",        obj->dont_wrap,     198);
    gFldAutoSel  = mkChk(@"Auto Select",       obj->auto_select,   178);
    gFldMultiple = mkChk(@"Multiple Lines",    obj->multiple_lines,158);
    gFldWide     = mkChk(@"Wide Margins",      obj->wide_margins,  138);
    gFldFixed    = mkChk(@"Fixed Line Height", obj->fixed_lh,      118);
    gFldLines    = mkChk(@"Show Lines",        obj->show_lines,     98);
    gFldTab      = mkChk(@"Auto Tab",          obj->auto_tab,       78);
    gFldNoSearch = mkChk(@"Don't Search",      obj->dont_search,    58);
    gFldShared   = mkChk(@"Shared Text",       obj->shared_text,    38);

    /* Multiple Lines n'a de sens qu'avec Auto Select : HyperCard la grise
     * tant que l'autre n'est pas cochée. */
    [gFldMultiple setEnabled:(obj->auto_select != 0)];
    [gFldAutoSel setTarget:self];
    [gFldAutoSel setAction:@selector(autoSelectToggled:)];

    // --- taille de texte ---
    NSTextField *tl = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 178, 70, 18)];
    [tl setStringValue:@"Text size:"];
    [tl setBezeled:NO]; [tl setDrawsBackground:NO]; [tl setEditable:NO];
    [c addSubview:tl];

    gFldTextSize = [[NSTextField alloc] initWithFrame:NSMakeRect(86, 176, 60, 22)];
    [gFldTextSize setStringValue:[NSString stringWithFormat:@"%d", obj->textsize]];
    [c addSubview:gFldTextSize];

    // --- boutons ---
    NSButton *(^mkFI)(NSString*, SEL, CGFloat, CGFloat) =
        ^NSButton*(NSString *t, SEL a, CGFloat x, CGFloat y) {
            NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, 96, 28)];
            [b setTitle:t]; [b setBezelStyle:NSBezelStyleRounded];
            [b setTarget:self]; [b setAction:a];
            [c addSubview:b];
            return b;
        };
    mkFI(@"Text Style…", @selector(fldTextStyle:), 16, 88);
    mkFI(@"Script…", @selector(fldScript:), 16, 52);
    /* Sous la dernière case, qui descend maintenant à y=38 : à y=16 les
     * boutons passent dessous sans la toucher. */
    mkFI(@"Cancel",  @selector(fldCancel:), 160, 8);
    NSButton *ok = mkFI(@"OK", @selector(fldOK:), 264, 8);
    [ok setKeyEquivalent:@"\r"];

    panneau_donne_le_coller(gFldPanel, self, NO);
    [gFldPanel makeKeyAndOrderFront:nil];
}
- (void)fldTextStyle:(id)sender {
    show_style_panel(self, gFldTarget);
}

- (void)infoTextStyle:(id)sender {
    show_style_panel(self, gInfoTarget);
}

- (void)styleOK:(id)sender {
    commit_style_panel();
    [gStylePanel close];
    gStyleTarget = NULL;
    [self setNeedsDisplay:YES];
}

/* Le panneau de polices du système reste utile pour ce que le menu ne donne
 * pas : les corps non listés et l'aperçu. Il ne touchera qu'aux bits gras et
 * italique, changeFont: préservant les six autres. */
/* « Auto Select » commande « Multiple Lines » : la seconde n'a de sens qu'avec
 * la première, et HyperCard la grise tant que l'autre n'est pas cochée. */
- (void)autoSelectToggled:(id)sender {
    (void)sender;
    BOOL on = ([gFldAutoSel state] == NSControlStateValueOn);
    [gFldMultiple setEnabled:on];
    if (!on) [gFldMultiple setState:NSControlStateValueOff];
    /* Auto Select impose le verrouillage : la case se coche d'elle-même, pour
     * que l'utilisateur voie tout de suite ce qu'implique son choix. */
    if (on) [gFldLock setState:NSControlStateValueOn];
}

/* Un seul alignement à la fois. Voir la note de la construction : l'exclusion
 * est explicite plutôt que déduite du groupage automatique d'AppKit. */
- (void)styleAlign:(id)sender {
    for (int i = 0; i < 3; i++)
        [gStyleAlign[i] setState:(gStyleAlign[i] == sender)
                                 ? NSControlStateValueOn
                                 : NSControlStateValueOff];
}

- (void)styleFont:(id)sender {
    if (!gStyleTarget) return;
    commit_style_panel();          /* voir le commentaire de commit_style_panel */

    gFontTarget = gStyleTarget;
    CGFloat sz = gStyleTarget->textsize > 0 ? gStyleTarget->textsize : 12;
    NSFont *f = nil;
    if (gStyleTarget->textfont && *gStyleTarget->textfont) {
        NSString *nm = hcv_texte(gStyleTarget->textfont);
        /* Pas les noms de police système, qui commencent par un point :
         * CoreText refuse de les servir par leur nom et rend du Times en
         * l'annonçant dans la console. Les piles enregistrées avant que l'on
         * ne stocke le nom de famille en portent encore. */
        if (nm && ![nm hasPrefix:@"."])
            f = [NSFont fontWithName:nm size:sz];
    }
    if (!f) f = [NSFont systemFontOfSize:sz];
    [[NSFontManager sharedFontManager] setSelectedFont:f isMultiple:NO];
    [[NSFontManager sharedFontManager] orderFrontFontPanel:self];
}

/* Le panneau des polices vient de changer la taille d'un objet. Si c'est
 * celui qu'affiche le dialogue d'info, remettre la petite case en accord :
 * sinon fldOK:/infoOK: reecriront l'ancienne valeur par-dessus la nouvelle,
 * et l'utilisateur verra sa taille revenir toute seule en arriere. */
void hc_sync_size_field(Object *o)
{
    if (!o) return;
    if (o == gFldTarget  && gFldTextSize)  [gFldTextSize  setIntValue:o->textsize];
    if (o == gInfoTarget && gInfoTextSize) [gInfoTextSize setIntValue:o->textsize];
}

- (void)fldOK:(id)sender {
    Object *o = gFldTarget;
    if (o) {
        set_cstr(&o->name,  [gFldName stringValue]);
        set_cstr(&o->style, [gFldStyle titleOfSelectedItem]);
        o->textsize     = [[gFldTextSize stringValue] intValue];   /* voir hc_sync_field_dialog */
        o->locktext     = ([gFldLock state]     == NSControlStateValueOn);
        o->wide_margins = ([gFldWide state]     == NSControlStateValueOn);
        o->fixed_lh     = ([gFldFixed state]    == NSControlStateValueOn);
        o->show_lines   = ([gFldLines state]    == NSControlStateValueOn);
        o->auto_tab     = ([gFldTab state]      == NSControlStateValueOn);
        o->dont_search  = ([gFldNoSearch state] == NSControlStateValueOn);
        /* Par hc_set_shared_text et non en direct : la bascule déménage le
         * texte et les plages de style entre la carte et l'objet. */
        hc_set_shared_text(o, ([gFldShared state] == NSControlStateValueOn));
        o->dont_wrap    = ([gFldNoWrap state]   == NSControlStateValueOn);
        o->auto_select  = ([gFldAutoSel state]  == NSControlStateValueOn);
        o->multiple_lines = ([gFldMultiple state] == NSControlStateValueOn);
        /* Auto Select impose le verrouillage : on ne tape pas dans une liste
         * de choix, et sans cela le clic ouvrirait l'éditeur au lieu de
         * sélectionner la ligne. HyperCard fait de même. */
        if (o->auto_select) o->locktext = 1;
    }
    [gFldPanel close];
    close_style_panel();
    gFldTarget  = NULL;
    gFontTarget = NULL;   /* sinon le panneau des polices reste braque sur
                             ce champ et toutes les modifications de police
                             suivantes lui reviennent, quel que soit l'objet
                             reellement selectionne. */
    [self setNeedsDisplay:YES];
}

- (void)fldCancel:(id)sender {
    [gFldPanel close];
    close_style_panel();
    gFontTarget = NULL;
    gFldTarget = NULL;
}

- (void)fldScript:(id)sender {
    Object *o = gFldTarget;
    [self fldOK:sender];
    if (o) [self editScriptOf:o];
}
/* Les puits rendus inactifs, et le sélecteur fermé : un puits encore actif
 * après la fermeture du panneau recevrait la couleur suivante choisie
 * ailleurs — dans la palette de peinture par exemple. */
static void info_couleurs_ferme(void)
{
    for (int i = 0; i < 3; i++)
        if (gInfoCoulPuits[i]) [gInfoCoulPuits[i] deactivate];
    if ([NSColorPanel sharedColorPanelExists])
        [[NSColorPanel sharedColorPanel] orderOut:nil];
}

- (void)showButtonInfo:(Object *)obj {
    if (!obj) return;
    gInfoTarget = obj;

    if (gInfoPanel) [gInfoPanel close];
    gInfoPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(300, 260, 360, 336)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gInfoPanel setTitle:@"Button Info"];
    [gInfoPanel setReleasedWhenClosed:NO];
    NSView *c = [gInfoPanel contentView];

    /* --- les couleurs : la rangée du haut, dans les 36 points ajoutés ---
     * Le contenu se compte depuis le BAS : agrandir le panneau par le haut
     * laisse tout le reste à sa place, relevée au point près. */
    {
        static NSString *const TITRES[3] = { @"Back", @"Fore", @"Hilite" };
        int    coul[3]  = { obj->backcolor, obj->forecolor, obj->hilitecolor };
        int    alpha[3] = { obj->backalpha, obj->forealpha, obj->hilitealpha };
        NSColor *defaut[3] = { [NSColor whiteColor], [NSColor blackColor],
                               [NSColor blackColor] };
        [[NSColorPanel sharedColorPanel] setShowsAlpha:YES];
        for (int i = 0; i < 3; i++) {
            CGFloat x = 16 + i * 114;
            gInfoCoulCase[i] = [[NSButton alloc] initWithFrame:NSMakeRect(x, 300, 60, 20)];
            [gInfoCoulCase[i] setButtonType:NSButtonTypeSwitch];
            [gInfoCoulCase[i] setTitle:TITRES[i]];
            [gInfoCoulCase[i] setState:coul[i] ? NSControlStateValueOn
                                               : NSControlStateValueOff];
            [c addSubview:gInfoCoulCase[i]];
            gInfoCoulPuits[i] = [[NSColorWell alloc] initWithFrame:NSMakeRect(x + 62, 298, 40, 24)];
            [gInfoCoulPuits[i] setColor:info_couleur_de(coul[i], alpha[i], defaut[i])];
            [gInfoCoulPuits[i] setTag:i];
            [gInfoCoulPuits[i] setTarget:self];
            [gInfoCoulPuits[i] setAction:@selector(infoCouleurChoisie:)];
            [c addSubview:gInfoCoulPuits[i]];
        }
    }

    // --- nom ---
    NSTextField *lb = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 262, 90, 18)];
    [lb setStringValue:@"Button Name:"];
    [lb setBezeled:NO]; [lb setDrawsBackground:NO]; [lb setEditable:NO];
    [c addSubview:lb];

    gInfoName = [[NSTextField alloc] initWithFrame:NSMakeRect(110, 260, 232, 22)];
    [gInfoName setStringValue:hcv_texte(obj->name)];
    [c addSubview:gInfoName];

    // --- identifiants (lecture seule) ---
    /* Même correction que pour les champs : le rang vient du noyau, les deux
     * compteurs sont distincts, et le libellé dit enfin si l'objet est posé
     * sur la carte ou sur le fond — « Card button » était écrit en dur, y
     * compris pour un bouton de fond. */
    int bnum  = hc_object_number(obj);
    int pnum  = hc_part_number(obj);
    NSString *kind = hc_owner_is_bg(obj) ? @"Bg" : @"Card";

    NSTextField *ids = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 200, 200, 52)];
    [ids setStringValue:[NSString stringWithFormat:
        @"%@ button number: %d\n%@ part number: %d\n%@ button ID: %d",
        kind, bnum, kind, pnum, kind, obj->id]];
    [ids setBezeled:NO]; [ids setDrawsBackground:NO]; [ids setEditable:NO];
    [c addSubview:ids];

    // --- style ---
    NSTextField *sl = [[NSTextField alloc] initWithFrame:NSMakeRect(224, 234, 40, 18)];
    [sl setStringValue:@"Style:"];
    [sl setBezeled:NO]; [sl setDrawsBackground:NO]; [sl setEditable:NO];
    [c addSubview:sl];
    NSTextField *tl = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 126, 70, 18)];
        [tl setStringValue:@"Text size:"];
        [tl setBezeled:NO]; [tl setDrawsBackground:NO]; [tl setEditable:NO];
        [c addSubview:tl];

        gInfoTextSize = [[NSTextField alloc] initWithFrame:NSMakeRect(86, 124, 60, 22)];
    [gInfoTextSize setStringValue:[NSString stringWithFormat:@"%d", obj->textsize]];        [c addSubview:gInfoTextSize];
    gInfoStyle = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(224, 210, 120, 24)];
    [gInfoStyle addItemsWithTitles:@[@"transparent", @"opaque", @"rectangle",
                                         @"shadow", @"roundRect", @"checkBox",
                                         @"radioButton", @"standard", @"default",
                                         @"oval", @"popup", @"polygon"]];
    /* « polygon » : l'extension de HC. Il FAUT qu'il soit dans la liste —
     * select_style retombe sur le premier article, « transparent », pour un
     * style qu'elle ignore, et la validation l'écrirait : ouvrir l'Info d'un
     * polygone puis OK l'aurait changé en bouton transparent. */
    
    select_style(gInfoStyle, obj->style);
    [c addSubview:gInfoStyle];

    // --- cases a cocher ---
    gInfoShowName = [[NSButton alloc] initWithFrame:NSMakeRect(224, 178, 120, 20)];
    [gInfoShowName setButtonType:NSButtonTypeSwitch];
    [gInfoShowName setTitle:@"Show Name"];
    [gInfoShowName setState:obj->showname ? NSControlStateValueOn : NSControlStateValueOff];
    [c addSubview:gInfoShowName];

    gInfoAutoHilite = [[NSButton alloc] initWithFrame:NSMakeRect(224, 156, 120, 20)];
    [gInfoAutoHilite setButtonType:NSButtonTypeSwitch];
    [gInfoAutoHilite setTitle:@"Auto Hilite"];
    [gInfoAutoHilite setState:obj->autohilite ? NSControlStateValueOn : NSControlStateValueOff];
    [c addSubview:gInfoAutoHilite];

    /* « Enabled », à la place qu'elle occupe dans l'Info bouton de HyperCard 2 :
     * sous Auto Hilite. Décochée, le bouton ne reçoit plus les messages de
     * souris et son nom se dessine en gris. */
    gInfoEnabled = [[NSButton alloc] initWithFrame:NSMakeRect(224, 134, 120, 20)];
    [gInfoEnabled setButtonType:NSButtonTypeSwitch];
    [gInfoEnabled setTitle:@"Enabled"];
    [gInfoEnabled setState:obj->enabled ? NSControlStateValueOn : NSControlStateValueOff];
    [c addSubview:gInfoEnabled];

    /* « Shared Hilite » : n'a de sens que pour un bouton de FOND, l'allumage
     * d'un bouton de carte n'ayant personne avec qui être partagé. On la grise
     * plutôt que de la cacher, pour que la boîte garde la même allure. */
    gInfoSharedHilite = [[NSButton alloc] initWithFrame:NSMakeRect(224, 112, 130, 20)];
    [gInfoSharedHilite setButtonType:NSButtonTypeSwitch];
    [gInfoSharedHilite setTitle:@"Shared Hilite"];
    [gInfoSharedHilite setState:obj->shared_hilite ? NSControlStateValueOn : NSControlStateValueOff];
    [gInfoSharedHilite setEnabled:(obj->owner && obj->owner->type == OBJ_BACKGROUND)];
    [c addSubview:gInfoSharedHilite];

    // --- icone (par identifiant, en attendant le selecteur) ---
    NSTextField *il = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 156, 40, 18)];
    [il setStringValue:@"Icon:"];
    [il setBezeled:NO]; [il setDrawsBackground:NO]; [il setEditable:NO];
    [c addSubview:il];

    gInfoIconField = [[NSTextField alloc] initWithFrame:NSMakeRect(56, 154, 70, 22)];
    [gInfoIconField setStringValue:[NSString stringWithFormat:@"%d", obj->icon]];
    [c addSubview:gInfoIconField];

    /* --- famille : le groupement des boutons radio ---
     *
     * « None » est la valeur par défaut et n'est PAS la famille zéro : c'est
     * l'absence de famille, et elle NE GROUPE RIEN. Deux boutons radio sans
     * famille sont deux boutons indépendants — c'est la règle d'HyperCard, et
     * c'est précisément pourquoi « the family of » y a été introduit en 2.0.
     *
     * Ce commentaire a dit le contraire un temps : « un bouton sans famille
     * ne s'exclut qu'avec les autres boutons radio sans famille ». C'était
     * vrai d'une commodité que nous avions ajoutée au CLIC, et que le SCRIPT
     * n'appliquait pas — les deux chemins donnaient deux états de pile
     * différents. La commodité est partie ; il ne reste qu'une règle.
     *
     * Le rang dans le menu EST le numéro de famille : index 0 = None = 0,
     * index 1 = famille 1, et ainsi de suite jusqu'à 15. Pas de table de
     * correspondance à tenir à jour, donc pas de table à désynchroniser. */
    /* PLACEMENT : à droite du bouton « Text Style… », qui va de x=16 à x=112
     * et de y=88 à y=116. La première version posait le menu en x=70 et le
     * recouvrait — invisible ici, faute d'AppKit pour compiler, et visible
     * seulement à l'écran. On relève donc les rectangles voisins plutôt que
     * de poser au jugé : « Shared Hilite » occupe y=112..132, la rangée de
     * boutons y=52..80. La bande x=130..344, y=88..112 est libre. */
    NSTextField *fl = [[NSTextField alloc] initWithFrame:NSMakeRect(130, 92, 54, 18)];
    [fl setStringValue:@"Family:"];
    [fl setBezeled:NO]; [fl setDrawsBackground:NO]; [fl setEditable:NO];
    [c addSubview:fl];

    gInfoFamily = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(186, 88, 80, 24)];
    [gInfoFamily addItemWithTitle:@"None"];
    for (int i = 1; i <= 15; i++)
        [gInfoFamily addItemWithTitle:[NSString stringWithFormat:@"%d", i]];
    {
        int f = obj->family;
        [gInfoFamily selectItemAtIndex:(f >= 0 && f <= 15) ? f : 0];
    }
    /* TOUJOURS ACTIF, ET C'EST DÉLIBÉRÉ.
     *
     * La première version grisait le menu hors d'un bouton radio, comme
     * « Shared Hilite » l'est hors d'un bouton de fond. Mais le STYLE se
     * change dans ce même panneau : on aurait choisi « radioButton » dans le
     * menu du dessus pour trouver la famille encore inerte, et il aurait
     * fallu valider puis rouvrir pour la poser. Un piège que rien ne signale.
     *
     * Une famille sur un bouton qui n'est pas radio ne fait rien : elle est
     * rangée, et prend effet le jour où le style le devient. Rien à griser,
     * donc rien à resynchroniser. */
    [c addSubview:gInfoFamily];

    // --- boutons ---
    NSButton *(^mk)(NSString*, SEL, CGFloat, CGFloat) =
        ^NSButton*(NSString *t, SEL a, CGFloat x, CGFloat y) {
            NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, 96, 28)];
            [b setTitle:t];
            [b setBezelStyle:NSBezelStyleRounded];
            [b setTarget:self];
            [b setAction:a];
            [c addSubview:b];
            return b;
        };
    mk(@"Text Style…", @selector(infoTextStyle:), 16, 88);
    mk(@"Script…", @selector(infoScript:), 16, 52);
    mk(@"Contents…", @selector(infoContents:), 224, 52);
    mk(@"Icon…",   @selector(infoIcon:),   120, 52);
    mk(@"Cancel",  @selector(infoCancel:), 128, 16);
    NSButton *ok = mk(@"OK", @selector(infoOK:), 232, 16);
    [ok setKeyEquivalent:@"\r"];

    panneau_donne_le_coller(gInfoPanel, self, NO);
    [gInfoPanel makeKeyAndOrderFront:nil];
}
- (void)contentsCancel:(id)sender {
    [gContentsPanel close];
    gContentsTarget = NULL;
}
- (void)contentsOK:(id)sender {
    if (gContentsTarget)
        hc_set_field_text(gContentsTarget, [[gContentsView string] UTF8String]);
    [gContentsPanel close];
    gContentsTarget = NULL;
    [self setNeedsDisplay:YES];
}
- (void)showCardInfo {
    if (hcv_menu_trappe("Card Info…")) return;   /* la pile détourne l'article */
    Object *card = hc_current_card();
    if (!card) return;
    gCardTarget = card;

    /* Vingt points de plus en hauteur : les deux verrous ont pris la place
     * qui restait au-dessus des boutons. */
    gCardPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(300, 300, 340, 220)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gCardPanel setTitle:@"Card Info"];
    [gCardPanel setReleasedWhenClosed:NO];
    NSView *c = [gCardPanel contentView];

    NSTextField *lb = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 180, 90, 18)];
    [lb setStringValue:@"Card Name:"];
    [lb setBezeled:NO]; [lb setDrawsBackground:NO]; [lb setEditable:NO];
    [c addSubview:lb];

    gCardName = [[NSTextField alloc] initWithFrame:NSMakeRect(110, 178, 214, 22)];
    [gCardName setStringValue:hcv_texte(card->name)];
    [c addSubview:gCardName];

    // rang de la carte dans la pile et total
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    int rang = 0, total = 0;
    if (stack)
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i]->type == OBJ_CARD) {
                total++;
                if (stack->parts[i] == card) rang = total;
            }

    NSTextField *ids = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 116, 308, 52)];
    /* « Card fields » affichait card->nparts, c'est-à-dire boutons compris.
     * On compte les champs, et eux seuls — et ceux de la carte, sans y
     * ajouter ceux du fond, puisque « card field N » les numérote à part. */
    [ids setStringValue:[NSString stringWithFormat:
        @"Card number: %d out of %d\nCard ID: %d\nCard fields: %d",
        rang, total, card->id, hc_part_count(card, OBJ_FIELD)]];
    [ids setBezeled:NO]; [ids setDrawsBackground:NO]; [ids setEditable:NO];
    [c addSubview:ids];

    /* « Card Marked », comme dans l'Info carte de HyperCard 2.
     *
     * Le marquage existait partout AILLEURS — « mark this card », « set the
     * marked of card 3 to true », « go next marked card », « the number of
     * marked cards », et il s'enregistre avec la pile — mais cette boîte, qui
     * est l'endroit où on s'attend à le trouver, ne le montrait pas. Marquer
     * une carte à la main demandait de passer par la boîte de messages. */
    gCardMarked = [[NSButton alloc] initWithFrame:NSMakeRect(16, 96, 200, 20)];
    [gCardMarked setButtonType:NSButtonTypeSwitch];
    [gCardMarked setTitle:@"Card Marked"];
    [gCardMarked setState:card->marked ? NSControlStateValueOn
                                       : NSControlStateValueOff];
    [c addSubview:gCardMarked];

    /* Les deux verrous, dans l'ordre de HyperCard 2.
     *
     * « Don't Search » saute la carte ENTIÈRE lors d'un find, et non tel ou
     * tel de ses champs — c'est ainsi qu'on tient un mode d'emploi ou une
     * carte d'index hors des résultats sans cocher chaque champ.
     *
     * « Can't Delete » refuse « delete this card » comme l'article de menu.
     * Les deux s'enregistrent avec la pile : un verrou qui disparaît à la
     * sauvegarde ne protège rien. */
    gCardDontSearch = [[NSButton alloc] initWithFrame:NSMakeRect(16, 76, 220, 20)];
    [gCardDontSearch setButtonType:NSButtonTypeSwitch];
    [gCardDontSearch setTitle:@"Don't Search This Card"];
    [gCardDontSearch setState:card->dont_search ? NSControlStateValueOn
                                                : NSControlStateValueOff];
    [c addSubview:gCardDontSearch];

    gCardCantDelete = [[NSButton alloc] initWithFrame:NSMakeRect(16, 56, 220, 20)];
    [gCardCantDelete setButtonType:NSButtonTypeSwitch];
    [gCardCantDelete setTitle:@"Can't Delete This Card"];
    [gCardCantDelete setState:card->cant_delete ? NSControlStateValueOn
                                                : NSControlStateValueOff];
    [c addSubview:gCardCantDelete];

    NSButton *(^mkCD)(NSString*, SEL, CGFloat) = ^NSButton*(NSString *t, SEL a, CGFloat x) {
        NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(x, 16, 88, 28)];
        [b setTitle:t]; [b setBezelStyle:NSBezelStyleRounded];
        [b setTarget:self]; [b setAction:a];
        [c addSubview:b];
        return b;
    };
    mkCD(@"Script…", @selector(cardScript:), 16);
    mkCD(@"Cancel",  @selector(cardCancel:), 148);
    NSButton *ok = mkCD(@"OK", @selector(cardOK:), 240);
    [ok setKeyEquivalent:@"\r"];

    panneau_donne_le_coller(gCardPanel, self, NO);
    [gCardPanel makeKeyAndOrderFront:nil];
}
- (void)cardOK:(id)sender {
    if (gCardTarget) {
        free(gCardTarget->name);
        gCardTarget->name = strdup([[gCardName stringValue] UTF8String]);
        gCardTarget->marked =
            ([gCardMarked state] == NSControlStateValueOn) ? 1 : 0;
        gCardTarget->dont_search =
            ([gCardDontSearch state] == NSControlStateValueOn) ? 1 : 0;
        gCardTarget->cant_delete =
            ([gCardCantDelete state] == NSControlStateValueOn) ? 1 : 0;
    }
    [gCardPanel close];
    gCardTarget = NULL;
    gCardMarked = gCardDontSearch = gCardCantDelete = nil;
    [self setNeedsDisplay:YES];
}
- (void)showBackgroundInfo {
    if (hcv_menu_trappe("Bkgnd Info…")) return;   /* la pile détourne l'article */
    Object *card = hc_current_card();
    if (!card || !card->bg) return;
    Object *bg = card->bg;
    gBgTarget = bg;

    gBgPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(320, 280, 340, 200 + 40)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gBgPanel setTitle:@"Background Info"];
    [gBgPanel setReleasedWhenClosed:NO];
    NSView *c = [gBgPanel contentView];

    NSTextField *lb = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 200, 120, 18)];
    [lb setStringValue:@"Background Name:"];
    [lb setBezeled:NO]; [lb setDrawsBackground:NO]; [lb setEditable:NO];
    [c addSubview:lb];

    gBgName = [[NSTextField alloc] initWithFrame:NSMakeRect(140, 198, 184, 22)];
    [gBgName setStringValue:hcv_texte(bg->name)];
    [c addSubview:gBgName];

    // combien de cartes partagent ce fond ?
    int nCards = 0;
    Object *stack = bg->owner;
    if (stack)
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i]->type == OBJ_CARD && stack->parts[i]->bg == bg) nCards++;

    NSTextField *ids = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 136, 308, 52)];
    [ids setStringValue:[NSString stringWithFormat:
        @"Background ID: %d\nCards in this background: %d\nFields: %d",
        bg->id, nCards, bg->nparts]];
    [ids setBezeled:NO]; [ids setDrawsBackground:NO]; [ids setEditable:NO];
    [c addSubview:ids];

    /* Les mêmes verrous que dans l'Info carte, à l'échelle du FOND.
     *
     * « Don't Search » y saute toutes les cartes du fond d'un coup ; « Can't
     * Delete » refuse la disparition du fond, c'est-à-dire la suppression de
     * sa DERNIÈRE carte — rien d'autre ne supprime un fond ici. */
    gBgDontSearch = [[NSButton alloc] initWithFrame:NSMakeRect(16, 92, 250, 20)];
    [gBgDontSearch setButtonType:NSButtonTypeSwitch];
    [gBgDontSearch setTitle:@"Don't Search This Background"];
    [gBgDontSearch setState:bg->dont_search ? NSControlStateValueOn
                                            : NSControlStateValueOff];
    [c addSubview:gBgDontSearch];

    gBgCantDelete = [[NSButton alloc] initWithFrame:NSMakeRect(16, 68, 250, 20)];
    [gBgCantDelete setButtonType:NSButtonTypeSwitch];
    [gBgCantDelete setTitle:@"Can't Delete This Background"];
    [gBgCantDelete setState:bg->cant_delete ? NSControlStateValueOn
                                            : NSControlStateValueOff];
    [c addSubview:gBgCantDelete];

    NSButton *(^mkBG)(NSString*, SEL, CGFloat) = ^NSButton*(NSString *t, SEL a, CGFloat x) {
        NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(x, 16, 88, 28)];
        [b setTitle:t]; [b setBezelStyle:NSBezelStyleRounded];
        [b setTarget:self]; [b setAction:a];
        [c addSubview:b];
        return b;
    };
    mkBG(@"Script…", @selector(bgScript:), 16);
    mkBG(@"Cancel",  @selector(bgCancel:), 148);
    NSButton *ok = mkBG(@"OK", @selector(bgOK:), 240);
    [ok setKeyEquivalent:@"\r"];

    panneau_donne_le_coller(gBgPanel, self, NO);
    [gBgPanel makeKeyAndOrderFront:nil];
}

- (void)bgOK:(id)sender {
    if (gBgTarget) {
        free(gBgTarget->name);
        gBgTarget->name = strdup([[gBgName stringValue] UTF8String]);
        gBgTarget->dont_search =
            ([gBgDontSearch state] == NSControlStateValueOn) ? 1 : 0;
        gBgTarget->cant_delete =
            ([gBgCantDelete state] == NSControlStateValueOn) ? 1 : 0;
    }
    [gBgPanel close];
    gBgTarget = NULL;
    gBgDontSearch = gBgCantDelete = nil;
    [self setNeedsDisplay:YES];
}

- (void)bgCancel:(id)sender {
    [gBgPanel close];
    gBgTarget = NULL;
    gBgDontSearch = gBgCantDelete = nil;
}

- (void)bgScript:(id)sender {
    Object *bg = gBgTarget;
    [self bgOK:sender];
    if (bg) [self editScriptOf:bg];
}
- (void)showStackInfo {
    if (hcv_menu_trappe("Stack Info…")) return;   /* la pile détourne l'article */
    Object *card = hc_current_card();
    if (!card) return;
    Object *stack = card->owner;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) return;
    gStackTarget = stack;

    gStackPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(280, 320, 340, 200)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gStackPanel setTitle:@"Stack Info"];
    [gStackPanel setReleasedWhenClosed:NO];
    NSView *c = [gStackPanel contentView];

    NSTextField *lb = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 160, 90, 18)];
    [lb setStringValue:@"Stack Name:"];
    [lb setBezeled:NO]; [lb setDrawsBackground:NO]; [lb setEditable:NO];
    [c addSubview:lb];

    gStackName = [[NSTextField alloc] initWithFrame:NSMakeRect(110, 158, 214, 22)];
    [gStackName setStringValue:hcv_texte(stack->name)];
    [c addSubview:gStackName];

    int nCards = 0, nBgs = 0;
    for (int i = 0; i < stack->nparts; i++) {
        if (stack->parts[i]->type == OBJ_CARD)       nCards++;
        else if (stack->parts[i]->type == OBJ_BACKGROUND) nBgs++;
    }

    NSTextField *ids = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 96, 308, 52)];
    [ids setStringValue:[NSString stringWithFormat:
        @"Cards: %d\nBackgrounds: %d\nCard size: %d x %d",
        nCards, nBgs, stack->w, stack->h]];
    [ids setBezeled:NO]; [ids setDrawsBackground:NO]; [ids setEditable:NO];
    [c addSubview:ids];

    NSButton *(^mkST)(NSString*, SEL, CGFloat) = ^NSButton*(NSString *t, SEL a, CGFloat x) {
        NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(x, 16, 88, 28)];
        [b setTitle:t]; [b setBezelStyle:NSBezelStyleRounded];
        [b setTarget:self]; [b setAction:a];
        [c addSubview:b];
        return b;
    };
    mkST(@"Script…", @selector(stackScript:), 16);
    mkST(@"Cancel",  @selector(stackCancel:), 148);
    NSButton *ok = mkST(@"OK", @selector(stackOK:), 240);
    [ok setKeyEquivalent:@"\r"];

    panneau_donne_le_coller(gStackPanel, self, NO);
    [gStackPanel makeKeyAndOrderFront:nil];
}

- (void)stackOK:(id)sender {
    if (gStackTarget) {
        free(gStackTarget->name);
        gStackTarget->name = strdup([[gStackName stringValue] UTF8String]);
    }
    [gStackPanel close];
    gStackTarget = NULL;
    [self updateWindowTitle];
    [self setNeedsDisplay:YES];
}

- (void)stackCancel:(id)sender {
    [gStackPanel close];
    gStackTarget = NULL;
}

- (void)stackScript:(id)sender {
    Object *st = gStackTarget;
    [self stackOK:sender];
    if (st) [self editScriptOf:st];
}




- (void)cardCancel:(id)sender {
    [gCardPanel close];
    gCardTarget = NULL;
    gCardMarked = gCardDontSearch = gCardCantDelete = nil;
}

- (void)cardScript:(id)sender {
    Object *cd = gCardTarget;
    [self cardOK:sender];
    if (cd) [self editScriptOf:cd];
}


- (void)infoContents:(id)sender {
    Object *o = gInfoTarget;
    if (!o) return;
    gContentsTarget = o;

    gContentsPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(340, 240, 320, 260)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gContentsPanel setTitle:@"Contents"];
    [gContentsPanel setReleasedWhenClosed:NO];
    NSView *c = [gContentsPanel contentView];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(12, 52, 296, 192)];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSBezelBorder];
    NSTextView *tv = [[NSTextView alloc] initWithFrame:[[scroll contentView] bounds]];
    [tv setFont:[NSFont systemFontOfSize:12]];
    [tv setString:hcv_texte(o->contents ? o->contents : "")];
    [scroll setDocumentView:tv];
    [c addSubview:scroll];
    gContentsView = tv;

    NSButton *cancel = [[NSButton alloc] initWithFrame:NSMakeRect(108, 12, 96, 28)];
    [cancel setTitle:@"Cancel"];
    [cancel setBezelStyle:NSBezelStyleRounded];
    [cancel setTarget:self];
    [cancel setAction:@selector(contentsCancel:)];
    [c addSubview:cancel];

    NSButton *ok = [[NSButton alloc] initWithFrame:NSMakeRect(212, 12, 96, 28)];
    [ok setTitle:@"OK"];
    [ok setBezelStyle:NSBezelStyleRounded];
    [ok setKeyEquivalent:@"\r"];
    [ok setTarget:self];
    [ok setAction:@selector(contentsOK:)];
    [c addSubview:ok];

    panneau_donne_le_coller(gContentsPanel, self, NO);
    [gContentsPanel makeKeyAndOrderFront:nil];
}

/* UN PANNEAU D'INFO NE SURVIT PAS À L'OBJET QU'IL MONTRE.
 *
 * Les sept panneaux d'info s'ouvrent par makeKeyAndOrderFront: — ils ne sont
 * PAS modaux. Pendant qu'ils sont ouverts, on peut fermer la fenêtre de la
 * pile, supprimer l'objet depuis la carte, ou laisser un script le faire. Leur
 * cible était alors un pointeur sur de la mémoire rendue, et « OK » écrivait
 * dedans : free(gCardTarget->name) sur une carte déjà libérée, set_cstr dans
 * un champ qui n'existe plus.
 *
 * C'est le site jumeau de deux corrections déjà faites. cocoa_object_gone
 * efface gFontTarget, la cible du panneau des POLICES, pour exactement cette
 * raison ; et hcicon_panel_stack_closing, juste en dessous, referme le
 * panneau des ICÔNES quand sa pile s'en va. Les panneaux d'info, eux, étaient
 * restés hors des deux listes. Trouvé par un audit, en relisant qui retient
 * un Object* : non mesuré sur la machine, faute de pouvoir lancer AppKit ici.
 *
 * On REFERME plutôt que de vider la cible et laisser le panneau ouvert : des
 * champs qui décrivent un objet mort ne mèneraient nulle part, et le seul
 * geste possible — OK — n'aurait plus rien à faire. Chaque gestionnaire OK
 * teste déjà sa cible avant d'écrire ; la remettre à NULL suffit donc à les
 * désarmer, même si AppKit délivrait un clic déjà en file. */
void hcdlg_objet_disparu(Object *mort)
{
    if (!mort) return;
    if (gInfoTarget     == mort) { [gInfoPanel close];     gInfoTarget     = NULL; }
    if (gFldTarget      == mort) { [gFldPanel close];      gFldTarget      = NULL; }
    if (gStyleTarget    == mort) { [gStylePanel close];    gStyleTarget    = NULL; }
    if (gCardTarget     == mort) { [gCardPanel close];     gCardTarget     = NULL; }
    if (gBgTarget       == mort) { [gBgPanel close];       gBgTarget       = NULL; }
    if (gStackTarget    == mort) { [gStackPanel close];    gStackTarget    = NULL; }
    if (gContentsTarget == mort) { [gContentsPanel close]; gContentsTarget = NULL; }
}

/* Une pile se ferme : le panneau Icônes retient des Object* qui ne doivent pas
 * lui survivre — gIconStack, et la pile que gIconBits garde pour dessiner.
 *
 * On referme donc, plutôt que de tenter de le repointer ailleurs : le panneau
 * montre les ressources d'UNE pile, et celle-ci n'existe plus. Passer NULL
 * ferme quelle que soit la pile.
 *
 * gInfoTarget n'est pas touché : le panneau étant fermé, iconOK: ne peut plus
 * s'exécuter, et l'info du bouton peut légitimement être ouverte par ailleurs. */
void hcicon_panel_stack_closing(Object *stack)
{
    if (!gIconPanel) return;
    if (stack && gIconStack != stack) return;

    [gIconPanel close];
    gIconPanel  = nil;
    gIconGrid   = nil;
    gIconLabel  = nil;
    gIconBits   = nil;
    gIconName   = nil;
    gIconInfo   = nil;
    gIconStack  = NULL;
    gIconNameId = 0;
}

/* « Édition › Icône… » — la deuxième porte d'entrée du panneau Icônes.
 *
 * Toujours disponible, y compris sans sélection : le panneau gère les icônes
 * de la pile, qui sont des ressources. Sans bouton pour cible, on peut créer,
 * dessiner et renommer ; OK referme alors sans rien attribuer.
 *
 * gInfoTarget n'est normalement posé que par l'info du bouton : on le pose ici
 * sur l'objet sélectionné s'il s'agit d'un bouton, à NULL sinon.
 * gInfoIconField est remis à nil parce que le panneau d'info n'est pas ouvert —
 * iconOK: y écrit, et le laisser pointer sur le champ d'un panneau refermé
 * reviendrait à peindre dans le vide. */
- (void)editIcon:(id)sender {
    if (hcv_menu_trappe("Icon…")) return;   /* la pile détourne l'article */
    Object *sel = hcv_selection();
    gInfoTarget = (sel && sel->type == OBJ_BUTTON) ? sel : NULL;
    gInfoIconField = nil;
    gInfoFamily    = nil;   /* même raison : infoOK: y écrirait dans le vide */
    [self infoIcon:sender];
}

- (void)infoIcon:(id)sender {
    (void)sender;
    /* gInfoTarget peut être NULL : le panneau sert alors uniquement à gérer les
     * icônes de la pile, sans en attribuer aucune. */
    Object *o = gInfoTarget;

    /* La pile porte le catalogue : on la retrouve par la carte courante. */
    Object *card = hc_current_card();
    gIconStack = (card && card->owner) ? card->owner : NULL;
    hcicon_edit_sync(gIconStack);

    CGFloat gw   = ICONGRID_COLS * ICONGRID_CELL;
    CGFloat gh   = [IconGrid heightForCount:hcicon_catalog_count()];
    CGFloat bits = [HCFatBits side];

    /* Deux colonnes : le catalogue a gauche, l'edition a droite, hauts
     * alignes. Le panneau n'est pas retourne — les ordonnees partent du bas. */
    const CGFloat LX = 12, RX = LX + gw + 16 + 16;
    const CGFloat W  = RX + bits + 12;
    /* 452 et non 420 : la seconde rangée de boutons descend jusqu'à y=64, et
     * la rangée du bas occupe 20..48. Trente-deux points de plus les séparent.
     *
     * Et +72 pour la couleur : la case à cocher sur une ligne, la bande de
     * palette sur deux rangées de 22. Tout ce qui est SOUS la grille descend
     * d'autant — c'est pourquoi le décalage est nommé plutôt que recopié à
     * chaque ordonnée : un oubli mettrait deux vues l'une sur l'autre. */
    const CGFloat COUL = 24 + [HCIconPalette height] + 8;
    const CGFloat H  = 452 + COUL;
    const CGFloat TOP = H - 16;              /* haut commun aux deux colonnes */

    gIconPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(360, 200, W, H)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gIconPanel setTitle:@"Icônes"];
    [gIconPanel setReleasedWhenClosed:NO];
    NSView *c = [gIconPanel contentView];

    /* ---- colonne de gauche : le catalogue ---- */
    NSScrollView *scroll = [[NSScrollView alloc]
        initWithFrame:NSMakeRect(LX, TOP - 288, gw + 16, 288)];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSBezelBorder];

    gIconGrid = [[IconGrid alloc] initWithFrame:NSMakeRect(0, 0, gw, gh)];
    gIconGrid.selected = o ? o->icon : 0;
    gIconGrid.target   = self;
    gIconGrid.action   = @selector(iconPicked:);
    [scroll setDocumentView:gIconGrid];
    [c addSubview:scroll];

    gIconLabel = [[NSTextField alloc]
        initWithFrame:NSMakeRect(LX, TOP - 288 - 24, gw, 18)];
    [gIconLabel setBezeled:NO]; [gIconLabel setDrawsBackground:NO];
    [gIconLabel setEditable:NO];
    [c addSubview:gIconLabel];

    /* ---- colonne de droite : l'edition ---- */
    gIconBits = [[HCFatBits alloc]
        initWithFrame:NSMakeRect(RX, TOP - bits, bits, bits)];
    gIconBits.iconId = o ? o->icon : 0;
    gIconBits.stack  = gIconStack;
    gIconBits.target = self;
    gIconBits.action = @selector(iconEdited:);
    [c addSubview:gIconBits];

    /* ---- la couleur, entre la grille et le nom ---- */
    gIconCoul = [[NSButton alloc]
        initWithFrame:NSMakeRect(RX, TOP - bits - 22, bits, 20)];
    [gIconCoul setButtonType:NSButtonTypeSwitch];
    [gIconCoul setTitle:@"Couleur"];
    [gIconCoul setFont:[NSFont systemFontOfSize:11]];
    [gIconCoul setTarget:self];
    [gIconCoul setAction:@selector(iconCouleur:)];
    [c addSubview:gIconCoul];

    gIconPal = [[HCIconPalette alloc]
        initWithFrame:NSMakeRect(RX, TOP - bits - 22 - [HCIconPalette height] - 2,
                                 bits, [HCIconPalette height])];
    gIconPal.stack  = gIconStack;
    gIconPal.iconId = o ? o->icon : 0;
    gIconPal.grille = gIconBits;
    gIconPal.target = self;
    gIconPal.action = @selector(iconEdited:);
    [c addSubview:gIconPal];

    gIconName = [[NSTextField alloc]
        initWithFrame:NSMakeRect(RX, TOP - bits - 30 - COUL, bits, 22)];
    [gIconName setTarget:self];
    [gIconName setAction:@selector(iconRename:)];
    [c addSubview:gIconName];

    gIconInfo = [[NSTextField alloc]
        initWithFrame:NSMakeRect(RX, TOP - bits - 52 - COUL, bits, 18)];
    [gIconInfo setBezeled:NO]; [gIconInfo setDrawsBackground:NO];
    [gIconInfo setEditable:NO];
    [c addSubview:gIconInfo];

    NSButton *(^mkEB)(NSString*, SEL, CGFloat, int) =
        ^NSButton*(NSString *t, SEL a, CGFloat x, int rangee) {
        NSButton *b = [[NSButton alloc]
            initWithFrame:NSMakeRect(x, TOP - bits - 84 - COUL - rangee * 32, 62, 26)];
        [b setTitle:t];
        [b setBezelStyle:NSBezelStyleRounded];
        [b setFont:[NSFont systemFontOfSize:10]];
        [b setTarget:self];
        [b setAction:a];
        [c addSubview:b];
        return b;
    };
    mkEB(@"Nouvelle", @selector(iconNew:),       RX,       0);
    mkEB(@"Dupliquer",@selector(iconDuplicate:), RX + 64,  0);
    mkEB(@"Effacer",  @selector(iconErase:),     RX + 128, 0);
    mkEB(@"Supprimer",@selector(iconDelete:),    RX + 192, 0);
    mkEB(@"Pivoter",  @selector(iconRotate:),    RX,       1);
    mkEB(@"Coller",   @selector(iconColler:),    RX + 64,  1);

    /* ---- rangee du bas, commune ---- */
    NSButton *(^mkIB)(NSString*, SEL, CGFloat) = ^NSButton*(NSString *t, SEL a, CGFloat x) {
        NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(x, 12, 76, 28)];
        [b setTitle:t];
        [b setBezelStyle:NSBezelStyleRounded];
        [b setTarget:self];
        [b setAction:a];
        [c addSubview:b];
        return b;
    };
    /* Sans bouton pour cible, « Aucune » et « OK » n'ont rien à attribuer :
     * OK ne fait alors que refermer. Les icônes créées ou dessinées, elles,
     * restent dans la pile — ce sont des ressources. */
    mkIB(@"Aucune", @selector(iconNone:),   LX);
    mkIB(@"Cancel", @selector(iconCancel:), W - 172);
    NSButton *ok = mkIB(@"OK", @selector(iconOK:), W - 88);
    [ok setKeyEquivalent:@"\r"];

    [self iconRefresh];
    panneau_donne_le_coller(gIconPanel, self, YES);
    [gIconPanel makeKeyAndOrderFront:nil];
}

/* Valide le contenu du champ Nom sur l'icône qu'il nomme.
 *
 * À appeler AVANT tout ce qui change la sélection. Le champ n'envoie son action
 * qu'à la touche Entrée ; cliquer la grille ou un bouton ne la déclenche pas, et
 * IconGrid n'acceptant pas le premier répondant, le champ ne perd même pas le
 * focus — controlTextDidEndEditing: ne servirait donc à rien ici. */
- (void)iconCommitName {
    if (!gIconName || gIconNameId == 0) return;
    if (![gIconName isEditable]) return;          /* icône d'origine */

    struct StackIcon *own = hc_icon_get(gIconStack, gIconNameId);
    if (!own) return;

    const char *typed = [[gIconName stringValue] UTF8String];
    if (!typed) return;
    if (own->name && strcmp(own->name, typed) == 0) return;   /* rien n'a changé */

    hcicon_edit_rename(gIconStack, gIconNameId, typed);
}

/* Seul endroit qui remet les deux colonnes d'accord. Toutes les actions y
 * passent : c'est ce qui evite qu'une d'elles oublie un morceau. */
- (void)iconRefresh {
    int id = gIconGrid ? gIconGrid.selected : 0;
    const HCIcon     *ic  = hcicon_find(id);
    struct StackIcon *own = hc_icon_get(gIconStack, id);

    gIconBits.iconId = id;
    gIconBits.stack  = gIconStack;

    [gIconName setStringValue:hcv_texte(ic ? ic->name : NULL)];
    /* Une icone d'origine ne se renomme pas : elle est const. Elle le devient
     * des qu'on la dessine, hcicon_edit_editable la recopiant dans la pile. */
    [gIconName setEditable:(own != NULL)];
    gIconNameId = id;

    if (id == 0)
        [gIconInfo setStringValue:@"Aucune icône"];
    else
        [gIconInfo setStringValue:[NSString stringWithFormat:@"N° %d — %@",
            id, own ? @"pile" : @"d'origine"]];

    [gIconLabel setStringValue:
        [NSString stringWithFormat:@"%d icônes", hcicon_catalog_count()]];

    /* ---- la couleur ----
     * La bande suit l'icône choisie, et la case à cocher dit son état. Les
     * deux passent par ICI et nulle part ailleurs : c'est ce qui évite qu'une
     * action oublie d'en remettre une à jour et qu'on peigne dans la palette
     * de l'icône précédente. */
    gIconPal.iconId = id;
    gIconPal.stack  = gIconStack;
    gIconPal.grille = gIconBits;

    int en_couleur = hcicon_edit_est_couleur(gIconStack, id);
    [gIconCoul setState:en_couleur ? NSControlStateValueOn : NSControlStateValueOff];
    /* Une icône d'origine n'est pas encore dans la pile : cocher la case la
     * recopiera, donc la case reste active. Sans icône du tout, elle ne veut
     * rien dire. */
    [gIconCoul setEnabled:(id != 0)];

    /* LA COULEUR CHOISIE REVIENT À UN INDEX VALIDE quand on change d'icône :
     * la palette de la suivante est plus courte, et garder l'index d'avant
     * ferait peindre avec une couleur qui n'existe pas — donc en « ? », le
     * caractère qui ne doit jamais apparaître. */
    if (en_couleur) {
        struct HcIconCouleur *cc = own ? hc_icon_couleur(own) : NULL;
        if (!cc || gIconBits.couleurCourante >= cc->ncouleurs ||
            gIconBits.couleurCourante < 0)
            gIconBits.couleurCourante = (cc && cc->ncouleurs > 1) ? 1 : 0;
    }

    [gIconGrid reload];
    [gIconBits setNeedsDisplay:YES];
    [gIconPal  setNeedsDisplay:YES];
    [self setNeedsDisplay:YES];
}

/* La case « Couleur ». Allumer ne perd pas le dessin — chaque pixel d'encre
 * devient l'index 1, noir —, éteindre garde la silhouette. Les deux gestes
 * sont donc réversibles tant qu'on n'a pas repeint, ce qui permet d'essayer
 * sans rien risquer. */
- (void)iconCouleur:(id)sender {
    (void)sender;
    int id = gIconGrid ? gIconGrid.selected : 0;
    if (id == 0) return;
    hcicon_edit_couleur(gIconStack, id,
                        [gIconCoul state] == NSControlStateValueOn);
    [self iconRefresh];
}

- (void)iconSelect:(int)id {
    gIconGrid.selected = id;
    [self iconRefresh];
}

/* iconPicked: arrive APRÈS qu'IconGrid a changé sa sélection : la validation
 * du nom s'appuie donc sur gIconNameId, qui désigne encore l'ancienne. */
- (void)iconPicked:(id)sender { (void)sender; [self iconCommitName]; [self iconRefresh]; }
- (void)iconEdited:(id)sender { (void)sender; [self iconRefresh]; }

- (void)iconNew:(id)sender {
    (void)sender;
    [self iconCommitName];
    int id = hcicon_edit_new(gIconStack);
    if (id) [self iconSelect:id];
}

- (void)iconDuplicate:(id)sender {
    (void)sender;
    [self iconCommitName];
    int id = hcicon_edit_duplicate(gIconStack, gIconGrid.selected);
    if (id) [self iconSelect:id];
}

/* Quart de tour horaire. Quatre clics ramènent au point de départ : une grille
 * carrée tourne sans qu'aucun pixel ne sorte, donc sans perte. */
- (void)iconRotate:(id)sender {
    (void)sender;
    [self iconCommitName];
    hcicon_edit_rotate(gIconStack, gIconGrid.selected);
    [self iconRefresh];
}

/* Coller une image du presse-papiers dans l'icône.
 *
 * On ne se demande pas s'il y a une image : hcicon_edit_colle rend 0 quand il
 * n'y en a pas, et l'on dit alors pourquoi. Un bouton qui ne fait rien sans
 * expliquer est plus agaçant qu'un bouton grisé — mais on ne peut pas le
 * griser à coup sûr, le presse-papiers changeant sous nos pieds. */
- (void)iconColler:(id)sender {
    (void)sender;
    [self iconCommitName];
    int id = gIconGrid ? gIconGrid.selected : 0;
    if (id == 0) {
        /* Ne rien faire SANS RIEN DIRE était le second défaut du même relevé :
         * le panneau s'ouvre sans sélection dès qu'on vient du menu Édition
         * plutôt que de l'info d'un bouton, et le bouton « Coller » semblait
         * alors cassé — le même symptôme que le collage qui partait ailleurs,
         * et c'est pourquoi les deux se corrigent ensemble. */
        [gIconInfo setStringValue:@"Choisissez une icône, ou « Nouvelle »."];
        return;
    }

    int n = hcicon_edit_colle(gIconStack, id);
    [self iconRefresh];

    if (n == 0) {
        [gIconInfo setStringValue:@"Le presse-papiers ne contient pas d'image."];
        return;
    }
    /* Le compte de couleurs DIT s'il y a eu perte : 255 tout rond veut dire
     * que la découpe médiane a travaillé, moins veut dire que les couleurs
     * tenaient et que le collage est exact. C'est une information qu'on ne
     * peut pas lire sur le dessin. */
    [gIconInfo setStringValue:
        [NSString stringWithFormat:@"Collé — %d couleur%@%@", n - 1,
            (n - 1) > 1 ? @"s" : @"",
            (n >= HC_ICON_COULEURS_MAX) ? @" (image réduite)" : @" (exact)"]];
}

- (void)iconErase:(id)sender {
    (void)sender;
    [self iconCommitName];
    hcicon_edit_erase(gIconStack, gIconGrid.selected);
    [self iconRefresh];
}

/* Supprimer ne rend pas leur icone aux boutons qui la portent : ils gardent un
 * numero mort et n'affichent plus rien, comme dans HyperCard. On l'annonce
 * plutot que de laisser la surprise pour plus tard. */
- (void)iconDelete:(id)sender {
    (void)sender;
    int id = gIconGrid.selected;
    if (!hc_icon_get(gIconStack, id)) { NSBeep(); return; }

    gIconNameId = 0;          /* on supprime : rien à valider */
    int users = hcicon_edit_users(gIconStack, id);
    if (users > 0) {
        NSAlert *a = [[NSAlert alloc] init];
        [a setMessageText:[NSString stringWithFormat:
            @"%d bouton%@ utilise%@ encore cette icône.",
            users, users > 1 ? @"s" : @"", users > 1 ? @"nt" : @""]];
        [a setInformativeText:@"Ils garderont son numéro et n'afficheront plus rien."];
        [a addButtonWithTitle:@"Supprimer"];
        [a addButtonWithTitle:@"Annuler"];
        if ([a runModal] != NSAlertFirstButtonReturn) return;
    }

    hcicon_edit_delete(gIconStack, id);
    [self iconSelect:0];
}

/* Touche Entrée dans le champ. Le gros du travail est dans iconCommitName,
 * qui sert aussi à tous les autres chemins de sortie du champ. */
- (void)iconRename:(id)sender {
    (void)sender;
    [self iconCommitName];
    [self iconRefresh];
}

- (void)iconOK:(id)sender {
    (void)sender;
    [self iconCommitName];
    if (gInfoTarget && gIconGrid) {
        gInfoTarget->icon = gIconGrid.selected;
        /* nil quand on vient du menu Édition plutôt que de l'info du bouton. */
        if (gInfoIconField)
            [gInfoIconField setStringValue:
                [NSString stringWithFormat:@"%d", gIconGrid.selected]];
    }
    [gIconPanel close];
    [self setNeedsDisplay:YES];
}

- (void)iconNone:(id)sender {
    (void)sender;
    [self iconSelect:0];
}

/* Cancel ne renonce qu'a l'ATTRIBUTION. Les icones creees ou dessinees restent
 * dans la pile : ce sont des ressources, pas une propriete du bouton, et les
 * defaire supposerait un historique que le noyau n'a pas. */
- (void)iconCancel:(id)sender {
    (void)sender;
    /* Le nom se valide même ici : Cancel ne renonce qu'à l'attribution, et un
     * nom fraîchement tapé fait partie de l'icône, pas du bouton. */
    [self iconCommitName];
    [gIconPanel close];
}
 



- (void)infoOK:(id)sender {
    Object *o = gInfoTarget;
        if (o) {
            // nom
            set_cstr(&o->name,  [gInfoName stringValue]);
            // style
            set_cstr(&o->style, [gInfoStyle titleOfSelectedItem]);
            /* Un bouton qui devient polygone sans sommets naît triangle :
             * la même fonction que « set the style … to polygon ». */
            hc_polygone_par_defaut(o);
            o->textsize = [[gInfoTextSize stringValue] intValue];
            o->showname   = ([gInfoShowName state]   == NSControlStateValueOn);
            o->autohilite = ([gInfoAutoHilite state] == NSControlStateValueOn);
            o->enabled    = ([gInfoEnabled state]    == NSControlStateValueOn);
            o->shared_hilite = ([gInfoSharedHilite state] == NSControlStateValueOn);
            o->icon = [[gInfoIconField stringValue] intValue];
            /* Le rang EST le numéro : index 0 = None = pas de famille.
             *
             * On passe par hc_set_family plutôt que d'écrire o->family :
             * entrer dans une famille en étant allumé doit éteindre les
             * autres, sinon le groupe aurait deux boutons allumés — un état
             * qu'aucun clic ne peut produire. Écrire le champ en direct
             * aurait donné exactement cette incohérence, visible seulement
             * à la fermeture du panneau. */
            if (gInfoFamily)
                hc_set_family(o, (int)[gInfoFamily indexOfSelectedItem]);
            /* Les couleurs : case décochée, rien de posé. */
            int *coul[3]  = { &o->backcolor, &o->forecolor, &o->hilitecolor };
            int *alpha[3] = { &o->backalpha, &o->forealpha, &o->hilitealpha };
            for (int i = 0; i < 3; i++) {
                if (!gInfoCoulCase[i] || !gInfoCoulPuits[i]) continue;
                if ([gInfoCoulCase[i] state] == NSControlStateValueOn)
                    info_couleur_vers([gInfoCoulPuits[i] color], coul[i], alpha[i]);
                else
                    *coul[i] = *alpha[i] = 0;
            }
        }
    info_couleurs_ferme();
    [gInfoPanel close];
    close_style_panel();
    gInfoTarget = NULL;
    gFontTarget = NULL;   /* comme fldOK: : sinon le panneau des polices reste
                             braque sur ce bouton et toute modification de
                             police suivante lui revient. */
    [self setNeedsDisplay:YES];
}

/* Choisir une couleur dans un puits, c'est vouloir s'en servir : la case
 * se coche. */
- (void)infoCouleurChoisie:(id)sender {
    NSInteger i = [sender tag];
    if (i >= 0 && i < 3 && gInfoCoulCase[i])
        [gInfoCoulCase[i] setState:NSControlStateValueOn];
}

- (void)infoCancel:(id)sender {
    info_couleurs_ferme();
    [gInfoPanel close];
    close_style_panel();
    gInfoTarget = NULL;
    gFontTarget = NULL;
}

- (void)infoScript:(id)sender {
    Object *o = gInfoTarget;
    [self infoOK:sender];        // valider les changements avant
    if (o) [self editScriptOf:o];
}

@end
