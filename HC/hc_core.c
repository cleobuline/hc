/* hc_core.c — Noyau du clone HyperCard.
 * Modèle d'objets, chaîne de messages hiérarchique, mini-interpréteur.
 * C99 portable, sans dépendance.
 */
#include "hc_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <strings.h>

/* ==================== outils chaînes ==================== */

static char *dupstr(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int ci_equal(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* compare le début de `s` avec `pfx`, sans casse */
static int ci_prefix(const char *s, const char *pfx)
{
    while (*pfx) {
        if (!*s) return 0;
        if (tolower((unsigned char)*s) != tolower((unsigned char)*pfx)) return 0;
        s++; pfx++;
    }
    return 1;
}

/* compare le début de `s` avec le mot `w`, sans casse, en exigeant une
 * vraie frontière de mot : « button "x" » et « button » passent,
 * « buttonnette "x" » non. */
static int ci_word(const char *s, const char *w)
{
    if (!ci_prefix(s, w)) return 0;
    char c = s[strlen(w)];
    return !(isalnum((unsigned char)c) || c == '_');
}

static const char *skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* copie le mot suivant dans out ; renvoie la position après le mot */
static const char *next_word(const char *s, char *out, int outlen)
{
    s = skip_spaces(s);
    int i = 0;
    while (*s && !isspace((unsigned char)*s) && i < outlen - 1)
        out[i++] = *s++;
    out[i] = '\0';
    return s;
}

/* extrait un littéral entre guillemets ; renvoie la position après le guillemet fermant */
static const char *quoted(const char *s, char *out, int outlen)
{
    s = skip_spaces(s);
    out[0] = '\0';
    if (*s != '"') return s;
    s++;
    int i = 0;
    while (*s && *s != '"' && i < outlen - 1)
        out[i++] = *s++;
    out[i] = '\0';
    if (*s == '"') s++;
    return s;
}

/* ==================== état global ==================== */

/* Garde-fou : profondeur maximale d'imbrication des messages.
 * Sans lui, un gestionnaire qui se renvoie son propre message
 * (`on boum / send "boum" to me / end boum`) épuise la pile C
 * et le programme meurt. Le vrai HyperCard répondait
 * « Too much recursion » ; on fait pareil, en douceur. */
/* Taille d'une valeur manipulee par l'interpreteur : variables, arguments,
 * proprietes. C'est ce plafond qui limite « get the script of me » ; un
 * script plus long est tronque, et le reecrire le mutile.
 *
 * Les tampons vivent desormais dans l'arene (voir plus bas), pas sur la
 * pile. Ce reglage ne coute donc plus de profondeur de recursion, seulement
 * de la memoire. Mesures sur le champ Calendrier et sur une recursion nue :
 *
 *                      pile / niveau     arene au pic (profondeur 63)
 *   avant l'arene        306 752 o        --        (plafond : 26 niveaux)
 *   apres, HC_VAL 16 Ko    6 720 o        49 Mo
 *   apres, HC_VAL 64 Ko    6 720 o       202 Mo
 *
 * 16 Ko couvre tres largement les scripts d'epoque (celui du Calendrier fait
 * 9,1 Ko) et garde le pic d'arene raisonnable. L'arene retombe a zero entre
 * deux commandes, et une boucle de 20 000 tours n'y consomme que 700 Ko :
 * la liberation est bien par instruction, pas par gestionnaire.
 *
 * Le vrai correctif reste a venir : allouer chaque valeur a sa taille reelle
 * plutot qu'au plafond. La plupart des valeurs font quelques octets ; ce sont
 * les ~50 tampons vivants par niveau, tous dimensionnes au maximum, qui font
 * le pic. Cela demande de remplacer les signatures (char *out, int outlen)
 * par un type chaine dynamique — un chantier a part entiere. */
#define HC_VAL 16384

/* Le garde-fou de recursion peut revenir a sa valeur d'origine : a 6,7 Ko de
 * pile par niveau, 64 niveaux ne coutent que 436 Ko sur les 8 Mo du fil
 * principal. Une recursion emballee rend « trop de recursion » au lieu de
 * faire tomber l'application. */
#define HC_MAX_DEPTH 64

static Object *g_current_card = NULL;
static int     g_trace = 1;
static int     g_pass  = 0;   /* levé par `pass` : le message doit continuer */

/* `the result` : les commandes susceptibles d'échouer y déposent un message,
 * et le vident quand elles réussissent. Les autres commandes n'y touchent
 * pas — c'est ce qui permet d'écrire « go … » puis « put the result ». */
static char    g_result[HC_VAL] = "";

/* Levé dès qu'un « the script of … » a été tronqué faute de place dans un
 * tampon de valeur. Tant qu'il est levé, « set script of … » refuse d'écrire :
 * dans le gestionnaire courant, la valeur lue est forcément mutilée, et la
 * réécrire détruirait le script. Sauvé et remis à zéro à chaque entrée de
 * gestionnaire, pour qu'un appel imbriqué ne contamine pas son appelant. */
static int     g_script_clipped = 0;

/* Arguments du gestionnaire courant, pour `the params`, param(n), paramCount.
   g_params[0] est le nom du message ; les suivants sont les arguments. */
static char    g_params[16][HC_VAL];
static int     g_nparams = 0;

static void set_result(const char *msg) { snprintf(g_result, sizeof g_result, "%s", msg); }
static Object *g_me     = NULL;  /* l'objet dont le script s'exécute      → `me` */
static Object *g_target = NULL;  /* le destinataire initial du message    → `the target` */
static int     g_depth = 0;   /* profondeur, pour l'indentation de la trace */

void hc_trace(int on) { g_trace = on; }
void hc_set_current_card(Object *card) { g_current_card = card; }
Object *hc_current_card(void) { return g_current_card; }

const char *hc_typename(ObjType t)
{
    switch (t) {
        case OBJ_STACK:      return "stack";
        case OBJ_BACKGROUND: return "background";
        case OBJ_CARD:       return "card";
        case OBJ_BUTTON:     return "button";
        case OBJ_FIELD:      return "field";
    }
    return "?";
}

void hc_describe(Object *o, char *buf, int buflen)
{
    if (!o) { snprintf(buf, buflen, "(nul)"); return; }
    if (o->name)
        snprintf(buf, buflen, "%s \"%s\"", hc_typename(o->type), o->name);
    else
        snprintf(buf, buflen, "%s id %d", hc_typename(o->type), o->id);
}

/* ---- hôte : sortie déléguée ---- */

static void console_line(HcLineKind kind, int depth, const char *text)
{
    int n = depth > 12 ? 12 : depth;
    for (int i = 0; i < n; i++) fputs("   ", stdout);
    if (kind == HC_MSG) fputs("   [message box] ", stdout);
    fputs(text, stdout);
    fputc('\n', stdout);
}

/* Repli console : de quoi tester le noyau sans interface graphique. */
static char g_console_buf[HC_VAL];

static const char *console_ask(const char *prompt, const char *deflt)
{
    printf("   [ask] %s [%s] ", prompt, deflt ? deflt : "");
    fflush(stdout);
    if (!fgets(g_console_buf, sizeof g_console_buf, stdin)) return NULL;
    size_t n = strlen(g_console_buf);
    while (n && (g_console_buf[n-1] == '\n' || g_console_buf[n-1] == '\r'))
        g_console_buf[--n] = '\0';
    if (!g_console_buf[0] && deflt) {
        snprintf(g_console_buf, sizeof g_console_buf, "%s", deflt);
    }
    return g_console_buf;
}

static const char *console_answer(const char *prompt, const char *b1,
                                  const char *b2, const char *b3)
{
    printf("   [answer] %s  (1=%s", prompt, b1 ? b1 : "OK");
    if (b2) printf(" 2=%s", b2);
    if (b3) printf(" 3=%s", b3);
    printf(") ");
    fflush(stdout);
    if (!fgets(g_console_buf, sizeof g_console_buf, stdin)) return b1;
    int c = atoi(g_console_buf);
    if (c == 3 && b3) return b3;
    if (c == 2 && b2) return b2;
    return b1 ? b1 : "OK";
}

/* Valeurs par défaut en console : la souris est relâchée et aucune touche
 * n'est enfoncée. C'est ce qui permet à « repeat until the mouse is up » de
 * se terminer immédiatement en ligne de commande au lieu de boucler à vide. */
static const char *console_global(const char *name)
{
    if (ci_equal(name, "mouse"))      return "up";
    if (ci_equal(name, "mouseLoc"))   return "0,0";
    if (ci_equal(name, "optionKey"))  return "up";
    if (ci_equal(name, "commandKey")) return "up";
    if (ci_equal(name, "shiftKey"))   return "up";
    return NULL;
}

static const HcHost g_console_host = {
    console_line, NULL, console_ask, console_answer,
    console_global, NULL, NULL, NULL
};
static const HcHost *g_host = &g_console_host;

void hc_set_host(const HcHost *h) { g_host = h ? h : &g_console_host; }

/* Émet une ligne vers l'hôte. Le format ne doit PAS inclure le saut de ligne
   final ni l'indentation : l'hôte s'en charge. */
static void emit(HcLineKind kind, const char *fmt, ...)
{
    /* Tampon propre, hors arène : arena_buf() appelle emit() en cas de
     * saturation, et une récursion mutuelle entre l'allocateur et le
     * rapporteur d'erreurs serait fatale. Les messages sont courts. */
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (g_host && g_host->line) g_host->line(kind, g_depth, buf);
}

/* ==================== arène de tampons ====================
 *
 * Les tampons de valeur ne vivent plus sur la pile. Mesure avant ce
 * changement : 75 tampons vivants par niveau de récursion, soit 75 × HC_VAL
 * par appel de gestionnaire. HC_VAL ne pouvait donc pas dépasser quelques
 * kilo-octets sans faire déborder les 8 Mo du fil principal — et un script
 * un peu long devenait intronçonnable.
 *
 * Principe : une arène à pointeur de sommet. Allouer avance le sommet ;
 * libérer le ramène où il était. Chaque fonction note le sommet à l'entrée
 * (ARENA_MARK) et le restaure avant de rendre la main (ARENA_FREE). La
 * discipline est sûre parce qu'un appelé note toujours un sommet PLUS HAUT
 * que son appelant : sa libération ne peut donc jamais toucher les tampons
 * de celui qui l'a appelé.
 *
 * L'arène est faite de blocs chaînés jamais déplacés : un realloc
 * invaliderait les tampons déjà distribués. Les blocs restent alloués après
 * libération, et resservent — on ne paie l'allocation système qu'une fois.
 *
 * Les fonctions à sorties multiples (term_value, call_function, exec_line :
 * 40, 36 et 57 return) ne sont volontairement pas instrumentées. Leurs
 * appelants directs — parse_factor et exec_stmt — le sont, et cela suffit :
 * toute valeur est recopiée dans le `out` de l'appelant avant chaque retour.
 */
#define HC_ARENA_BLOCK     (4u * 1024u * 1024u)
#define HC_ARENA_MAXBLOCKS 32             /* plafond dur : 128 Mo */

static char  *g_ablk[HC_ARENA_MAXBLOCKS];
static int    g_ablk_count = 0;
static size_t g_atop = 0;                 /* sommet virtuel : bloc × taille + offset */
static size_t g_ahigh = 0;                /* plus haut sommet atteint (diagnostic) */
static char   g_apanic[HC_VAL];           /* filet en cas d'arène saturée */

#define ARENA_MARK  size_t _amark = g_atop
#define ARENA_FREE  (g_atop = _amark)

static void *arena_alloc(size_t n)
{
    n = (n + 15u) & ~(size_t)15;                    /* alignement confortable */
    if (n == 0 || n > HC_ARENA_BLOCK) return NULL;

    size_t bi  = g_atop / HC_ARENA_BLOCK;
    size_t off = g_atop % HC_ARENA_BLOCK;
    if (off + n > HC_ARENA_BLOCK) { bi++; off = 0; }   /* ne pas chevaucher */
    if (bi >= HC_ARENA_MAXBLOCKS) return NULL;

    while (g_ablk_count <= (int)bi) {
        char *b = (char *)malloc(HC_ARENA_BLOCK);
        if (!b) return NULL;
        g_ablk[g_ablk_count++] = b;
    }
    g_atop = bi * (size_t)HC_ARENA_BLOCK + off + n;
    if (g_atop > g_ahigh) g_ahigh = g_atop;
    return g_ablk[bi] + off;
}

/* Un tampon de valeur. Ne renvoie jamais NULL : en cas de saturation on rend
 * le filet, ce qui dégrade le résultat mais n'écrase pas la mémoire. */
static char *arena_buf(void)
{
    char *p = (char *)arena_alloc(HC_VAL);
    if (!p) { emit(HC_ERR, "   !! arène de tampons saturée"); p = g_apanic; }
    p[0] = '\0';
    return p;
}

/* n tampons contigus, pour les tableaux d'arguments. */
static char (*arena_rows(int n))[HC_VAL]
{
    char (*p)[HC_VAL] = (char (*)[HC_VAL])arena_alloc((size_t)n * HC_VAL);
    if (!p) {
        emit(HC_ERR, "   !! arène de tampons saturée");
        return (char (*)[HC_VAL])g_apanic;      /* dégradé, mais borné */
    }
    for (int i = 0; i < n; i++) p[i][0] = '\0';
    return p;
}

static void arena_shutdown(void)
{
    for (int i = 0; i < g_ablk_count; i++) free(g_ablk[i]);
    g_ablk_count = 0;
    g_atop = g_ahigh = 0;
}

/* Signale à l'hôte qu'un champ a changé (rafraîchissement d'affichage). */
static void notify_field(Object *field)
{
    if (g_host && g_host->field_changed) g_host->field_changed(field);
}

/* Propriété globale lue chez l'hôte. NULL = nom inconnu. */
static const char *host_global(const char *name)
{
    if (g_host && g_host->global_get) return g_host->global_get(name);
    return NULL;
}

static void host_global_set(const char *name, const char *value)
{
    if (g_host && g_host->global_set) g_host->global_set(name, value);
}

/* Respiration : l'hôte redessine et traite ses événements. */
static void host_idle(void)
{
    if (g_host && g_host->idle) g_host->idle();
}

/* ==================== construction ==================== */

static int g_next_id = 1;

/* resultat de la derniere recherche : the foundText / foundField / foundLine */
static char    g_found_text[256] = "";
static Object *g_found_field = NULL;
static int     g_found_line = 0;
static int     g_found_start = 0;   /* offset du motif dans le texte du champ */
static int     g_found_len   = 0;   /* longueur du motif, 0 = rien de trouve */
static Object *g_found_card  = NULL;

/* pile de navigation : push cd / pop cd */
#define NAVSTACK_MAX 64
static Object *g_navstack[NAVSTACK_MAX];
static int     g_navtop = 0;

/* Force l'identifiant d'un objet relu depuis un fichier, et garde le
 * compteur au-dessus pour ne jamais réattribuer un id existant. */
void hc_set_id(Object *o, int id)
{
    if (!o || id <= 0) return;
    o->id = id;
    if (id >= g_next_id) g_next_id = id + 1;
}

/* ---- numérotation d'une part : voir hc_core.h pour la distinction ----
 * Ces deux fonctions sont la SEULE définition du rang. Le dialogue Infos les
 * recalculait de son côté, à partir de l'index brut dans parts[] : un champ
 * posé après cinq boutons s'y annonçait « Field number: 6 », et le script
 * « card field 6 » écrit sur cette foi ne désignait rien. */
int hc_object_number(Object *o)
{
    if (!o || !o->owner) return 0;
    int n = 0;
    for (int i = 0; i < o->owner->nparts; i++) {
        Object *p = o->owner->parts[i];
        if (p->type != o->type) continue;
        n++;
        if (p == o) return n;
    }
    return 0;
}

int hc_part_number(Object *o)
{
    if (!o || !o->owner) return 0;
    if (o->type != OBJ_BUTTON && o->type != OBJ_FIELD) return 0;
    int n = 0;
    for (int i = 0; i < o->owner->nparts; i++) {
        Object *p = o->owner->parts[i];
        if (p->type != OBJ_BUTTON && p->type != OBJ_FIELD) continue;
        n++;
        if (p == o) return n;
    }
    return 0;
}

int hc_owner_is_bg(Object *o)
{
    return (o && o->owner && o->owner->type == OBJ_BACKGROUND) ? 1 : 0;
}

int hc_part_count(Object *owner, ObjType type)
{
    if (!owner) return 0;
    int n = 0;
    for (int i = 0; i < owner->nparts; i++)
        if (owner->parts[i]->type == type) n++;
    return n;
}

/* Les plages de style sont definies bien plus bas, mais hc_free en a besoin. */
static void runs_free(struct RunList *rl);

static Object *new_object(ObjType type, Object *owner, const char *name)
{
    Object *o = calloc(1, sizeof(Object));
    if (!o) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
    o->type    = type;
    o->id      = g_next_id++;
    o->name    = dupstr(name);
    o->owner   = owner;
    o->visible = 1;
    o->showname = 1;   /* le nom s'affiche par défaut */
    return o;
}

static void add_part(Object *parent, Object *child)
{
    if (parent->nparts == parent->capparts) {
        int cap = parent->capparts ? parent->capparts * 2 : 4;
        Object **p = realloc(parent->parts, (size_t)cap * sizeof(Object *));
        if (!p) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
        parent->parts = p;
        parent->capparts = cap;
    }
    parent->parts[parent->nparts++] = child;
}

Object *hc_new_stack(const char *name)
{
    Object *o = new_object(OBJ_STACK, NULL, name);
    o->w = 512; o->h = 342;   /* taille de carte par défaut (comme le Mac classique) */
    return o;
}

Object *hc_new_background(Object *stack, const char *name)
{
    Object *o = new_object(OBJ_BACKGROUND, stack, name);
    add_part(stack, o);
    return o;
}

Object *hc_new_card(Object *stack, Object *bg, const char *name)
{
    Object *o = new_object(OBJ_CARD, stack, name);
    o->bg = bg;
    add_part(stack, o);
    return o;
}

Object *hc_new_button(Object *owner, const char *name)
{
    Object *o = new_object(OBJ_BUTTON, owner, name);
    o->x = 20; o->y = 20; o->w = 120; o->h = 24;   /* défaut HyperCard-ish */
    add_part(owner, o);
    return o;
}

Object *hc_new_field(Object *owner, const char *name)
{
    Object *o = new_object(OBJ_FIELD, owner, name);
    o->contents = dupstr("");
    o->x = 20; o->y = 60; o->w = 200; o->h = 100;
    add_part(owner, o);
    return o;
}

/* Normalise les fins de ligne : « \r\n » (Windows) et « \r » seul (Mac
 * classique) deviennent « \n ». Sans ça, un script en « \r » n'est qu'une
 * seule ligne géante : find_handler reconnaît bien « on mouseUp », mais le
 * corps — cherché après le premier « \n » — est vide. Le gestionnaire est
 * alors annoncé comme traité et ne fait rien, ce qui est très déroutant.
 * Les scripts d'HyperCard d'origine sont tous en « \r ». */
/* Caractères spéciaux du Macintosh. Les scripts d'origine sont encodés en
 * MacRoman ; recopiés depuis un navigateur ils arrivent en UTF-8. On accepte
 * les deux, car on ne peut pas savoir d'où vient la pile :
 *
 *     ¬   0xC2   /  0xC2 0xAC        continuation : la ligne suivante suit
 *     ≠   0xAD   /  0xE2 0x89 0xA0   devient « <> »
 *     ≤   0xB2   /  0xE2 0x89 0xA4   devient « <= »
 *     ≥   0xB3   /  0xE2 0x89 0xA5   devient « >= »
 *
 * L'ambiguïté du 0xC2 se lève seule : en UTF-8 il est toujours suivi d'un
 * octet de continuation (0x80-0xBF), et l'unique séquence qui nous intéresse
 * est 0xC2 0xAC. Un 0xC2 suivi d'autre chose est un ¬ MacRoman ; un 0xC2
 * suivi d'un octet de continuation autre que 0xAC est un caractère UTF-8
 * quelconque (« ² », « ° »…) qu'on recopie intact.
 *
 * La sortie peut être plus longue que l'entrée (un octet « ≠ » devient deux),
 * d'où l'allocation au double. */
static int is_utf8_cont(unsigned char c) { return c >= 0x80 && c <= 0xBF; }

static char *dup_script(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = (char *)malloc(2 * n + 2);
    if (!d) return NULL;
    char *w = d;

    for (const unsigned char *p = (const unsigned char *)s; *p; ) {
        /* --- continuation de ligne --- */
        int cont = 0;
        if (p[0] == 0xC2 && p[1] == 0xAC)              { cont = 1; p += 2; }
        else if (p[0] == 0xC2 && !is_utf8_cont(p[1]))  { cont = 1; p += 1; }
        if (cont) {
            /* avaler les blancs puis la fin de ligne : les deux lignes n'en
             * font plus qu'une, séparées par une espace. */
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\r') { p++; if (*p == '\n') p++; }
            else if (*p == '\n') p++;
            *w++ = ' ';
            continue;
        }

        /* --- opérateurs de comparaison --- */
        if (p[0] == 0xE2 && p[1] == 0x89) {
            if (p[2] == 0xA0) { *w++ = '<'; *w++ = '>'; p += 3; continue; }
            if (p[2] == 0xA4) { *w++ = '<'; *w++ = '='; p += 3; continue; }
            if (p[2] == 0xA5) { *w++ = '>'; *w++ = '='; p += 3; continue; }
        }
        if (p[0] == 0xAD) { *w++ = '<'; *w++ = '>'; p++; continue; }
        if (p[0] == 0xB2) { *w++ = '<'; *w++ = '='; p++; continue; }
        if (p[0] == 0xB3) { *w++ = '>'; *w++ = '='; p++; continue; }

        /* --- caractère UTF-8 multi-octets : recopie intégrale --- */
        if (p[0] >= 0xC2 && p[0] <= 0xF4) {
            int len = p[0] >= 0xF0 ? 4 : p[0] >= 0xE0 ? 3 : 2;
            for (int i = 0; i < len && p[i]; i++) *w++ = (char)p[i];
            while (len-- && *p) p++;
            continue;
        }

        /* --- fins de ligne --- */
        if (p[0] == '\r') {
            p++;
            if (*p == '\n') p++;          /* \r\n : une seule fin de ligne */
            *w++ = '\n';
            continue;
        }

        *w++ = (char)*p++;
    }
    *w = '\0';
    return d;
}

void hc_set_script(Object *o, const char *script)
{
    free(o->script);
    o->script = dup_script(script);
}

void hc_free(Object *o)
{
    if (!o) return;
    for (int i = 0; i < o->nparts; i++) hc_free(o->parts[i]);
    free(o->parts);
    free(o->name);
    free(o->script);
    free(o->contents);
    free(o->style);
    free(o->textfont);
    for (int i = 0; i < o->nbgtexts; i++) {
        free(o->bgtexts[i].text);
        runs_free(&o->bgtexts[i].runs);
    }
    free(o->bgtexts);
    runs_free(&o->runs);
    free(o->paint);
    free(o);
}

/* Retire un objet (bouton/champ) de la couche de son propriétaire et le libère.
 * Renvoie 1 si trouvé et supprimé, 0 sinon. */
int hc_delete_part(Object *o)
{
    if (!o || !o->owner) return 0;
    Object *parent = o->owner;
    for (int i = 0; i < parent->nparts; i++) {
        if (parent->parts[i] == o) {
            /* décaler les suivants pour combler le trou */
            for (int j = i; j < parent->nparts - 1; j++)
                parent->parts[j] = parent->parts[j + 1];
            parent->nparts--;
            hc_free(o);
            return 1;
        }
    }
    return 0;
}

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

/* Définis plus bas, mais le presse-papiers en a besoin ici. */
static int runs_room(struct RunList *rl, int need);

/* Clone profond d'un bouton ou d'un champ. Le clone n'a PAS de propriétaire
 * et garde l'identifiant de l'original : c'est hc_paste_part qui en attribue
 * un neuf au moment de la pose, parce que c'est là seulement qu'on sait dans
 * quelle pile il atterrit. */
static Object *clone_part(Object *o)
{
    if (!o || (o->type != OBJ_BUTTON && o->type != OBJ_FIELD)) return NULL;

    Object *c = calloc(1, sizeof(Object));
    if (!c) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }

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
    c->icon         = o->icon;
    c->selectedline = o->selectedline;
    c->locktext     = o->locktext;
    c->wide_margins = o->wide_margins;
    c->fixed_lh     = o->fixed_lh;
    c->show_lines   = o->show_lines;
    c->auto_tab     = o->auto_tab;
    c->dont_search  = o->dont_search;
    c->shared_text  = o->shared_text;
    c->textstyle    = o->textstyle;
    c->scroll       = o->scroll;

    /* Les plages de style : chaque nom de police est duppé à son tour, sinon
     * deux objets partageraient le même pointeur et le second hc_free()
     * libérerait une seconde fois. */
    if (o->runs.n > 0 && runs_room(&c->runs, o->runs.n)) {
        for (int i = 0; i < o->runs.n; i++) {
            c->runs.v[i] = o->runs.v[i];
            c->runs.v[i].font = dupstr(o->runs.v[i].font);
        }
        c->runs.n = o->runs.n;
    }

    return c;
}

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
    c->id = g_next_id++;
    c->owner = owner;

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
    return c;
}

Object *hc_clipboard_part(void) { return g_clipboard; }

void hc_clipboard_clear(void)
{
    if (g_clipboard) hc_free(g_clipboard);
    g_clipboard = NULL;
}

/* ==================== chaîne de messages ==================== */

/* Construit la chaîne de remontée depuis `target`.
 * Fidèle à HyperCard : un objet (de carte OU de fond) remonte d'abord
 * à la carte courante, puis à son fond, puis à la pile.
 */
static int build_chain(Object *target, Object *chain[], int max)
{
    int n = 0;
    if (!target || max < 1) return 0;

    chain[n++] = target;

    Object *card = NULL, *bg = NULL, *stack = NULL;

    switch (target->type) {
        case OBJ_BUTTON:
        case OBJ_FIELD:
            /* le propriétaire est soit une carte, soit un fond */
            if (target->owner && target->owner->type == OBJ_CARD) {
                card = target->owner;
            } else {
                /* objet de fond : on passe par la carte courante */
                card = g_current_card;
            }
            break;
        case OBJ_CARD:
            card = target;
            break;
        case OBJ_BACKGROUND:
            bg = target;
            break;
        case OBJ_STACK:
            stack = target;
            break;
    }

    if (card && card != target && n < max) chain[n++] = card;
    if (card && !bg) bg = card->bg;
    if (bg && bg != target && n < max) chain[n++] = bg;

    if (!stack) {
        if (card)       stack = card->owner;
        else if (bg)    stack = bg->owner;
        else if (target->owner) stack = target->owner;
    }
    if (stack && stack != target && n < max) chain[n++] = stack;

    return n;
}

/* ==================== recherche de gestionnaire ==================== */

/* Cherche `on <message>` en début de ligne dans le script.
 * Renvoie un pointeur sur le début du corps, et remplit *end avec la fin.
 */
/* isfunc : 0 cherche « on <nom> », 1 cherche « function <nom> ».
 * Les deux familles vivent dans le même script et ne se marchent pas dessus :
 * HyperCard permet à une pile d'avoir « on date » et « function date ». */
static const char *find_handler_k(const char *script, const char *message,
                                  int isfunc,
                                  const char **body_end, const char **hdr_out)
{
    if (!script) return NULL;
    const char *p = script;
    const char *kw = isfunc ? "function " : "on ";
    int kwlen = isfunc ? 9 : 3;

    while (*p) {
        const char *line = skip_spaces(p);
        if (ci_prefix(line, kw)) {
            char name[64];
            next_word(line + kwlen, name, sizeof name);
            if (ci_equal(name, message)) {
                if (hdr_out) *hdr_out = line;   /* l'en-tête, pour ses paramètres */
                /* corps = après la fin de cette ligne */
                const char *body = strchr(line, '\n');
                body = body ? body + 1 : line + strlen(line);
                /* chercher `end <message>` */
                const char *q = body;
                while (*q) {
                    const char *l2 = skip_spaces(q);
                    if (ci_prefix(l2, "end ")) {
                        char n2[64];
                        next_word(l2 + 4, n2, sizeof n2);
                        if (ci_equal(n2, message)) {
                            *body_end = q;
                            return body;
                        }
                    }
                    const char *nl = strchr(q, '\n');
                    if (!nl) break;
                    q = nl + 1;
                }
                /* pas de `end` : le corps va jusqu'au bout */
                *body_end = body + strlen(body);
                return body;
            }
        }
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    return NULL;
}

static const char *find_handler(const char *script, const char *message,
                                const char **body_end, const char **hdr_out)
{
    return find_handler_k(script, message, 0, body_end, hdr_out);
}

/* ==================== résolution de références ==================== */

static Object *find_part(Object *owner, ObjType type, const char *name)
{
    if (!owner) return NULL;
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (p->type == type && p->name && ci_equal(p->name, name)) return p;
    }
    return NULL;
}

/* part par id absolu */
static Object *find_part_by_id(Object *owner, ObjType type, int id)
{
    if (!owner) return NULL;
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (p->type == type && p->id == id) return p;
    }
    return NULL;
}

/* part par rang parmi les objets de ce type (1-based) */
static Object *find_part_by_rank(Object *owner, ObjType type, int rank)
{
    if (!owner || rank < 1) return NULL;
    int n = 0;
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (p->type == type && ++n == rank) return p;
    }
    return NULL;
}

static Object *find_card_by_name(Object *stack, const char *name)
{
    if (!stack) return NULL;
    for (int i = 0; i < stack->nparts; i++) {
        Object *p = stack->parts[i];
        if (p->type == OBJ_CARD && p->name && ci_equal(p->name, name)) return p;
    }
    return NULL;
}

/* ---- cartes : l'ordre, pour « go next card » ---- */

static int card_count(Object *stack)
{
    int n = 0;
    if (!stack) return 0;
    for (int i = 0; i < stack->nparts; i++)
        if (stack->parts[i]->type == OBJ_CARD) n++;
    return n;
}

/* n-ième carte, 0-based, en ne comptant que les cartes */
static Object *nth_card(Object *stack, int n)
{
    if (!stack || n < 0) return NULL;
    for (int i = 0; i < stack->nparts; i++) {
        if (stack->parts[i]->type != OBJ_CARD) continue;
        if (n-- == 0) return stack->parts[i];
    }
    return NULL;
}

static int card_index(Object *stack, Object *card)
{
    int n = 0;
    if (!stack || !card) return -1;
    for (int i = 0; i < stack->nparts; i++) {
        if (stack->parts[i]->type != OBJ_CARD) continue;
        if (stack->parts[i] == card) return n;
        n++;
    }
    return -1;
}

/* Déclarés ici parce que resolve() en a besoin : un descripteur d'objet peut
 * porter une expression (« field f », « button (i+1) »), qu'il faut donc
 * pouvoir évaluer alors que les variables et l'analyseur ne sont définis que
 * bien plus bas. */
static const char *var_get(const char *name);
static void eval_expr(const char *s, char *out, int outlen);

/* Évalue le jeton qui désigne un objet, quand ce n'est ni un nombre littéral
 * ni une chaîne entre guillemets. Trois formes, dans cet ordre :
 *
 *   (expr)      évaluée par l'analyseur — « button (i + 1) »
 *   identif.    lue comme variable      — « field f »
 *   le reste    rendu tel quel          — « field toto », nom nu
 *
 * On n'appelle l'analyseur QUE sur la forme parenthésée. Un identificateur nu
 * passe par var_get directement : eval_expr repasserait par term_value, qui
 * commence justement par appeler resolve(), et l'on tournerait en rond sur
 * une tournure inattendue. La parenthèse, elle, est un signal explicite du
 * script, et son contenu ne peut pas se replier sur le jeton d'origine.
 *
 * Sortie vide si le jeton est composé de plusieurs mots : c'est alors une
 * tournure que resolve() ne sait pas lire, et mieux vaut ne rien prétendre. */
static void eval_id_token(const char *ref, char *out, int outlen)
{
    out[0] = '\0';
    ref = skip_spaces(ref);
    if (!*ref) return;

    if (*ref == '(') { eval_expr(ref, out, outlen); }
    else {
        char tok[128];
        const char *after = next_word(ref, tok, sizeof tok);
        if (*skip_spaces(after)) return;          /* plusieurs mots : on passe */
        const char *v = var_get(tok);
        snprintf(out, outlen, "%s", v ? v : tok);
    }

    /* Les espaces de bord fausseraient aussi bien le test « est-ce un
     * nombre ? » que la comparaison de nom. */
    int n = (int)strlen(out);
    while (n > 0 && isspace((unsigned char)out[n-1])) out[--n] = '\0';
    int lead = 0;
    while (out[lead] && isspace((unsigned char)out[lead])) lead++;
    if (lead) memmove(out, out + lead, strlen(out + lead) + 1);
}

/* Résout une référence du genre :
 *   button "ok"        (carte courante, puis fond)
 *   bg button "nav"    (fond de la carte courante)
 *   field "notes"
 *   field f            (rang ou nom pris dans une variable)
 *   button (i + 1)     (expression entre parenthèses)
 *   card "accueil"
 *   this card / next card / previous card / first card / last card
 *   me / the target
 *   stack
 */
static Object *resolve(const char *ref)
{
    ref = skip_spaces(ref);
    Object *card = g_current_card;
    Object *bg   = card ? card->bg : NULL;
    Object *stack = card ? card->owner : NULL;

    /* « the » est facultatif devant un descripteur : the field "notes",
       the target, the next card… */
    if (ci_word(ref, "the")) ref = skip_spaces(ref + 3);

    /* --- pronoms --- */
    if (ci_word(ref, "me")) return g_me;
    if (ci_word(ref, "target")) return g_target;

    int want_bg = 0;
    if (ci_word(ref, "bg") || ci_word(ref, "background")) {
        want_bg = 1;
        ref = skip_spaces(strchr(ref, ' ') ? strchr(ref, ' ') : ref + strlen(ref));
        if (!*ref) return bg;   /* « background » seul = le fond de la carte courante */

        /* « background "second" », « bg id 4 », « background 2 » : le fond
         * lui-même, et non un objet posé dessus. Sans ceci, « background »
         * n'est qu'un préfixe pour « bg button … » et un fond nommé reste
         * introuvable. */
        if (*ref == '"') {
            char nm[128];
            quoted(ref, nm, sizeof nm);
            for (int i = 0; stack && i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_BACKGROUND &&
                    stack->parts[i]->name && ci_equal(stack->parts[i]->name, nm))
                    return stack->parts[i];
            return NULL;
        }
        if (ci_word(ref, "id")) {
            int wanted = atoi(skip_spaces(ref + 2));
            for (int i = 0; stack && i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_BACKGROUND &&
                    stack->parts[i]->id == wanted)
                    return stack->parts[i];
            return NULL;
        }
        if (isdigit((unsigned char)*ref)) {
            int n = atoi(ref) - 1;          /* 1-based en HyperTalk */
            for (int i = 0; stack && i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_BACKGROUND && n-- == 0)
                    return stack->parts[i];
            return NULL;
        }
        /* « go background commun » : nom de fond sans guillemets. */
        if (!ci_word(ref, "button") && !ci_word(ref, "btn") &&
            !ci_word(ref, "field")  && !ci_word(ref, "fld")  &&
            !ci_word(ref, "part")) {
            char nm[128];
            int n = 0;
            while (ref[n] && n < (int)sizeof nm - 1) { nm[n] = ref[n]; n++; }
            while (n > 0 && isspace((unsigned char)nm[n-1])) n--;
            nm[n] = '\0';
            for (int i = 0; stack && i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_BACKGROUND &&
                    stack->parts[i]->name && ci_equal(stack->parts[i]->name, nm))
                    return stack->parts[i];
        }
    } else if (ci_word(ref, "card") || ci_word(ref, "cd")) {
        /* "card button" / "card field" / "card \"nom\"" / "card 3" */
        const char *after = skip_spaces(strchr(ref, ' ') ? strchr(ref, ' ') : ref + strlen(ref));
        if (*after == '"') {
            char nm[128];
            quoted(after, nm, sizeof nm);
            return find_card_by_name(stack, nm);
        }
        if (ci_word(after, "id")) {                    /* card id N */
            int wanted = atoi(skip_spaces(after + 2));
            for (int i = 0; i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_CARD && stack->parts[i]->id == wanted)
                    return stack->parts[i];
            return NULL;
        }
        if (isdigit((unsigned char)*after))
            return nth_card(stack, atoi(after) - 1);   /* 1-based en HyperTalk */

        /* « go card canard » : HyperCard accepte un nom de carte sans
         * guillemets. On ne tente le nom nu que si ce qui suit n'est pas un
         * objet posé sur la carte, sinon « card button "ok" » y passerait. */
        if (*after && !ci_word(after, "button") && !ci_word(after, "btn") &&
                      !ci_word(after, "field")  && !ci_word(after, "fld") &&
                      !ci_word(after, "part")   && !ci_word(after, "window")) {
            char nm[128];
            int n = 0;
            while (after[n] && n < (int)sizeof nm - 1) { nm[n] = after[n]; n++; }
            while (n > 0 && isspace((unsigned char)nm[n-1])) n--;
            nm[n] = '\0';
            Object *c = find_card_by_name(stack, nm);
            if (c) return c;
        }
        ref = after;
    }

    /* --- cartes désignées par leur rang --- */
    if (ci_word(ref, "this")) {
        const char *w = skip_spaces(ref + 4);
        if (!*w || ci_word(w, "card") || ci_word(w, "cd")) return card;
        if (ci_word(w, "stack")) return stack;
        if (ci_word(w, "background") || ci_word(w, "bg")) return bg;
    }
    if (ci_word(ref, "next"))  return nth_card(stack, card_index(stack, card) + 1);
    if (ci_word(ref, "prev") || ci_word(ref, "previous"))
                               return nth_card(stack, card_index(stack, card) - 1);
    if (ci_word(ref, "first")) return nth_card(stack, 0);
    if (ci_word(ref, "last"))  return nth_card(stack, card_count(stack) - 1);

    if (ci_word(ref, "stack")) {
        const char *after = skip_spaces(ref + 5);
        if (!*after) return stack;
        if (*after == '"') {                 /* « stack "Essai" » */
            char nm[128];
            quoted(after, nm, sizeof nm);
            if (stack && stack->name && ci_equal(stack->name, nm)) return stack;
            return NULL;                      /* une seule pile ouverte à la fois */
        }
    }

    ObjType t;
    if (ci_word(ref, "button") || ci_word(ref, "btn")) { t = OBJ_BUTTON; }
    else if (ci_word(ref, "field") || ci_word(ref, "fld")) { t = OBJ_FIELD; }
    else return NULL;
    while (*ref && !isspace((unsigned char)*ref) && *ref != '"') ref++;
    ref = skip_spaces(ref);

    /* --- button id N / field id N --- */
    if (ci_word(ref, "id")) {
        const char *a = skip_spaces(ref + 2);
        int wanted;
        if (isdigit((unsigned char)*a)) {
            wanted = atoi(a);
        } else {
            /* « field id n » : l'identifiant vient d'une variable. */
            char v[128];
            eval_id_token(a, v, sizeof v);
            wanted = atoi(v);
        }
        Object *o = find_part_by_id(card, t, wanted);
        if (!o) o = find_part_by_id(bg, t, wanted);
        return o;
    }

    /* --- button N (par rang, 1-based) --- */
    if (isdigit((unsigned char)*ref)) {
        int n = atoi(ref);
        Object *o = find_part_by_rank(want_bg ? bg : card, t, n);
        if (!o && !want_bg) o = find_part_by_rank(bg, t, n);
        return o;
    }

    char nm[256];
    nm[0] = '\0';

    if (*ref == '"') {
        quoted(ref, nm, sizeof nm);
    } else {
        /* --- désignateur dynamique : « field f », « button (i + 1) » ------
         * HyperCard accepte une expression là où l'on écrit d'ordinaire un
         * rang ou un nom. C'est ce qui rend les boucles possibles :
         *
         *     repeat with f = 1 to the number of fields
         *         set the textStyle of field f to plain
         *     end repeat
         *
         * Le jeton n'est ici ni un chiffre ni une chaîne entre guillemets :
         * on l'évalue, puis on regarde CE QUI EN SORT — un nombre désigne un
         * rang, autre chose un nom. La variable n'a donc pas à savoir laquelle
         * des deux formes elle porte, exactement comme « field 3 » et
         * « field "titre" » cohabitent.
         *
         * Une variable jamais affectée vaut son propre nom en HyperTalk :
         * « field toto » retombe naturellement sur le champ nommé toto, sans
         * cas particulier. Auparavant quoted() rendait une chaîne vide sur un
         * jeton nu et resolve() abandonnait aussitôt — les deux formes
         * échouaient ensemble. */
        eval_id_token(ref, nm, sizeof nm);

        /* Un rang, si tout ce qui sort est un nombre. */
        int nlen = (int)strlen(nm);
        if (nlen > 0 && (int)strspn(nm, "0123456789") == nlen) {
            int n = atoi(nm);
            Object *o = find_part_by_rank(want_bg ? bg : card, t, n);
            if (!o && !want_bg) o = find_part_by_rank(bg, t, n);
            return o;
        }
    }

    if (!nm[0]) return NULL;

    Object *o = NULL;
    if (want_bg) {
        o = find_part(bg, t, nm);
    } else {
        o = find_part(card, t, nm);
        if (!o) o = find_part(bg, t, nm);   /* repli sur le fond */
    }
    return o;
}

/* ==================== variables ==================== */

/* HyperTalk : une variable est locale au gestionnaire qui l'emploie,
 * sauf si celui-ci l'a déclarée `global`. La boîte de message, elle,
 * travaille directement dans l'espace global. */

typedef struct { char *name, *val; } Var;

typedef struct Frame {
    Var   *v;   int n,   cap;
    char **gl;  int ngl, capgl;   /* noms déclarés `global` dans ce gestionnaire */
} Frame;

static Frame  g_globals;        /* vit aussi longtemps que le programme */
static Frame *g_frame = NULL;   /* gestionnaire en cours ; NULL = boîte de message */

static int frame_has_global(Frame *f, const char *name)
{
    if (!f) return 0;
    for (int i = 0; i < f->ngl; i++)
        if (ci_equal(f->gl[i], name)) return 1;
    return 0;
}

/* la table où vit `name` */
static Frame *frame_for(const char *name)
{
    if (!g_frame) return &g_globals;
    if (frame_has_global(g_frame, name)) return &g_globals;
    return g_frame;
}

static const char *var_get(const char *name)
{
    Frame *f = frame_for(name);
    for (int i = 0; i < f->n; i++)
        if (ci_equal(f->v[i].name, name)) return f->v[i].val;
    return NULL;
}

static void var_set(const char *name, const char *val)
{
    Frame *f = frame_for(name);
    for (int i = 0; i < f->n; i++)
        if (ci_equal(f->v[i].name, name)) {
            free(f->v[i].val);
            f->v[i].val = dupstr(val);
            return;
        }
    if (f->n == f->cap) {
        int cap = f->cap ? f->cap * 2 : 8;
        Var *p = realloc(f->v, (size_t)cap * sizeof *p);
        if (!p) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
        f->v = p; f->cap = cap;
    }
    f->v[f->n].name = dupstr(name);
    f->v[f->n].val  = dupstr(val);
    f->n++;
}

static void frame_declare_global(Frame *f, const char *name)
{
    if (!f || !*name || frame_has_global(f, name)) return;
    if (f->ngl == f->capgl) {
        int cap = f->capgl ? f->capgl * 2 : 4;
        char **p = realloc(f->gl, (size_t)cap * sizeof *p);
        if (!p) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
        f->gl = p; f->capgl = cap;
    }
    f->gl[f->ngl++] = dupstr(name);
}

static void frame_clear(Frame *f)
{
    for (int i = 0; i < f->n; i++)   { free(f->v[i].name); free(f->v[i].val); }
    for (int i = 0; i < f->ngl; i++) { free(f->gl[i]); }
    free(f->v); free(f->gl);
    memset(f, 0, sizeof *f);
}

void hc_shutdown(void)
{
    /* Le presse-papiers survit volontairement à la pile qui l'a rempli :
     * c'est ce qui permet de coller d'une pile à l'autre. Il faut donc le
     * libérer ici, et pas dans hc_free. */
    hc_clipboard_clear();
    frame_clear(&g_globals);
    arena_shutdown();
}

/* ==================== évaluation d'expressions ==================== */

/*  expr     := ou
 *  ou       := et        { "or"  et }
 *  et       := non       { "and" non }
 *  non      := [ "not" ] compare
 *  compare  := concat [ (= <> is "is not" < > <= >= contains) concat ]
 *  concat   := somme     { (& | &&) somme }
 *  somme    := produit   { (+ | -) produit }
 *  produit  := facteur   { (* | / | mod | div) facteur }
 *  facteur  := "(" expr ")" | "-" facteur | nombre | "…" | référence
 */

static int ci_nequal(const char *a, const char *b, int len)
{
    for (int i = 0; i < len; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
    return 1;
}

static int ci_cmp(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* « a contient b », sans tenir compte de la casse */
static int ci_strstr(const char *hay, const char *needle)
{
    size_t nl = strlen(needle);
    if (nl == 0) return 1;
    size_t hl = strlen(hay);
    if (nl > hl) return 0;
    for (size_t i = 0; i + nl <= hl; i++)
        if (ci_nequal(hay + i, needle, (int)nl)) return 1;
    return 0;
}

/* une valeur est-elle un nombre ? */
static int as_num(const char *s, double *d)
{
    if (!s) return 0;
    const char *t = skip_spaces(s);
    if (!*t) return 0;
    char *e;
    double v = strtod(t, &e);
    if (e == t) return 0;
    while (*e == ' ' || *e == '\t') e++;
    if (*e) return 0;
    *d = v;
    return 1;
}

static void put_num(double d, char *out, int outlen)
{
    if (d == (double)(long long)d && d > -1e15 && d < 1e15)
        snprintf(out, outlen, "%lld", (long long)d);
    else
        snprintf(out, outlen, "%.10g", d);
}

/* mots qui terminent une référence : ce sont des opérateurs ou des
   prépositions de commande, jamais des morceaux de nom d'objet */
static int is_stop_word(const char *s, int len)
{
    static const char *kw[] = { "is", "contains", "mod", "div", "and", "or",
                                "not", "then", "into", "after", "before", NULL };
    for (int i = 0; kw[i]; i++)
        if ((int)strlen(kw[i]) == len && ci_nequal(s, kw[i], len)) return 1;
    return 0;
}

/* Ramasse le texte d'une référence (« the name of me », « field "x" », « compteur »)
   en s'arrêtant au premier opérateur. */
static void collect_ref(const char **p, char *buf, int buflen)
{
    const char *s = *p;
    int n = 0;
    buf[0] = '\0';

    for (;;) {
        const char *w = skip_spaces(s);

        /* « word (i+1) of x » : le groupe parenthésé fait partie de la référence */
        if (*w == '(' && n > 0) {
            const char *q = w + 1;
            int depth = 1, inq2 = 0;
            while (*q && depth) {
                if (*q == '"') inq2 = !inq2;
                else if (!inq2 && *q == '(') depth++;
                else if (!inq2 && *q == ')') depth--;
                q++;
            }
            int len = (int)(q - w);
            if (n && n < buflen - 1) buf[n++] = ' ';
            if (n + len > buflen - 1) len = buflen - 1 - n;
            if (len > 0) { memcpy(buf + n, w, (size_t)len); n += len; }
            buf[n] = '\0';
            s = q;
            continue;
        }

        if (!*w || strchr("&+-*/^<>=(),", *w)) { s = w; break; }

        const char *st = w, *q = w;
        if (*w == '"') {
            q = w + 1;
            while (*q && *q != '"') q++;
            if (*q == '"') q++;
        } else {
            while (*q && !isspace((unsigned char)*q) && !strchr("&+-*/^<>=(),\"", *q)) q++;
            if (is_stop_word(st, (int)(q - st))) { s = w; break; }
        }

        int len = (int)(q - st);
        if (n && n < buflen - 1) buf[n++] = ' ';
        if (n + len > buflen - 1) len = buflen - 1 - n;
        if (len > 0) { memcpy(buf + n, st, (size_t)len); n += len; }
        buf[n] = '\0';
        s = q;
    }
    *p = s;
}

/* ==================== expressions de morceau (chunks) ==================== */

/* La signature d'HyperTalk : découper du texte sans effort.
 *     char 3 of x        char 1 to 5 of x
 *     word 2 of x        word 2 to 4 of x
 *     item 3 of x        line 2 of field "notes"
 *     the first word of x, the last line of x, the middle item of x
 *     the number of words in x
 * Les morceaux s'emboîtent : « word 2 of line 3 of field "notes" ».
 */

typedef enum { CH_NONE, CH_CHAR, CH_WORD, CH_ITEM, CH_LINE } ChunkType;

static void eval_expr(const char *s, char *out, int outlen);
static const char *find_kw(const char *s, const char *w);   /* défini plus bas */
static int hc_send_args(Object *target, const char *message,
                        char argv[][HC_VAL], int argc);   /* défini plus bas */
static int hc_call_user_function(Object *target, const char *name,
                                 char argv[][HC_VAL], int argc);  /* idem */

/* Reconnaît un nom de morceau et dit combien de caractères il occupe. */
static ChunkType chunk_kind(const char *s, int *used)
{
    static const struct { const char *w; ChunkType t; } tab[] = {
        { "characters", CH_CHAR }, { "character", CH_CHAR },
        { "chars", CH_CHAR }, { "char", CH_CHAR },
        { "words", CH_WORD }, { "word", CH_WORD },
        { "items", CH_ITEM }, { "item", CH_ITEM },
        { "lines", CH_LINE }, { "line", CH_LINE },
        { NULL, CH_NONE }
    };
    for (int i = 0; tab[i].w; i++)
        if (ci_word(s, tab[i].w)) { *used = (int)strlen(tab[i].w); return tab[i].t; }
    return CH_NONE;
}

static char chunk_sep(ChunkType t)
{
    if (t == CH_ITEM) return ',';
    if (t == CH_LINE) return '\n';
    if (t == CH_WORD) return ' ';
    return '\0';
}

/* Combien de morceaux de ce type dans `s` ? */
static int chunk_count(const char *s, ChunkType t)
{
    int len = (int)strlen(s);
    if (t == CH_CHAR) return len;
    if (len == 0) return 0;

    if (t == CH_ITEM || t == CH_LINE) {
        char d = chunk_sep(t);
        int n = 1;
        for (int i = 0; i < len; i++) if (s[i] == d) n++;
        if (s[len-1] == d) n--;          /* un séparateur final ne crée pas de morceau */
        return n;
    }
    int n = 0, i = 0;
    while (i < len) {
        while (i < len && isspace((unsigned char)s[i])) i++;
        if (i >= len) break;
        n++;
        while (i < len && !isspace((unsigned char)s[i])) i++;
    }
    return n;
}

/* Bornes du n-ième morceau (1-based). Renvoie 0 s'il n'existe pas. */
static int chunk_span1(const char *s, ChunkType t, int n, int *b, int *e)
{
    int len = (int)strlen(s);
    if (n < 1) return 0;

    if (t == CH_CHAR) {
        if (n > len) return 0;
        *b = n - 1; *e = n; return 1;
    }
    if (t == CH_ITEM || t == CH_LINE) {
        char d = chunk_sep(t);
        int count = 1, start = 0;
        for (int i = 0; i <= len; i++) {
            if (i == len || s[i] == d) {
                if (count == n) { *b = start; *e = i; return 1; }
                count++; start = i + 1;
            }
        }
        return 0;
    }
    int count = 0, i = 0;
    while (i < len) {
        while (i < len && isspace((unsigned char)s[i])) i++;
        if (i >= len) break;
        int start = i;
        while (i < len && !isspace((unsigned char)s[i])) i++;
        if (++count == n) { *b = start; *e = i; return 1; }
    }
    return 0;
}

/* Bornes d'un intervalle a..b (b <= 0 : un seul morceau). */
static int chunk_span(const char *s, ChunkType t, int a, int b, int *st, int *en)
{
    int b1, e1, b2, e2;
    if (!chunk_span1(s, t, a, &b1, &e1)) return 0;
    if (b <= 0 || b == a) { *st = b1; *en = e1; return 1; }
    if (!chunk_span1(s, t, b, &b2, &e2)) { *st = b1; *en = (int)strlen(s); return 1; }
    *st = b1; *en = e2;
    return 1;
}

/* Décompose « [the] [ordinal] <type> [n [to m]] of <reste> ».
   Renvoie 1 si c'en est une ; `rest` pointe alors sur ce qui suit « of ». */
static int parse_chunk(const char *t, ChunkType *type,
                       char *ia, int lia, char *ib, int lib,
                       const char **rest, int *ordinal)
{
    static const char *ord[] = { "first", "second", "third", "fourth", "fifth",
                                 "sixth", "seventh", "eighth", "ninth", "tenth", NULL };
    const char *s = skip_spaces(t);
    ia[0] = ib[0] = '\0';
    *ordinal = 0;

    if (ci_word(s, "the")) s = skip_spaces(s + 3);

    for (int i = 0; ord[i]; i++)
        if (ci_word(s, ord[i])) { *ordinal = i + 1; s = skip_spaces(s + strlen(ord[i])); break; }
    if (!*ordinal) {
        if      (ci_word(s, "last"))   { *ordinal = -1; s = skip_spaces(s + 4); }
        else if (ci_word(s, "middle")) { *ordinal = -2; s = skip_spaces(s + 6); }
        else if (ci_word(s, "any"))    { *ordinal = -3; s = skip_spaces(s + 3); }
    }

    int used = 0;
    ChunkType ct = chunk_kind(s, &used);
    if (ct == CH_NONE) return 0;
    s = skip_spaces(s + used);

    const char *of = find_kw(s, "of");
    const char *in = find_kw(s, "in");
    if (in && (!of || in < of)) of = in;
    if (!of) return 0;

    if (!*ordinal) {
        /* indices explicites, éventuellement « a to b » */
        const char *to = find_kw(s, "to");
        if (to && to < of) {
            int n = (int)(to - s);
            if (n > lia - 1) n = lia - 1;
            memcpy(ia, s, (size_t)n); ia[n] = '\0';
            n = (int)(of - (to + 2));
            if (n > lib - 1) n = lib - 1;
            if (n < 0) n = 0;
            memcpy(ib, to + 2, (size_t)n); ib[n] = '\0';
        } else {
            int n = (int)(of - s);
            if (n > lia - 1) n = lia - 1;
            memcpy(ia, s, (size_t)n); ia[n] = '\0';
        }
        if (!*skip_spaces(ia)) return 0;   /* « word of x » n'a pas de sens */
    }

    *type = ct;
    *rest = of + 2;
    return 1;
}

/* Traduit ordinal/indices en bornes concrètes dans `src`. */
static void chunk_indices(const char *src, ChunkType ct, int ordinal,
                          const char *ia, const char *ib, int *a, int *b)
{
    *a = *b = 0;
    if (ordinal > 0)       { *a = ordinal; }
    else if (ordinal == -1) *a = chunk_count(src, ct);
    else if (ordinal == -2) *a = (chunk_count(src, ct) + 1) / 2;
    else if (ordinal == -3) {
        int n = chunk_count(src, ct);
        *a = n > 0 ? (rand() % n) + 1 : 0;
    } else {
        char v[128]; double d = 0;
        eval_expr(ia, v, sizeof v); as_num(v, &d); *a = (int)d;
        if (ib && *skip_spaces(ib)) {
            eval_expr(ib, v, sizeof v); as_num(v, &d); *b = (int)d;
        }
    }
}

/* Lit un morceau. Renvoie 0 si `t` n'est pas une expression de morceau. */
static int chunk_read(const char *t, char *out, int outlen)
{
    ChunkType ct; char ia[128], ib[128]; const char *rest; int ordinal;
    if (!parse_chunk(t, &ct, ia, sizeof ia, ib, sizeof ib, &rest, &ordinal)) return 0;

    char *src = arena_buf();
    eval_expr(rest, src, HC_VAL);      /* récursif : les morceaux s'emboîtent */

    int a, b, st, en;
    chunk_indices(src, ct, ordinal, ia, ib, &a, &b);
    out[0] = '\0';
    if (chunk_span(src, ct, a, b, &st, &en)) {
        int len = en - st;
        if (len > outlen - 1) len = outlen - 1;
        if (len > 0) memcpy(out, src + st, (size_t)len);
        out[len > 0 ? len : 0] = '\0';
    }
    return 1;
}


/* ==================== plages de style ====================
 *
 * Une plage couvre [start, start+len) dans le texte du champ. Ce qu'aucune
 * plage ne couvre prend le style du champ entier. Les plages sont tenues
 * triées, sans recouvrement et sans trou vide : `runs_tidy` s'en charge après
 * chaque manipulation.
 */

/* Liste active d'un champ. Un champ de fond non partagé a un style PAR CARTE,
 * comme il a un texte par carte : sinon le gras posé sur une carte se
 * retrouverait sur toutes celles du même fond. */
static int field_is_percard(Object *field);   /* défini plus bas */

static struct RunList *runs_of(Object *field)
{
    if (!field || field->type != OBJ_FIELD) return NULL;
    if (field_is_percard(field) && g_current_card) {
        Object *cd = g_current_card;
        for (int i = 0; i < cd->nbgtexts; i++)
            if (cd->bgtexts[i].field_id == field->id)
                return &cd->bgtexts[i].runs;
        return NULL;          /* la carte n'a pas encore d'entrée : rien à styler */
    }
    return &field->runs;
}

static void runs_free(struct RunList *rl)
{
    if (!rl) return;
    for (int i = 0; i < rl->n; i++) free(rl->v[i].font);
    free(rl->v);
    rl->v = NULL; rl->n = rl->cap = 0;
}

static int runs_room(struct RunList *rl, int need)
{
    if (rl->n + need <= rl->cap) return 1;
    int cap = rl->cap ? rl->cap * 2 : 8;
    while (cap < rl->n + need) cap *= 2;
    struct TextRun *v = (struct TextRun *)realloc(rl->v, (size_t)cap * sizeof *v);
    if (!v) return 0;
    rl->v = v; rl->cap = cap;
    return 1;
}

/* Une plage qui ne dit rien sur aucun des trois attributs : elle décrit
 * exactement le champ, autant ne pas la garder. Attention, `style == 0` n'est
 * PAS muet — c'est « plain », qui a le pouvoir d'effacer le gras du champ.
 * Seul HC_STYLE_INHERIT signifie « je ne me prononce pas ». */
static int run_is_mute(const struct TextRun *r)
{
    return r->style == HC_STYLE_INHERIT && r->size == 0 && !r->font;
}

/* Deux plages voisines ne se fusionnent que si elles s'accordent sur les trois
 * attributs. Comparer le seul masque de style recollait « Geneva gras » et
 * « Monaco gras » en une plage, dont la police était celle de la première. */
static int run_same_attrs(const struct TextRun *a, const struct TextRun *b)
{
    if (a->style != b->style || a->size != b->size) return 0;
    if (!a->font && !b->font) return 1;
    if (!a->font || !b->font) return 0;
    return strcmp(a->font, b->font) == 0;
}

static void runs_sort(struct RunList *rl)
{
    for (int i = 1; i < rl->n; i++) {                /* tri par insertion */
        struct TextRun t = rl->v[i];
        int k = i - 1;
        while (k >= 0 && rl->v[k].start > t.start) { rl->v[k+1] = rl->v[k]; k--; }
        rl->v[k+1] = t;
    }
}

/* Trie, jette les plages muettes, fusionne les voisines identiques. Appelé
 * après toute modification pour que la liste reste canonique — deux listes
 * équivalentes ont ainsi la même représentation. */
static void runs_tidy(struct RunList *rl)
{
    if (!rl) return;

    for (int i = 0; i < rl->n; ) {                   /* jeter les inutiles */
        if (rl->v[i].len <= 0 || run_is_mute(&rl->v[i])) {
            free(rl->v[i].font);
            for (int k = i; k + 1 < rl->n; k++) rl->v[k] = rl->v[k+1];
            rl->n--;
        } else i++;
    }
    runs_sort(rl);
    for (int i = 0; i + 1 < rl->n; ) {               /* fusionner les jointives */
        struct TextRun *a = &rl->v[i], *b = &rl->v[i+1];
        if (a->start + a->len == b->start && run_same_attrs(a, b)) {
            a->len += b->len;
            free(b->font);
            for (int k = i + 1; k + 1 < rl->n; k++) rl->v[k] = rl->v[k+1];
            rl->n--;
        } else i++;
    }
}

/* Recale les plages après une écriture : `oldlen` caractères à la position
 * `at` ont été remplacés par `newlen`. Les trois règles, vérifiées dans
 * HyperCard 2.4 :
 *   - plage entièrement recouverte  -> détruite
 *   - plage contenant `at`, ou finissant juste à `at` -> allongée
 *     (la frontière est collante : le caractère inséré hérite du style de
 *      son voisin de gauche, y compris juste après la fin d'une plage)
 *   - plage située après -> décalée de (newlen - oldlen)
 */
static void runs_edit(struct RunList *rl, int at, int oldlen, int newlen)
{
    if (!rl || rl->n == 0) return;
    int d = newlen - oldlen, end = at + oldlen;

    for (int i = 0; i < rl->n; i++) {
        struct TextRun *r = &rl->v[i];
        int rs = r->start, re = r->start + r->len;

        if (re < at)                { continue; }                 /* avant */
        if (rs >= end)              { r->start += d; continue; }  /* après */
        if (rs >= at && re <= end)  { r->len = 0; continue; }     /* recouverte */

        int keepL = (rs < at)  ? at - rs  : 0;                    /* survit à gauche */
        int keepR = (re > end) ? re - end : 0;                    /* survit à droite */

        if (keepL && !keepR)      { r->len = keepL + newlen; }    /* collante */
        else if (keepR && !keepL) { r->start = at + newlen; r->len = keepR; }
        else                      { r->len = keepL + newlen + keepR; }
    }
    runs_tidy(rl);
}

/* Quel(s) attribut(s) une écriture concerne. Les trois sont indépendants :
 * poser un style ne doit pas emporter la police avec lui. */
#define RA_STYLE 1
#define RA_SIZE  2
#define RA_FONT  4

static void run_apply(struct TextRun *r, int mask,
                      int style, int size, const char *font)
{
    if (mask & RA_STYLE) r->style = style;
    if (mask & RA_SIZE)  r->size  = size;
    if (mask & RA_FONT) {
        free(r->font);
        r->font = (font && *font) ? dupstr(font) : NULL;
    }
}

/* Coupe en deux la plage qui enjambe `pos`, s'il y en a une. Après un appel
 * en `start` puis en `end`, plus aucune plage ne chevauche la frontière :
 * chacune est entièrement dedans ou entièrement dehors. */
static int runs_split_at(struct RunList *rl, int pos)
{
    for (int i = 0; i < rl->n; i++) {
        struct TextRun *r = &rl->v[i];
        if (pos <= r->start || pos >= r->start + r->len) continue;

        if (!runs_room(rl, 1)) return 0;
        r = &rl->v[i];                            /* realloc a pu tout déplacer */
        struct TextRun tail = *r;
        tail.font  = r->font ? dupstr(r->font) : NULL;
        tail.start = pos;
        tail.len   = r->start + r->len - pos;
        r->len     = pos - r->start;
        rl->v[rl->n++] = tail;
        return 1;                                 /* les plages ne se recouvrent
                                                   * pas : une seule enjambe */
    }
    return 1;
}

/* Pose un attribut sur [start, start+len) SANS toucher aux deux autres.
 *
 * L'ancienne version rasait toute plage recouverte pour en poser une neuve :
 * « set the textStyle of word 3 to bold » effaçait donc la police de ce mot.
 * On procède maintenant en trois temps : découper aux frontières, combler les
 * trous par des plages muettes pour que l'intervalle soit intégralement
 * couvert, puis n'écrire que l'attribut demandé sur chaque plage concernée. */
static int runs_set_attr(struct RunList *rl, int start, int len, int mask,
                         int style, int size, const char *font)
{
    if (!rl || len <= 0 || start < 0) return 0;
    int end = start + len;

    if (!runs_split_at(rl, start)) return 0;
    if (!runs_split_at(rl, end))   return 0;
    runs_sort(rl);

    /* Combler : tout caractère de l'intervalle doit appartenir à une plage,
     * sinon l'attribut n'aurait nulle part où s'écrire. Les plages ajoutées
     * sont muettes — elles décrivent le champ — jusqu'à ce qu'on écrive
     * dedans juste après. */
    int cursor = start, n0 = rl->n;
    for (int i = 0; i < n0 && cursor < end; i++) {
        int rs = rl->v[i].start, re = rs + rl->v[i].len;
        if (re <= start) continue;
        if (rs >= end)   break;
        if (rs > cursor) {
            if (!runs_room(rl, 1)) return 0;
            struct TextRun g = { cursor, rs - cursor, HC_STYLE_INHERIT, 0, NULL };
            rl->v[rl->n++] = g;
        }
        if (re > cursor) cursor = re;
    }
    if (cursor < end) {
        if (!runs_room(rl, 1)) return 0;
        struct TextRun g = { cursor, end - cursor, HC_STYLE_INHERIT, 0, NULL };
        rl->v[rl->n++] = g;
    }

    for (int i = 0; i < rl->n; i++) {
        struct TextRun *r = &rl->v[i];
        if (r->start >= start && r->start + r->len <= end && r->len > 0)
            run_apply(r, mask, style, size, font);
    }

    runs_tidy(rl);
    return 1;
}

/* Style effectif de [start, start+len), sachant que ce qu'aucune plage ne
 * couvre vaut `dflt`. Renvoie HC_STYLE_MIXED si la plage n'est pas homogène —
 * c'est ce que le guide d'Apple appelle « mixed ». */
static int runs_get_style(struct RunList *rl, int start, int len, int dflt)
{
    if (len <= 0) return dflt;
    int first = -2;
    for (int c = start; c < start + len; c++) {
        int st = dflt;
        if (rl) {
            for (int i = 0; i < rl->n; i++)
                if (c >= rl->v[i].start && c < rl->v[i].start + rl->v[i].len) {
                    if (rl->v[i].style != HC_STYLE_INHERIT) st = rl->v[i].style;
                    break;
                }
        }
        if (first == -2) first = st;
        else if (st != first) return HC_STYLE_MIXED;
    }
    return first == -2 ? dflt : first;
}

/* Police effective de [start, start+len). Écrit « mixed » si la plage n'est
 * pas homogène, comme le fait la lecture du style. `dflt` est la police du
 * champ, qui s'applique partout où aucune plage ne se prononce. */
static void runs_get_font(struct RunList *rl, int start, int len,
                          const char *dflt, char *out, int outlen)
{
    if (!dflt) dflt = "";
    const char *first = NULL;
    for (int c = start; c < start + len; c++) {
        const char *fn = dflt;
        if (rl)
            for (int i = 0; i < rl->n; i++)
                if (c >= rl->v[i].start && c < rl->v[i].start + rl->v[i].len) {
                    if (rl->v[i].font) fn = rl->v[i].font;
                    break;
                }
        if (!first) first = fn;
        else if (strcmp(fn, first) != 0) { snprintf(out, outlen, "mixed"); return; }
    }
    snprintf(out, outlen, "%s", first ? first : dflt);
}

/* Corps effectif de [start, start+len). Renvoie -1 pour « mixed » : zéro est
 * déjà pris par « le champ n'a pas de taille explicite ». */
static int runs_get_size(struct RunList *rl, int start, int len, int dflt)
{
    int first = -2;
    for (int c = start; c < start + len; c++) {
        int sz = dflt;
        if (rl)
            for (int i = 0; i < rl->n; i++)
                if (c >= rl->v[i].start && c < rl->v[i].start + rl->v[i].len) {
                    if (rl->v[i].size) sz = rl->v[i].size;
                    break;
                }
        if (first == -2) first = sz;
        else if (sz != first) return -1;
    }
    return first == -2 ? dflt : first;
}

/* Un mot -> son bit. 0 si le mot n'est pas un nom de style (« plain » compris :
 * il ne vaut aucun bit, mais reste un nom légitime — voir style_is_names). */
static int style_bit_of_name(const char *w)
{
    if      (ci_equal(w, "bold"))      return HC_BOLD;
    else if (ci_equal(w, "italic"))    return HC_ITALIC;
    else if (ci_equal(w, "underline")) return HC_UNDERLINE;
    else if (ci_equal(w, "outline"))   return HC_OUTLINE;
    else if (ci_equal(w, "shadow"))    return HC_SHADOW;
    else if (ci_equal(w, "condensed") || ci_equal(w, "condense")
                                      || ci_equal(w, "condens")) return HC_CONDENSE;
    else if (ci_equal(w, "extend") || ci_equal(w, "extended")) return HC_EXTEND;
    else if (ci_equal(w, "group"))     return HC_GROUP;
    return 0;
}

/* « bold,condense » -> bits. `plain` n'est pas un bit mais l'absence de bits,
 * et il est écrasé par tout ce qui l'accompagne, comme le veut le guide.
 * Le guide écrit « condensed », le menu du Mac « Condense » et le script du
 * Calendrier « condense » : les trois sont acceptés. */
static int style_from_names(const char *s)
{
    int bits = 0;
    while (s && *s) {
        while (*s == ' ' || *s == '\t' || *s == ',' || *s == '"') s++;
        if (!*s) break;
        char w[32]; int k = 0;
        while (*s && *s != ',' && *s != ' ' && *s != '\t' && *s != '"'
               && k < (int)sizeof w - 1) w[k++] = *s++;
        w[k] = '\0';
        bits |= style_bit_of_name(w);
        /* « plain » et les mots inconnus n'ajoutent rien */
    }
    return bits;
}

/* Vrai si TOUS les mots de `s` nomment un style. C'est ce qui sépare la liste
 * de noms, qu'HyperCard écrit sans guillemets — « to bold,underline » — d'une
 * expression à évaluer — « to s & ",italic" ». Les guillemets ne sont pas des
 * séparateurs ici : une liste citée est traitée un cran plus haut. */
static int style_is_names(const char *s)
{
    int words = 0;
    while (s && *s) {
        while (*s == ' ' || *s == '\t' || *s == ',') s++;
        if (!*s) break;
        char w[32]; int k = 0;
        while (*s && *s != ',' && *s != ' ' && *s != '\t') {
            if (k < (int)sizeof w - 1) w[k++] = *s;
            s++;
        }
        w[k] = '\0';
        if (!style_bit_of_name(w) && !ci_equal(w, "plain")) return 0;
        words++;
    }
    return words > 0;
}

static void style_to_names(int bits, char *out, int outlen)
{
    static const struct { int b; const char *n; } T[] = {
        { HC_BOLD, "bold" }, { HC_ITALIC, "italic" }, { HC_UNDERLINE, "underline" },
        { HC_OUTLINE, "outline" }, { HC_SHADOW, "shadow" },
        { HC_CONDENSE, "condense" }, { HC_EXTEND, "extend" }, { HC_GROUP, "group" }
    };
    if (bits == HC_STYLE_MIXED) { snprintf(out, outlen, "mixed"); return; }
    int pos = 0;
    out[0] = '\0';
    for (int i = 0; i < (int)(sizeof T / sizeof T[0]); i++)
        if (bits & T[i].b)
            pos += snprintf(out + pos, outlen - pos, "%s%s", pos ? "," : "", T[i].n);
    if (!pos) snprintf(out, outlen, "plain");
}

/* Résout « <morceaux> of <champ> » en un intervalle ABSOLU de caractères dans
 * le texte du champ. container_set travaille en relatif à chaque niveau de sa
 * récursion, ce qui suffit pour écrire du texte mais pas pour situer une
 * plage de style. On refait donc la descente en cumulant les décalages.
 * Renvoie NULL si `ref` n'est pas un morceau de champ. */
static Object *chunk_target(const char *ref, int *st, int *en)
{
    ChunkType ct; char ia[128], ib[128]; const char *rest; int ordinal;
    if (!parse_chunk(ref, &ct, ia, sizeof ia, ib, sizeof ib, &rest, &ordinal))
        return NULL;

    Object *fld;
    int base_off = 0;
    char *base = arena_buf();

    int inner_st, inner_en;
    Object *inner = chunk_target(rest, &inner_st, &inner_en);
    if (inner) {                                   /* morceau de morceau */
        fld = inner;
        base_off = inner_st;
        int len = inner_en - inner_st;
        if (len < 0) len = 0;
        snprintf(base, HC_VAL, "%.*s", len, hc_field_text(fld) + inner_st);
    } else {
        fld = resolve(rest);
        if (!fld || fld->type != OBJ_FIELD) return NULL;
        snprintf(base, HC_VAL, "%s", hc_field_text(fld));
    }

    int a, b, s2, e2;
    chunk_indices(base, ct, ordinal, ia, ib, &a, &b);
    if (!chunk_span(base, ct, a, b, &s2, &e2)) return NULL;
    *st = base_off + s2;
    *en = base_off + e2;
    return fld;
}

/* Intervalle de la dernière écriture, posé par container_set et consommé par
 * hc_set_field_text : c'est le seul moyen de savoir si l'écriture portait sur
 * un morceau (les plages se recalent) ou sur le champ entier (elles meurent). */
static Object *g_edit_fld = NULL;
static int     g_edit_at = -1, g_edit_old = 0, g_edit_new = 0;

/* Écrit dans un conteneur : champ, variable, ou morceau de l'un des deux.
 * mode : 0 remplacer, 1 après, 2 avant, 3 supprimer.
 * L'appel est récursif, donc « word 2 of line 3 of field "notes" » marche.
 * Renvoie 1 si la destination a été reconnue.
 */
static int container_set_body(const char *ref, const char *val, int mode);

/* container_set est recursif : « char 2 of word 2 of me » se traite en trois
 * passes emboitees. Seule la PLUS EXTERNE connait l'intervalle que l'ecriture
 * vise reellement ; les suivantes voient un remplacement complet du morceau
 * englobant. Sans ce garde-fou, la note d'intervalle etait ecrasee par le
 * niveau interne et les plages de style se croyaient recouvertes. */
static int g_cset_depth = 0;

static int container_set(const char *ref, const char *val, int mode)
{
    if (g_cset_depth == 0) { g_edit_fld = NULL; g_edit_at = -1; }
    g_cset_depth++;
    int r = container_set_body(ref, val, mode);
    g_cset_depth--;
    return r;
}

static int container_set_body(const char *ref, const char *val, int mode)
{
    ChunkType ct; char ia[128], ib[128]; const char *rest; int ordinal;

    if (parse_chunk(ref, &ct, ia, sizeof ia, ib, sizeof ib, &rest, &ordinal)) {
        /* Noter l'intervalle absolu visé dans le champ, pour que les plages de
         * style puissent se recaler. Sans cette note, hc_set_field_text ne voit
         * qu'un texte entier remplacé et détruit tout le style. */
        if (g_cset_depth == 1) {
            int cst, cen;
            Object *cf = chunk_target(ref, &cst, &cen);
            if (cf) {
                g_edit_fld = cf;
                int vlen = (int)strlen(val);
                if      (mode == 1) { g_edit_at = cen; g_edit_old = 0; g_edit_new = vlen; }
                else if (mode == 2) { g_edit_at = cst; g_edit_old = 0; g_edit_new = vlen; }
                else if (mode == 3) {
                    /* Supprimer un morceau emporte aussi son separateur —
                     * « delete word 1 » retire « Sun » ET l'espace qui suit.
                     * Meme ajustement que plus bas, sinon les plages qui
                     * suivent se decalent d'un caractere de trop. */
                    char sep = chunk_sep(ct);
                    const char *ft = hc_field_text(cf);
                    int fl = (int)strlen(ft);
                    if (sep) {
                        if      (cen < fl && ft[cen] == sep)  cen++;
                        else if (cst > 0  && ft[cst-1] == sep) cst--;
                    }
                    g_edit_at = cst; g_edit_old = cen - cst; g_edit_new = 0;
                }
                else                { g_edit_at = cst; g_edit_old = cen - cst; g_edit_new = vlen; }
            }
        }
        char *base = arena_buf();
        eval_expr(rest, base, HC_VAL);

        int a, b, st, en;
        chunk_indices(base, ct, ordinal, ia, ib, &a, &b);

        char sepstr[2] = { chunk_sep(ct), '\0' };
        char *neuf = arena_buf();

        if (!chunk_span(base, ct, a, b, &st, &en)) {
            if (mode == 3) return 1;            /* rien à supprimer */
            /* Le rang visé dépasse le contenu : compléter avec des éléments
             * vides jusqu'à ce rang, comme le fait HyperTalk. */
            if (sepstr[0] && a > 0) {
                int have = 0;
                if (*base) {
                    have = 1;
                    for (const char *q = base; *q; q++)
                        if (*q == sepstr[0]) have++;
                }
                int need = (have == 0) ? (a - 1) : (a - have);
                int pos = 0;
                pos += snprintf(neuf + pos, HC_VAL - pos, "%s", base);
                for (int k = 0; k < need && pos < (int)HC_VAL - 2; k++)
                    neuf[pos++] = sepstr[0];
                neuf[pos] = '\0';
                snprintf(neuf + pos, HC_VAL - pos, "%s", val);
            } else {
                snprintf(neuf, HC_VAL, "%s%s%s", base,
                         (*base && sepstr[0]) ? sepstr : "", val);
            }
        } else {
            char *old = arena_buf();
            int len = en - st;
            if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
            if (len < 0) len = 0;
            memcpy(old, base + st, (size_t)len); old[len] = '\0';

            char *piece = arena_buf();
            if      (mode == 1) snprintf(piece, HC_VAL, "%s%s", old, val);
            else if (mode == 2) snprintf(piece, HC_VAL, "%s%s", val, old);
            else if (mode == 3) piece[0] = '\0';
            else                snprintf(piece, HC_VAL, "%s", val);

            if (mode == 3 && sepstr[0]) {       /* supprimer emporte un séparateur */
                int bl = (int)strlen(base);
                if      (en < bl && base[en] == sepstr[0]) en++;
                else if (st > 0  && base[st-1] == sepstr[0]) st--;
            }
            snprintf(neuf, HC_VAL, "%.*s%s%s", st, base, piece, base + en);
        }
        return container_set(rest, neuf, 0);
    }

    char *merged = arena_buf();
    Object *o = resolve(ref);
    if (o && o->type == OBJ_FIELD) {
        /* passer par hc_field_text / hc_set_field_text : un champ de fond non
         * partagé a un texte propre à chaque carte */
        const char *old = hc_field_text(o);
        if      (mode == 1) snprintf(merged, HC_VAL, "%s%s", old, val);
        else if (mode == 2) snprintf(merged, HC_VAL, "%s%s", val, old);
        else if (mode == 3) merged[0] = '\0';
        else                snprintf(merged, HC_VAL, "%s", val);
        hc_set_field_text(o, merged);
        notify_field(o);
        return 1;
    }
    if (o) return 0;                            /* un bouton n'est pas un conteneur */

    char vname[128];
    const char *after = next_word(ref, vname, sizeof vname);
    if (vname[0] && vname[0] != '"' && !*skip_spaces(after)) {
        const char *old = var_get(vname);
        if (!old) old = "";
        if      (mode == 1) snprintf(merged, HC_VAL, "%s%s", old, val);
        else if (mode == 2) snprintf(merged, HC_VAL, "%s%s", val, old);
        else if (mode == 3) merged[0] = '\0';
        else                snprintf(merged, HC_VAL, "%s", val);
        var_set(vname, merged);
        return 1;
    }
    return 0;
}

/* ==================== fonctions intégrées ==================== */

/* Deux syntaxes, comme dans HyperTalk :
 *     the <fonction>              the date, the ticks
 *     the <fonction> of <expr>    the length of x
 *     <fonction>(<args>)          min(3,1,2), offset("b","abc")
 */

/* Découpe les arguments d'un appel : virgules de premier niveau seulement,
   les parenthèses et les guillemets protègent. */
static int split_args(const char *s, char args[][HC_VAL], int maxargs)
{
    int n = 0, depth = 0, inq = 0, len = 0;
    args[0][0] = '\0';
    for (const char *p = s; *p; p++) {
        if (*p == '"') inq = !inq;
        else if (!inq && *p == '(') depth++;
        else if (!inq && *p == ')') depth--;

        if (!inq && depth == 0 && *p == ',') {
            args[n][len] = '\0';
            if (++n >= maxargs) return n;
            len = 0; args[n][0] = '\0';
            continue;
        }
        if (len < 511) args[n][len++] = *p;
    }
    args[n][len] = '\0';
    if (*skip_spaces(args[n])) n++;
    return n;
}

static void format_date(char *out, int outlen, int mode)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (!tm) { out[0] = '\0'; return; }
    const char *fmt = "%m/%d/%y";
    if      (mode == 1) fmt = "%a, %b %d, %Y";     /* abbreviated date */
    else if (mode == 2) fmt = "%A, %B %d, %Y";     /* long date */
    else if (mode == 3) fmt = "%H:%M";             /* time */
    else if (mode == 4) fmt = "%H:%M:%S";          /* long time */
    strftime(out, (size_t)outlen, fmt, tm);
}

/* ==================== dates : analyse et mise en forme ==================== */

/* Tables en anglais, volontairement : HyperTalk n'est pas localisé, et un
 * script de 1990 compare ses résultats à « January » ou « Sun ». Passer par
 * strftime ferait dépendre le sens du script de la locale de la machine. */
static const char *k_month[12] = {
    "January","February","March","April","May","June",
    "July","August","September","October","November","December" };
static const char *k_day[7] = {
    "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday" };

/* Secondes du Macintosh : depuis le 1er janvier 1904, pas 1970. */
#define HC_MAC_EPOCH 2082844800LL

enum { DF_SECONDS, DF_DATEITEMS, DF_SHORTDATE, DF_LONGDATE,
       DF_ABBREVDATE, DF_SHORTTIME, DF_LONGTIME, DF_NONE };

static int name_index(const char *w, const char **tab, int n)
{
    for (int i = 0; i < n; i++) {
        if (ci_equal(w, tab[i])) return i;
        /* forme abrégée : les trois premières lettres suffisent, comme
         * « Aug » pour August ou « Wed » pour Wednesday. */
        if (strlen(w) == 3 && ci_nequal(w, tab[i], 3)) return i;
    }
    return -1;
}

/* Analyse une date, une heure, une liste dateItems ou des secondes.
 * Renvoie 1 si la chaîne a été comprise, et remplit *tm (normalisé par
 * mktime, donc avec le jour de la semaine correct).
 *
 * Le séparateur lève l'ambiguïté, comme chez Apple : les dateItems sont
 * toujours séparés par des virgules, la date courte toujours par des
 * barres obliques. « 2026,8,7 » est donc une année-mois-jour, tandis que
 * « 8/7/26 » est un mois-jour-année. */
static int parse_datetime(const char *s, struct tm *tm)
{
    if (!s) return 0;

    int nums[12], nn = 0;
    int mon = -1, hh = -1, mi = 0, ss = 0, meridian = 0;  /* 1 = AM, 2 = PM */
    int sawslash = 0, sawcolon = 0, sawname = 0;

    const char *p = s;
    while (*p) {
        while (*p && !isalnum((unsigned char)*p)) {
            if (*p == '/') sawslash = 1;
            p++;
        }
        if (!*p) break;

        if (isdigit((unsigned char)*p)) {
            int v = 0;
            while (isdigit((unsigned char)*p)) { v = v * 10 + (*p - '0'); p++; }
            if (*p == ':') {                       /* début d'une heure */
                sawcolon = 1;
                hh = v; p++;
                mi = 0;
                while (isdigit((unsigned char)*p)) { mi = mi * 10 + (*p - '0'); p++; }
                if (*p == ':') {
                    p++; ss = 0;
                    while (isdigit((unsigned char)*p)) { ss = ss * 10 + (*p - '0'); p++; }
                }
            } else if (nn < 12) {
                nums[nn++] = v;
            }
        } else {
            char w[32]; int k = 0;
            while (isalpha((unsigned char)*p) && k < (int)sizeof w - 1) w[k++] = *p++;
            w[k] = '\0';
            if (ci_equal(w, "AM")) meridian = 1;
            else if (ci_equal(w, "PM")) meridian = 2;
            else {
                int m = name_index(w, k_month, 12);
                if (m >= 0) { mon = m; sawname = 1; }
                else if (name_index(w, k_day, 7) >= 0) sawname = 1;  /* jour : ignoré */
            }
        }
    }

    memset(tm, 0, sizeof *tm);
    tm->tm_isdst = -1;

    /* --- secondes du Macintosh : un seul nombre, et il est énorme --- */
    if (!sawslash && !sawcolon && !sawname && nn == 1 && nums[0] > 100000) {
        time_t t = (time_t)((long long)nums[0] - HC_MAC_EPOCH);
        struct tm *lt = localtime(&t);
        if (!lt) return 0;
        *tm = *lt;
        return 1;
    }

    int year = -1, day = -1;

    if (sawname) {                       /* « Friday, August 7, 2026 » */
        if (nn >= 1) day  = nums[0];
        if (nn >= 2) year = nums[1];
        if (mon < 0) return 0;           /* un nom de jour seul n'est pas une date */
        if (day < 0) day = 1;            /* « August » = le 1er août */
    } else if (sawslash) {               /* « 8/7/26 » : mois, jour, année */
        if (nn >= 1) mon  = nums[0] - 1;
        if (nn >= 2) day  = nums[1];
        if (nn >= 3) year = nums[2];
    } else if (nn >= 3) {                /* dateItems : y, m, d, h, mn, s, dow */
        year = nums[0]; mon = nums[1] - 1; day = nums[2];
        if (nn >= 4) hh = nums[3];
        if (nn >= 5) mi = nums[4];
        if (nn >= 6) ss = nums[5];
        /* nums[6] est le jour de la semaine : recalculé, jamais lu */
    } else if (sawcolon) {               /* heure seule : on garde aujourd'hui */
        time_t now = time(NULL);
        struct tm *lt = localtime(&now);
        if (!lt) return 0;
        year = lt->tm_year + 1900; mon = lt->tm_mon; day = lt->tm_mday;
    } else {
        return 0;
    }

    if (mon < 0 || day < 0) return 0;
    if (year < 0) {                      /* année tue : celle en cours */
        time_t now = time(NULL);
        struct tm *lt = localtime(&now);
        if (!lt) return 0;
        year = lt->tm_year + 1900;
    }
    if (year < 100) year += (year < 70) ? 2000 : 1900;   /* comme le Macintosh */

    if (hh < 0) hh = 0;
    if (meridian == 2 && hh < 12) hh += 12;              /* 3 PM → 15 */
    if (meridian == 1 && hh == 12) hh = 0;               /* 12 AM → 0 */

    tm->tm_year = year - 1900;
    tm->tm_mon  = mon;
    tm->tm_mday = day;
    tm->tm_hour = hh;
    tm->tm_min  = mi;
    tm->tm_sec  = ss;

    /* mktime normalise et remplit tm_wday. C'est lui qui fait marcher
     * « put 1 into item 3 of d » suivi de « convert d to dateItems » :
     * un 31 février devient un 3 mars, et le jour de la semaine suit. */
    if (mktime(tm) == (time_t)-1) return 0;
    return 1;
}

/* Nom de format → constante DF_*. Renvoie DF_NONE si le mot est inconnu. */
static int date_format_code(const char *spec)
{
    const char *s = skip_spaces(spec);
    if (ci_word(s, "the")) s = skip_spaces(s + 3);

    int wantlong = 0, wantabbr = 0;
    for (;;) {
        if (ci_word(s, "long"))        { wantlong = 1; s = skip_spaces(s + 4); }
        else if (ci_word(s, "short"))  {               s = skip_spaces(s + 5); }
        else if (ci_word(s, "abbreviated")) { wantabbr = 1; s = skip_spaces(s + 11); }
        else if (ci_word(s, "abbrev")) { wantabbr = 1; s = skip_spaces(s + 6); }
        else if (ci_word(s, "abbr"))   { wantabbr = 1; s = skip_spaces(s + 4); }
        else if (ci_word(s, "english")){               s = skip_spaces(s + 7); }
        else break;
    }
    if (ci_word(s, "dateitems")) return DF_DATEITEMS;
    if (ci_word(s, "seconds") || ci_word(s, "secs")) return DF_SECONDS;
    if (ci_word(s, "date"))
        return wantlong ? DF_LONGDATE : wantabbr ? DF_ABBREVDATE : DF_SHORTDATE;
    if (ci_word(s, "time"))
        return wantlong ? DF_LONGTIME : DF_SHORTTIME;
    return DF_NONE;
}

static void emit_datetime(struct tm *tm, int fmt, char *out, int outlen)
{
    int h12 = tm->tm_hour % 12; if (h12 == 0) h12 = 12;
    const char *ampm = tm->tm_hour < 12 ? "AM" : "PM";

    switch (fmt) {
    case DF_DATEITEMS:
        snprintf(out, outlen, "%d,%d,%d,%d,%d,%d,%d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday + 1);
        break;
    case DF_SHORTDATE:
        snprintf(out, outlen, "%d/%d/%02d",
                 tm->tm_mon + 1, tm->tm_mday, (tm->tm_year + 1900) % 100);
        break;
    case DF_LONGDATE:
        snprintf(out, outlen, "%s, %s %d, %d", k_day[tm->tm_wday],
                 k_month[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
        break;
    case DF_ABBREVDATE:
        snprintf(out, outlen, "%.3s, %.3s %d, %d", k_day[tm->tm_wday],
                 k_month[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
        break;
    case DF_SHORTTIME:
        snprintf(out, outlen, "%d:%02d %s", h12, tm->tm_min, ampm);
        break;
    case DF_LONGTIME:
        snprintf(out, outlen, "%d:%02d:%02d %s", h12, tm->tm_min, tm->tm_sec, ampm);
        break;
    case DF_SECONDS: {
        struct tm copy = *tm;
        snprintf(out, outlen, "%lld", (long long)mktime(&copy) + HC_MAC_EPOCH);
        break;
    }
    default:
        out[0] = '\0';
    }
}

/* Renvoie 1 si `t` était bien un appel de fonction. */
static int call_function_body(const char *t, char *out, int outlen)
{
    const char *s = skip_spaces(t);
    if (ci_word(s, "the")) s = skip_spaces(s + 3);

    /* formes composées : long date, short time, abbreviated date… */
    int datemode = -1;
    if (ci_word(s, "long")) {
        const char *w = skip_spaces(s + 4);
        if (ci_word(w, "date")) datemode = 2; else if (ci_word(w, "time")) datemode = 4;
    } else if (ci_word(s, "short")) {
        const char *w = skip_spaces(s + 5);
        if (ci_word(w, "date")) datemode = 0; else if (ci_word(w, "time")) datemode = 3;
    } else if (ci_word(s, "abbreviated") || ci_word(s, "abbrev") || ci_word(s, "abbr")) {
        const char *w = strchr(s, ' ');
        if (w && ci_word(skip_spaces(w), "date")) datemode = 1;
    }
    if (datemode >= 0) { format_date(out, outlen, datemode); return 1; }

    char name[64];
    const char *after = next_word(s, name, sizeof name);
    if (!name[0]) return 0;

    /* --- sans argument --- */
    if (!*skip_spaces(after)) {
        if (ci_equal(name, "date")) { format_date(out, outlen, 0); return 1; }
        if (ci_equal(name, "result")) { snprintf(out, outlen, "%s", g_result); return 1; }
        if (ci_equal(name, "foundtext")) { snprintf(out, outlen, "%s", g_found_text); return 1; }
        if (ci_equal(name, "foundfield")) {
            if (g_found_field) hc_describe(g_found_field, out, outlen);
            else snprintf(out, outlen, "%s", "");
            return 1;
        }
        if (ci_equal(name, "foundline")) {
            if (g_found_field && g_found_line > 0) {
                char d[96]; hc_describe(g_found_field, d, sizeof d);
                snprintf(out, outlen, "line %d of %s", g_found_line, d);
            } else snprintf(out, outlen, "%s", "");
            return 1;
        }
        if (ci_equal(name, "paramcount")) { snprintf(out, outlen, "%d", g_nparams - 1); return 1; }
        if (ci_equal(name, "params")) {
            /* tous les paramètres, nom du message inclus, séparés par des virgules */
            out[0] = '\0';
            int pos = 0;
            for (int i = 0; i < g_nparams; i++)
                pos += snprintf(out + pos, outlen - pos, "%s%s", i ? "," : "", g_params[i]);
            return 1;
        }
        if (ci_equal(name, "time")) { format_date(out, outlen, 3); return 1; }
        if (ci_equal(name, "seconds") || ci_equal(name, "secs")) {
            /* comme sur Macintosh : secondes depuis le 1er janvier 1904 */
            snprintf(out, outlen, "%lld", (long long)time(NULL) + 2082844800LL);
            return 1;
        }
        if (ci_equal(name, "ticks")) {
            snprintf(out, outlen, "%lld", (long long)(clock() * 60 / CLOCKS_PER_SEC));
            return 1;
        }
    }

    /* --- arguments : « of <expr> » ou « (a, b, c) » --- */
    char (*raw)[HC_VAL]  = arena_rows(8);
    char (*vals)[HC_VAL] = arena_rows(8);
    int nargs = 0;
    const char *q = skip_spaces(after);

    if (*q == '(') {
        const char *end = q + 1;
        int depth = 1, inq = 0;
        while (*end && depth) {
            if (*end == '"') inq = !inq;
            else if (!inq && *end == '(') depth++;
            else if (!inq && *end == ')') depth--;
            if (depth) end++;
        }
        char *inner = arena_buf();
        int len = (int)(end - (q + 1));
        if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
        if (len < 0) len = 0;
        memcpy(inner, q + 1, (size_t)len); inner[len] = '\0';
        nargs = split_args(inner, raw, 8);
    } else if (ci_word(q, "of")) {
        snprintf(raw[0], sizeof raw[0], "%s", q + 2);
        nargs = 1;
    } else {
        return 0;
    }

    for (int i = 0; i < nargs; i++) eval_expr(raw[i], vals[i], sizeof vals[i]);

    double a = 0, b = 0;
    if (nargs > 0) as_num(vals[0], &a);
    if (nargs > 1) as_num(vals[1], &b);
    (void)b;

    /* --- une entrée --- */
    if (ci_equal(name, "length")) { snprintf(out, outlen, "%d", (int)strlen(vals[0])); return 1; }
    if (ci_equal(name, "abs"))    { put_num(a < 0 ? -a : a, out, outlen); return 1; }
    if (ci_equal(name, "trunc"))  { put_num((double)(long long)a, out, outlen); return 1; }
    if (ci_equal(name, "round"))  { put_num(a < 0 ? -(double)(long long)(-a + 0.5)
                                                  :  (double)(long long)( a + 0.5), out, outlen); return 1; }
    if (ci_equal(name, "sqrt"))   { put_num(a >= 0 ? sqrt(a) : 0, out, outlen); return 1; }
    if (ci_equal(name, "exp"))    { put_num(exp(a), out, outlen); return 1; }
    if (ci_equal(name, "ln"))     { put_num(a > 0 ? log(a) : 0, out, outlen); return 1; }
    if (ci_equal(name, "log2"))   { put_num(a > 0 ? log(a) / log(2.0) : 0, out, outlen); return 1; }
    if (ci_equal(name, "sin"))    { put_num(sin(a), out, outlen); return 1; }
    if (ci_equal(name, "cos"))    { put_num(cos(a), out, outlen); return 1; }
    if (ci_equal(name, "tan"))    { put_num(tan(a), out, outlen); return 1; }
    if (ci_equal(name, "atan"))   { put_num(atan(a), out, outlen); return 1; }
    if (ci_equal(name, "random")) {
        static int seeded = 0;
        if (!seeded) { srand((unsigned)time(NULL)); seeded = 1; }
        int n = (int)a;
        snprintf(out, outlen, "%d", n > 0 ? (rand() % n) + 1 : 0); return 1;
    }
    if (ci_equal(name, "charToNum")) { snprintf(out, outlen, "%d", (unsigned char)vals[0][0]); return 1; }
    if (ci_equal(name, "numToChar")) { snprintf(out, outlen, "%c", (int)a); return 1; }

    /* value() : évalue une chaîne comme une expression. Le petit vertige
       d'HyperTalk — du texte qui redevient du calcul. */
    if (ci_equal(name, "value")) { eval_expr(vals[0], out, outlen); return 1; }

    /* param(n) : le n-ième paramètre. param(0) est le nom du message. */
    if (ci_equal(name, "param")) {
        int i = (int)a;
        snprintf(out, outlen, "%s", (i >= 0 && i < g_nparams) ? g_params[i] : "");
        return 1;
    }

    /* --- deux entrées --- */
    if (ci_equal(name, "offset")) {
        int pos = 0, nl = (int)strlen(vals[0]), hl = (int)strlen(vals[1]);
        for (int i = 0; nl && i + nl <= hl; i++)
            if (ci_nequal(vals[1] + i, vals[0], nl)) { pos = i + 1; break; }
        snprintf(out, outlen, "%d", pos);
        return 1;
    }

    /* --- nombre variable d'entrées --- */
    if (ci_equal(name, "min") || ci_equal(name, "max") ||
        ci_equal(name, "sum") || ci_equal(name, "average") || ci_equal(name, "avg")) {
        if (nargs == 0) { snprintf(out, outlen, "0"); return 1; }
        int wantmin = ci_equal(name, "min");
        double acc = 0, best = 0;
        for (int i = 0; i < nargs; i++) {
            double v = 0;
            as_num(vals[i], &v);
            acc += v;
            if (i == 0 || (wantmin ? v < best : v > best)) best = v;
        }
        if      (ci_equal(name, "sum")) put_num(acc, out, outlen);
        else if (ci_equal(name, "min") || ci_equal(name, "max")) put_num(best, out, outlen);
        else                            put_num(acc / nargs, out, outlen);
        return 1;
    }

    /* --- fonction définie par l'utilisateur ---
     * Dernier recours, après tous les noms intégrés : une pile qui définit
     * « function length » ne doit pas masquer celle du noyau, comme dans
     * HyperCard. La recherche part de l'objet dont le script tourne et
     * remonte la chaîne carte → fond → pile. */
    {
        Object *from = g_me ? g_me : g_current_card;
        char (*uargv)[HC_VAL] = arena_rows(8);
        for (int i = 0; i < nargs; i++)
            snprintf(uargv[i], sizeof uargv[i], "%s", vals[i]);
        if (hc_call_user_function(from, name, uargv, nargs)) {
            snprintf(out, outlen, "%s", g_result);
            return 1;
        }
    }

    return 0;
}

static int call_function(const char *t, char *out, int outlen)
{
    ARENA_MARK;
    int r = call_function_body(t, out, outlen);
    ARENA_FREE;
    return r;
}

/* ==================== propriétés géométriques ==================== */

/* La pile qui contient cet objet (l'objet lui-même si c'en est une). */
static Object *owning_stack(Object *o)
{
    while (o && o->type != OBJ_STACK) o = o->owner;
    return o;
}

/* Rectangle effectif. Une carte ou un fond n'a pas de géométrie propre :
 * c'est celle de sa pile, comme dans HyperCard, où « the rect of this card »
 * vaut « 0,0,largeur,hauteur ». Une pile sans taille explicite garde le
 * format d'origine du Macintosh, 512×342. */
static void obj_rect(Object *o, int *L, int *T, int *R, int *B)
{
    if (o->type == OBJ_CARD || o->type == OBJ_BACKGROUND || o->type == OBJ_STACK) {
        Object *st = owning_stack(o);
        *L = 0; *T = 0;
        *R = (st && st->w > 0) ? st->w : 512;
        *B = (st && st->h > 0) ? st->h : 342;
        return;
    }
    *L = o->x; *T = o->y; *R = o->x + o->w; *B = o->y + o->h;
}

/* Lit une propriété géométrique dans `out`. Renvoie 0 si `prop` n'en est pas. */
static int geom_read(Object *o, const char *prop, char *out, int outlen)
{
    int L, T, R, B;
    obj_rect(o, &L, &T, &R, &B);
    if (ci_equal(prop, "rect") || ci_equal(prop, "rectangle"))
        snprintf(out, outlen, "%d,%d,%d,%d", L, T, R, B);
    else if (ci_equal(prop, "topleft"))   snprintf(out, outlen, "%d,%d", L, T);
    else if (ci_equal(prop, "botright") || ci_equal(prop, "bottomright"))
                                          snprintf(out, outlen, "%d,%d", R, B);
    else if (ci_equal(prop, "left"))      snprintf(out, outlen, "%d", L);
    else if (ci_equal(prop, "top"))       snprintf(out, outlen, "%d", T);
    else if (ci_equal(prop, "right"))     snprintf(out, outlen, "%d", R);
    else if (ci_equal(prop, "bottom"))    snprintf(out, outlen, "%d", B);
    else if (ci_equal(prop, "width"))     snprintf(out, outlen, "%d", R - L);
    else if (ci_equal(prop, "height"))    snprintf(out, outlen, "%d", B - T);
    else if (ci_equal(prop, "loc") || ci_equal(prop, "location"))
                                          snprintf(out, outlen, "%d,%d", (L + R) / 2, (T + B) / 2);
    else return 0;
    return 1;
}

/* n premiers entiers d'une chaîne « a,b,c,d ». Renvoie le compte lu. */
static int parse_ints(const char *s, int *v, int maxn)
{
    int n = 0;
    while (*s && n < maxn) {
        while (*s == ' ' || *s == '\t' || *s == ',') s++;
        if (!*s) break;
        v[n++] = atoi(s);
        while (*s && *s != ',') s++;
    }
    return n;
}

/* Écrit une propriété géométrique depuis une chaîne. Renvoie 0 si pas géométrique. */
static int geom_write(Object *o, const char *prop, const char *val)
{
    int p[4];
    /* Redimensionner une carte ou un fond, c'est redimensionner la pile :
     * c'est elle qui porte la taille, et l'interface la relit de là. */
    if (o->type == OBJ_CARD || o->type == OBJ_BACKGROUND) {
        Object *st = owning_stack(o);
        if (st) o = st;
    }
    if (ci_equal(prop, "rect") || ci_equal(prop, "rectangle")) {
        if (parse_ints(val, p, 4) == 4) { o->x = p[0]; o->y = p[1]; o->w = p[2]-p[0]; o->h = p[3]-p[1]; }
    } else if (ci_equal(prop, "topleft")) {
        if (parse_ints(val, p, 2) == 2) { o->w += o->x - p[0]; o->h += o->y - p[1]; o->x = p[0]; o->y = p[1]; }
    } else if (ci_equal(prop, "botright") || ci_equal(prop, "bottomright")) {
        if (parse_ints(val, p, 2) == 2) { o->w = p[0] - o->x; o->h = p[1] - o->y; }
    } else if (ci_equal(prop, "left"))   { o->x = atoi(val); }
    else if (ci_equal(prop, "top"))      { o->y = atoi(val); }
    else if (ci_equal(prop, "right"))    { o->w = atoi(val) - o->x; }
    else if (ci_equal(prop, "bottom"))   { o->h = atoi(val) - o->y; }
    else if (ci_equal(prop, "width"))    { o->w = atoi(val); }
    else if (ci_equal(prop, "height"))   { o->h = atoi(val); }
    else if (ci_equal(prop, "loc") || ci_equal(prop, "location")) {
        if (parse_ints(val, p, 2) == 2) { o->x = p[0] - o->w/2; o->y = p[1] - o->h/2; }
    } else return 0;
    if (o->w < 0) o->w = 0;
    if (o->h < 0) o->h = 0;
    return 1;
}

/* Le mot est-il un nom de propriété ? Sert à accepter « loc of me » sans
 * « the ». La liste couvre exactement les propriétés que sait lire la
 * branche ci-dessous : y ajouter un nom sans l'ajouter là serait un piège. */
static int is_prop_name(const char *w, int len)
{
    static const char *tab[] = {
        "rect", "rectangle", "topleft", "botright", "bottomright",
        "left", "top", "right", "bottom", "width", "height",
        "loc", "location", "id", "name", "visible", "showname", "shownname",
        "icon", "selectedline", "selectedlines", "locktext", "widemargins",
        "fixedlineheight", "showlines", "autotab", "dontsearch", "sharedtext",
        "textfont", "scroll", "textstyle", "hilite", "highlight", "autohilite",
        "textsize", "textheight", "script", "text", "contents", "style", NULL
    };
    for (int i = 0; tab[i]; i++)
        if ((int)strlen(tab[i]) == len && ci_nequal(w, tab[i], len)) return 1;
    return 0;
}

/* « <propriété> of … » sans « the » devant ? */
static int prop_word_before_of(const char *t)
{
    const char *w = skip_spaces(t);
    const char *q = w;
    while (*q && !isspace((unsigned char)*q)) q++;
    if (q == w) return 0;
    if (!is_prop_name(w, (int)(q - w))) return 0;
    return ci_word(skip_spaces(q), "of");
}

static void term_value_body(const char *t, char *out, int outlen)
{
    t = skip_spaces(t);
    out[0] = '\0';
    if (!*t) return;

    if (*t == '"') { quoted(t, out, outlen); return; }

    /* --- constantes --- */
    if (ci_equal(t, "return") || ci_equal(t, "linefeed")) { snprintf(out, outlen, "\n"); return; }
    if (ci_equal(t, "space"))  { snprintf(out, outlen, " ");  return; }
    if (ci_equal(t, "tab"))    { snprintf(out, outlen, "\t"); return; }
    if (ci_equal(t, "quote"))  { snprintf(out, outlen, "\""); return; }
    if (ci_equal(t, "comma"))  { snprintf(out, outlen, ",");  return; }
    if (ci_equal(t, "empty"))  { out[0] = '\0'; return; }

    /* --- expressions de morceau : word 2 of …, the last line of … --- */
    if (chunk_read(t, out, outlen)) return;

    /* --- the number of <morceaux> in|of <expr> --- */
    if (ci_word(t, "the") || ci_word(t, "number")) {
        const char *w = ci_word(t, "the") ? skip_spaces(t + 3) : t;
        if (ci_word(w, "number")) {
            const char *k = skip_spaces(w + 6);
            if (ci_word(k, "of")) k = skip_spaces(k + 2);

            if (ci_word(k, "cards") || ci_word(k, "cds")) {
                snprintf(out, outlen, "%d",
                         card_count(g_current_card ? g_current_card->owner : NULL));
                return;
            }
            /* the number of [card|bg] buttons|fields [of <carte>]
             *
             * La portée manquait : « the number of fields » additionnait la
             * carte ET le fond, alors que les rangs, eux, se comptent
             * séparément dans chacun. Sur une carte de 3 champs posée sur un
             * fond qui en porte 3, le compteur annonçait 6 et « field 4 » ne
             * désignait rien — toute boucle « repeat with f = 1 to the number
             * of fields » partait droit dans le mur. Le total reste la valeur
             * par défaut, par compatibilité, mais on peut désormais demander
             * l'un ou l'autre. */
            {
                const char *k2 = k;
                int scope = 0;              /* 0 = total, 1 = carte, 2 = fond */
                if (ci_word(k2, "card") || ci_word(k2, "cd")) {
                    const char *a = k2;
                    while (*a && !isspace((unsigned char)*a)) a++;
                    a = skip_spaces(a);
                    if (ci_word(a, "buttons") || ci_word(a, "btns") ||
                        ci_word(a, "fields")  || ci_word(a, "flds")) { scope = 1; k2 = a; }
                } else if (ci_word(k2, "bg") || ci_word(k2, "background")) {
                    const char *a = k2;
                    while (*a && !isspace((unsigned char)*a)) a++;
                    a = skip_spaces(a);
                    if (ci_word(a, "buttons") || ci_word(a, "btns") ||
                        ci_word(a, "fields")  || ci_word(a, "flds")) { scope = 2; k2 = a; }
                }

                if (ci_word(k2, "buttons") || ci_word(k2, "btns") ||
                    ci_word(k2, "fields")  || ci_word(k2, "flds")) {
                    ObjType want = (k2[0]=='b' || k2[0]=='B') ? OBJ_BUTTON : OBJ_FIELD;
                    const char *r = k2;
                    while (*r && !isspace((unsigned char)*r)) r++;
                    r = skip_spaces(r);
                    if (ci_word(r, "of")) r = skip_spaces(r + 2);
                    Object *card = *r ? resolve(r) : g_current_card;
                    if (card && card->type != OBJ_CARD && card->type != OBJ_BACKGROUND)
                        card = g_current_card;
                    int n = 0;
                    if (card) {
                        /* Un fond désigné explicitement ne compte que le sien. */
                        int own = (scope != 2) || card->type == OBJ_BACKGROUND;
                        if (own)
                            for (int i = 0; i < card->nparts; i++)
                                if (card->parts[i]->type == want) n++;
                        if (scope != 1 && card->type == OBJ_CARD && card->bg)
                            for (int i = 0; i < card->bg->nparts; i++)
                                if (card->bg->parts[i]->type == want) n++;
                    }
                    snprintf(out, outlen, "%d", n);
                    return;
                }
            }
            int used = 0;
            ChunkType ct = chunk_kind(k, &used);
            if (ct != CH_NONE) {
                const char *r = skip_spaces(k + used);
                if (ci_word(r, "in") || ci_word(r, "of")) r = skip_spaces(r + 2);
                char *src = arena_buf();
                eval_expr(r, src, HC_VAL);
                snprintf(out, outlen, "%d", chunk_count(src, ct));
                return;
            }

            /* the number of <objet> : son RANG parmi ses semblables.
             *
             * « the number of field "test" » répond 2 si c'est le deuxième
             * champ de son propriétaire. C'est le pendant exact de la
             * désignation par rang, et donc de quoi savoir quel chiffre écrire
             * dans « field N » — ou constater qu'un champ vit sur le fond et
             * non sur la carte. En dernier recours : les morceaux et les
             * pluriels ont déjà eu leur tour, il ne reste qu'un objet. */
            {
                Object *ob = resolve(k);
                if (ob && (ob->type == OBJ_BUTTON || ob->type == OBJ_FIELD)) {
                    int n = hc_object_number(ob);
                    if (n > 0) { snprintf(out, outlen, "%d", n); return; }
                }
                if (ob && ob->type == OBJ_CARD) {
                    int n = card_index(ob->owner, ob);
                    if (n >= 0) { snprintf(out, outlen, "%d", n + 1); return; }
                }
            }
        }
    }

    /* --- fonctions intégrées --- */
    if (call_function(t, out, outlen)) return;

    /* --- [the] [short|long] <propriété> of <objet> ---
     * HyperCard tolère l'omission de « the » : « bottom of this cd »,
     * « loc of me ». On ne l'accepte que si le premier mot est bien un nom
     * de propriété connu, sinon « item 1 of x » ou une variable suivie de
     * « of » se feraient happer. */
    if (ci_word(t, "the") || prop_word_before_of(t)) {
        const char *w = ci_word(t, "the") ? skip_spaces(t + 3) : t;

        int shortf = 0;
        if (ci_word(w, "short")) { shortf = 1; w = skip_spaces(w + 5); }
        else if (ci_word(w, "long")) { w = skip_spaces(w + 4); }

        const char *of = find_kw(w, "of");
        if (of) {
            char prop[32];
            int pl = (int)(of - w);
            while (pl > 0 && (w[pl-1] == ' ' || w[pl-1] == '\t')) pl--;
            if (pl > 0 && pl < (int)sizeof prop) {
                memcpy(prop, w, (size_t)pl); prop[pl] = '\0';

                /* Lecture sur une plage de texte :
                 *     if the textStyle of the clickChunk is bold
                 * La cible peut donc etre calculee : « of + 2 » est evalue
                 * comme expression si ce n'est pas deja un morceau litteral. */
                if (ci_equal(prop, "textstyle") || ci_equal(prop, "textfont") ||
                    ci_equal(prop, "textsize")  || ci_equal(prop, "textheight")) {
                    int cst, cen;
                    Object *cf = chunk_target(of + 2, &cst, &cen);
                    if (cf) {
                        struct RunList *rl = runs_of(cf);
                        if (ci_equal(prop, "textstyle")) {
                            style_to_names(runs_get_style(rl, cst, cen - cst,
                                                          cf->textstyle),
                                           out, outlen);
                        } else if (ci_equal(prop, "textfont")) {
                            runs_get_font(rl, cst, cen - cst, cf->textfont,
                                          out, outlen);
                        } else {
                            int sz = runs_get_size(rl, cst, cen - cst, cf->textsize);
                            if (sz < 0) snprintf(out, outlen, "mixed");
                            else        snprintf(out, outlen, "%d", sz);
                        }
                        return;
                    }
                }

                Object *o = resolve(of + 2);
                if (o) {
                    if (geom_read(o, prop, out, outlen)) return;
                    if (ci_equal(prop, "id"))      { snprintf(out, outlen, "%d", o->id); return; }
                    if (ci_equal(prop, "name")) {
                        if (shortf) snprintf(out, outlen, "%s", o->name ? o->name : "");
                        else        hc_describe(o, out, outlen);
                        return;
                    }
                    if (ci_equal(prop, "visible")) { snprintf(out, outlen, "%s", o->visible ? "true" : "false"); return; }
                    if (ci_equal(prop, "showname") || ci_equal(prop, "shownname")) { snprintf(out, outlen, "%s", o->showname ? "true" : "false"); return; }
                    if (ci_equal(prop, "icon")) { snprintf(out, outlen, "%d", o->icon); return; }
                    if (ci_equal(prop, "selectedline") || ci_equal(prop, "selectedlines"))
                        { snprintf(out, outlen, "%d", o->selectedline); return; }
                    if (ci_equal(prop, "locktext")) { snprintf(out, outlen, "%s", o->locktext ? "true" : "false"); return; }
                    if (ci_equal(prop, "widemargins")) { snprintf(out, outlen, "%s", o->wide_margins ? "true" : "false"); return; }
                    if (ci_equal(prop, "fixedlineheight")) { snprintf(out, outlen, "%s", o->fixed_lh ? "true" : "false"); return; }
                    if (ci_equal(prop, "showlines")) { snprintf(out, outlen, "%s", o->show_lines ? "true" : "false"); return; }
                    if (ci_equal(prop, "autotab")) { snprintf(out, outlen, "%s", o->auto_tab ? "true" : "false"); return; }
                    if (ci_equal(prop, "dontsearch")) { snprintf(out, outlen, "%s", o->dont_search ? "true" : "false"); return; }
                    if (ci_equal(prop, "sharedtext")) { snprintf(out, outlen, "%s", o->shared_text ? "true" : "false"); return; }
                    if (ci_equal(prop, "textfont")) { snprintf(out, outlen, "%s", o->textfont ? o->textfont : ""); return; }
                    if (ci_equal(prop, "scroll")) { snprintf(out, outlen, "%d", o->scroll); return; }
                    if (ci_equal(prop, "textstyle")) {
                        /* Même formatage que la lecture sur un morceau : le
                         * code d'origine n'écrivait que les trois premiers
                         * bits, si bien qu'un objet en creux se relisait
                         * « plain » et perdait son style à l'enregistrement. */
                        style_to_names(o->textstyle, out, outlen);
                        return;
                    }
                    if (ci_equal(prop, "hilite") || ci_equal(prop, "highlight")) { snprintf(out, outlen, "%s", o->hilite ? "true" : "false"); return; }
                    if (ci_equal(prop, "autohilite")) { snprintf(out, outlen, "%s", o->autohilite ? "true" : "false"); return; }
                    if (ci_equal(prop, "textsize") || ci_equal(prop, "textheight")) { snprintf(out, outlen, "%d", o->textsize); return; }
                    if (ci_equal(prop, "script"))  {
                        const char *sc = o->script ? o->script : "";
                        /* Les valeurs du noyau tiennent dans des tampons fixes.
                         * Un script plus long est tronqué : le dire, sinon un
                         * « set script of me to it » réécrit la version mutilée
                         * sans que personne ne s'en aperçoive. */
                        if ((int)strlen(sc) >= outlen) {
                            g_script_clipped = 1;
                            emit(HC_ERR, "   !! script de %d octets tronqué à %d : "
                                         "toute réécriture sera refusée",
                                 (int)strlen(sc), outlen - 1);
                        }
                        snprintf(out, outlen, "%s", sc); return;
                    }
                    if (ci_equal(prop, "text") || ci_equal(prop, "contents"))
                                                   { snprintf(out, outlen, "%s", hc_field_text(o)); return; }
                    if (ci_equal(prop, "style"))   { snprintf(out, outlen, "%s", o->style ? o->style : "rectangle"); return; }
                }
            }
        }
    }

    /* --- un objet ? champ → contenu, autre → sa désignation --- */
    Object *o = resolve(t);
    if (o) {
        if (o->type == OBJ_FIELD) snprintf(out, outlen, "%s", hc_field_text(o));
        else                      hc_describe(o, out, outlen);
        return;
    }

    /* --- une variable ? --- */
    if (!strchr(t, ' ')) {
        const char *v = var_get(t);
        if (v) { snprintf(out, outlen, "%s", v); return; }
    }

    /* --- une propriété globale ? (« the mouse », « the mouseLoc »…) ---
     * Après les variables : un script qui nomme sa variable « mouse » garde
     * la priorité, comme dans HyperCard. */
    {
        const char *g = t;
        if (ci_word(g, "the")) g = skip_spaces(g + 3);
        if (*g && !strchr(g, ' ')) {
            const char *v = host_global(g);
            if (v) { snprintf(out, outlen, "%s", v); return; }
        }
    }

    /* --- sinon littéral non quoté, comme le faisait HyperCard --- */
    snprintf(out, outlen, "%s", t);
}

static void term_value(const char *t, char *out, int outlen)
{
    ARENA_MARK;
    term_value_body(t, out, outlen);
    ARENA_FREE;
}

static void parse_expr(const char **p, char *out, int outlen);

static int truthy(const char *s);

static void parse_factor(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    const char *s = skip_spaces(*p);
    out[0] = '\0';

    if (*s == '(') {
        *p = s + 1;
        parse_expr(p, out, outlen);
        s = skip_spaces(*p);
        if (*s == ')') s++;
        *p = s;
        { ARENA_FREE; return; }
    }
    if (*s == '-') {
        char *v = arena_buf();
        double d = 0;
        *p = s + 1;
        parse_factor(p, v, HC_VAL);
        as_num(v, &d);
        put_num(-d, out, outlen);
        { ARENA_FREE; return; }
    }
    /* `not` est au niveau 2 chez Apple, aussi serré que le moins unaire :
       « not 5 > 2 » se lit « (not 5) > 2 ». */
    if (ci_word(s, "not")) {
        char *v = arena_buf();
        *p = s + 3;
        parse_factor(p, v, HC_VAL);
        snprintf(out, outlen, "%s", truthy(v) ? "false" : "true");
        { ARENA_FREE; return; }
    }
    /* « there is a <objet> » / « there is no <objet> » / « there is not a … »
     * Tout ce qui suit désigne l'objet : l'expression s'arrête là, ce qui
     * suffit puisque la forme n'apparaît jamais qu'en position de condition. */
    if (ci_word(s, "there")) {
        const char *q = skip_spaces(s + 5);
        if (ci_word(q, "is")) {
            q = skip_spaces(q + 2);
            int negate = 0;
            if (ci_word(q, "not"))     { negate = 1; q = skip_spaces(q + 3); }
            else if (ci_word(q, "no")) { negate = 1; q = skip_spaces(q + 2); }
            if (ci_word(q, "an"))      q = skip_spaces(q + 2);
            else if (ci_word(q, "a"))  q = skip_spaces(q + 1);

            char *ref = arena_buf();
            snprintf(ref, HC_VAL, "%s", q);
            int k = (int)strlen(ref);
            while (k > 0 && isspace((unsigned char)ref[k-1])) ref[--k] = '\0';

            int found = resolve(ref) != NULL;
            snprintf(out, outlen, "%s", (found != negate) ? "true" : "false");
            *p = q + strlen(q);
            { ARENA_FREE; return; }
        }
    }

    if (isdigit((unsigned char)*s) || (*s == '.' && isdigit((unsigned char)s[1]))) {
        char *e;
        double d = strtod(s, &e);
        *p = e;
        put_num(d, out, outlen);
        { ARENA_FREE; return; }
    }
    if (*s == '"') {                 /* littéral : on ne prend que lui */
        *p = quoted(s, out, outlen);
        { ARENA_FREE; return; }
    }

    char *ref = arena_buf();
    const char *before = s;
    collect_ref(&s, ref, HC_VAL);
    if (s == before && *s) s++;      /* jamais de sur-place : pas de boucle infinie */
    *p = s;
    term_value(ref, out, outlen);
    ARENA_FREE;
    ARENA_FREE;
}

/* niveau 3 : exponentiation, associative à droite */
static void parse_power(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_factor(p, out, outlen);
    const char *s = skip_spaces(*p);
    if (*s != '^') return;
    *p = s + 1;

    char *rhs = arena_buf();
    double a = 0, b = 0;
    parse_power(p, rhs, HC_VAL);      /* récursif : 2^3^2 = 2^(3^2) */
    as_num(out, &a); as_num(rhs, &b);
    put_num(pow(a, b), out, outlen);
    ARENA_FREE;
    ARENA_FREE;
}

static void parse_product(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_power(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        int op;
        if      (*s == '*')          { op = '*'; s += 1; }
        else if (*s == '/')          { op = '/'; s += 1; }
        else if (ci_word(s, "mod"))  { op = 'm'; s += 3; }
        else if (ci_word(s, "div"))  { op = 'd'; s += 3; }
        else break;
        *p = s;

        char *rhs = arena_buf();
        double a = 0, b = 0, r = 0;
        parse_power(p, rhs, HC_VAL);
        as_num(out, &a); as_num(rhs, &b);
        if      (op == '*') r = a * b;
        else if (op == '/') r = (b != 0) ? a / b : 0;
        else if (op == 'd') r = (b != 0) ? (double)(long long)(a / b) : 0;
        else                r = (b != 0) ? a - b * (double)(long long)(a / b) : 0;
        put_num(r, out, outlen);
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void parse_sum(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_product(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        if (*s != '+' && *s != '-') break;
        int op = *s++;
        *p = s;

        char *rhs = arena_buf();
    double a = 0, b = 0;
        parse_product(p, rhs, HC_VAL);
        as_num(out, &a); as_num(rhs, &b);
        put_num(op == '+' ? a + b : a - b, out, outlen);
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void parse_concat(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_sum(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        int space;
        if      (s[0] == '&' && s[1] == '&') { space = 1; s += 2; }
        else if (s[0] == '&')                { space = 0; s += 1; }
        else break;
        *p = s;

        char *rhs = arena_buf();
        parse_sum(p, rhs, HC_VAL);
        int n = (int)strlen(out);
        if (space && n < outlen - 1) { out[n++] = ' '; out[n] = '\0'; }
        snprintf(out + n, (size_t)(outlen - n), "%s", rhs);
    }
    ARENA_FREE;
    ARENA_FREE;
}

/* Compare deux valeurs. Numérique si les deux en sont, sinon texte
   sans tenir compte de la casse — comme HyperTalk. */
static int compare_vals(int op, const char *x, const char *y)
{
    double a, b;
    if (op == 'c') return ci_strstr(x, y);      /* contains */
    if (op == 'i') return ci_strstr(y, x);      /* is in : l'inverse */
    if (as_num(x, &a) && as_num(y, &b)) {
        switch (op) {
            case '=': return a == b;
            case '!': return a != b;
            case '<': return a <  b;
            case '>': return a >  b;
            case 'l': return a <= b;
            default:  return a >= b;
        }
    }
    int c = ci_cmp(x, y);
    switch (op) {
        case '=': return c == 0;
        case '!': return c != 0;
        case '<': return c <  0;
        case '>': return c >  0;
        case 'l': return c <= 0;
        default:  return c >= 0;
    }
}

static int truthy(const char *s)
{
    double d;
    if (as_num(s, &d)) return d != 0;
    return ci_equal(s, "true");
}

/* ---- tests de type pour « is a[n] <type> » ----
   Le guide (chapitre 7) donne : number, integer, point, rect, date, logical. */

static void trim_copy(const char *s, char *out, int outlen)
{
    s = skip_spaces(s);
    snprintf(out, outlen, "%s", s);
    int n = (int)strlen(out);
    while (n > 0 && (out[n-1] == ' ' || out[n-1] == '\t')) out[--n] = '\0';
}

static int is_int_str(const char *s)
{
    double d;
    if (!as_num(s, &d)) return 0;
    return d == (double)(long long)d;
}

/* nombre d'items entiers séparés par des virgules ; -1 si l'un ne l'est pas */
static int int_items(const char *s)
{
    int n = 0;
    char buf[64];
    const char *p = s;
    for (;;) {
        const char *c = strchr(p, ',');
        int len = c ? (int)(c - p) : (int)strlen(p);
        if (len > (int)sizeof buf - 1) return -1;
        memcpy(buf, p, (size_t)len); buf[len] = '\0';
        if (!is_int_str(buf)) return -1;
        n++;
        if (!c) break;
        p = c + 1;
    }
    return n;
}

/* Forme numérique seulement (12/25/96, 1996-12-25). Sans horloge dans le
   noyau, on ne reconnaît pas encore « December 25, 1996 ». */
/* « is a date » et « convert » doivent s'accorder : ce que l'un accepte,
 * l'autre doit le reconnaître. Sans quoi un script qui valide une saisie
 * par « if it is a date then convert it » tourne en boucle sur une date
 * pourtant convertible — les dateItems, par exemple. */
static int looks_like_date(const char *s)
{
    struct tm tm;
    return parse_datetime(s, &tm);
}

static int is_of_type(const char *v, const char *ty)
{
    ARENA_MARK;
    char *t = arena_buf();
    double d;
    trim_copy(v, t, HC_VAL);

    if (ci_equal(ty, "number"))    return as_num(t, &d);
    if (ci_equal(ty, "integer"))   return is_int_str(t);
    if (ci_equal(ty, "logical") || ci_equal(ty, "boolean"))
        { ARENA_FREE; return ci_equal(t, "true") || ci_equal(t, "false"); }
    if (ci_equal(ty, "point"))     return int_items(t) == 2;
    if (ci_equal(ty, "rect") || ci_equal(ty, "rectangle")) return int_items(t) == 4;
    if (ci_equal(ty, "date"))      return looks_like_date(t);
    { ARENA_FREE; return 0; }
    ARENA_FREE;
    ARENA_FREE;
}

/* niveau 7 : comparaisons relationnelles, contains, is in.
   Attention : un « is » nu appartient au niveau 8, on le laisse passer. */
static void parse_relational(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_concat(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        int op = 0, neg = 0;

        if      (s[0] == '<' && s[1] == '=') { op = 'l'; s += 2; }
        else if (s[0] == '>' && s[1] == '=') { op = 'g'; s += 2; }
        else if (s[0] == '<' && s[1] == '>') break;      /* <> : niveau 8 */
        else if (s[0] == '<')                { op = '<'; s += 1; }
        else if (s[0] == '>')                { op = '>'; s += 1; }
        else if (ci_word(s, "contains"))     { op = 'c'; s += 8; }
        else if (ci_word(s, "is")) {
            const char *w = skip_spaces(s + 2);
            int notted = 0;
            if (ci_word(w, "not")) { notted = 1; w = skip_spaces(w + 3); }

            if (ci_word(w, "a") || ci_word(w, "an")) {      /* test de type */
                /* Lire le nom de type lettre à lettre, sans next_word : celui-ci
                 * s'arrête aux blancs et emporterait la parenthèse fermante de
                 * « (it is a date) », ce qui faisait échouer tous les tests de
                 * type placés entre parenthèses. */
                char ty[32]; int tk = 0;
                const char *q = skip_spaces(w + (ci_word(w, "an") ? 2 : 1));
                while (isalpha((unsigned char)*q) && tk < (int)sizeof ty - 1)
                    ty[tk++] = *q++;
                ty[tk] = '\0';
                *p = q;
                int r = is_of_type(out, ty);
                if (notted) r = !r;
                snprintf(out, outlen, "%s", r ? "true" : "false");
                continue;
            }
            if (ci_word(w, "within")) {                     /* point is within rect */
                *p = w + 6;
                char *rhs = arena_buf();
                parse_concat(p, rhs, HC_VAL);
                int pt[2], rc[4];
                int r = 0;
                if (parse_ints(out, pt, 2) == 2 && parse_ints(rhs, rc, 4) == 4)
                    r = (pt[0] >= rc[0] && pt[0] <= rc[2] &&
                         pt[1] >= rc[1] && pt[1] <= rc[3]);
                if (notted) r = !r;
                snprintf(out, outlen, "%s", r ? "true" : "false");
                continue;
            }
            if (ci_word(w, "in")) { op = 'i'; neg = notted; s = w + 2; }
            else break;                                     /* « is » / « is not » : niveau 8 */
        }
        else break;
        *p = s;

        char *rhs = arena_buf();
        parse_concat(p, rhs, HC_VAL);
        int r = compare_vals(op, out, rhs);
        if (neg) r = !r;
        snprintf(out, outlen, "%s", r ? "true" : "false");
    }
    ARENA_FREE;
    ARENA_FREE;
}

/* niveau 8 : égalités */
static void parse_equality(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_relational(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        int op = 0, neg = 0;

        if      (s[0] == '<' && s[1] == '>') { op = '!'; s += 2; }
        else if (s[0] == '=')                { op = '='; s += 1; }
        else if (ci_word(s, "is")) {
            const char *w = skip_spaces(s + 2);
            if (ci_word(w, "not")) { neg = 1; s = w + 3; }
            else s += 2;
            op = '=';
        }
        else break;
        *p = s;

        char *rhs = arena_buf();
        parse_relational(p, rhs, HC_VAL);
        int r = compare_vals(op, out, rhs);
        if (neg) r = !r;
        snprintf(out, outlen, "%s", r ? "true" : "false");
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void parse_and(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_equality(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        if (!ci_word(s, "and")) break;
        *p = s + 3;
        char *rhs = arena_buf();
        parse_equality(p, rhs, HC_VAL);
        snprintf(out, outlen, "%s", (truthy(out) && truthy(rhs)) ? "true" : "false");
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void parse_expr(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_and(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        if (!ci_word(s, "or")) break;
        *p = s + 2;
        char *rhs = arena_buf();
        parse_and(p, rhs, HC_VAL);
        snprintf(out, outlen, "%s", (truthy(out) || truthy(rhs)) ? "true" : "false");
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void eval_expr(const char *s, char *out, int outlen)
{
    ARENA_MARK;
    const char *p = s;
    parse_expr(&p, out, outlen);
    ARENA_FREE;
    ARENA_FREE;
}

/* Comme eval_expr, mais râle si l'analyseur n'a pas tout mangé.
 * C'est le garde-fou contre les fautes de frappe : sans lui, une
 * expression mal formée retombe silencieusement en littéral. */
static void eval_checked(const char *s, char *out, int outlen)
{
    ARENA_MARK;
    const char *p = s;
    parse_expr(&p, out, outlen);
    const char *left = skip_spaces(p);
    if (*left) {
        char shown[256];
        snprintf(shown, sizeof shown, "%s", left);
        int n = (int)strlen(shown);
        while (n > 0 && (shown[n-1] == ' ' || shown[n-1] == '\t')) shown[--n] = '\0';
        emit(HC_ERR, "   !! texte incompris, ignoré : « %s »", shown);
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void exec_line(Object *me, const char *line);

/* ==================== structures de contrôle ==================== */

/* Drapeaux de sortie, tous remis à zéro par hc_send. */
static int g_exit_handler = 0;   /* exit <gestionnaire> */
static int g_exit_repeat  = 0;   /* exit repeat */
static int g_next_repeat  = 0;   /* next repeat */

/* Une boucle sans fin bloquerait la console : plafond de sécurité. */
#define HC_MAX_LOOP 1000000

/* faut-il interrompre le fil d'exécution courant ? */
static int flow_broken(void)
{
    return g_pass || g_exit_handler || g_exit_repeat || g_next_repeat;
}

/* Cherche le mot `w` dans `s`, hors guillemets. Renvoie NULL sinon. */
static const char *find_kw(const char *s, const char *w)
{
    int inq = 0;
    size_t wl = strlen(w);
    for (const char *q = s; *q; q++) {
        if (*q == '"') { inq = !inq; continue; }
        if (inq) continue;
        if (q != s && !isspace((unsigned char)q[-1])) continue;
        if (ci_nequal(q, w, (int)wl)) {
            char c = q[wl];
            if (!c || isspace((unsigned char)c)) return q;
        }
    }
    return NULL;
}

/* Évalue un point pour « show … at ». HyperCard accepte les deux formes :
 *   show me at 100,120          → deux expressions séparées par une virgule
 *   show me at the mouseLoc     → une seule expression rendant « x,y »
 * On découpe donc aux virgules de premier niveau (hors guillemets et
 * parenthèses), on évalue chaque morceau, et on recolle avec une virgule.
 * Ainsi « show me at horz,vert » comme « show me at item 1 of p, 20 »
 * arrivent tous deux sous la forme « x,y » attendue par geom_write. */
static void eval_point(const char *s, char *out, int outlen)
{
    int pos = 0, depth = 0, inq = 0, n = 0;
    const char *part = skip_spaces(s);
    out[0] = '\0';

    for (const char *q = part; ; q++) {
        if (*q == '"') inq = !inq;
        else if (!inq && *q == '(') depth++;
        else if (!inq && *q == ')') depth--;

        if (*q && !(!inq && depth <= 0 && *q == ',')) continue;

        char *expr = arena_buf();
        char *val = arena_buf();
        int len = (int)(q - part);
        while (len > 0 && isspace((unsigned char)part[len-1])) len--;
        if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
        memcpy(expr, part, (size_t)len); expr[len] = '\0';
        eval_checked(expr, val, HC_VAL);

        pos += snprintf(out + pos, (size_t)(outlen - pos), "%s%s",
                        n ? "," : "", val);
        if (pos >= outlen) pos = outlen - 1;
        n++;

        if (!*q) break;
        part = skip_spaces(q + 1);
    }
}

/* Une ligne ouvre-t-elle un bloc ? (« if … then » sans suite, « repeat … ») */
static int opens_if(const char *s)
{
    if (!ci_word(s, "if")) return 0;
    const char *th = find_kw(s, "then");
    return th && !*skip_spaces(th + 4);
}

static int opens_repeat(const char *s)
{
    return ci_word(s, "repeat");
}

/* « else <instruction> » referme un if à lui seul : pas de « end if ».
   En revanche « else » seul, ou « else if … then » en bloc, ouvre une suite. */
static int else_closes(const char *s)
{
    if (!ci_word(s, "else")) return 0;
    const char *rest = skip_spaces(s + 4);
    if (!*rest) return 0;           /* « else » seul : le bloc continue */
    if (opens_if(rest)) return 0;   /* « else if … then » en bloc */
    return 1;
}

static int match_end(char **L, int from, int to, const char *what);

/* Formes hybrides « if … then <instruction> » + « else » à la ligne :
 * définies plus bas, mais les trois explorateurs ci-dessous doivent déjà
 * savoir les enjamber, sans quoi le « else » de l'if interne passe pour la
 * fin de l'if externe. */
static int is_inline_if(const char *s);
static int chain_end(char **L, int i, int to);

/* Vrai si L[i] ouvre une construction hybride qu'il faut enjamber d'un bloc. */
static int opens_inline_chain(char **L, int i, int to)
{
    const char *th;
    if (!is_inline_if(L[i])) return 0;

    /* Une ligne qui porte déjà son propre « else » est complète en elle-même :
     *     if the result <> empty then get error() else exit repeat
     *     else get error()          <- appartient à un if ENGLOBANT
     * Sans ce garde-fou on s'empare du « else » du dessous, et la branche
     * exécutée n'est pas celle que le script demande. */
    th = find_kw(L[i], "then");
    if (th && find_kw(th + 4, "else")) return 0;

    return i + 1 < to && ci_word(L[i+1], "else");
}

/* Index de la dernière ligne de la construction ouverte par L[open]. */
static int skip_block(char **L, int open, int to)
{
    if (opens_inline_chain(L, open, to)) return chain_end(L, open, to);
    if (opens_if(L[open]))     return match_end(L, open + 1, to, "if");
    if (opens_repeat(L[open])) return match_end(L, open + 1, to, "repeat");
    return open;
}

/* Index de la ligne fermante correspondante. Pour un « if », ce peut être
   « end if » ou l'« else <instruction> » qui le referme. Les constructions
   imbriquées sont sautées par récursion : plus de compteur de profondeur,
   qui ne survit pas à un bloc ayant deux fermetures possibles. */
static int match_end(char **L, int from, int to, const char *what)
{
    int isif = (strcmp(what, "if") == 0);
    for (int i = from; i < to; i++) {
        const char *s = L[i];
        if (opens_inline_chain(L, i, to) ||
            opens_if(s) || opens_repeat(s)) { i = skip_block(L, i, to); continue; }
        if (isif && else_closes(s)) return i;
        if (ci_word(s, "end")) {
            const char *w = skip_spaces(s + 3);
            if (isif  && ci_word(w, "if"))     return i;
            if (!isif && ci_word(w, "repeat")) return i;
        }
    }
    return to;
}

/* Premier « else » de même niveau entre `from` et `to`. */
static int find_else(char **L, int from, int to)
{
    for (int i = from; i < to; i++) {
        const char *s = L[i];
        if (opens_inline_chain(L, i, to) ||
            opens_if(s) || opens_repeat(s)) { i = skip_block(L, i, to); continue; }
        if (ci_word(s, "else")) return i;
    }
    return -1;
}

static void exec_block(Object *me, char **L, int from, int to);
static void exec_stmt(Object *me, const char *s);

/* `head` vaut « if <condition> then » ; le corps va de `from` à `end_idx`. */
static void exec_if(Object *me, const char *head, char **L, int from, int end_idx)
{
    ARENA_MARK;
    const char *th = find_kw(head, "then");
    char *cond = arena_buf();
    char *val = arena_buf();
    const char *c0 = skip_spaces(head + 2);   /* après « if » */
    int n = th ? (int)(th - c0) : (int)strlen(c0);
    if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
    memcpy(cond, c0, (size_t)n); cond[n] = '\0';
    eval_checked(cond, val, HC_VAL);

    int m = find_else(L, from, end_idx);
    if (truthy(val)) {
        exec_block(me, L, from, m >= 0 ? m : end_idx);
        { ARENA_FREE; return; }
    }
    if (m < 0) return;

    const char *rest = skip_spaces(L[m] + 4);   /* après « else » */
    if (!*rest) { exec_block(me, L, m + 1, end_idx); return; }
    if (opens_if(rest)) { exec_if(me, rest, L, m + 1, end_idx); return; }
    exec_stmt(me, rest);          /* « else <instruction> », if en ligne compris */
    ARENA_FREE;
    ARENA_FREE;
}

/* Exécute une instruction simple, ou un `if` tenant sur une seule ligne. */
static void exec_stmt(Object *me, const char *s)
{
    ARENA_MARK;
    if (ci_word(s, "if")) {
        const char *th = find_kw(s, "then");
        if (th) {
            char *cond = arena_buf();
            char *val = arena_buf();
            const char *c0 = skip_spaces(s + 2);
            int n = (int)(th - c0);
            if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
            memcpy(cond, c0, (size_t)n); cond[n] = '\0';
            eval_checked(cond, val, HC_VAL);

            const char *body = skip_spaces(th + 4);
            const char *el   = find_kw(body, "else");
            char *yes = arena_buf();
            int ny = el ? (int)(el - body) : (int)strlen(body);
            if (ny > (int)HC_VAL - 1) ny = (int)HC_VAL - 1;
            memcpy(yes, body, (size_t)ny); yes[ny] = '\0';

            if (truthy(val)) exec_stmt(me, yes);
            else if (el)     exec_stmt(me, skip_spaces(el + 4));
            { ARENA_FREE; return; }
        }
    }
    exec_line(me, s);
    ARENA_FREE;
    ARENA_FREE;
}

/* Un `if` dont le « then » porte déjà une instruction, mais dont le « else »
 * ouvre la ligne suivante. HyperCard admet cette forme hybride, et la chaîne :
 *
 *     if (it is in "1,3,5,7,8,10") or (it = 12) then return 31
 *     else if (it is in "4,6,9,11") then return 30
 *     else
 *       ...
 *     end if
 *
 * Ni opens_if (qui exige un « then » en fin de ligne) ni exec_stmt (qui ne
 * regarde qu'une ligne) ne savent la lire. On la traite en deux temps :
 * d'abord mesurer l'étendue de la construction, ensuite l'exécuter. */

/* Vrai si la ligne est « if <cond> then <instruction> », then non terminal. */
static int is_inline_if(const char *s)
{
    if (!ci_word(s, "if")) return 0;
    const char *th = find_kw(s, "then");
    return th && *skip_spaces(th + 4);
}

/* Indice de la dernière ligne de la construction ouverte en `i`. */
static int chain_end(char **L, int i, int to)
{
    int j = i;
    while (j + 1 < to && ci_word(L[j+1], "else")) {
        const char *rest = skip_spaces(L[j+1] + 4);
        if (!*rest)                                  /* « else » seul : bloc */
            return match_end(L, j + 2, to, "if");
        j++;
        if (!is_inline_if(rest)) break;              /* « else <instruction> » */
    }
    return j;
}

static void exec_if_chain(Object *me, char **L, int i, int to, int end)
{
    ARENA_MARK;
    const char *head = L[i];
    for (;;) {
        const char *th = find_kw(head, "then");
        char *cond = arena_buf();
        char *val = arena_buf();
        const char *c0 = skip_spaces(head + 2);      /* après « if » */
        int n = (int)(th - c0);
        if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
        memcpy(cond, c0, (size_t)n); cond[n] = '\0';
        eval_checked(cond, val, HC_VAL);

        if (truthy(val)) { exec_stmt(me, skip_spaces(th + 4)); return; }

        if (i + 1 >= to || !ci_word(L[i+1], "else")) return;
        const char *rest = skip_spaces(L[i+1] + 4);
        if (!*rest)          { exec_block(me, L, i + 2, end); return; }
        if (is_inline_if(rest)) { head = rest; i++; continue; }
        exec_stmt(me, rest); return;
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void exec_block(Object *me, char **L, int from, int to)
{
    for (int i = from; i < to; i++) {
        const char *s = L[i];

        /* --- if … then <instruction> avec un « else » à la ligne --- */
        if (opens_inline_chain(L, i, to)) {
            int e = chain_end(L, i, to);
            exec_if_chain(me, L, i, to, e);
            i = e;
            if (flow_broken()) return;
            continue;
        }

        /* --- if … then / end if --- */
        if (opens_if(s)) {
            int e = match_end(L, i + 1, to, "if");
            /* si c'est un « else <instruction> » qui referme, il fait
               partie de la construction : exec_if doit le voir */
            int body_end = (e < to && else_closes(L[e])) ? e + 1 : e;
            exec_if(me, s, L, i + 1, body_end);
            i = e;
            if (flow_broken()) return;
            continue;
        }

        /* --- repeat … / end repeat --- */
        if (opens_repeat(s)) {
            int e = match_end(L, i + 1, to, "repeat");
            const char *h = skip_spaces(s + 6);

            /* variantes de l'en-tête */
            int    kind = 0;            /* 0 sans fin, 1 n fois, 2 while, 3 until, 4 with */
            long   times = 0;
            char   var[128] = "", from_e[256] = "", to_e[256] = "", cond[256] = "";
            int    down = 0;

            if (!*h || ci_word(h, "forever")) kind = 0;
            else if (ci_word(h, "while")) { kind = 2; snprintf(cond, sizeof cond, "%s", h + 5); }
            else if (ci_word(h, "until")) { kind = 3; snprintf(cond, sizeof cond, "%s", h + 5); }
            else if (ci_word(h, "with")) {
                kind = 4;
                const char *q = next_word(skip_spaces(h + 4), var, sizeof var);
                q = skip_spaces(q);
                if (*q == '=') q++;
                const char *tt = find_kw(q, "to");
                const char *dn = find_kw(q, "down");
                if (dn && (!tt || dn < tt)) { down = 1; tt = find_kw(dn, "to"); }
                int n = tt ? (int)(tt - q) : (int)strlen(q);
                if (down && dn) n = (int)(dn - q);
                if (n > (int)sizeof from_e - 1) n = (int)sizeof from_e - 1;
                memcpy(from_e, q, (size_t)n); from_e[n] = '\0';
                snprintf(to_e, sizeof to_e, "%s", tt ? tt + 2 : "0");
            } else {
                kind = 1;
                const char *q = h;
                if (ci_word(q, "for")) q = skip_spaces(q + 3);

                /* Retirer le « times » final avant d'évaluer : sans ça,
                 * « repeat n times » fait lire « n times » comme une seule
                 * référence, qui ne vaut rien — donc zéro tour. Avec un
                 * littéral le problème ne se voyait pas, parse_factor
                 * s'arrêtant tout seul après le nombre. */
                char cnt[256];
                const char *tm = find_kw(q, "times");
                int cn = tm ? (int)(tm - q) : (int)strlen(q);
                while (cn > 0 && isspace((unsigned char)q[cn-1])) cn--;
                if (cn > (int)sizeof cnt - 1) cn = (int)sizeof cnt - 1;
                memcpy(cnt, q, (size_t)cn); cnt[cn] = '\0';

                char v[256];
                eval_expr(cnt, v, sizeof v);
                double d = 0; as_num(v, &d);
                times = (long)d;
            }

            double cur = 0, last = 0;
            if (kind == 4) {
                char v[256];
                eval_expr(from_e, v, sizeof v); as_num(v, &cur);
                eval_expr(to_e,   v, sizeof v); as_num(v, &last);
            }

            long iter = 0;
            for (;;) {
                if (kind == 1 && iter >= times) break;
                if (kind == 2 || kind == 3) {
                    char v[256];
                    eval_expr(cond, v, sizeof v);
                    int t = truthy(v);
                    if (kind == 2 && !t) break;
                    if (kind == 3 &&  t) break;
                }
                if (kind == 4) {
                    if (!down && cur > last) break;
                    if ( down && cur < last) break;
                    char v[64]; put_num(cur, v, sizeof v);
                    var_set(var, v);
                }
                if (++iter > HC_MAX_LOOP) {
                    emit(HC_ERR, "!! boucle interrompue après %d tours", HC_MAX_LOOP);
                    break;
                }

                exec_block(me, L, i + 1, e);
                host_idle();      /* l'hôte redessine et souffle */

                if (g_next_repeat) g_next_repeat = 0;
                if (g_exit_repeat) { g_exit_repeat = 0; break; }
                if (g_pass || g_exit_handler) break;

                if (kind == 4) cur += down ? -1 : 1;
            }
            i = e;
            if (flow_broken()) return;
            continue;
        }

        /* --- fermetures orphelines : on les laisse filer --- */
        if (ci_word(s, "end")) {
            const char *w = skip_spaces(s + 3);
            if (ci_word(w, "if") || ci_word(w, "repeat")) continue;
        }
        if (ci_word(s, "else")) continue;

        exec_stmt(me, s);
        if (flow_broken()) return;
    }
}

/* Découpe le corps d'un gestionnaire en lignes utiles, puis l'exécute. */
/* HyperCard autorise le « then » à ouvrir la ligne suivante :
 *
 *     if (theDayOfWeek - 1) <> 0
 *     then put char 1 to n of spaces() into theDayNumbers
 *
 * On recolle ces deux lignes avant toute analyse : exec_if ignore alors
 * complètement cette variante. Le recollage n'a lieu que si la ligne suivante
 * commence vraiment par « then », donc un `if` mal formé ne peut pas avaler
 * la suite du gestionnaire. */
static void join_split_then(char **L, int *n)
{
    for (int i = 0; i + 1 < *n; i++) {
        if (!ci_word(L[i], "if")) continue;
        if (find_kw(L[i], "then")) continue;
        if (!ci_word(L[i+1], "then")) continue;

        size_t need = strlen(L[i]) + strlen(L[i+1]) + 2;
        char *j = (char *)malloc(need);
        if (!j) return;
        snprintf(j, need, "%s %s", L[i], L[i+1]);
        free(L[i]); free(L[i+1]);
        L[i] = j;
        for (int k = i + 1; k + 1 < *n; k++) L[k] = L[k+1];
        (*n)--;
    }
}

static void exec_body(Object *me, const char *body, const char *end)
{
    int cap = 32, n = 0;
    char **L = malloc((size_t)cap * sizeof *L);
    if (!L) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }

    const char *p = body;
    while (p < end && *p) {
        const char *nl = strchr(p, '\n');
        const char *stop = (nl && nl < end) ? nl : end;
        int len = (int)(stop - p);

        char *line = arena_buf();
        if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
        memcpy(line, p, (size_t)len);
        line[len] = '\0';

        int L2 = (int)strlen(line);

        /* commentaire « -- » en fin de ligne, hors guillemets */
        int inq = 0;
        for (int k = 0; k < L2; k++) {
            if (line[k] == '"') inq = !inq;
            else if (!inq && line[k] == '-' && line[k+1] == '-') { line[k] = '\0'; L2 = k; break; }
        }

        while (L2 > 0 && (line[L2-1] == '\r' || line[L2-1] == ' ' || line[L2-1] == '\t'))
            line[--L2] = '\0';

        const char *t = skip_spaces(line);
        if (*t && !(t[0] == '-' && t[1] == '-')) {   /* -- commentaire */
            if (n == cap) {
                cap *= 2;
                char **q = realloc(L, (size_t)cap * sizeof *L);
                if (!q) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
                L = q;
            }
            L[n++] = dupstr(t);
        }

        if (!nl || nl >= end) break;
        p = nl + 1;
    }

    join_split_then(L, &n);
    exec_block(me, L, 0, n);

    for (int i = 0; i < n; i++) free(L[i]);
    free(L);
}

static void exec_line_body(Object *me, const char *line)
{
    (void)me;   /* servira pour `the target` / `me` dans les expressions */
    char verb[64];
    const char *rest = next_word(line, verb, sizeof verb);

    /* --- send "<message> [args]" to <objet> ---
     * Le message est une ligne de HyperTalk : un nom de gestionnaire suivi
     * d'arguments éventuels. « send "carre 6" to card X » appelle on carre
     * sur X avec l'argument 6. Les arguments sont évalués côté appelant. */
    if (ci_equal(verb, "send")) {
        char *msgline = arena_buf();
        rest = skip_spaces(rest);
        if (*rest == '"') {
            const char *after = quoted(rest, msgline, HC_VAL);
            char probe[8];
            next_word(after, probe, sizeof probe);
            if (ci_equal(probe, "to")) {
                rest = after;                       /* littéral simple */
            } else {
                /* Le message est une expression : « send "go " & n to … ».
                 * Elle court jusqu'au dernier « to » de premier niveau, les
                 * « to » enfermés dans les guillemets étant ignorés. */
                const char *last = NULL, *scan = rest;
                for (const char *k = find_kw(scan, "to"); k; k = find_kw(scan, "to")) {
                    last = k; scan = k + 2;
                }
                if (!last) { emit(HC_ERR, "?? send mal formé : %s", line); return; }
                char *expr = arena_buf();
                int n = (int)(last - rest);
                if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
                memcpy(expr, rest, (size_t)n); expr[n] = '\0';
                eval_expr(expr, msgline, HC_VAL);
                rest = last;
            }
        }
        else {
            /* message nu : tout jusqu'à « to » (au niveau supérieur) */
            const char *to_kw = find_kw(rest, "to");
            int len = to_kw ? (int)(to_kw - rest) : (int)strlen(rest);
            if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
            memcpy(msgline, rest, (size_t)len); msgline[len] = '\0';
            rest = to_kw ? to_kw : rest + strlen(rest);
        }

        char to[8];
        const char *r2 = next_word(rest, to, sizeof to);
        if (!ci_equal(to, "to")) { emit(HC_ERR, "?? send mal formé : %s", line); return; }

        Object *target = resolve(r2);
        if (!target) {
            set_result("destinataire introuvable");
            emit(HC_ERR, "   !! destinataire introuvable : %s", skip_spaces(r2));
            return;
        }

        /* découper le message en nom + arguments */
        char msg[128];
        const char *a = next_word(skip_spaces(msgline), msg, sizeof msg);
        char (*argv)[HC_VAL] = arena_rows(16);
        int argc = 0;
        a = skip_spaces(a);
        while (*a && argc < 16) {
            char *one = arena_buf();
            int len = 0, depth = 0, inq = 0;
            while (*a && !(depth == 0 && !inq && *a == ',')) {
                if (*a == '"') inq = !inq;
                else if (!inq && *a == '(') depth++;
                else if (!inq && *a == ')') depth--;
                if (len < 511) one[len++] = *a;
                a++;
            }
            one[len] = '\0';
            eval_expr(one, argv[argc], sizeof argv[argc]);   /* contexte appelant */
            argc++;
            if (*a == ',') a = skip_spaces(a + 1); else break;
        }

        set_result("");     /* un `return` dans le gestionnaire le remplira */
        hc_send_args(target, msg, argv, argc);
        return;
    }

    /* --- put <expr> [into|after|before <champ|variable>] --- */
    if (ci_equal(verb, "put")) {
        /* repérer la préposition, hors des chaînes entre guillemets */
        const char *kw = NULL;
        int kwlen = 0, mode = 0;    /* 0 = into (remplace), 1 = after, 2 = before */
        int inq = 0;
        for (const char *q = rest; *q; q++) {
            if (*q == '"') { inq = !inq; continue; }
            if (inq) continue;
            if (!(q == rest || isspace((unsigned char)q[-1]))) continue;
            if      (ci_word(q, "into"))   { kw = q; kwlen = 4; mode = 0; break; }
            else if (ci_word(q, "after"))  { kw = q; kwlen = 5; mode = 1; break; }
            else if (ci_word(q, "before")) { kw = q; kwlen = 6; mode = 2; break; }
        }

        char *val = arena_buf();
        if (!kw) {                       /* pas de destination : la boîte de message */
            eval_checked(rest, val, HC_VAL);
            emit(HC_MSG, "%s", val);
            return;
        }

        char *expr = arena_buf();
        int n = (int)(kw - rest);
        if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
        memcpy(expr, rest, (size_t)n); expr[n] = '\0';
        eval_checked(expr, val, HC_VAL);

        const char *dsttext = skip_spaces(kw + kwlen);
        if (container_set(dsttext, val, mode)) {
            set_result("");
            char *shown = arena_buf();
            eval_expr(dsttext, shown, HC_VAL);
            if (!*shown && *val) snprintf(shown, HC_VAL, "%s", val);
            emit(HC_INFO, "   → %s ← \"%s\"", dsttext, shown);
        } else {
            set_result("destination invalide");
            emit(HC_ERR, "   !! destination invalide : %s", dsttext);
        }
        return;
    }

    /* --- delete <morceau> of <conteneur> --- */
    if (ci_equal(verb, "delete")) {
        const char *d = skip_spaces(rest);
        if (container_set(d, "", 3)) {
            set_result("");
            emit(HC_INFO, "   → supprimé : %s", d);
        } else {
            set_result("rien à supprimer");
            emit(HC_ERR, "   !! rien à supprimer : %s", d);
        }
        return;
    }

    /* --- lock screen | unlock screen [with visual …] ---
     * Ce sont les alias de « set lockScreen to true/false », et c'est bien
     * ainsi qu'on les traite : l'hôte reçoit une propriété globale et décide
     * seul de suspendre ou non son rafraîchissement. Le noyau, lui, ne sait
     * toujours pas ce qu'est un écran. L'effet visuel éventuel est ignoré. */
    if (ci_equal(verb, "lock") || ci_equal(verb, "unlock")) {
        if (ci_word(skip_spaces(rest), "screen")) {
            host_global_set("lockScreen", ci_equal(verb, "lock") ? "true" : "false");
            set_result("");
            return;
        }
    }

    /* --- convert <conteneur> to <format> [and <format>] ---
     * Le résultat retourne dans le conteneur d'origine ; si la source n'en
     * est pas un (« convert the date to dateItems »), il atterrit dans `it`,
     * comme sur Macintosh. En cas d'échec, `the result` vaut « invalid date »
     * — c'est ce que les scripts d'époque testent. */
    if (ci_equal(verb, "convert")) {
        /* Le « to » qui compte est le dernier de premier niveau : la source
         * peut en contenir un elle-même, comme dans
         * « convert item 1 to 7 of calData() to dateItems ». */
        const char *to = NULL, *scan = rest;
        for (const char *k = find_kw(scan, "to"); k; k = find_kw(scan, "to")) {
            to = k; scan = k + 2;
        }
        if (!to) {
            emit(HC_ERR, "   !! convert sans « to » : %s", skip_spaces(rest));
            set_result("invalid date");
            return;
        }

        char *src = arena_buf();
        int n = (int)(to - rest);
        if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
        memcpy(src, rest, (size_t)n); src[n] = '\0';
        while (n > 0 && isspace((unsigned char)src[n-1])) src[--n] = '\0';
        const char *srcp = skip_spaces(src);

        /* le format cible, éventuellement double : « short date and long time » */
        char spec[256];
        snprintf(spec, sizeof spec, "%s", skip_spaces(to + 2));
        char *andkw = (char *)find_kw(spec, "and");
        int f1 = DF_NONE, f2 = DF_NONE;
        if (andkw) { *andkw = '\0'; f2 = date_format_code(andkw + 4); }
        f1 = date_format_code(spec);
        if (f1 == DF_NONE) {
            emit(HC_ERR, "   !! format de date inconnu : %s", skip_spaces(to + 2));
            set_result("invalid date");
            return;
        }

        char *val = arena_buf();
        eval_expr(srcp, val, HC_VAL);

        struct tm tm;
        if (!parse_datetime(val, &tm)) {
            emit(HC_ERR, "   !! date incomprise : « %s »", val);
            set_result("invalid date");
            return;
        }

        char *outv = arena_buf();
        char part2[256];
        emit_datetime(&tm, f1, outv, HC_VAL);
        if (f2 != DF_NONE) {
            emit_datetime(&tm, f2, part2, sizeof part2);
            int L = (int)strlen(outv);
            snprintf(outv + L, HC_VAL - L, " %s", part2);
        }

        /* Destination : le conteneur source s'il en est un. « the date »,
         * « the long time » et les appels de fonction n'en sont pas. */
        int to_it = ci_word(srcp, "the") || strchr(srcp, '(') != NULL;
        if (to_it || !container_set(srcp, outv, 0))
            var_set("it", outv);

        set_result("");
        emit(HC_INFO, "   → %s", outv);
        return;
    }

    /* --- return <expr> : dépose une valeur dans `the result` et sort --- */
    if (ci_equal(verb, "return")) {
        char *val = arena_buf();
        eval_checked(rest, val, HC_VAL);
        set_result(val);
        g_exit_handler = 1;
        return;
    }

    /* --- exit repeat | exit <gestionnaire> | next repeat --- */
    if (ci_equal(verb, "exit")) {
        if (ci_word(skip_spaces(rest), "repeat")) g_exit_repeat = 1;
        else                                      g_exit_handler = 1;
        return;
    }
    if (ci_equal(verb, "next")) {
        if (ci_word(skip_spaces(rest), "repeat")) g_next_repeat = 1;
        return;
    }

    /* --- set [the] <propriété> of <objet> to <expr> --- */
    if (ci_equal(verb, "set")) {
        const char *s = skip_spaces(rest);
        if (ci_word(s, "the")) s = skip_spaces(s + 3);

        char prop[64];
        const char *q = next_word(s, prop, sizeof prop);
        q = skip_spaces(q);

        /* Pas de « of » : c'est une propriété globale, pas celle d'un objet.
         * « set cursor to none », « set lockScreen to true »… */
        if (!ci_word(q, "of")) {
            /* « set word 2 of field "x" to bold » : le nom de propriete manque.
             * Sans ce garde-fou, « word » partait comme propriete globale chez
             * l'hote et l'ecriture disparaissait sans un mot. */
            if (ci_equal(prop, "char") || ci_equal(prop, "character")
             || ci_equal(prop, "word") || ci_equal(prop, "item")
             || ci_equal(prop, "line")) {
                set_result("propriete manquante");
                emit(HC_ERR, "   !! set : quelle propriete ? essayez "
                             "« set the textStyle of %s … »", skip_spaces(rest));
                return;
            }
            const char *to = find_kw(s, "to");
            if (!to) {
                emit(HC_ERR, "   !! set mal formé : %s", skip_spaces(rest));
                return;
            }
            char *val = arena_buf();
            eval_checked(to + 2, val, HC_VAL);
            host_global_set(prop, val);
            set_result("");
            emit(HC_INFO, "   → %s ← \"%s\"", prop, val);
            return;
        }
        q = skip_spaces(q + 2);

        /* Le « to » qui separe la cible de la valeur est le DERNIER de premier
         * niveau : la cible peut en contenir elle-meme, comme dans
         *     set the textStyle of word d of line 3 to numLines of me to bold
         * Couper au premier tronquait la cible a « line 3 ». */
        const char *to = NULL, *scan = q;
        for (const char *k = find_kw(scan, "to"); k; k = find_kw(scan, "to")) {
            to = k; scan = k + 2;
        }
        if (!to) {
            emit(HC_ERR, "   !! set sans « to » : %s", skip_spaces(rest));
            return;
        }

        char refbuf[256];
        int n = (int)(to - q);
        if (n > (int)sizeof refbuf - 1) n = (int)sizeof refbuf - 1;
        memcpy(refbuf, q, (size_t)n); refbuf[n] = '\0';
        while (n > 0 && (refbuf[n-1] == ' ' || refbuf[n-1] == '\t')) refbuf[--n] = '\0';

        char *val = arena_buf();
        /* « to bold,condense » n'est pas une expression : c'est une liste de
         * noms de styles, qu'HyperCard accepte sans guillemets. On prend donc
         * le texte brut, sauf s'il nomme une variable — auquel cas on lit sa
         * valeur, pour que « set the textStyle of X to myStyle » marche. */
        if (ci_equal(prop, "textstyle")) {
            const char *raw = skip_spaces(to + 2);
            int rl = (int)strlen(raw);
            while (rl > 0 && isspace((unsigned char)raw[rl-1])) rl--;
            snprintf(val, HC_VAL, "%.*s", rl, raw);

            /* Trancher sur le CONTENU, pas sur la ponctuation. L'ancien test
             * — « ni virgule ni espace, alors peut-être une variable » —
             * classait « s & ",italic" » parmi les listes de noms : il n'en
             * retenait que « italic » et jetait le reste, si bien qu'on ne
             * pouvait pas relire un style pour lui en ajouter un. Désormais
             * une suite de noms de style reste littérale, et tout le reste
             * est évalué comme l'expression que c'est. */
            if (!style_is_names(val)) {
                int n = (int)strlen(val);
                int quoted = 0;

                /* Une liste de noms peut arriver citée : « to "bold,italic" ».
                 * On la déguillemete pour la re-tester. L'ancienne écriture
                 * glissait le snprintf dans la condition via l'opérateur
                 * virgule, ce que le compilateur signale à juste titre : c'est
                 * le motif habituel d'un « f(a, b) » mal parenthésé. */
                if (n > 1 && val[0] == '"' && val[n-1] == '"') {
                    char *inner = arena_buf();
                    snprintf(inner, HC_VAL, "%.*s", n - 2, val + 1);
                    if (style_is_names(inner)) {
                        snprintf(val, HC_VAL, "%s", inner);
                        quoted = 1;
                    }
                }
                if (!quoted) eval_checked(to + 2, val, HC_VAL);
            }
        } else {
            eval_checked(to + 2, val, HC_VAL);
        }

        /* La cible peut designer une PLAGE DE TEXTE et non un objet :
         *     set the textStyle of word 3 of line 2 of field "cal" to bold
         * resolve() ne sait chercher que des objets, d'ou l'echec historique
         * « objet introuvable : word theNewDay of line 3 ». On essaie donc
         * d'abord le morceau, avant de retomber sur la resolution d'objet. */
        {
            int cst, cen;
            Object *cf = chunk_target(refbuf, &cst, &cen);
            if (cf) {
                /* Les trois attributs de texte se posent par plage, comme dans
                 * HyperCard 2.x ; le reste (rect, visible…) décrit un objet et
                 * n'a aucun sens sur un morceau. */
                int mask = 0;
                if      (ci_equal(prop, "textstyle")) mask = RA_STYLE;
                else if (ci_equal(prop, "textfont"))  mask = RA_FONT;
                else if (ci_equal(prop, "textsize") ||
                         ci_equal(prop, "textheight")) mask = RA_SIZE;

                if (!mask) {
                    /* Deux échecs bien différents, qu'un seul message
                     * confondait : « font » n'est pas une propriété du tout
                     * (c'est « textFont »), alors que « rect » en est une,
                     * mais qui décrit un objet et non un morceau. Annoncer
                     * « ne s'applique pas à un morceau » sur un nom inconnu
                     * envoyait chercher l'erreur au mauvais endroit. */
                    if (!is_prop_name(prop, (int)strlen(prop))) {
                        set_result("propriete inconnue");
                        emit(HC_ERR, "   !! propriete inconnue : %s"
                                     " (les proprietes de texte sont textFont,"
                                     " textSize, textStyle)", prop);
                    } else {
                        set_result("propriete non applicable a un morceau");
                        emit(HC_ERR, "   !! %s decrit un objet, pas un morceau"
                                     " de texte", prop);
                    }
                    return;
                }
                struct RunList *rl = runs_of(cf);
                if (!rl) {
                    set_result("champ sans stockage de style");
                    emit(HC_ERR, "   !! ce champ n'a pas encore de texte sur cette carte");
                    return;
                }
                runs_set_attr(rl, cst, cen - cst, mask,
                              (mask & RA_STYLE) ? style_from_names(val) : 0,
                              (mask & RA_SIZE)  ? atoi(val) : 0,
                              (mask & RA_FONT)  ? val : NULL);
                notify_field(cf);
                set_result("");
                emit(HC_INFO, "   → %s de [%d..%d[ ← \"%s\"", prop, cst, cen, val);
                return;
            }
        }

        Object *o = resolve(refbuf);
        if (!o) {
            /* Distinguer les deux echecs : une reference d'objet inconnue, ou
             * un morceau de texte qui n'existe pas (champ vide, rang au-dela
             * du contenu). « objet introuvable » sur « word 8 of line 3 of me »
             * envoyait chercher au mauvais endroit. */
            ChunkType xt; char xa[128], xb[128]; const char *xr; int xo;
            if (parse_chunk(refbuf, &xt, xa, sizeof xa, xb, sizeof xb, &xr, &xo)) {
                set_result("morceau hors limites");
                emit(HC_ERR, "   !! morceau hors limites : %s", refbuf);
            } else {
                set_result("objet introuvable");
                emit(HC_ERR, "   !! objet introuvable : %s", refbuf);
            }
            return;
        }

        char d[64]; hc_describe(o, d, sizeof d);   /* avant modification */

        if (ci_equal(prop, "name")) {
            free(o->name);
            o->name = dupstr(val);
        } else if (ci_equal(prop, "visible")) {
            o->visible = truthy(val);
        } else if (ci_equal(prop, "showname") || ci_equal(prop, "shownname")) {
            o->showname = truthy(val);
            notify_field(o);
        } else if (ci_equal(prop, "icon")) {
            o->icon = atoi(val);
            notify_field(o);
        } else if (ci_equal(prop, "selectedline") || ci_equal(prop, "selectedlines")) {
            o->selectedline = atoi(val);
            notify_field(o);
        } else if (ci_equal(prop, "locktext")) {
            o->locktext = truthy(val); notify_field(o);
        } else if (ci_equal(prop, "widemargins")) {
            o->wide_margins = truthy(val); notify_field(o);
        } else if (ci_equal(prop, "fixedlineheight")) {
            o->fixed_lh = truthy(val); notify_field(o);
        } else if (ci_equal(prop, "showlines")) {
            o->show_lines = truthy(val); notify_field(o);
        } else if (ci_equal(prop, "autotab")) {
            o->auto_tab = truthy(val); notify_field(o);
        } else if (ci_equal(prop, "dontsearch")) {
            o->dont_search = truthy(val); notify_field(o);
        } else if (ci_equal(prop, "sharedtext")) {
            o->shared_text = truthy(val); notify_field(o);
        } else if (ci_equal(prop, "scroll")) {
            o->scroll = atoi(val);
            if (o->scroll < 0) o->scroll = 0;
            notify_field(o);
        } else if (ci_equal(prop, "textfont")) {
            free(o->textfont);
            o->textfont = (*val) ? dupstr(val) : NULL;
            notify_field(o);
        } else if (ci_equal(prop, "textstyle")) {
            /* Passe par style_from_names, comme la pose sur un morceau. Le
             * code d'origine cherchait trois sous-chaînes à la main : il
             * ignorait outline, shadow, condense, extend et group — d'où un
             * « to bold,outline » sur un bouton qui se relisait « bold » —
             * et un mot comme « underlined » l'aurait déclenché à tort. */
            o->textstyle = style_from_names(val);
            notify_field(o);
        } else if (ci_equal(prop, "hilite") || ci_equal(prop, "highlight")) {
            o->hilite = truthy(val);
            notify_field(o);
        } else if (ci_equal(prop, "autohilite")) {
            o->autohilite = truthy(val);
        } else if (ci_equal(prop, "textsize") || ci_equal(prop, "textheight")) {
            o->textsize = atoi(val);
            notify_field(o);
        } else if (ci_equal(prop, "script")) {
            /* Une valeur qui touche exactement le plafond a presque
             * certainement été tronquée en chemin (« get the script of me »
             * sur un script plus long que HC_VAL). L'écrire telle quelle
             * détruirait le script. On refuse : mieux vaut un gestionnaire
             * qui échoue qu'une pile mutilée.
             *
             * Un script faisant pile HC_VAL-1 octets serait refusé à tort ;
             * c'est un prix négligeable face à la perte de l'original. */
            if (g_script_clipped || (int)strlen(val) >= HC_VAL - 1) {
                emit(HC_ERR, "   !! écriture refusée : ce gestionnaire a lu un "
                             "script tronqué ; l'écrire le détruirait");
                set_result("script tronqué");
                return;
            }
            hc_set_script(o, val);
        } else if (ci_equal(prop, "style")) {
            free(o->style);
            o->style = dupstr(val);
        } else if (ci_equal(prop, "text") || ci_equal(prop, "contents")) {
            if (o->type != OBJ_FIELD && o->type != OBJ_BUTTON) {
                emit(HC_ERR, "   !! seul un champ ou un bouton a un contenu");
                return;
            }
            hc_set_field_text(o, val);
            notify_field(o);
        } else if (geom_write(o, prop, val)) {
            notify_field(o);
        } else {
            set_result("propriété inconnue");
            emit(HC_ERR, "   !! propriété inconnue : %s", prop);
            return;
        }

        set_result("");
        emit(HC_INFO, "   → %s de %s ← \"%s\"", prop, d, val);
        return;
    }

    /* --- do <expr> : exécute du texte comme une instruction ---
     * Le pendant de value() : celle-ci rend du calcul, celle-là de l'action.
     * Un compteur évite qu'un « do » qui se relance ne mange la pile C. */
    if (ci_equal(verb, "do")) {
        static int do_depth = 0;
        if (do_depth >= 16) {
            emit(HC_ERR, "   !! do : trop d'imbrications");
            return;
        }
        char *val = arena_buf();
        eval_checked(rest, val, HC_VAL);

        do_depth++;
        /* plusieurs lignes : on les exécute l'une après l'autre */
        char *copie = dupstr(val), *p = copie;
        while (p && *p) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            const char *ligne = skip_spaces(p);
            if (*ligne && !(ligne[0] == '-' && ligne[1] == '-')) exec_stmt(me, ligne);
            if (!nl || flow_broken()) break;
            p = nl + 1;
        }
        free(copie);
        do_depth--;
        return;
    }

    /* --- get <expr> : le résultat va dans `it` --- */
    if (ci_equal(verb, "get")) {
        char *val = arena_buf();
        eval_checked(rest, val, HC_VAL);
        var_set("it", val);
        emit(HC_INFO, "   → it ← \"%s\"", val);
        return;
    }

    /* --- global a, b, c --- */
    if (ci_equal(verb, "global")) {
        const char *q = skip_spaces(rest);
        while (*q) {
            char nm[128];
            int i = 0;
            while (*q && *q != ',' && !isspace((unsigned char)*q) && i < (int)sizeof nm - 1)
                nm[i++] = *q++;
            nm[i] = '\0';
            if (nm[0]) frame_declare_global(g_frame, nm);
            while (*q == ',' || *q == ' ' || *q == '\t') q++;
        }
        return;
    }

    /* --- add / subtract / multiply / divide ---
     *   add 1 to i            subtract 2 from x
     *   multiply x by 3       divide t by 2
     * Le conteneur peut etre une variable, un champ ou un chunk. */
    if (ci_equal(verb, "add") || ci_equal(verb, "subtract") ||
        ci_equal(verb, "multiply") || ci_equal(verb, "divide")) {

        const char *sep = NULL; int seplen = 0;
        if      (ci_equal(verb, "add"))      { sep = "to";   seplen = 2; }
        else if (ci_equal(verb, "subtract")) { sep = "from"; seplen = 4; }
        else                                  { sep = "by";   seplen = 2; }

        const char *r = skip_spaces(rest);
        const char *kw = NULL;
        int inq = 0;
        for (const char *q = r; *q; q++) {
            if (*q == '"') { inq = !inq; continue; }
            if (inq) continue;
            if ((q == r || isspace((unsigned char)q[-1])) && ci_word(q, sep)) { kw = q; break; }
        }
        if (!kw) {
            emit(HC_ERR, "   !! syntaxe : %s <valeur> %s <conteneur>", verb, sep);
            set_result("syntax error");
            return;
        }

        /* add/subtract : <valeur> to|from <conteneur>
         * multiply/divide : <conteneur> by <valeur>   (ordre inverse) */
        int valueFirst = (ci_equal(verb, "add") || ci_equal(verb, "subtract"));

        char *left = arena_buf();
        char amount[128];
        char *cur = arena_buf();
        int n = (int)(kw - r);
        if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
        memcpy(left, r, n); left[n] = 0;
        /* retirer les espaces de fin */
        for (int k = (int)strlen(left) - 1; k >= 0 && isspace((unsigned char)left[k]); k--)
            left[k] = 0;

        const char *right = skip_spaces(kw + seplen);
        const char *dst;
        if (valueFirst) {
            eval_expr(left, amount, sizeof amount);
            dst = right;
        } else {
            eval_expr(right, amount, sizeof amount);
            dst = left;
        }
        eval_expr(dst, cur, HC_VAL);

        double a = 0, b = 0;
        as_num(cur, &a);
        as_num(amount, &b);
        double res = a;
        if      (ci_equal(verb, "add"))      res = a + b;
        else if (ci_equal(verb, "subtract")) res = a - b;
        else if (ci_equal(verb, "multiply")) res = a * b;
        else {
            if (b == 0) { emit(HC_ERR, "   !! division par zero"); set_result("divide by zero"); return; }
            res = a / b;
        }

        char out[64];
        put_num(res, out, sizeof out);
        if (!container_set(dst, out, 0)) {
            emit(HC_ERR, "   !! destination inconnue : %s", dst);
            set_result("no such container");
            return;
        }
        set_result("");
        return;
    }

    /* --- find [string|word|chars|whole] "texte" [in <champ>] ---
     * Parcourt les cartes à partir de la courante, en bouclant. Ignore les
     * champs marqués dontSearch. Le mode par défaut d'HyperTalk cherche un
     * mot commençant par le motif. */
    if (ci_equal(verb, "find")) {
        const char *r = skip_spaces(rest);
        int mode = 0;                  /* 0 = debut de mot, 1 = n'importe ou, 2 = mot entier */
        if      (ci_word(r, "string") || ci_word(r, "chars")) { mode = 1; r = skip_spaces(r + 6); }
        else if (ci_word(r, "whole"))  { mode = 1; r = skip_spaces(r + 5); }
        else if (ci_word(r, "word"))   { mode = 2; r = skip_spaces(r + 4); }

        /* separer le motif de l'eventuel « in <champ> » */
        const char *kw = NULL;
        int inq = 0;
        for (const char *q = r; *q; q++) {
            if (*q == '"') { inq = !inq; continue; }
            if (inq) continue;
            if ((q == r || isspace((unsigned char)q[-1])) && ci_word(q, "in")) { kw = q; break; }
        }
        char pat[256] = "", where[128] = "";
        if (kw) {
            char e[256];
            int n = (int)(kw - r);
            if (n > (int)sizeof e - 1) n = (int)sizeof e - 1;
            memcpy(e, r, n); e[n] = 0;
            eval_expr(e, pat, sizeof pat);
            snprintf(where, sizeof where, "%s", skip_spaces(kw + 2));
        } else {
            eval_expr(r, pat, sizeof pat);
        }
        if (!pat[0]) { set_result("not found"); return; }

        Object *stack = g_current_card ? g_current_card->owner : NULL;
        while (stack && stack->type != OBJ_STACK) stack = stack->owner;
        if (!stack) { set_result("not found"); return; }

        int total = card_count(stack);
        int start = card_index(stack, g_current_card);
        if (start < 0) start = 0;

        for (int k = 0; k < total; k++) {
            Object *cd = nth_card(stack, (start + k) % total);
            if (!cd) continue;

            /* champs de la carte puis du fond */
            Object *layers[2] = { cd, cd->bg };
            for (int L = 0; L < 2; L++) {
                Object *lay = layers[L];
                if (!lay) continue;
                for (int i = 0; i < lay->nparts; i++) {
                    Object *fl = lay->parts[i];
                    if (fl->type != OBJ_FIELD || fl->dont_search) continue;
                    if (where[0]) {          /* recherche restreinte a un champ */
                        Object *only = resolve(where);
                        if (only != fl) continue;
                    }

                    Object *saved = g_current_card;
                    g_current_card = cd;                 /* pour le texte par carte */
                    const char *tx = hc_field_text(fl);
                    const char *hit = NULL;
                    int line = 1;

                    for (const char *q = tx; *q; q++) {
                        if (*q == '\n') { line++; continue; }
                        int atword = (q == tx) || isspace((unsigned char)q[-1]);
                        if (mode == 1 || atword) {
                            size_t plen = strlen(pat);
                            if (strncasecmp(q, pat, plen) == 0) {
                                if (mode == 2) {         /* mot entier */
                                    char nx = q[plen];
                                    if (nx && !isspace((unsigned char)nx) &&
                                        !ispunct((unsigned char)nx)) continue;
                                }
                                hit = q;
                                break;
                            }
                        }
                    }
                    g_current_card = saved;

                    if (hit) {
                        snprintf(g_found_text, sizeof g_found_text, "%s", pat);
                        g_found_field = fl;
                        g_found_line = line;
                        g_found_start = (int)(hit - tx);
                        g_found_len   = (int)strlen(pat);
                        g_found_card  = cd;

                        if (cd != g_current_card) {      /* naviguer si besoin */
                            Object *old = g_current_card;
                            Object *oldbg = old ? old->bg : NULL;
                            if (old) hc_send(old, "closeCard");
                            if (oldbg && oldbg != cd->bg) hc_send(oldbg, "closeBackground");
                            g_current_card = cd;
                            if (cd->bg && cd->bg != oldbg) hc_send(cd->bg, "openBackground");
                            hc_send(cd, "openCard");
                        }
                        set_result("");
                        emit(HC_INFO, "   ⇒ trouvé \"%s\" dans la carte \"%s\"",
                             pat, cd->name ? cd->name : "?");
                        return;
                    }
                }
            }
        }
        g_found_text[0] = 0; g_found_field = NULL; g_found_line = 0;
        g_found_start = g_found_len = 0; g_found_card = NULL;
        set_result("not found");
        return;
    }

    /* --- ask "invite" [with "défaut"] --- */
    if (ci_equal(verb, "ask")) {
        const char *r = skip_spaces(rest);
        char *prompt = arena_buf();
        char *deflt = arena_buf();

        /* repérer « with » hors des chaînes */
        const char *kw = NULL;
        int inq = 0;
        for (const char *q = r; *q; q++) {
            if (*q == '"') { inq = !inq; continue; }
            if (inq) continue;
            if ((q == r || isspace((unsigned char)q[-1])) && ci_word(q, "with")) { kw = q; break; }
        }
        if (kw) {
            char *expr = arena_buf();
            int n = (int)(kw - r);
            if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
            memcpy(expr, r, n); expr[n] = '\0';
            eval_expr(expr, prompt, HC_VAL);
            eval_expr(skip_spaces(kw + 4), deflt, HC_VAL);
        } else {
            eval_expr(r, prompt, HC_VAL);
        }

        const char *rep = (g_host && g_host->ask) ? g_host->ask(prompt, deflt) : NULL;
        if (rep) { var_set("it", rep); set_result(""); }
        else     { var_set("it", "");  set_result("Cancel"); }
        return;
    }

    /* --- answer "invite" [with "a" [or "b" [or "c"]]] --- */
    if (ci_equal(verb, "answer")) {
        const char *r = skip_spaces(rest);
        char *prompt = arena_buf();
        char btn[3][128] = { "", "", "" };
        int nb = 0;

        const char *kw = NULL;
        int inq = 0;
        for (const char *q = r; *q; q++) {
            if (*q == '"') { inq = !inq; continue; }
            if (inq) continue;
            if ((q == r || isspace((unsigned char)q[-1])) && ci_word(q, "with")) { kw = q; break; }
        }
        if (kw) {
            char *expr = arena_buf();
            int n = (int)(kw - r);
            if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
            memcpy(expr, r, n); expr[n] = '\0';
            eval_expr(expr, prompt, HC_VAL);

            /* découper sur « or », hors des chaînes */
            const char *p2 = skip_spaces(kw + 4);
            while (*p2 && nb < 3) {
                const char *cut = NULL;
                inq = 0;
                for (const char *q = p2; *q; q++) {
                    if (*q == '"') { inq = !inq; continue; }
                    if (inq) continue;
                    if ((q == p2 || isspace((unsigned char)q[-1])) && ci_word(q, "or")) { cut = q; break; }
                }
                char one[256];
                int len = cut ? (int)(cut - p2) : (int)strlen(p2);
                if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
                memcpy(one, p2, len); one[len] = '\0';
                eval_expr(one, btn[nb], sizeof btn[nb]);
                nb++;
                if (!cut) break;
                p2 = skip_spaces(cut + 2);
            }
        } else {
            eval_expr(r, prompt, HC_VAL);
        }
        if (nb == 0) { snprintf(btn[0], sizeof btn[0], "OK"); nb = 1; }

        const char *rep = (g_host && g_host->answer)
            ? g_host->answer(prompt, btn[0], nb > 1 ? btn[1] : NULL,
                                              nb > 2 ? btn[2] : NULL)
            : btn[0];
        var_set("it", rep ? rep : "");
        set_result("");
        return;
    }

    /* --- push [card] : mémorise la carte courante --- */
    if (ci_equal(verb, "push")) {
        const char *r = skip_spaces(rest);
        Object *dst = *r ? resolve(r) : g_current_card;
        if (!dst || dst->type != OBJ_CARD) dst = g_current_card;
        if (!dst) { set_result("aucune carte a empiler"); return; }
        if (g_navtop >= NAVSTACK_MAX) {          /* pile pleine : on decale */
            for (int i = 1; i < NAVSTACK_MAX; i++) g_navstack[i-1] = g_navstack[i];
            g_navtop = NAVSTACK_MAX - 1;
        }
        g_navstack[g_navtop++] = dst;
        set_result("");
        emit(HC_INFO, "   ⇒ empile la carte \"%s\"", dst->name ? dst->name : "?");
        return;
    }

    /* --- pop [card] [into conteneur] : depile et y va --- */
    if (ci_equal(verb, "pop")) {
        if (g_navtop <= 0) { set_result("pile de navigation vide"); return; }
        Object *dst = g_navstack[--g_navtop];
        const char *r = skip_spaces(rest);
        if (ci_word(r, "card") || ci_word(r, "cd")) r = skip_spaces(r + (ci_word(r,"cd") ? 2 : 4));

        if (ci_word(r, "into")) {                /* pop cd into x : sans y aller */
            char cmd[320];
            snprintf(cmd, sizeof cmd, "put \"card id %d\" into %s",
                     dst->id, skip_spaces(r + 4));
            exec_line(me, cmd);
            set_result("");
            return;
        }

        Object *old = g_current_card;
        Object *oldbg = old ? old->bg : NULL;
        Object *newbg = dst->bg;
        if (old) hc_send(old, "closeCard");
        if (oldbg && oldbg != newbg) hc_send(oldbg, "closeBackground");
        g_current_card = dst;
        if (newbg && newbg != oldbg) hc_send(newbg, "openBackground");
        hc_send(dst, "openCard");
        set_result("");
        emit(HC_INFO, "   ⇒ depile vers \"%s\"", dst->name ? dst->name : "?");
        return;
    }

    /* --- go [to] card "nom" | next | previous | first | last | card 3 --- */
    if (ci_equal(verb, "go")) {
        const char *r = skip_spaces(rest);
        if (ci_word(r, "to")) r = skip_spaces(r + 2);
        Object *dst = resolve(r);
        if (!dst) {                          /* « go x » : evaluer d'abord */
            char v[256];
            eval_expr(r, v, sizeof v);
            if (v[0] && strcmp(v, r) != 0) dst = resolve(v);
        }
        if (!dst && ci_word(r, "card")) {   /* « go card » nu : la première */
            const char *w = skip_spaces(r + 4);
            if (!*w) dst = resolve("first card");
        }
        /* « go background "x" » mène à la PREMIÈRE CARTE de ce fond, et
         * « go stack "x" » à la première carte de la pile : dans HyperCard
         * on ne se tient jamais sur un fond, seulement sur une carte. */
        if (dst && (dst->type == OBJ_BACKGROUND || dst->type == OBJ_STACK)) {
            Object *stk = owning_stack(dst);
            Object *found = NULL;
            for (int i = 0; stk && i < stk->nparts; i++) {
                Object *c = stk->parts[i];
                if (c->type != OBJ_CARD) continue;
                if (dst->type == OBJ_STACK || c->bg == dst) { found = c; break; }
            }
            if (!found)
                emit(HC_ERR, "   !! aucune carte dans %s", hc_typename(dst->type));
            dst = found;
        }

        if (dst && dst->type == OBJ_CARD) {
            set_result("");
            Object *old   = g_current_card;
            Object *oldbg = old ? old->bg : NULL;
            if (old) hc_send(old, "closeCard");
            /* Changement de fond : les quatre messages, dans l'ordre
             * d'HyperCard. « find » le faisait déjà, « go » l'oubliait. */
            if (oldbg && oldbg != dst->bg) hc_send(oldbg, "closeBackground");
            g_current_card = dst;
            if (dst->bg && dst->bg != oldbg) hc_send(dst->bg, "openBackground");
            emit(HC_INFO, "   ⇒ va à la carte \"%s\"", dst->name ? dst->name : "?");
            hc_send(dst, "openCard");
        } else {
            set_result("carte introuvable");
            emit(HC_ERR, "   !! carte introuvable : %s", r);
        }
        return;
    }

    /* --- show <objet> [at <point>] / hide <objet> ---
     * « show me at h,v » est la primitive d'animation d'HyperCard : elle
     * déplace l'objet ET le rend visible, le centre allant au point donné
     * (comme « the loc », pas comme « the topLeft »). */
    if (ci_equal(verb, "show") || ci_equal(verb, "hide")) {
        int showing = ci_equal(verb, "show");
        const char *at = showing ? find_kw(rest, "at") : NULL;

        char refbuf[256];
        const char *ref = rest;
        if (at) {
            int n = (int)(at - rest);
            if (n > (int)sizeof refbuf - 1) n = (int)sizeof refbuf - 1;
            memcpy(refbuf, rest, (size_t)n); refbuf[n] = '\0';
            while (n > 0 && isspace((unsigned char)refbuf[n-1])) refbuf[--n] = '\0';
            ref = refbuf;
        }

        Object *o = resolve(ref);
        if (!o) {
            set_result("objet introuvable");
            emit(HC_ERR, "   !! objet introuvable : %s", ref);
            return;
        }

        o->visible = showing;
        char d[64]; hc_describe(o, d, sizeof d);

        if (at) {
            char pt[256];
            eval_point(skip_spaces(at + 2), pt, sizeof pt);
            if (!geom_write(o, "loc", pt)) {
                emit(HC_ERR, "   !! show … at : point mal formé : %s", pt);
                return;
            }
            notify_field(o);
            set_result("");
            emit(HC_INFO, "   → %s : visible en %s", d, pt);
            return;
        }

        notify_field(o);
        set_result("");
        emit(HC_INFO, "   → %s : %s", d, showing ? "visible" : "caché");
        return;
    }

    /* --- play <son> ---
     * HyperCard accepte une suite de notes derrière le nom du son
     * (« play "boing" tempo 200 c4 e4 »). On ne retient que le nom : le reste
     * demande un synthétiseur, pas un lecteur d'échantillon. */
    if (ci_equal(verb, "play")) {
        char val[256];
        const char *s = skip_spaces(rest);
        if (*s == '"') quoted(s, val, HC_VAL);
        else           next_word(s, val, HC_VAL);
        if (g_host && g_host->play_sound) g_host->play_sound(val);
        set_result("");
        emit(HC_INFO, "   ♪ play \"%s\"", val);
        return;
    }

    /* --- pass <message> --- */
    if (ci_equal(verb, "pass")) {
        g_pass = 1;
        emit(HC_INFO, "   ↑ pass");
        return;
    }

    /* --- beep : utile pour tester --- */
    if (ci_equal(verb, "beep")) {
        emit(HC_INFO, "   ♪ beep");
        return;
    }

    /* --- appel direct d'un gestionnaire : « carre 7 » → on carre n ---
     * Si un objet de la chaîne courante définit « on <verb> », on le lui
     * envoie avec les arguments (séparés par des virgules) évalués. */
    {
        Object *start = g_me ? g_me : g_current_card;
        Object *chain[8];
        int nc = build_chain(start, chain, 8);
        for (int i = 0; i < nc; i++) {
            const char *end = NULL, *hdr = NULL;
            if (find_handler(chain[i]->script, verb, &end, &hdr)) {
                char (*argv)[HC_VAL] = arena_rows(16);
                int argc = 0;
                const char *a = skip_spaces(rest);
                while (*a && argc < 16) {
                    /* découpe au niveau des virgules de premier niveau */
                    char *one = arena_buf();
            int len = 0, depth = 0, inq = 0;
                    while (*a && !(depth == 0 && !inq && *a == ',')) {
                        if (*a == '"') inq = !inq;
                        else if (!inq && *a == '(') depth++;
                        else if (!inq && *a == ')') depth--;
                        if (len < 511) one[len++] = *a;
                        a++;
                    }
                    one[len] = '\0';
                    eval_expr(one, argv[argc], sizeof argv[argc]);
                    argc++;
                    if (*a == ',') a = skip_spaces(a + 1); else break;
                }
                set_result("");
                hc_send_args(start, verb, argv, argc);
                return;
            }
        }
    }

    emit(HC_ERR, "   ?? verbe inconnu : %s", verb);
}

/* Enveloppe de libération. exec_line a 57 points de sortie : les instrumenter
 * un par un serait une invitation à l'erreur. On renomme le corps et on pose la
 * marque autour — une seule fois, sans toucher à sa logique. Même recette pour
 * term_value et call_function, qui en ont 40 et 36. */
static void exec_line(Object *me, const char *line)
{
    ARENA_MARK;
    exec_line_body(me, line);
    ARENA_FREE;
}

/* ==================== envoi d'un message ==================== */

/* Envoie un message accompagné d'une liste d'arguments déjà évalués.
   argv[0..argc-1] sont les valeurs des arguments (sans le nom du message). */
static int hc_send_args_k_body(Object *target, const char *message,
                          char argv[][HC_VAL], int argc, int isfunc)
{
    if (g_depth >= HC_MAX_DEPTH) {
        emit(HC_ERR, "!! trop de récursion : message \"%s\" abandonné", message);
        return 0;
    }

    Object *chain[8];
    int n = build_chain(target, chain, 8);

    /* `the target` vaut le destinataire initial pendant toute la remontée ;
       on empile l'ancien pour les envois imbriqués. */
    int saved_clipped = g_script_clipped;
    g_script_clipped = 0;
    Object *saved_target = g_target;
    Object *saved_me     = g_me;
    g_target = target;

    /* on empile les paramètres du gestionnaire appelant */
    char (*saved_params)[HC_VAL] = arena_rows(16);
    int  saved_nparams = g_nparams;
    for (int i = 0; i < g_nparams; i++)
        memcpy(saved_params[i], g_params[i], sizeof saved_params[i]);

    /* g_params[0] = nom du message, puis les arguments */
    snprintf(g_params[0], sizeof g_params[0], "%s", message);
    g_nparams = 1;
    for (int i = 0; i < argc && g_nparams < 16; i++)
        snprintf(g_params[g_nparams++], sizeof g_params[0], "%s", argv[i]);

    if (g_trace) {
        char d[64]; hc_describe(target, d, sizeof d);
        emit(HC_TRACE, "→ message \"%s\" à %s", message, d);
    }

    g_depth++;

    int handled = 0;
    for (int i = 0; i < n; i++) {
        Object *o = chain[i];
        const char *end = NULL, *hdr = NULL;
        const char *body = find_handler_k(o->script, message, isfunc, &end, &hdr);

        if (body) {
            if (g_trace) {
                char d[64]; hc_describe(o, d, sizeof d);
                emit(HC_TRACE, "· traité par %s", d);
            }
            /* chaque gestionnaire a ses propres variables locales */
            Frame  frame;   memset(&frame, 0, sizeof frame);
            Frame *savedf = g_frame;
            g_frame = &frame;

            /* lier les paramètres formels de l'en-tête aux arguments :
               « on carre n » → la variable locale n reçoit argv[0]. */
            if (hdr) {
                char pname[64];
                /* sauter le mot-clé (« on » / « function ») puis le nom :
                 * un décalage fixe casserait dès qu'on change de mot-clé. */
                const char *q = next_word(hdr, pname, sizeof pname);
                q = next_word(q, pname, sizeof pname);
                int idx = 0;
                for (;;) {
                    q = skip_spaces(q);
                    if (*q == ',') { q++; continue; }
                    if (!*q || *q == '\n') break;
                    /* Lire le nom sans next_word : celui-ci ne s'arrête qu'aux
                     * blancs et avalerait la virgule dans « on markToday
                     * theNewDay,theOldDay » — forme sans espace qu'emploient
                     * tous les scripts d'origine. Aucun paramètre n'était
                     * alors lié. */
                    int pk = 0;
                    while (*q && *q != ',' && *q != '\n' && !isspace((unsigned char)*q)
                           && pk < (int)sizeof pname - 1)
                        pname[pk++] = *q++;
                    pname[pk] = '\0';
                    if (!pname[0]) break;
                    var_set(pname, idx < argc ? argv[idx] : "");
                    idx++;
                }
            }

            g_pass = 0;
            g_exit_handler = g_exit_repeat = g_next_repeat = 0;
            g_me   = o;          /* `me` = l'objet dont le script tourne */
            exec_body(o, body, end);
            g_exit_handler = g_exit_repeat = g_next_repeat = 0;

            g_frame = savedf;
            frame_clear(&frame);
            handled = 1;
            if (g_pass) { g_pass = 0; continue; }   /* pass : on remonte */
            break;
        } else if (g_trace) {
            char d[64]; hc_describe(o, d, sizeof d);
            emit(HC_TRACE, "  (pas de gestionnaire dans %s)", d);
        }
    }

    if (!handled && g_trace) {
        /* Muet pour une recherche de fonction : ne pas trouver « numLines »
         * est le cas normal quand le nom est en fait une variable ou un
         * littéral. Seul un message resté sans preneur mérite la trace. */
        if (!isfunc)
            emit(HC_TRACE, "  ✗ message \"%s\" non traité", message);
    }

    g_depth--;
    g_me     = saved_me;
    g_target = saved_target;
    g_script_clipped = saved_clipped;

    /* dépiler les paramètres de l'appelant */
    for (int i = 0; i < saved_nparams; i++)
        memcpy(g_params[i], saved_params[i], sizeof g_params[i]);
    g_nparams = saved_nparams;

    return handled;
}

/* Frontière de message : chaque envoi rend ses tampons en sortant. Sans cela
 * une pile qui envoie des milliers de messages verrait l'arène croître sans
 * fin, puisque seuls parse_factor et exec_stmt libèrent en dessous. */
static int hc_send_args_k(Object *target, const char *message,
                          char argv[][HC_VAL], int argc, int isfunc)
{
    ARENA_MARK;
    int r = hc_send_args_k_body(target, message, argv, argc, isfunc);
    ARENA_FREE;
    return r;
}

static int hc_send_args(Object *target, const char *message,
                        char argv[][HC_VAL], int argc)
{
    return hc_send_args_k(target, message, argv, argc, 0);
}

/* Appel d'une fonction utilisateur : même remontée de la chaîne, mais on
 * cherche « function <nom> ». La valeur est déposée dans `the result` par
 * l'instruction `return` ; on la vide d'abord pour qu'une fonction sans
 * `return` rende bien la chaîne vide plutôt que le reliquat de l'appel
 * précédent. */
static int hc_call_user_function(Object *target, const char *name,
                                 char argv[][HC_VAL], int argc)
{
    if (!target) return 0;
    set_result("");
    return hc_send_args_k(target, name, argv, argc, 1);
}

int hc_send(Object *target, const char *message)
{
    ARENA_MARK;
    char (*none)[HC_VAL] = arena_rows(1);
    int r = hc_send_args(target, message, none, 0);
    ARENA_FREE;
    return r;
}

/* ==================== boîte de message ==================== */

Object *hc_resolve(const char *ref) { return resolve(ref); }

const char *hc_script_of(Object *o) { return o ? o->script : NULL; }

/* Un champ de fond non partagé a un texte propre à chaque carte. */
static int field_is_percard(Object *field)
{
    return field && field->type == OBJ_FIELD
        && field->owner && field->owner->type == OBJ_BACKGROUND
        && !field->shared_text;
}

/* Plage du dernier « find » : renvoie 1 si ce champ, sur cette carte, porte
 * le texte trouve, et remplit start et len. */
int hc_found_range(Object *field, int *start, int *len)
{
    if (!field || field != g_found_field || g_found_len <= 0) return 0;
    if (g_found_card && g_found_card != g_current_card) return 0;
    if (start) *start = g_found_start;
    if (len)   *len   = g_found_len;
    return 1;
}

const char *hc_field_text(Object *field)
{
    if (!field) return "";
    if (field_is_percard(field) && g_current_card) {
        Object *cd = g_current_card;
        for (int i = 0; i < cd->nbgtexts; i++)
            if (cd->bgtexts[i].field_id == field->id)
                return cd->bgtexts[i].text ? cd->bgtexts[i].text : "";
        return "";                       /* pas encore rempli sur cette carte */
    }
    return field->contents ? field->contents : "";
}

void hc_set_field_text(Object *field, const char *text)
{
    /* Recalage des plages de style. Si container_set a noté un intervalle pour
     * CE champ, l'écriture portait sur un morceau et les plages se recalent ;
     * sinon elle porte sur le champ entier et elles sont détruites — c'est la
     * règle du remplacement complet, observée dans HyperCard 2.4. */
    if (field && field->type == OBJ_FIELD) {
        struct RunList *rl = runs_of(field);
        if (rl) {
            if (g_edit_fld == field && g_edit_at >= 0)
                runs_edit(rl, g_edit_at, g_edit_old, g_edit_new);
            else
                runs_free(rl);
        }
    }
    g_edit_fld = NULL; g_edit_at = -1;

    if (!field || (field->type != OBJ_FIELD && field->type != OBJ_BUTTON)) return;

    if (field_is_percard(field) && g_current_card) {
        Object *cd = g_current_card;
        for (int i = 0; i < cd->nbgtexts; i++)
            if (cd->bgtexts[i].field_id == field->id) {
                free(cd->bgtexts[i].text);
                cd->bgtexts[i].text = dupstr(text ? text : "");
                return;
            }
        if (cd->nbgtexts == cd->capbgtexts) {
            int cap = cd->capbgtexts ? cd->capbgtexts * 2 : 4;
            struct BgText *p = realloc(cd->bgtexts, (size_t)cap * sizeof *p);
            if (!p) return;
            cd->bgtexts = p;
            cd->capbgtexts = cap;
        }
        cd->bgtexts[cd->nbgtexts].field_id = field->id;
        cd->bgtexts[cd->nbgtexts].text = dupstr(text ? text : "");
        /* realloc ne nettoie rien : sans ce memset la liste de plages
         * démarrerait sur un pointeur bidon, et le premier hc_run_add
         * (ou hc_free) partirait dessus. */
        memset(&cd->bgtexts[cd->nbgtexts].runs, 0,
               sizeof cd->bgtexts[cd->nbgtexts].runs);
        cd->nbgtexts++;
        return;
    }

    free(field->contents);
    field->contents = dupstr(text ? text : "");
}

/* ---- Plages de style : lecture par l'hote ---- */
int hc_run_count(Object *field)
{
    struct RunList *rl = runs_of(field);
    return rl ? rl->n : 0;
}

/* Les sentinelles ne sortent jamais du noyau : l'hôte reçoit des valeurs
 * effectives, sinon HC_STYLE_INHERIT (-2) allumerait, bit à bit, l'italique,
 * le souligné et tout le reste au premier « & HC_ITALIC » de la vue. */
int hc_run_attrs(Object *field, int i, int *start, int *len,
                 int *style, int *size, const char **font)
{
    struct RunList *rl = runs_of(field);
    if (!rl || i < 0 || i >= rl->n) return 0;
    struct TextRun *r = &rl->v[i];
    if (start) *start = r->start;
    if (len)   *len   = r->len;
    if (style) *style = (r->style == HC_STYLE_INHERIT) ? field->textstyle
                                                       : r->style;
    if (size)  *size  = r->size ? r->size : field->textsize;
    if (font)  *font  = r->font ? r->font : field->textfont;
    return 1;
}

int hc_run_at(Object *field, int i, int *start, int *len, int *style)
{
    return hc_run_attrs(field, i, start, len, style, NULL, NULL);
}

void hc_runs_clear(Object *field)
{
    struct RunList *rl = runs_of(field);
    if (rl) runs_free(rl);
}

int hc_run_add_full(Object *field, int start, int len,
                    int style, int size, const char *font)
{
    struct RunList *rl = runs_of(field);
    if (!rl || len <= 0 || start < 0) return 0;

    /* La vue passe des valeurs effectives ; celles qui coïncident avec le
     * champ redeviennent des sentinelles, sinon un simple gras figerait au
     * passage la police du champ dans chaque plage. */
    int fsz = field->textsize;
    const char *ffn = field->textfont;
    if (size == fsz) size = 0;
    if (font && ffn && strcmp(font, ffn) == 0) font = NULL;
    if (font && !*font) font = NULL;

    if (style == 0 && size == 0 && !font) return 0;   /* rien à dire */

    if (!runs_room(rl, 1)) return 0;
    struct TextRun n;
    n.start = start; n.len = len; n.style = style; n.size = size;
    n.font  = font ? dupstr(font) : NULL;
    rl->v[rl->n++] = n;
    runs_tidy(rl);
    return 1;
}

int hc_run_add(Object *field, int start, int len, int style)
{
    return hc_run_add_full(field, start, len, style, 0, NULL);
}

const char *hc_paint_of(Object *o)
{
    return o ? o->paint : NULL;
}

void hc_set_paint(Object *o, const char *base64)
{
    if (!o) return;
    if (o->type != OBJ_CARD && o->type != OBJ_BACKGROUND) return;
    free(o->paint);
    o->paint = (base64 && *base64) ? dupstr(base64) : NULL;
}

void hc_do(const char *line)
{
    ARENA_MARK;
    g_depth  = 0;
    g_pass   = 0;
    g_me     = g_current_card;   /* dans la boîte de message, `me` = la carte */
    g_target = g_current_card;
    g_exit_handler = g_exit_repeat = g_next_repeat = 0;
    exec_stmt(g_current_card, line);
    ARENA_FREE;
}
