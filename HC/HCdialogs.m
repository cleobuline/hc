//
//  HCdialogs.m — boites de dialogue Info (bouton, carte, fond, pile, icones, contenu)
//

#import "HCview.h"
#import "HCglobals.h"
#import "HCpalettes.h"
#import "HCicons.h"
#import "Hciconedit.h"
#import "graphics.h"

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
- (void)styleOK:(id)sender;
- (void)styleFont:(id)sender;
- (void)autoSelectToggled:(id)sender;
@end

/* --- etat des dialogues, prive a ce fichier --- */
static NSPanel      *gInfoPanel = nil;
static Object       *gInfoTarget = NULL;
static NSTextField  *gInfoName = nil;
static NSPopUpButton *gInfoStyle = nil;
static NSButton     *gInfoShowName = nil;
static NSButton     *gInfoAutoHilite = nil;
static NSTextField  *gInfoIconField = nil;
static NSTextField  *gInfoTextSize = nil;

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

    if (gStylePanel) [gStylePanel close];
    gStylePanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(340, 300, 240, 300)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gStylePanel setTitle:@"Text Style"];
    [gStylePanel setReleasedWhenClosed:NO];
    NSView *c = [gStylePanel contentView];

    /* --- police --- */
    NSTextField *fl = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 262, 70, 18)];
    [fl setStringValue:@"Text font:"];
    [fl setBezeled:NO]; [fl setDrawsBackground:NO]; [fl setEditable:NO];
    [c addSubview:fl];

    gStyleFont = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(90, 258, 134, 24)];
    [gStyleFont addItemsWithTitles:
        [[[NSFontManager sharedFontManager] availableFontFamilies]
            sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]];

    /* Sélectionner la police courante sans tenir compte de la casse : le
     * noyau stocke ce que le script a écrit (« monaco »), la liste système
     * porte le nom canonique (« Monaco »), et selectItemWithTitle: compare à
     * la lettre près. Même piège que select_style plus haut. */
    if (o->textfont && *o->textfont) {
        NSString *want = [NSString stringWithUTF8String:o->textfont];
        for (NSMenuItem *it in [gStyleFont itemArray])
            if ([[it title] caseInsensitiveCompare:want] == NSOrderedSame) {
                [gStyleFont selectItem:it];
                break;
            }
    }
    [c addSubview:gStyleFont];

    /* --- corps --- */
    NSTextField *zl = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 234, 70, 18)];
    [zl setStringValue:@"Text size:"];
    [zl setBezeled:NO]; [zl setDrawsBackground:NO]; [zl setEditable:NO];
    [c addSubview:zl];

    gStyleSize = [[NSTextField alloc] initWithFrame:NSMakeRect(90, 232, 60, 22)];
    [gStyleSize setIntValue:o->textsize];
    [c addSubview:gStyleSize];

    for (int i = 0; i < 8; i++) {
        gStyleBox[i] = [[NSButton alloc]
            initWithFrame:NSMakeRect(20, 202 - i * 22, 150, 20)];
        [gStyleBox[i] setButtonType:NSButtonTypeSwitch];
        [gStyleBox[i] setTitle:[NSString stringWithUTF8String:STYLE_LABELS[i]]];
        [gStyleBox[i] setState:(o->textstyle & (1 << i)) ? NSControlStateValueOn
                                                         : NSControlStateValueOff];
        [c addSubview:gStyleBox[i]];
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

    [gStylePanel makeKeyAndOrderFront:nil];
}

 
static NSPanel     *gBgPanel = nil;
static Object      *gBgTarget = NULL;
static NSTextField *gBgName = nil;

static NSPanel     *gCardPanel = nil;
static Object      *gCardTarget = NULL;
static NSTextField *gCardName = nil;

static NSPanel     *gStackPanel = nil;

static NSPanel     *gIconPanel = nil;
static IconGrid    *gIconGrid = nil;
static NSTextField *gIconLabel = nil;
/* moitie edition du panneau Icones */
static HCFatBits   *gIconBits = nil;
static NSTextField *gIconName = nil;
/* Quelle icône le champ Nom est en train de nommer. Indispensable : quand on
 * clique une autre icône, la sélection a déjà changé au moment où l'on valide,
 * et sans ce témoin on renommerait la nouvelle avec le texte de l'ancienne. */
static int          gIconNameId = 0;
static NSTextField *gIconInfo = nil;
static Object      *gIconStack = NULL;

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
    NSString *want = [NSString stringWithUTF8String:style && *style ? style : "rectangle"];
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
    [gFldName setStringValue:[NSString stringWithUTF8String:obj->name ? obj->name : ""]];
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

- (void)styleFont:(id)sender {
    if (!gStyleTarget) return;
    commit_style_panel();          /* voir le commentaire de commit_style_panel */

    gFontTarget = gStyleTarget;
    CGFloat sz = gStyleTarget->textsize > 0 ? gStyleTarget->textsize : 12;
    NSFont *f = nil;
    if (gStyleTarget->textfont && *gStyleTarget->textfont) {
        NSString *nm = [NSString stringWithUTF8String:gStyleTarget->textfont];
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
        o->shared_text  = ([gFldShared state]   == NSControlStateValueOn);
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
- (void)showButtonInfo:(Object *)obj {
    if (!obj) return;
    gInfoTarget = obj;

    if (gInfoPanel) [gInfoPanel close];
    gInfoPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(300, 260, 360, 300)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gInfoPanel setTitle:@"Button Info"];
    [gInfoPanel setReleasedWhenClosed:NO];
    NSView *c = [gInfoPanel contentView];

    // --- nom ---
    NSTextField *lb = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 262, 90, 18)];
    [lb setStringValue:@"Button Name:"];
    [lb setBezeled:NO]; [lb setDrawsBackground:NO]; [lb setEditable:NO];
    [c addSubview:lb];

    gInfoName = [[NSTextField alloc] initWithFrame:NSMakeRect(110, 260, 232, 22)];
    [gInfoName setStringValue:[NSString stringWithUTF8String:obj->name ? obj->name : ""]];
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
                                         @"oval", @"popup"]];
    
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

    // --- icone (par identifiant, en attendant le selecteur) ---
    NSTextField *il = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 156, 40, 18)];
    [il setStringValue:@"Icon:"];
    [il setBezeled:NO]; [il setDrawsBackground:NO]; [il setEditable:NO];
    [c addSubview:il];

    gInfoIconField = [[NSTextField alloc] initWithFrame:NSMakeRect(56, 154, 70, 22)];
    [gInfoIconField setStringValue:[NSString stringWithFormat:@"%d", obj->icon]];
    [c addSubview:gInfoIconField];

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
    Object *card = hc_current_card();
    if (!card) return;
    gCardTarget = card;

    gCardPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(300, 300, 340, 200)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gCardPanel setTitle:@"Card Info"];
    [gCardPanel setReleasedWhenClosed:NO];
    NSView *c = [gCardPanel contentView];

    NSTextField *lb = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 160, 90, 18)];
    [lb setStringValue:@"Card Name:"];
    [lb setBezeled:NO]; [lb setDrawsBackground:NO]; [lb setEditable:NO];
    [c addSubview:lb];

    gCardName = [[NSTextField alloc] initWithFrame:NSMakeRect(110, 158, 214, 22)];
    [gCardName setStringValue:[NSString stringWithUTF8String:card->name ? card->name : ""]];
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

    NSTextField *ids = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 96, 308, 52)];
    /* « Card fields » affichait card->nparts, c'est-à-dire boutons compris.
     * On compte les champs, et eux seuls — et ceux de la carte, sans y
     * ajouter ceux du fond, puisque « card field N » les numérote à part. */
    [ids setStringValue:[NSString stringWithFormat:
        @"Card number: %d out of %d\nCard ID: %d\nCard fields: %d",
        rang, total, card->id, hc_part_count(card, OBJ_FIELD)]];
    [ids setBezeled:NO]; [ids setDrawsBackground:NO]; [ids setEditable:NO];
    [c addSubview:ids];

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

    [gCardPanel makeKeyAndOrderFront:nil];
}
- (void)cardOK:(id)sender {
    if (gCardTarget) {
        free(gCardTarget->name);
        gCardTarget->name = strdup([[gCardName stringValue] UTF8String]);
    }
    [gCardPanel close];
    gCardTarget = NULL;
    [self setNeedsDisplay:YES];
}
- (void)showBackgroundInfo {
    Object *card = hc_current_card();
    if (!card || !card->bg) return;
    Object *bg = card->bg;
    gBgTarget = bg;

    gBgPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(320, 280, 340, 200)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gBgPanel setTitle:@"Background Info"];
    [gBgPanel setReleasedWhenClosed:NO];
    NSView *c = [gBgPanel contentView];

    NSTextField *lb = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 160, 120, 18)];
    [lb setStringValue:@"Background Name:"];
    [lb setBezeled:NO]; [lb setDrawsBackground:NO]; [lb setEditable:NO];
    [c addSubview:lb];

    gBgName = [[NSTextField alloc] initWithFrame:NSMakeRect(140, 158, 184, 22)];
    [gBgName setStringValue:[NSString stringWithUTF8String:bg->name ? bg->name : ""]];
    [c addSubview:gBgName];

    // combien de cartes partagent ce fond ?
    int nCards = 0;
    Object *stack = bg->owner;
    if (stack)
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i]->type == OBJ_CARD && stack->parts[i]->bg == bg) nCards++;

    NSTextField *ids = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 96, 308, 52)];
    [ids setStringValue:[NSString stringWithFormat:
        @"Background ID: %d\nCards in this background: %d\nFields: %d",
        bg->id, nCards, bg->nparts]];
    [ids setBezeled:NO]; [ids setDrawsBackground:NO]; [ids setEditable:NO];
    [c addSubview:ids];

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

    [gBgPanel makeKeyAndOrderFront:nil];
}

- (void)bgOK:(id)sender {
    if (gBgTarget) {
        free(gBgTarget->name);
        gBgTarget->name = strdup([[gBgName stringValue] UTF8String]);
    }
    [gBgPanel close];
    gBgTarget = NULL;
    [self setNeedsDisplay:YES];
}

- (void)bgCancel:(id)sender {
    [gBgPanel close];
    gBgTarget = NULL;
}

- (void)bgScript:(id)sender {
    Object *bg = gBgTarget;
    [self bgOK:sender];
    if (bg) [self editScriptOf:bg];
}
- (void)showStackInfo {
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
    [gStackName setStringValue:[NSString stringWithUTF8String:stack->name ? stack->name : ""]];
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
    [tv setString:[NSString stringWithUTF8String:o->contents ? o->contents : ""]];
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

    [gContentsPanel makeKeyAndOrderFront:nil];
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
    gInfoTarget = (gSelected && gSelected->type == OBJ_BUTTON) ? gSelected : NULL;
    gInfoIconField = nil;
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
     * la rangée du bas occupe 20..48. Trente-deux points de plus les séparent. */
    const CGFloat H  = 452;
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

    gIconName = [[NSTextField alloc]
        initWithFrame:NSMakeRect(RX, TOP - bits - 30, bits, 22)];
    [gIconName setTarget:self];
    [gIconName setAction:@selector(iconRename:)];
    [c addSubview:gIconName];

    gIconInfo = [[NSTextField alloc]
        initWithFrame:NSMakeRect(RX, TOP - bits - 52, bits, 18)];
    [gIconInfo setBezeled:NO]; [gIconInfo setDrawsBackground:NO];
    [gIconInfo setEditable:NO];
    [c addSubview:gIconInfo];

    NSButton *(^mkEB)(NSString*, SEL, CGFloat, int) =
        ^NSButton*(NSString *t, SEL a, CGFloat x, int rangee) {
        NSButton *b = [[NSButton alloc]
            initWithFrame:NSMakeRect(x, TOP - bits - 84 - rangee * 32, 62, 26)];
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

    [gIconName setStringValue:
        (ic && ic->name) ? [NSString stringWithUTF8String:ic->name] : @""];
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

    [gIconGrid reload];
    [gIconBits setNeedsDisplay:YES];
    [self setNeedsDisplay:YES];
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
            o->textsize = [[gInfoTextSize stringValue] intValue];
            o->showname   = ([gInfoShowName state]   == NSControlStateValueOn);
            o->autohilite = ([gInfoAutoHilite state] == NSControlStateValueOn);
            o->icon = [[gInfoIconField stringValue] intValue];
        }
    [gInfoPanel close];
    close_style_panel();
    gInfoTarget = NULL;
    gFontTarget = NULL;   /* comme fldOK: : sinon le panneau des polices reste
                             braque sur ce bouton et toute modification de
                             police suivante lui revient. */
    [self setNeedsDisplay:YES];
}

- (void)infoCancel:(id)sender {
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
