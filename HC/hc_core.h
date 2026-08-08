/* hc_core.h — Noyau du clone HyperCard : modèle d'objets et passage de messages.
 *
 * Hiérarchie fidèle à HyperCard :
 *     pile (stack)
 *       └── fonds (backgrounds)   ── boutons/champs de fond
 *       └── cartes (cards)        ── boutons/champs de carte
 *
 * Chaîne de remontée des messages (le coeur du système) :
 *     objet de carte  →  carte  →  fond  →  pile
 *     objet de fond   →  carte  →  fond  →  pile
 *
 * Un gestionnaire qui exécute `pass <message>` laisse le message
 * poursuivre sa route vers le maillon suivant.
 */
#ifndef HC_CORE_H
#define HC_CORE_H

typedef enum {
    OBJ_STACK,
    OBJ_BACKGROUND,
    OBJ_CARD,
    OBJ_BUTTON,
    OBJ_FIELD
} ObjType;

typedef struct Object Object;

/* ---- Styles par plage de caractères ----
 * HyperCard laisse le style, la police et le corps varier à l'intérieur d'un
 * même champ : « set the textStyle of word 3 of line 2 of field 1 to bold ».
 * Le texte porte donc une liste de plages, chacune couvrant un intervalle de
 * caractères [start, start+len) et donnant les attributs qui s'y appliquent.
 * Ce qui n'est couvert par aucune plage prend les attributs du champ entier.
 *
 * Les trois attributs sont indépendants, comme dans HyperCard 2.x : poser un
 * style sur un mot ne touche pas sa police, et réciproquement. Chacun a sa
 * SENTINELLE D'HÉRITAGE, qui veut dire « prends la valeur du champ » :
 *     font  == NULL              -> Object.textfont
 *     size  == 0                 -> Object.textsize
 *     style == HC_STYLE_INHERIT  -> Object.textstyle
 * Le style a besoin d'une sentinelle À PART de zéro : zéro veut dire « plain,
 * explicitement », ce qui doit effacer le gras du champ sur cette plage. Les
 * confondre rendait « set the textStyle of word 3 to plain » sans effet dans
 * un champ gras — la plage était jetée, et le mot reprenait le gras du champ.
 * Une plage dont les trois champs valent leur sentinelle ne dit plus rien et
 * se fait jeter par runs_tidy.
 *
 * Ces sentinelles sont INTERNES : hc_run_attrs les résout avant de rendre la
 * main à l'hôte, qui ne voit que des valeurs effectives. */
struct TextRun { int start, len; int style; int size; char *font; };
struct RunList { struct TextRun *v; int n, cap; };

/* Bits de style. Les trois premiers sont ceux qu'Object.textstyle utilisait
 * déjà, conservés à l'identique pour ne pas casser les piles enregistrées. */
#define HC_BOLD       1
#define HC_ITALIC     2
#define HC_UNDERLINE  4
#define HC_OUTLINE    8
#define HC_SHADOW    16
#define HC_CONDENSE  32
#define HC_EXTEND    64
#define HC_GROUP    128
#define HC_STYLE_MIXED   (-1) /* rendu par une lecture sur plage non homogène */
#define HC_STYLE_INHERIT (-2) /* plage muette sur le style : voir plus haut */

/* texte d'un champ de fond, propre à une carte, avec ses plages de style :
 * un champ de fond non partagé a un texte ET un style par carte. */
struct BgText { int field_id; char *text; struct RunList runs; };

struct Object {
    ObjType  type;
    int      id;
    char    *name;      /* nom de l'objet (peut être NULL) */
    char    *script;    /* script HyperTalk brut */

    Object  *owner;     /* propriétaire : pile pour fond/carte, carte ou fond pour les parts */
    Object  *bg;        /* pour une carte : son fond */

    Object **parts;     /* enfants : fonds+cartes pour la pile, boutons/champs sinon */
    int      nparts;
    int      capparts;

    int      visible;
    int      hilite;    /* bouton allumé (vidéo inverse) ; coché pour checkBox/radio */
    int      autohilite;/* le bouton s'allume automatiquement pendant le clic */
    int      textsize;  /* taille de police du nom (0 = défaut) */
    int      showname;  /* le nom du bouton est-il affiché ? (1 = oui par défaut) */
    int      icon;      /* identifiant de ressource ICON (0 = aucune) */
    int      selectedline; /* ligne choisie d'un bouton popup (1..n, 0 = aucune) */

    /* propriétés de champ */
    int      locktext;       /* le champ est-il non modifiable ? */
    int      wide_margins;   /* marges larges */
    int      fixed_lh;       /* interligne fixe */
    int      show_lines;     /* lignes de guidage visibles */
    int      auto_tab;       /* tab passe au champ suivant */
    int      dont_search;    /* exclu de find */
    int      shared_text;    /* texte partagé entre cartes du même fond */

    char    *textfont;       /* nom de police (NULL = défaut) */
    int      textstyle;      /* bits HC_BOLD… : style du champ entier, valeur
                              * de repli pour tout ce qu'aucune plage ne couvre */
    struct RunList runs;     /* champs : plages de style. Pour un champ de fond
                              * non partagé, c'est le repli ; le style réel de
                              * chaque carte est dans ses bgtexts (voir plus haut) */
    int      scroll;         /* décalage vertical d'un champ scrolling, en pixels */

    /* pour une CARTE : textes propres des champs de fond non partagés */
    struct BgText *bgtexts;
    int      nbgtexts, capbgtexts;
    char    *contents;  /* contenu textuel (champs) ; pour un champ de fond
                         * non partagé, c'est la valeur par défaut : le texte
                         * réel est stocké dans chaque carte (voir bgtexts) */

    /* géométrie : rectangle en coordonnées carte (pixels).
       rect = (left, top, right, bottom) = (x, y, x+w, y+h). */
    int      x, y, w, h;
    char    *style;     /* boutons : rectangle, roundRect, checkBox… ; NULL = défaut */
    char    *paint;     /* cartes/fonds : bitmap peinture encodé base64 (opaque pour le noyau) */
};

/* ---- Construction ---- */
Object *hc_new_stack(const char *name);
Object *hc_new_background(Object *stack, const char *name);
Object *hc_new_card(Object *stack, Object *bg, const char *name);
Object *hc_new_button(Object *owner, const char *name);
Object *hc_new_field(Object *owner, const char *name);
void    hc_set_script(Object *o, const char *script);
void    hc_free(Object *stack);
int     hc_delete_part(Object *o);

/* ---- Contexte d'exécution ---- */
void    hc_set_current_card(Object *card);
/* ---- Hôte : le noyau ne sait pas afficher, il délègue ----
 * Toute sortie du noyau (boîte de message, trace du dispatcher, erreurs)
 * passe par ces callbacks. La console les branche sur printf ; une interface
 * graphique les brancherait sur ses vues. Le noyau reste identique.
 *
 * kind identifie la nature de la ligne :
 *   HC_MSG    contenu de la boîte de message (« put » sans destination)
 *   HC_TRACE  bavardage du dispatcher (activé par hc_trace)
 *   HC_ERR    erreurs et avertissements (« !! », « ?? »)
 *   HC_INFO   retours d'action (« → x ← … », « ♪ beep »)
 * depth est la profondeur d'imbrication des messages (pour l'indentation). */
typedef enum { HC_MSG, HC_TRACE, HC_ERR, HC_INFO } HcLineKind;

typedef struct {
    void (*line)(HcLineKind kind, int depth, const char *text);
    void (*field_changed)(Object *field);   /* champ modifié : rafraîchir l'affichage */

    /* Boîtes de dialogue. L'hôte renvoie un pointeur valide jusqu'au prochain
     * appel ; NULL vaut annulation.
     *   ask     : saisie de texte, deflt peut être vide
     *   answer  : choix parmi 1 à 3 boutons ; b2 et b3 peuvent être NULL,
     *             le dernier fourni est le bouton par défaut */
    const char *(*ask)(const char *prompt, const char *deflt);
    const char *(*answer)(const char *prompt, const char *b1,
                          const char *b2, const char *b3);

    /* Propriétés globales : tout ce que le noyau ne peut pas connaître seul
     * (souris, clavier, écran, horloge). L'hôte renvoie une chaîne valide
     * jusqu'au prochain appel, ou NULL si le nom lui est inconnu.
     * Noms attendus par les scripts d'origine :
     *   mouse      « up » / « down »
     *   mouseLoc   « x,y » en coordonnées carte
     *   optionKey, commandKey, shiftKey   « up » / « down »
     * Le nom arrive sans « the » et sans tenir compte de la casse. */
    const char *(*global_get)(const char *name);
    void        (*global_set)(const char *name, const char *value);

    void (*play_sound)(const char *name);   /* play "boing" */

    /* Appelé à chaque tour de boucle « repeat ». C'est là qu'une interface
     * graphique redessine et rend la main : sans lui, une boucle d'animation
     * monopolise le fil principal et rien ne s'affiche avant la fin. */
    void (*idle)(void);
} HcHost;

/* Installe l'hôte. Passer NULL rétablit l'hôte console par défaut. */
void        hc_set_host(const HcHost *h);

Object *hc_current_card(void);

/* ---- Coeur : envoi d'un message ---- */
/* Renvoie 1 si un gestionnaire a traité le message, 0 sinon. */
int     hc_send(Object *target, const char *message);

/* Exécute une seule ligne de script dans le contexte de la carte courante
   (l'équivalent de la boîte de message d'HyperCard). */
void    hc_do(const char *line);

/* ---- Utilitaires ---- */
const char *hc_typename(ObjType t);
void        hc_describe(Object *o, char *buf, int buflen);
void        hc_trace(int on);

/* Résout une référence d'objet (« button "toto" », « the field "notes" »,
   « this card »…) dans le contexte de la carte courante. NULL si introuvable. */
Object     *hc_resolve(const char *ref);

/* Script brut d'un objet (peut être NULL). */
const char *hc_script_of(Object *o);

/* Pose le contenu textuel d'un champ (pour l'édition interactive). */
void        hc_set_field_text(Object *field, const char *text);
/* Texte effectif d'un champ : propre à la carte courante s'il s'agit d'un
 * champ de fond non partagé. Ne renvoie jamais NULL. */
const char *hc_field_text(Object *field);
/* ---- Plages de style : lecture par l'hôte, pour le rendu ----
 * L'hôte parcourt les plages du champ pour construire son texte attribué.
 * Les intervalles sont en caractères, dans le texte rendu par hc_field_text.
 * Ce qu'aucune plage ne couvre prend le style du champ (Object.textstyle). */
int         hc_run_count(Object *field);
int         hc_run_at(Object *field, int i, int *start, int *len, int *style);
/* Comme hc_run_at, mais rend aussi la police et le corps de la plage. Les
 * sentinelles sont déjà résolues : `font` et `size` valent ceux du champ quand
 * la plage ne dit rien, et ne sont donc jamais NULL ni nuls sans raison.
 * `font` pointe dans le modèle : valable jusqu'à la prochaine modification. */
int         hc_run_attrs(Object *field, int i, int *start, int *len,
                         int *style, int *size, const char **font);

/* ---- Plages de style : écriture en bloc, pour l'éditeur ----
 * Pendant la saisie c'est la vue qui détient le style ; à la fermeture elle
 * reconstruit les plages du noyau. On efface, puis on ajoute dans l'ordre.
 * hc_run_add tolère le désordre et les recouvrements : la liste est
 * normalisée ensuite. */
void        hc_runs_clear(Object *field);
int         hc_run_add(Object *field, int start, int len, int style);
/* Variante complète. `font` NULL ou vide et `size` nul valent « comme le
 * champ » : c'est ce que la vue doit passer quand la plage ne se distingue
 * pas du champ sur cet attribut, pour ne pas figer une valeur héritée. */
int         hc_run_add_full(Object *field, int start, int len,
                            int style, int size, const char *font);

/* Plage surlignee par le dernier « find » dans ce champ. 1 si trouve. */
int         hc_found_range(Object *field, int *start, int *len);

/* Calque de peinture (bitmap base64) d'une carte ou d'un fond.
   Le noyau ne l'interprète pas : il le stocke et le restitue tel quel. */
const char *hc_paint_of(Object *o);
void        hc_set_paint(Object *o, const char *base64);

/* Libère les variables globales (à appeler avant de quitter). */
void        hc_shutdown(void);
void        hc_set_id(Object *o, int id);

#endif /* HC_CORE_H */
