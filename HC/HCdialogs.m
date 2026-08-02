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

@implementation HCView (Dialogs)


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
    int rang = 0;
    Object *owner = obj->owner;
    if (owner)
        for (int i = 0; i < owner->nparts; i++)
            if (owner->parts[i] == obj) { rang = i + 1; break; }

    NSTextField *ids = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 200, 200, 52)];
    [ids setStringValue:[NSString stringWithFormat:
        @"Card button number: %d\nCard part number: %d\nCard button ID: %d",
        rang, rang, obj->id]];
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
    
    const char *st = obj->style ? obj->style : "rectangle";
    [gInfoStyle selectItemWithTitle:[NSString stringWithUTF8String:st]];
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
    [ids setStringValue:[NSString stringWithFormat:
        @"Card number: %d out of %d\nCard ID: %d\nCard fields: %d",
        rang, total, card->id, card->nparts]];
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
            free(o->name);
            o->name = strdup([[gInfoName stringValue] UTF8String]);
            // style
            free(o->style);
            o->style = strdup([[gInfoStyle titleOfSelectedItem] UTF8String]);
            o->textsize = [[gInfoTextSize stringValue] intValue];
            o->showname   = ([gInfoShowName state]   == NSControlStateValueOn);
            o->autohilite = ([gInfoAutoHilite state] == NSControlStateValueOn);
            o->icon = [[gInfoIconField stringValue] intValue];
        }
    [gInfoPanel close];
    gInfoTarget = NULL;
    [self setNeedsDisplay:YES];
}

- (void)infoCancel:(id)sender {
    [gInfoPanel close];
    gInfoTarget = NULL;
}

- (void)infoScript:(id)sender {
    Object *o = gInfoTarget;
    [self infoOK:sender];        // valider les changements avant
    if (o) [self editScriptOf:o];
}

@end
