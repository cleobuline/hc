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

/* Le résultat d'un CALCUL : celui-là passe par le numberFormat. Voir la note
 * au-dessus de hct_format_nombre pour la raison d'avoir deux portes. */
HctValeur hct_val_calcul(double x)
{
    char buf[64];
    int n = hct_ecrit_nombre_format(x, buf, sizeof buf);
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

/* ------------------------------------------------------ numberFormat
 *
 * « set the numberFormat to "0.00" » : le gabarit qui met en forme le
 * RÉSULTAT D'UN CALCUL. C'est la définition d'HyperCard, et elle est étroite
 * à dessein — une longueur, un rang, un compteur de boucle ne sont pas des
 * calculs, et les mettre en forme donnerait « char 1.00 of x ». Les sites
 * d'appel choisissent donc : hct_val_nombre reste brut, hct_val_calcul met
 * en forme. La question « est-ce un calcul ? » se tranche là où on connaît
 * la réponse.
 *
 * Le gabarit se lit en deux moitiés, de part et d'autre du point :
 *   à gauche, chaque « 0 » impose un chiffre — « 000 » écrit 7 en « 007 » ;
 *   à droite, chaque « 0 » impose une décimale, chaque « # » l'autorise
 *   sans l'imposer. « 0.0##" » rend 1.5 en « 1.5 » et 1.5678 en « 1.568 ».
 *
 * Le format vide rend le comportement par défaut, qui est exactement
 * « 0.###### » : six décimales, zéros de fin retirés, entiers sans point.
 * C'est le défaut d'HyperCard, et c'est pourquoi il n'a pas fallu l'écrire
 * comme un cas particulier — c'est l'absence de format. */
static char g_format[32] = "";

void hct_format_nombre(const char *f)
{
    if (!f) f = "";
    /* Un gabarit sans chiffre ne veut rien dire : on le lit comme un retour
     * au défaut plutôt que comme une consigne d'écrire zéro chiffre. */
    int utile = 0;
    for (const char *p = f; *p; p++) if (*p == '0' || *p == '#') { utile = 1; break; }
    if (!utile) { g_format[0] = 0; return; }
    snprintf(g_format, sizeof g_format, "%s", f);
}

const char *hct_format_nombre_lu(void) { return g_format; }

/* Décompose le gabarit. Rend 0 s'il n'y en a pas. */
static int format_lu(int *entiers, int *dec_min, int *dec_max)
{
    if (!g_format[0]) return 0;
    *entiers = 0; *dec_min = 0; *dec_max = 0;
    const char *pt = strchr(g_format, '.');
    for (const char *p = g_format; *p && (!pt || p < pt); p++)
        if (*p == '0') (*entiers)++;
    if (pt)
        for (const char *p = pt + 1; *p; p++) {
            if (*p == '0') { (*dec_max)++; *dec_min = *dec_max; }
            else if (*p == '#') (*dec_max)++;
        }
    return 1;
}

int hct_ecrit_nombre_format(double x, char *out, int taille)
{
    int ent, dmin, dmax;
    if (!format_lu(&ent, &dmin, &dmax))
        return hct_ecrit_nombre(x, out, taille);

    if (x != x) return snprintf(out, (size_t)taille, "NAN");
    if (x > 1e308 || x < -1e308)
        return snprintf(out, (size_t)taille, x > 0 ? "INF" : "-INF");

    char brut[64];
    int n = snprintf(brut, sizeof brut, "%.*f", dmax, x);
    if (n < 0) { if (taille) out[0] = 0; return 0; }

    /* Retirer les décimales facultatives inutilisées, jamais les imposées. */
    char *pt = strchr(brut, '.');
    if (pt) {
        int garde = dmin;
        char *fin = brut + strlen(brut) - 1;
        while (fin > pt && *fin == '0' && (int)(fin - pt) > garde) *fin-- = 0;
        if (fin == pt && garde == 0) *pt = 0;
    }

    /* Compléter la partie entière par des zéros de tête. Le signe passe
     * devant : « -007 » et non « 00-7 ». */
    const char *signe = "";
    char *corps = brut;
    if (*corps == '-') { signe = "-"; corps++; }
    int chiffres = 0;
    for (const char *p = corps; *p && *p != '.'; p++) chiffres++;

    char zeros[32];
    int manque = ent - chiffres;
    if (manque < 0) manque = 0;
    if (manque > (int)sizeof zeros - 1) manque = (int)sizeof zeros - 1;
    for (int i = 0; i < manque; i++) zeros[i] = '0';
    zeros[manque] = 0;

    return snprintf(out, (size_t)taille, "%s%s%s", signe, zeros, corps);
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
