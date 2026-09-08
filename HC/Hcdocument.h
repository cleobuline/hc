#ifndef HCdocument_h
#define HCdocument_h

#import <Cocoa/Cocoa.h>
#import "hc_core.h"

@class HCView;

/* ═══ Une pile ouverte, avec sa fenêtre ══════════════════════════════════════
 *
 * Jusqu'ici l'application tenait une seule pile dans une variable globale, et
 * une seule fenêtre venue du nib. Ouvrir plusieurs piles demande de regrouper
 * ce qui va ensemble : la pile, le fichier d'où elle vient, sa fenêtre et sa
 * vue.
 *
 * La classe ne POSSÈDE pas la pile au sens de la mémoire — c'est hc_free qui
 * la libère, et le noyau tient son propre registre. Elle possède en revanche
 * la fenêtre et la vue, qui n'existent que pour elle.
 *
 * `path` est nil tant que la pile n'a jamais été enregistrée. Il sert à deux
 * choses : « Enregistrer » sans redemander, et la recherche de « go to stack
 * "X" », qui commence à côté de la pile courante comme le faisait HyperCard. */
@interface HCDocument : NSObject <NSWindowDelegate>

@property (assign, nonatomic) Object       *stack;
@property (strong, nonatomic) NSString     *path;
@property (strong, nonatomic) NSWindow     *window;
@property (strong, nonatomic) HCView       *view;
@property (assign, nonatomic) int           cardCount;  /* cartes créées */

/* Vrai dès que cette fenêtre a été au premier plan une première fois.
 *
 * suspendStack et resumeStack disent qu'on QUITTE une pile ouverte pour une
 * autre, et qu'on y REVIENT. La toute première fois qu'une fenêtre passe au
 * premier plan, on n'y revient pas : on l'ouvre, et c'est openStack qui le
 * dit. Sans ce drapeau, chaque ouverture aurait envoyé resumeStack dans la
 * foulée d'openStack, et une pile qui règle ses menus dans les deux les
 * aurait posés deux fois. */
@property (assign, nonatomic) BOOL          dejaVue;

/* Vrai à partir de windowWillClose:. Fermer une fenêtre lui fait perdre le
 * premier plan, ce qui enverrait suspendStack juste avant closeStack. */
@property (assign, nonatomic) BOOL          enFermeture;

/* Le pendant de dejaVue pour les messages système du cycle de vie. */
- (void)envoiePile:(const char *)message;

/* Le document actif : celui dont la fenêtre est au premier plan. C'est lui
 * que visent les commandes de menu et les scripts. */
+ (HCDocument *)current;
+ (void)setCurrent:(HCDocument *)doc;

/* Tous les documents ouverts, dans l'ordre d'ouverture. */
+ (NSArray<HCDocument *> *)allDocuments;

/* Celui qui porte cette pile, ou nil. Sert aux callbacks du noyau, qui ne
 * connaissent que des Object *. */
+ (HCDocument *)documentForStack:(Object *)stack;

/* Fabrique un document complet : fenêtre, vue, enregistrement.
 *
 * `path` peut être nil pour une pile jamais enregistrée. La fenêtre est
 * décalée par rapport à la précédente, faute de quoi les piles s'empileraient
 * exactement l'une sur l'autre et l'on croirait qu'il ne s'en est ouvert
 * qu'une. */
+ (HCDocument *)documentWithStack:(Object *)stack path:(NSString *)path;

/* Enregistre le document dans la liste et auprès du noyau. */
- (void)registerDocument;
- (void)unregisterDocument;

@end

#endif /* HCdocument_h */
