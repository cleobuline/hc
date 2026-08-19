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
 *     color == HC_COLOR_INHERIT  -> noir, ou la couleur du champ
 *
 * La couleur est un ajout : HyperCard était en noir et blanc, et n'avait pas
 * de « textColor ». Elle suit néanmoins la même mécanique que les trois
 * autres, ce qui évite d'inventer un second système de plages. Encodée en
 * 0xRRGGBB, avec une sentinelle À PART de zéro — qui est le noir, une couleur
 * légitime qu'on doit pouvoir poser explicitement.
 * Le style a besoin d'une sentinelle À PART de zéro : zéro veut dire « plain,
 * explicitement », ce qui doit effacer le gras du champ sur cette plage. Les
 * confondre rendait « set the textStyle of word 3 to plain » sans effet dans
 * un champ gras — la plage était jetée, et le mot reprenait le gras du champ.
 * Une plage dont les trois champs valent leur sentinelle ne dit plus rien et
 * se fait jeter par runs_tidy.
 *
 * Ces sentinelles sont INTERNES : hc_run_attrs les résout avant de rendre la
 * main à l'hôte, qui ne voit que des valeurs effectives. */
struct TextRun { int start, len; int style; int size; char *font; int color; };
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
#define HC_COLOR_INHERIT (-1) /* plage muette sur la couleur : voir plus haut */

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
    int marked;        /* carte marquee : « mark this card ». Toujours 0 ailleurs. */
    int      autohilite;/* le bouton s'allume automatiquement pendant le clic */
    int      textsize;  /* taille de police du nom (0 = défaut) */
    /* Interligne d'un champ, en pixels. 0 = déduit du corps, à la manière
     * d'HyperCard : quatre tiers de la taille de police.
     *
     * C'est une propriété du CHAMP ENTIER, et non des plages : HyperCard
     * espaçait toutes les lignes d'un champ également, quelles que soient les
     * tailles employées ligne à ligne. Les scripts d'époque s'appuient
     * dessus — « (mouseV - top of the target) div the textHeight » est la
     * façon canonique de trouver la ligne cliquée. Confondre textHeight et
     * textSize, comme on le faisait, rendait cette formule fausse d'un tiers
     * de ligne à chaque ligne. */
    int      textheight;
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

    /* Carte marquée. « mark cards where <condition> » les désigne, « print
     * marked cards » ou « go next marked card » les parcourt : c'est ainsi
     * qu'on travaille sur un sous-ensemble d'une pile sans la modifier. */


    /* Alignement du texte : 0 gauche, 1 centré, 2 droite. HyperCard n'en
     * connaît pas d'autre — pas de justifié. */
    int      text_align;

    /* autoSelect : cliquer dans le champ sélectionne la LIGNE entière au lieu
     * de poser un curseur. C'est ainsi que se font les sommaires et les listes
     * de choix, sans une ligne de script — le sommaire de MacCam fait à la
     * main ce que cette propriété donne pour rien.
     *
     * Elle suppose le champ verrouillé : un champ où l'on peut taper n'a pas
     * de raison de sélectionner des lignes entières. HyperCard imposait la
     * même condition. */
    int      auto_select;
    int      multiple_lines;  /* autoSelect : plusieurs lignes à la fois */

    /* dontWrap : les lignes trop longues sont coupées au bord au lieu de
     * passer à la ligne. Utile pour les données en colonnes, où un retour
     * automatique décalerait tout. */
    int      dont_wrap;

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

/* Supprime une carte et la libère. Renvoie 0 sans rien faire si c'est la
 * DERNIÈRE carte de la pile : HyperCard refusait aussi, et une pile sans carte
 * n'a pas de sens.
 *
 * Si la carte supprimée était la carte courante, celle-ci passe à la suivante
 * — ou à la précédente s'il n'y en a pas. L'hôte doit donc relire
 * hc_current_card après l'appel, et lâcher AVANT tout ce qu'il retient de la
 * carte : objet sélectionné, champ en édition, sélection de texte. */
int     hc_delete_card(Object *card);

/* Nombre de cartes d'une pile. */
int     hc_card_count(Object *stack);

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

    /* ---- panneaux de fichier ----
     * Rendent le chemin choisi, ou NULL si l'utilisateur annule.
     *
     * Distincts d'ask et answer parce qu'ils ne demandent pas du texte mais un
     * emplacement. Ils servent aussi de recours à « open file » quand le nom
     * seul ne mène à rien — c'est ce que faisait HyperCard, dont les scripts
     * écrivent « open file "notes" » sans chemin.
     *
     * Sous le bac à sable de macOS, c'est la seule façon d'atteindre un
     * fichier hors des dossiers autorisés : le désigner vaut autorisation. */
    const char *(*answer_file)(const char *prompt);
    const char *(*ask_file)(const char *prompt, const char *deflt);

    /* Enregistre une COPIE de la pile sous un autre nom.
     *
     * C'est ce que veut dire « save stack "X" as "Y" » : dupliquer, et non
     * enregistrer les modifications en cours — HyperCard écrivait en continu,
     * et n'avait pas de commande d'enregistrement.
     *
     * Le noyau délègue parce qu'il ne connaît pas le format de fichier :
     * celui-ci vit dans hc_file.c, que le noyau n'inclut pas. Renvoyer 0
     * signale l'échec, que la commande traduit dans « the result ». */
    int (*save_stack)(Object *stack, const char *path);

    /* ---- plusieurs piles ouvertes ----
     * open_stack   « go to stack "X" » sur une pile qui n'est pas ouverte :
     *              l'hôte la trouve, la charge, l'enregistre par
     *              hc_register_stack et la rend. NULL s'il ne la trouve pas.
     *              Seul l'hôte sait où chercher un fichier et comment lui
     *              donner une fenêtre.
     * stack_changed  la pile courante vient de changer : à l'hôte d'amener sa
     *              fenêtre au premier plan et de redessiner. */
    Object *(*open_stack)(const char *nom);

    /* Charge une pile SANS lui donner de fenêtre.
     *
     * C'est ce que demande « start using stack "X" » : la pile entre dans la
     * chaîne de messages, ses gestionnaires deviennent appelables, mais elle
     * reste invisible. Une bibliothèque n'a pas à s'afficher — HyperCard ne la
     * montrait pas, et ouvrir une fenêtre pour chaque pile d'outils déclarée
     * encombrerait l'écran sans rien apporter.
     *
     * L'hôte doit l'enregistrer par hc_register_stack et la retenir pour
     * pouvoir la libérer plus tard : le noyau ne possède aucune pile. */
    Object *(*load_stack)(const char *nom);

    /* Imprime des cartes.
     *
     * `cartes` est un tableau de `n` cartes, dans l'ordre où elles doivent
     * sortir. Le noyau se contente de le composer — il ne sait rien du papier,
     * des marges ni du pilote d'imprimante.
     *
     * Une carte par page : HyperCard en disposait plusieurs sur une feuille,
     * mais ses piles faisaient toutes 512×342. Avec des tailles libres, une
     * grille demanderait de décider quoi faire d'une carte plus large que la
     * page — pour un gain qui ne vaut pas cette complication. */
    void (*print_cards)(Object **cartes, int n);
    void    (*stack_changed)(Object *stack);

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

    /* ---- outils et gestes simulés ----
     * Le noyau ne connaît ni la palette ni la souris : il analyse la commande
     * et délègue. `mods` est une liste éventuelle de touches séparées par des
     * virgules — « shiftKey », « optionKey », « commandKey » — ou NULL.
     *
     * choose_tool  « choose line tool »  : nom d'outil en minuscules, tel
     *              qu'HyperCard l'écrit (browse, brush, line, spray…).
     * drag         « drag from 10,10 to 90,90 » : trace avec l'outil courant.
     * click_at     « click at 100,120 » : un clic complet, appui et relâchement.
     * type_text    « type "bonjour" » : frappe dans ce qui a le focus.
     *
     * Ensemble, choose et drag permettent à un script de DESSINER, ce qui
     * était une signature d'HyperCard : les piles s'y fabriquaient des
     * graphiques à la volée. */
    void (*choose_tool)(const char *name);
    void (*drag)(int x1, int y1, int x2, int y2, const char *mods);
    void (*click_at)(int x, int y, const char *mods);
    void (*type_text)(const char *text, const char *mods);

    /* ---- effet de transition ----
     * Appelé juste AVANT un changement de carte, quand un « visual » a été
     * armé. L'hôte photographie l'écran de départ, laisse le noyau changer de
     * carte, puis anime le passage à l'arrivée.
     *
     * `effect` porte le nom d'HyperCard, éventuellement en plusieurs mots
     * (« barn door open », « iris close »). `speed` vaut « fast », « slow »,
     * « very fast », « very slow » ou la chaîne vide. `image` vaut « black »,
     * « white », « gray », « inverse », « card », ou la chaîne vide — c'est
     * VERS quoi on fond, un fondu au noir se faisant en deux temps :
     * « visual dissolve to black » puis « visual dissolve to card ». */
    void (*visual_effect)(const char *effect, const char *speed, const char *image);

    /* Sélection de texte posée par « select … of field X ». L'hôte met la
     * surbrillance dans son éditeur de champ : sans lui, la sélection serait
     * vraie pour les scripts et invisible à l'écran. `field` NULL veut dire
     * « plus rien de sélectionné ». Les bornes sont en caractères, dans le
     * texte rendu par hc_field_text ; une longueur nulle est un point
     * d'insertion. */
    void (*selection_changed)(Object *field, int start, int len);

    /* Appelé à chaque tour de boucle « repeat ». C'est là qu'une interface
     * graphique redessine et rend la main : sans lui, une boucle d'animation
     * monopolise le fil principal et rien ne s'affiche avant la fin. */
    void (*idle)(void);
} HcHost;

/* Installe l'hôte. Passer NULL rétablit l'hôte console par défaut. */
void        hc_set_host(const HcHost *h);

Object *hc_current_card(void);

/* 1 si un gestionnaire est en cours d'exécution. L'hôte s'en sert pour ne pas
 * envoyer « idle » au milieu d'un script. */
int     hc_is_running(void);

/* ---- Registre des piles ouvertes ----
 * Le noyau ne possède aucune pile : hc_load en rend une, hc_free la libère.
 * Mais pour que « stack "X" » et « go to stack "X" » désignent autre chose
 * que la pile courante, il doit savoir lesquelles sont ouvertes.
 *
 * L'hôte enregistre une pile après l'avoir chargée, et la RETIRE avant de la
 * libérer — sans quoi le registre garderait un pointeur mort. */
void    hc_register_stack(Object *stack);
void    hc_unregister_stack(Object *stack);
int     hc_stack_count(void);
Object *hc_stack_at(int i);

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
/* Comme hc_run_attrs, plus la couleur. HC_COLOR_INHERIT y signifie « pas de
 * couleur propre » : l'hôte emploie alors la sienne, noire d'ordinaire. */
int         hc_run_attrs_color(Object *field, int i, int *start, int *len,
                               int *style, int *size, const char **font,
                               int *color);

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
/* Variante avec la couleur, en 0xRRGGBB. HC_COLOR_INHERIT laisse la plage
 * muette sur cet attribut, comme font NULL et 0 pour la police et le corps. */
int         hc_run_add_color(Object *field, int start, int len,
                             int style, int size, const char *font, int color);

/* Interligne effectif d'un champ, en pixels : textheight s'il est posé, sinon
 * quatre tiers du corps. Utilisé par le noyau pour « the textHeight » et par
 * l'hôte pour dessiner — une seule définition, sans quoi les scripts
 * calculeraient sur un interligne différent de celui qu'ils voient. */
int         hc_text_height(Object *o);

/* Plage surlignee par le dernier « find » dans ce champ. 1 si trouve. */
int         hc_found_range(Object *field, int *start, int *len);

/* ---- Sélection de texte ----
 * Posée par le verbe « select » et lue par « the selection ». L'hôte l'appelle
 * aussi quand l'utilisateur sélectionne à la souris, pour que les scripts
 * voient la même chose que l'écran.
 *
 * Bornes en caractères dans le texte rendu par hc_field_text, demi-ouvertes :
 * [start, start+len). Une longueur nulle est un point d'insertion.
 * Passer NULL comme champ efface la sélection.
 *
 * hc_set_selection prévient l'hôte par selection_changed ; l'hôte doit donc se
 * garder de la rappeler en réaction, sous peine de boucle. */
void        hc_set_selection(Object *field, int start, int len);
int         hc_get_selection(Object **field, int *start, int *len);

/* Calque de peinture (bitmap base64) d'une carte ou d'un fond.
   Le noyau ne l'interprète pas : il le stocke et le restitue tel quel. */
const char *hc_paint_of(Object *o);
void        hc_set_paint(Object *o, const char *base64);

/* Libère les variables globales (à appeler avant de quitter). */
void        hc_shutdown(void);
void        hc_set_id(Object *o, int id);

/* ---- Numérotation d'une part ----
 * HyperCard distingue deux compteurs, et les confondre fait écrire des
 * scripts qui ne désignent pas l'objet qu'on croit :
 *
 *   hc_object_number  rang parmi les objets DE MÊME TYPE chez le propriétaire.
 *                     C'est le N de « field N » et « button N ».
 *   hc_part_number    rang parmi TOUTES les parts, boutons et champs mêlés.
 *                     C'est le N de « part N », et l'ordre de tabulation.
 *
 * Un champ posé après cinq boutons a donc le numéro de champ 1 et le numéro
 * de part 6. Les deux renvoient 0 si l'objet n'est pas une part.
 *
 * Attention : la numérotation est PROPRE AU PROPRIÉTAIRE. Un champ de fond
 * numéro 1 se désigne « bg field 1 », et « card field 1 » est un autre objet.
 * hc_owner_is_bg dit de quel côté on se trouve. */
int         hc_object_number(Object *o);
int         hc_part_number(Object *o);
int         hc_owner_is_bg(Object *o);
/* Nombre de parts d'un type donné chez un propriétaire (carte OU fond, sans
 * addition des deux : c'est ce que la numérotation par rang suppose). */
int         hc_part_count(Object *owner, ObjType type);

/* ---- Presse-papiers d'objets (copier/couper/coller boutons et champs) ----
 * HyperCard copie un OBJET, pas des pixels : le clone emporte nom, script,
 * géométrie, style, police, icône, et pour un champ ses plages et son texte.
 *
 * Le presse-papiers garde un objet DÉTACHÉ, sans propriétaire. Il survit donc
 * à la suppression de sa carte d'origine et au chargement d'une autre pile —
 * c'est ce qui permet de coller d'une pile vers une autre. hc_shutdown le
 * libère.
 *
 * À la pose, hc_paste_part attribue un identifiant NEUF (deux objets de même
 * id rendraient « field id 42 » ambigu), rattache l'objet au propriétaire
 * demandé, et le décale de quelques pixels si la place est déjà occupée.
 * C'est le propriétaire qui décide de la nature : coller sur une carte un
 * bouton pris sur un fond en fait un bouton de carte.
 *
 * L'hôte doit envoyer « newButton » ou « newField » à l'objet rendu : certains
 * scripts (le calendrier d'origine, par exemple) s'initialisent là. */
int      hc_copy_part(Object *o);            /* 1 si copié */
int      hc_cut_part(Object *o);             /* copie puis supprime */
Object  *hc_paste_part(Object *owner);       /* NULL si rien à coller */
Object  *hc_clipboard_part(void);            /* NULL si vide ; ne pas libérer */
void     hc_clipboard_clear(void);

#endif /* HC_CORE_H */
