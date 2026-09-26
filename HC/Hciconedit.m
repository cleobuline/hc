/* HCiconedit.m — gros bits et operations sur les icones de la pile.
 *
 * Le catalogue et le choix appartiennent a IconGrid (HCpalettes) ; ici on ne
 * fait que modifier. La separation tient a une raison simple : le choix d'une
 * icone marche aussi sur les icones d'origine, la modification non — HCICONS
 * est const. C'est hcicon_edit_editable qui absorbe la difference, et c'est
 * le seul endroit qui la connaisse.
 */
#import "Hciconedit.h"
#import "HCicons.h"

#include <string.h>
#include <math.h>      /* lround : l'arrondi des composantes de couleur */

#define CELL   8                 /* cote d'un pixel d'icone, en points */
#define SIDE   (32 * CELL)

/* ==================== operations ==================== */

/* Le catalogue rend des HCIcon, la pile des StackIcon : deux structures
 * differentes qui portent le meme dessin. On passe par un StackIcon
 * temporaire pour n'avoir qu'UNE logique de copie, celle du noyau, plutot
 * qu'une seconde ecrite ici qui oublierait la couleur un jour. */
static void hc_icon_copie_dessin_hcicon(struct StackIcon *dst, const HCIcon *src)
{
    struct StackIcon tmp;
    memset(&tmp, 0, sizeof tmp);
    memcpy(tmp.bits, src->bits, HC_ICON_BYTES);
    tmp.couleur = src->couleur;          /* emprunte, jamais libere ici */
    hc_icon_copie_dessin(dst, &tmp);
}

/* Pile a laquelle le catalogue est actuellement lie. */
static Object *gBoundStack = NULL;

void hcicon_edit_sync(Object *stack)
{
    gBoundStack = stack;
    hcicon_use_stack_icons(stack ? stack->icons  : NULL,
                           stack ? stack->nicons : 0);
}

void hcicon_edit_bind(Object *stack)
{
    if (stack == gBoundStack) return;   /* deja la bonne : rien a faire */
    hcicon_edit_sync(stack);
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
    if (e) hc_icon_copie_dessin_hcicon(e, src);
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
    /* UNE SEULE PORTE POUR DUPLIQUER UN DESSIN, celle du noyau, qui a un
     * harnais. Ce fichier en avait une a lui — un memcpy des bits, puis la
     * couleur ajoutee a cote — et le presse-papiers en avait une troisieme.
     * Trois copies a la main, dont deux avaient deja oublie la couleur : une
     * de moins est une occasion de moins de l'oublier. */
    hc_icon_copie_dessin_hcicon(e, src);
    hcicon_edit_sync(stack);
    return nid;
}

void hcicon_edit_erase(Object *stack, int id)
{
    struct StackIcon *e = hcicon_edit_editable(stack, id);
    if (!e) return;
    memset(e->bits, 0, HC_ICON_BYTES);
    /* EFFACER VIDE AUSSI LES PIXELS EN COULEUR, mais GARDE LA PALETTE : on
     * efface un dessin, pas un jeu de couleurs. Redessiner avec les memes
     * teintes est le cas courant apres un effacement, et les avoir perdues
     * obligerait a les rechoisir une par une. */
    struct HcIconCouleur *c = hc_icon_couleur(e);
    if (c) memset(c->pixels, 0, HC_ICON_PIXELS);
    hcicon_edit_sync(stack);
}

/* bits_get et bits_set vivaient ici, pour la rotation. Elles sont parties
 * avec elle dans le noyau : les garder inutilisees ferait un avertissement,
 * et les garder « au cas ou » ferait deux facons de toucher les memes octets.
 */

void hcicon_edit_rotate(Object *stack, int id)
{
    struct StackIcon *e = hcicon_edit_editable(stack, id);
    if (!e) return;

    /* LA ROTATION EST PASSEE DANS LE NOYAU, couleur comprise.
     *
     * Elle y a un harnais qui DESSINE le resultat — et c'est ce qui a montre
     * que les deux chemins, couleur et noir et blanc, tournaient d'abord en
     * sens opposes. Une icone tournee du mauvais cote a exactement le meme
     * nombre d'encres : rien d'autre qu'un dessin ne pouvait le voir.
     *
     * Le sens n'a pas change : quart de tour horaire, le pixel qui arrive en
     * (ligne, colonne) vient de (31 - colonne, ligne). */
    hc_icon_tourne(e);
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

/* ---- couleur ----
 *
 * Toute la logique est dans le noyau (hc_icons.c) : elle y a un harnais, ce
 * que ce fichier n'aura jamais. Ce qui suit n'est que la porte, et elle
 * rend l'icone EDITABLE d'abord — une icone d'origine se recopie dans la pile
 * avant d'etre modifiee, sinon on peindrait dans une table const. */
int hcicon_edit_couleur(Object *stack, int id, int allume)
{
    struct StackIcon *e = hcicon_edit_editable(stack, id);
    if (!e) return 0;

    int etait = hc_icon_couleur(e) != NULL;
    if (etait == (allume != 0)) return 0;

    if (allume) {
        /* Le noir pour l'index 1 : c'est ce que l'icone montrait deja, donc
         * allumer la couleur ne change RIEN a l'ecran tant qu'on n'a pas
         * peint. Une icone qui changerait d'aspect au moment ou l'on coche
         * la case ferait croire qu'on a casse quelque chose. */
        if (!hc_icon_couleur_depuis_bits(e, 0, 0, 0)) return 0;
    } else {
        hc_icon_couleur_ote(e);
    }
    hcicon_edit_sync(stack);
    return 1;
}

int hcicon_edit_est_couleur(Object *stack, int id)
{
    /* hcicon_find plutot que hc_icon_get : une icone d'origine n'est pas dans
     * la pile, et la question a quand meme une reponse — non. */
    struct StackIcon *e = hc_icon_get(stack, id);
    return e && hc_icon_couleur(e) != NULL;
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

/* L'icone EDITABLE, ou NULL. On passe par la pile et non par le catalogue :
 * le catalogue est une copie de travail, et peindre dedans ne se sauverait
 * pas. */
- (struct StackIcon *)iconeDeLaPile {
    return self.stack ? hc_icon_get(self.stack, self.iconId) : NULL;
}

/* La couleur d'un pixel, ou nil s'il est transparent ou si l'icone est en
 * noir et blanc. Meme regle que hcicon_draw : un index hors palette compte
 * pour transparent, de sorte qu'un fichier abime laisse un trou visible
 * plutot qu'un aplat noir qu'on prendrait pour du dessin. */
- (NSColor *)couleurRow:(int)row col:(int)col {
    struct StackIcon *e = [self iconeDeLaPile];
    const struct HcIconCouleur *c = e ? hc_icon_couleur(e) : NULL;
    if (!c) return nil;
    int idx = c->pixels[row * 32 + col];
    if (idx <= 0 || idx >= c->ncouleurs) return nil;
    return [NSColor colorWithCalibratedRed:c->palette[idx][0] / 255.0
                                     green:c->palette[idx][1] / 255.0
                                      blue:c->palette[idx][2] / 255.0
                                     alpha:1.0];
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;

    [[NSColor whiteColor] setFill];
    NSRectFill([self bounds]);

    /* L'encre d'abord, la grille par dessus : l'inverse noierait les traits
     * gris sous les pixels noirs, et l'on ne compterait plus les cases. */
    struct StackIcon *edit = [self iconeDeLaPile];
    if (edit && hc_icon_couleur(edit)) {
        for (int row = 0; row < 32; row++)
            for (int col = 0; col < 32; col++) {
                NSColor *co = [self couleurRow:row col:col];
                if (!co) continue;
                [co setFill];
                NSRectFill(NSMakeRect(col * CELL, row * CELL, CELL, CELL));
            }
    } else {
        [[NSColor blackColor] setFill];
        for (int row = 0; row < 32; row++)
            for (int col = 0; col < 32; col++)
                if ([self pixelRow:row col:col])
                    NSRectFill(NSMakeRect(col * CELL, row * CELL, CELL, CELL));
    }

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

    if (hc_icon_couleur(e)) {
        /* EN COULEUR, « v » ne vaut plus 1 ou 0 mais « pose » ou « efface ».
         * Poser met l'index choisi dans la bande de palette ; effacer met
         * zero, la transparence — la gomme et la transparence sont le meme
         * geste, et c'est ce qui evite d'avoir un masque a tenir a part. */
        int idx = v ? self.couleurCourante : 0;
        if (idx < 0) idx = 0;
        if (hc_icon_pixel_lu(e, col, row) == idx) return;   /* rien n'a bouge */
        /* hc_icon_pixel_pose met la silhouette a jour : `bits` ne peut donc
         * pas dater d'un coup de pinceau, et le bouton qui porte l'icone
         * reste juste meme au milieu d'un glisse. */
        hc_icon_pixel_pose(e, col, row, idx);
    } else {
        unsigned char mask = (unsigned char)(0x80 >> (col & 7));
        unsigned char *b   = &e->bits[row * 4 + col / 8];
        unsigned char nv   = v ? (*b | mask) : (*b & (unsigned char)~mask);
        if (nv == *b) return;          /* rien n'a bouge : pas de redessin */
        *b = nv;
    }

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

    /* EN COULEUR, le bascule se fait sur la couleur CHOISIE, pas sur l'encre.
     * Cliquer un pixel qui porte deja la teinte courante l'efface ; cliquer
     * n'importe quel autre la pose. On garde donc le geste de MacPaint — la
     * main qui repasse sur ses pas n'efface pas ce qu'elle vient de poser —
     * en le transposant a une palette. */
    struct StackIcon *edit = [self iconeDeLaPile];
    if (edit && hc_icon_couleur(edit))
        _drawValue = (hc_icon_pixel_lu(edit, col, row) == self.couleurCourante)
                     ? 0 : 1;
    else
        _drawValue = [self pixelRow:row col:col] ? 0 : 1;

    [self paintRow:row col:col value:_drawValue];
}

- (void)mouseDragged:(NSEvent *)e {
    int row, col;
    if (![self hitRow:&row col:&col forEvent:e]) return;
    [self paintRow:row col:col value:_drawValue];
}

@end

/* ==================== la bande de palette ==================== */

#define PAL_CASE  22            /* cote d'une case, en points */
#define PAL_COLS  11
#define PAL_ROWS  2
#define PAL_MAX   (PAL_COLS * PAL_ROWS)

@implementation HCIconPalette

+ (CGFloat)height { return PAL_ROWS * PAL_CASE; }

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)event { (void)event; return YES; }

- (struct StackIcon *)icone {
    return self.stack ? hc_icon_get(self.stack, self.iconId) : NULL;
}

- (const struct HcIconCouleur *)couleurs {
    struct StackIcon *e = [self icone];
    return e ? hc_icon_couleur(e) : NULL;
}

/* Combien de cases la bande MONTRE : les couleurs de l'icone, plus celle du
 * « + », sans jamais deborder les deux rangees. Une palette plus riche reste
 * entiere dans le fichier et dans l'icone ; c'est le CHOIX A LA MAIN qui
 * s'arrete la, pas le dessin. */
- (int)nbCases {
    const struct HcIconCouleur *c = [self couleurs];
    if (!c) return 0;
    int n = c->ncouleurs + 1;                   /* +1 pour le « + » */
    return n > PAL_MAX ? PAL_MAX : n;
}

- (NSRect)rectDe:(int)i {
    return NSMakeRect((i % PAL_COLS) * PAL_CASE, (i / PAL_COLS) * PAL_CASE,
                      PAL_CASE - 2, PAL_CASE - 2);
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    [[NSColor windowBackgroundColor] setFill];
    NSRectFill([self bounds]);

    const struct HcIconCouleur *c = [self couleurs];
    if (!c) {
        NSDictionary *at = @{ NSFontAttributeName: [NSFont systemFontOfSize:10],
                              NSForegroundColorAttributeName: [NSColor disabledControlTextColor] };
        [@"(icône en noir et blanc)" drawAtPoint:NSMakePoint(2, 4) withAttributes:at];
        return;
    }

    int n = [self nbCases];
    for (int i = 0; i < n; i++) {
        NSRect r = [self rectDe:i];

        if (i == 0) {
            /* LA TRANSPARENCE SE MONTRE, elle ne se cache pas : c'est la
             * gomme, et il faut pouvoir la choisir comme une couleur. Un
             * damier gris, la convention de tout le monde pour « rien ». */
            [[NSColor whiteColor] setFill];
            NSRectFill(r);
            [[NSColor colorWithWhite:0.80 alpha:1.0] setFill];
            for (int y = 0; y < (int)r.size.height; y += 4)
                for (int x = (y / 4 % 2) * 4; x < (int)r.size.width; x += 8)
                    NSRectFill(NSMakeRect(r.origin.x + x, r.origin.y + y, 4, 4));
        } else if (i < c->ncouleurs) {
            [[NSColor colorWithCalibratedRed:c->palette[i][0] / 255.0
                                       green:c->palette[i][1] / 255.0
                                        blue:c->palette[i][2] / 255.0
                                       alpha:1.0] setFill];
            NSRectFill(r);
        } else {
            /* La case d'ajout. */
            [[NSColor controlBackgroundColor] setFill];
            NSRectFill(r);
            NSDictionary *at = @{ NSFontAttributeName: [NSFont systemFontOfSize:14],
                                  NSForegroundColorAttributeName: [NSColor controlTextColor] };
            [@"+" drawAtPoint:NSMakePoint(r.origin.x + 5, r.origin.y + 1) withAttributes:at];
        }

        /* Le contour, et le cadre epais de la case choisie. Sans lui on ne
         * sait pas avec quoi l'on peint — et c'est la premiere question qu'on
         * se pose en reprenant un dessin. */
        int choisie = (self.grille && self.grille.couleurCourante == i);
        [(choisie ? [NSColor controlTextColor]
                  : [NSColor colorWithWhite:0.55 alpha:1.0]) setStroke];
        NSBezierPath *p = [NSBezierPath bezierPathWithRect:
            NSInsetRect(r, 0.5, 0.5)];
        [p setLineWidth:choisie ? 2 : 1];
        [p stroke];
    }
}

- (int)caseSousEvent:(NSEvent *)e {
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    int col = (int)floor(p.x / PAL_CASE), row = (int)floor(p.y / PAL_CASE);
    if (col < 0 || col >= PAL_COLS || row < 0 || row >= PAL_ROWS) return -1;
    int i = row * PAL_COLS + col;
    return i < [self nbCases] ? i : -1;
}

/* Le selecteur du systeme, ouvert pour l'index en cours d'edition.
 *
 * NSColorPanel est un singleton : on lui repose cible et action a CHAQUE
 * ouverture, et l'action verifie ce qu'elle touche avant d'ecrire. Sans cette
 * verification, une palette ouverte puis une icone changee ferait peindre la
 * couleur dans l'icone d'a cote — le genre de defaut qui ne se voit qu'une
 * fois le fichier enregistre. */
static int gPalIndexEnCours = 0;

- (void)ouvreSelecteurPour:(int)index {
    gPalIndexEnCours = index;
    NSColorPanel *cp = [NSColorPanel sharedColorPanel];
    const struct HcIconCouleur *c = [self couleurs];
    if (c && index >= 1 && index < c->ncouleurs)
        [cp setColor:[NSColor colorWithCalibratedRed:c->palette[index][0] / 255.0
                                               green:c->palette[index][1] / 255.0
                                                blue:c->palette[index][2] / 255.0
                                               alpha:1.0]];
    [cp setTarget:self];
    [cp setAction:@selector(couleurChoisie:)];
    [cp setContinuous:YES];
    [cp makeKeyAndOrderFront:nil];
}

- (void)couleurChoisie:(id)sender {
    struct StackIcon *e = [self icone];
    if (!e || !hc_icon_couleur(e)) return;            /* plus rien a peindre */
    if (gPalIndexEnCours < 1) return;                 /* jamais l'index 0 */

    NSColor *co = [[(NSColorPanel *)sender color]
                     colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
    if (!co) return;

    hc_icon_palette_pose(e, gPalIndexEnCours,
                         (unsigned char)lround([co redComponent]   * 255.0),
                         (unsigned char)lround([co greenComponent] * 255.0),
                         (unsigned char)lround([co blueComponent]  * 255.0));
    hcicon_edit_sync(self.stack);
    [self setNeedsDisplay:YES];
    if (self.grille) [self.grille setNeedsDisplay:YES];
    if (self.target && self.action)
        [NSApp sendAction:self.action to:self.target from:self];
}

- (void)mouseDown:(NSEvent *)e {
    int i = [self caseSousEvent:e];
    if (i < 0) return;

    const struct HcIconCouleur *c = [self couleurs];
    if (!c) return;

    if (i >= c->ncouleurs) {
        /* La case « + » : on ajoute une couleur et on la choisit aussitot.
         * Le gris moyen plutot que le noir — le noir est deja l'index 1 sur
         * une icone qui vient du noir et blanc, et ajouter un second noir
         * donnerait deux cases qu'on ne distingue pas. */
        struct StackIcon *ic = [self icone];
        int nouv = hc_icon_palette_index(ic, 0x80, 0x80, 0x80);
        if (nouv <= 0) return;                 /* palette pleine */
        hcicon_edit_sync(self.stack);
        if (self.grille) self.grille.couleurCourante = nouv;
        [self setNeedsDisplay:YES];
        [self ouvreSelecteurPour:nouv];
        if (self.target && self.action)
            [NSApp sendAction:self.action to:self.target from:self];
        return;
    }

    if (self.grille) {
        self.grille.couleurCourante = i;
        [self.grille setNeedsDisplay:YES];
    }
    [self setNeedsDisplay:YES];

    /* Double-clic sur une couleur existante : on la MODIFIE. L'index 0 en est
     * exclu — la transparence n'a pas de teinte a changer. */
    if ([e clickCount] >= 2 && i >= 1) [self ouvreSelecteurPour:i];
}

@end

/* ==================== coller une image ==================== */

int hcicon_edit_colle(Object *stack, int id)
{
    NSArray *img = [[NSPasteboard generalPasteboard]
                      readObjectsForClasses:@[[NSImage class]] options:nil];
    if ([img count] == 0) return 0;
    NSImage *src = img[0];

    struct StackIcon *e = hcicon_edit_editable(stack, id);
    if (!e) return 0;

    /* LE TAMPON EST PREMULTIPLIE, et il faut le defaire avant de lire.
     *
     * CoreGraphics ne sait pas dessiner dans un contexte non premultiplie :
     * demander NSBitmapFormatAlphaNonpremultiplied rend un contexte nul, et
     * le collage echouerait sans rien dire. On prend donc le format par
     * defaut et l'on divise par l'alpha a la lecture. Sans cette division,
     * un pixel a demi transparent arriverait ASSOMBRI — et comme on seuille
     * l'alpha juste apres, il resterait sombre sans raison visible. */
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:HC_ICON_COTE
                      pixelsHigh:HC_ICON_COTE
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSCalibratedRGBColorSpace
                     bytesPerRow:HC_ICON_COTE * 4
                    bitsPerPixel:32];
    if (!rep) return 0;

    NSGraphicsContext *ctx =
        [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!ctx) return 0;

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];

    /* AUCUNE INTERPOLATION, ET C'EST LE POINT LE PLUS IMPORTANT DE CETTE
     * FONCTION.
     *
     * Par defaut Cocoa LISSE quand il met a l'echelle. Sur une photo c'est ce
     * qu'on veut ; sur une TRAME — un damier de deux couleurs, comme en
     * produit la palette de motifs — c'est un desastre : chaque pixel du
     * resultat devient une MOYENNE de ses voisins, et deux couleurs franches
     * en donnent des dizaines d'intermediaires. On collait une trame rouge et
     * blanche et l'on recuperait quarante roses.
     *
     * Releve a l'usage, sur une vraie trame : « le coller rajoute des
     * couleurs quand j'ai copie des trames en couleurs ». La decoupe mediane
     * n'y etait pour rien — elle recevait deja les quarante roses.
     *
     * Le plus proche voisin garde donc les couleurs EXACTES de la source, ce
     * qui rend le collage sans perte dans le cas qui compte le plus : un bout
     * de dessin fait dans HC, avec les motifs de HC. */
    [[NSGraphicsContext currentContext]
        setImageInterpolation:NSImageInterpolationNone];

    [[NSColor clearColor] setFill];
    NSRectFill(NSMakeRect(0, 0, HC_ICON_COTE, HC_ICON_COTE));

    /* TENIR SANS SE DEFORMER, et centre. Une image large ecrasee au carre
     * devient illisible a cette taille, et c'est justement a cette taille
     * qu'on a le moins de pixels a perdre. */
    NSSize sz = [src size];
    CGFloat ech = (sz.width <= 0 || sz.height <= 0) ? 1.0
                : fmin(HC_ICON_COTE / sz.width, HC_ICON_COTE / sz.height);
    CGFloat w = sz.width * ech, h = sz.height * ech;
    [src drawInRect:NSMakeRect((HC_ICON_COTE - w) / 2, (HC_ICON_COTE - h) / 2,
                               w, h)
           fromRect:NSZeroRect
          operation:NSCompositingOperationSourceOver
           fraction:1.0];
    [NSGraphicsContext restoreGraphicsState];

    unsigned char *src_px = [rep bitmapData];
    if (!src_px) return 0;

    static unsigned char rvba[HC_ICON_PIXELS * 4];
    for (int i = 0; i < HC_ICON_PIXELS; i++) {
        int a = src_px[i * 4 + 3];
        for (int k = 0; k < 3; k++) {
            int v = src_px[i * 4 + k];
            /* defaire la premultiplication ; a == 0 n'a pas de couleur a
             * retrouver, et le pixel sera transparent de toute facon */
            rvba[i * 4 + k] = (a > 0 && a < 255)
                ? (unsigned char)((v * 255 + a / 2) / a)
                : (unsigned char)v;
        }
        rvba[i * 4 + 3] = (unsigned char)a;
    }

    int n = hc_icon_colle_rvba(e, rvba);
    if (n) hcicon_edit_sync(stack);
    return n;
}
