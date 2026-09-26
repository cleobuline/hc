/* hc_icons.c — Table des icônes d'une pile.
 *
 * HyperCard rangeait les icônes de boutons en ressources ICON dans le fichier
 * de pile : une pile emportait ses icônes, et un bouton n'en retenait que le
 * numéro. La table appartient donc à l'objet PILE, et se sauve avec elle
 * (blocs « iconres » de hc_file.c).
 *
 * Le noyau ne sait pas dessiner : les 128 octets lui sont opaques, exactement
 * comme l'est déjà le base64 de `paint`.
 */
#include "hc_core.h"

#include <stdlib.h>
#include <string.h>

static char *icon_dupstr(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static int is_stack(Object *o)
{
    return o && o->type == OBJ_STACK;
}

struct StackIcon *hc_icon_get(Object *stack, int id)
{
    if (!is_stack(stack)) return NULL;
    for (int i = 0; i < stack->nicons; i++)
        if (stack->icons[i].id == id) return &stack->icons[i];
    return NULL;
}

int hc_icon_count(Object *stack)
{
    return is_stack(stack) ? stack->nicons : 0;
}

struct StackIcon *hc_icon_at(Object *stack, int i)
{
    if (!is_stack(stack) || i < 0 || i >= stack->nicons) return NULL;
    return &stack->icons[i];
}

struct StackIcon *hc_icon_add(Object *stack, int id, const char *name)
{
    if (!is_stack(stack)) return NULL;

    /* Le numéro est unique. Reposer la même icône la remplace : sans ça, un
     * fichier contenant deux fois le même numéro ferait grossir la table à
     * chaque relecture. */
    struct StackIcon *e = hc_icon_get(stack, id);

    if (!e) {
        if (stack->nicons == stack->capicons) {
            int cap = stack->capicons ? stack->capicons * 2 : 8;
            struct StackIcon *t = realloc(stack->icons, (size_t)cap * sizeof *t);
            if (!t) return NULL;
            stack->icons    = t;
            stack->capicons = cap;
        }
        e = &stack->icons[stack->nicons];
        /* realloc rend de la mémoire non initialisée : sans ce nettoyage,
         * `name` part sur un pointeur bidon et le free d'après y passe. Les
         * 128 octets valent zéro du même coup, ce qu'on veut pour une icône
         * neuve. */
        memset(e, 0, sizeof *e);
        e->id = id;
        stack->nicons++;
    }

    free(e->name);
    e->name = icon_dupstr(name);
    return e;
}

void hc_icon_remove(Object *stack, int id)
{
    struct StackIcon *e = hc_icon_get(stack, id);
    if (!e) return;

    free(e->name);
    free(e->couleur);
    int i = (int)(e - stack->icons);
    memmove(e, e + 1, (size_t)(stack->nicons - i - 1) * sizeof *e);
    stack->nicons--;
}

/* ---------------------------------------------------------- couleur ---- */

struct HcIconCouleur *hc_icon_couleur(struct StackIcon *ic)
{
    return ic ? ic->couleur : NULL;
}

/* REDÉRIVER LA SILHOUETTE, et c'est ce qui fait tenir tout l'édifice.
 *
 * `bits` reste la vérité pour tout ce qui ne sait pas dessiner en couleur :
 * l'affichage d'un bouton sur un écran en noir et blanc, une pile relue par
 * un binaire d'avant, et surtout les cent chemins de code qui lisent `bits`
 * sans se demander s'il y a une couleur à côté. Plutôt que de les convertir
 * un par un, on garantit que `bits` DIT TOUJOURS LA VÉRITÉ : tout pixel non
 * transparent est de l'encre.
 *
 * La disposition est celle d'une ressource ICON : 32 lignes de 4 octets, bit
 * de poids fort à gauche. */
void hc_icon_silhouette(struct StackIcon *ic)
{
    if (!ic || !ic->couleur) return;
    memset(ic->bits, 0, HC_ICON_BYTES);
    for (int y = 0; y < HC_ICON_COTE; y++)
        for (int x = 0; x < HC_ICON_COTE; x++)
            if (ic->couleur->pixels[y * HC_ICON_COTE + x])
                ic->bits[y * 4 + x / 8] |= (unsigned char)(0x80 >> (x % 8));
}

struct HcIconCouleur *hc_icon_couleur_cree(struct StackIcon *ic)
{
    if (!ic) return NULL;
    if (ic->couleur) return ic->couleur;

    struct HcIconCouleur *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    /* Une palette a toujours au moins son entrée 0, la transparence. Compter
     * zéro couleur rendrait un bloc de fichier sans aucune ligne « c », et le
     * relecteur ne saurait pas distinguer « pas encore peinte » de « abîmée ». */
    c->ncouleurs = 1;
    ic->couleur = c;
    /* Les pixels sont tous transparents : la silhouette devient vide, et
     * c'est juste — une icône en couleur qui n'a rien de peint ne montre
     * rien. Celle qui vient du noir et blanc se repeint par-dessus. */
    hc_icon_silhouette(ic);
    return c;
}

void hc_icon_couleur_ote(struct StackIcon *ic)
{
    if (!ic || !ic->couleur) return;
    free(ic->couleur);
    ic->couleur = NULL;
    /* On NE TOUCHE PAS à `bits` : la silhouette est la dernière trace de ce
     * qui a été peint, et l'effacer avec les couleurs ferait disparaître
     * l'icône entière pour qui voulait seulement lui retirer sa couleur. */
}

/* ------------------------------------------------------- édition ------- */

/* PASSER EN COULEUR SANS PERDRE LE DESSIN.
 *
 * Chaque pixel d'encre devient l'index 1, peint de la couleur demandée. C'est
 * le geste « mettre en couleur » de l'éditeur, et il DOIT être sans perte :
 * une icône qui se viderait en gagnant la couleur ferait perdre son travail à
 * qui clique pour voir ce que ça donne.
 *
 * Une icône déjà en couleur n'est pas touchée — le geste est idempotent, ce
 * qui évite qu'un double clic ne réduise une icône à deux couleurs. */
int hc_icon_couleur_depuis_bits(struct StackIcon *ic,
                                unsigned char r, unsigned char v, unsigned char b)
{
    if (!ic) return 0;
    if (ic->couleur) return 1;

    /* LA SILHOUETTE D'ABORD, ET C'EST INDISPENSABLE.
     *
     * hc_icon_couleur_cree pose des pixels tous transparents PUIS redérive la
     * silhouette — donc elle vide `bits`. Lire `ic->bits` après elle ne rend
     * que des zéros, et l'icône passait en couleur entièrement effacée.
     *
     * Le défaut ne se voyait dans aucun compte séparé : la palette était
     * juste, les pixels étaient tous valides, l'aller-retour marchait. C'est
     * la comparaison « 256 encres avant, 0 peints après » qui l'a montré. */
    unsigned char source[HC_ICON_BYTES];
    memcpy(source, ic->bits, HC_ICON_BYTES);

    struct HcIconCouleur *c = hc_icon_couleur_cree(ic);
    if (!c) return 0;

    c->ncouleurs   = 2;
    c->palette[1][0] = r; c->palette[1][1] = v; c->palette[1][2] = b;
    for (int y = 0; y < HC_ICON_COTE; y++)
        for (int x = 0; x < HC_ICON_COTE; x++)
            c->pixels[y * HC_ICON_COTE + x] =
                (source[y * 4 + x / 8] & (0x80 >> (x % 8))) ? 1 : 0;
    /* La silhouette ne change pas — on vient de la recopier — mais on la
     * redérive quand même : c'est la seule façon que l'invariant « bits dit
     * toujours la vérité » ne dépende pas de la justesse du code ci-dessus. */
    hc_icon_silhouette(ic);
    return 1;
}

int hc_icon_pixel_lu(const struct StackIcon *ic, int x, int y)
{
    if (!ic || !ic->couleur) return 0;
    if (x < 0 || y < 0 || x >= HC_ICON_COTE || y >= HC_ICON_COTE) return 0;
    return ic->couleur->pixels[y * HC_ICON_COTE + x];
}

void hc_icon_pixel_pose(struct StackIcon *ic, int x, int y, int index)
{
    if (!ic || !ic->couleur) return;
    if (x < 0 || y < 0 || x >= HC_ICON_COTE || y >= HC_ICON_COTE) return;
    if (index < 0 || index >= HC_ICON_COULEURS_MAX) return;

    unsigned char *p = &ic->couleur->pixels[y * HC_ICON_COTE + x];
    if (*p == (unsigned char)index) return;
    *p = (unsigned char)index;

    /* LA SILHOUETTE SUIT CHAQUE COUP DE PINCEAU. On pourrait la redériver
     * seulement à l'enregistrement, mais alors `bits` serait faux entre-temps
     * — et c'est justement entre-temps que l'interface le lit pour afficher
     * le bouton. Mille vingt-quatre pixels relus par clic ne coûtent rien à
     * cette échelle. */
    hc_icon_silhouette(ic);
}

int hc_icon_palette_pose(struct StackIcon *ic, int index,
                         unsigned char r, unsigned char v, unsigned char b)
{
    if (!ic || !ic->couleur) return 0;
    /* L'INDEX 0 EST LA TRANSPARENCE, il n'a pas de couleur à porter. Le
     * laisser écrire donnerait une entrée de palette que rien ne peut
     * afficher, et qu'on chercherait longtemps. */
    if (index < 1 || index >= HC_ICON_COULEURS_MAX) return 0;

    struct HcIconCouleur *c = ic->couleur;
    c->palette[index][0] = r;
    c->palette[index][1] = v;
    c->palette[index][2] = b;
    if (index >= c->ncouleurs) c->ncouleurs = index + 1;
    return 1;
}

int hc_icon_palette_index(struct StackIcon *ic,
                          unsigned char r, unsigned char v, unsigned char b)
{
    if (!ic || !ic->couleur) return 0;
    struct HcIconCouleur *c = ic->couleur;

    for (int i = 1; i < c->ncouleurs; i++)
        if (c->palette[i][0] == r && c->palette[i][1] == v &&
            c->palette[i][2] == b)
            return i;

    /* PALETTE PLEINE : on rend 0 plutôt que d'écraser une couleur au hasard.
     * L'appelant — le collage d'une image — doit alors choisir la plus
     * proche, ce que le noyau ne sait pas faire : il ne dessine pas, et
     * « la plus proche » est une question de perception, pas d'arithmétique. */
    if (c->ncouleurs >= HC_ICON_COULEURS_MAX) return 0;

    int i = c->ncouleurs;
    c->palette[i][0] = r; c->palette[i][1] = v; c->palette[i][2] = b;
    c->ncouleurs = i + 1;
    return i;
}

/* Un quart de tour horaire : le pixel qui arrive en (ligne, colonne) vient de
 * (31 - colonne, ligne). On écrit dans un tampon plutôt qu'en place — la
 * rotation lit des pixels qu'elle a déjà réécrits, et la faire sur le tableau
 * lui-même brouillerait le dessin dès la première ligne.
 *
 * La palette ne tourne pas, évidemment : seuls les index bougent.
 *
 * LES DEUX CHEMINS DOIVENT TOURNER DANS LE MÊME SENS, et la première version
 * de celui en couleur tournait à l'envers — out[y][x] = in[x][31-y] au lieu de
 * in[31-x][y]. Ça ne se voit dans aucun compte de pixels : une icône tournée
 * du mauvais côté a exactement le même nombre d'encres. C'est le dessin du
 * harnais qui le montre, et c'est précisément pour ça qu'il est là. */
void hc_icon_tourne(struct StackIcon *ic)
{
    if (!ic) return;

    if (ic->couleur) {
        unsigned char out[HC_ICON_PIXELS];
        for (int y = 0; y < HC_ICON_COTE; y++)
            for (int x = 0; x < HC_ICON_COTE; x++)
                out[y * HC_ICON_COTE + x] =
                    ic->couleur->pixels[(HC_ICON_COTE - 1 - x) * HC_ICON_COTE + y];
        memcpy(ic->couleur->pixels, out, sizeof out);
        hc_icon_silhouette(ic);
        return;
    }

    unsigned char out[HC_ICON_BYTES];
    memset(out, 0, sizeof out);
    for (int y = 0; y < HC_ICON_COTE; y++)
        for (int x = 0; x < HC_ICON_COTE; x++) {
            int sx = y, sy = HC_ICON_COTE - 1 - x;
            if (ic->bits[sy * 4 + sx / 8] & (0x80 >> (sx % 8)))
                out[y * 4 + x / 8] |= (unsigned char)(0x80 >> (x % 8));
        }
    memcpy(ic->bits, out, HC_ICON_BYTES);
}

/* Pas de « numéro libre » ici : le noyau ne voit que la pile, alors qu'un
 * numéro libre doit l'être aussi dans le catalogue compilé dans l'application.
 * C'est hcicon_edit_free_id, côté Cocoa, qui tranche. */

/* À appeler depuis hc_free, sur la branche OBJ_STACK. */
void hc_icons_free(Object *stack)
{
    if (!is_stack(stack)) return;
    for (int i = 0; i < stack->nicons; i++) {
        free(stack->icons[i].name);
        free(stack->icons[i].couleur);
    }
    free(stack->icons);
    stack->icons    = NULL;
    stack->nicons   = 0;
    stack->capicons = 0;
}
