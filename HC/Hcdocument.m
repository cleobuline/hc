//
//  HCdocument.m — une pile ouverte, avec sa fenêtre et sa vue
//

#import "HCdocument.h"
#import "HCview.h"
#import "HCglobals.h"

/* Définie dans HCview.m : désigne le HCDoc de la fenêtre active. */
extern void hc_set_active_doc(void *d);

/* La liste des documents ouverts, et celui qui est actif.
 *
 * Une liste plutôt qu'un tableau de taille fixe : rien n'oblige à borner le
 * nombre de piles ouvertes côté interface, et le noyau a déjà sa propre limite
 * pour son registre. */
static NSMutableArray<HCDocument *> *gDocs = nil;
static HCDocument *gCurrentDoc = nil;

@implementation HCDocument

+ (NSArray<HCDocument *> *)allDocuments {
    if (!gDocs) gDocs = [NSMutableArray array];
    return gDocs;
}

+ (HCDocument *)current { return gCurrentDoc; }

+ (void)setCurrent:(HCDocument *)doc {
    if (!doc || doc == gCurrentDoc) return;
    gCurrentDoc = doc;

    /* gView désigne la vue ACTIVE : c'est par lui que passent le noyau et les
     * palettes, qui n'ont pas à savoir combien de fenêtres existent. Le faire
     * suivre ici est ce qui rend le reste du programme indifférent au nombre
     * de piles ouvertes. */
    gView = doc.view;
    /* L'état de document suit la fenêtre active : sans cela, toutes les vues
     * partageraient un même champ en édition, un même objet sélectionné, une
     * même carte — et la seconde fenêtre afficherait le contenu de la
     * première. */
    hc_set_active_doc([doc.view docState]);
}

+ (HCDocument *)documentForStack:(Object *)stack {
    if (!stack) return nil;
    for (HCDocument *d in [self allDocuments])
        if (d.stack == stack) return d;
    return nil;
}

+ (HCDocument *)documentWithStack:(Object *)stack path:(NSString *)path {
    if (!stack) return nil;

    int w = stack->w > 0 ? stack->w : 512;
    int h = stack->h > 0 ? stack->h : 342;

    /* Décaler chaque nouvelle fenêtre : sans cela elles se superposeraient au
     * pixel près, et rien ne dirait qu'une seconde pile s'est ouverte. */
    NSInteger rang = [[self allDocuments] count];
    CGFloat dx = 24 * (rang % 8), dy = 24 * (rang % 8);

    NSWindow *win = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(140 + dx, 320 - dy, w, h)
                     /* Pas de redimensionnement : la taille d'une fenetre de pile
                      * est celle de ses cartes, comme dans HyperCard, ou l'on
                      * passait par « Infos pile » et non par le coin de la
                      * fenetre. Ce n'est pas qu'une question de fidelite — le
                      * calque de peinture est cree a la taille de la vue, et le
                      * laisser changer en cours de route le faisait repartir du
                      * PNG enregistre, effacant tout ce qui avait ete peint
                      * depuis la derniere sauvegarde. */
                     styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                NSWindowStyleMaskMiniaturizable)
                    backing:NSBackingStoreBuffered defer:NO];
    [win setReleasedWhenClosed:NO];

    HCView *v = [[HCView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [v setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [win setContentView:v];

    HCDocument *d = [[HCDocument alloc] init];
    d.stack  = stack;
    d.path   = path;
    d.window = win;
    d.view   = v;
    [win setDelegate:d];

    [d registerDocument];
    [win setTitle:path ? [path lastPathComponent]
                       : (stack->name ? [NSString stringWithUTF8String:stack->name]
                                      : @"Sans titre")];
    [win makeKeyAndOrderFront:nil];
    return d;
}

- (void)registerDocument {
    if (!gDocs) gDocs = [NSMutableArray array];
    if (![gDocs containsObject:self]) [gDocs addObject:self];
    if (self.stack) hc_register_stack(self.stack);
    [HCDocument setCurrent:self];
}

- (void)unregisterDocument {
    /* Retirer du registre du noyau AVANT toute libération : il y garde des
     * pointeurs empruntés, et une pile libérée sans avoir été retirée y
     * laisserait une adresse morte. */
    if (self.stack) hc_unregister_stack(self.stack);
    [gDocs removeObject:self];
    if (gCurrentDoc == self) {
        gCurrentDoc = [gDocs lastObject];
        gView = gCurrentDoc.view;
        hc_set_active_doc(gCurrentDoc ? [gCurrentDoc.view docState] : NULL);
    }
}

/* ---- fenêtre ---- */

/* La fenêtre passe au premier plan : ce document devient l'actif.
 *
 * C'est le seul endroit qui décide du document courant, et c'est voulu :
 * l'utilisateur désigne la pile sur laquelle il travaille en cliquant sur sa
 * fenêtre, comme dans toute application à documents multiples. */
- (void)windowDidBecomeMain:(NSNotification *)note {
    [HCDocument setCurrent:self];
    if (self.stack) {
        /* Replacer le noyau sur une carte de CETTE pile : les scripts et les
         * commandes de menu travaillent sur la carte courante, qui doit suivre
         * la fenêtre au premier plan. */
        Object *cur = hc_current_card();
        if (!cur || cur->owner != self.stack) {
            for (int i = 0; i < self.stack->nparts; i++)
                if (self.stack->parts[i]->type == OBJ_CARD) {
                    hc_set_current_card(self.stack->parts[i]);
                    break;
                }
        }
    }
    [self.view setNeedsDisplay:YES];
}

- (void)windowWillClose:(NSNotification *)note {
    (void)note;

    /* Fermer une fenêtre libère sa pile.
     *
     * L'ordre compte, et chaque étape répare une façon de planter :
     *
     * 1. la vue lâche ce qu'elle retient — champ en édition, objet
     *    sélectionné, cache de bitmaps — sinon le prochain redessin suivrait
     *    des pointeurs dans de la mémoire rendue ;
     * 2. le noyau perd la pile de son registre, faute de quoi « stack "X" »
     *    la retrouverait après sa mort ;
     * 3. si la carte courante du noyau appartenait à cette pile, on le
     *    déplace vers une pile qui survit — l'interpréteur lit
     *    hc_current_card() à chaque commande ;
     * 4. et seulement là, hc_free.
     *
     * Sans cette libération, chaque fermeture perdait la pile entière :
     * cartes, champs, plages de style, bitmaps de peinture. */
    Object *pile = self.stack;

    /* Rendre CE document actif le temps du nettoyage : resetForNewStack agit
     * sur le document actif, et fermer une fenêtre qui n'est pas au premier
     * plan viderait celui d'une autre. */
    HCDocument *avant = [HCDocument current];
    [HCDocument setCurrent:self];
    [self.view resetForNewStack];
    [self.view clearPaintCache];
    [self unregisterDocument];
    /* unregisterDocument a déjà désigné un remplaçant si nous étions l'actif ;
     * on ne rétablit donc l'ancien que s'il survit et n'était pas nous. */
    if (avant && avant != self && [[HCDocument allDocuments] containsObject:avant])
        [HCDocument setCurrent:avant];

    if (pile) {
        Object *cur = hc_current_card();
        if (cur && cur->owner == pile) {
            Object *repli = NULL;
            HCDocument *autre = [HCDocument current];
            if (autre && autre.stack && autre.stack != pile) {
                for (int i = 0; i < autre.stack->nparts; i++)
                    if (autre.stack->parts[i]->type == OBJ_CARD) {
                        repli = autre.stack->parts[i];
                        break;
                    }
            }
            hc_set_current_card(repli);   /* NULL si c'était la dernière pile */
        }
        self.stack = NULL;
        hc_free(pile);
    }

    /* Rompre le lien : AppKit peut encore envoyer des messages à un délégué
     * après la fermeture, et nous n'avons plus rien à en faire. */
    [self.window setDelegate:nil];
    self.view   = nil;
    self.window = nil;
}

@end
