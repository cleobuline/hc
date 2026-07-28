/* hc_file.c — Format de pile texte, v1.
 *
 * Un objet s'ouvre par une ligne d'en-tête et se ferme par « end <type> ».
 * Les lignes de script et de contenu sont préfixées par « | » : ainsi un
 * script contenant « end mouseUp » ou « end script » ne casse pas l'analyse.
 *
 *     stack "Essai"
 *     script
 *     | on mouseUp
 *     |   beep
 *     | end mouseUp
 *     end script
 *     end stack
 *
 *     background "commun"
 *     button "suivant"
 *     script
 *     | on mouseUp
 *     |   go to card "zaza"
 *     | end mouseUp
 *     end script
 *     end button
 *     end background
 *
 *     card "accueil" background "commun"
 *     field "notes"
 *     contents
 *     | du texte
 *     end contents
 *     end field
 *     end card
 */
#include "hc_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *dupstr_file(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
    memcpy(p, s, n);
    return p;
}

/* ==================== écriture ==================== */

static void put_block_wrap(FILE *f, const char *tag, const char *text, int wrap)
{
    if (!text || !*text) return;
    fprintf(f, "%s\n", tag);
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (wrap) {
            /* découper en tronçons de 100 caractères (base64) : le lecteur
               les recolle et le décodeur base64 ignore les sauts de ligne. */
            int off = 0;
            while (off < len) {
                int chunk = (len - off > 100) ? 100 : (len - off);
                fprintf(f, "| %.*s\n", chunk, p + off);
                off += chunk;
            }
            if (len == 0) fprintf(f, "|\n");
        } else {
            fprintf(f, "| %.*s\n", len, p);
        }
        if (!nl) break;
        p = nl + 1;
    }
    fprintf(f, "end %s\n", tag);
}

static void put_block(FILE *f, const char *tag, const char *text)
{
    put_block_wrap(f, tag, text, 0);   /* scripts, contenus : pas de découpage */
}

static void put_paint(FILE *f, const char *b64)
{
    put_block_wrap(f, "paint", b64, 1);   /* base64 : découpé en lignes courtes */
}

static void put_part(FILE *f, Object *o)
{
    const char *kind = (o->type == OBJ_BUTTON) ? "button" : "field";
    fprintf(f, "%s \"%s\"\n", kind, o->name ? o->name : "");
    fprintf(f, "rect %d,%d,%d,%d\n", o->x, o->y, o->x + o->w, o->y + o->h);
    if (o->style) fprintf(f, "style \"%s\"\n", o->style);
    put_block(f, "script", o->script);
    if (o->type == OBJ_FIELD) put_block(f, "contents", o->contents);
    if (!o->visible) fprintf(f, "hidden\n");
    if (o->hilite) fprintf(f, "hilite\n");
    if (o->autohilite) fprintf(f, "autohilite\n");
    if (o->textsize) fprintf(f, "textsize %d\n", o->textsize);
    if (!o->showname) fprintf(f, "hidename\n");   /* nom masqué (défaut = affiché) */
    fprintf(f, "end %s\n", kind);
}

int hc_save(Object *stack, const char *path)
{
    if (!stack || stack->type != OBJ_STACK) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "-- pile HyperCard (format maison v1)\n\n");

    fprintf(f, "stack \"%s\"\n", stack->name ? stack->name : "");
    fprintf(f, "size %d,%d\n", stack->w, stack->h);
    put_block(f, "script", stack->script);
    fprintf(f, "end stack\n\n");

    /* les fonds d'abord : les cartes s'y réfèrent par leur nom */
    for (int i = 0; i < stack->nparts; i++) {
        Object *bg = stack->parts[i];
        if (bg->type != OBJ_BACKGROUND) continue;
        fprintf(f, "background \"%s\"\n", bg->name ? bg->name : "");
        put_block(f, "script", bg->script);
        put_paint(f, bg->paint);
        for (int j = 0; j < bg->nparts; j++) put_part(f, bg->parts[j]);
        fprintf(f, "end background\n\n");
    }

    for (int i = 0; i < stack->nparts; i++) {
        Object *c = stack->parts[i];
        if (c->type != OBJ_CARD) continue;
        fprintf(f, "card \"%s\"", c->name ? c->name : "");
        if (c->bg && c->bg->name) fprintf(f, " background \"%s\"", c->bg->name);
        fprintf(f, "\n");
        put_block(f, "script", c->script);
        put_paint(f, c->paint);
        for (int j = 0; j < c->nparts; j++) put_part(f, c->parts[j]);
        fprintf(f, "end card\n\n");
    }

    fclose(f);
    return 0;
}

/* ==================== lecture ==================== */

static void rtrim(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' ||
                     s[n-1] == ' '  || s[n-1] == '\t'))
        s[--n] = '\0';
}

static char *ltrim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* extrait le n-ième littéral entre guillemets de la ligne */
static int get_quoted(const char *line, int which, char *out, int outlen)
{
    int found = 0;
    const char *p = line;
    out[0] = '\0';
    while (*p) {
        if (*p == '"') {
            p++;
            const char *start = p;
            while (*p && *p != '"') p++;
            if (found == which) {
                int len = (int)(p - start);
                if (len > outlen - 1) len = outlen - 1;
                memcpy(out, start, (size_t)len);
                out[len] = '\0';
                return 1;
            }
            found++;
            if (*p == '"') p++;
        } else p++;
    }
    return 0;
}

/* accumulateur de texte pour les blocs « | » */
typedef struct { char *buf; size_t len, cap; } Acc;

static void acc_line(Acc *a, const char *s)
{
    size_t n = strlen(s);
    if (a->len + n + 2 > a->cap) {
        size_t cap = a->cap ? a->cap * 2 : 256;
        while (cap < a->len + n + 2) cap *= 2;
        char *p = realloc(a->buf, cap);
        if (!p) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
        a->buf = p; a->cap = cap;
    }
    memcpy(a->buf + a->len, s, n);
    a->len += n;
    a->buf[a->len++] = '\n';
    a->buf[a->len]   = '\0';
}

static char *acc_take(Acc *a)
{
    char *r = a->buf;
    a->buf = NULL; a->len = a->cap = 0;
    return r;
}

static Object *find_bg(Object *stack, const char *name)
{
    for (int i = 0; i < stack->nparts; i++) {
        Object *o = stack->parts[i];
        if (o->type == OBJ_BACKGROUND && o->name && strcmp(o->name, name) == 0)
            return o;
    }
    return NULL;
}

Object *hc_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    Object *stack = NULL;   /* la pile */
    Object *owner = NULL;   /* fond ou carte en cours */
    Object *part  = NULL;   /* bouton ou champ en cours */
    Object *target = NULL;  /* à qui appartient le bloc en cours */

    char line[4096], nm[256], nm2[256]; (void)nm2;
    Acc acc = {0};
    int in_script = 0, in_contents = 0, in_paint = 0;

    while (fgets(line, sizeof line, f)) {
        rtrim(line);
        char *s = ltrim(line);

        /* --- lignes d'un bloc --- */
        if (in_script || in_contents || in_paint) {
            if (s[0] == '|') {
                acc_line(&acc, (s[1] == ' ') ? s + 2 : s + 1);
                continue;
            }
            if (strcmp(s, "end script") == 0) {
                char *t = acc_take(&acc);
                if (target && t) hc_set_script(target, t);
                free(t);
                in_script = 0;
                continue;
            }
            if (strcmp(s, "end contents") == 0) {
                char *t = acc_take(&acc);
                if (target && target->type == OBJ_FIELD) {
                    free(target->contents);
                    target->contents = t ? t : NULL;
                } else free(t);
                in_contents = 0;
                continue;
            }
            if (strcmp(s, "end paint") == 0) {
                char *t = acc_take(&acc);
                if (target && (target->type == OBJ_CARD || target->type == OBJ_BACKGROUND)) {
                    free(target->paint);
                    target->paint = t ? t : NULL;
                } else free(t);
                in_paint = 0;
                continue;
            }
            continue;   /* ligne parasite dans un bloc : ignorée */
        }

        if (!*s || (s[0] == '-' && s[1] == '-')) continue;   /* vide / commentaire */

        /* --- ouverture de blocs texte --- */
        if (strcmp(s, "script") == 0)   { in_script = 1;   continue; }
        if (strcmp(s, "contents") == 0) { in_contents = 1; continue; }
        if (strcmp(s, "paint") == 0)    { in_paint = 1;    continue; }

        /* --- en-têtes d'objets --- */
        if (strncmp(s, "stack ", 6) == 0) {
            get_quoted(s, 0, nm, sizeof nm);
            stack = hc_new_stack(nm);
            target = stack;
            continue;
        }
        if (!stack) continue;   /* rien avant la pile */

        if (strncmp(s, "size ", 5) == 0) {
            int sw, sh;
            if (sscanf(s + 5, "%d,%d", &sw, &sh) == 2) {
                stack->w = sw; stack->h = sh;
            }
            continue;
        }
        if (strncmp(s, "background ", 11) == 0) {
            get_quoted(s, 0, nm, sizeof nm);
            owner = hc_new_background(stack, nm);
            target = owner;
            continue;
        }
        if (strncmp(s, "card ", 5) == 0) {
            get_quoted(s, 0, nm, sizeof nm);
            Object *bg = NULL;
            if (get_quoted(s, 1, nm2, sizeof nm2)) bg = find_bg(stack, nm2);
            owner = hc_new_card(stack, bg, nm);
            target = owner;
            continue;
        }
        if (strncmp(s, "button ", 7) == 0 && owner) {
            get_quoted(s, 0, nm, sizeof nm);
            part = hc_new_button(owner, nm);
            target = part;
            continue;
        }
        if (strncmp(s, "field ", 6) == 0 && owner) {
            get_quoted(s, 0, nm, sizeof nm);
            part = hc_new_field(owner, nm);
            target = part;
            continue;
        }
        if (strcmp(s, "hidden") == 0) {
            if (target) target->visible = 0;
            continue;
        }
        if (strcmp(s, "hilite") == 0) {
            if (target) target->hilite = 1;
            continue;
        }
        if (strcmp(s, "autohilite") == 0) {
            if (target) target->autohilite = 1;
            continue;
        }
        if (strncmp(s, "textsize ", 9) == 0 && part) {
            part->textsize = atoi(s + 9);
            continue;
        }
        if (strcmp(s, "hidename") == 0 && part) {
            part->showname = 0;
            continue;
        }
        if (strncmp(s, "rect ", 5) == 0 && part) {
            int a, b, c, d;
            if (sscanf(s + 5, "%d,%d,%d,%d", &a, &b, &c, &d) == 4) {
                part->x = a; part->y = b; part->w = c - a; part->h = d - b;
            }
            continue;
        }
        if (strncmp(s, "style ", 6) == 0 && part) {
            get_quoted(s, 0, nm, sizeof nm);
            free(part->style);
            part->style = nm[0] ? dupstr_file(nm) : NULL;
            continue;
        }

        /* --- fermetures --- */
        if (strcmp(s, "end button") == 0 || strcmp(s, "end field") == 0) {
            part = NULL; target = owner; continue;
        }
        if (strcmp(s, "end card") == 0 || strcmp(s, "end background") == 0) {
            owner = NULL; target = stack; continue;
        }
        if (strcmp(s, "end stack") == 0) { target = NULL; continue; }
    }

    free(acc.buf);
    fclose(f);
    return stack;
}
