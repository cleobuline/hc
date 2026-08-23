/* hct_val.c — valeurs HyperTalk. Voir hct_val.h pour les règles. */

#include "hct_val.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>

/* ------------------------------------------------------- construction */

HctValeur hct_val_vide(void)
{
    HctValeur v;
    v.txt = malloc(1);
    if (v.txt) v.txt[0] = '\0';
    v.len = 0;
    return v;
}

HctValeur hct_val_texte_n(const char *s, int len)
{
    HctValeur v;
    if (!s || len < 0) len = 0;
    v.txt = malloc((size_t)len + 1);
    if (!v.txt) { v.len = 0; return v; }
    if (len) memcpy(v.txt, s, (size_t)len);
    v.txt[len] = '\0';
    v.len = len;
    return v;
}

HctValeur hct_val_texte(const char *s)
{
    return hct_val_texte_n(s, s ? (int)strlen(s) : 0);
}

HctValeur hct_val_nombre(double x)
{
    char buf[64];
    int n = hct_ecrit_nombre(x, buf, sizeof buf);
    return hct_val_texte_n(buf, n);
}

HctValeur hct_val_bool(int vrai)
{
    return hct_val_texte(vrai ? "true" : "false");
}

HctValeur hct_val_copie(HctValeur v)
{
    return hct_val_texte_n(v.txt, v.len);
}

void hct_val_libere(HctValeur *v)
{
    if (!v) return;
    free(v->txt);
    v->txt = NULL;
    v->len = 0;
}

/* ----------------------------------------------------------- lecture */

static const char *saute_blancs(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

int hct_est_nombre(const char *s)
{
    if (!s) return 0;
    s = saute_blancs(s);
    if (!*s) return 0;

    if (*s == '+' || *s == '-') s++;

    int chiffres = 0;
    while (isdigit((unsigned char)*s)) { s++; chiffres++; }
    if (*s == '.') {
        s++;
        while (isdigit((unsigned char)*s)) { s++; chiffres++; }
    }
    if (!chiffres) return 0;

    /* Notation scientifique : HyperCard l'accepte en lecture, et l'emploie
     * lui-même pour les très grands nombres. */
    if (*s == 'e' || *s == 'E') {
        const char *garde = s;
        s++;
        if (*s == '+' || *s == '-') s++;
        if (!isdigit((unsigned char)*s)) s = garde;
        else while (isdigit((unsigned char)*s)) s++;
    }

    s = saute_blancs(s);
    return *s == '\0';
}

double hct_vers_nombre(const char *s)
{
    if (!s) return 0.0;
    return strtod(s, NULL);
}

int hct_vers_bool(const char *s, int *valide)
{
    if (valide) *valide = 1;
    if (!s) { if (valide) *valide = 0; return 0; }
    s = saute_blancs(s);

    int n = 0;
    while (s[n] && s[n] != ' ' && s[n] != '\t') n++;

    if (n == 4 && !strncasecmp(s, "true", 4))  return 1;
    if (n == 5 && !strncasecmp(s, "false", 5)) return 0;

    if (valide) *valide = 0;
    return 0;
}

/* ------------------------------------------------- écriture d'un nombre
 *
 * Le format par défaut de HyperCard est « %.6g » à ceci près qu'un entier
 * s'écrit sans décimale, quelle que soit sa taille. On traite donc à part le
 * cas entier, sinon 1000000 s'afficherait « 1e+06 ». */
int hct_ecrit_nombre(double x, char *out, int taille)
{
    if (taille < 2) { if (taille) out[0] = 0; return 0; }

    if (x != x) return snprintf(out, (size_t)taille, "NAN");        /* NaN   */
    if (x > 1e308 || x < -1e308)
        return snprintf(out, (size_t)taille, x > 0 ? "INF" : "-INF");

    /* Entier exact et représentable : on l'écrit tel quel. */
    if (x == floor(x) && fabs(x) < 1e15) {
        long long e = (long long)x;
        return snprintf(out, (size_t)taille, "%lld", e);
    }

    int n = snprintf(out, (size_t)taille, "%.6f", x);
    if (n < 0 || n >= taille) return n < 0 ? 0 : taille - 1;

    /* Retirer les zéros de fin, puis le point s'il ne reste que lui. */
    if (strchr(out, '.')) {
        int i = n - 1;
        while (i > 0 && out[i] == '0') { out[i] = '\0'; i--; }
        if (i > 0 && out[i] == '.') { out[i] = '\0'; i--; }
        n = i + 1;
    }
    return n;
}

/* ------------------------------------------------------- comparaison */

int hct_compare(const char *a, const char *b, int *numerique)
{
    if (!a) a = "";
    if (!b) b = "";

    /* Numérique seulement si les DEUX se lisent comme des nombres. Sans cette
     * condition, « 10 » et « 9a » se compareraient de deux façons selon
     * l'ordre des opérandes. */
    if (hct_est_nombre(a) && hct_est_nombre(b)) {
        if (numerique) *numerique = 1;
        double x = hct_vers_nombre(a), y = hct_vers_nombre(b);
        if (x < y) return -1;
        if (x > y) return  1;
        return 0;
    }

    if (numerique) *numerique = 0;

    /* Texte : la comparaison de HyperTalk ignore la casse. */
    int r = strcasecmp(a, b);
    return r < 0 ? -1 : (r > 0 ? 1 : 0);
}

int hct_egal(const char *a, const char *b)
{
    return hct_compare(a, b, NULL) == 0;
}
