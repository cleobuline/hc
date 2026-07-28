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
    char    *contents;  /* contenu textuel (champs) */

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

/* Calque de peinture (bitmap base64) d'une carte ou d'un fond.
   Le noyau ne l'interprète pas : il le stocke et le restitue tel quel. */
const char *hc_paint_of(Object *o);
void        hc_set_paint(Object *o, const char *base64);

/* Libère les variables globales (à appeler avant de quitter). */
void        hc_shutdown(void);

#endif /* HC_CORE_H */
