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
 *     textstyle 1
 *     run 3,2,2
 *     end field
 *     end card
 *
 * Le style d'un champ tient sur deux lignes de nature différente :
 *   textstyle N   style du champ entier, valeur de repli
 *   run s,l,N     une plage : N s'applique aux `l` octets à partir de `s`
 * Un champ de fond non partagé a un texte ET un style par carte : la carte
 * les écrit ensemble, le bloc « bgtextdata » suivi de ses lignes « bgrun ».
 *
 *     bghilite 14
 *     bgtext 12
 *     bgtextdata
 *     | note propre a cette carte
 *     end bgtextdata
 *     bgrun 0,4,1
 *
 * Les icones appartiennent a la PILE, comme les ressources ICON de HyperCard :
 * une pile emporte ses icones, et un bouton n'en retient que le numero.
 *
 *     iconres 20554 "Terminator"
 *     | 00000000
 *     | 00018000
 *     | ...
 *     end iconres
 *
 * 32x32 en 1 bit, soit 128 octets, ecrits en hexadecimal a raison de quatre
 * octets par ligne : une ligne du fichier est une ligne de l'icone, et il y en
 * a trente-deux. L'hexadecimal plutot que le base64 des blocs « paint » :
 * c'est court, ca se lit, et ca se retouche a la main. Bit de poids fort a
 * gauche, bit a 1 = encre — la disposition de HCICONS, a l'octet pres.
 *
 * Le mot-cle est « iconres » et non « icon », deja pris par l'attribut de
 * bouton. Les distinguer au seul garde « && part » ne tiendrait que parce que
 * la pile s'ecrit avant les cartes : trop fragile pour qu'on s'y fie.
 *
 * Un binaire anterieur relisant une pile qui contient des icones ne les
 * comprend pas, mais ne s'y casse pas : « iconres ... » ne repond a aucun
 * prefixe connu et les lignes « | » hors bloc sont deja ignorees.
 */
#include "hc_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Écrit une chaîne entre guillemets, en protégeant les guillemets et les
 * contre-obliques qu'elle contient. Sans ça, un objet nommé
 *     go card "canard"
 * s'écrivait  button "go card "canard""  et le lecteur, qui s'arrête au
 * guillemet suivant, ne relisait que « go card ». Le nom était donc perdu
 * à l'écriture, pas à la lecture. */
static void put_quoted(FILE *f, const char *s)
{
    fputc('"', f);
    for (const char *p = s ? s : ""; *p; p++) {
        if (*p == '"' || *p == '\\') fputc('\\', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

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

/* Une icône de pile : en-tête, puis 128 octets en hexadécimal, huit par
 * ligne. Le nom passe par put_quoted, il peut donc contenir un guillemet. */
static void put_icon(FILE *f, const struct StackIcon *ic)
{
    fprintf(f, "iconres %d ", ic->id);
    put_quoted(f, ic->name ? ic->name : "");
    fputc('\n', f);
    /* Quatre octets par ligne : une ligne du fichier = une ligne de l'icone,
     * comme dans la source de HCICONS. Trente-deux lignes, et le dessin se
     * devine a l'oeil nu dans le fichier. */
    for (int i = 0; i < HC_ICON_BYTES; i += 4) {
        fputs("| ", f);
        for (int k = 0; k < 4; k++) fprintf(f, "%02X", ic->bits[i + k]);
        fputc('\n', f);
    }
    fprintf(f, "end iconres\n");
}

/* Plages de style d'un champ, une par ligne :
 *     run <start>,<len>,<style>
 * Les offsets sont en octets dans le texte du champ, comme dans le noyau
 * (hc_run_at les rend tels quels). La liste est déjà triée et fusionnée par
 * runs_tidy à l'écriture, on la recopie donc dans l'ordre.
 * Le tag change selon le porteur : « run » pour la liste du champ lui-même,
 * « bgrun » pour celle qu'une carte tient sur un champ de fond non partagé. */
static void put_runs(FILE *f, const char *tag, const struct RunList *rl)
{
    if (!rl) return;
    for (int i = 0; i < rl->n; i++) {
        const struct TextRun *r = &rl->v[i];
        if (r->len <= 0) continue;
        /* Une plage muette sur les trois attributs décrit le champ : inutile.
         * Attention, `style == 0` n'est pas muet — c'est « plain », qui doit
         * survivre à l'enregistrement dans un champ gras. L'ancien test le
         * jetait, et le mot reprenait le gras du champ au rechargement. */
        if (r->style == HC_STYLE_INHERIT && r->size == 0 && !r->font &&
            r->color == HC_COLOR_INHERIT) continue;

        /* Trois formes, de la plus courte à la plus longue, pour que les piles
         * déjà enregistrées gardent exactement la même allure et qu'un binaire
         * plus ancien continue de les lire :
         *     s,l,style
         *     s,l,style,corps,police
         *     s,l,style,corps,police,couleur
         * La police pouvant contenir des espaces mais jamais de virgule, la
         * couleur se lit sans ambiguïté après la dernière. */
        if (r->size == 0 && !r->font && r->color == HC_COLOR_INHERIT)
            fprintf(f, "%s %d,%d,%d\n", tag, r->start, r->len, r->style);
        else if (r->color == HC_COLOR_INHERIT)
            fprintf(f, "%s %d,%d,%d,%d,%s\n", tag, r->start, r->len,
                    r->style, r->size, r->font ? r->font : "");
        else
            fprintf(f, "%s %d,%d,%d,%d,%s,%d\n", tag, r->start, r->len,
                    r->style, r->size, r->font ? r->font : "", r->color);
    }
}

static void put_part(FILE *f, Object *o)
{
    const char *kind = (o->type == OBJ_BUTTON) ? "button" : "field";
    fprintf(f, "%s ", kind); put_quoted(f, o->name); fputc('\n', f);
    fprintf(f, "rect %d,%d,%d,%d\n", o->x, o->y, o->x + o->w, o->y + o->h);
    if (o->style) fprintf(f, "style \"%s\"\n", o->style);
    put_block(f, "script", o->script);
    if (o->type == OBJ_FIELD || o->type == OBJ_BUTTON) put_block(f, "contents", o->contents);
    if (!o->visible) fprintf(f, "hidden\n");
    if (o->hilite) fprintf(f, "hilite\n");
    if (o->autohilite) fprintf(f, "autohilite\n");
    if (o->textsize) fprintf(f, "textsize %d\n", o->textsize);
    /* Écrit seulement s'il a été posé explicitement : zéro veut dire
     * « déduit du corps », et les piles enregistrées avant l'existence
     * de cette ligne se relisent donc sans rien perdre. */
    if (o->textheight) fprintf(f, "textheight %d\n", o->textheight);
    if (!o->showname) fprintf(f, "hidename\n");   /* nom masqué (défaut = affiché) */
    /* Comme « hidename » : on n'écrit que l'exception. Le défaut étant actif,
     * une pile où personne n'a désactivé de bouton reste identique à ce qu'une
     * version antérieure écrivait — et relisible par elle. */
    if (!o->enabled)  fprintf(f, "disabled\n");
    /* Même règle : on n'écrit que l'exception, le défaut étant partagé. */
    if (o->type == OBJ_BUTTON && !o->shared_hilite)
        fprintf(f, "unsharedhilite\n");
    if (o->icon) fprintf(f, "icon %d\n", o->icon);
    if (o->selectedline) fprintf(f, "selectedline %d\n", o->selectedline);
    if (o->locktext) fprintf(f, "locktext\n");
    if (o->wide_margins) fprintf(f, "widemargins\n");
    /* Écrites seulement si posées : une pile enregistrée avant l'existence de
     * ces lignes se relit donc sans rien perdre, et le fichier ne s'alourdit
     * pas de valeurs par défaut. */
    if (o->marked)         fprintf(f, "marked\n");
    if (o->auto_select)    fprintf(f, "autoselect\n");
    if (o->multiple_lines) fprintf(f, "multiplelines\n");
    if (o->dont_wrap)      fprintf(f, "dontwrap\n");
    if (o->text_align)     fprintf(f, "textalign %d\n", o->text_align);
    if (o->fixed_lh) fprintf(f, "fixedlineheight\n");
    if (o->show_lines) fprintf(f, "showlines\n");
    fprintf(f, "id %d\n", o->id);
    if (o->auto_tab) fprintf(f, "autotab\n");
    if (o->dont_search) fprintf(f, "dontsearch\n");
    if (o->shared_text) fprintf(f, "sharedtext\n");
    if (o->textfont && *o->textfont) fprintf(f, "textfont %s\n", o->textfont);
    if (o->textstyle) fprintf(f, "textstyle %d\n", o->textstyle);
    if (o->type == OBJ_FIELD) put_runs(f, "run", &o->runs);
    if (o->scroll) fprintf(f, "scroll %d\n", o->scroll);
    fprintf(f, "end %s\n", kind);
}

int hc_save(Object *stack, const char *path)
{
    if (!stack || stack->type != OBJ_STACK) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "-- pile HyperCard (format maison v1)\n\n");

    fprintf(f, "stack "); put_quoted(f, stack->name); fputc('\n', f);
    fprintf(f, "size %d,%d\n", stack->w, stack->h);
    put_block(f, "script", stack->script);
    /* Les icônes tiennent dans le bloc de la pile : elles lui appartiennent,
     * et se relisent donc avant la première carte susceptible de s'y référer. */
    for (int i = 0; i < stack->nicons; i++) put_icon(f, &stack->icons[i]);
    fprintf(f, "end stack\n\n");

    /* les fonds d'abord : les cartes s'y réfèrent par leur nom */
    /* les fonds d'abord : les cartes s'y réfèrent par leur nom */
    for (int i = 0; i < stack->nparts; i++) {
        Object *bg = stack->parts[i];
        if (bg->type != OBJ_BACKGROUND) continue;

        /* Un fond sans carte n'a pas d'existence dans HyperCard : il n'y est
         * jamais créé seul, et disparaît avec sa dernière carte. On ne le
         * réécrit donc pas — sans quoi les coquilles vides se transmettent
         * d'enregistrement en enregistrement, et deux fonds homonymes rendent
         * « go background "x" » imprévisible selon lequel resolve() trouve. */
        int utilise = 0;
        for (int j = 0; j < stack->nparts && !utilise; j++)
            if (stack->parts[j]->type == OBJ_CARD && stack->parts[j]->bg == bg)
                utilise = 1;
        if (!utilise) continue;

        fprintf(f, "background "); put_quoted(f, bg->name); fputc('\n', f);
        fprintf(f, "id %d\n", bg->id);
        put_block(f, "script", bg->script);
        put_paint(f, bg->paint);
        for (int j = 0; j < bg->nparts; j++) put_part(f, bg->parts[j]);
        fprintf(f, "end background\n\n");
    }

    for (int i = 0; i < stack->nparts; i++) {
        Object *c = stack->parts[i];
        if (c->type != OBJ_CARD) continue;
        fprintf(f, "card "); put_quoted(f, c->name);
        if (c->bg && c->bg->name) { fprintf(f, " background "); put_quoted(f, c->bg->name); }
        fprintf(f, "\n");
        fprintf(f, "id %d\n", c->id);
        if (c->marked) fprintf(f, "marked\n");
        put_block(f, "script", c->script);
        put_paint(f, c->paint);
        /* L'allumage des boutons de fond NON PARTAGÉS appartient à la carte.
         * Une ligne par bouton allumé : l'absence vaut éteint, ce qui évite
         * une ligne par bouton et par carte dans une pile ordinaire. */
        for (int j = 0; j < c->nbghilites; j++)
            if (c->bghilites[j].hilite)
                fprintf(f, "bghilite %d\n", c->bghilites[j].button_id);

        for (int j = 0; j < c->nbgtexts; j++) {
            fprintf(f, "bgtext %d\n", c->bgtexts[j].field_id);
            put_block(f, "bgtextdata", c->bgtexts[j].text);
            /* le style suit le texte : un champ de fond non partagé a un
               style par carte, exactement comme il a un texte par carte */
            put_runs(f, "bgrun", &c->bgtexts[j].runs);
        }
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
            int len = 0;
            int keep = (found == which);
            /* Un guillemet précédé d'une contre-oblique fait partie du nom :
               il ne referme pas la chaîne (voir put_quoted). */
            while (*p && *p != '"') {
                char c = *p;
                if (c == '\\' && (p[1] == '"' || p[1] == '\\')) { p++; c = *p; }
                if (keep && len < outlen - 1) out[len++] = c;
                p++;
            }
            if (keep) { out[len] = '\0'; return 1; }
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

/* Comme acc_line, mais sans ajouter de saut de ligne : le bloc paint est du
 * base64 découpé à l'écriture, il doit se recoller à l'identique. */
static void acc_join(Acc *a, const char *s)
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
    a->buf[a->len] = '\0';
}

static char *acc_take(Acc *a)
{
    char *r = a->buf;
    a->buf = NULL; a->len = a->cap = 0;
    return r;
}

/* Ajoute une plage de style à une liste. On écrit directement dans la
 * struct RunList plutôt que de passer par hc_run_add : celui-ci vise la liste
 * « active » (celle de la carte courante pour un champ de fond non partagé),
 * or au chargement il n'y a pas encore de carte courante et c'est une liste
 * précise que l'on veut remplir. Le fichier a été écrit trié et fusionné,
 * donc pas besoin de normaliser. */
/* « s,l,st » (ancienne forme) ou « s,l,st,taille,police ». Le nom de police
 * vient en dernier et court jusqu'au bout de la ligne : il peut donc contenir
 * des espaces (« Times New Roman ») sans qu'on ait à le citer. */
static int parse_run(const char *s, int *start, int *len, int *style,
                     int *size, char *font, int fontlen, int *color)
{
    *size = 0; font[0] = '\0'; *color = HC_COLOR_INHERIT;
    if (sscanf(s, "%d,%d,%d", start, len, style) != 3) return 0;

    const char *p = s;
    for (int commas = 0; *p && commas < 3; p++)
        if (*p == ',') commas++;
    if (!*p) return 1;                       /* forme courte : rien de plus */

    *size = atoi(p);
    const char *q = strchr(p, ',');
    if (!q) return 1;                        /* taille sans police */
    q++;

    /* La couleur, s'il y en a une, suit la DERNIÈRE virgule : un nom de police
     * peut contenir des espaces mais jamais de virgule, donc la découpe est
     * sans ambiguïté. Son absence laisse la sentinelle, et les piles écrites
     * avant l'existence de ce champ se relisent sans rien perdre. */
    const char *derniere = strrchr(q, ',');
    int n;
    if (derniere) {
        *color = atoi(derniere + 1);
        n = (int)(derniere - q);
    } else {
        n = (int)strlen(q);
    }
    while (n > 0 && (q[n-1] == '\n' || q[n-1] == '\r')) n--;
    if (n >= fontlen) n = fontlen - 1;
    if (n < 0) n = 0;
    memcpy(font, q, (size_t)n); font[n] = '\0';
    return 1;
}

static void add_run(struct RunList *rl, int start, int len, int style,
                    int size, const char *font, int color)
{
    if (!rl || len <= 0 || start < 0) return;
    if (style == HC_STYLE_INHERIT && size == 0 && (!font || !*font) &&
        color == HC_COLOR_INHERIT) return;
    if (rl->n == rl->cap) {
        int cap = rl->cap ? rl->cap * 2 : 8;
        struct TextRun *v = (struct TextRun *)realloc(rl->v, (size_t)cap * sizeof *v);
        if (!v) return;
        rl->v = v; rl->cap = cap;
    }
    rl->v[rl->n].start = start;
    rl->v[rl->n].len   = len;
    rl->v[rl->n].style = style;
    rl->v[rl->n].size  = size;
    rl->v[rl->n].font  = (font && *font) ? dupstr_file(font) : NULL;
    rl->v[rl->n].color = color;
    rl->n++;
}

/* Un chiffre hexadécimal, ou -1. On ne se repose pas sur sscanf : une ligne
 * tronquée ou salie doit interrompre le remplissage sans écrire n'importe
 * quoi dans les 128 octets, et surtout sans déborder. */
static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
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
    int in_script = 0, in_contents = 0, in_paint = 0, in_bgtext = 0;
    int in_icon = 0;
    struct StackIcon *cur_icon = NULL;   /* icône en cours de remplissage */
    int icon_pos = 0;                    /* octets déjà lus, 0..HC_ICON_BYTES */
    int bgtext_id = 0;
    int last_bgtext = -1;   /* index de la dernière entrée bgtext créée : les
                               lignes « bgrun » qui suivent s'y rattachent */

    while (fgets(line, sizeof line, f)) {
        rtrim(line);
        char *s = ltrim(line);

        /* --- lignes d'un bloc --- */
        if (in_script || in_contents || in_paint || in_bgtext || in_icon) {
            if (s[0] == '|') {
                const char *piece = (s[1] == ' ') ? s + 2 : s + 1;
                if (in_icon) {
                    /* Paires de chiffres hexadécimaux. On s'arrête au premier
                     * caractère qui n'en est pas un, et de toute façon à
                     * HC_ICON_BYTES : une ligne trop longue ne déborde pas. */
                    for (const char *p = piece; p[0] && p[1]; p += 2) {
                        int hi = hexval((unsigned char)p[0]);
                        int lo = hexval((unsigned char)p[1]);
                        if (hi < 0 || lo < 0) break;
                        if (icon_pos >= HC_ICON_BYTES) break;
                        if (cur_icon) cur_icon->bits[icon_pos] = (unsigned char)(hi * 16 + lo);
                        icon_pos++;
                    }
                }
                else if (in_paint) acc_join(&acc, piece);   /* base64 : recoller */
                else               acc_line(&acc, piece);
                continue;
            }
            if (strcmp(s, "end iconres") == 0) {
                in_icon = 0; cur_icon = NULL; icon_pos = 0;
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
                if (target && (target->type == OBJ_FIELD || target->type == OBJ_BUTTON)) {
                    free(target->contents);
                    target->contents = t ? t : NULL;
                } else free(t);
                in_contents = 0;
                continue;
            }
            if (strcmp(s, "end bgtextdata") == 0) {
                char *t = acc_take(&acc);
                if (owner && owner->type == OBJ_CARD && bgtext_id) {
                    if (owner->nbgtexts == owner->capbgtexts) {
                        int cap = owner->capbgtexts ? owner->capbgtexts * 2 : 4;
                        struct BgText *bp = realloc(owner->bgtexts, (size_t)cap * sizeof *bp);
                        if (bp) { owner->bgtexts = bp; owner->capbgtexts = cap; }
                    }
                    if (owner->nbgtexts < owner->capbgtexts) {
                        struct BgText *e = &owner->bgtexts[owner->nbgtexts];
                        e->field_id = bgtext_id;
                        e->text = t ? t : dupstr_file("");
                        /* realloc rend de la mémoire non initialisée : sans ce
                           nettoyage, la liste de plages part sur un pointeur
                           bidon et hc_free y passe. */
                        memset(&e->runs, 0, sizeof e->runs);
                        last_bgtext = owner->nbgtexts;
                        owner->nbgtexts++;
                        t = NULL;
                    }
                }
                free(t);
                in_bgtext = 0;
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
        /* Allumage d'un bouton de fond non partagé, sur CETTE carte.
         * `owner` désigne le fond ou la carte en cours : on vérifie donc que
         * c'est bien une carte, un fond n'ayant pas de table d'allumage. */
        if (strncmp(s, "bghilite ", 9) == 0 && owner && owner->type == OBJ_CARD) {
            hc_set_hilite_raw(owner, atoi(s + 9), 1);
            continue;
        }
        if (strncmp(s, "bgtext ", 7) == 0) { bgtext_id = atoi(s + 7); continue; }
        if (strcmp(s, "bgtextdata") == 0)  { in_bgtext = 1; continue; }

        /* --- icône de pile ---
         * L'en-tête porte le numéro puis le nom : iconres 20554 "Terminator".
         * L'entrée est créée vide, les lignes « | » la remplissent ; une icône
         * dont le bloc serait tronqué garde donc ses octets manquants à zéro
         * plutôt que de disparaître. */
        if (strncmp(s, "iconres ", 8) == 0 && stack) {
            int iid = atoi(s + 8);
            if (!get_quoted(s, 0, nm, sizeof nm)) nm[0] = 0;
            cur_icon = hc_icon_add(stack, iid, nm);
            icon_pos = 0;
            in_icon  = 1;
            continue;
        }

        /* --- plages de style --- */
        if (strncmp(s, "run ", 4) == 0) {
            int a, b, c, sz, co; char fn[128];
            if (part && part->type == OBJ_FIELD &&
                parse_run(s + 4, &a, &b, &c, &sz, fn, sizeof fn, &co))
                add_run(&part->runs, a, b, c, sz, fn, co);
            continue;
        }
        if (strncmp(s, "bgrun ", 6) == 0) {
            int a, b, c, sz, co; char fn[128];
            if (owner && owner->type == OBJ_CARD &&
                last_bgtext >= 0 && last_bgtext < owner->nbgtexts &&
                parse_run(s + 6, &a, &b, &c, &sz, fn, sizeof fn, &co))
                add_run(&owner->bgtexts[last_bgtext].runs, a, b, c, sz, fn, co);
            continue;
        }

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
            last_bgtext = -1;
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
        if (strcmp(s, "marked") == 0) {
            if (target) target->marked = 1;
            continue;
        }
        if (strcmp(s, "autohilite") == 0) {
            if (target) target->autohilite = 1;
            continue;
        }
        if (strncmp(s, "textheight ", 11) == 0 && part) {
            part->textheight = atoi(s + 11);
            continue;
        }
        if (strncmp(s, "textsize ", 9) == 0 && part) {
            part->textsize = atoi(s + 9);
            continue;
        }
        if (strncmp(s, "icon ", 5) == 0 && part) {
            part->icon = atoi(s + 5);
            continue;
        }
        if (strncmp(s, "selectedline ", 13) == 0 && part) {
            part->selectedline = atoi(s + 13);
            continue;
        }
        if (strcmp(s, "locktext") == 0 && part)       { part->locktext = 1; continue; }
        if (strcmp(s, "widemargins") == 0 && part)    { part->wide_margins = 1; continue; }
        if (strcmp(s, "marked") == 0 && target)        { target->marked = 1; continue; }
        if (strcmp(s, "autoselect") == 0 && part)     { part->auto_select = 1; continue; }
        if (strcmp(s, "multiplelines") == 0 && part)  { part->multiple_lines = 1; continue; }
        if (strcmp(s, "dontwrap") == 0 && part)       { part->dont_wrap = 1; continue; }
        if (strncmp(s, "textalign ", 10) == 0 && part) { part->text_align = atoi(s + 10); continue; }
        if (strcmp(s, "fixedlineheight") == 0 && part) { part->fixed_lh = 1; continue; }
        if (strcmp(s, "showlines") == 0 && part)      { part->show_lines = 1; continue; }
        if (strcmp(s, "autotab") == 0 && part)        { part->auto_tab = 1; continue; }
        if (strcmp(s, "dontsearch") == 0 && part)     { part->dont_search = 1; continue; }
        if (strcmp(s, "sharedtext") == 0 && part)     { part->shared_text = 1; continue; }
        if (strncmp(s, "textfont ", 9) == 0 && part) {
            free(part->textfont);
            part->textfont = dupstr_file(s + 9);
            continue;
        }
        if (strncmp(s, "textstyle ", 10) == 0 && part) {
            part->textstyle = atoi(s + 10);
            continue;
        }
        if (strncmp(s, "id ", 3) == 0 && target) {
            hc_set_id(target, atoi(s + 3));
            continue;
        }
        if (strncmp(s, "scroll ", 7) == 0 && part) {
            part->scroll = atoi(s + 7);
            continue;
        }
        if (strcmp(s, "hidename") == 0 && part) {
            part->showname = 0;
            continue;
        }
        if (strcmp(s, "disabled") == 0 && part) {
            part->enabled = 0;
            continue;
        }
        if (strcmp(s, "unsharedhilite") == 0 && part) {
            part->shared_hilite = 0;
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
            owner = NULL; target = stack; last_bgtext = -1; continue;
        }
        if (strcmp(s, "end stack") == 0) { target = NULL; continue; }
    }

    free(acc.buf);
    fclose(f);
    return stack;
}
