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

/* L'allumage d'un bouton de fond NON PARTAGÉ, pour une carte donnée.
 *
 * Pendant exact de BgText : quand sharedHilite est faux, l'allumage cesse
 * d'être une propriété du bouton pour devenir une propriété de la carte —
 * chaque carte du fond garde le sien. Les cases à cocher d'un questionnaire
 * en dépendent, elles n'auraient aucun sens partagées.
 *
 * Une carte ne retient que les boutons qu'elle a effectivement allumés :
 * l'absence d'entrée vaut éteint. */
struct BgHilite { int button_id; int hilite; };

/* ---- Icônes de pile ----
 * HyperCard rangeait les icônes de boutons en ressources ICON dans le fichier
 * de pile lui-même : une pile emportait ses icônes, et un bouton n'en retenait
 * que le numéro. On fait pareil, la table appartient à l'objet PILE.
 *
 * 32×32 en 1 bit, soit 128 octets : 32 lignes de 4 octets, bit de poids fort à
 * gauche, bit à 1 = encre. C'est la disposition d'une ressource ICON, et celle
 * du catalogue d'origine compilé dans l'application — les deux tables se
 * lisent donc avec le même code.
 *
 * Le noyau ne sait pas dessiner : ces octets lui sont opaques, comme l'est
 * déjà le base64 de `paint`. */
#define HC_ICON_BYTES 128

struct StackIcon {
    int           id;
    char         *name;                    /* possédé : libéré par hc_free */
    unsigned char bits[HC_ICON_BYTES];
};

struct Object {
    ObjType  type;
    int      id;
    char    *name;      /* nom de l'objet (peut être NULL) */
    char    *script;    /* script HyperTalk brut */

    /* Arbre du script, analysé une seule fois et gardé.
     *
     * L'ancien interpréteur relit et redécoupe le texte à chaque instruction :
     * sur une boucle de cent mille tours, les deux tiers du temps y passent.
     * L'exécuteur v3 travaille sur un arbre, qu'il serait absurde de
     * reconstruire à chaque message.
     *
     * Opaques ici : hc_core.h est du C pur et ne connaît pas les types de la
     * bibliothèque. Voir script_arbre() dans hc_core.c.
     *
     * INVALIDATION : toute écriture dans `script` doit passer par
     * hc_set_script, qui jette l'arbre. Modifier le champ en direct laisserait
     * un arbre décrivant l'ancien texte, et le gestionnaire exécuté ne serait
     * plus celui qu'on lit. */
    void    *arbre;     /* HctNoeud *   — racine, ou NULL                  */
    void    *reserve;   /* HctReserve * — mémoire de l'arbre               */
    void    *lot;       /* HctLot *     — jetons, pointant dans `script`   */
    int      arbre_sain;/* 1 si l'analyse n'a rien signalé                 */
    int      arbre_signale; /* le refus a déjà été tracé une fois           */
    int      arbre_faute_ligne; /* ligne de la première faute, 0 si aucune  */

    /* Un script peut se réécrire LUI-MÊME pendant qu'il s'exécute : le
     * calendrier d'Apple range ses données dans son propre script. Libérer
     * alors l'arbre et son texte tirerait le sol sous les pieds de
     * l'exécuteur — les nœuds ET les jetons y pointent.
     *
     * On compte donc les exécutions en cours, et l'on diffère. */
    int      arbre_usage;   /* profondeur d'exécution de cet arbre          */
    int      arbre_perime;  /* à jeter dès que l'usage retombe à zéro       */
    char   **textes_gardes; /* anciens scripts, encore lus par des jetons   */
    int      ntextes_gardes;

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
    /* Bouton actif ? (1 = oui par défaut)
     *
     * Case « Enabled » de l'Info bouton de HyperCard 2. Un bouton désactivé ne
     * reçoit plus les messages de souris et son nom se dessine en gris : il
     * reste visible et à sa place, simplement inerte et visiblement inactif.
     * C'est le grisage des contrôles du Toolbox, que HyperCard a repris. */
    int      enabled;
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
    /* Allumage partagé entre cartes du même fond ? (1 = oui par défaut)
     *
     * Case « Shared Hilite » de l'Info bouton. À faux, l'allumage réel est
     * stocké dans chaque carte (voir bghilites) et non dans ce champ : ne
     * jamais lire ni écrire o->hilite directement pour un bouton de fond,
     * passer par hc_hilite_of et hc_set_hilite. */
    int      shared_hilite;

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

    /* pour une CARTE : l'allumage des boutons de fond non partagés. */
    struct BgHilite *bghilites;
    int      nbghilites, capbghilites;
    char    *contents;  /* contenu textuel (champs) ; pour un champ de fond
                         * non partagé, c'est la valeur par défaut : le texte
                         * réel est stocké dans chaque carte (voir bgtexts) */

    /* géométrie : rectangle en coordonnées carte (pixels).
       rect = (left, top, right, bottom) = (x, y, x+w, y+h). */
    int      x, y, w, h;
    char    *style;     /* boutons : rectangle, roundRect, checkBox… ; NULL = défaut */
    char    *paint;     /* cartes/fonds : bitmap peinture encodé base64 (opaque pour le noyau) */

    /* pour une PILE : ses icônes. Nul partout ailleurs. */
    struct StackIcon *icons;
    int      nicons, capicons;
};

/* ---- Construction ---- */
/* Relevé des retours de la v3 vers l'ancien interpréteur : ce que l'exécuteur
 * v3 ne sait pas faire lui-même et rend à exec_stmt / term_value.
 *
 * Sert à décider ce qu'on peut retirer de hc_core.c : promener toutes les
 * piles, puis appeler hc_v3_bilan(). Ce qui n'y figure jamais est un candidat
 * à la coupe ; le reste porte encore. */
void hc_v3_bilan(void);
void hc_v3_bilan_remise_a_zero(void);

Object *hc_new_stack(const char *name);
Object *hc_new_background(Object *stack, const char *name);
Object *hc_new_card(Object *stack, Object *bg, const char *name);
Object *hc_new_button(Object *owner, const char *name);
Object *hc_new_field(Object *owner, const char *name);
void    hc_set_script(Object *o, const char *script);

/* Jette l'arbre analysé d'un objet. hc_set_script et hc_free l'appellent déjà ;
 * à appeler soi-même si l'on modifie `script` par un autre chemin — l'arbre
 * décrirait sinon un texte qui n'existe plus. */
void    hc_arbre_oublie(Object *o);
void    hc_free(Object *stack);
int     hc_delete_part(Object *o);
/* Rend 1 si le noyau a modifié quelque chose de visible depuis le dernier
   appel, et remet le drapeau à zéro. */
int hc_take_visual_dirty(void);
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

/* ---- allumage d'un bouton ----
 * LE seul chemin d'accès. Pour un bouton de fond dont sharedHilite est faux,
 * l'allumage vit dans la carte et non dans le bouton : lire ou écrire
 * o->hilite en direct donnerait la même valeur sur toutes les cartes du fond,
 * ce qui est précisément ce que « non partagé » exclut.
 *
 * `card` est la carte où l'on regarde ; NULL prend la carte courante. */
/* Bascule « Shared Text » d'un champ de fond, en déménageant texte et plages
 * de style entre la carte et l'objet. Ne jamais écrire field->shared_text en
 * direct : le contenu deviendrait inaccessible. */
void    hc_set_shared_text(Object *field, int shared);

int     hc_hilite_of(Object *btn, Object *card);
void    hc_set_hilite(Object *btn, Object *card, int on);
/* Pose l'entrée par identifiant, pour le chargement, qui n'a pas l'objet. */
void    hc_set_hilite_raw(Object *card, int button_id, int on);

/* ---- Icônes de pile ----
 * `stack` doit être une pile ; tout rend NULL ou 0 sinon.
 *
 * hc_icon_add REMPLACE l'entrée si le numéro existe déjà, au lieu d'en ajouter
 * une seconde. Sans quoi un fichier contenant deux fois le même numéro ferait
 * grossir la table à chaque relecture, et hcicon_find n'aurait aucun moyen de
 * savoir laquelle des deux compte. Les octets d'une entrée neuve valent zéro :
 * c'est au dessinateur, pas au noyau, de la remplir.
 *
 * Les pointeurs rendus visent l'intérieur de la table et ne survivent donc pas
 * à un hc_icon_add ni à un hc_icon_remove — relire par numéro après coup. */
struct StackIcon *hc_icon_add(Object *stack, int id, const char *name);
struct StackIcon *hc_icon_get(Object *stack, int id);
struct StackIcon *hc_icon_at(Object *stack, int i);
int               hc_icon_count(Object *stack);
void              hc_icon_remove(Object *stack, int id);
/* ---- résolution d'un nom d'icône ----
 * « set the icon of me to "Close Box" » doit marcher aussi bien que par numéro.
 * Mais chercher un nom suppose de connaître les DEUX catalogues, celui de la
 * pile et celui compilé dans l'application — et ce dernier vit dans un fichier
 * Objective-C, que ce noyau en C99 ne peut pas inclure.
 *
 * D'où ce crochet : la couche Cocoa dépose sa fonction au démarrage, le noyau
 * l'appelle sans savoir ce qu'il y a derrière. Tant qu'elle n'est pas posée,
 * on se rabat sur atoi — les numéros marchent, les noms non. */
typedef int (*HCIconResolver)(const char *text);
void hc_set_icon_resolver(HCIconResolver fn);
int  hc_resolve_icon(const char *text);

/* Même mécanisme, pour savoir si un numéro est pris dans le catalogue
 * d'origine : transplanter les icônes d'une carte collée demande un numéro
 * libre, et « libre » doit l'être des deux côtés. Sans ce crochet, on ne
 * vérifie que la pile — au pire on masque une icône d'origine. */
typedef int (*HCIconTakenFn)(int id);
void hc_set_icon_builtin_check(HCIconTakenFn fn);

/* Il n'y a volontairement PAS de « hc_icon_next_id » ici. Choisir un numéro
 * libre suppose de connaître aussi le catalogue d'icônes compilé dans
 * l'application, dont les identifiants s'étalent de 128 à plus de 32000 ; le
 * noyau ne voit que la pile et se tromperait. Ce choix appartient à la couche
 * qui voit les deux tables — cf. hcicon_edit_free_id. */
/* Libère la table. À appeler depuis hc_free, sur la branche OBJ_STACK. */
void              hc_icons_free(Object *stack);

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
    void (*do_menu)(const char *item);
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

/* Ce calque — carte ou fond — appartient-il encore à une pile ouverte ?
 *
 * À interroger avant de se servir d'un pointeur conservé hors du noyau : un
 * cache indexé par adresse ne sait pas que son objet est mort. Ne déréférence
 * jamais l'argument, il ne fait que le comparer aux objets vivants. */
int     hc_layer_is_live(Object *layer);

/* ---- Coeur : envoi d'un message ---- */
/* Renvoie 1 si un gestionnaire a traité le message, 0 sinon. */
int     hc_send(Object *target, const char *message);

/* Choisir un article de menu. Envoie « doMenu <article> » à la carte courante
 * et n'exécute l'article que si personne ne l'intercepte — voir hc_core.c.
 * C'est par ici que doit passer TOUT déclenchement de menu, script ou clic. */
void    hc_do_menu(const char *item);

/* La moitié « message » de hc_do_menu : propose l'article à la pile et rend 1
 * si un gestionnaire l'a pris. Pour l'interface, dont les actions natives
 * savent déjà agir et n'ont qu'à demander d'abord. */
int     hc_menu_trappe(const char *item);

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

/* ---- copier-coller de CARTES ----
 * Même presse-papiers que les parts : copier une carte écrase un bouton copié,
 * comme dans HyperCard.
 *
 * Une carte emporte ses boutons et champs de carte, son calque de peinture, sa
 * marque, et ses bgtexts — le texte non partagé des champs de fond, qui
 * appartient à la carte. Le FOND n'est pas copié : la carte s'appuie dessus,
 * elle ne le possède pas. Le presse-papiers en retient l'identifiant et le
 * retrouve au collage.
 *
 * Le FOND part aussi, en copie : coller dans une AUTRE pile l'y recrée. Coller
 * là où l'on a copié réutilise le fond existant — reconnu par identité, non par
 * identifiant, deux piles chargées de fichiers différents pouvant porter le
 * même numéro sans rien avoir de commun.
 *
 * Recréer un fond lui donne des identifiants neufs, y compris à ses champs. Les
 * bgtexts de la carte, qui les désignent, sont réétiquetés par position — sans
 * quoi le texte non partagé de la carte disparaîtrait à l'écran.
 *
 * Les ICÔNES de pile qu'utilisent les boutons partent également : un bouton ne
 * retient qu'un numéro, qui désigne le catalogue de sa pile d'origine. À
 * l'arrivée, un numéro déjà pris par une icône identique est réutilisé ; pris
 * par une autre, on en prend un libre et l'on réétiquette les boutons. Les
 * icônes d'origine ne voyagent pas : elles existent partout.
 *
 * La carte collée se pose juste après la carte courante, comme dans HyperCard,
 * et reçoit des identifiants neufs — elle et ses parts. */
int      hc_copy_card(Object *card);         /* 1 si copiée */
int      hc_cut_card(Object *card);          /* copie puis supprime */
Object  *hc_paste_card(Object *stack);       /* NULL si rien à coller */
Object  *hc_duplicate_card(Object *card);    /* sans toucher au presse-papiers */

/* À appeler quand une pile se ferme, AVANT hc_free : le presse-papiers y
 * emprunte un pointeur de fond, qui ne doit pas lui survivre. */
void     hc_clipboard_stack_closing(Object *stack);
Object  *hc_clipboard_part(void);            /* NULL si vide ; ne pas libérer */
int      hc_clipboard_has_card(void);        /* le presse-papiers porte une carte */
int      hc_clipboard_has_part(void);        /* ... un bouton ou un champ */
void     hc_clipboard_clear(void);

#endif /* HC_CORE_H */
