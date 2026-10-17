/* hc_xcmd.c — ce que chaque module emporte avec lui.
 *
 * Un module tourne sans système : pas de bibliothèque C, pas de fichiers,
 * pas d'écran. Ce fichier lui donne le strict nécessaire — la mémoire, les
 * chaînes, snprintf — et la BOÎTE AUX PARAMÈTRES, où HC dépose le bloc et
 * les chaînes avant chaque appel.
 *
 * C'est une bibliothèque RÉDUITE, et elle le dit :
 *   · la mémoire va par tailles en puissances de deux : un bloc rendu par
 *     free resert à un bloc de la même classe, jamais à un autre ;
 *   · snprintf ne connaît que %d %u %x %c %s et %% ;
 *   · rien pour les nombres à virgule, rien pour les fichiers. */

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "hc_xcmd.h"

/* ---- la mémoire ---- */

/* La mémoire libre commence où l'éditeur de liens la pose, et s'étend en
 * demandant des pages à HC, jusqu'à la borne qu'il fixe. */
extern unsigned char __heap_base;
static unsigned char *g_tas = &__heap_base;

static void *tas_prend(size_t n)
{
    unsigned char *p = g_tas;
    size_t fin = (size_t)(p + n);
    if (fin < (size_t)p) return NULL;
    size_t pages = __builtin_wasm_memory_size(0);
    if (fin > pages * 65536) {
        size_t manque = (fin - pages * 65536 + 65535) / 65536;
        if (__builtin_wasm_memory_grow(0, manque) == (size_t)-1) return NULL;
    }
    g_tas = p + n;
    return p;
}

/* LES CLASSES. Un bloc de la classe c fait 16 << c octets, en-tête compris ;
 * l'en-tête (16 octets, pour garder l'alignement) retient la classe. Un bloc
 * libéré va dans la liste de sa classe, et le prochain malloc de cette
 * classe le reprend. Simple, et sans émiettement : un module qui demande
 * puis rend les mêmes tailles — les chaînes des rappels — ne grossit plus. */
enum { ENTETE = 16, CLASSES = 27 };
static void *g_libres[CLASSES];

static int classe_de(size_t n)
{
    int c = 0;
    while (c < CLASSES && ((size_t)ENTETE << c) < n) c++;
    return c;
}

void *malloc(size_t n)
{
    if (n > ((size_t)ENTETE << (CLASSES - 1)) - ENTETE) return NULL;
    int c = classe_de(n + ENTETE);
    unsigned char *b = g_libres[c];
    if (b) g_libres[c] = *(void **)b;
    else if (!(b = tas_prend((size_t)ENTETE << c))) return NULL;
    *(int *)b = c;
    return b + ENTETE;
}

void free(void *p)
{
    if (!p) return;
    unsigned char *b = (unsigned char *)p - ENTETE;
    int c = *(int *)b;
    *(void **)b = g_libres[c];
    g_libres[c] = b;
}

void *calloc(size_t a, size_t b)
{
    if (b && a > (size_t)-1 / b) return NULL;
    void *p = malloc(a * b);
    return p ? memset(p, 0, a * b) : NULL;
}

void *realloc(void *p, size_t n)
{
    if (!p) return malloc(n);
    int c = *(int *)((unsigned char *)p - ENTETE);
    size_t place = ((size_t)ENTETE << c) - ENTETE;
    if (n <= place) return p;
    void *q = malloc(n);
    if (!q) return NULL;
    memcpy(q, p, place);
    free(p);
    return q;
}

/* LA BOÎTE AUX PARAMÈTRES. HC demande la place dont il a besoin, y écrit le
 * bloc puis les chaînes, et appelle. La boîte resert d'un appel à l'autre ;
 * elle ne grandit que si un appel demande plus que les précédents. */
__attribute__((export_name("hc_boite")))
void *hc_boite(size_t n);
void *hc_boite(size_t n)
{
    static unsigned char *boite;
    static size_t taille;
    if (n > taille) {
        unsigned char *b = malloc(n);
        if (!b) return NULL;
        boite = b;
        taille = n;
    }
    return boite;
}

/* ---- les chaînes ---- */

void *memset(void *d, int c, size_t n)
{
    unsigned char *q = d;
    while (n--) *q++ = (unsigned char)c;
    return d;
}

void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *q = d;
    const unsigned char *r = s;
    while (n--) *q++ = *r++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *q = d;
    const unsigned char *r = s;
    if (q < r) {
        while (n--) *q++ = *r++;
    } else {
        q += n;
        r += n;
        while (n--) *--q = *--r;
    }
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n; n--, x++, y++)
        if (*x != *y) return *x - *y;
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        if (!*a) return 0;
    }
    return 0;
}

char *strcpy(char *d, const char *s)
{
    char *r = d;
    while ((*d = *s) != '\0') { d++; s++; }
    return r;
}

char *strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = '\0';
    return d;
}

char *strcat(char *d, const char *s)
{
    strcpy(d + strlen(d), s);
    return d;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return NULL;
    }
}

char *strstr(const char *h, const char *n)
{
    size_t k = strlen(n);
    if (!k) return (char *)h;
    for (; *h; h++)
        if (!strncmp(h, n, k)) return (char *)h;
    return NULL;
}

int toupper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
int tolower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
int isspace(int c) { return c == ' ' || (c >= 9 && c <= 13); }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalpha(int c) { return (c | 32) >= 'a' && (c | 32) <= 'z'; }
int isalnum(int c) { return isalpha(c) || isdigit(c); }

/* ---- les nombres entiers ----
 *
 * Ce que StrToLong et StrToNum faisaient chez HyperCard : un paramètre de
 * HyperTalk est du texte, « 12 » compris. */

long strtol(const char *s, char **fin, int base)
{
    const char *p = s;
    while (isspace((unsigned char)*p)) p++;
    int moins = 0;
    if (*p == '-' || *p == '+') moins = *p++ == '-';
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] | 32) == 'x'
        && isalnum((unsigned char)p[2])) {
        p += 2;
        base = 16;
    } else if (base == 0) {
        base = p[0] == '0' ? 8 : 10;
    }
    unsigned long v = 0;
    const char *debut = p;
    for (;; p++) {
        int c = (unsigned char)*p, d;
        if (isdigit(c)) d = c - '0';
        else if (isalpha(c)) d = (c | 32) - 'a' + 10;
        else break;
        if (d >= base) break;
        v = v * (unsigned long)base + (unsigned long)d;
    }
    if (fin) *fin = (char *)(p == debut ? s : p);
    return moins ? -(long)v : (long)v;
}

int atoi(const char *s) { return (int)strtol(s, NULL, 10); }

/* ---- snprintf, réduit ---- */

typedef struct { char *s; size_t n, k; } Sortie;

static void met(Sortie *o, char c)
{
    if (o->k + 1 < o->n) o->s[o->k] = c;
    o->k++;
}

static void met_nombre(Sortie *o, unsigned long v, unsigned base)
{
    char t[24];
    int i = 0;
    do {
        unsigned d = (unsigned)(v % base);
        t[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= base;
    } while (v);
    while (i) met(o, t[--i]);
}

int vsnprintf(char *s, size_t n, const char *f, va_list ap)
{
    Sortie o = { s, n, 0 };
    for (; *f; f++) {
        if (*f != '%') { met(&o, *f); continue; }
        switch (*++f) {
        case 'd': {
            int v = va_arg(ap, int);
            if (v < 0) { met(&o, '-'); met_nombre(&o, 0UL - (unsigned long)v, 10); }
            else met_nombre(&o, (unsigned long)v, 10);
            break;
        }
        case 'u': met_nombre(&o, va_arg(ap, unsigned), 10); break;
        case 'x': met_nombre(&o, va_arg(ap, unsigned), 16); break;
        case 'c': met(&o, (char)va_arg(ap, int)); break;
        case 's': {
            const char *t = va_arg(ap, const char *);
            if (!t) t = "(null)";
            while (*t) met(&o, *t++);
            break;
        }
        case '\0': f--; break;
        default: met(&o, *f); break;
        }
    }
    if (n) s[o.k < n ? o.k : n - 1] = '\0';
    return (int)o.k;
}

int snprintf(char *s, size_t n, const char *f, ...)
{
    va_list ap;
    va_start(ap, f);
    int k = vsnprintf(s, n, f, ap);
    va_end(ap);
    return k;
}

/* ---- les rappels qui rendent une chaîne ----
 *
 * HC garde la chaîne de son côté et n'en dit que la longueur, ou -1 ; le
 * module fait la place, puis la fait copier (hc_rendu). HC n'écrit donc
 * jamais dans un tas qu'il ne connaît pas. Les importés portent les noms
 * de HyperXCmd.h ; les fonctions de même nom, plus bas, sont celles que
 * le module appelle. */
HC_RAPPEL(EvalExpr)       int  hc_EvalExpr(const char *expr);
HC_RAPPEL(GetGlobal)      int  hc_GetGlobal(const char *nom);
HC_RAPPEL(SetGlobal)      void hc_SetGlobal(const char *nom, const char *v);
HC_RAPPEL(GetFieldByName) int  hc_GetFieldByName(int carte, const char *nom);
HC_RAPPEL(GetFieldByNum)  int  hc_GetFieldByNum(int carte, int numero);
HC_RAPPEL(GetFieldByID)   int  hc_GetFieldByID(int carte, int id);
HC_RAPPEL(SetFieldByName) int  hc_SetFieldByName(int carte, const char *nom,
                                                 const char *v);
HC_RAPPEL(SetFieldByNum)  int  hc_SetFieldByNum(int carte, int numero,
                                                const char *v);
HC_RAPPEL(SetFieldByID)   int  hc_SetFieldByID(int carte, int id,
                                               const char *v);
HC_RAPPEL(hc_rendu)       void hc_rendu(char *tampon, int taille);

static char *rendu(int n)
{
    if (n < 0) return NULL;
    char *s = malloc((size_t)n + 1);
    if (s) hc_rendu(s, n + 1);
    return s;
}

char *EvalExpr(const char *expr)       { return rendu(hc_EvalExpr(expr)); }
char *GetGlobal(const char *nom)       { return rendu(hc_GetGlobal(nom)); }
void  SetGlobal(const char *nom, const char *valeur)
{
    hc_SetGlobal(nom, valeur ? valeur : "");
}
char *GetFieldByName(int carte, const char *nom)
{
    return rendu(hc_GetFieldByName(carte, nom));
}
char *GetFieldByNum(int carte, int numero)
{
    return rendu(hc_GetFieldByNum(carte, numero));
}
char *GetFieldByID(int carte, int id)
{
    return rendu(hc_GetFieldByID(carte, id));
}
int SetFieldByName(int carte, const char *nom, const char *valeur)
{
    return hc_SetFieldByName(carte, nom, valeur ? valeur : "");
}
int SetFieldByNum(int carte, int numero, const char *valeur)
{
    return hc_SetFieldByNum(carte, numero, valeur ? valeur : "");
}
int SetFieldByID(int carte, int id, const char *valeur)
{
    return hc_SetFieldByID(carte, id, valeur ? valeur : "");
}

/* ---- les fenêtres ----
 *
 * Les rectangles passent en quatre entiers, dans l'ordre des scripts —
 * gauche, haut, droite, bas — ; les images et les tampons, en adresse et
 * longueur, que HC vérifie avant d'y toucher. */
HC_RAPPEL(NewXWindow)     int  hc_NewXWindow(int g, int h, int d, int b,
                                             const char *titre, int visible,
                                             int style, int flottante);
HC_RAPPEL(CloseXWindow)   void hc_CloseXWindow(int w);
HC_RAPPEL(DrawXWindow)    void hc_DrawXWindow(int w, const unsigned char *px,
                                              int npx, int largeur, int hauteur,
                                              const unsigned char *pal,
                                              int npal, int echelle);
HC_RAPPEL(CopyXWindow)    int  hc_CopyXWindow(int w);
HC_RAPPEL(Button)         int  hc_Button(void);
HC_RAPPEL(GetCardPicture) int  hc_GetCardPicture(int g, int h, int d, int b,
                                                 unsigned char *noir, int n);

WindowPtr NewXWindow(const Rect *r, const char *title, int visible,
                     int procID, int color, int floating)
{
    (void)color;
    if (!r) return 0;
    return hc_NewXWindow(r->left, r->top, r->right, r->bottom,
                         title ? title : "", visible != 0, procID,
                         floating != 0);
}

void CloseXWindow(WindowPtr window) { hc_CloseXWindow(window); }

void DrawXWindow(WindowPtr window, const unsigned char *pixels,
                 int largeur, int hauteur, const unsigned char *palette,
                 int echelle)
{
    if (!pixels || !palette || largeur <= 0 || hauteur <= 0
        || largeur > 0x7fffffff / hauteur)
        return;
    hc_DrawXWindow(window, pixels, largeur * hauteur, largeur, hauteur,
                   palette, 768, echelle);
}

int CopyXWindow(WindowPtr window) { return hc_CopyXWindow(window); }

int Button(void) { return hc_Button(); }

int GetCardPicture(const Rect *r, unsigned char *noir)
{
    if (!r || !noir) return 0;
    int l = r->right - r->left, h = r->bottom - r->top;
    if (l <= 0 || h <= 0 || l > 0x7fffffff / h) return 0;
    return hc_GetCardPicture(r->left, r->top, r->right, r->bottom, noir, l * h);
}

int StrToRect(const char *s, Rect *r)
{
    int v[4];
    if (!s || !r) return 0;
    for (int k = 0; k < 4; k++) {
        char *fin;
        v[k] = (int)strtol(s, &fin, 10);
        if (fin == s) return 0;
        s = fin;
        while (*s == ' ') s++;
        if (k < 3) {
            if (*s != ',') return 0;
            s++;
        }
    }
    r->left = (short)v[0];
    r->top = (short)v[1];
    r->right = (short)v[2];
    r->bottom = (short)v[3];
    return 1;
}

/* ---- les bulles d'aide ---- */

HC_RAPPEL(HMGetBalloons)  int  hc_HMGetBalloons(void);
HC_RAPPEL(HMShowBalloon)  int  hc_HMShowBalloon(const char *texte, int g, int h,
                                                int d, int b);

int HMGetBalloons(void) { return hc_HMGetBalloons(); }

int HMShowBalloon(const char *texte, const Rect *zone)
{
    if (!zone) return 0;
    return hc_HMShowBalloon(texte ? texte : "", zone->left, zone->top,
                            zone->right, zone->bottom);
}
