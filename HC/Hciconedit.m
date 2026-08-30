/* HCiconedit.m — gros bits et operations sur les icones de la pile.
 *
 * Le catalogue et le choix appartiennent a IconGrid (HCpalettes) ; ici on ne
 * fait que modifier. La separation tient a une raison simple : le choix d'une
 * icone marche aussi sur les icones d'origine, la modification non — HCICONS
 * est const. C'est hcicon_edit_editable qui absorbe la difference, et c'est
 * le seul endroit qui la connaisse.
 */
#import "HCiconedit.h"
#import "HCicons.h"

#include <string.h>

#define CELL   8                 /* cote d'un pixel d'icone, en points */
#define SIDE   (32 * CELL)

/* ==================== operations ==================== */

void hcicon_edit_sync(Object *stack)
{
    hcicon_use_stack_icons(stack ? stack->icons  : NULL,
                           stack ? stack->nicons : 0);
}

struct StackIcon *hcicon_edit_editable(Object *stack, int id)
{
    if (!stack || id == 0) return NULL;

    struct StackIcon *e = hc_icon_get(stack, id);
    if (e) return e;

    /* Icone d'origine : on la recopie dans la pile sous le meme numero.
     * hcicon_find donnant la priorite a la pile, la copie masque desormais
     * l'originale — dans cette pile seulement. */
    const HCIcon *src = hcicon_find(id);
    if (!src) return NULL;

    e = hc_icon_add(stack, id, src->name);
    if (e) memcpy(e->bits, src->bits, HC_ICON_BYTES);
    hcicon_edit_sync(stack);
    return e;
}

/* hcicon_find repond sur les deux tables a la fois : un numero pour lequel il
 * rend NULL est libre partout. On part de 1000 pour donner des numeros courts,
 * et l'on avance jusqu'au premier trou. */
int hcicon_edit_free_id(void)
{
    for (int id = 1000; id < 100000; id++)
        if (!hcicon_find(id)) return id;
    return 0;
}

int hcicon_edit_new(Object *stack)
{
    if (!stack) return 0;
    /* Le catalogue doit etre a jour AVANT de chercher un trou : une icone
     * creee juste avant et pas encore recopiee passerait pour libre, et la
     * seconde ecraserait la premiere. */
    hcicon_edit_sync(stack);
    int id = hcicon_edit_free_id();
    if (!id || !hc_icon_add(stack, id, "Sans titre")) return 0;
    hcicon_edit_sync(stack);
    return id;
}

int hcicon_edit_duplicate(Object *stack, int id)
{
    if (!stack) return 0;
    const HCIcon *src = hcicon_find(id);
    if (!src) return 0;

    hcicon_edit_sync(stack);
    int nid = hcicon_edit_free_id();
    if (!nid) return 0;
    struct StackIcon *e = hc_icon_add(stack, nid, src->name);
    if (!e) return 0;
    memcpy(e->bits, src->bits, HC_ICON_BYTES);
    hcicon_edit_sync(stack);
    return nid;
}

void hcicon_edit_erase(Object *stack, int id)
{
    struct StackIcon *e = hcicon_edit_editable(stack, id);
    if (!e) return;
    memset(e->bits, 0, HC_ICON_BYTES);
    hcicon_edit_sync(stack);
}

void hcicon_edit_rename(Object *stack, int id, const char *name)
{
    if (!hc_icon_get(stack, id)) return;   /* une icone d'origine ne se renomme pas */
    hc_icon_add(stack, id, name);          /* meme numero : remplace le nom */
    hcicon_edit_sync(stack);
}

int hcicon_edit_users(Object *stack, int id)
{
    if (!stack || id == 0) return 0;
    int n = 0;
    for (int i = 0; i < stack->nparts; i++) {
        Object *layer = stack->parts[i];          /* fonds et cartes */
        for (int j = 0; j < layer->nparts; j++)
            if (layer->parts[j]->type == OBJ_BUTTON &&
                layer->parts[j]->icon == id) n++;
    }
    return n;
}

void hcicon_edit_delete(Object *stack, int id)
{
    if (!hc_icon_get(stack, id)) return;   /* on ne supprime que celles de la pile */
    hc_icon_remove(stack, id);
    hcicon_edit_sync(stack);
}

/* ==================== grille d'edition ==================== */

@implementation HCFatBits {
    int _drawValue;    /* ce que pose le glisse en cours : 1 encre, 0 blanc */
}

+ (CGFloat)side { return SIDE; }

/* Retournee, comme HCView et IconGrid : hcicon_draw parcourt les lignes de 0 a
 * 31 en montant en ordonnee, ce qui ne donne le bon sens que dans un repere
 * descendant. Une vue non retournee afficherait tout la tete en bas. */
- (BOOL)isFlipped { return YES; }

/* Repondre des le PREMIER clic, meme si le panneau n'est pas actif : c'est ce
 * que font deja les autres palettes du programme. */
- (BOOL)acceptsFirstMouse:(NSEvent *)event { (void)event; return YES; }

- (int)pixelRow:(int)row col:(int)col {
    const HCIcon *ic = hcicon_find(self.iconId);
    if (!ic) return 0;
    return (ic->bits[row * 4 + col / 8] & (0x80 >> (col & 7))) ? 1 : 0;
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;

    [[NSColor whiteColor] setFill];
    NSRectFill([self bounds]);

    /* L'encre d'abord, la grille par dessus : l'inverse noierait les traits
     * gris sous les pixels noirs, et l'on ne compterait plus les cases. */
    [[NSColor blackColor] setFill];
    for (int row = 0; row < 32; row++)
        for (int col = 0; col < 32; col++)
            if ([self pixelRow:row col:col])
                NSRectFill(NSMakeRect(col * CELL, row * CELL, CELL, CELL));

    [[NSColor colorWithWhite:0.78 alpha:1.0] setStroke];
    NSBezierPath *g = [NSBezierPath bezierPath];
    for (int i = 0; i <= 32; i++) {
        [g moveToPoint:NSMakePoint(i * CELL + 0.5, 0)];
        [g lineToPoint:NSMakePoint(i * CELL + 0.5, SIDE)];
        [g moveToPoint:NSMakePoint(0,    i * CELL + 0.5)];
        [g lineToPoint:NSMakePoint(SIDE, i * CELL + 0.5)];
    }
    [g setLineWidth:1];
    [g stroke];

    /* Un trait plus marque tous les huit : sans reperes on ne se situe pas
     * dans une grille de 32 sans compter case par case. */
    [[NSColor colorWithWhite:0.45 alpha:1.0] setStroke];
    NSBezierPath *q = [NSBezierPath bezierPath];
    for (int i = 0; i <= 32; i += 8) {
        [q moveToPoint:NSMakePoint(i * CELL + 0.5, 0)];
        [q lineToPoint:NSMakePoint(i * CELL + 0.5, SIDE)];
        [q moveToPoint:NSMakePoint(0,    i * CELL + 0.5)];
        [q lineToPoint:NSMakePoint(SIDE, i * CELL + 0.5)];
    }
    [q setLineWidth:1];
    [q stroke];
}

- (BOOL)hitRow:(int *)row col:(int *)col forEvent:(NSEvent *)e {
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    int c = (int)floor(p.x / CELL), r = (int)floor(p.y / CELL);
    if (c < 0 || c > 31 || r < 0 || r > 31) return NO;
    *row = r; *col = c;
    return YES;
}

- (void)paintRow:(int)row col:(int)col value:(int)v {
    struct StackIcon *e = hcicon_edit_editable(self.stack, self.iconId);
    if (!e) return;

    unsigned char mask = (unsigned char)(0x80 >> (col & 7));
    unsigned char *b   = &e->bits[row * 4 + col / 8];
    unsigned char nv   = v ? (*b | mask) : (*b & (unsigned char)~mask);
    if (nv == *b) return;              /* rien n'a bouge : pas de redessin */
    *b = nv;

    hcicon_edit_sync(self.stack);
    [self setNeedsDisplay:YES];

    if (self.target && self.action)
        [NSApp sendAction:self.action to:self.target from:self];
}

/* Le premier clic decide de ce que fait tout le glisse : on pose de l'encre si
 * le pixel touche etait vide, on en enleve sinon. C'est le comportement des
 * gros bits de MacPaint, et il evite d'effacer ce qu'on vient de poser quand
 * la main repasse sur ses pas. */
- (void)mouseDown:(NSEvent *)e {
    int row, col;
    if (![self hitRow:&row col:&col forEvent:e]) return;
    _drawValue = [self pixelRow:row col:col] ? 0 : 1;
    [self paintRow:row col:col value:_drawValue];
}

- (void)mouseDragged:(NSEvent *)e {
    int row, col;
    if (![self hitRow:&row col:&col forEvent:e]) return;
    [self paintRow:row col:col value:_drawValue];
}

@end
