//
//  HCdialogs.m — boites de dialogue Info (bouton, carte, fond, pile, icones, contenu)
//

#import "HCview.h"
#import "HCglobals.h"
#import "HCpalettes.h"
#import "HCicons.h"
#import "graphics.h"

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
static NSTextField  *gFldTextSize = nil;
//extern Object *gFontTarget;

//Object *gFontTarget = NULL;    // objet vise par le panneau des polices

- (void)showFieldInfo:(Object *)obj {
    if (!obj) return;
    gFldTarget = obj;

    if (gFldPanel) [gFldPanel close];
    gFldPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(300, 240, 380, 320)
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
    gFldLock     = mkChk(@"Lock Text",         obj->locktext,     220);
    gFldWide     = mkChk(@"Wide Margins",      obj->wide_margins, 198);
    gFldFixed    = mkChk(@"Fixed Line Height", obj->fixed_lh,     176);
    gFldLines    = mkChk(@"Show Lines",        obj->show_lines,   154);
    gFldTab      = mkChk(@"Auto Tab",          obj->auto_tab,     132);
    gFldNoSearch = mkChk(@"Don't Search",      obj->dont_search,  110);
    gFldShared   = mkChk(@"Shared Text",       obj->shared_text,   88);

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
    mkFI(@"Cancel",  @selector(fldCancel:), 150, 16);
    NSButton *ok = mkFI(@"OK", @selector(fldOK:), 254, 16);
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
- (void)styleFont:(id)sender {
    if (!gStyleTarget) return;
    commit_style_panel();          /* voir le commentaire de commit_style_panel */

    gFontTarget = gStyleTarget;
    CGFloat sz = gStyleTarget->textsize > 0 ? gStyleTarget->textsize : 12;
    NSFont *f = nil;
    if (gStyleTarget->textfont && *gStyleTarget->textfont)
        f = [NSFont fontWithName:
                [NSString stringWithUTF8String:gStyleTarget->textfont] size:sz];
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

- (void)infoIcon:(id)sender {
    Object *o = gInfoTarget;
    if (!o) return;

    int rows = (NUM_HCICONS + ICONGRID_COLS - 1) / ICONGRID_COLS;
    CGFloat gw = ICONGRID_COLS * ICONGRID_CELL;
    CGFloat gh = rows * ICONGRID_CELL;

    gIconPanel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(360, 200, gw + 34, 380)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    [gIconPanel setTitle:@"Icons"];
    [gIconPanel setReleasedWhenClosed:NO];
    NSView *c = [gIconPanel contentView];

    NSScrollView *scroll = [[NSScrollView alloc]
        initWithFrame:NSMakeRect(12, 76, gw + 16, 288)];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSBezelBorder];

    gIconGrid = [[IconGrid alloc] initWithFrame:NSMakeRect(0, 0, gw, gh)];
    gIconGrid.selected = o->icon;
    [scroll setDocumentView:gIconGrid];
    [c addSubview:scroll];

    gIconLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(12, 50, gw, 18)];
    [gIconLabel setBezeled:NO]; [gIconLabel setDrawsBackground:NO];
    [gIconLabel setEditable:NO];
    [c addSubview:gIconLabel];

    NSButton *(^mkIB)(NSString*, SEL, CGFloat) = ^NSButton*(NSString *t, SEL a, CGFloat x) {
            NSButton *b = [[NSButton alloc] initWithFrame:NSMakeRect(x, 12, 76, 28)];
            [b setTitle:t];
            [b setBezelStyle:NSBezelStyleRounded];
            [b setTarget:self];
            [b setAction:a];
            [c addSubview:b];
            return b;
        };
        mkIB(@"Aucune", @selector(iconNone:),   12);
        mkIB(@"Cancel", @selector(iconCancel:), gw + 34 - 172);
        NSButton *ok = mkIB(@"OK", @selector(iconOK:), gw + 34 - 88);
        [ok setKeyEquivalent:@"\r"];

    [gIconPanel makeKeyAndOrderFront:nil];
}

- (void)iconOK:(id)sender {
    if (gInfoTarget && gIconGrid) {
        gInfoTarget->icon = gIconGrid.selected;
        [gInfoIconField setStringValue:[NSString stringWithFormat:@"%d", gIconGrid.selected]];
    }
    [gIconPanel close];
    [self setNeedsDisplay:YES];
}

- (void)iconNone:(id)sender {
    if (gIconGrid) { gIconGrid.selected = 0; [gIconGrid setNeedsDisplay:YES]; }
}

- (void)iconCancel:(id)sender {
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
