//
//  AppDelegate.m
//  HC
//

#import "AppDelegate.h"
#import "HCView.h"
#import "hc_core.h"

@interface AppDelegate ()
@property (strong) IBOutlet NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // --- pile avec deux cartes ---
    Object *stack = hc_new_stack("Essai");

    // carte 1 : accueil
    Object *c1 = hc_new_card(stack, NULL, "accueil");
    hc_new_button(c1, "Bonjour");
    hc_new_button(c1, "Suivant");

    // carte 2 : seconde
    Object *c2 = hc_new_card(stack, NULL, "seconde");
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
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
}

@end
