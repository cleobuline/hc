/* hc_xcmd.h — écrire un XCMD ou un XFCN pour HC
 *
 * Sur le modèle de HyperXCmd.h, l'en-tête d'Apple pour les externes de
 * HyperCard : le même bloc de paramètres, les mêmes noms de rappels. Ce qui
 * change : le module est compilé en WebAssembly et tourne ENFERMÉ dans HC —
 * il ne touche que sa propre mémoire et les rappels ci-dessous ; les
 * paramètres sont des chaînes C, plus des Handle ni des chaînes Pascal ; un
 * rappel s'appelle directement, plus par un saut à entryPoint avec un
 * numéro de requête.
 *
 *     #include "hc_xcmd.h"
 *
 *     HC_XCMD(bonjour)                     bonjour "Patricia"
 *     {
 *         SendCardMessage("beep");
 *     }
 *
 *     HC_XFCN(double)                      put double(21)
 *     {
 *         paramPtr->returnValue = "42";
 *     }
 *
 * Le nom entre parenthèses est celui qu'appelle HyperTalk. Un même module
 * peut en porter plusieurs, commandes et fonctions mêlées. Voir LISEZMOI.md
 * pour compiler, et exemples/bonjour.c. */

#ifndef HC_XCMD_H
#define HC_XCMD_H

/* LE BLOC DE PARAMÈTRES. Ses champs sont ceux de HyperCard, dans le même
 * ordre, jusqu'à passFlag ; la suite du bloc d'Apple (entryPoint, request,
 * inArgs…) servait à appeler les rappels et n'a plus lieu d'être.
 *
 * HC le remplit avant l'appel, dans la mémoire du module. Les chaînes qu'il
 * y pose ne vivent que le temps de l'appel : les recopier pour les garder. */
typedef struct XCmdBlock {
    short          paramCount;   /* 0 à 16 */
    char          *params[16];   /* params[0] est le PREMIER paramètre */
    char          *returnValue;  /* XFCN : la valeur rendue ; NULL : vide */
    unsigned char  passFlag;     /* 1 : HC poursuit sa recherche, comme « pass » */
} XCmdBlock, *XCmdPtr;

/* HC lit ce bloc à des places fixes : elles ne doivent pas bouger. */
_Static_assert(sizeof(void *) == 4, "un module HC se compile pour wasm32");
_Static_assert(__builtin_offsetof(XCmdBlock, params) == 4, "params à +4");
_Static_assert(__builtin_offsetof(XCmdBlock, returnValue) == 68, "returnValue à +68");
_Static_assert(__builtin_offsetof(XCmdBlock, passFlag) == 72, "passFlag à +72");

/* LES RAPPELS — ce que HC tend au module, et RIEN d'autre.
 *
 * Le module les importe du module « hc » ; HC refuse de charger un module
 * qui en demande un qu'il ne connaît pas, plutôt que de le laisser tomber au
 * premier appel. Ils portent les noms de HyperXCmd.h.
 *
 * Ils jouent comme la boîte de messages, sur la carte courante, mais avec
 * les variables du gestionnaire qui a appelé le module.
 *
 * CE QU'ILS RENDENT. Chez HyperCard, un Handle que l'XCMD jette ensuite
 * (DisposHandle) ; ici, une chaîne allouée par malloc, à rendre par free.
 * NULL dit l'échec — le « xresFail » de HyperCard. */
#define HC_RAPPEL(nom) __attribute__((import_module("hc"), import_name(#nom)))

/* Une ligne de HyperTalk, jouée sur la carte courante comme si on la
 * tapait dans la boîte de messages : « put "x" », « go next », « beep ». */
HC_RAPPEL(SendCardMessage) void SendCardMessage(const char *msg);

/* La valeur d'une expression : EvalExpr("the date"), EvalExpr("x + 1").
 * NULL si elle ne se lit pas, ou si son calcul échoue. */
char *EvalExpr(const char *expr);

/* Une variable globale de HyperTalk. Inconnue, elle se lit vide ;
 * SetGlobal la crée au besoin. Un gestionnaire qui veut la lire la
 * déclare, comme d'habitude : « global pln0 ». */
char *GetGlobal(const char *nom);
void  SetGlobal(const char *nom, const char *valeur);

/* Le texte d'un champ de la carte courante (carte = 1) ou de son fond
 * (carte = 0) — le « cardFieldFlag » de HyperCard —, désigné par son nom,
 * son numéro parmi les champs de cette couche, ou son id. NULL s'il n'y en
 * a pas. Les Set rendent 1, ou 0 s'il n'y a pas de tel champ. */
char *GetFieldByName(int carte, const char *nom);
char *GetFieldByNum(int carte, int numero);
char *GetFieldByID(int carte, int id);
int   SetFieldByName(int carte, const char *nom, const char *valeur);
int   SetFieldByNum(int carte, int numero, const char *valeur);
int   SetFieldByID(int carte, int id, const char *valeur);

/* LES FENÊTRES — les « XWindows » de HyperCard 2.
 *
 *     Rect r = { 40, 20, 296, 276 };            haut, gauche, bas, droite
 *     WindowPtr w = NewXWindow(&r, "CAM", 1, 17, 1, 1);
 *
 * ouvre une fenêtre, que les scripts désignent par son titre : « send step
 * to window "CAM" », « set the scale of window "CAM" to 2 », « close window
 * "CAM" ». Chacun de ces gestes revient au module PAR L'EXTERNE QUI L'A
 * OUVERTE, appelé avec paramCount à -1 et, dans params[0], un XWEventInfo —
 * comme chez HyperCard :
 *
 *     HC_XFCN(maccam)
 *     {
 *         if (paramPtr->paramCount == -1) {
 *             XWEventInfoPtr e = (XWEventInfoPtr)paramPtr->params[0];
 *             switch (e->what) {
 *             case xSendEvt:    e->eventParams[0] est le message
 *             case xSetPropEvt: [0] la propriété, [1] la valeur
 *             case xGetPropEvt: [0] la propriété ; e->eventResult = la valeur
 *             case xCloseEvt:   la fenêtre se ferme : rendre ce qu'elle tenait
 *             }
 *             return;
 *         }
 *         …                                  l'appel ordinaire
 *     }
 *
 * Un événement que le module ne sert pas, il le PASSE (passFlag = 1) : HC
 * sert alors lui-même les propriétés de toute fenêtre — rect, loc, visible,
 * name, width, height. HC les sert aussi quand c'est le module lui-même
 * qui les pose, par SendCardMessage, pendant qu'il tourne : « set the width
 * of window "CAM" to 256 » ne lui revient pas.
 *
 * L'instance du module, et donc tout ce qu'il garde en mémoire, vit autant
 * que ses fenêtres. Si elle s'arrête sur une faute, ou si HC l'arrête, ses
 * fenêtres se ferment avec elle. */

/* Un NUMÉRO, et non plus un pointeur : le module ne touche pas à la
 * fenêtre, il la nomme. 0 n'en désigne aucune. */
typedef int WindowPtr;

/* L'ordre de QuickDraw : haut, gauche, bas, droite. Les scripts écrivent
 * les rectangles dans un autre ordre — « gauche,haut,droite,bas ». */
typedef struct Rect { short top, left, bottom, right; } Rect;

/* Les événements, sous leurs numéros de HyperCard — lus dans le code de
 * MACCAM, qui les servait, et confirmés, noms et numéros, par le
 * HyperXCmd.h du Mac (Universal Interfaces 3.4, lu, non commis). */
enum {
    xOpenEvt    = 1000,     /* la fenêtre vient de s'ouvrir */
    xCloseEvt   = 1001,     /* elle se ferme : case de fermeture, close window */
    xSendEvt    = 1200,     /* send <message> to window */
    xSetPropEvt = 1201,     /* set the <propriété> of window */
    xGetPropEvt = 1202      /* the <propriété> of window */
};

/* De l'EventRecord du Mac, seul « what » a cours ici. */
typedef struct XWEventInfo {
    short      what;
    WindowPtr  eventWindow;
    char      *eventParams[9];
    char      *eventResult;     /* xGetPropEvt : la valeur, que HC recopie */
} XWEventInfo, *XWEventInfoPtr;

_Static_assert(__builtin_offsetof(XWEventInfo, eventWindow) == 4, "eventWindow à +4");
_Static_assert(__builtin_offsetof(XWEventInfo, eventParams) == 8, "eventParams à +8");
_Static_assert(__builtin_offsetof(XWEventInfo, eventResult) == 44, "eventResult à +44");

/* Ouvrir une fenêtre : son rectangle de CONTENU en coordonnées de l'écran,
 * son titre, visible ou non, son style — le procID du Mac, que HC note sans
 * le dessiner —, en couleur (toujours, ici), flottante au-dessus de la
 * carte ou non. Rend 0 si elle n'a pas pu s'ouvrir. xOpenEvt arrive au
 * module dès que l'appel qui l'a ouverte est fini. */
WindowPtr NewXWindow(const Rect *boundsRect, const char *title, int visible,
                     int procID, int color, int floating);

/* La fermer, sans xCloseEvt : c'est le module qui le veut. */
void CloseXWindow(WindowPtr window);

/* L'IMAGE DE LA FENÊTRE : `largeur` × `hauteur` points d'un octet, une ligne
 * après l'autre, chacun l'indice d'une couleur de `palette` — 256 couleurs
 * de trois octets, rouge, vert, bleu —, chaque point agrandi en un carré de
 * `echelle` de côté (de 1 à 64 : HC ramène l'échelle dans ces bornes),
 * depuis le coin haut-gauche. HC la garde et la redessine
 * quand il le faut : pas d'événement « update » à servir. Le CopyBits de
 * HyperCard, réduit à une image indexée. */
void DrawXWindow(WindowPtr window, const unsigned char *pixels,
                 int largeur, int hauteur, const unsigned char *palette,
                 int echelle);

/* L'image de la fenêtre, telle qu'on la voit, au presse-papiers. Rend 1, ou
 * 0 si la fenêtre n'est pas à ce module, ou si rien n'a pu s'y copier. */
int CopyXWindow(WindowPtr window);

/* LE BOUTON DE LA SOURIS est-il enfoncé ? Le piège Button du Mac, que les
 * boucles de MAC•CAM guettaient pour s'arrêter.
 *
 * Il répond oui AUSSI quand Cmd-. a été tapé : un module qui le guette
 * s'arrête de lui-même, et garde ses fenêtres. Un module qui ne le guette
 * pas, HC l'arrête, et il les perd. */
int Button(void);

/* LA PEINTURE DE LA CARTE COURANTE — celle de son fond quand on édite le
 * fond — dans le rectangle `r`, en coordonnées carte : un octet par point,
 * une ligne après l'autre, 1 où l'encre est sombre, 0 ailleurs. `noir` doit
 * tenir (right - left) × (bottom - top) octets. Rend 1, ou 0 s'il n'y a pas
 * de carte, ou pas d'hôte pour lire sa peinture. Le GetMaskAndData de
 * HyperCard, réduit à ce que MACCAM en lisait. */
int GetCardPicture(const Rect *r, unsigned char *noir);

/* « gauche,haut,droite,bas », comme l'écrivent les scripts, en Rect. Rend 1,
 * ou 0 si la chaîne n'en est pas un — *r reste alors tel quel. Le StrToRect
 * de HyperCard. */
int StrToRect(const char *s, Rect *r);

/* LES BULLES D'AIDE de System 7 — le Help Manager, réduit à ce que l'XCMD
 * Ballons de MAC•CAM en employait.
 *
 * HMGetBalloons : les bulles sont-elles allumées ? C'est l'article « Show
 * Balloons » du menu Help de HC, comme celui du menu des bulles de System 7.
 *
 * HMShowBalloon : `texte` dans une bulle pointée sur la souris, qui reste
 * tant que la souris est dans `zone` — en coordonnées carte, comme « the
 * rect of me ». Rend 1, ou 0 si les bulles sont éteintes : rien ne
 * s'affiche alors. Le message d'Apple (HMMessageRecord), la pointe et la
 * méthode se réduisent ici au texte et à la zone. */
int HMGetBalloons(void);
int HMShowBalloon(const char *texte, const Rect *zone);

/* L'ordre de QuickDraw : vertical, horizontal. */
typedef struct Point { short v, h; } Point;

/* LE GLISSER D'UN CONTOUR GRIS — le DragGrayRgn du Toolbox, qu'employait
 * l'XFCN FullDrag. Tant que le bouton de la souris est enfoncé, le contour
 * des `n` rectangles suit la souris, depuis le point `depart`, sans que la
 * souris sorte de `limite` ; hors de `marge`, il disparaît. `axe` : 0 libre,
 * 1 seulement à l'horizontale, 2 seulement à la verticale. Tout en
 * coordonnées carte.
 *
 * Rend le déplacement comme le Toolbox : le vertical dans le mot haut,
 * l'horizontal dans le mot bas (HiWord, LoWord) ; 0x80008000 si l'on a
 * lâché hors de `marge` ; 0 si le bouton était déjà relâché. La région du
 * Toolbox se réduit ici à des rectangles. */
long DragGrayRgn(const Rect *rects, int n, Point depart, const Rect *limite,
                 const Rect *marge, int axe);

#define HiWord(x) ((short)((unsigned long)(x) >> 16))
#define LoWord(x) ((short)((unsigned long)(x) & 0xffff))

/* DÉCLARER UN XCMD OU UN XFCN.
 *
 * Chacun devient une fonction exportée sous le nom « XCMD bonjour » ou
 * « XFCN bonjour » : c'est par ce nom que HC sait ce qu'apporte un module. */
#define HC_XCMD(nom) HC_ENTREE_(XCMD, nom)
#define HC_XFCN(nom) HC_ENTREE_(XFCN, nom)

/* paramPtr est marqué « peut ne pas servir » : un XCMD qui n'en lit rien —
 * le premier « hello world » — compile sans un avertissement. */
#define HC_ENTREE_(genre, nom)                                           \
    static void genre##_##nom(XCmdPtr paramPtr);                         \
    __attribute__((export_name(#genre " " #nom)))                        \
    void hc_entree_##genre##_##nom(XCmdPtr p);                           \
    void hc_entree_##genre##_##nom(XCmdPtr p) { genre##_##nom(p); }      \
    static void genre##_##nom(XCmdPtr paramPtr __attribute__((unused)))

#endif
