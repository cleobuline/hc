/* hct_chunk.c — découpage en morceaux. Voir hct_chunk.h pour les règles. */

#include "hct_chunk.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Un blanc au sens des MOTS de HyperTalk.
 *
 * Le retour à la ligne en est un : « the number of words of "a\nb" » vaut deux,
 * pas un. Ne séparer que sur l'espace et la tabulation collait les mots de part
 * et d'autre d'un saut de ligne — ce qui se voit dès qu'on compte les mots d'un
 * champ de plusieurs lignes.
 *
 * On n'emploie pas isspace() tel quel : sa réponse dépend de la locale, et
 * l'analyse d'un script ne doit pas changer selon les réglages du système. */
static int est_blanc(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

/* ------------------------------------------------------------ comptage */

int hct_chunk_compte(const char *s, HctSorteChunk sorte, char delim)
{
    if (!s) return 0;
    int len = (int)strlen(s);

    switch (sorte) {
        case HCT_CH_CHAR:
            return len;

        case HCT_CH_WORD: {
            /* Les blancs multiples ne comptent pas : on avance jusqu'au
             * premier caractère non blanc, et l'on compte les groupes. */
            int n = 0, i = 0;
            while (i < len) {
                while (i < len && est_blanc(s[i])) i++;
                if (i >= len) break;
                n++;
                while (i < len && !est_blanc(s[i])) i++;
            }
            return n;
        }

        case HCT_CH_ITEM: {
            /* Un séparateur final CRÉE un item vide : « a,b, » en compte
             * trois. C'est l'inverse des lignes, où « a\nb\n » en vaut deux —
             * une dissymétrie de HyperCard, pas une inattention. */
            if (len == 0) return 0;
            int n = 1;
            for (int i = 0; i < len; i++) if (s[i] == delim) n++;
            return n;
        }

        case HCT_CH_LINE: {
            if (len == 0) return 0;
            int n = 1;
            for (int i = 0; i < len; i++) if (s[i] == '\n') n++;
            /* Un saut de ligne final ne crée pas de ligne vide de plus :
             * HyperCard compte « a\nb\n » pour deux lignes. */
            if (len && s[len-1] == '\n') n--;
            return n;
        }
    }
    return 0;
}

/* ------------------------------------------------------------- bornes */

/* Bornes du morceau simple de rang n (1-based). trouve=0 si dépassement. */
static HctBornes borne_simple(const char *s, HctSorteChunk sorte, int n,
                              char delim)
{
    HctBornes b = { 0, 0, 0 };
    if (!s || n < 1) return b;
    int len = (int)strlen(s);

    if (sorte == HCT_CH_CHAR) {
        if (n > len) { b.deb = b.fin = len; return b; }
        b.deb = n - 1; b.fin = n; b.trouve = 1;
        return b;
    }

    if (sorte == HCT_CH_WORD) {
        int i = 0, k = 0;
        while (i < len) {
            while (i < len && est_blanc(s[i])) i++;
            if (i >= len) break;
            k++;
            int deb = i;
            while (i < len && !est_blanc(s[i])) i++;
            if (k == n) { b.deb = deb; b.fin = i; b.trouve = 1; return b; }
        }
        b.deb = b.fin = len;
        return b;
    }

    /* item et line : le séparateur crée un morceau, même vide. */
    char sep = (sorte == HCT_CH_ITEM) ? delim : '\n';
    int k = 1, deb = 0, i = 0;
    for (; i <= len; i++) {
        if (i == len || s[i] == sep) {
            if (k == n) { b.deb = deb; b.fin = i; b.trouve = 1; return b; }
            k++;
            deb = i + 1;
            if (i == len) break;
        }
    }
    b.deb = b.fin = len;
    return b;
}

HctBornes hct_chunk_bornes(const char *s, HctSorteChunk sorte,
                           int n, int n2, char delim)
{
    HctBornes a = borne_simple(s, sorte, n, delim);
    if (n2 <= 0 || n2 == n) return a;

    HctBornes z = borne_simple(s, sorte, n2, delim);

    /* Une plage dont le début existe reste utilisable même si la fin dépasse :
     * « char 3 to 99 of "abcde" » vaut "cde", comme dans HyperCard. */
    HctBornes r;
    r.deb = a.deb;
    r.fin = z.trouve ? z.fin : (int)strlen(s);
    r.trouve = a.trouve;
    if (r.fin < r.deb) r.fin = r.deb;
    return r;
}

/* ------------------------------------------------------------ lecture */

HctValeur hct_chunk_lit(const char *s, HctSorteChunk sorte,
                        int n, int n2, char delim)
{
    if (!s) return hct_val_vide();
    HctBornes b = hct_chunk_bornes(s, sorte, n, n2, delim);
    if (!b.trouve) return hct_val_vide();
    return hct_val_texte_n(s + b.deb, b.fin - b.deb);
}

/* ------------------------------------------------------------ écriture */

HctValeur hct_chunk_ecrit(const char *s, HctSorteChunk sorte,
                          int n, int n2, char delim, const char *val)
{
    if (!s) s = "";
    if (!val) val = "";
    int len = (int)strlen(s), lv = (int)strlen(val);

    HctBornes b = hct_chunk_bornes(s, sorte, n, n2, delim);

    if (b.trouve) {
        int taille = b.deb + lv + (len - b.fin);
        HctValeur r;
        r.txt = malloc((size_t)taille + 1);
        if (!r.txt) { r.len = 0; return r; }
        memcpy(r.txt, s, (size_t)b.deb);
        memcpy(r.txt + b.deb, val, (size_t)lv);
        memcpy(r.txt + b.deb + lv, s + b.fin, (size_t)(len - b.fin));
        r.txt[taille] = '\0';
        r.len = taille;
        return r;
    }

    /* Le rang dépasse : on étend.
     *
     * Pour item et line, HyperCard crée les morceaux vides intermédiaires —
     * « put "x" into item 5 of "a,b" » donne « a,b,,,x ». Pour char et word,
     * il ajoute simplement à la fin, avec un espace pour les mots. */
    int existants = hct_chunk_compte(s, sorte, delim);
    char sep = (sorte == HCT_CH_ITEM) ? delim
             : (sorte == HCT_CH_LINE) ? '\n'
             : (sorte == HCT_CH_WORD) ? ' ' : '\0';

    int manquants = 0;
    if (sorte == HCT_CH_ITEM || sorte == HCT_CH_LINE) {
        manquants = n - existants - 1;
        if (manquants < 0) manquants = 0;
    }

    int besoin_sep = (existants > 0 && sep) ? 1 : 0;
    int taille = len + besoin_sep + manquants + lv;

    HctValeur r;
    r.txt = malloc((size_t)taille + 1);
    if (!r.txt) { r.len = 0; return r; }

    int p = 0;
    memcpy(r.txt + p, s, (size_t)len); p += len;
    if (besoin_sep) r.txt[p++] = sep;
    for (int i = 0; i < manquants; i++) r.txt[p++] = sep;
    memcpy(r.txt + p, val, (size_t)lv); p += lv;
    r.txt[p] = '\0';
    r.len = p;
    return r;
}
