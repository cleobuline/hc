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

/* LA FIN DU MOT QUI COMMENCE EN `i`, GUILLEMETS COMPRIS.
 *
 * En HyperTalk un mot est « une suite de caractères sans espace, OU un texte
 * entre guillemets ». Ce n'est pas un détail de confort : l'idiome canonique
 * pour retrouver le nom de sa propre pile en dépend —
 *
 *     get the value of word 2 of the long name of me
 *
 * où « the long name » rend « stack "Graph Maker" ». Sans la règle, word 2
 * valait « "Graph » — un guillemet ouvert, que « the value of » refusait
 * ensuite avec « guillemet fermant manquant ». Mesuré sur le script de pile
 * de Graph Maker 2.2, où c'est exactement ce que fait wrongStack().
 *
 * Le guillemet n'ouvre un mot que s'il COMMENCE le mot : « abc"def ghi" »
 * garde ses deux mots, le premier étant « abc"def ». Un guillemet non refermé
 * emporte le reste de la chaîne — c'est la seule réponse qui ne coupe pas le
 * texte au milieu d'une citation. */
static int fin_du_mot(const char *s, int i, int len)
{
    if (i < len && s[i] == '"') {
        i++;
        while (i < len && s[i] != '"') i++;
        if (i < len) i++;          /* le guillemet fermant fait partie du mot */
        return i;
    }
    while (i < len && !est_blanc(s[i])) i++;
    return i;
}

/* ------------------------------------------------------------------ UTF-8 */

int hct_utf8_octets(const char *s, int i, int len)
{
    if (!s || i >= len) return 0;
    int k = i + 1;
    while (k < len && ((unsigned char)s[k] & 0xC0) == 0x80) k++;
    return k - i;   /* toujours >= 1 : l'appelant avance à coup sûr */
}

int hct_utf8_compte(const char *s)
{
    if (!s) return 0;
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;   /* on ne compte pas les continuations */
    return n;
}

int hct_utf8_compte_prefixe(const char *s, int octets)
{
    if (!s || octets <= 0) return 0;
    int n = 0;
    for (int i = 0; i < octets && s[i]; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) n++;
    return n;
}

/* LE SÉPARATEUR D'ITEMS EST UNE CHAÎNE, PAS UN OCTET.
 *
 * « set the itemDelimiter to "é" » ne retenait que le PREMIER OCTET de l'é,
 * soit 0xC3. Le découpage coupait donc sur cet octet-là : « item 2 of "aébéc" »
 * rendait « \xA9b » — la seconde moitié de l'é collée au b, une demi-séquence
 * UTF-8 que rien ne sait afficher. Et « the itemDelimiter » relu rendait 0xC3
 * seul, une chaîne invalide.
 *
 * Le compte, lui, donnait 3 PAR ACCIDENT : l'octet 0xC3 apparaît deux fois
 * dans « aébéc », donc le nombre d'items était juste et le contenu faux — le
 * pire des deux mondes pour qui cherche le défaut.
 *
 * HyperCard était en encodage mono-octet et la question ne se posait pas. Dès
 * lors que HC est en UTF-8, un délimiteur peut occuper plusieurs octets.
 *
 * Un délimiteur vide est traité comme la virgule : c'est ce que faisait déjà
 * l'ancien code (val[0] ? val[0] : ','), et découper sur rien n'a pas de sens. */
static const char *sep_ou_virgule(const char *delim)
{
    return (delim && *delim) ? delim : ",";
}

/* Le séparateur commence-t-il à s[i] ? */
static int sep_ici(const char *s, int i, int len, const char *sep, int lsep)
{
    return i + lsep <= len && memcmp(s + i, sep, (size_t)lsep) == 0;
}

/* ------------------------------------------------------------ comptage */

int hct_chunk_compte(const char *s, HctSorteChunk sorte, const char *delim)
{
    if (!s) return 0;
    int len = (int)strlen(s);

    switch (sorte) {
        case HCT_CH_CHAR:
            /* Des CARACTÈRES, pas des octets : « the number of chars of
             * "été" » vaut trois. Voir la note UTF-8 de hct_chunk.h. */
            (void)len;
            return hct_utf8_compte(s);

        case HCT_CH_WORD: {
            /* Les blancs multiples ne comptent pas : on avance jusqu'au
             * premier caractère non blanc, et l'on compte les groupes. */
            int n = 0, i = 0;
            while (i < len) {
                while (i < len && est_blanc(s[i])) i++;
                if (i >= len) break;
                n++;
                i = fin_du_mot(s, i, len);
            }
            return n;
        }

        case HCT_CH_ITEM: {
            /* Un séparateur final CRÉE un item vide : « a,b, » en compte
             * trois. C'est l'inverse des lignes, où « a\nb\n » en vaut deux —
             * une dissymétrie de HyperCard, pas une inattention. */
            if (len == 0) return 0;
            const char *sep = sep_ou_virgule(delim);
            int lsep = (int)strlen(sep);
            int n = 1;
            for (int i = 0; i < len; )
                if (sep_ici(s, i, len, sep, lsep)) { n++; i += lsep; }
                else i++;
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
                              const char *delim)
{
    HctBornes b = { 0, 0, 0 };
    if (!s || n < 1) return b;
    int len = (int)strlen(s);

    if (sorte == HCT_CH_CHAR) {
        /* Le rang est en CARACTÈRES, les bornes en OCTETS : on avance de
         * caractère en caractère jusqu'au n-ième. « char 1 of "été" » rendait
         * la moitié d'un é — une demi-séquence UTF-8, que rien ne sait
         * afficher. */
        int i = 0, k = 0;
        while (i < len) {
            int t = hct_utf8_octets(s, i, len);
            if (++k == n) { b.deb = i; b.fin = i + t; b.trouve = 1; return b; }
            i += t;
        }
        b.deb = b.fin = len;
        return b;
    }

    if (sorte == HCT_CH_WORD) {
        int i = 0, k = 0;
        while (i < len) {
            while (i < len && est_blanc(s[i])) i++;
            if (i >= len) break;
            k++;
            int deb = i;
            i = fin_du_mot(s, i, len);
            if (k == n) { b.deb = deb; b.fin = i; b.trouve = 1; return b; }
        }
        b.deb = b.fin = len;
        return b;
    }

    /* item et line : le séparateur crée un morceau, même vide.
     *
     * Les lignes se séparent toujours sur un saut de ligne, qui fait un octet.
     * Les items, eux, suivent itemDelimiter, qui peut en faire plusieurs — d'où
     * le pas de `lsep` et non de 1 : avancer d'un seul octet couperait au
     * milieu de la séquence et recommencerait sur sa seconde moitié. */
    const char *sep = (sorte == HCT_CH_ITEM) ? sep_ou_virgule(delim) : "\n";
    int lsep = (int)strlen(sep);
    int k = 1, deb = 0, i = 0;
    for (;;) {
        int fin_atteinte = (i >= len);
        if (fin_atteinte || sep_ici(s, i, len, sep, lsep)) {
            if (k == n) { b.deb = deb; b.fin = i; b.trouve = 1; return b; }
            if (fin_atteinte) break;
            k++;
            i  += lsep;
            deb = i;
        } else i++;
    }
    b.deb = b.fin = len;
    return b;
}

HctBornes hct_chunk_bornes(const char *s, HctSorteChunk sorte,
                           int n, int n2, const char *delim)
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
                        int n, int n2, const char *delim)
{
    if (!s) return hct_val_vide();
    HctBornes b = hct_chunk_bornes(s, sorte, n, n2, delim);
    if (!b.trouve) return hct_val_vide();
    return hct_val_texte_n(s + b.deb, b.fin - b.deb);
}

/* ---------------------------------------------------------- suppression
 *
 * « delete item 2 of "a,b,c" » rend "a,c" — le SÉPARATEUR part avec le
 * morceau. C'est ce qui distingue delete d'une écriture de vide, laquelle
 * rend "a,,c" : l'un retire un élément de la liste, l'autre le vide sans
 * l'ôter. Les confondre aurait été une corruption silencieuse de données,
 * du genre qu'on ne voit qu'en relisant un fichier des semaines plus tard.
 *
 * LA RÈGLE, relevée sur dix-huit cas rendus par l'ancien interprète : on
 * retire le morceau, plus UN séparateur — celui qui suit s'il en existe un,
 * sinon celui qui précède. D'où :
 *
 *     item 1 de "a,b,c"  ->  "b,c"     (séparateur d'après)
 *     item 3 de "a,b,c"  ->  "a,b"     (séparateur d'avant, il n'y en a plus après)
 *     item 1 de "a"      ->  ""        (aucun des deux)
 *     item 2 de "a,,c"   ->  "a,c"     (un morceau vide se retire comme un autre)
 *
 * CHAR fait exception : il n'a pas de séparateur, on retire les octets et
 * c'est tout.
 *
 * Un rang hors limites ne change rien — « delete item 9 of "a,b,c" » rend
 * "a,b,c". Étendre la chaîne pour y supprimer du vide n'aurait pas de sens,
 * et l'écriture, qui l'étend, le fait pour une raison qui ne vaut pas ici. */
HctValeur hct_chunk_supprime(const char *s, HctSorteChunk sorte,
                             int n, int n2, const char *delim)
{
    if (!s) return hct_val_vide();
    int len = (int)strlen(s);

    HctBornes b = hct_chunk_bornes(s, sorte, n, n2, delim);
    if (!b.trouve) return hct_val_texte_n(s, len);

    int deb = b.deb, fin = b.fin;
    if (sorte != HCT_CH_CHAR) {
        /* Le séparateur part AVEC le morceau — « delete item 2 of "a,b,c" »
         * rend « a,c » et non « a,,c ». Sa longueur n'est pas toujours un
         * octet : un itemDelimiter accentué en fait deux, et n'en retirer
         * qu'un laisserait la moitié d'une séquence UTF-8 dans le résultat.
         *
         * Les mots et les lignes gardent le pas de un : leurs séparateurs
         * — une espace, un saut de ligne — sont en ASCII par construction. */
        int lsep = 1;
        if (sorte == HCT_CH_ITEM) lsep = (int)strlen(sep_ou_virgule(delim));
        if (fin < len)         fin += lsep;   /* le séparateur qui suit  */
        else if (deb >= lsep)  deb -= lsep;   /* à défaut, celui d'avant */
    }
    if (deb < 0) deb = 0;
    if (fin > len) fin = len;
    if (fin < deb) fin = deb;

    /* Ce qui reste : avant, puis après. Même façon d'allouer que
     * hct_chunk_ecrit, juste en dessous. */
    int taille = deb + (len - fin);
    HctValeur r;
    r.txt = malloc((size_t)taille + 1);
    if (!r.txt) return hct_val_echec();
    memcpy(r.txt, s, (size_t)deb);
    memcpy(r.txt + deb, s + fin, (size_t)(len - fin));
    r.txt[taille] = '\0';
    r.len = taille;
    return r;
}

/* ------------------------------------------------------------ écriture */

HctValeur hct_chunk_ecrit(const char *s, HctSorteChunk sorte,
                          int n, int n2, const char *delim, const char *val)
{
    if (!s) s = "";
    if (!val) val = "";
    int len = (int)strlen(s), lv = (int)strlen(val);

    HctBornes b = hct_chunk_bornes(s, sorte, n, n2, delim);

    if (b.trouve) {
        int taille = b.deb + lv + (len - b.fin);
        HctValeur r;
        r.txt = malloc((size_t)taille + 1);
        if (!r.txt) return hct_val_echec();
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
    /* Le séparateur à INSÉRER. Une chaîne, comme celui sur lequel on découpe :
     * étendre « aébéc » avec un délimiteur « é » doit poser l'é entier, pas
     * son premier octet. Char n'en a pas, word pose une espace. */
    const char *sep = (sorte == HCT_CH_ITEM) ? sep_ou_virgule(delim)
                    : (sorte == HCT_CH_LINE) ? "\n"
                    : (sorte == HCT_CH_WORD) ? " " : "";
    int lsep = (int)strlen(sep);

    int manquants = 0;
    if (sorte == HCT_CH_ITEM || sorte == HCT_CH_LINE) {
        manquants = n - existants - 1;
        if (manquants < 0) manquants = 0;
    }

    int besoin_sep = (existants > 0 && lsep > 0) ? 1 : 0;
    int taille = len + (besoin_sep + manquants) * lsep + lv;

    HctValeur r;
    r.txt = malloc((size_t)taille + 1);
    if (!r.txt) return hct_val_echec();

    int p = 0;
    memcpy(r.txt + p, s, (size_t)len); p += len;
    if (besoin_sep) { memcpy(r.txt + p, sep, (size_t)lsep); p += lsep; }
    for (int i = 0; i < manquants; i++) { memcpy(r.txt + p, sep, (size_t)lsep); p += lsep; }
    memcpy(r.txt + p, val, (size_t)lv); p += lv;
    r.txt[p] = '\0';
    r.len = p;
    return r;
}
