#ifndef hcicons_h
#define hcicons_h

#import <Cocoa/Cocoa.h>

/* Icones ICON 32x32 d'origine HyperCard (Apple / FileMaker).
   bits : 128 octets, 32 lignes de 4 octets, bit de poids fort a gauche. */
typedef struct {
    int   id;
    const char *name;
    unsigned char bits[128];
} HCIcon;

extern const HCIcon HCICONS[];
extern const int NUM_HCICONS;

/* Retourne l'icone d'identifiant donne, ou NULL.
   Le catalogue de la pile courante est consulte EN PREMIER : une pile peut
   ainsi redefinir une icone d'origine sans qu'on touche au binaire. */
const HCIcon *hcicon_find(int id);

/* Catalogue de la pile courante.
 *
 * La pile reste la seule source de verite — ses icones vivent dans sa
 * structure et se sauvent avec elle. Ce qui suit n'en est qu'une COPIE de
 * travail, que hcicon_find peut parcourir sans rien savoir du noyau et dont
 * les pointeurs rendus restent valides jusqu'au prochain appel.
 *
 * A rappeler apres tout chargement de pile et apres toute retouche dans
 * l'editeur d'icones. Passer (NULL, 0) vide le catalogue : on retombe alors
 * sur les seules icones d'origine.
 *
 * `tab` pointe sur le tableau `icons` de la pile (struct StackIcon, hc_core.h) ;
 * il n'est lu que le temps de l'appel. */
struct StackIcon;
void hcicon_use_stack_icons(const struct StackIcon *tab, int n);

/* Parcours du catalogue de la pile, pour l'editeur. */
int            hcicon_stack_count(void);
const HCIcon  *hcicon_stack_at(int i);

/* ---- Catalogue complet ----
 * Les icones de la pile, puis celles d'origine qu'elle ne redefinit pas.
 * Meme ordre de priorite que hcicon_find : une icone redefinie n'apparait
 * qu'une fois, dans sa version de la pile. Sans cette regle on la verrait en
 * double sans savoir laquelle on modifie.
 *
 * L'ordre et les pointeurs changent a chaque hcicon_use_stack_icons : ne rien
 * en retenir d'un evenement a l'autre, relire par numero. */
int            hcicon_catalog_count(void);
const HCIcon  *hcicon_catalog_at(int i);

/* Ce numero est-il pris dans le catalogue D'ORIGINE ?
 *
 * Sans etat : ne regarde ni la pile ni le catalogue de travail. Sert au noyau,
 * qui doit choisir un numero libre en transplantant des icones d'une pile a
 * l'autre, et qui ne connait pas HCICONS. */
int hcicon_builtin_has(int id);

/* Le numero designe par un texte, pour « set the icon of me to ... ».
 *
 * Accepte un nombre (« 3071 ») ou un nom (« Close Box »), la casse et les
 * espaces de bord etant ignores. Cherche dans la pile d'abord, puis dans le
 * catalogue d'origine — meme priorite que hcicon_find, sinon une icone
 * redefinie repondrait sous son ancien dessin.
 *
 * Rend 0 quand rien ne correspond. Ce qui tombe bien : 0 veut deja dire
 * « aucune icone », et c'est le comportement de HyperCard devant un nom
 * inconnu. L'appelant qui veut distinguer les deux cas teste le texte. */
int hcicon_id_for_text(const char *s);

/* Dessine l'icone INVERSEE, pour un bouton transparent allume.
 *
 * L'encre passe au blanc et le blanc ENCLOS passe au noir : l'inversion se
 * limite a la forme de l'icone, sans carre autour ni debordement sur la carte.
 *
 * Le blanc enclos se distingue du fond par diffusion depuis les bords de la
 * grille : tout pixel eteint que l'on n'atteint pas ainsi est a l'interieur du
 * dessin. C'est la seule definition qui tienne pour une icone quelconque, et
 * elle vaut ce que vaut le dessin — une forme ouverte n'a pas d'interieur, et
 * seule son encre s'inversera. */
void hcicon_draw_inverted(const HCIcon *ic, NSRect r, CGFloat px);

/* Dessine une icone 32x32 centree dans le rect (px = taille d'un pixel). */
void hcicon_draw(const HCIcon *ic, NSRect r, CGFloat px);

#endif /* hcicons_h */
