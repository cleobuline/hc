//
//  AppDelegate.m
//  HC
//

#import "AppDelegate.h"
#import "HCview.h"
#import "hc_core.h"
#import "hc_file.h"
@interface AppDelegate ()
@property (strong) IBOutlet NSWindow *window;
@end

@implementation AppDelegate



static Object *gStack = NULL;
static int gCardCount = 0;   // pour nommer les nouvelles cartes
- (void)testDraw:(id)sender {
    [gView testScribble];
}
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // --- pile avec deux cartes ---
    //Object *stack = hc_new_stack("Essai");
    gStack = hc_new_stack("Essai");

        // un fond commun, partagé par les cartes
        Object *bg = hc_new_background(gStack, "commun");

        // carte 1 : accueil, sur le fond commun
        Object *c1 = hc_new_card(gStack, bg, "accueil");
        hc_new_button(c1, "Bonjour");
        hc_new_button(c1, "Suivant");

        // carte 2 : seconde, même fond
        Object *c2 = hc_new_card(gStack, bg, "seconde");
        hc_new_button(c2, "Retour");

    hc_set_current_card(c1);

    // --- géométrie et scripts ---
    hc_do("set the rect of button \"Bonjour\" to \"40,40,180,70\"");
    hc_do("set the rect of button \"Suivant\" to \"40,90,180,120\"");

    hc_set_script(hc_resolve("button \"Bonjour\""),
        "on mouseUp\n  put \"Bonjour, Patricia !\"\nend mouseUp\n");
    hc_set_script(hc_resolve("button \"Suivant\""),
        "on mouseUp\n  go next card\nend mouseUp\n");

    // pour la carte 2, il faut y aller pour résoudre son bouton
    hc_set_current_card(c2);
    hc_do("set the rect of button \"Retour\" to \"40,40,180,70\"");
    hc_set_script(hc_resolve("button \"Retour\""),
        "on mouseUp\n  go previous card\nend mouseUp\n");
    hc_set_current_card(c1);   // on revient à l'accueil pour démarrer

    // --- vue et message box ---
    NSRect frame = [[self.window contentView] bounds];
    HCView *view = [[HCView alloc] initWithFrame:frame];
    [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self.window setContentView:view];
    [self.window setTitle:@"HyperCard"];

    [view installMessageBox];
    [view installToolPalette];
    [view installPatternPalette];
    [view installWidthPalette];
    [view applyStackSize];

    // menu Fichier
        NSMenu *mainMenu = [NSApp mainMenu];
        NSMenuItem *fileItem = [[NSMenuItem alloc] init];
        NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"Pile"];
    NSMenuItem *siItem = [[NSMenuItem alloc] initWithTitle:@"Infos de la pile…"
                                                        action:@selector(showStackInfo)
                                                 keyEquivalent:@""];
        [siItem setTarget:view];
        [fileMenu addItem:siItem];
        [fileMenu addItemWithTitle:@"Nouvelle carte"
                            action:@selector(newCard:)
                     keyEquivalent:@"n"];
    NSMenuItem *ciItem = [[NSMenuItem alloc] initWithTitle:@"Infos de la carte…"
                                                        action:@selector(showCardInfo)
                                                 keyEquivalent:@""];
        [ciItem setTarget:view];
        [fileMenu addItem:ciItem];
        [fileMenu addItem:[NSMenuItem separatorItem]];
        [fileMenu addItemWithTitle:@"Ouvrir une pile…"
                            action:@selector(openStack:)
                     keyEquivalent:@"o"];
        [fileMenu addItemWithTitle:@"Enregistrer la pile…"
                            action:@selector(saveStack:)
                     keyEquivalent:@"s"];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Tramer la sélection"
                            action:@selector(ditherSelection:)
                     keyEquivalent:@"d"];
    [fileMenu addItem:[NSMenuItem separatorItem]];
        NSMenuItem *bgItem = [[NSMenuItem alloc] initWithTitle:@"Éditer le fond"
                                                        action:@selector(toggleBackground:)
                                                 keyEquivalent:@"b"];
        [bgItem setTarget:view];
        [fileMenu addItem:bgItem];
        [fileItem setSubmenu:fileMenu];
        [mainMenu addItem:fileItem];
    NSMenuItem *nbItem = [[NSMenuItem alloc] initWithTitle:@"Nouveau fond"
                                                        action:@selector(newBackground:)
                                                 keyEquivalent:@""];
        [nbItem setTarget:view];
        [fileMenu addItem:nbItem];

        NSMenuItem *biItem = [[NSMenuItem alloc] initWithTitle:@"Infos du fond…"
                                                        action:@selector(showBackgroundInfo)
                                                 keyEquivalent:@""];
        [biItem setTarget:view];
        [fileMenu addItem:biItem];
    [self.window setReleasedWhenClosed:NO];
    [view applyStackSize];  
}
- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag {
    if (!flag) [self.window makeKeyAndOrderFront:nil];
    return YES;
}
- (void)applicationWillTerminate:(NSNotification *)aNotification {
}
- (void)newCard:(id)sender {
    gCardCount++;
    char name[64];
    snprintf(name, sizeof name, "carte %d", gCardCount);
    // réutiliser le fond de la carte courante
    Object *cur = hc_current_card();
    Object *bg = cur ? cur->bg : NULL;
    Object *c = hc_new_card(gStack, bg, name);
    hc_set_current_card(c);
    [gView setNeedsDisplay:YES];
}
- (void)saveStack:(id)sender {
    const char *nm = gStack && gStack->name ? gStack->name : "MaPile";
    NSSavePanel *panel = [NSSavePanel savePanel];
    [panel setNameFieldStringValue:
        [NSString stringWithFormat:@"%s.stack", nm]];
    if ([panel runModal] == NSModalResponseOK) {
        [gView flushPaintToKernel];
        NSString *path = [[panel URL] path];
        if (hc_save(gStack, [path UTF8String]) != 0)
            NSLog(@"échec de la sauvegarde");
    }
}
- (void)openStack:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setCanChooseFiles:YES];
    [panel setAllowsMultipleSelection:NO];
    if ([panel runModal] == NSModalResponseOK) {
        NSString *path = [[panel URL] path];
        Object *loaded = hc_load([path UTF8String]);
        if (loaded) {
            hc_free(gStack);
            gStack = loaded;
            [gView clearPaintCache];
            for (int i = 0; i < gStack->nparts; i++) {
                if (gStack->parts[i]->type == OBJ_CARD) {
                    hc_set_current_card(gStack->parts[i]);
                    break;
                }
            }
            [gView applyStackSize];      // ← ajuster la fenêtre à la taille de la pile chargée
            [self.window makeKeyAndOrderFront:nil];
            [gView setNeedsDisplay:YES];
        } else {
            NSLog(@"échec du chargement");
        }
    }
}
@end
