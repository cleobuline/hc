/* hct_val.c — valeurs HyperTalk. Voir hct_val.h pour les règles. */

#include "hct_val.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>

/* ------------------------------------------------------- construction */

/* LE TEXTE D'UNE VALEUR N'EST PLUS JAMAIS NUL.
 *
 * Les constructeurs rendaient txt = NULL quand l'allocation échouait, et
 * l'en-tête l'avouait : « indiscernable d'une chaîne vide valide. Tout
 * consommateur doit donc traiter txt == NULL ». Personne ne pouvait tenir une
 * telle promesse — il y a des centaines de lectures de .txt dans le noyau,
 * et il suffit d'en oublier une.
 *
 * MESURÉ, en faisant échouer UN SEUL malloc et en balayant tous les points
 * d'allocation d'un script ordinaire : sept plantages sur quatre cents.
 *
 * Deux choses le remplacent :
 *
 *   UNE SENTINELLE. Une valeur en échec porte une chaîne vide STATIQUE, donc
 *   lisible et terminée par zéro. Aucun consommateur ne peut plus la
 *   déréférencer de travers, même celui qu'on aurait oublié. hct_val_libere
 *   la reconnaît et ne la libère pas.
 *
 *   UN DRAPEAU COLLANT. La sentinelle seule transformerait une pénurie en
 *   chaîne vide silencieuse — le défaut qu'on passe la vie à chasser
 *   ailleurs. Le drapeau dit qu'il s'est passé quelque chose, et l'exécuteur
 *   lève une faute au lieu de continuer sur une valeur inventée.
 *
 * Et hct_val_vide n'alloue plus DU TOUT : elle rend la sentinelle. Une valeur
 * vide était le seul cas où l'on demandait un octet au système pour n'y rien
 * mettre, et c'était aussi un point de panne pour rien. */
static char g_vide[1] = "";
static int  g_manque = 0;

HctValeur hct_val_echec(void)
{
    HctValeur v;
    v.txt = g_vide;
    v.len = 0;
    g_manque = 1;
    return v;
}

int  hct_val_manque(void)        { return g_manque; }
void hct_val_manque_efface(void) { g_manque = 0; }

HctValeur hct_val_vide(void)
{
    HctValeur v;
    v.txt = g_vide;
    v.len = 0;
    return v;
}

HctValeur hct_val_texte_n(const char *s, int len)
{
    HctValeur v;
    if (!s || len < 0) len = 0;
    if (len == 0) return hct_val_vide();
    v.txt = malloc((size_t)len + 1);
    if (!v.txt) return hct_val_echec();
    memcpy(v.txt, s, (size_t)len);
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

/* PRENDRE le texte d'une valeur, et la vider.
 *
 * L'appelant devient propriétaire, et devra free(). C'est la SEULE façon
 * correcte de sortir un texte d'une HctValeur, et la sentinelle est la raison :
 * une valeur vide ou en échec porte une chaîne STATIQUE, que free() ferait
 * exploser. Mesuré, dès la première tentative : « free(): invalid pointer »
 * dans le tri, où le texte à trier était repris à la main par « src_dyn =
 * v.txt ».
 *
 * On rend alors une copie neuve — un octet — plutôt que la sentinelle. NULL
 * seulement si même cet octet manque, et le drapeau est levé pour le dire. */
char *hct_val_prend(HctValeur *v)
{
    if (!v || !v->txt) return NULL;
    char *p;
    if (v->txt == g_vide) {
        p = malloc(1);
        if (p) p[0] = '\0';
        else   g_manque = 1;
    } else {
        p = v->txt;
    }
    v->txt = NULL;
    v->len = 0;
    return p;
}

void hct_val_libere(HctValeur *v)
{
    if (!v) return;
    /* La sentinelle est statique : la libérer serait une faute, et c'est le
     * prix — le seul — d'une chaîne vide qu'on n'alloue pas. */
    if (v->txt != g_vide) free(v->txt);
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

    /* INF ET NAN SE LISENT COMME DES NOMBRES, et il le FAUT.
     *
     * J'ai d'abord voulu n'accepter que INF, en me disant que NAN est un
     * diagnostic qu'on lit et pas un opérande qu'on calcule. Mesuré : ça ne
     * marche pas, et la pile elle-même dit pourquoi.
     *
     *     y = (x+2)*(x-3/2)^2*(x+1/2)/(x+1/2)/5
     *
     * Le 0/0 est SUIVI d'une division par cinq. Chaque opérateur repasse par
     * une chaîne, si bien que le NaN doit se relire pour traverser le reste de
     * l'expression. Et la preuve qu'il le traverse est à l'écran : HyperCard
     * affiche « -.500,NAN(004) » pour cette équation-là, code compris, APRÈS
     * la division par cinq. Refuser NAN en lecture donnait « un nombre est
     * attendu ici » — un dialogue de moins, un autre à la place.
     *
     * Ce qu'il ne faut pas perdre en échange, c'est la comparaison : NaN n'est
     * ni inférieur ni supérieur à quoi que ce soit, et hct_compare rendrait
     * donc « égal ». D'où hct_ordonnable, plus bas, que les opérateurs d'ordre
     * consultent, et le repli sur le TEXTE pour l'égalité — sans quoi
     * « if y = 5 » aurait répondu vrai sur un NaN, et « if y is "NAN(004)" »,
     * que la pile écrit, aurait répondu faux. */
    if ((s[0] == 'i' || s[0] == 'I') && (s[1] == 'n' || s[1] == 'N') &&
        (s[2] == 'f' || s[2] == 'F') && *saute_blancs(s + 3) == '\0')
        return 1;
    if ((s[0] == 'n' || s[0] == 'N') && (s[1] == 'a' || s[1] == 'A') &&
        (s[2] == 'n' || s[2] == 'N')) {
        const char *q = s + 3;
        if (*q == '(') {                 /* NAN(004) : le code de SANE */
            q++;
            if (!isdigit((unsigned char)*q)) return 0;
            while (isdigit((unsigned char)*q)) q++;
            if (*q != ')') return 0;
            q++;
        }
        return *saute_blancs(q) == '\0';
    }

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

int hct_vers_rang(const char *s, int *hors)
{
    if (hors) *hors = 0;
    double d = hct_vers_nombre(s);
    /* Le test passe par le double AVANT toute conversion : comparer après
     * coup ne sert à rien, le mal est déjà fait. NaN échoue les deux
     * comparaisons, donc il tombe ici aussi. */
    if (!(d >= -(double)HCT_RANG_MAX && d <= (double)HCT_RANG_MAX)) {
        if (hors) *hors = 1;
        return 0;
    }
    return (int)d;
}

int hct_vers_bool(const char *s, int *valide)
{
    if (valide) *valide = 1;
    if (!s) { if (valide) *valide = 0; return 0; }
    s = saute_blancs(s);

    int n = 0;
    while (s[n] && s[n] != ' ' && s[n] != '\t') n++;

    /* Et RIEN d'autre derrière que des blancs.
     *
     * On ne regardait que le premier mot : « true patate » était donc vrai,
     * et « "true patate" is a boolean » répondait true — alors que le contrat
     * dit que seuls true et false sont acceptés. Un test sur une variable mal
     * remplie passait ainsi pour une réponse. */
    const char *fin = saute_blancs(s + n);
    if (*fin) { if (valide) *valide = 0; return 0; }

    if (n == 4 && !strncasecmp(s, "true", 4))  return 1;
    if (n == 5 && !strncasecmp(s, "false", 5)) return 0;

    if (valide) *valide = 0;
    return 0;
}

/* INF ET NAN : L'ÉCRITURE, EN UN SEUL ENDROIT.
 *
 * SANE — l'arithmétique du Macintosh, dont HyperCard se sert — ne fait pas
 * d'erreur sur une division par zéro : elle rend une VALEUR, et cette valeur
 * porte la raison. « 1/0 » vaut INF, « 0/0 » vaut NAN(004), et une pile
 * d'époque s'en sert : HypoGraph 0.91 écrit « else if y is "NAN(004)" then
 * put "y=0/0 (indeterminate)" », et teste ailleurs « if y≠"NAN(037)" ».
 *
 * Mesuré sur la pile originelle, sous HyperCard : au point où la courbe
 * traverse sa singularité, le traceur affiche « -.500,NAN(004) ». HC y
 * répondait « division par zéro » et ouvrait un dialogue.
 *
 * Le code voyage dans la charge utile du NaN, et il SURVIT aux calculs
 * suivants — mesuré : nan("4")/5 et nan("4")*2+1 la gardent. C'est ce qui
 * permet à « (…)/(x+1/2)/5 » de rendre encore NAN(004) après sa division par
 * cinq. Charge nulle — le NaN que le matériel fabrique tout seul — s'écrit
 * « NAN » sans parenthèses, faute d'avoir une raison à donner.
 *
 * DEUX PORTES, celle du format par défaut et celle du numberFormat, et la
 * pile emprunte la seconde : elle pose « set the numberFormat to "0.000" ».
 * D'où cette fonction plutôt que deux copies.
 *
 * Rend le nombre d'octets écrits, ou -1 si x est fini — auquel cas l'appelant
 * poursuit son chemin normal. */
static int ecrit_non_fini(double x, char *out, int taille)
{
    if (isnan(x)) {
        unsigned long long u = 0;
        memcpy(&u, &x, sizeof x < sizeof u ? sizeof x : sizeof u);
        unsigned long code = (unsigned long)(u & 0x7FFFFFFFFFFFFULL);
        if (code) return snprintf(out, (size_t)taille, "NAN(%03lu)", code);
        return snprintf(out, (size_t)taille, "NAN");
    }
    if (isinf(x))
        return snprintf(out, (size_t)taille, x > 0 ? "INF" : "-INF");
    return -1;
}

/* Un NaN qui porte sa RAISON, à la façon de SANE.
 *
 * L'exposant tout à un et le bit « silencieux » font le NaN ; les bits bas
 * portent le code. On le pose par les bits plutôt que par nan("4") pour
 * n'avoir pas à passer par une chaîne, et parce que le masque dit exactement
 * ce qui est écrit. */
double hct_nan_code(unsigned long code)
{
    unsigned long long u = 0x7FF8000000000000ULL | (code & 0x7FFFFFFFFFFFFULL);
    double d = 0;
    memcpy(&d, &u, sizeof d < sizeof u ? sizeof d : sizeof u);
    return d;
}

/* ------------------------------------------------- écriture d'un nombre
 *
 * Le format par défaut de HyperCard est « %.6g » à ceci près qu'un entier
 * s'écrit sans décimale, quelle que soit sa taille. On traite donc à part le
 * cas entier, sinon 1000000 s'afficherait « 1e+06 ». */
int hct_ecrit_nombre(double x, char *out, int taille)
{
    if (taille < 2) { if (taille) out[0] = 0; return 0; }

    /* L'INFINI SE DEMANDE À LA BIBLIOTHÈQUE, PAS À UNE BORNE ÉCRITE À LA MAIN.
     *
     * Le test portait sur 1e308. Or le plus grand double vaut à peu près
     * 1,7976931348623157e308 : tout l'intervalle entre les deux est FINI et
     * parfaitement représentable, et s'écrivait pourtant « INF ». « put 1.5e308 »
     * répondait INF, et « put 1e308 * 1.5 » aussi — alors que la seconde a bien
     * un résultat. isinf et isnan disent exactement ce qu'on voulait savoir. */
    /* Ni fini ni formatable : INF et NAN s'écrivent de la même façon des deux
     * côtés. Une seule définition, ci-dessus — les avoir recopiées aurait
     * garanti qu'un jour l'une des deux change seule. */
    {
        int n = ecrit_non_fini(x, out, taille);
        if (n >= 0) return n;
    }

    /* Entier exact et représentable : on l'écrit tel quel. */
    if (x == floor(x) && fabs(x) < 1e15) {
        long long e = (long long)x;
        return snprintf(out, (size_t)taille, "%lld", e);
    }

    /* LA FORME DÉCIMALE NE TIENT PAS TOUJOURS.
     *
     * « put 1e100 + 0 » demande cent un chiffres ; le tampon en fait
     * soixante-quatre. L'ancien code rendait alors « taille - 1 » et laissait
     * les soixante-deux premiers chiffres, c'est-à-dire un nombre
     * COMPLÈTEMENT FAUX — 1e62 au lieu de 1e100 — sans un mot.
     *
     * snprintf dit ce qu'il AURAIT écrit : quand ça ne tient pas, on repasse
     * en notation scientifique, qui tient toujours. Le commentaire en tête
     * annonçait d'ailleurs « %.6g » depuis le début.
     *
     * La bascule se décide sur la PLACE et non sur une magnitude choisie à
     * la main : « put 1e16 + 0 » continue donc de rendre ses dix-sept
     * chiffres en clair, comme avant, parce qu'ils tiennent. */
    int n = snprintf(out, (size_t)taille, "%.6f", x);
    if (n < 0) { out[0] = 0; return 0; }
    if (n >= taille) {
        n = snprintf(out, (size_t)taille, "%g", x);
        return (n < 0) ? 0 : (n >= taille ? taille - 1 : n);
    }

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

    /* Même correction qu'au format par défaut : la borne 1e308 déclarait
     * infinis des nombres finis jusqu'à 1,797e308. */
    /* Ni fini ni formatable : INF et NAN s'écrivent de la même façon des deux
     * côtés. Une seule définition, ci-dessus — les avoir recopiées aurait
     * garanti qu'un jour l'une des deux change seule. */
    {
        int n = ecrit_non_fini(x, out, taille);
        if (n >= 0) return n;
    }

    /* Même garde qu'au format par défaut : un gabarit ne peut pas mettre en
     * forme ce qui ne tient pas dans le tampon, et rendre les premiers
     * chiffres d'un nombre en donne un autre. Mieux vaut la notation
     * scientifique, hors gabarit mais juste. */
    char brut[64];
    int n = snprintf(brut, sizeof brut, "%.*f", dmax, x);
    if (n < 0) { if (taille) out[0] = 0; return 0; }
    if (n >= (int)sizeof brut) {
        n = snprintf(out, (size_t)taille, "%g", x);
        return (n < 0) ? 0 : (n >= taille ? taille - 1 : n);
    }

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
        double x = hct_vers_nombre(a), y = hct_vers_nombre(b);

        /* UN NaN NE SE COMPARE PAS EN NOMBRE : il n'est ni inférieur ni
         * supérieur, et les trois tests ci-dessous rendraient donc 0 —
         * c'est-à-dire ÉGAL. « if y = 5 » aurait répondu vrai sur un NaN, et
         * c'est exactement le genre de réponse fausse et silencieuse qu'on
         * cherche à ne pas produire.
         *
         * On retombe donc sur le TEXTE, ce qui donne aussi ce que la pile
         * attend : « if y is "NAN(004)" » compare deux fois la même chaîne et
         * répond vrai, « if y is "NAN(037)" » répond faux. L'ORDRE, lui, ne
         * doit rien rendre du tout — voir hct_ordonnable, que les opérateurs
         * < > <= >= consultent avant d'appeler ici. */
        if (isnan(x) || isnan(y)) {
            if (numerique) *numerique = 0;
            int t = strcasecmp(a, b);
            return t < 0 ? -1 : (t > 0 ? 1 : 0);
        }

        if (numerique) *numerique = 1;
        if (x < y) return -1;
        if (x > y) return  1;
        return 0;
    }

    if (numerique) *numerique = 0;

    /* Texte : la comparaison de HyperTalk ignore la casse. */
    int r = strcasecmp(a, b);
    return r < 0 ? -1 : (r > 0 ? 1 : 0);
}

/* Ces deux opérandes se COMPARENT-ILS EN ORDRE ?
 *
 * Non dès qu'un NaN est en jeu : « NAN(004) > 625 », « NAN < 3 » et leurs
 * inverses sont tous FAUX, comme le veut l'arithmétique à virgule flottante.
 * Rendre l'un d'eux vrai serait pire qu'une erreur — une pile qui borne ses
 * points par « if ny > itt » aurait tracé vers un point qui n'existe pas.
 *
 * L'égalité n'en dépend pas : elle passe par hct_compare, qui se replie sur le
 * texte et rend donc « NAN(004) is "NAN(004)" » vrai, ce que la pile écrit. */
int hct_ordonnable(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    if (!hct_est_nombre(a) || !hct_est_nombre(b)) return 1;   /* texte : ordonnable */
    return !(isnan(hct_vers_nombre(a)) || isnan(hct_vers_nombre(b)));
}

int hct_egal(const char *a, const char *b)
{
    return hct_compare(a, b, NULL) == 0;
}
