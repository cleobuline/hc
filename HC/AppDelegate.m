//
//  AppDelegate.m
//  HC
//

#import "AppDelegate.h"
#import "HCview.h"
#import "HCdialogs.h"
#import "hc_core.h"
#import "hc_file.h"
@interface AppDelegate ()
@property (strong) IBOutlet NSWindow *window;
@end

@implementation AppDelegate



static Object *gStack = NULL;
static int gCardCount = 0;   // pour nommer les nouvelles cartes

/* Pile reclamee par le Finder avant que l'interface existe. Voir
 * application:openFile: plus bas : l'Apple Event d'ouverture arrive ENTRE
 * applicationWillFinishLaunching: et applicationDidFinishLaunching:, donc
 * gView est encore nil au moment ou le Finder nous parle. On met le chemin
 * de cote et on le traite une fois la vue construite. */
static NSString *gPendingOpen = nil;

/* Retrouve le menu Fichier fourni par le nib.
 *
 * Le chercher par son titre serait fragile : il s'appelle « File » ou
 * « Fichier » selon la langue du système, et le nib n'est pas localisé. On le
 * reconnaît donc à son CONTENU — c'est celui qui porte print: ou
 * runPageLayout:, deux actions qu'AppKit n'y met jamais par hasard.
 *
 * À défaut, on prend le deuxième menu de la barre : le premier est toujours
 * le menu d'application, le suivant est Fichier dans tous les gabarits. */
static NSMenu *find_file_menu(void)
{
    NSMenu *main = [NSApp mainMenu];
    if (!main) return nil;

    for (NSMenuItem *top in [main itemArray]) {
        NSMenu *sub = [top submenu];
        if (!sub) continue;
        for (NSMenuItem *it in [sub itemArray])
            if ([it action] == @selector(print:) ||
                [it action] == @selector(runPageLayout:))
                return sub;
    }
    return [main numberOfItems] > 1 ? [[main itemAtIndex:1] submenu] : nil;
}
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    /* Pile vide au démarrage : une carte sur un fond, rien de plus.
     * La pile de démonstration qui vivait ici — deux cartes, trois boutons et
     * leurs scripts — a fait son temps : « Nouvelle pile » et « Ouvrir » sont
     * en place, et le programme n'a plus à se fabriquer un contenu factice
     * pour avoir quelque chose à montrer. */
    gStack = hc_new_stack("Sans titre");
    Object *bg = hc_new_background(gStack, "commun");
    Object *c1 = hc_new_card(gStack, bg, "carte 1");
    hc_set_current_card(c1);

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

    /* --- menu « Pile » : nos commandes propres, ajoutées en bout de barre.
     * Le menu Fichier, lui, vient du nib et se remplit plus bas. --- */
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

    /* --- menu Fichier : ouvrir et enregistrer ---
     * Leur place naturelle, et celle où tout utilisateur de Mac va les
     * chercher. On les met en tête, avant Fermer et Imprimer.
     *
     * La cible est posée explicitement : sans elle l'envoi remonte la chaîne
     * des réponses, qui part du premier répondant — donc de la vue, puis de
     * la fenêtre. Le délégué d'application n'y figure qu'en dernier recours,
     * et un champ en cours d'édition pourrait avaler le message avant lui. */
    NSMenu *sysFile = find_file_menu();
    if (sysFile) {
        /* Faire le ménage AVANT d'insérer les nôtres, sinon les index de
         * l'insertion se décalent.
         *
         * Le nib apporte les commandes de document standard d'AppKit — New,
         * Open…, Open Recent, Save…, Save As… — qui ne mènent nulle part ici :
         * ce programme ne repose pas sur NSDocument, et elles restent grisées
         * en doublon des nôtres. On les retire par leur ACTION et non par leur
         * titre, qui change avec la langue du système.
         *
         * Print et Page Setup restent : ils serviront le jour où l'impression
         * d'une carte sera écrite. */
        SEL morts[] = {
            @selector(newDocument:),
            @selector(openDocument:),
            @selector(saveDocument:),
            @selector(saveDocumentAs:),
            @selector(saveDocumentTo:),
            @selector(revertDocumentToSaved:),
        };
        for (NSInteger i = [sysFile numberOfItems] - 1; i >= 0; i--) {
            NSMenuItem *it = [sysFile itemAtIndex:i];

            /* « Open Recent » n'a pas d'action propre : c'est un sous-menu
             * peuplé par le contrôleur de documents. On le reconnaît à ce
             * qu'il porte un sous-menu contenant clearRecentDocuments:. */
            BOOL recent = NO;
            for (NSMenuItem *sub in [[it submenu] itemArray])
                if ([sub action] == @selector(clearRecentDocuments:)) { recent = YES; break; }

            BOOL mort = recent;
            for (unsigned k = 0; !mort && k < sizeof morts / sizeof *morts; k++)
                if ([it action] == morts[k]) mort = YES;

            if (mort) [sysFile removeItemAtIndex:i];
        }

        /* Deux séparateurs qui se suivent, ou un séparateur en tête, sont ce
         * que laisse toujours une suppression d'entrées. */
        for (NSInteger i = [sysFile numberOfItems] - 1; i >= 0; i--) {
            if (![[sysFile itemAtIndex:i] isSeparatorItem]) continue;
            if (i == 0 || i == [sysFile numberOfItems] - 1 ||
                [[sysFile itemAtIndex:i-1] isSeparatorItem])
                [sysFile removeItemAtIndex:i];
        }

        /* HyperCard réservait Cmd-N à « Nouvelle carte », et laissait
         * « Nouvelle pile » sans raccourci. On garde cette répartition : c'est
         * la carte qu'on crée cent fois par séance, pas la pile. */
        NSMenuItem *np = [[NSMenuItem alloc] initWithTitle:@"Nouvelle pile…"
                                                    action:@selector(newStack:)
                                             keyEquivalent:@""];
        [np setTarget:self];

        NSMenuItem *op = [[NSMenuItem alloc] initWithTitle:@"Ouvrir une pile…"
                                                    action:@selector(openStack:)
                                             keyEquivalent:@"o"];
        [op setTarget:self];

        NSMenuItem *sv = [[NSMenuItem alloc] initWithTitle:@"Enregistrer la pile…"
                                                    action:@selector(saveStack:)
                                             keyEquivalent:@"s"];
        [sv setTarget:self];

        [sysFile insertItem:np atIndex:0];
        [sysFile insertItem:op atIndex:1];
        [sysFile insertItem:sv atIndex:2];
        [sysFile insertItem:[NSMenuItem separatorItem] atIndex:3];
    } else {
        /* Nib inattendu : plutôt que de perdre les deux commandes, on les
         * laisse dans le menu Pile, là où elles étaient. */
        [fileMenu addItem:[NSMenuItem separatorItem]];
        NSMenuItem *np = [fileMenu addItemWithTitle:@"Nouvelle pile…"
                                             action:@selector(newStack:)
                                      keyEquivalent:@""];
        [np setTarget:self];
        NSMenuItem *op = [fileMenu addItemWithTitle:@"Ouvrir une pile…"
                                             action:@selector(openStack:)
                                      keyEquivalent:@"o"];
        [op setTarget:self];
        NSMenuItem *sv = [fileMenu addItemWithTitle:@"Enregistrer la pile…"
                                             action:@selector(saveStack:)
                                      keyEquivalent:@"s"];
        [sv setTarget:self];
    }

    /* --- menu « Outils » : afficher ou masquer les palettes ---
     * HyperCard laissait refermer ses palettes et les rappeler d'ici, avec une
     * coche devant celles qui sont à l'écran. Une seule entrée par palette,
     * qui bascule : deux entrées « Afficher » et « Masquer » en laisseraient
     * toujours une inutile. Le tag identifie la palette côté vue. */
    NSMenuItem *toolsItem = [[NSMenuItem alloc] init];
    NSMenu *toolsMenu = [[NSMenu alloc] initWithTitle:@"Outils"];
    struct { NSString *title; NSInteger tag; NSString *key; } pals[] = {
        { @"Outils",    1, @"1" },
        { @"Motifs",    2, @"2" },
        { @"Épaisseur", 3, @"3" },
        { @"Pinceaux",  4, @"4" },
    };
    for (int i = 0; i < 4; i++) {
        NSMenuItem *mi = [[NSMenuItem alloc] initWithTitle:pals[i].title
                                                    action:@selector(togglePalette:)
                                             keyEquivalent:pals[i].key];
        [mi setTag:pals[i].tag];
        [mi setTarget:view];
        [toolsMenu addItem:mi];
    }
    [toolsItem setSubmenu:toolsMenu];
    [mainMenu addItem:toolsItem];

    /* --- retirer les menus du gabarit qui ne servent à rien ici ---
     * View, Window et Help viennent du nib d'Xcode. Le premier ne pilote
     * qu'une barre d'outils inexistante ; les deux autres s'adressent à une
     * application à fenêtres multiples et à un livre d'aide qui n'existe pas.
     *
     * L'ordre compte : AppKit garde une référence sur les menus Fenêtre et
     * Aide et continue d'y écrire — la liste des fenêtres ouvertes, l'entrée
     * de recherche d'aide. Il faut donc les lui RENDRE avant de les retirer,
     * sinon il écrit dans un menu qui n'est plus dans la barre.
     *
     * On les reconnaît à leur contenu et non à leur titre, qui suit la langue
     * du système : ce nib n'est pas localisé, mais la barre, elle, l'est. */
    [NSApp setWindowsMenu:nil];
    [NSApp setHelpMenu:nil];

    /* --- menu Edit : ne garder que ce qui a un sens ici ---
     * Le gabarit apporte quatre sous-menus qui s'adressent à un traitement de
     * texte : Find, Spelling and Grammar, Substitutions, Transformations. Rien
     * de tout cela ne s'applique à une pile.
     *
     * Speech reste : HyperCard avait « speak » en HyperTalk via MacinTalk, et
     * des piles entières lisaient leur texte à voix haute.
     *
     * Find est remplacé par la version d'HyperCard : pas de panneau, mais
     * « find "" » préparé dans la boîte de message. */
    NSMenu *editMenu = nil;
    for (NSMenuItem *top in [mainMenu itemArray]) {
        for (NSMenuItem *it in [[top submenu] itemArray])
            if ([it action] == @selector(paste:)) { editMenu = [top submenu]; break; }
        if (editMenu) break;
    }

    if (editMenu) {
        SEL sousMenusMorts[] = {
            @selector(performFindPanelAction:),       /* Find                  */
            @selector(showGuessPanel:),               /* Spelling and Grammar  */
            @selector(checkSpelling:),
            @selector(orderFrontSubstitutionsPanel:), /* Substitutions         */
            @selector(toggleSmartInsertDelete:),
            @selector(uppercaseWord:),                /* Transformations       */
            @selector(capitalizeWord:),
        };
        for (NSInteger i = [editMenu numberOfItems] - 1; i >= 0; i--) {
            NSMenu *sub = [[editMenu itemAtIndex:i] submenu];
            if (!sub) continue;
            BOOL mort = NO;
            for (NSMenuItem *it in [sub itemArray]) {
                for (unsigned k = 0; !mort && k < sizeof sousMenusMorts / sizeof *sousMenusMorts; k++)
                    if ([it action] == sousMenusMorts[k]) mort = YES;
                if (mort) break;
            }
            if (mort) [editMenu removeItemAtIndex:i];
        }

        for (NSInteger i = [editMenu numberOfItems] - 1; i >= 0; i--) {
            if (![[editMenu itemAtIndex:i] isSeparatorItem]) continue;
            if (i == 0 || i == [editMenu numberOfItems] - 1 ||
                [[editMenu itemAtIndex:i-1] isSeparatorItem])
                [editMenu removeItemAtIndex:i];
        }

        [editMenu addItem:[NSMenuItem separatorItem]];
        NSMenuItem *fd = [[NSMenuItem alloc] initWithTitle:@"Chercher…"
                                                    action:@selector(findInStack:)
                                             keyEquivalent:@"f"];
        [fd setTarget:view];
        [editMenu addItem:fd];
    }

    SEL signatures[] = {
        @selector(toggleToolbarShown:),           /* View   */
        @selector(runToolbarCustomizationPalette:),
        @selector(arrangeInFront:),               /* Window */
        @selector(performMiniaturize:),
        @selector(performZoom:),
        @selector(showHelp:),                     /* Help   */
    };
    for (NSInteger i = [mainMenu numberOfItems] - 1; i >= 0; i--) {
        NSMenu *sub = [[mainMenu itemAtIndex:i] submenu];
        if (!sub || sub == toolsMenu || sub == fileMenu || sub == sysFile) continue;

        BOOL mort = NO;
        for (NSMenuItem *it in [sub itemArray]) {
            for (unsigned k = 0; !mort && k < sizeof signatures / sizeof *signatures; k++)
                if ([it action] == signatures[k]) mort = YES;
            if (mort) break;
        }
        if (mort) [mainMenu removeItemAtIndex:i];
    }

    [self.window setReleasedWhenClosed:NO];
    [view applyStackSize];

    /* Le Finder nous a peut-etre demande une pile avant que tout ceci existe. */
    if (gPendingOpen) {
        NSString *p = gPendingOpen;
        gPendingOpen = nil;
        [self loadStackAtPath:p];
    }
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
/* Installe une pile déjà construite : libère l'ancienne, remet la vue à zéro,
 * se place sur la première carte. Partagé par l'ouverture d'un fichier et la
 * création d'une pile neuve — les deux font exactement la même chose une fois
 * la pile en main, et deux copies de cette séquence finiraient par diverger. */
- (void)installStack:(Object *)st {
    /* AVANT hc_free : la vue garde des pointeurs dans l'ancienne pile (objet
     * sélectionné, champ en cours d'édition, cache de peinture). Les libérer
     * sans prévenir la vue laisse des pointeurs pendants qui plantent au
     * premier redessin. */
    [gView resetForNewStack];

    hc_free(gStack);
    gStack = st;
    [gView clearPaintCache];

    Object *first = NULL;
    for (int i = 0; i < gStack->nparts; i++) {
        if (gStack->parts[i]->type == OBJ_CARD) { first = gStack->parts[i]; break; }
    }
    if (!first) {                       /* pile sans carte : on en fabrique une */
        Object *bg = hc_new_background(gStack, "commun");
        first = hc_new_card(gStack, bg, "carte 1");
    }
    hc_set_current_card(first);

    [gView applyStackSize];
    [gView updateWindowTitle];
    [self.window makeKeyAndOrderFront:nil];
    [gView setNeedsDisplay:YES];
}

/* Charge une pile et l'installe. Le corps de l'ancien openStack:, sorti du
 * panneau de selection pour que le double-clic dans le Finder puisse
 * emprunter exactement le meme chemin. */
- (BOOL)loadStackAtPath:(NSString *)path {
    Object *loaded = hc_load([path UTF8String]);
    if (!loaded) {
        NSLog(@"échec du chargement : %@", path);
        NSAlert *a = [[NSAlert alloc] init];
        [a setMessageText:@"Pile illisible"];
        [a setInformativeText:[path lastPathComponent]];
        [a runModal];
        return NO;
    }
    [self installStack:loaded];
    gCardCount = 0;          /* la numérotation repart avec la nouvelle pile */
    return YES;
}

/* --- Nouvelle pile ---
 * On jette la pile courante. La confirmation n'est pas du zèle : il n'y a pas
 * d'indicateur de modification dans ce programme, donc rien ne distingue une
 * pile fraîchement enregistrée d'une heure de travail non sauvegardée. Tant
 * que ce sera le cas, mieux vaut demander. */
- (void)newStack:(id)sender {
    NSAlert *a = [[NSAlert alloc] init];
    [a setMessageText:@"Nouvelle pile"];
    [a setInformativeText:@"La pile ouverte sera fermée. "
                           @"Ce qui n'a pas été enregistré sera perdu."];
    [a addButtonWithTitle:@"Créer"];
    [a addButtonWithTitle:@"Annuler"];

    NSTextField *nameField =
        [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 240, 24)];
    [nameField setStringValue:@"Sans titre"];
    [a setAccessoryView:nameField];
    [[a window] setInitialFirstResponder:nameField];

    if ([a runModal] != NSAlertFirstButtonReturn) return;

    NSString *name = [[nameField stringValue]
        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    if ([name length] == 0) name = @"Sans titre";

    Object *st = hc_new_stack([name UTF8String]);
    Object *bg = hc_new_background(st, "commun");
    hc_new_card(st, bg, "carte 1");

    [self installStack:st];
    gCardCount = 0;
}

- (void)openStack:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setCanChooseFiles:YES];
    [panel setAllowsMultipleSelection:NO];
    if ([panel runModal] == NSModalResponseOK)
        [self loadStackAtPath:[[panel URL] path]];
}

/* Double-clic sur une pile dans le Finder. Sans ce message, AppKit refuse
 * le document avec « cannot open files in the HyperCard Stack format »,
 * meme quand le type est correctement declare dans Info.plist. */
- (BOOL)application:(NSApplication *)sender openFile:(NSString *)path {
    if (!gView) { gPendingOpen = path; return YES; }   /* trop tot : voir plus haut */
    return [self loadStackAtPath:path];
}

/* Plusieurs piles laches d'un coup sur l'icone du Dock. */
- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)paths {
    BOOL ok = NO;
    for (NSString *p in paths) {
        if (!gView) { gPendingOpen = p; ok = YES; break; }  /* on n'en garde qu'une */
        ok = [self loadStackAtPath:p] || ok;
    }
    [sender replyToOpenOrPrint:ok ? NSApplicationDelegateReplySuccess
                                  : NSApplicationDelegateReplyFailure];
}
@end
