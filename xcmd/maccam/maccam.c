/* maccam.c — l'XFCN MACCAM de MAC•CAM Workshop, en module de HC : la
 * fenêtre « CAM », où la machine applique les tables du FORTH.
 *
 *     get maccam(the rect of bg btn "startup", "CAM", "True", 17, "True", "True")
 *     set the pln7 of window "CAM" to fld "pln0"
 *     set the image7 of window "CAM" to the rect of bg btn "pln7"
 *     send step to window "CAM"
 *
 * Ce qu'elle fait est ce que faisait l'original, lu dans son code 68000
 * (docs/mesures/xcmd.txt) : ses six paramètres et leurs défauts, ses trente
 * propriétés, ses dix-sept messages. La machine elle-même est
 * maccam_coeur.c ; ce fichier n'est que son passage dans HC.
 *
 * CE QUI CHANGE, décidé par l'utilisatrice : les palettes clut de la pile
 * ne sont pas reprises — la fenêtre a celle du moteur MacCam moderne, et
 * « palette » s'accepte sans rien changer ; « PictResource », qui écrivait
 * une ressource PICT dans la pile, photographie la fenêtre au
 * presse-papiers, comme « toscrap ».
 *
 * CE QUI S'AJOUTE, proposé par l'utilisatrice le 18 octobre : « planes »,
 * pour masquer des plans au dessin — ce que les clut faisaient en 1990, et
 * que le moteur MacCam moderne fait déjà. Et, le 19 octobre, « colors » :
 * la couleur de chaque combinaison de plans, qui remplace les clut pour
 * de bon. Voir pose(), plus bas. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hc_xcmd.h"
#include "maccam_coeur.h"

/* L'original gardait l'état d'une fenêtre dans son refCon : il pouvait en
 * ouvrir plusieurs. On en tient quelques-unes. */
typedef struct {
    WindowPtr w;
    Cam       cam;
    int       ouverte;          /* xOpenEvt a fait la grille */
    int       largeur, hauteur; /* le contenu demandé à NewXWindow */
    int       echelle;
    int       hilite;
    int       masque;           /* « planes » : 0x80 le plan 0 … 0x10 le plan 3 */
    unsigned char couleurs[16][3];  /* « colors » : par combinaison de plans */
    char      nom[256];         /* pour se poser ses propriétés de fenêtre */
} Fenetre;

enum { FENETRES = 8 };
static Fenetre g_f[FENETRES];

static Fenetre *de(WindowPtr w)
{
    for (int i = 0; i < FENETRES; i++)
        if (g_f[i].w == w) return &g_f[i];
    return NULL;
}

/* UPstrcmp, dans l'original : les noms ne distinguent pas les capitales. */
static int pareil(const char *a, const char *b)
{
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
        a++;
        b++;
    }
    return tolower((unsigned char)*a) == tolower((unsigned char)*b);
}

/* StrToBool, et StringToNum : « true », et un entier signé. */
static int booleen(const char *s) { return pareil(s, "true"); }
static int nombre(const char *s)  { return (int)strtol(s, NULL, 10); }

/* Un paramètre de l'appel, ou NULL s'il manque ou s'il est vide : le
 * défaut de getParam. */
static const char *param(XCmdPtr p, int i)
{
    if (i >= p->paramCount || !p->params[i]) return NULL;
    const char *s = p->params[i];
    while (*s == ' ') s++;
    return *s ? s : NULL;
}

/* ---- l'image ---- */

/* Les couleurs d'origine, celles de cam_couleur, pour les seize
 * combinaisons des quatre plans. La combinaison k lit les plans dans
 * l'ordre du FORTH : k = 8·plan 0 + 4·plan 1 + 2·plan 2 + plan 3, soit les
 * quatre bits hauts du pixel. */
static void couleurs_origine(Fenetre *f)
{
    for (int k = 0; k < 16; k++) cam_couleur((unsigned char)(k << 4), f->couleurs[k]);
}

/* La palette du dessin : un plan masqué (« planes ») compte comme éteint,
 * puis la combinaison qui reste prend sa couleur (« colors »). Les quatre
 * bits bas, qu'aucun plan n'occupe, ne changent rien. */
static unsigned char *palette(const Fenetre *f)
{
    static unsigned char p[768];
    for (int i = 0; i < 256; i++)
        memcpy(p + 3 * i, f->couleurs[((i & f->masque) >> 4) & 15], 3);
    return p;
}

/* draw : la grille entière, bordure comprise, agrandie de l'échelle. */
static void dessine(const Fenetre *f)
{
    if (f->ouverte)
        DrawXWindow(f->w, f->cam.pix, f->cam.w, f->cam.h, palette(f),
                    f->echelle);
}

/* ---- l'ouverture : main, puis DoOpen ---- */

/* maccam(rect, titre, visible, style, couleur, flottante) : les six
 * paramètres de NewXWindow, avec les défauts de l'original — 50,50,100,100,
 * « Sans Titre », visible, le style 0x800, en couleur, flottante. Rend
 * « true », ou « false »
 * si la fenêtre ne s'ouvre pas. */
static void ouvre(XCmdPtr p)
{
    Fenetre *f = de(0);
    Rect r = { 50, 50, 100, 100 };
    const char *s;
    if ((s = param(p, 0))) StrToRect(s, &r);    /* illisible : le défaut */
    const char *titre = param(p, 1) ? param(p, 1) : "Sans Titre";
    int visible   = (s = param(p, 2)) ? booleen(s) : 1;
    int style     = (s = param(p, 3)) ? nombre(s) : 0x800;
    int couleur   = (s = param(p, 4)) ? booleen(s) : 1;
    int flottante = (s = param(p, 5)) ? booleen(s) : 1;
    WindowPtr w = f ? NewXWindow(&r, titre, visible, style, couleur, flottante)
                    : 0;
    if (!w) {
        p->returnValue = "false";
        return;
    }
    memset(f, 0, sizeof *f);
    f->w = w;
    f->largeur = r.right - r.left;
    f->hauteur = r.bottom - r.top;
    f->echelle = 1;
    f->masque = 0xF0;                           /* « 1111 » : tous les plans */
    couleurs_origine(f);
    snprintf(f->nom, sizeof f->nom, "%s", titre);
    p->returnValue = "true";
}

/* DoOpen : la grille a la taille du contenu de la fenêtre. Trop petite
 * pour la machine, ou sans mémoire, la fenêtre reste vide. */
static void a_l_ouverture(Fenetre *f)
{
    f->ouverte = cam_ouvre(&f->cam, f->largeur, f->hauteur) == 0;
    dessine(f);
}

static void a_la_fermeture(Fenetre *f)
{
    if (f->ouverte) cam_ferme(&f->cam);
    memset(f, 0, sizeof *f);
}

/* ---- les messages : doMessage ---- */

/* Un pas, puis le dessin. stepfast et ses sœurs bouclent jusqu'au clic —
 * le piège Button —, ou jusqu'à Cmd-., qui le fait répondre oui. */
static void pas(Fenetre *f, int voisinage, int boucle)
{
    do {
        cam_pas(&f->cam, voisinage);
        dessine(f);
    } while (boucle && !Button());
}

/* Rend 1 si le message est de MACCAM, 0 s'il passe. */
static int message(Fenetre *f, const char *m)
{
    Cam *c = &f->cam;
    if (!f->ouverte) return 1;
    if (pareil(m, "step"))           pas(f, CAM_SELON_MAJOR, 0);
    else if (pareil(m, "stepfast"))  pas(f, CAM_SELON_MAJOR, 1);
    else if (pareil(m, "stepmfast")) pas(f, CAM_MARGOLUS, 1);
    else if (pareil(m, "stepvfast")) pas(f, CAM_VONN, 1);
    else if (pareil(m, "erasea"))    { cam_efface(c); dessine(f); }
    else if (pareil(m, "erase1"))    { cam_efface_bit(c, 0); dessine(f); }
    else if (pareil(m, "erase2"))    { cam_efface_bit(c, 1); dessine(f); }
    else if (pareil(m, "blacka"))    {}                     /* rien, comme l'original */
    else if (pareil(m, "toscrap"))   CopyXWindow(f->w);
    else {
        for (int k = 1; k <= 8; k++) {          /* fill1 à fill8 : bit 0 à 7 */
            char n[8];
            snprintf(n, sizeof n, "fill%d", k);
            if (pareil(m, n)) {
                cam_remplit(c, k - 1);
                dessine(f);
                return 1;
            }
        }
        return 0;
    }
    return 1;
}

/* ---- les propriétés : setProp, getProp ---- */

enum { PROPRIETES = 32 };

static const char *const NOMS[PROPRIETES] = {
    "hilite", "name", "visible", "height", "width", "rect",
    "image0", "image1", "image2", "image3", "image4", "image5", "image6",
    "image7", "scale", "pln7", "pln6", "pln5", "pln4", "pln3", "pln2",
    "pln1", "pln0", "palette", "PictResource", "major", "minor",
    "alternate", "phaseA", "phaseB",
    "planes", "colors"                          /* les ajouts : voir pose() */
};

static int rang(const char *prop)
{
    for (int i = 0; i < PROPRIETES; i++)
        if (pareil(prop, NOMS[i])) return i;
    return -1;
}

/* Une propriété de fenêtre, posée par HC pendant que le module tourne. */
static void pose_fenetre(const Fenetre *f, const char *prop, int valeur)
{
    char l[400];
    snprintf(l, sizeof l, "set the %s of window \"%s\" to %d", prop, f->nom,
             valeur);
    SendCardMessage(l);
}

/* image0 à image7 : la peinture de la carte dans le rectangle donné, dont
 * chaque point noir allume le bit du plan. */
static void image(Fenetre *f, int bit, const char *valeur)
{
    Rect r;
    if (!f->ouverte || !StrToRect(valeur, &r)) return;
    int l = r.right - r.left, h = r.bottom - r.top;
    if (l <= 0 || h <= 0) return;
    unsigned char *noir = malloc((size_t)l * (size_t)h);
    if (!noir) return;
    if (GetCardPicture(&r, noir)) cam_image(&f->cam, bit, noir, l, h);
    free(noir);
    dessine(f);
}

/* « colors » : les lignes « pppp r,g,b », lues dans l'ordre, chacune
 * peignant les combinaisons que son motif désigne — « 0 » ou « 1 » par
 * plan, dans l'ordre du FORTH, « x » pour l'un ou l'autre. Rend 1 si tout
 * est lu (t est alors la nouvelle table), 2 pour une valeur vide (la
 * palette d'origine), 0 au premier refus : une ligne qu'on ne comprend
 * pas fait tout refuser, et rien ne change, comme pour « planes ». */
static int couleurs_lues(const char *v, unsigned char t[16][3])
{
    while (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n') v++;
    if (!*v) return 2;
    while (*v) {
        const char *fin = v;
        while (*fin && *fin != '\r' && *fin != '\n') fin++;
        const char *s = v;
        while (s < fin && (*s == ' ' || *s == '\t')) s++;
        if (s < fin) {
            char motif[4];
            int n = 0, rgb[3];
            for (; s < fin && strchr("01xX", *s); s++)
                if (++n <= 4) motif[n - 1] = *s;
            if (n != 4 || s >= fin || (*s != ' ' && *s != '\t')) return 0;
            for (int c = 0; c < 3; c++) {
                while (s < fin && (*s == ' ' || *s == '\t')) s++;
                int x = 0, chiffres = 0;
                for (; s < fin && isdigit((unsigned char)*s); s++) {
                    x = x * 10 + (*s - '0');
                    if (++chiffres > 3) return 0;
                }
                if (!chiffres || x > 255) return 0;
                rgb[c] = x;
                while (s < fin && (*s == ' ' || *s == '\t')) s++;
                if (c < 2) {
                    if (s >= fin || *s != ',') return 0;
                    s++;
                }
            }
            if (s != fin) return 0;
            for (int k = 0; k < 16; k++) {
                int oui = 1;
                for (int b = 0; b < 4; b++) {
                    int bit = (k >> (3 - b)) & 1;
                    if ((motif[b] == '0' && bit) || (motif[b] == '1' && !bit)) oui = 0;
                }
                if (oui)
                    for (int c = 0; c < 3; c++) t[k][c] = (unsigned char)rgb[c];
            }
        }
        v = fin;
        while (*v == '\r' || *v == '\n') v++;
    }
    return 1;
}

/* Rend 1 si la propriété est de MACCAM, 0 si elle passe à HC. */
static int pose(Fenetre *f, const char *prop, const char *valeur)
{
    int i = rang(prop);
    Cam *c = &f->cam;
    switch (i) {
    case 0:                                     /* hilite : retenu, sans dessin */
        f->hilite = booleen(valeur);
        return 1;
    case 1:                                     /* name : HC renomme */
        snprintf(f->nom, sizeof f->nom, "%s", valeur);
        return 0;
    case 2: case 3: case 4: case 5:             /* visible, height, width, rect */
        return 0;
    case 14: {                                  /* scale : la fenêtre, carrée */
        int e = nombre(valeur);
        if (e < 1) e = 1;
        f->echelle = e;
        if (f->ouverte) {
            pose_fenetre(f, "width", c->w * e);
            pose_fenetre(f, "height", c->w * e);
        }
        dessine(f);
        return 1;
    }
    case 15: case 16: case 17: case 18:         /* pln7 à pln4 */
        if (f->ouverte) cam_pose_pln(c, 22 - i, valeur);
        return 1;
    case 23:                                    /* palette : la nôtre reste */
        return 1;
    case 24:                                    /* PictResource : la photo */
        CopyXWindow(f->w);
        return 1;
    case 25: c->major = nombre(valeur); return 1;
    case 26: c->minor = nombre(valeur); return 1;
    case 27: c->alternate = nombre(valeur); return 1;
    case 28: c->phaseA = nombre(valeur); return 1;
    case 29: c->phaseB = nombre(valeur); return 1;
    case 30: {
        /* « planes », l'ajout de HC, proposé par l'utilisatrice le 18
         * octobre : un chiffre par plan, dans l'ordre du FORTH — « 1000 » le
         * plan 0 seul, « 0011 » les plans 2 et 3, « 1111 » tous, le défaut.
         * Un plan masqué se DESSINE éteint ; la machine calcule toujours
         * les quatre, et la réversibilité n'en sait rien. Moins de quatre
         * chiffres se complètent à gauche : sans guillemets, « 0100 » peut
         * arriver en « 100 ». Autre chose qu'un à quatre chiffres 0 ou 1 :
         * le masque ne change pas, comme les autres propriétés qui ne
         * comprennent pas ce qu'on leur donne. Pas « palette » : dans
         * l'original, « 1000 » y est le numéro d'une clut. */
        int n = 0, m = 0;
        while (*valeur == ' ') valeur++;
        /* Le masque ne retient que les quatre premiers chiffres, comme le
         * motif de « colors » : trente-deux « 1 » de suite faisaient
         * déborder m (revue du 24 octobre, jumeau du FORTH). */
        for (; valeur[n] == '0' || valeur[n] == '1'; n++)
            if (n < 4) m = m << 1 | (valeur[n] - '0');
        const char *reste = valeur + n;
        while (*reste == ' ') reste++;
        if (n >= 1 && n <= 4 && !*reste) {
            f->masque = m << 4;
            dessine(f);
        }
        return 1;
    }
    case 31: {
        /* « colors », l'ajout de HC décidé par l'utilisatrice le 19
         * octobre, à la place des clut de 1990 : la couleur de chaque
         * combinaison de plans. « 1000 255,0,0 » peint le plan 0 seul en
         * rouge ; « 1xxx 255,255,255 », toute combinaison où le plan 0 est
         * allumé, en blanc. Les lignes jouent dans l'ordre, la dernière
         * l'emporte ; ce qu'aucune ne désigne garde sa couleur. Vide : la
         * palette d'origine. Rien ne change pour la machine, seul le
         * dessin. */
        unsigned char t[16][3];
        memcpy(t, f->couleurs, sizeof t);
        int lu = couleurs_lues(valeur, t);
        if (lu == 2) couleurs_origine(f);
        else if (lu == 1) memcpy(f->couleurs, t, sizeof t);
        if (lu) dessine(f);
        return 1;
    }
    default:
        if (i >= 6 && i <= 13) {                /* image0 à image7 */
            image(f, i - 6, valeur);
            return 1;
        }
        return i >= 0;                          /* pln3 à pln0 : rien */
    }
}

/* getpln : une table, « 1 » là où l'OCTET n'est pas nul — les deux plans de
 * la demi-machine mêlés, comme dans l'original. */
static char *table(const unsigned char *t)
{
    static char s[CAM_TABLE + 1];
    for (int i = 0; i < CAM_TABLE; i++) s[i] = t[i] ? '1' : '0';
    s[CAM_TABLE] = '\0';
    return s;
}

/* Rend la valeur, ou NULL : la propriété passe à HC. L'original servait
 * hilite, image0 (la table de CAM-A), image1 (celle de CAM-B), phaseA —
 * après un SysBeep — et phaseB ; name, visible, height, width et rect sont
 * ce que HC sert lui-même, pour toute fenêtre. Le reste passait. */
static char *lit(Fenetre *f, const char *prop)
{
    static char s[16];
    switch (rang(prop)) {
    case 0:  return f->hilite ? "true" : "false";
    case 6:  return f->ouverte ? table(f->cam.lutA) : NULL;
    case 7:  return f->ouverte ? table(f->cam.lutB) : NULL;
    case 28:
        SendCardMessage("beep");
        snprintf(s, sizeof s, "%d", f->cam.phaseA);
        return s;
    case 29:
        snprintf(s, sizeof s, "%d", f->cam.phaseB);
        return s;
    case 30:                                    /* « planes », sur quatre chiffres */
        for (int k = 0; k < 4; k++) s[k] = (f->masque & (0x80 >> k)) ? '1' : '0';
        s[4] = '\0';
        return s;
    case 31: {                                  /* « colors » : les seize, 0000 à 1111 */
        static char c[16 * 20];
        size_t n = 0;
        for (int k = 0; k < 16; k++)
            n += (size_t)snprintf(c + n, sizeof c - n, "%s%d%d%d%d %d,%d,%d",
                                  k ? "\n" : "", k >> 3 & 1, k >> 2 & 1, k >> 1 & 1,
                                  k & 1, f->couleurs[k][0], f->couleurs[k][1],
                                  f->couleurs[k][2]);
        return c;
    }
    default:
        return NULL;
    }
}

/* ---- les événements ---- */

static void evenement(XCmdPtr p)
{
    XWEventInfoPtr e = (XWEventInfoPtr)p->params[0];
    Fenetre *f = de(e->eventWindow);
    if (!f) {
        p->passFlag = 1;
        return;
    }
    switch (e->what) {
    case xOpenEvt:
        a_l_ouverture(f);
        return;
    case xCloseEvt:
        a_la_fermeture(f);
        p->passFlag = 1;
        return;
    case xSendEvt:
        if (!message(f, e->eventParams[0])) p->passFlag = 1;
        return;
    case xSetPropEvt:
        if (!pose(f, e->eventParams[0], e->eventParams[1])) p->passFlag = 1;
        return;
    case xGetPropEvt:
        e->eventResult = lit(f, e->eventParams[0]);
        if (!e->eventResult) p->passFlag = 1;
        return;
    }
    p->passFlag = 1;
}

HC_XFCN(maccam)
{
    if (paramPtr->paramCount == -1) evenement(paramPtr);
    else                            ouvre(paramPtr);
}
