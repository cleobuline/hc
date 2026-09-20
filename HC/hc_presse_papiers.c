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
static Object *g_clip_bg_live  = NULL;   /* le fond D'ORIGINE, emprunté */
static Object *g_clip_bg_stack = NULL;   /* la pile de g_clip_bg_live */

/* ═══ CE QU'ON A DÉJÀ PORTÉ, ET OÙ ══════════════════════════════════════
 *
 * LE DÉFAUT QUE CECI CORRIGE. Coller dans une autre pile deux cartes qui
 * PARTAGENT un fond y créait DEUX fonds :
 *
 *     pile B : un fond « Commun », les cartes Une et Deux dessus
 *     copier Une, coller dans A   -> A gagne un fond « Commun »
 *     copier Deux, coller dans A  -> A gagne un SECOND fond « Commun »
 *
 * Les deux cartes se retrouvaient sur des fonds distincts, alors qu'elles en
 * partageaient un. Modifier le fond n'en changeait plus qu'une, et le texte
 * des champs de fond cessait d'être commun. Sur une pile où l'on rapatrie
 * des cartes une par une, on finit avec autant de fonds que de cartes.
 *
 * POURQUOI LA MÉMOIRE EXISTANTE NE SUFFISAIT PAS. g_clip_bg_live retenait
 * déjà « le fond, et la pile où il est », et hc_paste_card l'écrasait après
 * avoir recréé un fond, si bien que coller DEUX FOIS LA MÊME CARTE
 * réutilisait bien. Mais hc_copy_card la repose à chaque copie : la
 * correspondance était attachée au CONTENU DU PRESSE-PAPIERS, et mourait
 * avec lui. Or ce qu'il faut retenir n'a rien à voir avec le presse-papiers ;
 * c'est une propriété des PILES : « le fond F de B a déjà été porté dans A,
 * et c'est celui-ci ».
 *
 * D'où cette table, qui survit aux copies. Elle ne DÉRÉFÉRENCE jamais
 * src_bg ni src_pile — seulement des comparaisons d'adresses — et le fond
 * recréé est cherché dans les parts[] de sa pile avant d'être rendu : s'il a
 * été supprimé entre-temps, on en recrée un, ce qui est la bonne réponse.
 *
 * SA TAILLE EST BORNÉE, et le dire vaut mieux que de le cacher : au-delà de
 * trente-deux correspondances vivantes, la plus ancienne part. Le
 * comportement retombe alors sur celui d'avant — un fond de plus — au lieu
 * de refuser le collage. Trente-deux, c'est trente-deux couples (fond, pile
 * d'accueil) simultanés ; une pile qui en demanderait plus aurait des
 * problèmes plus grands que celui-ci. */
#define HC_BG_PORTES_MAX 32
typedef struct {
    /* SOUS QUELLE IDENTITÉ le fond copié est retenu : le fond d'origine tant
     * qu'il vit, la copie du presse-papiers une fois sa pile fermée. Voir
     * hc_paste_card, qui choisit — aucune des deux ne couvre à elle seule la
     * durée voulue. Jamais DÉRÉFÉRENCÉE : seulement comparée. */
    Object *src_bg;
    Object *src_pile;   /* la pile du fond d'origine, pour purger à sa fermeture */
    Object *pile;       /* la pile d'accueil */
    Object *cree;       /* le fond qu'on y a recréé */
} BgPorte;
static BgPorte g_bg_portes[HC_BG_PORTES_MAX];
static int     g_nbg_portes = 0;

/* Ce fond a-t-il déjà été porté dans cette pile ? NULL sinon. */
static Object *bg_deja_porte(Object *src_bg, Object *pile)
{
    if (!src_bg || !pile) return NULL;
    for (int i = 0; i < g_nbg_portes; i++) {
        if (g_bg_portes[i].src_bg != src_bg) continue;
        if (g_bg_portes[i].pile   != pile)   continue;
        /* Toujours là ? L'utilisateur a pu le supprimer depuis. */
        for (int k = 0; k < pile->nparts; k++)
            if (pile->parts[k] == g_bg_portes[i].cree) return g_bg_portes[i].cree;
        return NULL;
    }
    return NULL;
}

static void bg_note_porte(Object *src_bg, Object *src_pile,
                          Object *pile, Object *cree)
{
    if (!src_bg || !pile || !cree) return;
    for (int i = 0; i < g_nbg_portes; i++)
        if (g_bg_portes[i].src_bg == src_bg && g_bg_portes[i].pile == pile) {
            g_bg_portes[i].cree = cree;
            g_bg_portes[i].src_pile = src_pile;
            return;
        }
    if (g_nbg_portes == HC_BG_PORTES_MAX) {
        memmove(&g_bg_portes[0], &g_bg_portes[1],
                sizeof g_bg_portes[0] * (HC_BG_PORTES_MAX - 1));
        g_nbg_portes--;
    }
    g_bg_portes[g_nbg_portes].src_bg   = src_bg;
    g_bg_portes[g_nbg_portes].src_pile = src_pile;
    g_bg_portes[g_nbg_portes].pile     = pile;
    g_bg_portes[g_nbg_portes].cree     = cree;
    g_nbg_portes++;
}

/* ═══ RECONNAÎTRE UN FOND QU'ON Y A DÉJÀ PORTÉ, APRÈS UNE RELANCE ══════
 *
 * CE QUE LA TABLE NE PEUT PAS FAIRE. Elle vit en mémoire : elle répond
 * parfaitement tant que l'application tourne, et ne répond plus rien après
 * un redémarrage. Mesuré :
 *
 *     session 1 : copier Une dans B, coller dans A  -> A gagne « Commun »
 *     quitter, relancer
 *     session 2 : copier Deux dans B, coller dans A -> A gagne un SECOND
 *                                                      « Commun »
 *
 * C'est le défaut d'origine, décalé d'une relance. Or rapatrier des cartes
 * une par une est justement ce qu'on fait sur plusieurs jours.
 *
 * OÙ METTRE LA MÉMOIRE. Pas dans un fichier à côté : il faudrait y désigner
 * la pile source, donc par son chemin ou son nom, et l'utilisateur a le
 * droit de renommer ou de déplacer ses piles — la correspondance mentirait
 * au premier renommage, en silence. La seule mémoire qui ne mente pas est
 * CELLE QUE LE FOND PORTE SUR LUI, dans la pile d'accueil, et qui part avec
 * elle quoi qu'il arrive à la pile d'origine.
 *
 * CE QU'ON COMPARE : ce que la carte PERDRAIT si on la rattachait au mauvais
 * fond, et c'est cela qui fait la sûreté du procédé. Le nom du fond, sa
 * peinture, et ses parts — nombre, type, nom et rectangle, dans le même
 * ordre. Un fond qui répond oui à tout cela ressemble, à l'écran, EXACTEMENT
 * à celui d'où la carte vient.
 *
 * CE QU'ON NE COMPARE PAS, et pourquoi :
 *
 *   - l'IDENTIFIANT, parce qu'il n'est pas toujours reprenable. Le compteur
 *     repart à 1 dans chaque session, donc deux piles écrites séparément se
 *     disputent les petits numéros : porter un fond « id 2 » dans une pile
 *     qui a déjà un « id 2 » oblige à en donner un neuf. Exiger l'égalité
 *     ferait échouer la reconnaissance dans le cas le plus ordinaire qui
 *     soit. Il sert donc de DÉPARTAGE quand plusieurs fonds conviennent,
 *     et id_adopte le reprend quand la place est libre — ce qui rend la
 *     reconnaissance certaine dans ce cas-là, et rend au passage leur sens
 *     aux scripts qui disent « bg field id 12 ».
 *
 *   - le SCRIPT, parce qu'il est invisible et qu'il est ce qu'on adapte le
 *     plus volontiers dans la pile d'accueil. L'exiger ferait réapparaître
 *     le fond en double à la première retouche. Il départage, lui aussi.
 *
 * ON REFUSE DE RECONNAÎTRE UN FOND SANS SIGNE PARTICULIER — ni nom, ni
 * peinture, ni parts. Deux fonds vides ne se distinguent par rien, et c'est
 * précisément pour cela qu'il ne faut pas les confondre : la ressemblance ne
 * prouve plus rien, alors qu'un fond de plus ne coûte rien.
 *
 * CE PROCÉDÉ N'EST PAS CERTAIN, et il est fait pour échouer du bon côté : si
 * le fond d'accueil a été retouché depuis, on ne le reconnaît plus et l'on
 * en crée un second — le comportement d'avant, agaçant. Jamais l'inverse :
 * on ne rattache pas une carte à un fond qui ne ressemble pas au sien. */

static int meme_texte(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b) == 0;
}

/* Ce fond a-t-il de quoi être reconnu ? */
static int fond_a_un_signe(const Object *f)
{
    if (!f) return 0;
    if (f->nparts > 0) return 1;
    if (f->name  && f->name[0])  return 1;
    if (f->paint && f->paint[0]) return 1;
    return 0;
}

/* Les deux fonds se ressemblent-ils AU POINT QU'UNE CARTE NE VERRAIT PAS LA
 * DIFFÉRENCE ? L'ordre des parts compte : c'est par POSITION que
 * remap_bgtexts rattache le texte par carte aux champs du fond retenu. */
static int fond_meme_apparence(const Object *a, const Object *b)
{
    if (!a || !b) return 0;
    if (a->nparts != b->nparts) return 0;
    if (!meme_texte(a->name,  b->name))  return 0;
    if (!meme_texte(a->paint, b->paint)) return 0;

    for (int i = 0; i < a->nparts; i++) {
        const Object *pa = a->parts[i], *pb = b->parts[i];
        if (pa->type != pb->type) return 0;
        if (pa->x != pb->x || pa->y != pb->y) return 0;
        if (pa->w != pb->w || pa->h != pb->h) return 0;
        if (!meme_texte(pa->name, pb->name)) return 0;
    }
    return 1;
}

/* Le fond de cette pile qui est le portrait du modèle, ou NULL.
 *
 * Plusieurs peuvent convenir — la pile contient déjà deux copies du même
 * fond, par exemple parce qu'une version antérieure les a dupliquées. On
 * prend alors celui qui a gardé l'identifiant d'origine, sinon celui dont le
 * script est le même, sinon le premier venu : ils se valent, puisqu'ils se
 * ressemblent tous. */
static Object *fond_reconnu_dans(const Object *modele, Object *pile)
{
    if (!modele || !pile) return NULL;
    if (!fond_a_un_signe(modele)) return NULL;

    Object *premier = NULL, *meme_script = NULL;

    for (int i = 0; i < pile->nparts; i++) {
        Object *f = pile->parts[i];
        if (f->type != OBJ_BACKGROUND) continue;
        if (!fond_meme_apparence(modele, f)) continue;

        if (f->id == modele->id) return f;
        if (!meme_script && meme_texte(f->script, modele->script)) meme_script = f;
        if (!premier) premier = f;
    }
    return meme_script ? meme_script : premier;
}

/* Retire toutes les correspondances qui mentionnent cet objet. */
static void bg_portes_oublie(Object *mort)
{
    int k = 0;
    for (int i = 0; i < g_nbg_portes; i++) {
        BgPorte *e = &g_bg_portes[i];
        if (e->src_bg == mort || e->src_pile == mort ||
            e->pile   == mort || e->cree     == mort) continue;
        g_bg_portes[k++] = *e;
    }
    g_nbg_portes = k;
}

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
     * en page sans le moindre avertissement.
     *
     * DEUX CAS, UNE SEULE QUESTION — « ce fond est-il déjà ici ? » :
     *
     *   - on colle DANS LA PILE D'OÙ L'ON A COPIÉ : le fond d'origine est là,
     *     c'est lui ;
     *   - on colle AILLEURS, PENDANT CETTE SESSION : on l'y a peut-être déjà
     *     porté, et la table des correspondances le dit ;
     *   - on colle AILLEURS, ET ON A REDÉMARRÉ DEPUIS : la table est vide, et
     *     c'est le fond d'accueil lui-même qu'on interroge — il ressemble au
     *     nôtre, ou il ne lui ressemble pas.
     *
     * Le deuxième cas manquait, et c'était tout le défaut : coller deux cartes
     * qui partagent un fond y créait deux fonds. Le troisième manquait aussi,
     * mais il ne se voyait qu'après une relance. */
    Object *bg = NULL;
    int bg_recree = 0;               /* le fond a-t-il été créé à l'instant ? */

    /* SOUS QUELLE IDENTITÉ LA TABLE RETIENT CE FOND — et il en faut DEUX,
     * parce qu'aucune ne couvre à elle seule la durée voulue.
     *
     * g_clip_bg_live est le fond D'ORIGINE. Il traverse les copies
     * successives : copier Une puis Deux, qui partagent un fond, donne deux
     * contenus de presse-papiers mais le même fond vivant — c'est lui qui
     * fait tenir la correspondance d'une copie à l'autre. Mais il s'efface
     * quand sa pile ferme, et il le doit : une adresse libérée ne se compare
     * même plus.
     *
     * g_clip_bg_copy est la COPIE que le presse-papiers possède. Elle ne
     * traverse pas les copies — chacune la remplace — mais elle SURVIT à la
     * mort de la pile source, puisque c'est nous qui la tenons.
     *
     * Le défaut que ceci corrige, mesuré : après la fermeture de la pile
     * source, bg_note_porte recevait NULL et refusait d'enregistrer, si bien
     * que DEUX COLLAGES DE SUITE créaient deux fonds. La reconnaissance par
     * l'apparence l'avait masqué pour un fond qui a un signe distinctif,
     * mais pas pour un fond vide, qu'elle refuse de reconnaître — et c'était
     * la même ligne qui manquait dans les deux cas.
     *
     * L'adresse de la copie ne peut pas être confondue avec celle d'une
     * copie plus ancienne : hc_free purge la table par hc_pp_oublie avant
     * que l'allocateur ne puisse rendre l'adresse à quelqu'un d'autre. */
    Object *cle = g_clip_bg_live ? g_clip_bg_live : g_clip_bg_copy;

    if (g_clip_bg_live && g_clip_bg_stack == stack) {
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i] == g_clip_bg_live) { bg = g_clip_bg_live; break; }
    }
    if (!bg) bg = bg_deja_porte(cle, stack);

    /* Rien en mémoire : on regarde la pile d'accueil. Et l'on NOTE ce qu'on y
     * trouve, pour que les collages suivants passent par l'identité plutôt
     * que par la ressemblance — retoucher le fond après le premier collage ne
     * doit pas défaire la correspondance au milieu d'une session. */
    if (!bg && g_clip_bg_copy) {
        bg = fond_reconnu_dans(g_clip_bg_copy, stack);
        if (bg) bg_note_porte(cle, g_clip_bg_stack, stack, bg);
    }

    /* Absent : on le recrée depuis la copie, et ON NOTE la correspondance —
     * pas en écrasant g_clip_bg_live, qui désigne le fond D'ORIGINE et dont
     * la prochaine copie a besoin. C'est exactement ce que faisait l'ancienne
     * version, et pourquoi la mémoire mourait avec le presse-papiers. */
    if (!bg && g_clip_bg_copy) {
        bg = place_layer_clone(stack, g_clip_bg_copy, OBJ_BACKGROUND, NULL, NULL);
        if (!bg) return NULL;
        bg_recree = 1;

        /* LES IDENTIFIANTS D'ORIGINE, QUAND LA PLACE EST LIBRE.
         *
         * Ils ne servent pas à reconnaître le fond — fond_reconnu_dans ne
         * s'appuie pas dessus, justement parce qu'ils ne sont pas toujours
         * reprenables — mais ils le reconnaissent À COUP SÛR quand ils ont pu
         * être repris. Et ils rendent leur sens aux scripts qui désignent un
         * champ de fond par « bg field id 12 ».
         *
         * Le fond D'ABORD, ses parts ENSUITE, et chacune pour son compte : un
         * numéro déjà pris laisse simplement celui que place_layer_clone
         * vient de donner. C'est un mieux opportuniste, jamais une
         * obligation. */
        id_adopte(stack, bg, g_clip_bg_copy->id);
        for (int i = 0; i < bg->nparts && i < g_clip_bg_copy->nparts; i++)
            id_adopte(stack, bg->parts[i], g_clip_bg_copy->parts[i]->id);

        bg_note_porte(cle, g_clip_bg_stack, stack, bg);
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
    if (!stack) return;
    /* Les correspondances qui la mentionnent — d'un côté comme de l'autre —
     * partent avec elle : leurs pointeurs ne désigneraient plus rien, et on
     * ne doit même pas les COMPARER à une adresse réutilisée depuis. */
    bg_portes_oublie(stack);
    if (g_clip_bg_stack != stack) return;
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
    /* La table retient quatre pointeurs par ligne, et aucun n'est possédé :
     * un fond supprimé à la main, une pile fermée, et la ligne désigne du
     * vide. On la retire plutôt que de risquer une comparaison avec une
     * adresse que l'allocateur a rendue à quelqu'un d'autre. */
    bg_portes_oublie(mort);
}
