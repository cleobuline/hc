/* hc_presse_papiers.c — copier, couper, coller : objets et cartes.
 *
 * PREMIER MORCEAU DÉTACHÉ DE hc_core.c, qui faisait 15 734 lignes.
 *
 * Choisi parce que c'est le bloc LE PLUS ISOLÉ du fichier, mesuré avant de
 * couper : sept symboles à exposer pour sept cents lignes déplacées, là où
 * le pont v3 en aurait demandé quatre-vingt-six pour six mille huit cents.
 * Le raisonnement complet et le tableau des mesures sont dans hc_interne.h.
 *
 * LE DÉPLACEMENT EST MÉCANIQUE. Pas une ligne de logique n'a changé, et la
 * suite de non-régression le prouve : 215 conformes avant, 215 après, aucune
 * référence n'a bougé. Le seul changement de forme est celui qu'un découpage
 * en C impose — six fonctions du noyau ont perdu leur `static` pour être
 * appelables d'ici.
 *
 * UNE SEULE CHOSE A ÉTÉ REDESSINÉE, et elle réduit la surface au lieu de
 * l'agrandir : hc_pp_oublie. Deux globales de ce fichier — le fond emprunté
 * et la pile d'où il vient — étaient remises à NULL par oublie_objet_interne,
 * dans hc_core.c. Les partager aurait demandé de les rendre visibles toutes
 * les deux ; un VERBE exposé à leur place en demande une seule, et l'état
 * reste privé. C'est la seule façon dont ce découpage change quelque chose,
 * et il le change dans le bon sens.
 */
#include "hc_interne.h"
#include <stdlib.h>
#include <string.h>

/* ==================== presse-papiers d'objets ====================
 *
 * HyperCard copie un OBJET, pas des pixels ni du texte : un bouton collé
 * emporte son script, son icône, sa police, son style. Le presse-papiers
 * garde donc un Object complet, simplement DÉTACHÉ — owner à NULL, absent
 * du tableau parts[] de qui que ce soit. Il survit ainsi à la suppression de
 * sa carte d'origine, et même au chargement d'une autre pile.
 *
 * La liste de ce qu'un clone doit emporter est celle de put_part() dans
 * hc_file.c : si le format de fichier le sérialise, le clone le copie. Les
 * deux doivent rester d'accord, sans quoi un objet collé perdrait à l'écran
 * ce qu'il aurait gardé sur disque. */
static Object *g_clipboard = NULL;


/* Clone profond d'un bouton ou d'un champ. Le clone n'a PAS de propriétaire
 * et garde l'identifiant de l'original : c'est hc_paste_part qui en attribue
 * un neuf au moment de la pose, parce que c'est là seulement qu'on sait dans
 * quelle pile il atterrit. */
static Object *clone_part(Object *o)
{
    if (!o || (o->type != OBJ_BUTTON && o->type != OBJ_FIELD)) return NULL;

    /* Faute de place on rend NULL plutôt que de mourir : les deux appelants
     * (copier une partie, coller) traitent déjà NULL comme « rien copié ». Une
     * copie qui échoue est un désagrément ; un arrêt en perd la pile. */
    Object *c = calloc(1, sizeof(Object));
    if (!c) return NULL;

    c->type = o->type;
    c->id   = o->id;              /* remplacé à la pose */
    c->owner = NULL;              /* détaché : c'est tout l'intérêt */

    c->name     = dupstr(o->name);
    c->script   = dupstr(o->script);
    c->contents = dupstr(o->contents);
    c->style    = dupstr(o->style);
    c->textfont = dupstr(o->textfont);

    c->x = o->x; c->y = o->y; c->w = o->w; c->h = o->h;

    c->visible      = o->visible;
    c->hilite       = o->hilite;
    c->autohilite   = o->autohilite;
    c->textsize     = o->textsize;
    c->showname     = o->showname;
    c->enabled      = o->enabled;
    c->shared_hilite = o->shared_hilite;
    c->icon         = o->icon;
    c->selectedline = o->selectedline;
    c->locktext     = o->locktext;
    c->wide_margins = o->wide_margins;
    c->fixed_lh     = o->fixed_lh;
    c->show_lines   = o->show_lines;
    c->auto_tab     = o->auto_tab;
    c->dont_search  = o->dont_search;
    c->cant_delete  = o->cant_delete;
    c->shared_text  = o->shared_text;
    c->textstyle    = o->textstyle;
    c->scroll       = o->scroll;
    /* SEPT PROPRIÉTÉS QUE CETTE LISTE AVAIT LAISSÉES DERRIÈRE.
     *
     * Une recopie écrite champ par champ dérive : chaque propriété ajoutée à
     * Object depuis des mois devait être ajoutée ici aussi, et ne l'a pas
     * été. Mesuré, en posant les propriétés puis en copiant-collant :
     *
     *   text_align       source 2     copie 0     PERDU
     *   auto_select      source 1     copie 0     PERDU
     *   multiple_lines   source 1     copie 0     PERDU
     *   dont_wrap        source 1     copie 0     PERDU
     *   textheight       source 27    copie 0     PERDU
     *   family           source 5     copie 0     PERDU
     *   titlewidth       source 48    copie 0     PERDU
     *
     * Cinq de ces sept étaient là bien avant la famille : un champ « ne pas
     * couper les mots », copié-collé, se remettait à couper. Sans message,
     * et sur un objet qui a l'air identique.
     *
     * tests/harnais/clonepart.c compare désormais TOUS les champs scalaires
     * d'un original et de sa copie, un par un. C'est lui la garde : ajouter
     * un champ à Object sans l'ajouter ici fera tomber la suite. */
    c->text_align    = o->text_align;
    c->auto_select   = o->auto_select;
    c->multiple_lines = o->multiple_lines;
    c->dont_wrap     = o->dont_wrap;
    c->textheight    = o->textheight;
    c->family        = o->family;
    c->titlewidth    = o->titlewidth;

    /* Les plages de style : chaque nom de police est duppé à son tour, sinon
     * deux objets partageraient le même pointeur et le second hc_free()
     * libérerait une seconde fois. */
    if (o->runs.n > 0 && runs_room(&c->runs, o->runs.n)) {
        for (int i = 0; i < o->runs.n; i++) {
            c->runs.v[i] = o->runs.v[i];
            c->runs.v[i].font = dupstr(o->runs.v[i].font);
            /* .color part avec la copie de structure ci-dessus. */
        }
        c->runs.n = o->runs.n;
    }

    return c;
}

/* Définies avec le reste du presse-papiers, plus bas : un bouton emporte son
 * icône comme une carte emporte les siennes. */
static void clip_bg_clear(void);
static void clip_collect_icon_de(Object *stack, Object *p);
static void transplant_icons_objet(Object *stack, Object *part);

int hc_copy_part(Object *o)
{
    Object *c = clone_part(o);
    if (!c) return 0;

    /* Un champ de fond NON PARTAGÉ garde son texte dans chaque carte, pas
     * dans l'objet. Ce qu'on voit à l'écran vient donc de la carte courante :
     * c'est ce texte-là qu'il faut emporter, et non le contenu par défaut de
     * l'objet, qui est souvent vide. */
    if (o->type == OBJ_FIELD && !o->shared_text &&
        o->owner && o->owner->type == OBJ_BACKGROUND) {
        const char *seen = hc_field_text(o);
        if (seen && *seen) { free(c->contents); c->contents = dupstr(seen); }
    }

    if (g_clipboard) hc_free(g_clipboard);
    /* LE BOUTON EMPORTE SON ICÔNE, comme une carte emporte les siennes.
     *
     * clip_bg_clear remet aussi la table d'icônes à zéro : sans cet appel, un
     * bouton copié après une carte hériterait des icônes de celle-ci. */
    clip_bg_clear();
    clip_collect_icon_de(owning_stack(o), o);

    g_clipboard = c;
    return 1;
}

int hc_cut_part(Object *o)
{
    if (!hc_copy_part(o)) return 0;
    return hc_delete_part(o);
}

Object *hc_paste_part(Object *owner)
{
    if (!g_clipboard || !owner) return NULL;
    if (owner->type != OBJ_CARD && owner->type != OBJ_BACKGROUND) return NULL;

    Object *c = clone_part(g_clipboard);
    if (!c) return NULL;

    /* Identifiant NEUF. Deux objets de même id rendraient « field id 42 »
     * ambigu, et hc_save écrirait deux fois la même clé. */
    c->owner = owner;
    c->id = id_neuf(owning_stack(owner));

    /* Le propriétaire décide de la nature : coller sur une carte un bouton
     * pris sur un fond en fait un bouton de carte. C'est le comportement
     * d'HyperCard, et la seule lecture cohérente — l'objet vit désormais là.
     * Un champ ne peut être « partagé » que sur un fond. */
    if (owner->type == OBJ_CARD) c->shared_text = 0;

    /* Décalage si la place est déjà prise, pour que le collé ne se cache pas
     * exactement derrière l'original. HyperCard fait de même. */
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (p->type == c->type && p->x == c->x && p->y == c->y) {
            c->x += 8; c->y += 8;
            i = -1;                     /* re-vérifier depuis le début */
        }
    }

    add_part(owner, c);

    /* Et son icône arrive avec lui. Voir transplant_icons_objet : sans cela le
     * numéro désignait, dans la pile d'arrivée, un autre dessin ou aucun. */
    transplant_icons_objet(owning_stack(owner), c);
    return c;
}

/* ==================== copier-coller de CARTES ====================
 *
 * Une carte emporte plus qu'un bouton : ses boutons et champs de carte, son
 * calque de peinture, sa marque, et surtout ses bgtexts — le texte non partagé
 * des champs de FOND, qui appartient à la carte et non au fond. L'oublier
 * donnerait une copie visuellement identique mais vide de son contenu propre.
 *
 * Le FOND, lui, n'est pas copié : une carte s'appuie dessus, elle ne le
 * possède pas. Le clone retient donc son IDENTIFIANT, jamais son pointeur —
 * le presse-papiers promet de survivre à la fermeture d'une pile, et un
 * pointeur de fond y deviendrait mort. On résout au collage.
 *
 * Conséquence assumée : coller dans une autre pile échoue, faute d'y trouver
 * ce fond. C'est le périmètre choisi ; cloner un fond manquant se ferait ici,
 * et nulle part ailleurs.
 */

/* Le fond de la carte au presse-papiers, sous DEUX formes.
 *
 * `copie` est une copie détachée, que le presse-papiers possède : c'est elle
 * qui permet de recréer le fond dans une pile qui ne l'a pas. `vivant` est le
 * fond réel, emprunté, avec la pile où il se trouve — il sert à reconnaître
 * qu'on colle là où l'on a copié, et donc à réutiliser plutôt qu'à dupliquer.
 *
 * `vivant` n'est jamais DÉRÉFÉRENCÉ, seulement comparé aux fonds de la pile
 * visée. Et hc_clipboard_stack_closing l'efface quand sa pile disparaît, ce
 * qui interdit même la comparaison avec un pointeur mort. */
static Object *g_clip_bg_copy  = NULL;   /* possédée */
static Object *g_clip_bg_live  = NULL;   /* empruntée, ou NULL */
static Object *g_clip_bg_stack = NULL;   /* pile de g_clip_bg_live */

/* Les icônes que la carte copiée utilise, et qui appartiennent à SA pile.
 *
 * Un bouton ne retient qu'un numéro, et ce numéro désigne le catalogue de sa
 * pile d'origine : collé ailleurs, il ne montrerait plus rien. Les icônes
 * d'origine, elles, existent partout — inutile de les transporter, et c'est
 * automatique puisqu'on ne prend que ce que hc_icon_get trouve dans la pile. */
static struct StackIcon *g_clip_icons  = NULL;
static int               g_clip_nicons = 0;

/* Défini plus bas, avec les autres crochets d'icônes, mais la transplantation
 * en a besoin ici. */

/* Copie profonde des bgtexts d'une carte vers une autre. */
static void clone_bgtexts(Object *dst, Object *src)
{
    if (src->nbgtexts <= 0) return;

    dst->bgtexts = calloc((size_t)src->nbgtexts, sizeof *dst->bgtexts);
    if (!dst->bgtexts) hc_memoire_epuisee("textes de fond d'une carte copiée");
    dst->capbgtexts = src->nbgtexts;

    for (int i = 0; i < src->nbgtexts; i++) {
        dst->bgtexts[i].field_id = src->bgtexts[i].field_id;
        dst->bgtexts[i].text     = dupstr(src->bgtexts[i].text);
        memset(&dst->bgtexts[i].runs, 0, sizeof dst->bgtexts[i].runs);

        /* Chaque nom de police est duppé à son tour : partager le pointeur
         * ferait libérer deux fois au second hc_free. Même raison que dans
         * clone_part. */
        struct RunList *sr = &src->bgtexts[i].runs;
        if (sr->n > 0 && runs_room(&dst->bgtexts[i].runs, sr->n)) {
            for (int k = 0; k < sr->n; k++) {
                dst->bgtexts[i].runs.v[k]      = sr->v[k];
                dst->bgtexts[i].runs.v[k].font = dupstr(sr->v[k].font);
            }
            dst->bgtexts[i].runs.n = sr->n;
        }
    }
    dst->nbgtexts = src->nbgtexts;
}

/* Clone d'une couche — carte ou fond — avec ses boutons et ses champs.
 * Détachée, identifiants inchangés : c'est la pose qui en attribue de neufs. */
static Object *clone_layer(Object *o, ObjType type)
{
    if (!o || o->type != type) return NULL;

    /* Comme clone_part : NULL se traite, l'arrêt non. */
    Object *c = calloc(1, sizeof(Object));
    if (!c) return NULL;

    c->type   = type;
    c->id     = o->id;             /* remplacé à la pose */
    c->owner  = NULL;              /* détachée */
    c->bg     = NULL;              /* résolu à la pose */

    c->name   = dupstr(o->name);
    c->script = dupstr(o->script);
    /* Pas de recopie de l'arbre : ses jetons pointeraient dans le script de
     * l'ORIGINAL. Le clone réanalysera le sien à la première demande. */
    c->arbre = c->reserve = c->lot = NULL;
    c->arbre_sain = 0;
    c->paint  = dupstr(o->paint);
    c->marked = o->marked;
    /* Les deux verrous suivent la copie. « cantDelete » surtout : une carte
     * protégée dont la copie ne l'est plus offre un contournement en deux
     * gestes — copier, coller, supprimer l'original… qui reste protégé, mais
     * le duplicata, lui, ne l'était pas. */
    c->dont_search = o->dont_search;
    c->cant_delete = o->cant_delete;

    for (int i = 0; i < o->nparts; i++) {
        Object *p = clone_part(o->parts[i]);
        if (!p) continue;          /* clone_part ne prend que boutons et champs */
        p->owner = c;
        add_part(c, p);
    }

    if (type == OBJ_CARD) {
        clone_bgtexts(c, o);
        /* L'allumage par carte suit la carte, comme son texte non partagé.
         * Copie plate : ni chaînes ni plages de style à dupliquer. */
        if (o->nbghilites > 0) {
            c->bghilites = calloc((size_t)o->nbghilites, sizeof *c->bghilites);
            if (c->bghilites) {
                memcpy(c->bghilites, o->bghilites,
                       (size_t)o->nbghilites * sizeof *c->bghilites);
                c->nbghilites = c->capbghilites = o->nbghilites;
            }
        }
    }
    return c;
}

/* Réétiquette les bgtexts d'une carte quand son fond a changé d'exemplaire.
 *
 * Les bgtexts désignent les champs de fond par IDENTIFIANT, et un fond cloné
 * en reçoit de neufs : sans cette étape, le texte non partagé de la carte
 * n'appartiendrait plus à personne et disparaîtrait de l'écran.
 *
 * On travaille par POSITION, non par identifiant : un clone conserve l'ordre
 * de ses parts, donc l'indice i de l'un désigne le même champ que l'indice i
 * de l'autre. C'est la seule correspondance qui tienne, les identifiants
 * étant précisément ce qui a changé. */
static void remap_bgtexts(Object *card, Object *bgsrc, Object *bgdst)
{
    if (!card || !bgsrc || !bgdst || bgsrc == bgdst) return;

    for (int i = 0; i < card->nbgtexts; i++) {
        int idx = -1;
        for (int k = 0; k < bgsrc->nparts; k++)
            if (bgsrc->parts[k]->id == card->bgtexts[i].field_id) { idx = k; break; }

        if (idx >= 0 && idx < bgdst->nparts)
            card->bgtexts[i].field_id = bgdst->parts[idx]->id;
    }

    /* L'allumage par carte désigne les boutons de fond de la même façon, et se
     * perdrait tout autant : une case cochée redeviendrait vide. */
    for (int i = 0; i < card->nbghilites; i++) {
        int idx = -1;
        for (int k = 0; k < bgsrc->nparts; k++)
            if (bgsrc->parts[k]->id == card->bghilites[i].button_id) { idx = k; break; }

        if (idx >= 0 && idx < bgdst->nparts)
            card->bghilites[i].button_id = bgdst->parts[idx]->id;
    }
}

/* Pose un clone de couche dans une pile, avec des identifiants neufs.
 *
 * Deux objets de même id rendraient « card id 12 » ambigu, et hc_save
 * écrirait deux fois la même clé. */
static Object *place_layer_clone(Object *stack, Object *modele, ObjType type,
                                 Object *bg, Object *apres)
{
    Object *c = clone_layer(modele, type);
    if (!c) return NULL;

    c->owner = stack;
    c->bg    = bg;

    add_part(stack, c);            /* d'abord en fin, puis on le remonte */

    /* ON NUMÉROTE APRÈS AVOIR ATTACHÉ, ET PAS AVANT.
     *
     * Le repli d'id_neuf cherche un trou DANS LA PILE. Tant que la couche
     * n'y est pas, deux parts de suite reçoivent le même trou — on aurait
     * échangé un identifiant illisible contre un doublon, ce qui est pire.
     * L'ordre n'est donc pas cosmétique : c'est lui qui rend le repli juste.
     *
     * Les identifiants que clone_layer a recopiés du modèle sont encore en
     * place à cet instant ; id_libre_dans les compte comme pris et les
     * évite, ce qui est exactement ce qu'on veut. */
    c->id = id_neuf(stack);
    for (int i = 0; i < c->nparts; i++) c->parts[i]->id = id_neuf(stack);

    /* Insertion juste après `apres`, comme HyperCard qui colle derrière la
     * carte courante. parts[] mêle fonds et cartes : on décale bêtement, la
     * position relative des fonds n'ayant aucune importance. */
    if (apres) {
        int ia = -1;
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i] == apres) { ia = i; break; }
        if (ia >= 0) {
            for (int i = stack->nparts - 1; i > ia + 1; i--)
                stack->parts[i] = stack->parts[i - 1];
            stack->parts[ia + 1] = c;
        }
    }
    return c;
}

static void clip_bg_clear(void)
{
    if (g_clip_bg_copy) hc_free(g_clip_bg_copy);
    g_clip_bg_copy  = NULL;
    g_clip_bg_live  = NULL;
    g_clip_bg_stack = NULL;

    for (int i = 0; i < g_clip_nicons; i++) free(g_clip_icons[i].name);
    free(g_clip_icons);
    g_clip_icons  = NULL;
    g_clip_nicons = 0;
}

/* Trace du transport d'icônes, éteinte. Elle était restée à 1 depuis la mise
 * au point du transport : l'application expédiée écrivait sur stderr à chaque
 * copie et à chaque collage. Mettre à 1 pour la rallumer. */
#define HC_TRACE_ICONS 0

/* Ramasse l'icône de pile qu'emploie CE bouton-ci. */
static void clip_collect_icon_de(Object *stack, Object *p)
{
    if (!stack || !p) return;
    if (p->type != OBJ_BUTTON || p->icon == 0) return;

    struct StackIcon *src = hc_icon_get(stack, p->icon);
    if (!src) return;                           /* icône d'origine : partout */

    for (int k = 0; k < g_clip_nicons; k++)
        if (g_clip_icons[k].id == p->icon) return;

    struct StackIcon *t = realloc(g_clip_icons,
                                  (size_t)(g_clip_nicons + 1) * sizeof *t);
    if (!t) return;
    g_clip_icons = t;

    memset(&g_clip_icons[g_clip_nicons], 0, sizeof *g_clip_icons);
    g_clip_icons[g_clip_nicons].id   = src->id;
    g_clip_icons[g_clip_nicons].name = dupstr(src->name);
    memcpy(g_clip_icons[g_clip_nicons].bits, src->bits, HC_ICON_BYTES);
    g_clip_nicons++;
#if HC_TRACE_ICONS
    fprintf(stderr, "[icone] ramassee %d \"%s\"\n",
            src->id, src->name ? src->name : "");
#endif
}

/* Ramasse dans `layer` les icônes de pile qu'utilisent ses boutons. */
static void clip_collect_icons(Object *stack, Object *layer)
{
    if (!stack || !layer) return;

#if HC_TRACE_ICONS
    fprintf(stderr, "[icone] examen de %s \"%s\" : %d part(s)\n",
            layer->type == OBJ_CARD ? "la carte" : "le fond",
            layer->name ? layer->name : "", layer->nparts);
    for (int i = 0; i < layer->nparts; i++)
        fprintf(stderr, "[icone]   part %d type=%d icon=%d \"%s\"\n",
                i, layer->parts[i]->type, layer->parts[i]->icon,
                layer->parts[i]->name ? layer->parts[i]->name : "");
#endif

    for (int i = 0; i < layer->nparts; i++)
        clip_collect_icon_de(stack, layer->parts[i]);
}

/* Un numéro libre dans cette pile, hors du catalogue d'origine, ET hors des
 * numéros que le presse-papiers n'a pas encore posés.
 *
 * Ce dernier point n'est pas un détail : si l'icône 1500 doit être renumérotée
 * en 1000 alors qu'une autre du même lot porte déjà 1000 et attend son tour,
 * la seconde écraserait la première — hc_icon_add remplace sur numéro égal. */
static int icon_free_id_in(Object *stack)
{
    for (int id = 1000; id < 100000; id++) {
        if (hc_icon_get(stack, id))  continue;
        if (icon_id_is_builtin(id))  continue;

        int reserve = 0;
        for (int k = 0; k < g_clip_nicons && !reserve; k++)
            if (g_clip_icons[k].id == id) reserve = 1;
        if (reserve) continue;

        return id;
    }
    return 0;
}

static void remap_button_icons(Object *layer, int oldid, int newid)
{
    if (!layer || oldid == newid) return;
    for (int i = 0; i < layer->nparts; i++)
        if (layer->parts[i]->type == OBJ_BUTTON && layer->parts[i]->icon == oldid)
            layer->parts[i]->icon = newid;
}

/* Installe dans la pile d'arrivée les icônes du presse-papiers.
 *
 * Trois cas par icône :
 *   — le numéro est libre : on la pose telle quelle ;
 *   — le numéro est pris par une icône IDENTIQUE : on réutilise, rien à faire.
 *     C'est le cas ordinaire quand on colle là où l'on a copié, et aussi quand
 *     on colle deux fois de suite dans la même pile ;
 *   — le numéro est pris par une AUTRE icône : on en prend un libre et l'on
 *     réétiquette les boutons. Deux piles chargées de fichiers différents
 *     peuvent parfaitement numéroter deux dessins distincts de la même façon ;
 *     réutiliser aveuglément mettrait la mauvaise image sur le bouton. */
/* `bg` ne doit être passé que si le fond vient d'être RECRÉÉ.
 *
 * Un fond réutilisé est partagé par toutes les cartes de la pile : réétiqueter
 * ses boutons changerait les icônes de chacune d'elles. Le cas ne se présente
 * pas tant que réutilisation rime avec icônes identiques — mais il suffit
 * d'avoir retouché une icône entre le copier et le coller pour que les bits
 * diffèrent, et l'on abîmerait la pile entière pour une carte collée. */
/* Poser l'entrée `i` du presse-papiers dans `stack`, et dire sous quel numéro.
 * Rend 0 s'il n'y a rien à faire ou si la pose échoue. */
static int pose_une_icone(Object *stack, int i, int *newid_out)
{
    int oldid = g_clip_icons[i].id;
    int newid = oldid;

    struct StackIcon *ex = hc_icon_get(stack, oldid);
    int identique = ex &&
        memcmp(ex->bits, g_clip_icons[i].bits, HC_ICON_BYTES) == 0;

    if (!identique) {
        if (ex) {
            newid = icon_free_id_in(stack);
#if HC_TRACE_ICONS
            fprintf(stderr, "[icone] %d deja pris par un autre dessin"
                            " -> %d\n", oldid, newid);
#endif
            if (!newid) return 0;               /* on laisse le numéro mort */
        }
        struct StackIcon *e = hc_icon_add(stack, newid, g_clip_icons[i].name);
#if HC_TRACE_ICONS
        fprintf(stderr, "[icone] pose %d \"%s\" -> %s\n",
                newid, g_clip_icons[i].name ? g_clip_icons[i].name : "",
                e ? "ok" : "ECHEC");
#endif
        if (!e) return 0;
        memcpy(e->bits, g_clip_icons[i].bits, HC_ICON_BYTES);
    }
#if HC_TRACE_ICONS
    else fprintf(stderr, "[icone] %d deja presente a l'identique\n", oldid);
#endif

    *newid_out = newid;
    return 1;
}

static void transplant_icons(Object *stack, Object *card, Object *bg)
{
#if HC_TRACE_ICONS
    fprintf(stderr, "[icone] transplantation de %d icone(s)\n", g_clip_nicons);
#endif
    for (int i = 0; i < g_clip_nicons; i++) {
        int oldid = g_clip_icons[i].id, newid;
        if (!pose_une_icone(stack, i, &newid)) continue;
        remap_button_icons(card, oldid, newid);
        remap_button_icons(bg,   oldid, newid);
    }
}

/* LA MÊME TRANSPLANTATION, POUR UN OBJET SEUL.
 *
 * Copier un BOUTON — et non une carte — n'emportait aucune icône : ni
 * hc_copy_part ni hc_paste_part ne les regardaient. Le bouton collé gardait
 * son numéro, et à l'arrivée ce numéro appartenait à un autre dessin, ou à
 * aucun. Mesuré : le bouton affichait l'icône de la pile de destination, et
 * la sienne était perdue.
 *
 * C'est le même défaut que pour les cartes, corrigé pour elles seules. Un
 * chemin sur deux, c'est le genre de moitié qui ne se voit pas — jusqu'à ce
 * qu'on copie un bouton. */
static void transplant_icons_objet(Object *stack, Object *part)
{
    if (!stack || !part) return;
    for (int i = 0; i < g_clip_nicons; i++) {
        int oldid = g_clip_icons[i].id, newid;
        if (!pose_une_icone(stack, i, &newid)) continue;
        if (part->type == OBJ_BUTTON && part->icon == oldid) part->icon = newid;
    }
}

int hc_copy_card(Object *card)
{
    if (!card || card->type != OBJ_CARD) return 0;

    Object *c = clone_layer(card, OBJ_CARD);
    if (!c) return 0;

    /* Le fond part AUSSI au presse-papiers, en copie. C'est ce qui rend le
     * collage dans une autre pile possible : la carte s'appuie sur un fond
     * qui, là-bas, n'existe pas. */
    Object *bgc = card->bg ? clone_layer(card->bg, OBJ_BACKGROUND) : NULL;

    if (g_clipboard) hc_free(g_clipboard);
    clip_bg_clear();

    /* Après clip_bg_clear, qui remet la table à zéro. */
    clip_collect_icons(card->owner, card);
    clip_collect_icons(card->owner, card->bg);

    g_clipboard     = c;
    g_clip_bg_copy  = bgc;
    g_clip_bg_live  = card->bg;
    g_clip_bg_stack = card->owner;
    return 1;
}

/* Couper : copier puis supprimer.
 *
 * hc_delete_card fait disparaître le fond avec sa dernière carte. Le fond
 * vivant est donc oublié ici, mais sa COPIE reste au presse-papiers : coller
 * le recréera. C'est précisément ce que la copie du fond apporte. */
int hc_cut_card(Object *card)
{
    if (!hc_copy_card(card)) return 0;
    g_clip_bg_live  = NULL;
    g_clip_bg_stack = NULL;
    return hc_delete_card(card);
}

Object *hc_paste_card(Object *stack)
{
    if (!g_clipboard || g_clipboard->type != OBJ_CARD) return NULL;
    if (!stack || stack->type != OBJ_STACK) return NULL;

    /* Le fond existe-t-il déjà ici ?
     *
     * On le reconnaît par IDENTITÉ, pas par identifiant : deux piles chargées
     * de fichiers différents peuvent porter le même numéro sans rien avoir de
     * commun, et rattacher la carte au mauvais fond lui ferait perdre sa mise
     * en page sans le moindre avertissement. */
    Object *bg = NULL;
    int bg_recree = 0;               /* le fond a-t-il été créé à l'instant ? */
    if (g_clip_bg_live && g_clip_bg_stack == stack) {
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i] == g_clip_bg_live) { bg = g_clip_bg_live; break; }
    }

    /* Absent : on le recrée depuis la copie. Et on le retient comme fond
     * vivant de cette pile, pour que coller une deuxième fois la même carte
     * réutilise ce fond au lieu d'en empiler un second. */
    if (!bg && g_clip_bg_copy) {
        bg = place_layer_clone(stack, g_clip_bg_copy, OBJ_BACKGROUND, NULL, NULL);
        if (!bg) return NULL;
        bg_recree       = 1;
        g_clip_bg_live  = bg;
        g_clip_bg_stack = stack;
    }
    if (!bg) return NULL;

    /* LA CARTE COURANTE PAR L'ACCESSEUR PUBLIC, pas par la globale.
     *
     * g_current_card est `static` dans hc_core.c, et la lire d'ici aurait
     * demandé un septième symbole partagé. C'est une simple LECTURE, et
     * hc_current_card() la sert déjà — elle est dans hc_core.h depuis
     * toujours. Un symbole de moins pour rien du tout. */
    Object *cur = hc_current_card();
    if (cur && cur->owner != stack) cur = NULL;

    Object *c = place_layer_clone(stack, g_clipboard, OBJ_CARD, bg, cur);
    if (!c) return NULL;

    /* Les bgtexts du clone désignent les champs de la COPIE du fond ; il faut
     * les faire pointer sur ceux du fond réellement utilisé. Sans effet quand
     * les deux se confondent. */
    remap_bgtexts(c, g_clip_bg_copy, bg);

    /* Les icônes ensuite : elles ne dépendent pas du fond, mais leurs numéros
     * peuvent changer. Le fond n'est réétiqueté QUE s'il vient d'être recréé :
     * réutilisé, il appartient aussi aux autres cartes de la pile, et le
     * toucher changerait leurs icônes à toutes. */
    transplant_icons(stack, c, bg_recree ? bg : NULL);
    return c;
}

/* Dupliquer : ne passe pas par le presse-papiers, qui garde donc ce qu'il
 * avait. C'est ce qu'on attend d'une commande « Dupliquer ». Le fond est
 * partagé, jamais dupliqué : la carte reste dans sa pile. */
Object *hc_duplicate_card(Object *card)
{
    if (!card || card->type != OBJ_CARD || !card->owner) return NULL;
    return place_layer_clone(card->owner, card, OBJ_CARD, card->bg, card);
}

/* Une pile se ferme : oublier ce que le presse-papiers y emprunte.
 *
 * Seul le fond VIVANT est concerné — la carte et la copie du fond sont
 * possédées et survivent. Sans cet oubli, un pointeur mort resterait comparé
 * aux fonds d'une pile future, et une adresse réemployée le ferait passer
 * pour un fond qu'il n'est pas. */
void hc_clipboard_stack_closing(Object *stack)
{
    if (!stack || g_clip_bg_stack != stack) return;
    g_clip_bg_live  = NULL;
    g_clip_bg_stack = NULL;
}

/* Le presse-papiers, tel quel — carte ou part. Ne pas libérer. */
Object *hc_clipboard_part(void) { return g_clipboard; }

/* Un seul presse-papiers pour deux natures : ces deux fonctions disent laquelle
 * il porte. Sans elles, « Coller » devrait déduire du contexte ce qu'il pose,
 * et se tromperait dès qu'une carte a été copiée puis l'outil Bouton choisi. */
int hc_clipboard_has_card(void)
{
    return (g_clipboard && g_clipboard->type == OBJ_CARD) ? 1 : 0;
}

int hc_clipboard_has_part(void)
{
    return (g_clipboard && (g_clipboard->type == OBJ_BUTTON ||
                            g_clipboard->type == OBJ_FIELD)) ? 1 : 0;
}

void hc_clipboard_clear(void)
{
    if (g_clipboard) hc_free(g_clipboard);
    g_clipboard = NULL;
}
/* ==================== l'oubli d'un objet mort ==================== *
 *
 * Voir hc_interne.h : le presse-papiers retient deux pointeurs qu'il ne
 * possède pas, et hc_free vient nous dire qu'ils meurent.
 *
 * On ne déréférence PAS `mort` — comparer des adresses et remettre à zéro,
 * rien de plus. L'objet est déjà en cours de libération quand on arrive ici.
 * ================================================================= */
void hc_pp_oublie(Object *mort)
{
    if (!mort) return;
    if (g_clip_bg_live  == mort) g_clip_bg_live  = NULL;
    if (g_clip_bg_stack == mort) g_clip_bg_stack = NULL;
}
