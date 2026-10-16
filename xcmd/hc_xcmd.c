/* hc_xcmd.c — ce que chaque module emporte avec lui.
 *
 * Un module tourne sans système : pas de bibliothèque C, pas de fichiers,
 * pas d'écran. Ce fichier lui donne le strict nécessaire — la mémoire, les
 * chaînes, snprintf — et la BOÎTE AUX PARAMÈTRES, où HC dépose le bloc et
 * les chaînes avant chaque appel.
 *
 * C'est une bibliothèque RÉDUITE, et elle le dit :
 *   · free ne rend rien : la mémoire allouée par un module ne fait que
 *     croître, jusqu'à la borne que HC lui fixe ;
 *   · snprintf ne connaît que %d %u %x %c %s et %% ;
 *   · rien pour les nombres à virgule, rien pour les fichiers. */

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* ---- la mémoire ---- */

/* La mémoire libre commence où l'éditeur de liens la pose. */
extern unsigned char __heap_base;
static unsigned char *g_tas = &__heap_base;

void *malloc(size_t n)
{
    n = (n + 15) & ~(size_t)15;
    unsigned char *p = g_tas;
    size_t fin = (size_t)(p + n);
    size_t pages = __builtin_wasm_memory_size(0);
    if (fin > pages * 65536) {
        size_t manque = (fin - pages * 65536 + 65535) / 65536;
        if (__builtin_wasm_memory_grow(0, manque) == (size_t)-1) return NULL;
    }
    g_tas = p + n;
    return p;
}

void free(void *p) { (void)p; }

void *calloc(size_t a, size_t b)
{
    if (b && a > (size_t)-1 / b) return NULL;
    void *p = malloc(a * b);
    return p ? memset(p, 0, a * b) : NULL;
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
