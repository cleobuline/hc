#import "HCtext.h"
#include <stdlib.h>   /* getenv, pour la trace HC_RUNS_DEBUG */

/* ═══ Police, style et geometrie du texte ════════════════════════════════════
 *
 * Extrait de HCview.m sans aucune modification de comportement : les memes
 * fonctions, dans le meme ordre, avec les memes commentaires. Seul change
 * l'emplacement, et le fait que dix d'entre elles ne sont plus « static »
 * puisque HCview.m les appelle depuis l'exterieur.
 *
 * Ce groupe ne depend d'aucune variable globale de l'interface, ni d'aucune
 * methode de HCView : il ne parle qu'au noyau et a Cocoa. C'est ce qui en
 * faisait le premier morceau a sortir. */

static NSFont *font_with_traits(NSFont *f, BOOL bold, BOOL italic, BOOL *faux);
static NSFont *font_by_loose_name(NSString *want, CGFloat sz);
static NSMutableDictionary *style_attrs(int style, NSFont *base, NSColor *color);

/* La police d'un objet SANS ses traits : famille et corps seulement. Les
 * traits sont posés ensuite par style_attrs, qui sait aussi les simuler.
 * Les garder séparés évite la double source de vérité entre le nom de fonte
 * et les bits de style, qui nous a déjà coûté cher. */
/* Vrai pendant qu'on construit la chaîne DESTINÉE À L'ÉDITEUR.
 *
 * La même chaîne sert au dessin et à la NSTextView, mais les deux n'espacent
 * pas les lignes pareil : le dessin ajoute l'interligne recommandé par la
 * police, la NSTextView non. Un pixel par ligne, mesuré. On ne peut donc pas
 * leur donner exactement le même style de paragraphe — d'où ce drapeau, qui
 * dit lequel des deux on sert. */
BOOL gForEditor = NO;

NSFont *obj_base_font(Object *o, CGFloat defSize) {
    CGFloat sz = o->textsize > 0 ? o->textsize : defSize;
    NSFont *f = nil;
    if (o->textfont && *o->textfont)
        f = font_by_loose_name([NSString stringWithUTF8String:o->textfont], sz);
    if (!f) f = [NSFont systemFontOfSize:sz];
    return f;
}

NSFont *obj_font(Object *o, CGFloat defSize) {
    /* Même chemin que les plages de style : convertFont:toHaveTrait: rend la
     * fonte système inchangée et sans rien dire, si bien qu'un champ resté
     * dans la police par défaut ne pouvait pas passer au gras — et que le
     * panneau de polices, en recevant une fonte sans trait, croyait le gras
     * éteint et l'écrasait au coup suivant. */
    return font_with_traits(obj_base_font(o, defSize),
                            (o->textstyle & HC_BOLD)   ? YES : NO,
                            (o->textstyle & HC_ITALIC) ? YES : NO, NULL);
}





 

/* ---- attributs de texte d'un objet ---- */
/* Attributs de dessin d'un objet — nom de bouton, étiquette de case à cocher,
 * ligne courante d'un popup. Passe désormais par style_attrs, si bien qu'un
 * bouton rend le creux, l'ombré, l'approche et les traits simulés exactement
 * comme un champ. Auparavant il ne connaissait que le souligné, et « set the
 * textStyle of button "x" to outline » restait sans effet visible. */
NSDictionary *obj_attrs(Object *o, CGFloat defSize, NSColor *color) {
    return style_attrs(o->textstyle, obj_base_font(o, defSize), color);
}
/* `group` n'a aucun equivalent Cocoa : c'est une semantique HyperCard, pas un
 * rendu. On le porte comme attribut personnalise, sinon il disparaitrait au
 * premier aller-retour par l'editeur. */
static NSString * const kHCGroupAttribute = @"HCGroup";

/* Le gras synthétique et le contour se disputaient NSStrokeWidth : poser l'un
 * effaçait l'autre, et « gras + creux » revenait creux tout court. On garde
 * donc une trace explicite du gras simulé, au lieu de la déduire du signe du
 * trait. */
static NSString * const kHCFauxBoldAttribute = @"HCFauxBold";

/* ---- texte attribué d'un champ : les plages de style ----
 *
 * Le noyau tient des plages de caractères (hc_run_count / hc_run_at). On part
 * des attributs du champ entier, puis on surcharge chaque plage. Les plages
 * arrivent triées et sans recouvrement, il n'y a donc aucune imbrication à
 * démêler ici.
 */

/* Les plages du noyau sont des décalages en OCTETS dans le texte UTF-8, alors
 * que NSString compte en unités UTF-16. C'est identique tant que le texte est
 * en ASCII — le calendrier l'est — et faux dès le premier accent. */
static NSUInteger utf16_from_byte(const char *utf8, int byteoff)
{
    if (byteoff <= 0) return 0;
    NSString *pre = [[NSString alloc] initWithBytes:utf8
                                             length:(NSUInteger)byteoff
                                           encoding:NSUTF8StringEncoding];
    return pre ? [pre length] : (NSUInteger)byteoff;   /* coupe au milieu d'un
                                                        * caractère : repli */
}

/* Applique gras/italique de façon fiable.
 *
 * NSFontManager convertFont:toHaveTrait: échoue silencieusement sur la police
 * système : San Francisco n'est pas exposée par nom de famille, et on récupère
 * la même fonte sans le moindre avertissement. On passe donc par le
 * descripteur et ses traits symboliques, qui, eux, la connaissent. Si même
 * cela échoue — police sans variante grasse — on grossit le trait, ce que le
 * Macintosh appelait le gras synthétique. */
static NSFont *font_with_traits(NSFont *f, BOOL bold, BOOL italic, BOOL *faux)
{
    if (faux) *faux = NO;
    if (!f || (!bold && !italic)) return f;

    NSFontDescriptorSymbolicTraits want = 0;
    if (bold)   want |= NSFontDescriptorTraitBold;
    if (italic) want |= NSFontDescriptorTraitItalic;

    NSFontDescriptor *fd =
        [[f fontDescriptor] fontDescriptorWithSymbolicTraits:
            [[f fontDescriptor] symbolicTraits] | want];
    NSFont *nf = [NSFont fontWithDescriptor:fd size:[f pointSize]];

    if (!nf) {                              /* repli : l'ancienne méthode */
        NSFontManager *fm = [NSFontManager sharedFontManager];
        nf = f;
        if (bold)   nf = [fm convertFont:nf toHaveTrait:NSBoldFontMask];
        if (italic) nf = [fm convertFont:nf toHaveTrait:NSItalicFontMask];
    }
    if (!nf) nf = f;

    /* A-t-on vraiment obtenu le gras ? Sinon on le simulera au trait.
     * L'italique n'a pas d'équivalent : sur une famille sans variante penchée,
     * « to italic » reste stocké dans le noyau mais ne se voit pas — c'est le
     * choix retenu, plutôt qu'une inclinaison synthétique. */
    if (bold && faux) {
        NSFontDescriptorSymbolicTraits got = [[nf fontDescriptor] symbolicTraits];
        if (!(got & NSFontDescriptorTraitBold)) *faux = YES;
    }
    return nf;
}

/* Cocoa veut le nom exact : « monaco » ne rend rien, « Monaco » rend la
 * police. HyperCard, lui, se moquait de la casse — les scripts d'origine
 * écrivent « geneva » aussi souvent que « Geneva ». On tente donc le nom tel
 * quel, puis on le cherche parmi les familles installées en ignorant la
 * casse. Sans cela un « set the textFont … to "monaco" » était accepté par le
 * noyau, stocké, relu — et restait invisible à l'écran. */
static NSFont *font_by_loose_name(NSString *want, CGFloat sz)
{
    if (![want length]) return nil;

    /* Les noms commençant par un point sont ceux des polices système —
     * « .SFNS-Regular », « .AppleSystemUIFont ». CoreText refuse de les servir
     * par leur nom et rend du Times, en le signalant à chaque appel :
     *
     *   Client requested name ".SFNS-Regular", it will get Times-Roman
     *
     * On rend donc directement la police système, qui est ce que ce nom
     * désigne. Le cas survient avec les piles enregistrées avant que l'on ne
     * stocke le nom de FAMILLE plutôt que le nom PostScript : leurs plages
     * portent encore l'ancien nom, et doivent continuer de s'afficher. */
    if ([want hasPrefix:@"."]) return [NSFont systemFontOfSize:sz];

    NSFont *f = [NSFont fontWithName:want size:sz];
    if (f) return f;

    NSFontManager *fm = [NSFontManager sharedFontManager];

    /* Par famille : « Monaco », « Times New Roman »… C'est ce qu'écrivent les
     * scripts HyperCard. fontWithName: accepte un nom de famille, mais exige
     * la casse exacte, d'où la comparaison souple. */
    for (NSString *fam in [fm availableFontFamilies])
        if ([fam caseInsensitiveCompare:want] == NSOrderedSame) {
            if ((f = [NSFont fontWithName:fam size:sz])) return f;
            /* Famille reconnue mais sans membre au nom de la famille : on
             * prend son premier membre (« Helvetica Neue » -> « HelveticaNeue-
             * Regular »). availableMembersOfFontFamily rend des tableaux dont
             * le premier élément est le nom de fonte. */
            for (NSArray *m in [fm availableMembersOfFontFamily:fam]) {
                if ([m count] < 1) continue;
                if ((f = [NSFont fontWithName:m[0] size:sz])) return f;
            }
        }

    /* Dernier recours : un nom de FONTE et non de famille — « Courier-Bold »,
     * que certains scripts écrivent tel quel. */
    for (NSString *nm in [fm availableFonts])
        if ([nm caseInsensitiveCompare:want] == NSOrderedSame)
            if ((f = [NSFont fontWithName:nm size:sz])) return f;
    return nil;
}

/* La police d'une plage : son nom et son corps à elle, sur lesquels viendront
 * se poser les traits. `fallback` est la police du champ, utilisée si le nom
 * demandé n'existe pas sur cette machine — HyperCard faisait de même plutôt
 * que de rendre du vide. */
static NSFont *run_base_font(const char *name, int size, NSFont *fallback)
{
    CGFloat sz = size > 0 ? (CGFloat)size
                          : (fallback ? [fallback pointSize] : 12);
    if (name && *name) {
        NSFont *f = font_by_loose_name([NSString stringWithUTF8String:name], sz);
        if (f) return f;
        if (getenv("HC_RUNS_DEBUG"))
            NSLog(@"[runs]   police introuvable : \"%s\" -> repli sur le champ",
                  name);
    }
    if (!fallback) return [NSFont systemFontOfSize:sz];
    if (sz == [fallback pointSize]) return fallback;
    return [NSFont fontWithDescriptor:[fallback fontDescriptor] size:sz];
}

/* Traduit un masque de style HyperCard en attributs Cocoa. C'est LE seul
 * endroit où cette traduction est écrite : le champ y passe par plage, le
 * bouton par son nom entier. Les avoir en double, c'était garantir qu'un
 * effet ajouté d'un côté manquerait de l'autre — ce qui était le cas du
 * creux et de l'ombré, absents des boutons. */
static NSMutableDictionary *style_attrs(int style, NSFont *base, NSColor *color)
{
    NSMutableDictionary *at = [NSMutableDictionary dictionary];
    if (!color) color = [NSColor blackColor];
    at[NSForegroundColorAttributeName] = color;

    BOOL faux = NO;
    NSFont *f = font_with_traits(base,
                                 (style & HC_BOLD)   ? YES : NO,
                                 (style & HC_ITALIC) ? YES : NO, &faux);
    if (f) at[NSFontAttributeName] = f;

    /* Un seul NSStrokeWidth pour deux effets, il faut donc trancher une fois :
     *   creux            -> trait positif (contour seul)
     *   creux ET gras    -> trait positif plus épais, comme le faisait le Mac
     *   gras simulé seul -> trait négatif (remplir ET contourner)
     * Le gras simulé est en plus marqué par son propre attribut : le signe du
     * trait ne suffit plus à le retrouver quand le creux l'a emporté. */
    if (faux) at[kHCFauxBoldAttribute] = @(1);

    double stroke = 0.0;
    if (style & HC_OUTLINE)  stroke = (faux || (style & HC_BOLD)) ? 5.0 : 3.0;
    else if (faux)           stroke = -3.0;
    if (stroke != 0.0) {
        at[NSStrokeWidthAttributeName] = @(stroke);
        at[NSStrokeColorAttributeName] = color;
    }

    if (style & HC_UNDERLINE)
        at[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);

    if (style & HC_SHADOW) {
        NSShadow *sh = [[NSShadow alloc] init];
        [sh setShadowOffset:NSMakeSize(1, -1)];
        [sh setShadowBlurRadius:0];
        [sh setShadowColor:[NSColor grayColor]];
        at[NSShadowAttributeName] = sh;
    }

    /* Condense et extend se rendent par l'approche : c'est ce que faisait le
     * Macintosh, qui rapprochait ou écartait les glyphes sans changer de fonte. */
    if (style & HC_CONDENSE) at[NSKernAttributeName] = @(-1.0);
    if (style & HC_EXTEND)   at[NSKernAttributeName] = @(1.5);

    /* HC_GROUP ne se voit pas, mais il doit survivre a un aller-retour par
     * l'editeur : on le porte comme attribut personnalise. */
    if (style & HC_GROUP) at[kHCGroupAttribute] = @(1);

    return at;
}

static void apply_run_style(NSMutableAttributedString *as, NSRange r,
                            int style, NSFont *base, NSColor *color)
{
    [as addAttributes:style_attrs(style, base, color) range:r];
}

/* Pose la surbrillance de sélection SUR la chaîne attribuée.
 *
 * C'est AppKit qui placera le fond, avec exactement la mise en page qui sert
 * à tracer le texte : la surbrillance ne peut donc pas se décaler. Les deux
 * tentatives précédentes calculaient sa position à part — d'abord en
 * retraçant le texte par-dessus, puis avec un NSLayoutManager monté pour
 * l'occasion — et les deux tombaient à côté, parce qu'une seconde mise en
 * page n'est jamais tout à fait la première.
 *
 * Vidéo inverse, comme l'original en noir et blanc : fond noir, texte blanc. */
static void apply_selection_highlight(NSMutableAttributedString *as, Object *o)
{
    if (!o || o->type != OBJ_FIELD) return;

    /* Jamais sur la chaîne destinée à l'ÉDITEUR.
     *
     * La NSTextView dessine sa propre sélection ; la nôtre ferait double
     * emploi. Pire, ses attributs — texte blanc sur fond noir — restaient dans
     * le NSTextView, et la relecture des plages à la fermeture les prenait
     * pour des couleurs choisies par l'utilisateur : le texte sélectionné
     * ressortait en blanc. */
    if (gForEditor) return;

    Object *sel = NULL; int start = 0, len = 0;
    hc_get_selection(&sel, &start, &len);
    if (sel != o || len <= 0) return;

    const char *tx = hc_field_text(o);
    NSUInteger n  = [as length];
    NSUInteger u0 = utf16_from_byte(tx, start);
    NSUInteger u1 = utf16_from_byte(tx, start + len);
    if (u0 >= n) return;
    if (u1 > n)  u1 = n;
    if (u1 <= u0) return;

    NSRange r = NSMakeRange(u0, u1 - u0);
    [as addAttribute:NSBackgroundColorAttributeName value:[NSColor blackColor] range:r];
    [as addAttribute:NSForegroundColorAttributeName value:[NSColor whiteColor] range:r];
}

/* ═══ MÉMOIRE DES CHAÎNES ATTRIBUÉES ════════════════════════════════════
 *
 * Un champ de mille lignes coûtait près d'une seconde par redessin, mesuré.
 * La raison n'est pas le nombre d'objets sur la carte — le noyau, lui, ne met
 * qu'une microseconde par clic — mais le fait qu'un seul champ refaisait DEUX
 * mises en page complètes de tout son texte à chaque image :
 *
 *   field_text_height, pour la barre de défilement, construisait la chaîne
 *   attribuée puis mesurait la hauteur de MILLE lignes ;
 *   puis le dessin reconstruisait la même chaîne et remettait tout en page,
 *   pour n'en afficher que vingt.
 *
 * On mémorise donc le résultat, en le comparant à ce dont il dépend : le
 * texte, la police, le corps, le style, l'interligne fixe — et gForEditor,
 * qui change le style de paragraphe. L'oublier ferait partager une entrée
 * entre l'éditeur et le dessin, dont les interlignes diffèrent d'un pixel
 * par ligne : c'est justement ce que ce drapeau existe pour distinguer.
 *
 * Comparer les entrées plutôt que s'abonner à une notification : le noyau ne
 * prévient l'hôte que pour le TEXTE d'un champ (field_changed), pas pour ses
 * propriétés de style. Une mémoire fondée sur une notification laisserait
 * donc un champ figé après « set the textFont of field 1 to Monaco ».
 *
 * La chaîne rendue est immuable et partagée : aucun appelant ne la modifie,
 * tous la recopient (initWithAttributedString: ou setAttributedString:).
 *
 * `at` n'entre pas dans la clé : tous les appelants le dérivent de l'objet
 * par obj_attrs(o, 12, noir), donc les propriétés de l'objet le déterminent
 * entièrement. */
#define HC_TEXTE_MEMO 6

static NSMutableArray<NSDictionary *> *g_memo_champs = nil;

/* La signature de ce qui détermine le rendu d'un champ.
 *
 * Deux entrées ne sautent pas aux yeux et manquaient à la première version,
 * ce qui a coûté la surbrillance des champs-listes :
 *
 *   LA SÉLECTION. apply_selection_highlight l'incruste DANS la chaîne
 *   attribuée — fond noir, texte blanc — parce qu'une surbrillance calculée
 *   à part tombait à côté (voir son commentaire). Elle change donc le rendu
 *   sans toucher au texte : « selectline » n'aurait plus rien affiché.
 *
 *   LES PLAGES DE STYLE. « set the textStyle of char 5 to 10 of field 1 to
 *   bold » ne change ni le texte ni le style du champ entier. Sans elles
 *   dans la clé, le gras ne serait apparu qu'au prochain changement de
 *   texte. On les résume en une chaîne : elles sont peu nombreuses, et
 *   c'est le prix d'une comparaison sûre. */
static NSString *champ_plages(Object *o)
{
    int n = hc_run_count(o);
    if (n <= 0) return @"";
    NSMutableString *r = [NSMutableString stringWithCapacity:(NSUInteger)n * 16];
    for (int i = 0; i < n; i++) {
        int a = 0, l = 0, st = 0, sz = 0, co = HC_COLOR_INHERIT;
        const char *fn = NULL;
        if (!hc_run_attrs_color(o, i, &a, &l, &st, &sz, &fn, &co)) continue;
        [r appendFormat:@"%d/%d/%d/%d/%s/%d;",
                        a, l, st, sz, fn ? fn : "", co];
    }
    return r;
}

static NSDictionary *champ_signature(Object *o, NSString *s)
{
    Object *sel = NULL; int start = 0, len = 0;
    hc_get_selection(&sel, &start, &len);

    return @{ @"objet"   : [NSValue valueWithPointer:o],
              @"texte"   : s ?: @"",
              @"police"  : [NSString stringWithUTF8String:
                              (o->textfont && *o->textfont) ? o->textfont : ""],
              @"corps"   : @(o->textsize),
              @"style"   : @(o->textstyle),
              @"interl"  : @(o->fixed_lh),
              @"editeur" : @(gForEditor),
              @"seldeb"  : @(sel == o ? start : -1),
              @"sellen"  : @(sel == o ? len   : 0),
              @"plages"  : champ_plages(o) };
}

static BOOL champ_signature_egale(NSDictionary *a, NSDictionary *b)
{
    return [a[@"objet"]   isEqual:b[@"objet"]]
        && [a[@"corps"]   isEqual:b[@"corps"]]
        && [a[@"style"]   isEqual:b[@"style"]]
        && [a[@"interl"]  isEqual:b[@"interl"]]
        && [a[@"editeur"] isEqual:b[@"editeur"]]
        && [a[@"seldeb"]  isEqual:b[@"seldeb"]]
        && [a[@"sellen"]  isEqual:b[@"sellen"]]
        && [a[@"police"]  isEqualToString:b[@"police"]]
        && [a[@"plages"]  isEqualToString:b[@"plages"]]
        && [a[@"texte"]   isEqualToString:b[@"texte"]];
}

static NSAttributedString *field_attr_string_construit(Object *o, NSString *s,
                                                       NSDictionary *at);

NSAttributedString *field_attr_string(Object *o, NSString *s, NSDictionary *at)
{
    if (!g_memo_champs) g_memo_champs = [[NSMutableArray alloc] init];

    NSDictionary *sig = champ_signature(o, s);

    for (NSUInteger i = 0; i < [g_memo_champs count]; i++) {
        NSDictionary *e = g_memo_champs[i];
        if (!champ_signature_egale(e[@"sig"], sig)) continue;
        /* Le plus récemment servi passe en tête : le champ que l'on redessine
         * en boucle est trouvé du premier coup. */
        if (i > 0) {
            [g_memo_champs removeObjectAtIndex:i];
            [g_memo_champs insertObject:e atIndex:0];
        }
        return e[@"chaine"];
    }

    NSAttributedString *as = [field_attr_string_construit(o, s, at) copy];

    [g_memo_champs insertObject:@{ @"sig" : sig, @"chaine" : as } atIndex:0];
    while ([g_memo_champs count] > HC_TEXTE_MEMO)
        [g_memo_champs removeLastObject];

    return as;
}

static NSAttributedString *field_attr_string_construit(Object *o, NSString *s,
                                                       NSDictionary *at)
{
    NSMutableAttributedString *as =
        [[NSMutableAttributedString alloc] initWithString:s attributes:at];

    /* ---- style de paragraphe : interligne, alignement, retour à la ligne ----
     *
     * Les trois sont posés ici, dans la chaîne elle-même, plutôt que laissés
     * au moteur de rendu : -drawInRect: et la NSTextView de l'éditeur
     * n'espacent pas les lignes de la même façon, et l'écart — minuscule par
     * ligne — s'accumulait jusqu'à quatre lignes entières sur vingt. Poser la
     * consigne dans le texte règle la question à la source, aucun des deux
     * moteurs n'ayant plus son mot à dire. */
    NSMutableParagraphStyle *ps =
        [[NSParagraphStyle defaultParagraphStyle] mutableCopy];

    /* L'interligne n'est imposé QUE si « fixedLineHeight » est coché.
     *
     * C'est le sens de cette propriété de champ : coché, toutes les lignes ont
     * la même hauteur, et un mot en gros corps est rogné ; décoché, chaque
     * ligne prend la hauteur qu'il lui faut, et un passage en 18 points
     * repousse ses voisines.
     *
     * L'imposer dans les deux cas — ce que je faisais depuis que l'interligne
     * fixe a servi à réconcilier le dessin et l'éditeur — écrasait les lignes
     * hautes les unes sur les autres et faussait leur décompte, donc le clic
     * et la sélection avec.
     *
     * Le décompte reste juste dans les deux modes, parce que click_line_number
     * interroge la mise en page réelle plutôt que de diviser par une hauteur
     * supposée : c'est précisément ce que cette correction préserve. */
    if (o->fixed_lh) {
        CGFloat lh = hc_text_height(o);
        if (lh < 1) lh = 12;
        [ps setMinimumLineHeight:lh];
        [ps setMaximumLineHeight:lh];

        /* Poser le texte SUR sa ligne, et non sous le trait du dessus.
         *
         * AppKit agrandit une ligne contrainte vers le bas, en laissant la
         * ligne de base là où elle serait sans contrainte : le texte se
         * retrouve collé en haut, avec tout le vide dessous. HyperCard fait
         * l'inverse — le texte repose sur la ligne, comme sur du papier
         * réglé, et l'espace supplémentaire va au-dessus.
         *
         * On décale donc la ligne de base de ce que la contrainte a ajouté.
         * L'écart peut être nul ou négatif si l'interligne demandé est plus
         * court que la police, auquel cas on ne décale rien : mieux vaut un
         * texte rogné qu'un texte remonté hors de sa ligne. */
        NSFont *fb = obj_base_font(o, 12);
        if (fb) {
            NSAttributedString *une =
                [[NSAttributedString alloc] initWithString:@"Mg"
                                                attributes:@{NSFontAttributeName: fb}];
            CGFloat nat = [une boundingRectWithSize:NSMakeSize(10000, CGFLOAT_MAX)
                                            options:NSStringDrawingUsesLineFragmentOrigin
                          ].size.height;
            CGFloat sup = lh - nat;
            if (sup > 0) [as addAttribute:NSBaselineOffsetAttributeName
                                    value:@(-sup)
                                    range:NSMakeRange(0, [as length])];
        }
    } else {
        /* Interligne variable, mais consigne EXPLICITE quand même.
         *
         * Sans elle, -drawInRect: et la NSTextView de l'éditeur retombent
         * chacune sur leurs propres métriques, et l'écart se voit d'un mode à
         * l'autre : les lignes paraissent plus serrées en édition. Fixer le
         * multiple à 1 et l'espacement à zéro ne contraint pas la hauteur des
         * lignes — chacune garde celle que sa police lui donne — mais impose
         * aux deux moteurs la même règle pour les enchaîner. */
        [ps setLineHeightMultiple:1.0];
        [ps setParagraphSpacing:0.0];
        [ps setParagraphSpacingBefore:0.0];

        /* L'interligne que -drawInRect: ajoute et que la NSTextView omet.
         *
         * Mesuré : 112 pixels au dessin contre 105 en édition pour sept
         * lignes, soit exactement un pixel par ligne. C'est le « leading » de
         * la police — l'espace qu'elle recommande entre deux lignes. On le
         * rend explicite, ce qui accorde les deux moteurs sans contraindre la
         * hauteur des lignes : chacune garde celle que sa police lui donne.
         *
         * On le demande à la police du champ, plutôt que de coder 1 en dur :
         * une police de titre en recommande davantage qu'une police de
         * labeur, et l'écart suivrait. */
        [ps setLineSpacing:0.0];

        if (gForEditor) {
            /* Imposer à l'éditeur la hauteur de ligne du DESSIN.
             *
             * Mesuré sur sept lignes de 12 points : 112 pixels au dessin
             * contre 105 en édition, soit 16 contre 15. Un pixel par ligne.
             *
             * D'où vient-il ? Pas du « leading » de la police, qui vaut zéro
             * ici — je l'ai cru et le réglage n'a rien changé. C'est
             * -drawInRect: qui arrondit la hauteur de ligne vers le haut, là
             * où le gestionnaire de mise en page garde la valeur exacte.
             *
             * On demande donc à l'éditeur la même hauteur, calculée sur la
             * police : ascendante + descendante, arrondie au pixel supérieur,
             * comme le fait le dessin. Chaque ligne garde alors la hauteur que
             * sa police lui donne — l'interligne reste variable — mais les
             * deux moteurs l'arrondissent pareil. */
            /* La hauteur de ligne du dessin, MESURÉE et non reconstituée.
             *
             * Deux tentatives ont échoué avant celle-ci : le « leading » de la
             * police vaut zéro, et « ascendante - descendante » arrondie donne
             * 15 là où le dessin en fait 16. Plutôt que de deviner une
             * troisième formule, on demande au dessin lui-même : une ligne de
             * texte, mesurée par -boundingRectWithSize:, rend exactement la
             * hauteur qu'il emploiera. */
            NSFont *fb = obj_base_font(o, 12);
            if (fb) {
                NSAttributedString *une =
                    [[NSAttributedString alloc] initWithString:@"Mg"
                                                    attributes:@{NSFontAttributeName: fb}];
                CGFloat h = [une boundingRectWithSize:NSMakeSize(10000, CGFLOAT_MAX)
                                              options:NSStringDrawingUsesLineFragmentOrigin
                            ].size.height;
                if (h > 0) [ps setMinimumLineHeight:h];
            }
        }
    }

    /* Alignement et retour à la ligne, posés au même endroit que l'interligne
     * — ils appartiennent tous trois au style de PARAGRAPHE, et les séparer
     * obligerait à parcourir la chaîne deux fois.
     *
     * dontWrap coupe au bord au lieu de passer à la ligne : c'est ce qu'il
     * faut pour des données en colonnes, où un retour automatique décalerait
     * tout le tableau. */
    switch (o->text_align) {
        case 1:  [ps setAlignment:NSTextAlignmentCenter]; break;
        case 2:  [ps setAlignment:NSTextAlignmentRight];  break;
        default: [ps setAlignment:NSTextAlignmentLeft];   break;
    }
    if (o->dont_wrap) [ps setLineBreakMode:NSLineBreakByClipping];

    [as addAttribute:NSParagraphStyleAttributeName value:ps
               range:NSMakeRange(0, [as length])];

    int n = hc_run_count(o);
    if (getenv("HC_RUNS_DEBUG"))
        NSLog(@"[runs] champ %s : %d plage(s)", o->name ? o->name : "?", n);
    if (n <= 0) { apply_selection_highlight(as, o); return as; }

    const char *tx  = hc_field_text(o);
    NSFont  *base   = at[NSFontAttributeName];
    NSColor *color  = at[NSForegroundColorAttributeName];
    NSUInteger len  = [s length];

    for (int i = 0; i < n; i++) {
        int a = 0, l = 0, st = 0, sz = 0, co = HC_COLOR_INHERIT;
        const char *fn = NULL;
        if (!hc_run_attrs_color(o, i, &a, &l, &st, &sz, &fn, &co) || l <= 0) continue;
        NSUInteger u0 = utf16_from_byte(tx, a);
        NSUInteger u1 = utf16_from_byte(tx, a + l);
        if (u0 >= len) continue;
        if (u1 > len)  u1 = len;
        if (u1 <= u0)  continue;
        if (getenv("HC_RUNS_DEBUG"))
            NSLog(@"[runs]   [%d..%d[ style %d corps %d police %s"
                   " -> UTF16 [%lu..%lu[",
                  a, a + l, st, sz, fn ? fn : "(champ)",
                  (unsigned long)u0, (unsigned long)u1);
        /* La plage porte sa propre police : les traits se posent dessus, pas
         * sur celle du champ. Sans cela « Geneva » restait invisible tant que
         * le champ était en Helvetica. */
        /* La couleur de la plage l'emporte sur celle du champ. HC_COLOR_INHERIT
         * veut dire « la plage ne se prononce pas » : on garde alors celle du
         * champ, comme pour la police et le corps. */
        NSColor *cr = color;
        if (co != HC_COLOR_INHERIT)
            /* sRGB, le même espace que celui de la relecture.
             *
             * On posait en « calibrated » et on relisait en « device » : deux
             * espaces distincts, donc une conversion à chaque aller-retour
             * entre l'édition et la visualisation. Elle n'est pas neutre — la
             * trace montrait 1.0 devenir 0.999996 — et l'erreur s'accumulait :
             * les couleurs pâlissaient jusqu'au blanc au fil des allers et
             * retours. Un seul espace des deux côtés, et il n'y a plus de
             * conversion du tout. */
            cr = [NSColor colorWithSRGBRed:((co >> 16) & 255) / 255.0
                                     green:((co >>  8) & 255) / 255.0
                                      blue:( co        & 255) / 255.0
                                     alpha:1.0];
        apply_run_style(as, NSMakeRange(u0, u1 - u0), st,
                        run_base_font(fn, sz, base), cr);
    }
    apply_selection_highlight(as, o);
    return as;
}


/* ---- Traduction retour : attributs Cocoa -> bits du noyau ----
 *
 * Pendant la saisie, c'est la NSTextStorage qui détient le style : elle sait
 * déjà tout faire, y compris Cmd-B. À la fermeture on relit ses attributs et
 * on reconstruit les plages du noyau. Le Toolbox faisait exactement cela avec
 * son TEStyleRec — une table de plages pointant vers des styles ; Cocoa en est
 * la descendante directe, les deux modèles ne sont pas rivaux.
 *
 * `group` n'a aucun équivalent Cocoa : c'est une sémantique HyperCard, pas un
 * rendu. On le porte comme attribut personnalisé, sinon il disparaîtrait au
 * premier aller-retour. */
int style_bits_from_attrs(NSDictionary *a)
{
    int st = 0;

    NSFont *f = a[NSFontAttributeName];
    if (f) {
        NSFontDescriptorSymbolicTraits t = [[f fontDescriptor] symbolicTraits];
        if (t & NSFontDescriptorTraitBold)   st |= HC_BOLD;
        if (t & NSFontDescriptorTraitItalic) st |= HC_ITALIC;
    }

    NSNumber *u = a[NSUnderlineStyleAttributeName];
    if (u && [u intValue] != 0) st |= HC_UNDERLINE;

    /* Le trait NÉGATIF est notre gras synthétique, pas un contour : seul un
     * trait positif signifie « lettres creuses ». Confondre les deux
     * transformerait chaque gras en outline à la première sauvegarde.
     * Quand les deux styles coexistent, le trait est positif et ne dit plus
     * rien du gras : c'est l'attribut dédié qui le rapporte. */
    NSNumber *sw = a[NSStrokeWidthAttributeName];
    if (sw) {
        double v = [sw doubleValue];
        if (v > 0)      st |= HC_OUTLINE;
        else if (v < 0) st |= HC_BOLD;
    }
    if (a[kHCFauxBoldAttribute]) st |= HC_BOLD;

    if (a[NSShadowAttributeName]) st |= HC_SHADOW;

    NSNumber *k = a[NSKernAttributeName];
    if (k) {
        double v = [k doubleValue];
        if (v < 0)      st |= HC_CONDENSE;
        else if (v > 0) st |= HC_EXTEND;
    }

    if (a[kHCGroupAttribute]) st |= HC_GROUP;
    return st;
}

/* Décalage en OCTETS correspondant à un index UTF-16 : l'inverse de
 * utf16_from_byte. Le noyau raisonne en octets, NSString en unités UTF-16. */
int byte_from_utf16(NSString *s, NSUInteger u16)
{
    if (u16 == 0) return 0;
    if (u16 > [s length]) u16 = [s length];
    NSString *pre = [s substringToIndex:u16];
    return (int)[pre lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
}

/* hauteur totale du texte d'un champ, dans sa largeur utile */
/* ═══ MISE EN PAGE CONSERVÉE ════════════════════════════════════════════
 *
 * Même avec la chaîne attribuée en mémoire, le dessin composait encore les
 * quarante-huit kilo-octets d'un champ à chaque image pour n'en montrer
 * qu'une vingtaine de lignes : -drawInRect: met en page tout ce qu'on lui
 * donne, sans savoir ce qui sera visible.
 *
 * On garde donc le trio NSTextStorage / NSLayoutManager / NSTextContainer
 * d'une image à l'autre. Deux gains, et le second est le vrai :
 *
 *   la mise en page n'est plus refaite quand rien n'a changé ;
 *   NSLayoutManager compose PARESSEUSEMENT — en ne lui demandant que la
 *   plage de glyphes du rectangle visible, il n'aura jamais à composer les
 *   lignes situées plus bas.
 *
 * Le conteneur a la largeur du texte et une hauteur infinie : c'est sur la
 * largeur que le texte se replie, et la hauteur est justement ce qu'on ne
 * veut pas calculer.
 *
 * La largeur entre dans la clé, comme pour la hauteur : la changer replie
 * le texte autrement. */
NSLayoutManager *field_layout(Object *o, NSString *s, NSDictionary *at,
                              CGFloat largeur, NSTextContainer **conteneur)
{
    static NSMutableArray<NSDictionary *> *memo = nil;
    if (!memo) memo = [[NSMutableArray alloc] init];

    NSDictionary *sig = champ_signature(o, s);
    NSNumber *larg = @(largeur);

    for (NSUInteger i = 0; i < [memo count]; i++) {
        NSDictionary *e = memo[i];
        if (![e[@"largeur"] isEqualToNumber:larg]) continue;
        if (!champ_signature_egale(e[@"sig"], sig)) continue;
        if (i > 0) {
            [memo removeObjectAtIndex:i];
            [memo insertObject:e atIndex:0];
        }
        if (conteneur) *conteneur = e[@"conteneur"];
        return e[@"disposeur"];
    }

    NSTextStorage   *ts = [[NSTextStorage alloc]
                            initWithAttributedString:field_attr_string(o, s, at)];
    NSLayoutManager *lm = [[NSLayoutManager alloc] init];
    NSTextContainer *tc = [[NSTextContainer alloc]
                            initWithContainerSize:NSMakeSize(largeur, CGFLOAT_MAX)];
    [tc setLineFragmentPadding:0];
    [lm addTextContainer:tc];
    [ts addLayoutManager:lm];

    /* Les trois sont retenus ensemble : le disposeur ne garde qu'une
     * référence faible sur son magasin, qui disparaîtrait sinon. */
    [memo insertObject:@{ @"sig"       : sig,
                          @"largeur"   : larg,
                          @"magasin"   : ts,
                          @"disposeur" : lm,
                          @"conteneur" : tc } atIndex:0];
    while ([memo count] > HC_TEXTE_MEMO) [memo removeLastObject];

    if (conteneur) *conteneur = tc;
    return lm;
}

/* La hauteur est mémorisée séparément de la chaîne : c'est ELLE qui coûte
 * cher. boundingRectWithSize met en page le texte entier — mille lignes pour
 * en afficher vingt —, et le dessin d'un champ à défilement la demande à
 * chaque image, uniquement pour dimensionner la poignée de la barre.
 *
 * La largeur entre dans la clé : c'est sur elle que le texte se replie, donc
 * elle change la hauteur. Le reste vient de champ_signature. */
CGFloat field_text_height(Object *o, NSRect tr) {
    /* hc_field_text et non o->contents : un champ de fond non partagé a un
     * texte par carte, et c'est celui-là qu'on affiche. */
    const char *tx = hc_field_text(o);
    NSString *s = [NSString stringWithUTF8String:tx ? tx : ""];
    if ([s length] == 0) return 0;

    static NSMutableArray<NSDictionary *> *memo = nil;
    if (!memo) memo = [[NSMutableArray alloc] init];

    NSDictionary *sig = champ_signature(o, s);
    NSNumber *larg = @(tr.size.width);

    for (NSUInteger i = 0; i < [memo count]; i++) {
        NSDictionary *e = memo[i];
        if (![e[@"largeur"] isEqualToNumber:larg]) continue;
        if (!champ_signature_egale(e[@"sig"], sig)) continue;
        if (i > 0) {
            [memo removeObjectAtIndex:i];
            [memo insertObject:e atIndex:0];
        }
        return [e[@"hauteur"] doubleValue];
    }

    NSDictionary *at = obj_attrs(o, 12, [NSColor blackColor]);
    /* Mesurer sur le texte attribué : du gras occupe plus de place, et un
     * champ défilant se tromperait de hauteur. */
    NSAttributedString *as = field_attr_string(o, s, at);
    NSRect b = [as boundingRectWithSize:NSMakeSize(tr.size.width, CGFLOAT_MAX)
                                options:NSStringDrawingUsesLineFragmentOrigin];

    [memo insertObject:@{ @"sig"     : sig,
                          @"largeur" : larg,
                          @"hauteur" : @(b.size.height) } atIndex:0];
    while ([memo count] > HC_TEXTE_MEMO) [memo removeLastObject];

    return b.size.height;
}

/* ---------------------------------------------------------------------------
 * Géométrie du texte d'un champ : UNE SEULE source de vérité, partagée par le
 * dessin, l'éditeur et le suivi de la barre de défilement.
 *
 * Auparavant chaque endroit refaisait son propre calcul — le dessin retirait
 * la barre puis les marges, l'éditeur se contentait d'un NSInsetRect(…, 2, 2)
 * — et les deux divergeaient de quelques pixels. Le texte sautait donc en
 * passant du mode édition au mode navigation, et la plage de défilement
 * n'était pas la même de part et d'autre.
 * ------------------------------------------------------------------------- */
NSRect field_text_rect(Object *o) {
    NSRect r = NSMakeRect(o->x, o->y, o->w, o->h);
    const char *st = o->style ? o->style : "rectangle";
    if (strcmp(st, "scrolling") == 0) {
        r.size.width -= 16;                    /* place prise par la barre */
    } else if (strcmp(st, "shadow") == 0) {
        r.size.width  -= 3;                    /* l'ombre portée */
        r.size.height -= 3;
    }
    CGFloat m = o->wide_margins ? 8 : 4;
    return NSInsetRect(r, m, m);
}

/* Défilement maximal : ce qui dépasse de la partie visible, jamais négatif. */
CGFloat field_max_scroll(Object *o) {
    NSRect tr = field_text_rect(o);
    CGFloat maxs = field_text_height(o, tr) - tr.size.height;
    return maxs > 0 ? maxs : 0;
}

/* Ramener o->scroll dans ses bornes. Il n'était borné qu'en bas : les flèches
 * et le clic-page pouvaient le pousser au-delà de la fin du texte, et le champ
 * finissait sur du blanc pendant que la poignée, elle, restait bloquée en bas. */
void field_clamp_scroll(Object *o) {
    CGFloat maxs = field_max_scroll(o);
    if (o->scroll > maxs) o->scroll = (int)lround(maxs);
    if (o->scroll < 0)    o->scroll = 0;
}

/* Rectangle dans lequel le texte est RÉELLEMENT tracé : la zone visible,
 * remontée du défilement et étendue à la hauteur du contenu.
 *
 * Partagé par le dessin et le test de clic. Les deux le calculaient chacun de
 * leur côté, et sur un champ défilant le clic tombait à côté d'exactement le
 * défilement courant — le dessin appliquait le décalage, le test de clic
 * croyait le compenser. Une seule fonction, et la question ne se pose plus.
 * C'est le même remède qu'au rectangle de texte lui-même, à la surbrillance
 * de sélection et au décompte des lignes : ne pas recalculer à côté. */
NSRect field_text_draw_rect(Object *o) {
    NSRect tr = field_text_rect(o);
    if (!(o->style && strcmp(o->style, "scrolling") == 0)) return tr;

    NSRect off = tr;
    off.origin.y -= o->scroll;
    CGFloat full = field_text_height(o, tr);
    if (full < tr.size.height) full = tr.size.height;
    off.size.height = full;
    return off;
}
