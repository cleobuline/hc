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
#define HC_MAX_DEPTH 64

static Object *g_current_card = NULL;
static int     g_trace = 1;
static int     g_pass  = 0;   /* levé par `pass` : le message doit continuer */

/* `the result` : les commandes susceptibles d'échouer y déposent un message,
 * et le vident quand elles réussissent. Les autres commandes n'y touchent
 * pas — c'est ce qui permet d'écrire « go … » puis « put the result ». */
static char    g_result[512] = "";

/* Arguments du gestionnaire courant, pour `the params`, param(n), paramCount.
   g_params[0] est le nom du message ; les suivants sont les arguments. */
static char    g_params[16][512];
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

static const HcHost g_console_host = { console_line, NULL };
static const HcHost *g_host = &g_console_host;

void hc_set_host(const HcHost *h) { g_host = h ? h : &g_console_host; }

/* Émet une ligne vers l'hôte. Le format ne doit PAS inclure le saut de ligne
   final ni l'indentation : l'hôte s'en charge. */
static void emit(HcLineKind kind, const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (g_host && g_host->line) g_host->line(kind, g_depth, buf);
}

/* Signale à l'hôte qu'un champ a changé (rafraîchissement d'affichage). */
static void notify_field(Object *field)
{
    if (g_host && g_host->field_changed) g_host->field_changed(field);
}

/* ==================== construction ==================== */

static int g_next_id = 1;

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

void hc_set_script(Object *o, const char *script)
{
    free(o->script);
    o->script = dupstr(script);
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
    free(o->paint);
    free(o);
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
static const char *find_handler(const char *script, const char *message,
                                const char **body_end, const char **hdr_out)
{
    if (!script) return NULL;
    const char *p = script;

    while (*p) {
        const char *line = skip_spaces(p);
        if (ci_prefix(line, "on ")) {
            char name[64];
            next_word(line + 3, name, sizeof name);
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

/* Résout une référence du genre :
 *   button "ok"        (carte courante, puis fond)
 *   bg button "nav"    (fond de la carte courante)
 *   field "notes"
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
    } else if (ci_word(ref, "card") || ci_word(ref, "cd")) {
        /* "card button" / "card field" / "card \"nom\"" / "card 3" */
        const char *after = skip_spaces(strchr(ref, ' ') ? strchr(ref, ' ') : ref + strlen(ref));
        if (*after == '"') {
            char nm[128];
            quoted(after, nm, sizeof nm);
            return find_card_by_name(stack, nm);
        }
        if (isdigit((unsigned char)*after))
            return nth_card(stack, atoi(after) - 1);   /* 1-based en HyperTalk */
        ref = after;
    }

    /* --- cartes désignées par leur rang --- */
    if (ci_word(ref, "this")) {
        const char *w = skip_spaces(ref + 4);
        if (!*w || ci_word(w, "card")) return card;
        if (ci_word(w, "stack")) return stack;
        if (ci_word(w, "background") || ci_word(w, "bg")) return bg;
    }
    if (ci_word(ref, "next"))  return nth_card(stack, card_index(stack, card) + 1);
    if (ci_word(ref, "prev") || ci_word(ref, "previous"))
                               return nth_card(stack, card_index(stack, card) - 1);
    if (ci_word(ref, "first")) return nth_card(stack, 0);
    if (ci_word(ref, "last"))  return nth_card(stack, card_count(stack) - 1);

    if (ci_equal(ref, "stack")) return stack;

    ObjType t;
    if (ci_word(ref, "button") || ci_word(ref, "btn")) { t = OBJ_BUTTON; }
    else if (ci_word(ref, "field") || ci_word(ref, "fld")) { t = OBJ_FIELD; }
    else return NULL;
    while (*ref && !isspace((unsigned char)*ref) && *ref != '"') ref++;
    ref = skip_spaces(ref);

    /* --- button id N / field id N --- */
    if (ci_word(ref, "id")) {
        int wanted = atoi(skip_spaces(ref + 2));
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

    char nm[128];
    quoted(ref, nm, sizeof nm);
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

void hc_shutdown(void) { frame_clear(&g_globals); }

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

        if (!*w || strchr("&+-*/<>=(),", *w)) { s = w; break; }

        const char *st = w, *q = w;
        if (*w == '"') {
            q = w + 1;
            while (*q && *q != '"') q++;
            if (*q == '"') q++;
        } else {
            while (*q && !isspace((unsigned char)*q) && !strchr("&+-*/<>=(),\"", *q)) q++;
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
                        char argv[][512], int argc);   /* défini plus bas */

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

    char src[2048];
    eval_expr(rest, src, sizeof src);      /* récursif : les morceaux s'emboîtent */

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


/* Écrit dans un conteneur : champ, variable, ou morceau de l'un des deux.
 * mode : 0 remplacer, 1 après, 2 avant, 3 supprimer.
 * L'appel est récursif, donc « word 2 of line 3 of field "notes" » marche.
 * Renvoie 1 si la destination a été reconnue.
 */
static int container_set(const char *ref, const char *val, int mode)
{
    ChunkType ct; char ia[128], ib[128]; const char *rest; int ordinal;

    if (parse_chunk(ref, &ct, ia, sizeof ia, ib, sizeof ib, &rest, &ordinal)) {
        char base[2048];
        eval_expr(rest, base, sizeof base);

        int a, b, st, en;
        chunk_indices(base, ct, ordinal, ia, ib, &a, &b);

        char sepstr[2] = { chunk_sep(ct), '\0' };
        char neuf[4096];

        if (!chunk_span(base, ct, a, b, &st, &en)) {
            if (mode == 3) return 1;            /* rien à supprimer */
            snprintf(neuf, sizeof neuf, "%s%s%s", base,
                     (*base && sepstr[0]) ? sepstr : "", val);
        } else {
            char old[2048];
            int len = en - st;
            if (len > (int)sizeof old - 1) len = (int)sizeof old - 1;
            if (len < 0) len = 0;
            memcpy(old, base + st, (size_t)len); old[len] = '\0';

            char piece[2048];
            if      (mode == 1) snprintf(piece, sizeof piece, "%s%s", old, val);
            else if (mode == 2) snprintf(piece, sizeof piece, "%s%s", val, old);
            else if (mode == 3) piece[0] = '\0';
            else                snprintf(piece, sizeof piece, "%s", val);

            if (mode == 3 && sepstr[0]) {       /* supprimer emporte un séparateur */
                int bl = (int)strlen(base);
                if      (en < bl && base[en] == sepstr[0]) en++;
                else if (st > 0  && base[st-1] == sepstr[0]) st--;
            }
            snprintf(neuf, sizeof neuf, "%.*s%s%s", st, base, piece, base + en);
        }
        return container_set(rest, neuf, 0);
    }

    char merged[4096];
    Object *o = resolve(ref);
    if (o && o->type == OBJ_FIELD) {
        const char *old = o->contents ? o->contents : "";
        if      (mode == 1) snprintf(merged, sizeof merged, "%s%s", old, val);
        else if (mode == 2) snprintf(merged, sizeof merged, "%s%s", val, old);
        else if (mode == 3) merged[0] = '\0';
        else                snprintf(merged, sizeof merged, "%s", val);
        free(o->contents);
        o->contents = dupstr(merged);
        notify_field(o);
        return 1;
    }
    if (o) return 0;                            /* un bouton n'est pas un conteneur */

    char vname[128];
    const char *after = next_word(ref, vname, sizeof vname);
    if (vname[0] && vname[0] != '"' && !*skip_spaces(after)) {
        const char *old = var_get(vname);
        if (!old) old = "";
        if      (mode == 1) snprintf(merged, sizeof merged, "%s%s", old, val);
        else if (mode == 2) snprintf(merged, sizeof merged, "%s%s", val, old);
        else if (mode == 3) merged[0] = '\0';
        else                snprintf(merged, sizeof merged, "%s", val);
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
static int split_args(const char *s, char args[][512], int maxargs)
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

/* Renvoie 1 si `t` était bien un appel de fonction. */
static int call_function(const char *t, char *out, int outlen)
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
    char raw[8][512], vals[8][512];
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
        char inner[512];
        int len = (int)(end - (q + 1));
        if (len > (int)sizeof inner - 1) len = (int)sizeof inner - 1;
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
    if (ci_equal(name, "random")) { int n = (int)a;
                                    snprintf(out, outlen, "%d", n > 0 ? (rand() % n) + 1 : 0); return 1; }
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

    return 0;
}

/* ==================== propriétés géométriques ==================== */

/* Lit une propriété géométrique dans `out`. Renvoie 0 si `prop` n'en est pas. */
static int geom_read(Object *o, const char *prop, char *out, int outlen)
{
    int L = o->x, T = o->y, R = o->x + o->w, B = o->y + o->h;
    if (ci_equal(prop, "rect") || ci_equal(prop, "rectangle"))
        snprintf(out, outlen, "%d,%d,%d,%d", L, T, R, B);
    else if (ci_equal(prop, "topleft"))   snprintf(out, outlen, "%d,%d", L, T);
    else if (ci_equal(prop, "botright") || ci_equal(prop, "bottomright"))
                                          snprintf(out, outlen, "%d,%d", R, B);
    else if (ci_equal(prop, "left"))      snprintf(out, outlen, "%d", L);
    else if (ci_equal(prop, "top"))       snprintf(out, outlen, "%d", T);
    else if (ci_equal(prop, "right"))     snprintf(out, outlen, "%d", R);
    else if (ci_equal(prop, "bottom"))    snprintf(out, outlen, "%d", B);
    else if (ci_equal(prop, "width"))     snprintf(out, outlen, "%d", o->w);
    else if (ci_equal(prop, "height"))    snprintf(out, outlen, "%d", o->h);
    else if (ci_equal(prop, "loc") || ci_equal(prop, "location"))
                                          snprintf(out, outlen, "%d,%d", L + o->w/2, T + o->h/2);
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

static void term_value(const char *t, char *out, int outlen)
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
            /* the number of buttons|fields [of <carte>] */
            if (ci_word(k, "buttons") || ci_word(k, "btns") ||
                ci_word(k, "fields")  || ci_word(k, "flds")) {
                ObjType want = (k[0]=='b' || k[0]=='B') ? OBJ_BUTTON : OBJ_FIELD;
                const char *r = k;
                while (*r && !isspace((unsigned char)*r)) r++;
                r = skip_spaces(r);
                if (ci_word(r, "of")) r = skip_spaces(r + 2);
                Object *card = *r ? resolve(r) : g_current_card;
                if (card && card->type != OBJ_CARD && card->type != OBJ_BACKGROUND)
                    card = g_current_card;
                int n = 0;
                if (card) {
                    for (int i = 0; i < card->nparts; i++)
                        if (card->parts[i]->type == want) n++;
                    if (card->type == OBJ_CARD && card->bg)
                        for (int i = 0; i < card->bg->nparts; i++)
                            if (card->bg->parts[i]->type == want) n++;
                }
                snprintf(out, outlen, "%d", n);
                return;
            }
            int used = 0;
            ChunkType ct = chunk_kind(k, &used);
            if (ct != CH_NONE) {
                const char *r = skip_spaces(k + used);
                if (ci_word(r, "in") || ci_word(r, "of")) r = skip_spaces(r + 2);
                char src[2048];
                eval_expr(r, src, sizeof src);
                snprintf(out, outlen, "%d", chunk_count(src, ct));
                return;
            }
        }
    }

    /* --- fonctions intégrées --- */
    if (call_function(t, out, outlen)) return;

    /* --- the [short|long] <propriété> of <objet> --- */
    if (ci_word(t, "the")) {
        const char *w = skip_spaces(t + 3);

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
                    if (ci_equal(prop, "hilite") || ci_equal(prop, "highlight")) { snprintf(out, outlen, "%s", o->hilite ? "true" : "false"); return; }
                    if (ci_equal(prop, "autohilite")) { snprintf(out, outlen, "%s", o->autohilite ? "true" : "false"); return; }
                    if (ci_equal(prop, "textsize") || ci_equal(prop, "textheight")) { snprintf(out, outlen, "%d", o->textsize); return; }
                    if (ci_equal(prop, "script"))  { snprintf(out, outlen, "%s", o->script ? o->script : ""); return; }
                    if (ci_equal(prop, "text") || ci_equal(prop, "contents"))
                                                   { snprintf(out, outlen, "%s", o->contents ? o->contents : ""); return; }
                    if (ci_equal(prop, "style"))   { snprintf(out, outlen, "%s", o->style ? o->style : "rectangle"); return; }
                }
            }
        }
    }

    /* --- un objet ? champ → contenu, autre → sa désignation --- */
    Object *o = resolve(t);
    if (o) {
        if (o->type == OBJ_FIELD && o->contents) snprintf(out, outlen, "%s", o->contents);
        else                                     hc_describe(o, out, outlen);
        return;
    }

    /* --- une variable ? --- */
    if (!strchr(t, ' ')) {
        const char *v = var_get(t);
        if (v) { snprintf(out, outlen, "%s", v); return; }
    }

    /* --- sinon littéral non quoté, comme le faisait HyperCard --- */
    snprintf(out, outlen, "%s", t);
}

static void parse_expr(const char **p, char *out, int outlen);

static int truthy(const char *s);

static void parse_factor(const char **p, char *out, int outlen)
{
    const char *s = skip_spaces(*p);
    out[0] = '\0';

    if (*s == '(') {
        *p = s + 1;
        parse_expr(p, out, outlen);
        s = skip_spaces(*p);
        if (*s == ')') s++;
        *p = s;
        return;
    }
    if (*s == '-') {
        char v[512]; double d = 0;
        *p = s + 1;
        parse_factor(p, v, sizeof v);
        as_num(v, &d);
        put_num(-d, out, outlen);
        return;
    }
    /* `not` est au niveau 2 chez Apple, aussi serré que le moins unaire :
       « not 5 > 2 » se lit « (not 5) > 2 ». */
    if (ci_word(s, "not")) {
        char v[512];
        *p = s + 3;
        parse_factor(p, v, sizeof v);
        snprintf(out, outlen, "%s", truthy(v) ? "false" : "true");
        return;
    }
    if (isdigit((unsigned char)*s) || (*s == '.' && isdigit((unsigned char)s[1]))) {
        char *e;
        double d = strtod(s, &e);
        *p = e;
        put_num(d, out, outlen);
        return;
    }
    if (*s == '"') {                 /* littéral : on ne prend que lui */
        *p = quoted(s, out, outlen);
        return;
    }

    char ref[512];
    const char *before = s;
    collect_ref(&s, ref, sizeof ref);
    if (s == before && *s) s++;      /* jamais de sur-place : pas de boucle infinie */
    *p = s;
    term_value(ref, out, outlen);
}

/* niveau 3 : exponentiation, associative à droite */
static void parse_power(const char **p, char *out, int outlen)
{
    parse_factor(p, out, outlen);
    const char *s = skip_spaces(*p);
    if (*s != '^') return;
    *p = s + 1;

    char rhs[512]; double a = 0, b = 0;
    parse_power(p, rhs, sizeof rhs);      /* récursif : 2^3^2 = 2^(3^2) */
    as_num(out, &a); as_num(rhs, &b);
    put_num(pow(a, b), out, outlen);
}

static void parse_product(const char **p, char *out, int outlen)
{
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

        char rhs[512]; double a = 0, b = 0, r = 0;
        parse_power(p, rhs, sizeof rhs);
        as_num(out, &a); as_num(rhs, &b);
        if      (op == '*') r = a * b;
        else if (op == '/') r = (b != 0) ? a / b : 0;
        else if (op == 'd') r = (b != 0) ? (double)(long long)(a / b) : 0;
        else                r = (b != 0) ? a - b * (double)(long long)(a / b) : 0;
        put_num(r, out, outlen);
    }
}

static void parse_sum(const char **p, char *out, int outlen)
{
    parse_product(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        if (*s != '+' && *s != '-') break;
        int op = *s++;
        *p = s;

        char rhs[512]; double a = 0, b = 0;
        parse_product(p, rhs, sizeof rhs);
        as_num(out, &a); as_num(rhs, &b);
        put_num(op == '+' ? a + b : a - b, out, outlen);
    }
}

static void parse_concat(const char **p, char *out, int outlen)
{
    parse_sum(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        int space;
        if      (s[0] == '&' && s[1] == '&') { space = 1; s += 2; }
        else if (s[0] == '&')                { space = 0; s += 1; }
        else break;
        *p = s;

        char rhs[512];
        parse_sum(p, rhs, sizeof rhs);
        int n = (int)strlen(out);
        if (space && n < outlen - 1) { out[n++] = ' '; out[n] = '\0'; }
        snprintf(out + n, (size_t)(outlen - n), "%s", rhs);
    }
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
static int looks_like_date(const char *s)
{
    char buf[64];
    int n = 0;
    const char *p = s;
    while (*p) {
        const char *q = p;
        while (*q && *q != '/' && *q != '-') q++;
        int len = (int)(q - p);
        if (len == 0 || len > (int)sizeof buf - 1) return 0;
        memcpy(buf, p, (size_t)len); buf[len] = '\0';
        if (!is_int_str(buf)) return 0;
        n++;
        if (!*q) break;
        p = q + 1;
    }
    return n == 3;
}

static int is_of_type(const char *v, const char *ty)
{
    char t[512];
    double d;
    trim_copy(v, t, sizeof t);

    if (ci_equal(ty, "number"))    return as_num(t, &d);
    if (ci_equal(ty, "integer"))   return is_int_str(t);
    if (ci_equal(ty, "logical") || ci_equal(ty, "boolean"))
        return ci_equal(t, "true") || ci_equal(t, "false");
    if (ci_equal(ty, "point"))     return int_items(t) == 2;
    if (ci_equal(ty, "rect") || ci_equal(ty, "rectangle")) return int_items(t) == 4;
    if (ci_equal(ty, "date"))      return looks_like_date(t);
    return 0;
}

/* niveau 7 : comparaisons relationnelles, contains, is in.
   Attention : un « is » nu appartient au niveau 8, on le laisse passer. */
static void parse_relational(const char **p, char *out, int outlen)
{
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
                char ty[32];
                const char *q = next_word(skip_spaces(w + (ci_word(w, "an") ? 2 : 1)),
                                          ty, sizeof ty);
                *p = q;
                int r = is_of_type(out, ty);
                if (notted) r = !r;
                snprintf(out, outlen, "%s", r ? "true" : "false");
                continue;
            }
            if (ci_word(w, "within")) {                     /* point is within rect */
                *p = w + 6;
                char rhs[512];
                parse_concat(p, rhs, sizeof rhs);
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

        char rhs[512];
        parse_concat(p, rhs, sizeof rhs);
        int r = compare_vals(op, out, rhs);
        if (neg) r = !r;
        snprintf(out, outlen, "%s", r ? "true" : "false");
    }
}

/* niveau 8 : égalités */
static void parse_equality(const char **p, char *out, int outlen)
{
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

        char rhs[512];
        parse_relational(p, rhs, sizeof rhs);
        int r = compare_vals(op, out, rhs);
        if (neg) r = !r;
        snprintf(out, outlen, "%s", r ? "true" : "false");
    }
}

static void parse_and(const char **p, char *out, int outlen)
{
    parse_equality(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        if (!ci_word(s, "and")) break;
        *p = s + 3;
        char rhs[512];
        parse_equality(p, rhs, sizeof rhs);
        snprintf(out, outlen, "%s", (truthy(out) && truthy(rhs)) ? "true" : "false");
    }
}

static void parse_expr(const char **p, char *out, int outlen)
{
    parse_and(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        if (!ci_word(s, "or")) break;
        *p = s + 2;
        char rhs[512];
        parse_and(p, rhs, sizeof rhs);
        snprintf(out, outlen, "%s", (truthy(out) || truthy(rhs)) ? "true" : "false");
    }
}

static void eval_expr(const char *s, char *out, int outlen)
{
    const char *p = s;
    parse_expr(&p, out, outlen);
}

/* Comme eval_expr, mais râle si l'analyseur n'a pas tout mangé.
 * C'est le garde-fou contre les fautes de frappe : sans lui, une
 * expression mal formée retombe silencieusement en littéral. */
static void eval_checked(const char *s, char *out, int outlen)
{
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

/* Index de la ligne fermante correspondante (« end if », « end repeat »),
   en tenant compte des imbrications. Renvoie `to` si rien n'est trouvé. */
static int match_end(char **L, int from, int to, const char *what)
{
    int depth = 0;
    char endw[32];
    snprintf(endw, sizeof endw, "end %s", what);
    for (int i = from; i < to; i++) {
        const char *s = L[i];
        if (opens_if(s) || opens_repeat(s)) depth++;
        else if (ci_word(s, "end")) {
            const char *w = skip_spaces(s + 3);
            if (ci_word(w, "if") || ci_word(w, "repeat")) {
                if (depth == 0 && ci_prefix(s, endw)) return i;
                depth--;
            }
        }
    }
    return to;
}

/* Premier « else » de même niveau entre `from` et `to`. */
static int find_else(char **L, int from, int to)
{
    int depth = 0;
    for (int i = from; i < to; i++) {
        const char *s = L[i];
        if (opens_if(s) || opens_repeat(s)) depth++;
        else if (ci_word(s, "end")) { if (depth > 0) depth--; }
        else if (depth == 0 && ci_word(s, "else")) return i;
    }
    return -1;
}

static void exec_block(Object *me, char **L, int from, int to);

/* `head` vaut « if <condition> then » ; le corps va de `from` à `end_idx`. */
static void exec_if(Object *me, const char *head, char **L, int from, int end_idx)
{
    const char *th = find_kw(head, "then");
    char cond[512], val[512];
    const char *c0 = skip_spaces(head + 2);   /* après « if » */
    int n = th ? (int)(th - c0) : (int)strlen(c0);
    if (n > (int)sizeof cond - 1) n = (int)sizeof cond - 1;
    memcpy(cond, c0, (size_t)n); cond[n] = '\0';
    eval_checked(cond, val, sizeof val);

    int m = find_else(L, from, end_idx);
    if (truthy(val)) {
        exec_block(me, L, from, m >= 0 ? m : end_idx);
        return;
    }
    if (m < 0) return;

    const char *rest = skip_spaces(L[m] + 4);   /* après « else » */
    if (!*rest) { exec_block(me, L, m + 1, end_idx); return; }
    if (ci_word(rest, "if")) { exec_if(me, rest, L, m + 1, end_idx); return; }
    exec_line(me, rest);                        /* « else <instruction> » */
}

/* Exécute une instruction simple, ou un `if` tenant sur une seule ligne. */
static void exec_stmt(Object *me, const char *s)
{
    if (ci_word(s, "if")) {
        const char *th = find_kw(s, "then");
        if (th) {
            char cond[512], val[512];
            const char *c0 = skip_spaces(s + 2);
            int n = (int)(th - c0);
            if (n > (int)sizeof cond - 1) n = (int)sizeof cond - 1;
            memcpy(cond, c0, (size_t)n); cond[n] = '\0';
            eval_checked(cond, val, sizeof val);

            const char *body = skip_spaces(th + 4);
            const char *el   = find_kw(body, "else");
            char yes[512];
            int ny = el ? (int)(el - body) : (int)strlen(body);
            if (ny > (int)sizeof yes - 1) ny = (int)sizeof yes - 1;
            memcpy(yes, body, (size_t)ny); yes[ny] = '\0';

            if (truthy(val)) exec_stmt(me, yes);
            else if (el)     exec_stmt(me, skip_spaces(el + 4));
            return;
        }
    }
    exec_line(me, s);
}

static void exec_block(Object *me, char **L, int from, int to)
{
    for (int i = from; i < to; i++) {
        const char *s = L[i];

        /* --- if … then / end if --- */
        if (opens_if(s)) {
            int e = match_end(L, i + 1, to, "if");
            exec_if(me, s, L, i + 1, e);
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
                char v[256];
                eval_expr(q, v, sizeof v);
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

        char line[512];
        if (len > (int)sizeof line - 1) len = (int)sizeof line - 1;
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

    exec_block(me, L, 0, n);

    for (int i = 0; i < n; i++) free(L[i]);
    free(L);
}

static void exec_line(Object *me, const char *line)
{
    (void)me;   /* servira pour `the target` / `me` dans les expressions */
    char verb[64];
    const char *rest = next_word(line, verb, sizeof verb);

    /* --- send "<message> [args]" to <objet> ---
     * Le message est une ligne de HyperTalk : un nom de gestionnaire suivi
     * d'arguments éventuels. « send "carre 6" to card X » appelle on carre
     * sur X avec l'argument 6. Les arguments sont évalués côté appelant. */
    if (ci_equal(verb, "send")) {
        char msgline[512];
        rest = skip_spaces(rest);
        if (*rest == '"') rest = quoted(rest, msgline, sizeof msgline);
        else {
            /* message nu : tout jusqu'à « to » (au niveau supérieur) */
            const char *to_kw = find_kw(rest, "to");
            int len = to_kw ? (int)(to_kw - rest) : (int)strlen(rest);
            if (len > (int)sizeof msgline - 1) len = (int)sizeof msgline - 1;
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
        char argv[16][512];
        int argc = 0;
        a = skip_spaces(a);
        while (*a && argc < 16) {
            char one[512]; int len = 0, depth = 0, inq = 0;
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

        char val[512];
        if (!kw) {                       /* pas de destination : la boîte de message */
            eval_checked(rest, val, sizeof val);
            emit(HC_MSG, "%s", val);
            return;
        }

        char expr[512];
        int n = (int)(kw - rest);
        if (n > (int)sizeof expr - 1) n = (int)sizeof expr - 1;
        memcpy(expr, rest, (size_t)n); expr[n] = '\0';
        eval_checked(expr, val, sizeof val);

        const char *dsttext = skip_spaces(kw + kwlen);
        if (container_set(dsttext, val, mode)) {
            set_result("");
            char shown[512];
            eval_expr(dsttext, shown, sizeof shown);
            if (!*shown && *val) snprintf(shown, sizeof shown, "%s", val);
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

    /* --- return <expr> : dépose une valeur dans `the result` et sort --- */
    if (ci_equal(verb, "return")) {
        char val[512];
        eval_checked(rest, val, sizeof val);
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
        if (!ci_word(q, "of")) {
            emit(HC_ERR, "   !! set mal formé : %s", skip_spaces(rest));
            return;
        }
        q = skip_spaces(q + 2);

        const char *to = find_kw(q, "to");
        if (!to) {
            emit(HC_ERR, "   !! set sans « to » : %s", skip_spaces(rest));
            return;
        }

        char refbuf[256];
        int n = (int)(to - q);
        if (n > (int)sizeof refbuf - 1) n = (int)sizeof refbuf - 1;
        memcpy(refbuf, q, (size_t)n); refbuf[n] = '\0';
        while (n > 0 && (refbuf[n-1] == ' ' || refbuf[n-1] == '\t')) refbuf[--n] = '\0';

        char val[2048];
        eval_checked(to + 2, val, sizeof val);

        Object *o = resolve(refbuf);
        if (!o) {
            set_result("objet introuvable");
            emit(HC_ERR, "   !! objet introuvable : %s", refbuf);
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
        } else if (ci_equal(prop, "hilite") || ci_equal(prop, "highlight")) {
            o->hilite = truthy(val);
            notify_field(o);
        } else if (ci_equal(prop, "autohilite")) {
            o->autohilite = truthy(val);
        } else if (ci_equal(prop, "textsize") || ci_equal(prop, "textheight")) {
            o->textsize = atoi(val);
            notify_field(o);
        } else if (ci_equal(prop, "script")) {
            hc_set_script(o, val);
        } else if (ci_equal(prop, "style")) {
            free(o->style);
            o->style = dupstr(val);
        } else if (ci_equal(prop, "text") || ci_equal(prop, "contents")) {
            if (o->type != OBJ_FIELD) {
                emit(HC_ERR, "   !! seul un champ a un contenu");
                return;
            }
            free(o->contents);
            o->contents = dupstr(val);
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
        char val[2048];
        eval_checked(rest, val, sizeof val);

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
        char val[512];
        eval_checked(rest, val, sizeof val);
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

    /* --- go [to] card "nom" | next | previous | first | last | card 3 --- */
    if (ci_equal(verb, "go")) {
        const char *r = skip_spaces(rest);
        if (ci_word(r, "to")) r = skip_spaces(r + 2);
        Object *dst = resolve(r);
        if (!dst && ci_word(r, "card")) {   /* « go card » nu : la première */
            const char *w = skip_spaces(r + 4);
            if (!*w) dst = resolve("first card");
        }
        if (dst && dst->type == OBJ_CARD) {
            set_result("");
            Object *old = g_current_card;
            if (old) hc_send(old, "closeCard");
            g_current_card = dst;
            emit(HC_INFO, "   ⇒ va à la carte \"%s\"", dst->name ? dst->name : "?");
            hc_send(dst, "openCard");
        } else {
            set_result("carte introuvable");
            emit(HC_ERR, "   !! carte introuvable : %s", r);
        }
        return;
    }

    /* --- show / hide <objet> --- */
    if (ci_equal(verb, "show") || ci_equal(verb, "hide")) {
        Object *o = resolve(rest);
        if (o) {
            o->visible = ci_equal(verb, "show");
            char d[64]; hc_describe(o, d, sizeof d);
            emit(HC_INFO, "   → %s : %s", d, o->visible ? "visible" : "caché");
        }
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
                char argv[16][512];
                int argc = 0;
                const char *a = skip_spaces(rest);
                while (*a && argc < 16) {
                    /* découpe au niveau des virgules de premier niveau */
                    char one[512]; int len = 0, depth = 0, inq = 0;
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

/* ==================== envoi d'un message ==================== */

/* Envoie un message accompagné d'une liste d'arguments déjà évalués.
   argv[0..argc-1] sont les valeurs des arguments (sans le nom du message). */
static int hc_send_args(Object *target, const char *message,
                        char argv[][512], int argc)
{
    if (g_depth >= HC_MAX_DEPTH) {
        emit(HC_ERR, "!! trop de récursion : message \"%s\" abandonné", message);
        return 0;
    }

    Object *chain[8];
    int n = build_chain(target, chain, 8);

    /* `the target` vaut le destinataire initial pendant toute la remontée ;
       on empile l'ancien pour les envois imbriqués. */
    Object *saved_target = g_target;
    Object *saved_me     = g_me;
    g_target = target;

    /* on empile les paramètres du gestionnaire appelant */
    char saved_params[16][512];
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
        const char *body = find_handler(o->script, message, &end, &hdr);

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
                const char *q = next_word(hdr + 3, pname, sizeof pname);  /* saute « on nom » */
                int idx = 0;
                for (;;) {
                    q = skip_spaces(q);
                    if (*q == ',') { q++; continue; }
                    if (!*q || *q == '\n') break;
                    q = next_word(q, pname, sizeof pname);
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
        emit(HC_TRACE, "  ✗ message \"%s\" non traité", message);
    }

    g_depth--;
    g_me     = saved_me;
    g_target = saved_target;

    /* dépiler les paramètres de l'appelant */
    for (int i = 0; i < saved_nparams; i++)
        memcpy(g_params[i], saved_params[i], sizeof g_params[i]);
    g_nparams = saved_nparams;

    return handled;
}

int hc_send(Object *target, const char *message)
{
    char none[1][512];
    return hc_send_args(target, message, none, 0);
}

/* ==================== boîte de message ==================== */

Object *hc_resolve(const char *ref) { return resolve(ref); }

const char *hc_script_of(Object *o) { return o ? o->script : NULL; }

void hc_set_field_text(Object *field, const char *text)
{
    if (!field || field->type != OBJ_FIELD) return;
    free(field->contents);
    field->contents = dupstr(text ? text : "");
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
    g_depth  = 0;
    g_pass   = 0;
    g_me     = g_current_card;   /* dans la boîte de message, `me` = la carte */
    g_target = g_current_card;
    g_exit_handler = g_exit_repeat = g_next_repeat = 0;
    exec_stmt(g_current_card, line);
}
