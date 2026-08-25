/* hct_lex.c — analyse lexicale de HyperTalk. Voir hct_lex.h pour les principes. */

#include "hct_lex.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------------------------------------------------------------- synonymes */

/* Annexe F du guide Apple. « in » pour « of » est volontairement absent :
 * voir hct_lex.h. « part » non plus — il vaut « button ou field » selon le
 * contexte, ce que seul le parseur peut trancher. */
static const struct { const char *court, *plein; } SYNONYMES[] = {
    { "abbr",      "abbreviated" }, { "abbrev",  "abbreviated" },
    { "bg",        "background"  }, { "bkgnd",   "background"  },
    { "bgs",       "backgrounds" }, { "bkgnds",  "backgrounds" },
    { "botright",  "bottomright" },
    { "btn",       "button"      }, { "btns",    "buttons"     },
    { "cd",        "card"        }, { "cds",     "cards"       },
    { "char",      "character"   }, { "chars",   "characters"  },
    { "fld",       "field"       }, { "flds",    "fields"      },
    { "grey",      "gray"        },
    { "hilite",    "highlight"   }, { "highlite","highlight"   },
    { "hilight",   "highlight"   },
    { "loc",       "location"    },
    { "mid",       "middle"      },
    { "msg",       "message"     },
    { "poly",      "polygon"     },
    { "prev",      "previous"    },
    { "rect",      "rectangle"   },
    { "reg",       "regular"     },
    { "sec",       "seconds"     }, { "secs",    "seconds"     },
    { "tick",      "ticks"       },
    { NULL, NULL }
};

static int egal_ci(const char *a, int alen, const char *b)
{
    int i;
    for (i = 0; i < alen; i++) {
        if (!b[i]) return 0;
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
    }
    return b[alen] == '\0';
}

const char *hct_synonyme(const char *mot, int len)
{
    int i;
    for (i = 0; SYNONYMES[i].court; i++)
        if (egal_ci(mot, len, SYNONYMES[i].court)) return SYNONYMES[i].plein;
    return NULL;
}

/* ------------------------------------------------------------------ genres */

const char *hct_genre_nom(HctGenre g)
{
    switch (g) {
        case HCT_FIN:    return "fin";
        case HCT_EOL:    return "eol";
        case HCT_IDENT:  return "ident";
        case HCT_NOMBRE: return "nombre";
        case HCT_CHAINE: return "chaine";
        case HCT_OP:     return "op";
        case HCT_ERREUR: return "ERREUR";
    }
    return "?";
}

char *hct_texte(const HctJeton *j, char *out, size_t taille)
{
    size_t n = (size_t)j->len;
    if (n >= taille) n = taille ? taille - 1 : 0;
    if (n) memcpy(out, j->deb, n);
    if (taille) out[n] = '\0';
    return out;
}

/* ------------------------------------------------------------- accumulation */

static int pousse(HctLot *lot, HctGenre genre, const char *deb, int len,
                  int ligne, int col, const char *norme, const char *msg)
{
    if (lot->n == lot->cap) {
        int cap = lot->cap ? lot->cap * 2 : 256;
        HctJeton *p = realloc(lot->jetons, (size_t)cap * sizeof *p);
        if (!p) return 0;
        lot->jetons = p;
        lot->cap = cap;
    }
    HctJeton *j = &lot->jetons[lot->n++];
    j->genre = genre; j->deb = deb; j->len = len;
    j->ligne = ligne; j->col = col;
    j->norme = norme; j->msg = msg;
    if (genre == HCT_ERREUR) lot->nerr++;
    return 1;
}

/* Opérateurs à deux caractères, à tester avant ceux d'un seul.
 * Les formes « is not », « there is a » etc. sont des SUITES de mots : elles
 * relèvent du parseur, pas du lexer, qui n'a aucun moyen de savoir si « is »
 * commence un opérateur composé ou termine une phrase. */
static const char *OPS2[] = { "<=", ">=", "<>", "&&", NULL };

/* Un caractère : les opérateurs, la ponctuation, les parenthèses.
 * Le point-virgule sépare deux instructions sur une même ligne. */
static const char *OPS1 = "+-*/^&<>=(),;";

/* ------------------------------------------------------------------- lexage */

int hct_lex(const char *src, HctLot *lot)
{
    memset(lot, 0, sizeof *lot);
    if (!src) { pousse(lot, HCT_FIN, "", 0, 1, 1, NULL, NULL); return 1; }

    const char *p = src;
    int ligne = 1;
    const char *deb_ligne = src;   /* pour calculer la colonne */

    #define COL(q)  ((int)((q) - deb_ligne) + 1)

    while (*p) {

        /* --- espaces horizontaux ------------------------------------- */
        if (*p == ' ' || *p == '\t') { p++; continue; }

        /* --- continuation de ligne -----------------------------------
         * Le caractère « soft return » du Mac classique, ¬ (0xAC en MacRoman,
         * "\xC2\xAC" en UTF-8). La ligne logique se poursuit : on avale le
         * saut de ligne qui suit et on n'émet aucun EOL. */
        if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0xAC) {
            p += 2;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\r') p++;
            if (*p == '\n') { p++; ligne++; deb_ligne = p; }
            continue;
        }
        if ((unsigned char)p[0] == 0xAC) {          /* MacRoman non converti */
            p++;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\r') p++;
            if (*p == '\n') { p++; ligne++; deb_ligne = p; }
            continue;
        }

        /* --- commentaire : « -- » jusqu'au bout de la ligne ----------- */
        if (p[0] == '-' && p[1] == '-') {
            while (*p && *p != '\n' && *p != '\r') p++;
            continue;
        }

        /* --- fin de ligne --------------------------------------------
         * HyperTalk est sensible aux lignes : une instruction par ligne.
         * On émet un EOL, mais jamais deux de suite — les lignes vides et
         * les lignes de commentaire ne doivent pas encombrer le parseur. */
        if (*p == '\n' || *p == '\r') {
            const char *q = p;
            int lg = ligne, cl = COL(q);   /* AVANT de déplacer deb_ligne */
            if (p[0] == '\r' && p[1] == '\n') p += 2; else p++;
            ligne++; deb_ligne = p;
            if (lot->n && lot->jetons[lot->n-1].genre != HCT_EOL)
                if (!pousse(lot, HCT_EOL, q, 1, lg, cl, NULL, NULL))
                    return 0;
            continue;
        }

        /* --- chaîne littérale ----------------------------------------
         * HyperTalk n'a pas d'échappement : une chaîne ne peut pas contenir
         * de guillemet, et ne peut pas franchir une fin de ligne. */
        if (*p == '"') {
            const char *q = p + 1;
            int col = COL(p);
            while (*q && *q != '"' && *q != '\n' && *q != '\r') q++;
            if (*q != '"') {
                if (!pousse(lot, HCT_ERREUR, p, (int)(q - p), ligne, col, NULL,
                            "guillemet fermant manquant")) return 0;
                p = q;
                continue;
            }
            if (!pousse(lot, HCT_CHAINE, p + 1, (int)(q - p - 1), ligne, col,
                        NULL, NULL)) return 0;
            p = q + 1;
            continue;
        }

        /* --- nombre --------------------------------------------------
         * Un point n'est un séparateur décimal que s'il est suivi d'un
         * chiffre : « 3.5 » est un nombre, « card 3.name » ne l'est pas. */
        /* Un nombre commence par un chiffre, ou par un POINT suivi d'un
         * chiffre : « .02 » est du HyperTalk parfaitement valide, et les
         * scripts d'époque l'écrivent volontiers — Graph Maker teste
         * « if change > .02 » avant de remplir chaque part de son camembert.
         *
         * Sans ce cas, le point partait en « caractère inattendu », la
         * comparaison échouait, et le remplissage était sauté en silence. */
        if (isdigit((unsigned char)*p) ||
            (*p == '.' && isdigit((unsigned char)p[1]))) {
            const char *q = p;
            int col = COL(p), points = 0;
            if (*q == '.') { q++; points++;
                             while (isdigit((unsigned char)*q)) q++; }
            while (isdigit((unsigned char)*q) ||
                   (*q == '.' && isdigit((unsigned char)q[1]))) {
                if (*q == '.') points++;
                q++;
            }
            if (points > 1) {
                if (!pousse(lot, HCT_ERREUR, p, (int)(q - p), ligne, col, NULL,
                            "nombre mal formé")) return 0;
            } else if (!pousse(lot, HCT_NOMBRE, p, (int)(q - p), ligne, col,
                               NULL, NULL)) return 0;
            p = q;
            continue;
        }

        /* --- mot -----------------------------------------------------
         * Lettres, chiffres et souligné. HyperTalk autorise le chiffre à
         * l'intérieur d'un nom, mais pas en tête — ce cas est déjà pris par
         * la branche des nombres. */
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *q = p;
            int col = COL(p);
            while (isalnum((unsigned char)*q) || *q == '_') q++;
            int len = (int)(q - p);
            if (!pousse(lot, HCT_IDENT, p, len, ligne, col,
                        hct_synonyme(p, len), NULL)) return 0;
            p = q;
            continue;
        }

        /* --- opérateurs à deux caractères ----------------------------- */
        {
            int i, pris = 0;
            for (i = 0; OPS2[i]; i++)
                if (p[0] == OPS2[i][0] && p[1] == OPS2[i][1]) {
                    if (!pousse(lot, HCT_OP, p, 2, ligne, COL(p), NULL, NULL))
                        return 0;
                    p += 2; pris = 1; break;
                }
            if (pris) continue;
        }

        /* --- opérateurs à un caractère -------------------------------- */
        if (strchr(OPS1, *p)) {
            if (!pousse(lot, HCT_OP, p, 1, ligne, COL(p), NULL, NULL)) return 0;
            p++;
            continue;
        }

        /* --- les trois symboles hérités du Mac ------------------------
         * ≤ ≥ ≠ , en UTF-8 sur trois octets. Le guide les documente au même
         * rang que <= >= <> (annexe E). */
        if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x89) {
            unsigned char c = (unsigned char)p[2];
            if (c == 0xA4 || c == 0xA5 || c == 0xA0) {
                if (!pousse(lot, HCT_OP, p, 3, ligne, COL(p), NULL, NULL))
                    return 0;
                p += 3;
                continue;
            }
        }

        /* --- caractère inattendu --------------------------------------
         * On avale la séquence UTF-8 entière : sans cela un « ∞ », qui pèse
         * trois octets, produirait trois erreurs, et une ligne de bannière en
         * donnerait des centaines. Une erreur par CARACTÈRE, pas par octet. */
        {
            unsigned char c0 = (unsigned char)p[0];
            int n = 1;
            if      ((c0 & 0xE0) == 0xC0) n = 2;
            else if ((c0 & 0xF0) == 0xE0) n = 3;
            else if ((c0 & 0xF8) == 0xF0) n = 4;
            for (int k = 1; k < n; k++)
                if (((unsigned char)p[k] & 0xC0) != 0x80) { n = k; break; }
            if (!pousse(lot, HCT_ERREUR, p, n, ligne, COL(p), NULL,
                        "caractère inattendu")) return 0;
            p += n;
        }
    }

    if (!pousse(lot, HCT_FIN, p, 0, ligne, COL(p), NULL, NULL)) return 0;
    return lot->nerr == 0;

    #undef COL
}

void hct_lot_libere(HctLot *lot)
{
    free(lot->jetons);
    memset(lot, 0, sizeof *lot);
}
