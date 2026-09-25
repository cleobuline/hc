/* hc_core.c — Noyau du clone HyperCard.
 * Modèle d'objets, chaîne de messages hiérarchique, mini-interpréteur.
 * C99 portable, sans dépendance.
 */
#include "hc_core.h"
#include "hc_interne.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <strings.h>
#include <limits.h>
#include "hct_bloc.h"
#include "hct_chunk.h"   /* morceaux : étape 1 de la reprise v3 */
#include "hc_file.h"     /* hc_taille_fichier : « the size » d'une pile */
#include "hct_val.h"     /* valeurs  : étape 2 */
#include "hct_exec.h"    /* exécuteur : étape 3 */
#include "hct_eval.h"
#define HC_MAX_LOOP 1000000
/* ==================== outils chaînes ==================== */
#define HC_V3_DEFAUT 1
char *dupstr(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int ci_equal(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* compare le début de `s` avec `pfx`, sans casse */
static int ci_prefix(const char *s, const char *pfx)
{
    while (*pfx) {
        if (!*s) return 0;
        if (tolower((unsigned char)*s) != tolower((unsigned char)*pfx)) return 0;
        s++; pfx++;
    }
    return 1;
}

/* compare le début de `s` avec le mot `w`, sans casse, en exigeant une
 * vraie frontière de mot : « button "x" » et « button » passent,
 * « buttonnette "x" » non. */
static int ci_word(const char *s, const char *w)
{
    if (!ci_prefix(s, w)) return 0;
    char c = s[strlen(w)];
    return !(isalnum((unsigned char)c) || c == '_');
}

static const char *skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* copie le mot suivant dans out ; renvoie la position après le mot */
static const char *next_word(const char *s, char *out, int outlen)
{
    s = skip_spaces(s);
    int i = 0;
    while (*s && !isspace((unsigned char)*s) && i < outlen - 1)
        out[i++] = *s++;
    out[i] = '\0';
    return s;
}

/* extrait un littéral entre guillemets ; renvoie la position après le guillemet fermant */
static const char *quoted(const char *s, char *out, int outlen)
{
    s = skip_spaces(s);
    out[0] = '\0';
    if (*s != '"') return s;
    s++;
    int i = 0;
    while (*s && *s != '"' && i < outlen - 1)
        out[i++] = *s++;
    out[i] = '\0';
    if (*s == '"') s++;
    return s;
}

/* ═══ CITER ET RELIRE UN NOM DANS UN DESCRIPTEUR ══════════════════════
 *
 * « the long name » promet une chose : ce qu'il rend doit pouvoir désigner
 * l'objet d'où il vient. Le fabricant écrivait « %s "%s" » et le lecteur
 * s'arrêtait au premier guillemet — un nom qui en contient un cassait donc le
 * descripteur en deux.
 *
 *     set the name of card button 1 to "a" & quote & "b"
 *     put the name of card button 1        ->  card button "a"b"
 *     put the short name of it             ->  short name of it   (le LITTÉRAL)
 *
 * Pas d'erreur : la chaîne inventée circulait. Mesuré exactement ainsi.
 *
 * Le fichier .stack savait déjà écrire ces noms — put_quoted échappe depuis la
 * correction du retour à la ligne. C'est le LANGAGE qui n'avait pas reçu la
 * même règle : une syntaxe d'échappement dans le fichier, aucune dans les
 * descripteurs. On prend donc la même convention des deux côtés, parce que
 * deux syntaxes pour une seule idée finissent toujours par diverger.
 *
 * Les échappements INCONNUS sont rendus tels quels, comme dans le fichier :
 * un nom contenant « \b » se relit « \b », et les descripteurs déjà écrits
 * dans les scripts d'une pile continuent de se lire comme avant.
 *
 * Rend la longueur qu'il aurait fallu, à la manière de snprintf : c'est ce qui
 * permet à l'appelant de voir la troncature au lieu de la subir. */
static int descripteur_cite(const char *nom, char *out, int outlen)
{
    int besoin = 2;                       /* les deux guillemets */
    int i = 0;
    if (outlen > 0) out[0] = '\0';

#define DC_POSE(c) do { if (i < outlen - 1) out[i++] = (c); besoin++; } while (0)
    if (outlen > 1) { out[i++] = '"'; }
    for (const char *p = nom ? nom : ""; *p; p++) {
        switch (*p) {
        case '"':  DC_POSE('\\'); DC_POSE('"');  break;
        case '\\': DC_POSE('\\'); DC_POSE('\\'); break;
        case '\n': DC_POSE('\\'); DC_POSE('n');  break;
        case '\r': DC_POSE('\\'); DC_POSE('r');  break;
        default:   DC_POSE(*p);
        }
    }
#undef DC_POSE
    besoin -= 2;                          /* DC_POSE a compté ses caractères */
    if (i < outlen - 1) out[i++] = '"';
    if (outlen > 0) out[i] = '\0';
    return besoin + 2;
}

/* La lecture, symétrique. Même rôle que quoted(), mais pour un DESCRIPTEUR.
 *
 * quoted() reste inchangée : elle sert aussi à lire les littéraux de script, et
 * HyperTalk n'y connaît aucun échappement — « put "a\b" » vaut a\b, et doit
 * continuer. Les deux métiers se ressemblaient assez pour partager une
 * fonction, pas assez pour partager une règle. */
static const char *descripteur_lit(const char *s, char *out, int outlen)
{
    s = skip_spaces(s);
    if (outlen > 0) out[0] = '\0';
    if (*s != '"') return s;
    s++;
    int i = 0;
    while (*s && *s != '"') {
        char c = *s;
        if (c == '\\' && s[1]) {
            switch (s[1]) {
            case '"':  c = '"';  s++; break;
            case '\\': c = '\\'; s++; break;
            case 'n':  c = '\n'; s++; break;
            case 'r':  c = '\r'; s++; break;
            default:   break;      /* échappement inconnu : la barre reste */
            }
        }
        if (i < outlen - 1) out[i++] = c;
        s++;
    }
    if (i < outlen) out[i] = '\0';
    if (*s == '"') s++;
    return s;
}

/* ==================== état global ==================== */

/* Garde-fou : profondeur maximale d'imbrication des messages.
 * Sans lui, un gestionnaire qui se renvoie son propre message
 * (`on boum / send "boum" to me / end boum`) épuise la pile C
 * et le programme meurt. Le vrai HyperCard répondait
 * « Too much recursion » ; on fait pareil, en douceur. */
/* Taille d'une valeur manipulée par le PONT vers l'ancien évaluateur :
 * variables, arguments, propriétés, tout ce qui voyage dans un « char *out ».
 * Le chemin v3 n'en dépend plus — HctValeur alloue son texte à sa taille — mais
 * arena_buf sert encore ces tampons, tous dimensionnés au plafond.
 *
 * Ce plafond a d'abord été 16 Ko, puis monté jusqu'à 1 Mio pour que « read from
 * file » cesse de tronquer. Il est redescendu à 64 Ko, parce qu'à 1 Mio
 * L'ARÈNE SATURAIT SUR UN SCRIPT LÉGAL et rendait de mauvais résultats.
 *
 * L'arène plafonne à 1 Go, soit 1 024 tampons à 1 Mio. Une récursion par
 * message en consomme une trentaine par niveau : la saturation arrivait donc
 * vers le 32e niveau, pour un plafond de profondeur de 64. Passé ce point,
 * arena_buf rend g_apanic — un tampon STATIQUE PARTAGÉ — et les valeurs
 * s'écrasent mutuellement. Mesuré, récursion à 63 niveaux :
 *
 *                 résultat            pic mémoire
 *   1 Mio      2272 au lieu de 4473     saturé
 *   256 Ko     juste                     38 Mo
 *   64 Ko      juste                     14 Mo
 *
 * 64 Ko laisse la marge la plus large : aucune saturation même à 400
 * évaluations par niveau, vingt fois le cas mesuré. Le prix est que
 * « get the script of » et « read from file » tronquent à 64 Ko au lieu d'un
 * mégaoctet — troncature ANNONCÉE, et toute réécriture du script est refusée
 * (voir g_script_clipped), donc elle ne perd rien en silence. Un script
 * d'époque est très loin de cette taille : celui du Calendrier fait 9,1 Ko.
 *
 * 256 Ko reste disponible d'une ligne si un jour un script dépasse 64 Ko :
 * même justesse, 24 Mo de pic en plus, quatre fois moins de marge.
 *
 * Le vrai correctif reste à venir : allouer chaque valeur à sa taille réelle
 * plutôt qu'au plafond. Cela demande de remplacer les signatures
 * (char *out, int outlen) par un type chaîne dynamique — un chantier à part
 * entière, qui rendrait ce plafond sans objet. */
#define HC_VAL 65536

/* Le garde-fou de recursion peut revenir a sa valeur d'origine : a 6,7 Ko de
 * pile par niveau, 64 niveaux ne coutent que 436 Ko sur les 8 Mo du fil
 * principal. Une recursion emballee rend « trop de recursion » au lieu de
 * faire tomber l'application. */
#define HC_MAX_DEPTH 64

static Object *g_current_card = NULL;

/* ---- piles ouvertes -------------------------------------------------------
 *
 * Le noyau ne POSSÈDE aucune pile : hc_load en rend une, hc_free la libère,
 * et c'est l'hôte qui décide de leur sort. Mais pour que « stack "X" » désigne
 * autre chose que la pile courante, il faut bien qu'il sache lesquelles sont
 * ouvertes.
 *
 * D'où ce registre, que l'hôte tient à jour. Il ne détient rien non plus : ce
 * sont des pointeurs empruntés, et l'hôte doit retirer une pile AVANT de la
 * libérer, sans quoi le registre pointerait dans le vide. */
/* Le registre GRANDIT. Il était plafonné à seize, et au-delà « hc_register_stack »
 * ne faisait rien — silencieusement : la dix-septième pile s'ouvrait, l'hôte
 * l'affichait, mais « go to stack "X" » ne la trouvait pas et « the stacks » ne
 * la nommait pas. Un plafond qui ment est pire qu'un plafond qui refuse.
 *
 * Il n'y a d'ailleurs aucune raison de plafonner : ce sont des pointeurs
 * empruntés, huit octets par pile ouverte. */
static Object **g_stacks   = NULL;
static int      g_nstacks  = 0;
static int      g_capstacks = 0;

static void emit(HcLineKind kind, const char *fmt, ...);

/* Déclarées ici parce que erreurs_vide, tout en tête, en a besoin : la table
 * des réglages vit six mille lignes plus bas, et l'envoi de message huit mille
 * encore après. Le détournement de « lockErrorDialogs » relie les trois. */
static int  reglage_valeur(const char *nom);
static void reglage_eteint(const char *nom);

/* Les lignes d'erreur du gestionnaire en cours, et l'objet fautif.
 * Voir emit() et la sortie de hc_send_args. */
static char    g_err_texte[2048];
static int     g_err_n = 0;
static Object *g_err_objet = NULL;
/* Une ligne tapée dans la BOÎTE DE MESSAGE compte comme un gestionnaire pour
 * l'accumulation des erreurs, bien qu'elle tourne à g_depth == 0. Voir emit_v
 * et hc_do. */
static int     g_msg_box = 0;

static int g_visual_dirty = 0;
static char g_visual_effect[64] = "";
static char g_visual_speed[16]  = "";
static char g_visual_image[16]  = "";

/* L'outil courant et la sélection de peinture vivent chez l'HÔTE, qui les
 * connaît déjà : il les pose à la souris comme au script, et lui seul voit
 * les pixels. Les répliquer ici aurait fait une seconde source de vérité,
 * aveugle à tout ce que l'utilisateur fait à la main.
 *
 * Le noyau se contente donc de demander « the tool » quand il en a besoin,
 * par host_global("tool") — qui rend la forme complète, « select tool ». */
/* ---- piles en usage ----
 * Déclarées par « start using stack "X" », retirées par « stop using ». Ce
 * sont des pointeurs empruntés, comme le registre : fermer une pile la retire
 * aussi d'ici, sans quoi la chaîne de messages suivrait une adresse morte. */
/* Celui-ci garde un plafond, contrairement au registre des piles : les piles en
 * usage sont des maillons de la CHAÎNE DE MESSAGES, que « chain[] » parcourt
 * sur une taille fixe, et une chaîne sans bord serait une mauvaise idée. Mais
 * le dépassement se dit maintenant, au lieu d'ignorer la demande en silence. */
#define HC_MAX_USING 8
static Object *g_using[HC_MAX_USING];
static int     g_nusing = 0;

void hc_register_stack(Object *stack)
{
    if (!stack || stack->type != OBJ_STACK) return;
    for (int i = 0; i < g_nstacks; i++)
        if (g_stacks[i] == stack) return;             /* déjà connue */
    if (g_nstacks == g_capstacks) {
        int cap = g_capstacks ? g_capstacks * 2 : 16;
        Object **n = realloc(g_stacks, (size_t)cap * sizeof *n);
        /* Faute de place on ne enregistre pas, mais on le DIT : sans ce message
         * la pile serait ouverte et introuvable, ce qui est exactement le
         * défaut qu'on vient de corriger. */
        /* Continuer serait pire que s'arrêter : la pile est ouverte chez
         * l'hôte, une fenêtre la montre, et le noyau ne la connaîtrait pas —
         * « go to stack "X" » ne la trouverait pas, hc_unregister_stack ne la
         * retirerait de rien, et sa libération laisserait l'hôte avec une
         * adresse morte. Cette fonction ne rend rien, donc l'appelant ne peut
         * pas refuser l'ouverture : le seul choix honnête est le point unique
         * d'épuisement mémoire, qui prévient l'hôte avant d'abandonner. */
        if (!n) hc_memoire_epuisee("registre des piles ouvertes");
        g_stacks = n; g_capstacks = cap;
    }
    g_stacks[g_nstacks++] = stack;
}

void hc_unregister_stack(Object *stack)
{
    /* La retirer AUSSI des piles en usage : une bibliothèque qu'on ferme
     * laisserait sinon son adresse dans la chaîne de messages, et le premier
     * message envoyé après sa libération irait la lire. */
    for (int i = 0; i < g_nusing; i++) {
        if (g_using[i] != stack) continue;
        for (int k = i; k + 1 < g_nusing; k++) g_using[k] = g_using[k+1];
        g_nusing--;
        break;
    }

    for (int i = 0; i < g_nstacks; i++) {
        if (g_stacks[i] != stack) continue;
        for (int k = i; k + 1 < g_nstacks; k++) g_stacks[k] = g_stacks[k+1];
        g_nstacks--;
        return;
    }
}

/* Pile ouverte portant ce nom, ou NULL. Casse ignorée, comme partout ailleurs
 * en HyperTalk. */
static Object *find_open_stack(const char *nom)
{
    if (!nom || !*nom) return NULL;
    for (int i = 0; i < g_nstacks; i++)
        if (g_stacks[i]->name && ci_equal(g_stacks[i]->name, nom))
            return g_stacks[i];
    /* PAR SON CHEMIN, À DÉFAUT DE SON NOM.
     *
     * « the long name of this stack » rend maintenant le chemin du fichier,
     * comme chez HyperCard. Une forme longue qui ne se relit pas ne serait
     * qu'une décoration — c'est la leçon du « of … » —, donc le résolveur
     * accepte les deux. Le chemin est comparé tel quel, casse comprise : un
     * système de fichiers peut la distinguer, et deviner serait pire. */
    for (int i = 0; i < g_nstacks; i++)
        if (g_stacks[i]->path && strcmp(g_stacks[i]->path, nom) == 0)
            return g_stacks[i];
    return NULL;
}

int hc_stack_count(void) { return g_nstacks; }

/* Ce calque appartient-il encore à une pile ouverte ?
 *
 * L'hôte garde un cache de bitmaps indexé par POINTEUR de carte ou de fond, et
 * le déverse dans le noyau avant chaque enregistrement. Rien n'y invalidait les
 * clés : une carte coupée, une pile fermée, et le pointeur restait dans le
 * cache jusqu'à faire appeler free sur de la mémoire rendue — un plantage à
 * l'enregistrement, très loin de sa cause.
 *
 * On ne peut pas interroger un pointeur mort, mais on peut le chercher parmi
 * les vivants : le registre des piles ouvertes donne la liste complète, et un
 * calque absent de toutes n'existe plus. Comparaisons seulement, aucun
 * déréférencement de la valeur douteuse. */
int hc_layer_is_live(Object *layer)
{
    if (!layer) return 0;
    for (int i = 0; i < hc_stack_count(); i++) {
        Object *st = hc_stack_at(i);
        if (!st) continue;
        if (st == layer) return 1;
        for (int k = 0; k < st->nparts; k++)
            if (st->parts[k] == layer) return 1;
    }
    return 0;
}

/* Comme hc_layer_is_live, mais à toute profondeur : boutons et champs
 * compris. Pour une VARIABLE LOCALE, que object_gone ne peut pas mettre à
 * NULL puisqu'il ne la connaît pas.
 *
 *     hc_send(hit, "mouseDown");
 *     hc_send(hit, "mouseUp");     <- et si mouseDown a fait « delete me » ?
 *
 * Ne déréférence jamais son argument : il ne sert qu'à être comparé aux
 * objets vivants. */
int hc_object_is_live(Object *o)
{
    if (!o) return 0;
    for (int i = 0; i < hc_stack_count(); i++) {
        Object *st = hc_stack_at(i);
        if (!st) continue;
        if (st == o) return 1;
        for (int k = 0; k < st->nparts; k++) {
            Object *couche = st->parts[k];
            if (couche == o) return 1;
            for (int j = 0; j < couche->nparts; j++)
                if (couche->parts[j] == o) return 1;
        }
    }
    return 0;
}

Object *hc_stack_at(int i)
{
    return (i >= 0 && i < g_nstacks) ? g_stacks[i] : NULL;
}

static int     g_trace = 1;
static int     g_pass  = 0;   /* levé par `pass` : le message doit continuer */

/* `the result` : les commandes susceptibles d'échouer y déposent un message,
 * et le vident quand elles réussissent. Les autres commandes n'y touchent
 * pas — c'est ce qui permet d'écrire « go … » puis « put the result ». */
static char    g_result[HC_VAL] = "";

/* Levé dès qu'un « the script of … » a été tronqué faute de place dans un
 * tampon de valeur. Tant qu'il est levé, « set script of … » refuse d'écrire :
 * dans le gestionnaire courant, la valeur lue est forcément mutilée, et la
 * réécrire détruirait le script. Sauvé et remis à zéro à chaque entrée de
 * gestionnaire, pour qu'un appel imbriqué ne contamine pas son appelant. */
static int     g_script_clipped = 0;

/* Arguments du gestionnaire courant, pour `the params`, param(n), paramCount.
   g_params[0] est le nom du message ; les suivants sont les arguments. */
/* Quinze ARGUMENTS au plus, plus le nom du message en tête. C'est la taille
 * de cette table qui fixe la limite, et « param(n) », « the params » et
 * « the paramCount » la lisent tous ici : tout autre plafond posé ailleurs
 * dans le noyau serait une seconde vérité, plus basse et muette. */
#define HC_ARGS_MAX 15
static char    g_params[HC_ARGS_MAX + 1][HC_VAL];
static int     g_nparams = 0;

static void set_result(const char *msg) { snprintf(g_result, sizeof g_result, "%s", msg); }
static Object *g_me     = NULL;  /* l'objet dont le script s'exécute      → `me` */
static Object *g_target = NULL;  /* le destinataire initial du message    → `the target` */
static int     g_depth = 0;   /* profondeur, pour l'indentation de la trace */

void hc_trace(int on) { g_trace = on; }
void hc_set_current_card(Object *card) { g_current_card = card; }
Object *hc_current_card(void) { return g_current_card; }

/* Un script est-il en cours ? Sert à l'hôte pour ne pas envoyer « idle »
 * pendant qu'un gestionnaire s'exécute : les deux s'imbriqueraient, et une
 * animation lancée depuis idle se relancerait à chaque tour de sa propre
 * boucle. */
int hc_is_running(void) { return g_depth > 0; }

const char *hc_typename(ObjType t)
{
    switch (t) {
        case OBJ_STACK:      return "stack";
        case OBJ_BACKGROUND: return "background";
        case OBJ_CARD:       return "card";
        case OBJ_BUTTON:     return "button";
        case OBJ_FIELD:      return "field";
    }
    return "?";
}

void hc_describe(Object *o, char *buf, int buflen)
{
    if (!o) { snprintf(buf, buflen, "(nul)"); return; }
    if (o->name)
        snprintf(buf, buflen, "%s \"%s\"", hc_typename(o->type), o->name);
    else
        snprintf(buf, buflen, "%s id %d", hc_typename(o->type), o->id);
}

/* Définis plus bas, mais hc_nom_de en a besoin. */

/* UN TERME DE DESCRIPTEUR : « card "Une" », ou « card id 101 » s'il n'a pas de
 * nom. C'est la brique dont hc_nom_de compose la portée. */
/* Taille d'un nom cité dans un descripteur. Les noms du modèle sont des
 * char* sans limite ; les descripteurs, eux, voyagent dans des tampons. Au
 * delà, on ne tronque pas — voir terme_objet. */
#define HC_NOM_MAX 1024

static void terme_objet(Object *o, const char *type, char *buf, int buflen)
{
    if (o->name && o->name[0]) {
        char cite[HC_NOM_MAX];
        int besoin = descripteur_cite(o->name, cite, sizeof cite);
        if (besoin < (int)sizeof cite &&
            snprintf(buf, buflen, "%s %s", type, cite) < buflen)
            return;
        /* UN NOM TROP LONG NE SE TRONQUE PAS : IL CHANGE DE FORME.
         *
         * Un descripteur tronqué est pire qu'absent — il désigne un autre
         * objet, ou aucun, et rien ne le dit. Mesuré avec un nom de 300
         * caractères : « the long name » rendait 187 caractères, et « the id
         * of » ce résultat rendait le littéral « id of r ».
         *
         * « type id N » désigne exactement le même objet, tient dans n'importe
         * quel tampon, et se re-résout toujours. C'est la même sortie que pour
         * un objet sans nom — donc pas une invention, juste l'autre forme
         * légitime du même descripteur. */
    }
    snprintf(buf, buflen, "%s id %d", type, o->id);
}

/* LE NOM D'UN OBJET, SOUS SES TROIS FORMES.
 *
 * Une seule définition, parce que les trois doivent rester cohérentes : ce que
 * « the long name » écrit, le résolveur doit savoir le relire.
 *
 *   HC_NOM_COURT   Bouton
 *   HC_NOM_ABREGE  button "Bouton"
 *   HC_NOM_LONG    card button "Bouton" of card id 101 of stack "Pile"
 *
 * La forme LONGUE est faite pour être RE-RÉSOLUE, et c'est tout son intérêt :
 * « the long name of me » passé à une fonction, puis employé comme référence
 * depuis une autre carte, doit retrouver le même objet. Elle porte donc la
 * couche — « card button » ou « bkgnd button », qui ne désignent pas le même
 * objet — et la portée complète jusqu'à la pile.
 *
 * Une part de FOND s'ancre sur son FOND et non sur une carte : elle existe
 * indépendamment de celle qu'on regarde, et son identité ne change pas d'une
 * carte à l'autre.
 *
 * Ce que HyperCard met et que nous n'avons pas : le CHEMIN du fichier dans
 * « stack "…" ». Le noyau ne connaît pas le chemin de ses piles — c'est le
 * document qui le tient —, et le nom suffit pour retrouver une pile ouverte,
 * ce qui est la propriété qu'on veut ici. */
void hc_nom_de(Object *o, int forme, char *out, int outlen)
{
    if (!o || outlen <= 0) { if (outlen > 0) out[0] = '\0'; return; }

    if (forme == HC_NOM_COURT) {
        snprintf(out, outlen, "%s", o->name ? o->name : "");
        return;
    }
    /* LA FORME ABRÉGÉE PORTE LA COUCHE, elle aussi.
     *
     * Elle rendait « button "ok" », ce qui ne désigne PAS un objet : une carte
     * et son fond peuvent porter chacun un bouton de ce nom, et rien dans la
     * réponse ne disait lequel. « the name of me » était donc inutilisable
     * pour désigner l'objet dont il venait — or c'est tout ce qu'on lui
     * demande.
     *
     * hc_describe, lui, ne change pas : c'est une ÉTIQUETTE de diagnostic, pas
     * un descripteur. Les traces disent « button "ok" » et continueront.
     *
     * Le mot est « bkgnd », celui d'HyperCard, et le même que la forme longue
     * emploie déjà : deux orthographes pour la même chose dans les deux formes
     * d'un même nom seraient une invitation à l'erreur. */
    if (forme != HC_NOM_LONG) {
        switch (o->type) {
        case OBJ_BACKGROUND: terme_objet(o, "bkgnd", out, outlen); return;
        case OBJ_BUTTON:
        case OBJ_FIELD: {
            char type[32];
            snprintf(type, sizeof type, "%s %s",
                     hc_owner_is_bg(o) ? "bkgnd" : "card",
                     o->type == OBJ_BUTTON ? "button" : "field");
            terme_objet(o, type, out, outlen);
            return;
        }
        default: hc_describe(o, out, outlen); return;
        }
    }

    Object *pile = owning_stack(o);
    char pl[HC_NOM_MAX + 64];
    if (!pile)                 snprintf(pl, sizeof pl, "%s", "");
    /* LE CHEMIN PLUTÔT QUE LE NOM, quand on le connaît. C'est ce que met
     * HyperCard, et c'est ce qui distingue deux piles ouvertes qui portent le
     * même nom. resolve sait relire les deux. */
    else if (pile->path && pile->path[0]) {
        /* Le chemin passe par la même porte : un dossier peut contenir un
         * guillemet, et le descripteur doit rester relisible. */
        char cite[HC_NOM_MAX];
        int besoin = descripteur_cite(pile->path, cite, sizeof cite);
        if (besoin < (int)sizeof cite)
            snprintf(pl, sizeof pl, "stack %s", cite);
        else
            terme_objet(pile, "stack", pl, sizeof pl);
    }
    else                       terme_objet(pile, "stack", pl, sizeof pl);

    switch (o->type) {
    case OBJ_STACK:
        snprintf(out, outlen, "%s", pl);
        return;

    case OBJ_BACKGROUND: {
        char t[HC_NOM_MAX + 64]; terme_objet(o, "bkgnd", t, sizeof t);
        if (*pl) snprintf(out, outlen, "%s of %s", t, pl);
        else     snprintf(out, outlen, "%s", t);
        return;
    }
    case OBJ_CARD: {
        char t[HC_NOM_MAX + 64]; terme_objet(o, "card", t, sizeof t);
        if (*pl) snprintf(out, outlen, "%s of %s", t, pl);
        else     snprintf(out, outlen, "%s", t);
        return;
    }
    case OBJ_BUTTON:
    case OBJ_FIELD: {
        int fond = hc_owner_is_bg(o);
        char t[HC_NOM_MAX + 64];
        char type[32];
        snprintf(type, sizeof type, "%s %s", fond ? "bkgnd" : "card",
                 o->type == OBJ_BUTTON ? "button" : "field");
        terme_objet(o, type, t, sizeof t);

        /* La couche qui la porte : le fond pour une part de fond, la carte
         * courante pour une part de carte — c'est bien celle-là, une part de
         * carte n'existant que sur la sienne. */
        Object *porteur = o->owner;
        if (!porteur) { snprintf(out, outlen, "%s", t); return; }
        char pc[HC_NOM_MAX + 64];
        terme_objet(porteur, fond ? "bkgnd" : "card", pc, sizeof pc);
        if (*pl) snprintf(out, outlen, "%s of %s of %s", t, pc, pl);
        else     snprintf(out, outlen, "%s of %s", t, pc);
        return;
    }
    }
    hc_describe(o, out, outlen);
}

/* ---- hôte : sortie déléguée ---- */

static void console_line(HcLineKind kind, int depth, const char *text)
{
    int n = depth > 12 ? 12 : depth;
    for (int i = 0; i < n; i++) fputs("   ", stdout);
    if (kind == HC_MSG) fputs("   [message box] ", stdout);
    fputs(text, stdout);
    fputc('\n', stdout);
}

/* Repli console : de quoi tester le noyau sans interface graphique. */
static char g_console_buf[HC_VAL];

static const char *console_ask(const char *prompt, const char *deflt)
{
    printf("   [ask] %s [%s] ", prompt, deflt ? deflt : "");
    fflush(stdout);
    if (!fgets(g_console_buf, sizeof g_console_buf, stdin)) return NULL;
    size_t n = strlen(g_console_buf);
    while (n && (g_console_buf[n-1] == '\n' || g_console_buf[n-1] == '\r'))
        g_console_buf[--n] = '\0';
    if (!g_console_buf[0] && deflt) {
        snprintf(g_console_buf, sizeof g_console_buf, "%s", deflt);
    }
    return g_console_buf;
}

static const char *console_answer(const char *prompt, const char *b1,
                                  const char *b2, const char *b3)
{
    printf("   [answer] %s  (1=%s", prompt, b1 ? b1 : "OK");
    if (b2) printf(" 2=%s", b2);
    if (b3) printf(" 3=%s", b3);
    printf(") ");
    fflush(stdout);
    if (!fgets(g_console_buf, sizeof g_console_buf, stdin)) return b1;
    int c = hc_entier(g_console_buf, 0, 9, 0);
    if (c == 3 && b3) return b3;
    if (c == 2 && b2) return b2;
    return b1 ? b1 : "OK";
}

/* Valeurs par défaut en console : la souris est relâchée et aucune touche
 * n'est enfoncée. C'est ce qui permet à « repeat until the mouse is up » de
 * se terminer immédiatement en ligne de commande au lieu de boucler à vide. */
static const char *console_global(const char *name)
{
    if (ci_equal(name, "mouse"))      return "up";
    if (ci_equal(name, "mouseLoc"))   return "0,0";
    if (ci_equal(name, "optionKey"))  return "up";
    if (ci_equal(name, "commandKey")) return "up";
    if (ci_equal(name, "shiftKey"))   return "up";
    return NULL;
}

/* Initialiseurs nommés : la structure gagne des champs au fil du temps, et une
 * liste positionnelle oblige à recompter à chaque ajout — c'est ainsi qu'un
 * callback finit branché sur le mauvais membre. */
static const HcHost g_console_host = {
    .line       = console_line,
    .ask        = console_ask,
    .answer     = console_answer,
    .global_get = console_global,
};
static const HcHost *g_host = &g_console_host;

void hc_set_host(const HcHost *h) { g_host = h ? h : &g_console_host; }

/* Émet une ligne vers l'hôte. Le format ne doit PAS inclure le saut de ligne
   final ni l'indentation : l'hôte s'en charge. */
/* LE CORPS, EN va_list — pour n'avoir qu'UNE mise en œuvre.
 *
 * emit reste la porte ordinaire ; hc_emet_erreur, plus bas, est celle que les
 * autres fichiers du noyau empruntent. Les deux passent par ici.
 *
 * Écrire hc_emet_erreur comme une copie de emit aurait marché le premier
 * jour : elle aurait envoyé la ligne à l'hôte. Elle aurait manqué la
 * SECONDE moitié du travail — l'accumulation des erreurs pour le dialogue de
 * fin — et les fautes de syntaxe, qui viennent toutes de hc_script.c,
 * seraient sorties dans la console sans jamais paraître dans la fenêtre. Un
 * corps partagé rend cet oubli impossible. */
/* REMET LES ERREURS ACCUMULÉES À L'HÔTE, EN UNE FOIS.
 *
 * Une seule faute produit plusieurs lignes — le message, l'extrait du script,
 * le résumé —, et les donner une par une ouvrirait trois dialogues. On vide
 * AVANT d'appeler, pour que l'hôte puisse relancer un script depuis son
 * dialogue sans se voir resservir l'erreur précédente.
 *
 * DEUX APPELANTS, UNE SEULE MISE EN ŒUVRE : la fin du gestionnaire le plus
 * extérieur, et la fin d'une ligne de la boîte de message. Ce corps était
 * écrit dans le premier ; la boîte de message n'avait rien, et ses erreurs
 * n'arrivaient qu'à la console. */
/* « the lockErrorDialogs » : L'ERREUR PART EN MESSAGE, PAS EN DIALOGUE.
 *
 * C'est la serrure dont HC avait la porte sans la clé. hct_verif.c listait
 * « errordialog » parmi les messages système légitimes depuis toujours, mais
 * RIEN ne l'envoyait jamais, et la propriété qui l'allume n'existait pas.
 * Une moitié de mécanisme, et aucun moyen de s'en apercevoir sans une pile
 * qui l'emploie.
 *
 * HypoGraph 0.91 l'emploie, aux deux bouts. Dans son script de pile :
 *
 *     on errorDialog them
 *       answer them with "Cancel"
 *       choose browse tool
 *     end errorDialog
 *
 * et dans son bouton « draw », en tête du gestionnaire :
 *
 *     --  set the lockErrorDialogs to true
 *
 * L'auteur traçait des milliers de points et ne voulait pas du dialogue
 * d'erreur de HyperCard à chacun — celui qui propose d'ouvrir l'éditeur de
 * script — mais le sien, avec un seul bouton.
 *
 * SI PERSONNE NE PREND LE MESSAGE, on ne remet pas le dialogue : la pile a
 * demandé le silence, et le lui rendre contre son gré viderait la propriété
 * de son sens. Mais on ne se tait pas non plus — la ligne est déjà partie au
 * moniteur par emit_v, et on y ajoute de quoi comprendre, sinon une pile qui
 * pose la serrure sans écrire le gestionnaire deviendrait aveugle sans savoir
 * pourquoi.
 *
 * LE GARDE-FOU : si le gestionnaire errorDialog tombe lui-même en erreur, le
 * détournement l'enverrait à errorDialog, et ainsi de suite. Pendant son
 * exécution on repasse donc par le dialogue ordinaire. */
static int g_dans_errordialog = 0;

/* La dernière ligne accumulée, pour compter ses répétitions plutôt que de les
 * empiler. Voir la note dans emit_v. */
static char g_err_derniere[1024];
static int  g_err_dernier_deb = -1;
static int  g_err_repete = 0;

static void erreurs_vide(void)
{
    if (g_err_n <= 0) return;
    char copie[sizeof g_err_texte];
    memcpy(copie, g_err_texte, (size_t)g_err_n + 1);
    Object *coupable = g_err_objet;
    g_err_n = 0; g_err_texte[0] = '\0'; g_err_objet = NULL;
    g_err_dernier_deb = -1; g_err_repete = 0; g_err_derniere[0] = '\0';

    Object *carte = hc_current_card();
    if (reglage_valeur("lockerrordialogs") && !g_dans_errordialog && carte) {
        g_dans_errordialog = 1;
        int pris = hc_send_arg(carte, "errorDialog", copie);
        g_dans_errordialog = 0;
        if (!pris)
            emit(HC_ERR, "   !! lockErrorDialogs est posé et aucun "
                         "gestionnaire « on errorDialog » n'a pris l'erreur");
        return;
    }

    if (g_host && g_host->erreur) g_host->erreur(copie, coupable);
}

static void emit_v(HcLineKind kind, const char *fmt, va_list ap)
{
    /* Tampon propre, hors arène : arena_buf() appelle emit() en cas de
     * saturation, et une récursion mutuelle entre l'allocateur et le
     * rapporteur d'erreurs serait fatale. Les messages sont courts. */
    char buf[1024];
    vsnprintf(buf, sizeof buf, fmt, ap);
    if (g_host && g_host->line) g_host->line(kind, g_depth, buf);

    /* ON RETIENT LES ERREURS POUR LES DIRE À LA FIN, EN UNE FOIS.
     *
     * Une seule erreur produit plusieurs lignes — le message, l'extrait du
     * script, le résumé. Les donner au dialogue une par une en ouvrirait
     * trois. On les accumule donc, et v3 les remet à la sortie du
     * gestionnaire le plus extérieur.
     *
     * LA BOÎTE DE MESSAGE COMPTE, elle aussi, et ce commentaire a dit le
     * contraire pendant des semaines : « hors gestionnaire, rien à accumuler,
     * personne n'attend derrière, et l'appelant a déjà eu la ligne ».
     *
     * « L'appelant a déjà eu la ligne » voulait dire : elle est partie par
     * le rappel `line`, que l'hôte Cocoa écrit dans la console de Xcode. Ce
     * qui revient, pour qui utilise l'application, à ne RIEN recevoir.
     * Mesuré, en tapant dans la boîte de message :
     *
     *     put the zorglub of this card   -> console seulement, pas de dialogue
     *     put field "Absent"             -> idem
     *     zorglub                        -> idem
     *     repeat with i = 1 up to 5      -> idem
     *
     * Les quatre sortes d'erreur, pas seulement la première. Et c'est
     * justement là qu'on a le plus besoin du dialogue : on vient de taper une
     * ligne et on attend une réponse.
     *
     * hc_do pose donc g_msg_box le temps de sa ligne, et vide comme le fait
     * le gestionnaire le plus extérieur. */
    if (kind == HC_ERR && (g_depth > 0 || g_msg_box)) {
        if (!g_err_n) g_err_objet = g_me;   /* le coupable, pour « Script » */

        /* UNE MÊME LIGNE RÉPÉTÉE SE COMPTE, ELLE NE S'EMPILE PAS.
         *
         * Une faute dans une boucle produit autant de lignes que de tours.
         * Signalé à l'écran : un traceur de courbes en mode point appelle
         * « click at » une fois par abscisse, et le refus d'une coordonnée non
         * finie remplissait le dialogue de sept lignes identiques — puis de
         * mille, sur une courbe plus fine.
         *
         * Sept copies n'apprennent rien de plus qu'une. Le NOMBRE, lui,
         * apprend quelque chose : il dit si la faute est un accident ou toute
         * la boucle. On garde donc la ligne une fois, suivie de son compte.
         *
         * Seules les lignes CONSÉCUTIVES et IDENTIQUES fusionnent : deux
         * fautes différentes qui alternent restent toutes les deux visibles,
         * ce qui est le cas où l'on a besoin de les voir. */
        if (g_err_dernier_deb >= 0 && strcmp(buf, g_err_derniere) == 0) {
            g_err_repete++;
            int place = (int)sizeof g_err_texte - g_err_dernier_deb - 2;
            if (place > 0) {
                int mis = snprintf(g_err_texte + g_err_dernier_deb,
                                   (size_t)place + 1, "%s (× %d)",
                                   g_err_derniere, g_err_repete);
                if (mis > place) mis = place;
                g_err_n = g_err_dernier_deb + mis;
            }
            return;
        }

        int reste = (int)sizeof g_err_texte - g_err_n - 2;
        if (reste > 0) {
            int debut = g_err_n + (g_err_n ? 1 : 0);   /* après le saut de ligne */
            int mis = snprintf(g_err_texte + g_err_n, (size_t)reste + 1,
                               "%s%s", g_err_n ? "\n" : "", buf);
            if (mis > reste) mis = reste;   /* tronqué : on garde ce qui tient */
            g_err_n += mis;
            g_err_dernier_deb = debut;
            g_err_repete = 1;
            snprintf(g_err_derniere, sizeof g_err_derniere, "%s", buf);
        }
    }
}

static void emit(HcLineKind kind, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_v(kind, fmt, ap);
    va_end(ap);
}

/* La porte des erreurs pour les AUTRES fichiers du noyau. Voir hc_interne.h,
 * qui dit pourquoi on n'exporte pas `emit` lui-même. */
void hc_emet_erreur(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_v(HC_ERR, fmt, ap);
    va_end(ap);
}

/* ==================== arène de tampons ====================
 *
 * Les tampons de valeur ne vivent plus sur la pile. Mesure avant ce
 * changement : 75 tampons vivants par niveau de récursion, soit 75 × HC_VAL
 * par appel de gestionnaire. HC_VAL ne pouvait donc pas dépasser quelques
 * kilo-octets sans faire déborder les 8 Mo du fil principal — et un script
 * un peu long devenait intronçonnable.
 *
 * Principe : une arène à pointeur de sommet. Allouer avance le sommet ;
 * libérer le ramène où il était. Chaque fonction note le sommet à l'entrée
 * (ARENA_MARK) et le restaure avant de rendre la main (ARENA_FREE). La
 * discipline est sûre parce qu'un appelé note toujours un sommet PLUS HAUT
 * que son appelant : sa libération ne peut donc jamais toucher les tampons
 * de celui qui l'a appelé.
 *
 * L'arène est faite de blocs chaînés jamais déplacés : un realloc
 * invaliderait les tampons déjà distribués. Les blocs restent alloués après
 * libération, et resservent — on ne paie l'allocation système qu'une fois.
 *
 * Les fonctions à sorties multiples (term_value, call_function : 40 et 36
 * return) ne sont volontairement pas instrumentées. Leur appelant direct,
 * parse_factor, l'est, et cela suffit :
 * toute valeur est recopiée dans le `out` de l'appelant avant chaque retour.
 */
/* Blocs de 16 Mo, plafond à 1 Go.
 *
 * L'arène est allouée À LA DEMANDE : ce plafond ne coûte rien tant qu'on ne
 * s'en approche pas, et une pile ordinaire n'en emploiera jamais un seul bloc.
 * Il ne sert qu'à borner l'emballement — un script qui alloue en boucle doit
 * s'arrêter quelque part plutôt que de faire ramer la machine entière.
 *
 * Les valeurs précédentes — blocs de 4 Mo, plafond de 128 Mo — saturaient dès
 * qu'on lisait un fichier de quelques centaines de kilo-octets : chaque
 * expression alloue plusieurs tampons de HC_VAL, et HC_VAL a grandi avec les
 * commandes de fichier. Les deux limites doivent monter ensemble. */
#define HC_ARENA_BLOCK     (16u * 1024u * 1024u)
#define HC_ARENA_MAXBLOCKS 64             /* plafond dur : 1 Go */

static char  *g_ablk[HC_ARENA_MAXBLOCKS];
static int    g_ablk_count = 0;
static size_t g_atop = 0;                 /* sommet virtuel : bloc × taille + offset */
static size_t g_ahigh = 0;                /* plus haut sommet atteint (diagnostic) */
static char   g_apanic[HC_VAL];           /* filet en cas d'arène saturée */

#define ARENA_MARK  size_t _amark = g_atop
#define ARENA_FREE  (g_atop = _amark)

static void *arena_alloc(size_t n)
{
    /* Vérifier AVANT l'arrondi : n + 15 peut déborder size_t si l'appelant
     * s'est trompé. Les appels actuels sont petits, mais l'arène est justement
     * le dernier endroit où transformer une taille impossible en petite
     * allocation apparemment valide. */
    if (n == 0 || n > HC_ARENA_BLOCK) return NULL;
    n = (n + 15u) & ~(size_t)15;                    /* alignement confortable */

    size_t bi  = g_atop / HC_ARENA_BLOCK;
    size_t off = g_atop % HC_ARENA_BLOCK;
    if (off + n > HC_ARENA_BLOCK) { bi++; off = 0; }   /* ne pas chevaucher */
    if (bi >= HC_ARENA_MAXBLOCKS) return NULL;

    while (g_ablk_count <= (int)bi) {
        char *b = (char *)malloc(HC_ARENA_BLOCK);
        if (!b) return NULL;
        g_ablk[g_ablk_count++] = b;
    }
    g_atop = bi * (size_t)HC_ARENA_BLOCK + off + n;
    if (g_atop > g_ahigh) g_ahigh = g_atop;
    return g_ablk[bi] + off;
}

/* Un tampon de valeur. Ne renvoie jamais NULL : en cas de saturation on rend
 * le filet, ce qui dégrade le résultat mais n'écrase pas la mémoire. */
static char *arena_buf(void)
{
    char *p = (char *)arena_alloc(HC_VAL);
    if (!p) { emit(HC_ERR, "   !! arène de tampons saturée"); p = g_apanic; }
    p[0] = '\0';
    return p;
}

/* n tampons contigus, pour les tableaux d'arguments.
 *
 * Contrairement à arena_buf, il n'existe PAS de repli sûr : g_apanic ne fait
 * qu'une ligne HC_VAL, alors qu'un appelant de arena_rows(16) en indexe seize.
 * Le caster en tableau donnait donc l'illusion d'un secours tout en écrivant
 * jusqu'à 15 Mo après le tampon. On rend NULL et chaque appelant abandonne
 * proprement l'opération en cours. */
static char (*arena_rows(int n))[HC_VAL]
{
    if (n <= 0 || (size_t)n > HC_ARENA_BLOCK / HC_VAL) {
        emit(HC_ERR, "   !! demande de tampons d'arène impossible");
        return NULL;
    }

    char (*p)[HC_VAL] = (char (*)[HC_VAL])arena_alloc((size_t)n * HC_VAL);
    if (!p) {
        emit(HC_ERR, "   !! arène de tampons saturée");
        return NULL;
    }
    for (int i = 0; i < n; i++) p[i][0] = '\0';
    return p;
}

static void arena_shutdown(void)
{
    for (int i = 0; i < g_ablk_count; i++) free(g_ablk[i]);
    g_ablk_count = 0;
    g_atop = g_ahigh = 0;
}

/* Écran verrouillé ? Suivi ici plutôt que relu chez l'hôte : la question est
 * posée à chaque écriture dans un champ, et un aller-retour par écriture
 * coûterait plus cher que ce qu'il évite. Posé par « lock/unlock screen » et
 * par « set lockScreen to … ». */
static int g_ecran_verrouille = 0;

/* « lock messages » : les messages SYSTÈME — openCard, closeCard,
 * openBackground, closeBackground — ne partent plus.
 *
 * C'est ce qui permet à un script de parcourir une pile sans réveiller les
 * gestionnaires de chaque carte : un sommaire qui relève un champ sur cent
 * cartes n'a pas à déclencher cent openCard. Sans cette commande, un tel
 * parcours exécute les scripts de toute la pile et peut échouer sur une
 * carte dont le gestionnaire suppose autre chose.
 *
 * Seuls les messages système sont retenus : un « send » explicite part
 * quand même, comme dans HyperCard. */
static int g_messages_verrouilles = 0;

/* La table des réglages d'environnement — userLevel, dragSpeed, blindTyping,
 * powerKeys, lockRecent, textArrows — vit bien plus bas, avec truthy et
 * as_num dont elle se sert. Sa LECTURE est demandée avant, par les deux
 * chemins de « the xxx ». D'où cette déclaration anticipée. */
static int reglage_lit(const char *nom, char *out, int outlen);

/* Les compteurs de l'ancien interprète — définis avec le relevé du bilan,
 * bien plus bas, mais appelés par du code qui le précède. */
static void v1_compte(const char *quoi, const char *porte);
static const char *v1_porte(const char *nom);
static const char *g_v1_porte = "?";

/* Un message SYSTÈME, retenu quand les messages sont verrouillés. Tous les
 * envois automatiques de changement de carte passent par ici — et eux seuls,
 * pour que « send » continue de partir. */
static void histo_arrive(Object *card);   /* l'historique, défini plus bas */

static void hc_send_systeme(Object *o, const char *message)
{
    if (!o) return;
    /* L'arrivée sur une carte se note ICI, et AVANT le verrou : « lock
     * messages » empêche le script de réagir, pas la navigation d'avoir eu
     * lieu. Un seul point pour les six endroits qui envoient openCard, et
     * pour ceux qu'on ajouterait. */
    if (o->type == OBJ_CARD && ci_equal(message, "openCard")) histo_arrive(o);
    if (g_messages_verrouilles) return;
    hc_send(o, message);
}

/* Champs modifiés pendant le verrou, à rafraîchir au déverrouillage.
 *
 * Demander un rafraîchissement GLOBAL au déverrouillage, comme je le faisais,
 * pouvait coûter plus cher que les rafraîchissements qu'on venait d'éviter :
 * l'hôte repeint alors la carte entière — boutons, image, tous les champs —
 * là où sans « lock screen » il n'aurait marqué qu'un seul champ. On retient
 * donc les champs touchés et on ne réveille que ceux-là.
 *
 * Au-delà de la capacité on retombe sur le rafraîchissement global, qui reste
 * correct : mieux vaut trop repeindre que pas assez. */
#define HC_VERROU_MAX 64
static Object *g_verrou_champs[HC_VERROU_MAX];
static int     g_verrou_n = 0;
static int     g_verrou_deborde = 0;

static void verrou_retiens(Object *field)
{
    for (int i = 0; i < g_verrou_n; i++)
        if (g_verrou_champs[i] == field) return;
    if (g_verrou_n >= HC_VERROU_MAX) { g_verrou_deborde = 1; return; }
    g_verrou_champs[g_verrou_n++] = field;
}

static void verrou_reveille(void)
{
    if (g_verrou_deborde) g_visual_dirty = 1;
    else if (g_host && g_host->field_changed)
        for (int i = 0; i < g_verrou_n; i++)
            g_host->field_changed(g_verrou_champs[i]);
    g_verrou_n = 0;
    g_verrou_deborde = 0;
}

/* Signale à l'hôte qu'un champ a changé (rafraîchissement d'affichage).
 *
 * Rien ne sort tant que l'écran est verrouillé : c'est la raison d'être de
 * « lock screen », et c'est ce qui rend supportable une boucle qui écrit dix
 * mille fois dans un champ. Sans ce filtre, chaque écriture provoquait un
 * redessin — l'interprétation ne pesait plus rien et l'affichage tout. */
static void notify_field(Object *field)
{
    if (g_ecran_verrouille) { verrou_retiens(field); return; }
    if (g_host && g_host->field_changed) g_host->field_changed(field);
}

/* Propriété globale lue chez l'hôte. NULL = nom inconnu.
 *
 * « cmdKey » EST « commandKey », ET PERSONNE NE LE SAVAIT.
 *
 * HyperTalk accepte les deux orthographes pour la même touche, et la table
 * V3_GLOBALES_HOTE les annonce toutes les deux. Mais un hôte implémente ce
 * qu'il lit dans une documentation, pas ce qu'une table du noyau promet : ni
 * l'hôte Cocoa ni l'hôte console n'avaient de branche « cmdKey ». Le noyau
 * transmettait donc fidèlement un nom que personne ne servait, et
 * « if the cmdKey is down » — une ligne ordinaire dans une pile de 1990 —
 * répondait « propriété ou fonction inconnue » au lieu de « up ».
 *
 * La traduction se fait ICI plutôt que dans chaque hôte, et c'est le point :
 * un synonyme d'orthographe n'est pas une connaissance de l'hôte. Réparé chez
 * l'un, il serait resté cassé chez l'autre — c'est exactement la faute qu'on
 * répète depuis des semaines, un chemin corrigé et son jumeau oublié. Un seul
 * passage obligé, et les deux hôtes guérissent ensemble, comme guérira celui
 * qu'on n'a pas encore écrit.
 *
 * Les couleurs, elles, ne sont pas ici : leurs cinq synonymes sont résolus
 * par une table de l'hôte qui sert la lecture ET l'écriture. Les y laisser
 * évite de partager une même liste entre deux fichiers. */
static const char *host_global(const char *name)
{
    if (name && ci_equal(name, "cmdKey")) name = "commandKey";
    if (g_host && g_host->global_get) return g_host->global_get(name);
    return NULL;
}

static void host_global_set(const char *name, const char *value)
{
    if (g_host && g_host->global_set) g_host->global_set(name, value);
}

/* Attendre une DURÉE réelle, en laissant l'hôte respirer.
 *
 * L'attente comptait des TOURS de host_idle, en supposant qu'un tour valait un
 * tick : c'était vrai tant que l'hôte dormait un soixantième de seconde à
 * chaque appel. Dès qu'il rend la main tout de suite — ce qu'il doit faire
 * pour qu'une boucle de tracé aille vite — « wait 10 seconds » passait en un
 * éclair. On mesure donc le temps, et l'on dort entre deux tours plutôt que de
 * brûler le processeur.
 *
 * Le pas de sommeil est court, un deux-cent-quarantième de seconde : assez
 * pour ne rien consommer, assez fin pour que « wait 1 tick » reste précis. */
static void host_idle(void);

static void attends(double secondes)
{
    if (secondes <= 0) { host_idle(); return; }

    struct timespec t0, t;
    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) { host_idle(); return; }

    for (;;) {
        host_idle();
        clock_gettime(CLOCK_MONOTONIC, &t);
        double ecoule = (double)(t.tv_sec - t0.tv_sec)
                      + (double)(t.tv_nsec - t0.tv_nsec) / 1e9;
        if (ecoule >= secondes) return;

        double reste = secondes - ecoule;
        double pause = reste < 1.0 / 240.0 ? reste : 1.0 / 240.0;
        struct timespec dodo = { 0, (long)(pause * 1e9) };
        nanosleep(&dodo, NULL);
    }
}

/* Respiration : l'hôte redessine et traite ses événements. */
static void host_idle(void)
{
    if (g_host && g_host->idle) g_host->idle();
}

/* ==================== construction ==================== */

static int g_next_id = 1;

/* resultat de la derniere recherche : the foundText / foundField / foundLine */
static char    g_found_text[256] = "";
static Object *g_found_field = NULL;
static int     g_found_line = 0;
static int     g_found_start = 0;   /* offset du motif dans le texte du champ */
static int     g_found_len   = 0;   /* longueur du motif, 0 = rien de trouve */
static Object *g_found_card  = NULL;

/* LA BOÎTE EST MONTRÉE, OU ELLE NE L'EST PLUS — et c'est autre chose que
 * « quelque chose a été trouvé ».
 *
 * HyperCard encadre le texte trouvé, et cet encadré disparaît au premier clic.
 * Les fonctions, elles, continuent de répondre : « the foundChunk » reste
 * valide jusqu'à la recherche suivante, c'est ce qui permet d'écrire
 *
 *     find "x"
 *     if the result is empty then select the foundChunk
 *
 * après avoir cliqué entre les deux. Un seul drapeau pour les deux notions
 * aurait donc effacé la réponse en même temps que le dessin. */
static int     g_found_montre = 0;

/* ---- sélection de texte ----
 * « select char 3 to 9 of field "toc" » pose une plage ; « the selection » la
 * relit. Le noyau ne fait que la RETENIR : c'est l'hôte qui la montre à
 * l'écran, via le callback selection_changed, parce que la surbrillance
 * appartient à l'éditeur de champ de l'interface et non au modèle.
 *
 * LES BORNES SONT EN OCTETS, dans le texte rendu par hc_field_text, et
 * demi-ouvertes : [start, start+len). Une longueur nulle est un simple point
 * d'insertion, ce qui est exactement ce que veut dire « select before char N »
 * dans HyperTalk.
 *
 * Ce commentaire disait « en caractères » et le code comptait des octets —
 * une contradiction sans conséquence tant que tout était en ASCII, et une
 * source d'erreurs dès que char a su compter les caractères accentués.
 *
 * L'OCTET EST LA BONNE UNITÉ INTERNE, et ce n'est pas un pis-aller :
 * hct_chunk_bornes rend des octets, la recherche rend des octets, le clic
 * dans un champ rend des octets, et le rendu du texte les convertit déjà vers
 * l'UTF-16 de Cocoa. Une seule convention à l'intérieur, et la conversion
 * UNIQUEMENT aux frontières :
 *
 *     noyau            octets UTF-8
 *     HyperTalk        numéros de caractères   (hct_utf8_compte_prefixe)
 *     Cocoa            unités UTF-16           (HCtext.m, et l'éditeur)
 *
 * « the selection » lit ces octets et rend le bon texte. C'est « the
 * selectedChunk » qui mentait : il annonçait l'offset d'octet comme un numéro
 * de caractère, d'où « char 1 to 2 » pour le premier é d'« été ». */
static Object *g_sel_field = NULL;
static int     g_sel_start = 0;
static int     g_sel_len   = 0;

void hc_set_selection(Object *field, int start, int len)
{
    if (field && field->type != OBJ_FIELD) return;
    if (start < 0) start = 0;
    if (len   < 0) len   = 0;

    if (field) {
        int n = (int)strlen(hc_field_text(field));
        if (start > n)       start = n;
        if (start + len > n) len   = n - start;
    }

    g_sel_field = field;
    g_sel_start = field ? start : 0;
    g_sel_len   = field ? len   : 0;

    /* Un changement de sélection est un changement VISIBLE : sans lever le
     * drapeau, cocoa_idle croit qu'il n'y a rien à repeindre et rend la main
     * sans rafraîchir. Une boucle « repeat while the mouse is down » qui suit
     * le pointeur ne montrait alors sa sélection qu'au relâchement. */
    g_visual_dirty = 1;

    /* L'hôte doit poser la surbrillance : sans cela « select line 2 » serait
     * vrai pour les scripts et invisible à l'écran. */
    if (g_host && g_host->selection_changed)
        g_host->selection_changed(g_sel_field, g_sel_start, g_sel_len);
}


int hc_get_selection(Object **field, int *start, int *len)
{
    if (field) *field = g_sel_field;
    if (start) *start = g_sel_start;
    if (len)   *len   = g_sel_len;
    return g_sel_field != NULL;
}

/* Texte sélectionné, ou chaîne vide. Écrit dans le tampon fourni. */
static void selection_text(char *out, int outlen)
{
    out[0] = '\0';
    if (!g_sel_field) return;
    const char *t = hc_field_text(g_sel_field);
    int n = (int)strlen(t);
    int s = g_sel_start, l = g_sel_len;
    if (s > n) return;
    if (s + l > n) l = n - s;
    if (l <= 0) return;
    snprintf(out, outlen, "%.*s", l, t + s);
}

/* pile de navigation : push cd / pop cd */
#define NAVSTACK_MAX 64
static Object *g_navstack[NAVSTACK_MAX];
static int     g_navtop = 0;

/* Force l'identifiant d'un objet relu depuis un fichier, et garde le
 * compteur au-dessus pour ne jamais réattribuer un id existant. */
int hc_entier_lu(const char *s, int mini, int maxi, int defaut, int *lu)
{
    if (lu) *lu = 0;
    if (!s) return defaut;
    /* strtod lit comme le reste du langage — « 1e3 » vaut mille, pas un — et
     * son endptr dit ce qu'aucune variante d'atoi ne sait dire : si QUELQUE
     * CHOSE a été lu. « abc » rend donc le défaut de l'appelant, et non zéro.
     *
     * Le test porte sur le DOUBLE, avant toute conversion vers int : comparer
     * après coup ne sert à rien, le débordement a déjà eu lieu et il est
     * indéfini. NaN échoue les deux comparaisons, donc tombe sur le défaut. */
    /* strtod lit aussi l'HEXADÉCIMAL, « 0x10 » valant seize. HyperTalk n'a pas
     * de littéral hexadécimal : « put 0x10 » n'y vaut pas seize, et le lecteur
     * d'entiers ne doit pas connaître un dialecte que le langage ignore. */
    {
        const char *t = s;
        while (*t == ' ' || *t == '\t') t++;
        if (*t == '+' || *t == '-') t++;
        if (t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) return defaut;
    }

    char *fin = NULL;
    double d = strtod(s, &fin);
    if (fin == s) return defaut;

    /* ET CE QUI RESTE DERRIÈRE COMPTE AUSSI.
     *
     * « 42patate » rendait 42, comme atoi. C'était garder le pire d'atoi en
     * croyant s'en débarrasser, et cela contredisait hct_est_nombre, dont le
     * contrat est précisément que TOUTE la chaîne soit numérique. Conséquences
     * directes : « set the width of field 1 to "100patate" » passait pour
     * cent, et « id 42xyz » dans un .stack pour l'identifiant 42.
     *
     * Les blancs de fin restent tolérés : ils viennent du fichier ou d'une
     * concaténation, pas de l'intention de l'auteur. « 42.9 » reste accepté et
     * tronqué — c'est une conversion du langage, pas un déchet. */
    while (*fin == ' ' || *fin == '\t' || *fin == '\n' || *fin == '\r') fin++;
    if (*fin) return defaut;

    if (!(d >= (double)mini && d <= (double)maxi)) return defaut;
    if (lu) *lu = 1;
    return (int)d;
}

int hc_entier(const char *s, int mini, int maxi, int defaut)
{
    return hc_entier_lu(s, mini, maxi, defaut, NULL);
}

int hc_coord(const char *s, int defaut)
{
    return hc_entier(s, -HC_COORD_MAX, HC_COORD_MAX, defaut);
}

/* Le nombre qui COMMENCE ICI et s'arrête à la virgule ou à la fin.
 *
 * hc_coord exige que toute la chaîne soit un nombre — c'est tout l'intérêt,
 * « 100patate » n'est pas cent — et les listes « x,y » ou « g,h,d,b » lui
 * donnaient le reste de la liste à digérer, virgule comprise. On isole donc le
 * champ. Un seul endroit, parce qu'il y a quatre appelants et qu'ils le
 * faisaient chacun à leur façon. */
static int coord_champ(const char *s, int defaut)
{
    if (!s) return defaut;
    const char *fin = s;
    while (*fin && *fin != ',') fin++;
    char champ[64];
    size_t l = (size_t)(fin - s);
    if (l >= sizeof champ) l = sizeof champ - 1;
    memcpy(champ, s, l); champ[l] = '\0';
    return hc_coord(champ, defaut);
}

/* L'ENTIER QUI COMMENCE ICI, ET QUI S'ARRÊTE AU PREMIER BLANC.
 *
 * hc_entier exige que TOUTE la chaîne soit un nombre — c'est tout son intérêt,
 * « 100patate » n'est pas cent. Mais un fichier ou une commande met souvent un
 * nombre AU MILIEU d'une ligne, suivi d'autre chose :
 *
 *     iconres 20554 "Terminator"
 *     card "Une" background "Fond" backgroundid 7
 *
 * Donner « 20554 "Terminator" » à hc_entier, c'est lui donner une chaîne qui
 * n'est pas un nombre : il rend le défaut, et l'appelant croit avoir lu.
 *
 * La famille est connue : coord_champ existe déjà pour les listes séparées par
 * des virgules, et quatre lecteurs y étaient déjà passés — parse_ints,
 * « drag from », « click at », la taille d'une plage de style. L'en-tête d'une
 * icône était le cinquième, et personne ne l'a vu : il n'y avait AUCUN harnais
 * pour les icônes. Toutes les icônes d'une pile relue prenaient le numéro 0,
 * et comme hc_icon_add remplace l'entrée de même numéro, quatre icônes n'en
 * faisaient plus qu'une — « aucune icône » pour tous les boutons.
 *
 * Deux lecteurs plutôt qu'un seul permissif : celui qui veut toute la chaîne
 * le dit, celui qui lit un champ le dit aussi. C'est à l'appel qu'on sait
 * lequel on veut, pas dans la conversion. */
int hc_entier_tete(const char *s, int mini, int maxi, int defaut)
{
    if (!s) return defaut;
    while (*s == ' ' || *s == '\t') s++;
    const char *fin = s;
    while (*fin && *fin != ' ' && *fin != '\t' &&
           *fin != '\n' && *fin != '\r') fin++;
    char champ[64];
    size_t l = (size_t)(fin - s);
    if (l >= sizeof champ) return defaut;   /* trop long pour être un entier */
    memcpy(champ, s, l); champ[l] = '\0';
    return hc_entier(champ, mini, maxi, defaut);
}

/* La borne du LECTEUR est celle du POSEUR, à un cran près et pour cause :
 * hc_set_id refuse « id >= HC_ID_MAX », donc aucun objet ne peut porter
 * HC_ID_MAX. hc_id l'acceptait quand même et rendait 1000000000 sur un
 * fichier qui portait cette valeur — un identifiant déclaré valide que rien
 * ne pouvait jamais détenir. Les deux disent maintenant la même chose. */
int hc_id(const char *s)   { return hc_entier(s, 1, HC_ID_MAX - 1, 0); }
int hc_rang(const char *s) { return hc_entier(s, 1, HC_ID_MAX, 0); }

static int id_pris_par_un_autre(Object *pile, int id, Object *moi);

void hc_set_id(Object *o, int id)
{
    /* L'IDENTIFIANT EST BORNÉ, ET LE COMPTEUR AVEC.
     *
     * Un .stack portant « id 2147483647 » faisait passer g_next_id à
     * INT_MIN — débordement signé, comportement indéfini — et la carte créée
     * ensuite recevait un identifiant négatif. Le fichier n'avait pas besoin
     * d'être malveillant pour cela : un compteur emballé ailleurs suffit.
     *
     * Au-delà de la borne, on REFUSE l'identifiant plutôt que de l'écrêter :
     * deux objets ramenés à la même valeur se retrouveraient homonymes, ce qui
     * est pire que de laisser celui-ci prendre un identifiant neuf. */
    /* La borne est celle du COMPTEUR, pas seulement celle de l'identifiant.
     *
     * « id > HC_ID_MAX » acceptait exactement HC_ID_MAX, et g_next_id passait
     * alors à HC_ID_MAX + 1 : l'objet créé ensuite recevait un identifiant que
     * ce même lecteur refuse. Le trou était juste à la frontière, là où
     * personne ne regarde — le harnais précédent essayait 2147483647, très
     * au-delà. C'est g_next_id, et non id, qui doit tenir dans les bornes. */
    if (!o || id <= 0 || id >= HC_ID_MAX) return;

    /* LES BORNES NE SUFFISENT PAS : IL FAUT AUSSI L'UNICITÉ.
     *
     * Cette fonction vérifiait que l'identifiant tient dans les bornes, et
     * s'arrêtait là. Un .stack portant deux fois « id 4 » chargeait donc deux
     * objets homonymes. Mesuré, et le résultat est pire qu'un doublon :
     *
     *   card field id 4              ->  (vide)
     *   the name of card field id 4  ->  (vide)
     *
     * Les DEUX champs deviennent inatteignables — pas seulement le seconde.
     * Et hc_save réécrit « id 4 » deux fois, si bien que le défaut survit au
     * rechargement : le fichier se transmet sa propre corruption.
     *
     * Le seul appelant est le lecteur de fichier, donc un doublon ne peut
     * venir que d'un .stack — édité à la main, fusionné, ou abîmé. C'est
     * précisément le cas où il faut le dire plutôt que de le subir.
     *
     * ON REFUSE, ET L'OBJET GARDE l'identifiant neuf que sa création lui a
     * donné. Il reste donc atteignable, sous un autre numéro. L'écraser à
     * l'identique aurait rendu les deux objets muets ; le renuméroter en
     * silence aurait fait mentir un « card id N » écrit dans un script. On
     * nomme donc l'objet et les deux numéros, pour que la pile soit
     * réparable. */
    Object *pile = owning_stack(o);
    if (pile && id_pris_par_un_autre(pile, id, o)) {
        char d[HC_NOM_MAX];
        hc_describe(o, d, sizeof d);
        emit(HC_ERR, "   !! identifiant %d déjà pris dans cette pile : "
                     "%s garde %d", id, d, o->id);
        return;
    }

    o->id = id;
    if (id >= g_next_id) g_next_id = id + 1;
}

/* ---- numérotation d'une part : voir hc_core.h pour la distinction ----
 * Ces deux fonctions sont la SEULE définition du rang. Le dialogue Infos les
 * recalculait de son côté, à partir de l'index brut dans parts[] : un champ
 * posé après cinq boutons s'y annonçait « Field number: 6 », et le script
 * « card field 6 » écrit sur cette foi ne désignait rien. */
int hc_object_number(Object *o)
{
    if (!o || !o->owner) return 0;
    int n = 0;
    for (int i = 0; i < o->owner->nparts; i++) {
        Object *p = o->owner->parts[i];
        if (p->type != o->type) continue;
        n++;
        if (p == o) return n;
    }
    return 0;
}

int hc_part_number(Object *o)
{
    if (!o || !o->owner) return 0;
    if (o->type != OBJ_BUTTON && o->type != OBJ_FIELD) return 0;
    int n = 0;
    for (int i = 0; i < o->owner->nparts; i++) {
        Object *p = o->owner->parts[i];
        if (p->type != OBJ_BUTTON && p->type != OBJ_FIELD) continue;
        n++;
        if (p == o) return n;
    }
    return 0;
}

/* DÉPLACER UNE PART DANS LA LISTE DE SON PROPRIÉTAIRE.
 *
 * Le rang d'une part n'est pas qu'un numéro : c'est l'ORDRE DE SUPERPOSITION
 * — la part de rang 1 est dessous, la dernière est dessus — et c'est aussi
 * l'ordre de tabulation. HyperCard le change par « Bring Closer » et « Send
 * Farther », et par « set the partNumber ». Nous ne savions faire ni l'un ni
 * l'autre : un bouton posé par erreur sous un champ y restait pour toujours.
 *
 * ON ÉCRÊTE PLUTÔT QUE DE REFUSER, contrairement à `family`. Les deux cas
 * n'ont rien à voir : une famille hors bornes rangerait le bouton dans un
 * groupe qu'il n'a pas choisi, avec des frères inventés, alors qu'un rang
 * hors bornes n'a qu'une lecture possible — le premier ou le dernier. Et
 * « set the partNumber to 999 » pour mettre au-dessus de tout est l'idiome
 * courant : le refuser casserait ce qu'on vient d'ajouter.
 *
 * ON DÉPLACE PAR ROTATION, et non par échange avec l'occupant du rang visé :
 * échanger bousculerait un troisième objet qui n'a rien demandé. La rotation
 * conserve l'ordre relatif de tous les autres, ce qui est le seul
 * comportement qu'un « amener au premier plan » puisse avoir.
 *
 * Rend 1 si la part a bougé ou y était déjà, 0 si l'objet n'est pas une part
 * ou n'a pas de propriétaire. */
int hc_set_part_number(Object *o, int rang)
{
    if (!o || (o->type != OBJ_BUTTON && o->type != OBJ_FIELD)) return 0;
    Object *pr = o->owner;
    if (!pr) return 0;

    /* Première passe : combien de parts, et où est celle-ci. */
    int total = 0, depuis = -1;
    for (int i = 0; i < pr->nparts; i++) {
        Object *p = pr->parts[i];
        if (p->type != OBJ_BUTTON && p->type != OBJ_FIELD) continue;
        total++;
        if (p == o) depuis = i;
    }
    if (depuis < 0 || total == 0) return 0;

    if (rang < 1)     rang = 1;
    if (rang > total) rang = total;

    /* Seconde passe : l'index de celle qui occupe le rang visé. On ne peut
     * pas le calculer dans la première — il faut connaître `total` pour
     * écrêter, et écrêter pour savoir ce qu'on cherche. */
    int n = 0, cible = -1;
    for (int i = 0; i < pr->nparts; i++) {
        Object *p = pr->parts[i];
        if (p->type != OBJ_BUTTON && p->type != OBJ_FIELD) continue;
        if (++n == rang) { cible = i; break; }
    }
    if (cible < 0 || cible == depuis) return 1;

    Object *moi = pr->parts[depuis];
    if (depuis < cible)
        for (int i = depuis; i < cible; i++) pr->parts[i] = pr->parts[i + 1];
    else
        for (int i = depuis; i > cible; i--) pr->parts[i] = pr->parts[i - 1];
    pr->parts[cible] = moi;
    return 1;
}

int hc_owner_is_bg(Object *o)
{
    return (o && o->owner && o->owner->type == OBJ_BACKGROUND) ? 1 : 0;
}

int hc_part_count(Object *owner, ObjType type)
{
    if (!owner) return 0;
    int n = 0;
    for (int i = 0; i < owner->nparts; i++)
        if (owner->parts[i]->type == type) n++;
    return n;
}

/* Les plages de style sont definies bien plus bas, mais hc_free en a besoin. */
/* Épuisement mémoire : le SEUL point de sortie.
 *
 * Il y en avait treize, chacun faisant « fprintf(stderr) ; exit(1) ». Dans une
 * application Cocoa, stderr n'est lu par personne : l'utilisateur voyait sa
 * pile disparaître sans un mot, et sans avoir pu l'enregistrer.
 *
 * On prévient donc l'hôte d'abord, pour qu'il ait une dernière chance de sauver
 * ce qui est ouvert et de l'annoncer. Puis on s'arrête : continuer après une
 * allocation manquée reviendrait à écrire dans un pointeur nul. */
void hc_memoire_epuisee(const char *quoi)
{
    static int deja = 0;
    if (!deja) {                       /* pas de récursion si l'hôte replante */
        deja = 1;
        emit(HC_ERR, "   !! mémoire épuisée : %s", quoi);
        if (g_host && g_host->panic) g_host->panic(quoi);
    }
    fprintf(stderr, "hc : mémoire épuisée (%s)\n", quoi);
    exit(1);
}

static void runs_free(struct RunList *rl);

/* Un identifiant libre DANS CETTE PILE, quand le compteur est à bout.
 *
 * L'unicité ne vaut que dans une pile — « card id 7 » se résout à l'intérieur
 * d'une pile, jamais entre elles —, donc la recherche s'y limite. Elle ne
 * tourne jamais en usage normal : il faut avoir chargé un fichier portant un
 * identifiant proche du plafond pour y arriver.
 *
 * Rend 0 si la pile est introuvable ou saturée. L'appelant garde alors le
 * plafond : deux objets homonymes valent mieux qu'un identifiant que notre
 * propre lecteur refuserait. */

static int id_pris_dans(Object *pile, int id)
{
    if (!pile) return 0;
    if (pile->id == id) return 1;
    for (int i = 0; i < pile->nparts; i++) {
        Object *couche = pile->parts[i];
        if (couche->id == id) return 1;
        for (int j = 0; j < couche->nparts; j++)
            if (couche->parts[j]->id == id) return 1;
    }
    return 0;
}

/* Comme id_pris_dans, mais SANS COMPTER `moi`.
 *
 * hc_set_id pose un identifiant sur un objet DÉJÀ attaché à sa pile : sans
 * cette exclusion, reposer sur un objet l'identifiant qu'il porte déjà le
 * ferait se déclarer en conflit avec lui-même. */
static int id_pris_par_un_autre(Object *pile, int id, Object *moi)
{
    if (!pile) return 0;
    if (pile != moi && pile->id == id) return 1;
    for (int i = 0; i < pile->nparts; i++) {
        Object *couche = pile->parts[i];
        if (couche != moi && couche->id == id) return 1;
        for (int j = 0; j < couche->nparts; j++)
            if (couche->parts[j] != moi && couche->parts[j]->id == id) return 1;
    }
    return 0;
}

/* Le plus petit identifiant libre, EN UNE SEULE PASSE.
 *
 * La version d'avant essayait 1, puis parcourait toute la pile ; puis 2, puis
 * reparcourait toute la pile. Sur une pile dont les petits numéros sont
 * occupés, cela fait N essais × N objets. Mesuré, avec le compteur épuisé :
 *
 *      500 objets   0,04 ms par création
 *     1000 objets   0,2  ms
 *     2000 objets   1,1  ms
 *     4000 objets   4,3  ms      — ×4 quand N double, donc bien du N²
 *
 * À 10 000 objets on est à ~27 ms par objet, à 50 000 l'application se fige.
 * Aucun fichier malveillant n'est nécessaire : une pile assez grosse et un
 * compteur épuisé suffisent.
 *
 * On relève donc les numéros occupés, on les trie, et on prend le premier
 * trou. Une allocation, un tri, une passe — et la même réponse.
 *
 * Si l'allocation échoue on retombe sur le parcours naïf plutôt que de
 * renoncer : lent vaut mieux que faux, et c'est le seul cas où il sert. */
static int cmp_id(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int id_libre_dans(Object *pile)
{
    if (!pile) return 0;

    int n = 1;                                   /* la pile elle-même */
    for (int i = 0; i < pile->nparts; i++)
        n += 1 + pile->parts[i]->nparts;

    int *pris = malloc((size_t)n * sizeof *pris);
    if (!pris) {
        for (int id = 1; id < HC_ID_MAX; id++)
            if (!id_pris_dans(pile, id)) return id;
        return 0;
    }

    int k = 0;
    pris[k++] = pile->id;
    for (int i = 0; i < pile->nparts; i++) {
        Object *couche = pile->parts[i];
        pris[k++] = couche->id;
        for (int j = 0; j < couche->nparts; j++)
            pris[k++] = couche->parts[j]->id;
    }
    qsort(pris, (size_t)k, sizeof *pris, cmp_id);

    int attendu = 1;
    for (int i = 0; i < k; i++) {
        if (pris[i] < attendu) continue;         /* doublon ou numéro nul */
        if (pris[i] > attendu) break;            /* le trou est ici */
        attendu = pris[i] + 1;
    }
    free(pris);
    return attendu < HC_ID_MAX ? attendu : 0;
}

/* UN IDENTIFIANT NEUF, ET LE SEUL ENDROIT QUI EN FABRIQUE.
 *
 * « o->id = g_next_id++ » n'avait aucune garde. Un fichier portant
 * « id 999999999 » pousse le compteur au plafond, et l'objet créé ensuite
 * reçoit un identifiant que notre PROPRE lecteur refuse au rechargement :
 * hc_id() rend 0 dessus, l'objet change silencieusement de numéro, et toute
 * référence « card id N » écrite dans un script cesse de désigner quoi que
 * ce soit.
 *
 * new_object avait reçu cette garde ; trois écritures l'avaient manquée —
 * hc_paste_part, et place_layer_clone pour la couche ET chacune de ses
 * parts. Encore un chemin corrigé et son jumeau oublié. D'où cette
 * fonction : il n'y a plus qu'un seul endroit à garder.
 *
 * `pile` sert au repli. Il faut qu'elle contienne DÉJÀ l'objet en cours de
 * numérotation et ses frères, sinon deux appels de suite rendent le même
 * trou — d'où l'ordre « attacher puis numéroter » chez les appelants. */
int id_neuf(Object *pile)
{
    if (g_next_id < HC_ID_MAX) return g_next_id++;
    int libre = id_libre_dans(pile);
    return libre ? libre : HC_ID_MAX - 1;
}

/* REPRENDRE L'IDENTIFIANT D'ORIGINE, QUAND LA PLACE EST LIBRE.
 *
 * Une couche portée d'une pile à l'autre reçoit un identifiant neuf, et perd
 * donc le sien. Deux choses s'y perdent avec lui : un script qui dit
 * « bg field id 12 » ne désigne plus rien là-bas, et la pile d'accueil n'a
 * plus aucune trace de CE dont son fond est la copie — trace qui, elle,
 * survivrait à l'enregistrement, puisque les identifiants sont écrits dans
 * le .stack.
 *
 * On reprend donc l'ancien numéro quand personne ne l'occupe, et l'on garde
 * le numéro neuf sinon. Le repli n'est pas un cas rare : le compteur repart
 * à 1 dans chaque session, si bien que deux piles écrites séparément se
 * disputent les petits numéros. C'est pour cela que la reconnaissance d'un
 * fond ne peut pas REPOSER sur l'identifiant — il aide quand il est là, il
 * ne prouve rien par son absence.
 *
 * ON AVANCE LE COMPTEUR, comme hc_set_id : un identifiant repris au-dessus
 * de g_next_id serait redistribué par id_neuf quelques objets plus tard, et
 * le doublon qu'on vient d'éviter reviendrait par la porte de derrière.
 *
 * Silencieuse, contrairement à hc_set_id : celui-ci lit un FICHIER, où un
 * doublon signale une pile abîmée qu'il faut pouvoir réparer. Ici le
 * conflit est la situation normale de deux piles étrangères, et il a une
 * réponse — garder le numéro neuf — qui ne demande rien à personne. */
int id_adopte(Object *pile, Object *o, int souhaite)
{
    if (!pile || !o) return 0;
    if (souhaite <= 0 || souhaite >= HC_ID_MAX) return 0;
    if (id_pris_par_un_autre(pile, souhaite, o)) return 0;

    o->id = souhaite;
    if (souhaite >= g_next_id) g_next_id = souhaite + 1;
    return 1;
}

static Object *new_object(ObjType type, Object *owner, const char *name)
{
    Object *o = calloc(1, sizeof(Object));
    if (!o) hc_memoire_epuisee("création d'objet");
    o->type    = type;
    o->id      = id_neuf(owning_stack(owner));
    o->name    = dupstr(name);
    o->owner   = owner;
    o->visible = 1;
    o->showname = 1;   /* le nom s'affiche par défaut */
    o->enabled  = 1;   /* et le bouton est actif */
    o->shared_hilite = 1;  /* allumage partagé entre cartes du même fond */
    return o;
}

void add_part(Object *parent, Object *child)
{
    if (parent->nparts == parent->capparts) {
        int cap = parent->capparts ? parent->capparts * 2 : 4;
        Object **p = realloc(parent->parts, (size_t)cap * sizeof(Object *));
        if (!p) hc_memoire_epuisee("liste des parties d'une couche");
        parent->parts = p;
        parent->capparts = cap;
    }
    parent->parts[parent->nparts++] = child;
}

Object *hc_new_stack(const char *name)
{
    Object *o = new_object(OBJ_STACK, NULL, name);
    o->w = 512; o->h = 342;   /* taille de carte par défaut (comme le Mac classique) */
    return o;
}

Object *hc_new_background(Object *stack, const char *name)
{
    Object *o = new_object(OBJ_BACKGROUND, stack, name);
    add_part(stack, o);
    return o;
}

Object *hc_new_card(Object *stack, Object *bg, const char *name)
{
    Object *o = new_object(OBJ_CARD, stack, name);
    o->bg = bg;
    add_part(stack, o);
    return o;
}

Object *hc_new_button(Object *owner, const char *name)
{
    Object *o = new_object(OBJ_BUTTON, owner, name);
    o->x = 20; o->y = 20; o->w = 120; o->h = 24;   /* défaut HyperCard-ish */
    add_part(owner, o);
    return o;
}

Object *hc_new_field(Object *owner, const char *name)
{
    Object *o = new_object(OBJ_FIELD, owner, name);
    o->contents = dupstr("");
    o->x = 20; o->y = 60; o->w = 200; o->h = 100;
    add_part(owner, o);
    return o;
}


/* ═══ L'HISTORIQUE DE NAVIGATION ═══════════════════════════════════════
 *
 * HyperCard retient les cartes visitées : « go back » retrace les pas, « the
 * recent cards » les énumère, et le menu Go en tire ses articles Back et
 * Recent. Rien de cela n'existait ici, et le commentaire du menu Go le disait
 * franchement : « Back, Home et Recent manquent faute d'historique de
 * navigation dans le noyau ».
 *
 * Une pile de cartes, la plus récemment visitée au sommet.
 *
 * Elle est alimentée à l'ARRIVÉE sur une carte, c'est-à-dire à l'envoi
 * d'openCard. C'est le seul instant où l'on est sûr qu'une navigation a eu
 * lieu, et il est UNIQUE : hc_send_systeme. Les affectations directes de
 * g_current_card ne comptent pas — il y en a vingt-deux, et la plupart sont
 * des allers-retours d'une ligne, le temps de lire le texte d'un champ de
 * fond. Les compter donnerait un historique de faux mouvements.
 *
 * Deux fois la même carte de suite ne fait qu'un pas : revenir là où l'on est
 * n'est pas un déplacement. */
#define HC_HISTO_MAX 64
static Object *g_histo[HC_HISTO_MAX];
static int     g_nhisto = 0;
static int     g_histo_gele = 0;   /* « go back » ne s'inscrit pas lui-même */

static void histo_arrive(Object *card)
{
    if (!card || g_histo_gele) return;
    if (g_nhisto > 0 && g_histo[g_nhisto - 1] == card) return;
    if (g_nhisto == HC_HISTO_MAX) {
        memmove(g_histo, g_histo + 1, sizeof g_histo - sizeof g_histo[0]);
        g_nhisto--;
    }
    g_histo[g_nhisto++] = card;
}

/* Une carte meurt : elle sort de l'historique. Sans cela « go back » y
 * trouverait une adresse morte — exactement la faute qu'object_gone vient de
 * corriger dans l'interface, et le noyau n'y échappe pas plus qu'elle. */
static void histo_oublie(Object *card)
{
    int j = 0;
    for (int i = 0; i < g_nhisto; i++)
        if (g_histo[i] != card) g_histo[j++] = g_histo[i];
    g_nhisto = j;
}

/* La carte d'où l'on vient. Le sommet est la carte courante : on la dépile,
 * et la destination est le nouveau sommet. Rendre NULL veut dire qu'il n'y a
 * nulle part où revenir. */
static Object *histo_recule(void)
{
    if (g_nhisto < 2) return NULL;
    g_nhisto--;
    return g_histo[g_nhisto - 1];
}

int hc_recent_count(void) { return g_nhisto; }

/* Rang 0 = la plus récente. L'ordre rendu est celui d'HyperCard : le menu
 * Recent montrait la dernière visitée en premier. */
Object *hc_recent_at(int i)
{
    if (i < 0 || i >= g_nhisto) return NULL;
    return g_histo[g_nhisto - 1 - i];
}

/* Les cartes visitées SANS DOUBLON, la plus récente d'abord.
 *
 * La pile brute en contient forcément : un aller-retour entre deux cartes
 * inscrit la première deux fois, et une navigation un peu longue finissait par
 * remplir le menu Recent de la même carte répétée trois ou quatre fois. C'est
 * ce que HyperCard ne montrait jamais — revisiter une carte y REMONTAIT sa
 * vignette au lieu d'en ajouter une seconde.
 *
 * Le dédoublonnage se fait ICI, à la lecture, et non dans histo_arrive : « go
 * back » retrace les pas, donc l'historique BRUT doit garder ses répétitions.
 * Deux besoins différents sur la même liste, chacun servi à sa façon.
 *
 * Coût quadratique, sur soixante-quatre entrées au plus. */
int hc_recent_distinct(Object **out, int max)
{
    int n = 0;
    for (int i = 0; i < g_nhisto && n < max; i++) {
        Object *c = hc_recent_at(i);
        if (!c) continue;
        int vu = 0;
        for (int k = 0; k < n; k++) if (out[k] == c) { vu = 1; break; }
        if (!vu) out[n++] = c;
    }
    return n;
}

/* Définie plus bas, après les globales qu'elle efface. */
static void oublie_objet_interne(Object *mort);

void hc_free(Object *o)
{
    if (!o) return;
    histo_oublie(o);
    /* LES RÉFÉRENCES INTERNES DU NOYAU, AU MÊME TITRE QUE CELLES DE L'HÔTE.
     *
     * object_gone, juste en dessous, protège remarquablement bien les
     * pointeurs de l'interface. Le noyau ne s'appliquait pas le même principe
     * à ses PROPRES références, une couche plus bas : la sélection, le
     * résultat de la dernière recherche et la pile de navigation gardaient
     * l'adresse d'un objet libéré. Trois lignes de HyperTalk suffisaient à
     * lire de la mémoire morte — mesuré sous AddressSanitizer :
     *
     *     select char 1 to 2 of card field "x"
     *     delete card field "x"
     *     put the selection          -> lecture après libération
     *
     *     find "toto"
     *     delete card field "x"
     *     put the foundField         -> lecture après libération
     *
     *     push card
     *     go next card
     *     delete card "A"
     *     pop card                   -> déréférencement d'une carte libérée
     *
     * Un seul endroit s'en charge, et c'est celui-ci : hc_free est le seul
     * point où un objet meurt, et il descend lui-même dans ses enfants, donc
     * un champ emporté par la suppression de sa carte est oublié lui aussi.
     * Recenser les appelants un par un, c'est en oublier un. */
    oublie_objet_interne(o);
    /* Prévenir l'hôte AVANT de libérer quoi que ce soit. C'est le seul
     * endroit où un objet meurt, donc le seul où le dire une fois pour
     * toutes — recenser les appelants un par un, c'est en oublier un. */
    if (g_host && g_host->object_gone) g_host->object_gone(o);
    for (int i = 0; i < o->nparts; i++) hc_free(o->parts[i]);
    free(o->parts);
    free(o->name);
    free(o->path);
    hc_arbre_oublie(o);          /* avant le script : les jetons y pointent */
    free(o->script);
    free(o->contents);
    free(o->style);
    free(o->textfont);
    free(o->bghilites);
    for (int i = 0; i < o->nbgtexts; i++) {
        free(o->bgtexts[i].text);
        runs_free(&o->bgtexts[i].runs);
    }
    free(o->bgtexts);
    runs_free(&o->runs);
    free(o->paint);
    /* LA TABLE D'ICÔNES DE LA PILE.
     *
     * hc_icons_free existait, était déclarée dans l'en-tête, et n'était
     * appelée NULLE PART : toute pile portant des icônes fuyait sa table
     * entière — cent vingt-huit octets par icône, plus son nom — à chaque
     * fermeture. Trouvé par le harnais « refus », le premier à charger puis
     * libérer une pile avec des icônes sous LeakSanitizer.
     *
     * Elle ne concerne que les piles ; hc_icons_free l'ignore pour le reste. */
    hc_icons_free(o);
    free(o);
}

/* Retire un objet (bouton/champ) de la couche de son propriétaire et le libère.
 * Renvoie 1 si trouvé et supprimé, 0 sinon. */
/* Définis plus bas, mais la suppression de carte en a besoin ici. */
static Object *nth_card(Object *stack, int n);
static int     card_index(Object *stack, Object *card);

/* Une suppression peut lancer du HyperTalk (deleteCard, deleteButton, ...),
 * qui peut à son tour supprimer d'autres objets. Deux protections sont donc
 * nécessaires :
 *
 *  - garder TOUTE la chaîne des suppressions en cours, et pas seulement le
 *    dernier objet, pour couper les cycles A -> B -> A ;
 *  - ne jamais libérer physiquement un objet tant qu'un message tourne ou
 *    qu'une suppression est encore sur la pile. « delete this card » depuis
 *    un bouton détache la carte tout de suite, mais le bouton et son arbre
 *    restent vivants jusqu'au retour de mouseUp.
 *
 * Les objets différés sont déjà détachés de leur propriétaire. Une pénurie
 * lors de l'allocation de la petite cellule de file provoque donc au pire une
 * fuite, préférable à une libération prématurée et à un use-after-free. */
typedef struct SuppressionActive {
    Object *objet;
    struct SuppressionActive *precedent;
} SuppressionActive;

static SuppressionActive *g_suppression_active = NULL;

typedef struct LiberationDifferee {
    Object *objet;
    struct LiberationDifferee *suivante;
} LiberationDifferee;

static LiberationDifferee *g_liberations_differees = NULL;

static int suppression_active(Object *o)
{
    for (SuppressionActive *s = g_suppression_active; s; s = s->precedent)
        if (s->objet == o) return 1;
    return 0;
}

/* `racine` contient-elle encore `cherche` ? Les objets de cette routine ne
 * sont jamais encore libérés ; elle ne sert qu'à éviter de mettre dans la
 * file à la fois un parent ET un enfant qu'il possède toujours. */
static int objet_contient(Object *racine, Object *cherche)
{
    if (!racine) return 0;
    if (racine == cherche) return 1;
    for (int i = 0; i < racine->nparts; i++)
        if (objet_contient(racine->parts[i], cherche)) return 1;
    return 0;
}

static void libere_ou_differe(Object *o)
{
    if (!o) return;
    if (g_depth == 0 && !g_suppression_active) {
        hc_free(o);
        return;
    }

    /* Un parent déjà en attente emportera cet objet avec lui. */
    for (LiberationDifferee *d = g_liberations_differees; d; d = d->suivante)
        if (objet_contient(d->objet, o)) return;

    /* Si l'on met maintenant un parent en attente, retirer de la file les
     * descendants qu'il possède encore : hc_free(parent) les libérera. */
    LiberationDifferee **pp = &g_liberations_differees;
    while (*pp) {
        LiberationDifferee *d = *pp;
        if (objet_contient(o, d->objet)) {
            *pp = d->suivante;
            free(d);
            continue;
        }
        pp = &d->suivante;
    }

    LiberationDifferee *d = malloc(sizeof *d);
    if (!d) {
        emit(HC_ERR, "   !! mémoire insuffisante : objet détaché non libéré");
        return;
    }
    d->objet = o;
    d->suivante = g_liberations_differees;
    g_liberations_differees = d;
}

static void libere_differees(void)
{
    if (g_depth != 0 || g_suppression_active) return;
    while (g_liberations_differees) {
        LiberationDifferee *d = g_liberations_differees;
        g_liberations_differees = d->suivante;
        Object *o = d->objet;
        free(d);
        hc_free(o);
    }
}

static int objet_dans_parent(Object *parent, Object *o)
{
    if (!parent || !o) return -1;
    for (int i = 0; i < parent->nparts; i++)
        if (parent->parts[i] == o) return i;
    return -1;
}

static void retire_part_index(Object *parent, int idx)
{
    for (int j = idx; j < parent->nparts - 1; j++)
        parent->parts[j] = parent->parts[j + 1];
    parent->nparts--;
}

/* Supprime une carte de sa pile.
 *
 * Refuse la DERNIÈRE carte : HyperCard faisait de même, et une pile sans carte
 * n'a pas de sens — le chargement en fabriquerait une d'office, donnant
 * l'impression que la suppression n'a rien fait.
 *
 * deleteCard part AVANT la disparition, mais son gestionnaire est libre de
 * modifier la pile. On recalcule donc présence, rang et nombre de cartes
 * APRÈS le callback au lieu de réutiliser des valeurs devenues périmées. */
int hc_delete_card(Object *card)
{
    if (!card || card->type != OBJ_CARD || !card->owner) return 0;
    if (suppression_active(card)) return 0;

    Object *stack = card->owner;
    int total = 0;
    for (int i = 0; i < stack->nparts; i++)
        if (stack->parts[i]->type == OBJ_CARD) total++;
    if (total <= 1) return 0;
    if (card_index(stack, card) < 0) return 0;

    /* « Can't Delete Card » : le verrou de l'Info carte.
     *
     * On refuse AVANT d'envoyer deleteCard — le message annonce une
     * suppression qui va avoir lieu, et le faire partir pour rien tromperait
     * un gestionnaire qui s'en sert pour ranger ses affaires. */
    if (card->cant_delete) { set_result("Can't delete card"); return 0; }

    /* Le verrou du FOND protège sa dernière carte, puisque c'est elle qui le
     * ferait disparaître. Rien d'autre ici ne supprime un fond. */
    if (card->bg && card->bg->cant_delete) {
        int restants = 0;
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i]->type == OBJ_CARD && stack->parts[i]->bg == card->bg)
                restants++;
        if (restants <= 1) {
            set_result("Can't delete background");
            return 0;
        }
    }

    SuppressionActive active = { card, g_suppression_active };
    g_suppression_active = &active;

    hc_send_systeme(card, "deleteCard");

    /* Le gestionnaire a pu supprimer une AUTRE carte. Il ne peut pas
     * supprimer celle-ci par hc_delete_card (le garde ci-dessus coupe le
     * cycle), mais il peut avoir changé le nombre et les indices. */
    int idx = card_index(stack, card);
    total = 0;
    for (int i = 0; i < stack->nparts; i++)
        if (stack->parts[i]->type == OBJ_CARD) total++;

    if (idx < 0 || total <= 1) {
        g_suppression_active = active.precedent;
        libere_differees();
        return 0;
    }

    Object *bg = card->bg;
    int pos = objet_dans_parent(stack, card);
    if (pos < 0) {
        g_suppression_active = active.precedent;
        libere_differees();
        return 0;
    }
    retire_part_index(stack, pos);

    if (g_current_card == card) {
        Object *suiv = nth_card(stack, idx);          /* celle qui a pris la place */
        if (!suiv) suiv = nth_card(stack, idx - 1);   /* c'était la dernière */
        g_current_card = suiv;
    }

    /* Si la commande vient du script d'un bouton de cette carte, le bouton,
     * sa carte et leur arbre sont encore utilisés jusqu'au retour du message. */
    libere_ou_differe(card);

    /* Le fond disparaît avec sa dernière carte. Le callback deleteBackground
     * peut toutefois recréer/changer des cartes : on revérifie donc tout
     * après lui avant de retirer le fond. */
    if (bg && bg->type == OBJ_BACKGROUND) {
        int reste = 0;
        for (int i = 0; i < stack->nparts && !reste; i++)
            if (stack->parts[i]->type == OBJ_CARD && stack->parts[i]->bg == bg)
                reste = 1;

        if (!reste && objet_dans_parent(stack, bg) >= 0 && !suppression_active(bg)) {
            SuppressionActive abg = { bg, g_suppression_active };
            g_suppression_active = &abg;
            hc_send_systeme(bg, "deleteBackground");

            reste = 0;
            for (int i = 0; i < stack->nparts && !reste; i++)
                if (stack->parts[i]->type == OBJ_CARD && stack->parts[i]->bg == bg)
                    reste = 1;

            int ibg = objet_dans_parent(stack, bg);
            if (!reste && ibg >= 0) {
                retire_part_index(stack, ibg);
                libere_ou_differe(bg);
            }
            g_suppression_active = abg.precedent;
        }
    }

    g_suppression_active = active.precedent;
    libere_differees();
    return 1;
}

int hc_text_height(Object *o)
{
    if (!o) return 16;
    if (o->textheight > 0) return o->textheight;
    int sz = o->textsize > 0 ? o->textsize : 12;
    return (sz * 4 + 1) / 3;      /* quatre tiers, arrondi comme HyperCard */
}

/* ---- allumage d'un bouton ----
 *
 * Un bouton de fond dont sharedHilite est faux range son allumage dans la
 * CARTE, pas dans lui-même : sans ce détour, cocher une case sur une carte la
 * cocherait sur toutes celles du fond.
 *
 * Une carte ne retient que les boutons qu'elle a allumés ; l'absence d'entrée
 * vaut éteint, ce qui évite d'écrire une ligne par bouton et par carte. */
static int hilite_par_carte(Object *btn)
{
    return btn && btn->type == OBJ_BUTTON && !btn->shared_hilite &&
           btn->owner && btn->owner->type == OBJ_BACKGROUND;
}

int hc_hilite_of(Object *btn, Object *card)
{
    if (!btn) return 0;
    if (!hilite_par_carte(btn)) return btn->hilite;

    if (!card) card = g_current_card;
    if (!card) return 0;
    for (int i = 0; i < card->nbghilites; i++)
        if (card->bghilites[i].button_id == btn->id)
            return card->bghilites[i].hilite;
    return 0;
}

/* Pose l'entrée directement, par identifiant de bouton.
 *
 * Le chargement s'en sert : il lit « bghilite 14 » et n'a pas l'objet sous la
 * main. Ne vérifie donc rien — c'est hc_set_hilite qui décide si la carte est
 * bien le bon dépositaire. */
void hc_set_hilite_raw(Object *card, int button_id, int on)
{
    if (!card) return;

    for (int i = 0; i < card->nbghilites; i++)
        if (card->bghilites[i].button_id == button_id) {
            card->bghilites[i].hilite = on ? 1 : 0;
            return;
        }

    if (!on) return;          /* éteint = pas d'entrée : rien à créer */

    if (card->nbghilites == card->capbghilites) {
        int cap = card->capbghilites ? card->capbghilites * 2 : 4;
        struct BgHilite *t = realloc(card->bghilites, (size_t)cap * sizeof *t);
        if (!t) return;
        card->bghilites = t;
        card->capbghilites = cap;
    }
    card->bghilites[card->nbghilites].button_id = button_id;
    card->bghilites[card->nbghilites].hilite    = 1;
    card->nbghilites++;
}

int hc_hilite_par_carte(Object *btn) { return hilite_par_carte(btn); }

const char *hc_stack_path(Object *stack)
{
    return (stack && stack->type == OBJ_STACK) ? stack->path : NULL;
}

void hc_set_stack_path(Object *stack, const char *path)
{
    if (!stack || stack->type != OBJ_STACK) return;
    char *neuf = path ? dupstr(path) : NULL;
    if (path && !neuf) hc_memoire_epuisee("chemin d'une pile");
    free(stack->path);
    stack->path = neuf;
}

/* LES BOUTONS RADIO : allumer l'un éteint ses frères de famille.
 *
 * Toute la mécanique radio d'HyperCard tient là. Elle n'appartient pas à
 * l'interface : un script qui fait « set the hilite of button "Oui" to true »
 * doit éteindre « Non » exactement comme un clic le ferait, sinon les deux
 * chemins divergent et l'un des deux ment.
 *
 * LA FAMILLE NE TRAVERSE PAS LES COUCHES. Deux boutons de familles égales,
 * l'un sur la carte et l'autre sur le fond, ne s'excluent pas : ce sont deux
 * groupes distincts, comme dans HyperCard. On compare donc le propriétaire,
 * pas seulement le numéro.
 *
 * Famille 0 n'exclut rien — c'est l'absence de famille, pas la famille
 * numéro zéro. Éteindre n'exclut rien non plus : décocher le dernier bouton
 * d'un groupe laisse le groupe vide, ce qui est un état légitime. */
/* Éteint l'entrée par carte d'un bouton sur TOUTES les cartes d'un fond.
 *
 * Voir eteint_la_famille : un bouton allumé PARTOUT ne peut pas laisser un
 * frère allumé sur une seule carte. */
static void eteint_partout_dans_le_fond(Object *fond, int button_id)
{
    Object *pile = fond ? fond->owner : NULL;
    if (!pile) return;
    for (int i = 0; i < pile->nparts; i++) {
        Object *c = pile->parts[i];
        if (c->type == OBJ_CARD && c->bg == fond)
            hc_set_hilite_raw(c, button_id, 0);
    }
}

/* LA PORTÉE DE L'EXTINCTION SUIT CELLE DE L'ALLUMAGE.
 *
 * Un bouton de fond a deux façons de s'allumer, et c'est tout le problème :
 * sharedHilite VRAI l'allume sur TOUTES les cartes du fond à la fois,
 * sharedHilite FAUX lui donne un état par carte, rangé dans la carte.
 *
 * L'extinction, elle, ne connaissait qu'une carte. Mesuré, avec R1 partagé et
 * R2 par carte, tous deux de famille 3 :
 *
 *     R2 allumé sur la carte B
 *     on se place sur A, on allume R1
 *     -> sur A : R1=1 R2=0        (juste)
 *     -> sur B : R1=1 R2=1        (DEUX de la famille 3 allumés)
 *
 * R1 s'allume partout puisqu'il est partagé ; R2 n'est éteint que sur la
 * carte où l'on se trouve. La famille est violée sur toutes les autres, et
 * l'utilisateur ne le voit qu'en y allant.
 *
 * LA RÈGLE : si le bouton qu'on allume s'allume PARTOUT, ses frères à état
 * par carte doivent s'éteindre partout aussi. S'il ne s'allume que sur une
 * carte, éteindre sur cette carte suffit — et c'est le cas courant, qui ne
 * coûte rien de plus qu'avant. */
static void eteint_la_famille(Object *btn, Object *card)
{
    if (!btn || btn->family <= 0 || !btn->owner) return;
    Object *couche = btn->owner;
    /* L'allumage de btn porte-t-il sur toutes les cartes du fond ? */
    int btn_partout = !hilite_par_carte(btn) && couche->type == OBJ_BACKGROUND;
    for (int i = 0; i < couche->nparts; i++) {
        Object *f = couche->parts[i];
        if (f == btn || f->type != OBJ_BUTTON) continue;
        if (f->family != btn->family) continue;
        if (!hilite_par_carte(f)) f->hilite = 0;
        else if (btn_partout)     eteint_partout_dans_le_fond(couche, f->id);
        else hc_set_hilite_raw(card ? card : g_current_card, f->id, 0);
    }
}

/* POSER LA FAMILLE, ET LA SEULE PORTE POUR LE FAIRE.
 *
 * Le dialogue Infos bouton écrivait o->family en direct dans sa première
 * version. Entrer dans une famille en étant allumé doit éteindre les autres,
 * sinon le groupe se retrouve avec deux boutons allumés — un état qu'aucun
 * clic ne peut produire, et qui n'apparaît qu'à la fermeture du panneau.
 * L'écriture par script passait déjà par cette règle ; le panneau ne devait
 * pas en avoir une seconde.
 *
 * Rend 0 et ne touche à rien hors de 0..15, pour que l'appelant puisse le
 * dire. On n'écrête pas : ramener 20 à 15 rangerait le bouton avec des frères
 * qu'il n'a pas choisis. */
int hc_set_family(Object *btn, int famille)
{
    if (!btn || btn->type != OBJ_BUTTON) return 0;
    if (famille < 0 || famille > 15) return 0;
    btn->family = famille;
    if (famille <= 0) return 1;

    /* ON RÉCONCILIE SUR CHAQUE CARTE OÙ CE BOUTON EST ALLUMÉ.
     *
     * Cette ligne ne regardait que la carte courante :
     *
     *     if (famille > 0 && hc_hilite_of(btn, NULL)) eteint_la_famille(btn, NULL);
     *
     * Or un bouton de FOND à sharedHilite faux a un état PAR CARTE : il peut
     * être éteint ici et allumé sur cinq autres. Lui donner une famille le
     * faisait alors entrer dans un groupe déjà pourvu, sur chacune de ces
     * cinq cartes, sans que rien ne soit éteint — l'état à deux allumés
     * qu'aucun clic ne peut produire, et qui n'apparaît qu'en allant voir.
     *
     * Le parcours ne coûte que pour ce cas-là : un bouton de carte, ou un
     * bouton de fond partagé, n'a qu'un seul état et sort par la branche du
     * dessous. */
    if (hilite_par_carte(btn)) {
        Object *fond = btn->owner;
        Object *pile = fond ? fond->owner : NULL;
        for (int i = 0; pile && i < pile->nparts; i++) {
            Object *c = pile->parts[i];
            if (c->type != OBJ_CARD || c->bg != fond) continue;
            if (hc_hilite_of(btn, c)) eteint_la_famille(btn, c);
        }
        return 1;
    }

    if (hc_hilite_of(btn, NULL)) eteint_la_famille(btn, NULL);
    return 1;
}

void hc_set_hilite(Object *btn, Object *card, int on)
{
    if (!btn) return;
    if (on) eteint_la_famille(btn, card);
    if (!hilite_par_carte(btn)) { btn->hilite = on ? 1 : 0; return; }

    if (!card) card = g_current_card;
    hc_set_hilite_raw(card, btn->id, on);
}

/* Un bouton de style radio est-il de CE style ? Les scripts écrivent les deux
 * casses, et l'ancien code de la vue comparait les deux à chaque endroit. */
static void eteint_la_famille(Object *btn, Object *card);

static int est_radio(Object *o)
{
    return o && o->type == OBJ_BUTTON && o->style &&
           (strcmp(o->style, "radioButton") == 0 || strcmp(o->style, "radiobutton") == 0);
}

static int est_case(Object *o)
{
    return o && o->type == OBJ_BUTTON && o->style &&
           (strcmp(o->style, "checkBox") == 0 || strcmp(o->style, "checkbox") == 0);
}

/* UN SEUL MÉCANISME D'EXCLUSION : LA FAMILLE. FAMILLE 0 NE GROUPE RIEN.
 *
 * Il y en avait deux. Le premier, antérieur aux familles, éteignait AU CLIC
 * tous les autres boutons de style radio de la carte et du fond — une
 * commodité que nous avions ajoutée, et qu'HyperCard n'a jamais eue. Le
 * second, par numéro de famille, est celui d'HyperCard.
 *
 * DEUX MÉCANISMES, C'ÉTAIT DEUX RÉPONSES À LA MÊME QUESTION, et elles se
 * contredisaient. Mesuré, sur deux radios SANS famille :
 *
 *     par SCRIPT (set the hilite) :  R1=true   R2=true
 *     par CLIC                    :  R1=false  R2=true
 *
 * Le script laissait les deux allumés, le clic n'en gardait qu'un. Le même
 * état de pile selon la porte empruntée.
 *
 * LE CHOIX EST CELUI D'HYPERCARD, et il est de l'auteure du projet :
 * famille 0 est l'ABSENCE de famille, pas un groupe. C'est précisément
 * pourquoi « the family of » a été inventé en 2.0 — avant elle, un script
 * devait éteindre ses voisins lui-même. La commodité du clic disparaît donc,
 * et avec elle la divergence : il ne reste qu'une règle, dans hc_set_hilite,
 * que le clic et le script traversent l'un comme l'autre. Ils ne peuvent
 * plus diverger parce qu'il n'y a plus qu'un chemin.
 *
 * CE QUE ÇA CHANGE POUR LES PILES EXISTANTES, et il faut le dire net : des
 * boutons radio sans famille cessent de s'éteindre mutuellement. Il faut
 * leur donner une famille — par le panneau Infos bouton, ou par
 * « set the family of button "X" to 1 ». C'est le prix du choix, il a été
 * pesé, et il rend la pile conforme à ce qu'un HyperCard d'époque en aurait
 * fait. */

/* LA FIN AUTOMATIQUE D'UN CLIC, ET SUR QUELLE CARTE ELLE S'APPLIQUE.
 *
 * Ce que HyperCard fait tout seul quand mouseUp a rendu la main : la case à
 * cocher bascule, le radio s'allume et éteint ses voisins, le bouton ordinaire
 * s'éteint.
 *
 * `carte_cliquee` est la carte SUR LAQUELLE LE CLIC A EU LIEU, retenue avant
 * l'envoi. La vue lisait la carte COURANTE après l'envoi, et un gestionnaire
 * n'a rien d'exceptionnel à changer de carte :
 *
 *     on mouseUp
 *       go next card
 *     end mouseUp
 *
 * Pour un bouton de fond à sharedHilite false, l'allumage se range dans la
 * table de la carte : l'extinction partait donc dans celle de la carte
 * D'ARRIVÉE. Avec « go to card 1 of stack "B" », on inscrivait dans une carte
 * de B l'identifiant d'un bouton de A — un état de bouton écrit sur une autre
 * pile. Le pointeur du bouton avait bien été sauvegardé avant le script ; son
 * contexte de carte ne l'avait pas été.
 *
 * Les deux doivent être encore vivants. Si la carte cliquée a disparu — le
 * gestionnaire l'a supprimée — il n'y a plus de table où écrire et l'on ne
 * retombe SURTOUT PAS sur la carte courante : c'est très exactement le défaut
 * qu'on corrige. Un allumage qui ne dépend pas de la carte, lui, s'applique
 * quand même : il vit sur le bouton. */
void hc_fin_de_clic(Object *btn, Object *carte_cliquee)
{
    if (!btn || !hc_object_is_live(btn) || btn->type != OBJ_BUTTON) return;

    Object *carte = (carte_cliquee && hc_object_is_live(carte_cliquee))
                    ? carte_cliquee : NULL;
    if (!carte && hilite_par_carte(btn)) return;

    if (est_case(btn))        hc_set_hilite(btn, carte, !hc_hilite_of(btn, carte));
    /* Un seul appel : hc_set_hilite éteint déjà la famille. Il y avait ici un
     * second appel, radio_exclusif, qui portait l'ancienne règle sans
     * famille — c'est lui qui faisait diverger le clic et le script. */
    else if (est_radio(btn)) hc_set_hilite(btn, carte, 1);
    else if (btn->autohilite) hc_set_hilite(btn, carte, 0);
}

int hc_card_count(Object *stack){
    if (!stack) return 0;
    int n = 0;
    for (int i = 0; i < stack->nparts; i++)
        if (stack->parts[i]->type == OBJ_CARD) n++;
    return n;
}

/* UNE PILE N'A-T-ELLE VRAIMENT JAMAIS SERVI ?
 *
 * L'interface remplace la pile « Sans titre » du démarrage quand on en ouvre
 * une autre, plutôt que d'accumuler des fenêtres vides. C'est commode, et
 * c'était FAUX : le test ne comptait que les CARTES. Une pile d'une seule
 * carte où l'on avait dessiné, posé des boutons et écrit un script passait
 * donc pour vierge, et partait à hc_free sans que rien ne le dise.
 *
 * « J'ouvre une autre pile et la pile sans titre disparaît » — et avec elle
 * tout ce qu'elle contenait.
 *
 * LA QUESTION EST ICI ET PAS DANS L'INTERFACE, pour deux raisons. Elle
 * s'écrivait en DEUX exemplaires dans AppDelegate.m — « nouvelle pile » et
 * « ouvrir une pile » — qui auraient divergé au premier critère ajouté ; et
 * elle ne regarde que l'arbre des objets, donc elle se MESURE, ce que rien
 * de ce qui touche aux fenêtres ne peut faire.
 *
 * CE QUI COMPTE POUR UNE TRACE : plus d'une carte, une part quelque part,
 * de la peinture, un script. Rien d'autre n'est vérifiable sans supposer ce
 * qu'est une pile « par défaut » — et dans le doute on rend « pas vierge »,
 * parce que se tromper dans ce sens fait garder une fenêtre de trop, tandis
 * que se tromper dans l'autre efface le travail de quelqu'un. */
int hc_stack_vierge(Object *stack)
{
    if (!stack) return 0;

    const char *sc = hc_script_of(stack);
    if (sc && *sc) return 0;

    int cartes = 0;
    for (int i = 0; i < stack->nparts; i++) {
        Object *o = stack->parts[i];
        if (!o) continue;

        /* Les fonds comptent autant que les cartes : on peut avoir tout
         * dessiné sur le fond sans jamais toucher la carte. */
        if (o->type == OBJ_CARD || o->type == OBJ_BACKGROUND) {
            if (o->type == OBJ_CARD && ++cartes > 1) return 0;
            if (o->nparts > 0) return 0;
            const char *p = hc_paint_of(o);
            if (p && *p) return 0;
            const char *s2 = hc_script_of(o);
            if (s2 && *s2) return 0;
        }
    }
    return 1;
}

/* Boutons/champs : même règle que pour les cartes. La pile de gardes coupe
 * aussi les cycles A -> B -> A, que l'ancien pointeur unique laissait passer. */
int hc_delete_part(Object *o)
{
    if (!o || !o->owner) return 0;
    if (o->type != OBJ_BUTTON && o->type != OBJ_FIELD) return 0;
    if (suppression_active(o)) return 0;

    Object *parent = o->owner;
    if (objet_dans_parent(parent, o) < 0) return 0;

    SuppressionActive active = { o, g_suppression_active };
    g_suppression_active = &active;
    hc_send_systeme(o, o->type == OBJ_BUTTON ? "deleteButton" : "deleteField");

    /* Le callback a pu détacher le parent entier. Sa mémoire reste néanmoins
     * vivante grâce à libere_ou_differe tant que cette suppression est active,
     * donc on peut retirer proprement le part de son tableau avant les frees. */
    int idx = objet_dans_parent(parent, o);
    if (idx >= 0) {
        retire_part_index(parent, idx);
        libere_ou_differe(o);
    }

    g_suppression_active = active.precedent;
    libere_differees();
    return idx >= 0;
}


/* ==================== chaîne de messages ==================== */

/* Construit la chaîne de remontée depuis `target`.
 * Fidèle à HyperCard : un objet (de carte OU de fond) remonte d'abord
 * à la carte courante, puis à son fond, puis à la pile.
 */
static int build_chain(Object *target, Object *chain[], int max)
{
    int n = 0;
    if (!target || max < 1) return 0;

    chain[n++] = target;

    Object *card = NULL, *bg = NULL, *stack = NULL;

    switch (target->type) {
        case OBJ_BUTTON:
        case OBJ_FIELD:
            /* le propriétaire est soit une carte, soit un fond */
            if (target->owner && target->owner->type == OBJ_CARD) {
                card = target->owner;
            } else {
                /* objet de fond : on passe par la carte courante */
                card = g_current_card;
            }
            break;
        case OBJ_CARD:
            card = target;
            break;
        case OBJ_BACKGROUND:
            bg = target;
            break;
        case OBJ_STACK:
            stack = target;
            break;
    }

    if (card && card != target && n < max) chain[n++] = card;
    if (card && !bg) bg = card->bg;
    if (bg && bg != target && n < max) chain[n++] = bg;

    if (!stack) {
        if (card)       stack = card->owner;
        else if (bg)    stack = bg->owner;
        else if (target->owner) stack = target->owner;
    }
    if (stack && stack != target && n < max) chain[n++] = stack;

    /* Les piles EN USAGE, après la pile courante.
     *
     * « start using stack "Outils" » insère une pile dans la chaîne : ses
     * gestionnaires deviennent appelables depuis n'importe quelle pile, sans
     * qu'on ait à les y recopier. C'était le mécanisme des bibliothèques de
     * l'époque — une pile de fonctions partagées, déclarée une fois.
     *
     * Elles viennent en DERNIER, et dans l'ordre inverse de leur déclaration :
     * la plus récemment déclarée est consultée en premier, comme dans
     * HyperCard. Une pile locale l'emporte donc toujours sur une bibliothèque,
     * ce qui permet de redéfinir localement un gestionnaire partagé. */
    for (int i = g_nusing - 1; i >= 0 && n < max; i--) {
        Object *u = g_using[i];
        if (!u || u == stack || u == target) continue;   /* déjà dans la chaîne */
        chain[n++] = u;
    }

    return n;
}

/* ==================== recherche de gestionnaire ==================== */

/* Cherche `on <message>` en début de ligne dans le script.
 * Renvoie un pointeur sur le début du corps, et remplit *end avec la fin.
 */
/* isfunc : 0 cherche « on <nom> », 1 cherche « function <nom> ».
 * Les deux familles vivent dans le même script et ne se marchent pas dessus :
 * HyperCard permet à une pile d'avoir « on date » et « function date ». */
static const char *find_handler_k(const char *script, const char *message,
                                  int isfunc,
                                  const char **body_end, const char **hdr_out)
{
    if (!script) return NULL;
    const char *p = script;
    const char *kw = isfunc ? "function " : "on ";
    int kwlen = isfunc ? 9 : 3;

    while (*p) {
        const char *line = skip_spaces(p);
        if (ci_prefix(line, kw)) {
            char name[64];
            next_word(line + kwlen, name, sizeof name);
            if (ci_equal(name, message)) {
                if (hdr_out) *hdr_out = line;   /* l'en-tête, pour ses paramètres */
                /* corps = après la fin de cette ligne */
                const char *body = strchr(line, '\n');
                body = body ? body + 1 : line + strlen(line);
                /* chercher `end <message>` */
                const char *q = body;
                while (*q) {
                    const char *l2 = skip_spaces(q);
                    if (ci_prefix(l2, "end ")) {
                        char n2[64];
                        next_word(l2 + 4, n2, sizeof n2);
                        if (ci_equal(n2, message)) {
                            *body_end = q;
                            return body;
                        }
                    }
                    const char *nl = strchr(q, '\n');
                    if (!nl) break;
                    q = nl + 1;
                }
                /* pas de `end` : le corps va jusqu'au bout */
                *body_end = body + strlen(body);
                return body;
            }
        }
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    return NULL;
}

static const char *find_handler(const char *script, const char *message,
                                const char **body_end, const char **hdr_out)
{
    return find_handler_k(script, message, 0, body_end, hdr_out);
}

/* ==================== résolution de références ==================== */

/* « PART » NE TRIE PAS : il compte les boutons et les champs mêlés.
 *
 * Les trois recherches ci-dessous prenaient un ObjType et ne savaient donc
 * répondre qu'à « le troisième BOUTON » ou « le troisième CHAMP ». Le
 * résolveur, faute de mieux, rangeait « part 3 » dans le seau des champs :
 * mesuré, sur une carte « un(bouton) deux(champ) trois(bouton)
 * quatre(champ) »,
 *
 *     part 1  ->  card field "deux"      au lieu de button "un"
 *     part 2  ->  card field "quatre"    au lieu de field "deux"
 *     part 3  ->  objet introuvable
 *
 * Une MAUVAISE RÉPONSE, pas une erreur : le script recevait un objet, et le
 * mauvais. Le comptage, lui, savait déjà compter les parts mêlées — « the
 * number of parts » rendait bien 4. Une règle connue d'un seul site, que les
 * autres n'appliquaient pas ; c'est la forme qui revient le plus souvent ici.
 *
 * Le sentinelle vit donc DANS LES TROIS RECHERCHES, et non chez l'appelant :
 * corriger le rang en laissant le nom et l'identifiant au champ aurait
 * refabriqué la même divergence un cran plus bas. */
#define HC_PART_QUELCONQUE (-1)

static int part_du_type(const Object *p, int type)
{
    if (type == HC_PART_QUELCONQUE)
        return p->type == OBJ_BUTTON || p->type == OBJ_FIELD;
    return (int)p->type == type;
}

static Object *find_part(Object *owner, int type, const char *name)
{
    if (!owner) return NULL;
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (part_du_type(p, type) && p->name && ci_equal(p->name, name)) return p;
    }
    return NULL;
}

/* part par id absolu */
static Object *find_part_by_id(Object *owner, int type, int id)
{
    if (!owner) return NULL;
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (part_du_type(p, type) && p->id == id) return p;
    }
    return NULL;
}

/* part par rang parmi les objets de ce type (1-based) */
/* Combien de parts de ce type chez ce propriétaire — avec la même règle que
 * find_part_by_rank, sentinelle comprise. Les ordinaux en ont besoin : « last
 * field » ne veut rien dire sans savoir combien il y en a, et le total dépend
 * de la COUCHE qu'on interroge. hc_part_count ne sait compter que par
 * ObjType, donc pas « part » ; il ne pouvait pas servir ici. */
static int compte_parts(Object *owner, int type)
{
    if (!owner) return 0;
    int n = 0;
    for (int i = 0; i < owner->nparts; i++)
        if (part_du_type(owner->parts[i], type)) n++;
    return n;
}

/* UN ORDINAL ÉCRIT EN TOUTES LETTRES, et ce qui le suit.
 *
 * L'analyseur v3 a la même table dans hct_expr.c, pour des JETONS ; celle-ci
 * lit du TEXTE, parce que « set » reconstruit sa cible en texte et la confie
 * à l'ancien résolveur. Deux lectures, soit — mais un seul endroit décide de
 * ce que « last » ou « middle » VEUT DIRE : v3_rang_ordinal, que les deux
 * appellent. C'est la divergence qu'on veut éviter, pas la duplication du
 * tableau de mots. */
static int v3_rang_ordinal(HctOrdinal o, int total);

static int ordinal_mot(const char *s, HctOrdinal *o, const char **apres)
{
    static const struct { const char *mot; HctOrdinal ord; } ORD[] = {
        { "first", HCT_ORD_PREMIER },   { "second", HCT_ORD_DEUXIEME },
        { "third", HCT_ORD_TROISIEME }, { "fourth", HCT_ORD_QUATRIEME },
        { "fifth", HCT_ORD_CINQUIEME }, { "sixth", HCT_ORD_SIXIEME },
        { "seventh", HCT_ORD_SEPTIEME },{ "eighth", HCT_ORD_HUITIEME },
        { "ninth", HCT_ORD_NEUVIEME },  { "tenth", HCT_ORD_DIXIEME },
        { "middle", HCT_ORD_MILIEU },   { "last", HCT_ORD_DERNIER },
        { "any", HCT_ORD_QUELCONQUE },  { NULL, HCT_ORD_AUCUN }
    };
    for (int k = 0; ORD[k].mot; k++)
        if (ci_word(s, ORD[k].mot)) {
            *o = ORD[k].ord;
            *apres = skip_spaces(s + strlen(ORD[k].mot));
            return 1;
        }
    return 0;
}

static Object *find_part_by_rank(Object *owner, int type, int rank)
{
    if (!owner || rank < 1) return NULL;
    int n = 0;
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (part_du_type(p, type) && ++n == rank) return p;
    }
    return NULL;
}

/* ═══ LES CARTES D'UN FOND ══════════════════════════════════════════════
 *
 * « card 1 of bg 2 » désigne la PREMIÈRE CARTE QUI UTILISE LE FOND 2, pas la
 * première carte de la pile. Toutes les recherches de carte ignoraient ce
 * « of » : elles indexaient dans la pile entière, et rendaient donc une carte
 * — la mauvaise — sans le moindre message. Mesuré, sur une pile A B C D dont
 * A et B sont sur le fond 1 et C et D sur le fond 2 :
 *
 *     card 1 of bg 2   ->  A     (c'est C)
 *     card 2 of bg 2   ->  B     (c'est D)
 *     go card 1 of bg 2 -> reste sur A
 *
 * Le COMPTAGE, lui, était juste : « the number of cards of bg 2 » rendait
 * bien 2. Une boucle écrite à la main, à un seul endroit, qui savait ce que
 * les quatre résolveurs ignoraient. C'est le signe habituel — une règle
 * connue d'un seul site est une règle que les autres n'appliquent pas.
 *
 * `fond` à NULL prend toute la pile : les versions sans fond ci-dessous ne
 * sont plus que des appels à celles-ci, si bien qu'aucune des deux familles
 * ne peut dériver de l'autre. */

static int card_count_de(Object *stack, Object *fond)
{
    int n = 0;
    if (!stack) return 0;
    for (int i = 0; i < stack->nparts; i++) {
        Object *c = stack->parts[i];
        if (c->type != OBJ_CARD) continue;
        if (fond && c->bg != fond) continue;
        n++;
    }
    return n;
}

/* n-ième carte du fond, 0-based. */
static Object *nth_card_de(Object *stack, Object *fond, int n)
{
    if (!stack || n < 0) return NULL;
    for (int i = 0; i < stack->nparts; i++) {
        Object *c = stack->parts[i];
        if (c->type != OBJ_CARD) continue;
        if (fond && c->bg != fond) continue;
        if (n-- == 0) return c;
    }
    return NULL;
}

/* Son rang dans le fond, 0-based ; -1 si elle n'y est pas. */
static int card_index_de(Object *stack, Object *fond, Object *card)
{
    int n = 0;
    if (!stack || !card) return -1;
    for (int i = 0; i < stack->nparts; i++) {
        Object *c = stack->parts[i];
        if (c->type != OBJ_CARD) continue;
        if (fond && c->bg != fond) continue;
        if (c == card) return n;
        n++;
    }
    return -1;
}

static Object *card_par_nom_de(Object *stack, Object *fond, const char *name)
{
    if (!stack) return NULL;
    for (int i = 0; i < stack->nparts; i++) {
        Object *p = stack->parts[i];
        if (p->type != OBJ_CARD) continue;
        if (fond && p->bg != fond) continue;
        if (p->name && ci_equal(p->name, name)) return p;
    }
    return NULL;
}

static Object *card_par_id_de(Object *stack, Object *fond, int id)
{
    if (!stack) return NULL;
    for (int i = 0; i < stack->nparts; i++) {
        Object *p = stack->parts[i];
        if (p->type != OBJ_CARD) continue;
        if (fond && p->bg != fond) continue;
        if (p->id == id) return p;
    }
    return NULL;
}

/* ---- cartes : l'ordre, pour « go next card » ---- */

static int card_count(Object *stack) { return card_count_de(stack, NULL); }

/* n-ième carte, 0-based, en ne comptant que les cartes */
static Object *nth_card(Object *stack, int n) { return nth_card_de(stack, NULL, n); }

static int card_index(Object *stack, Object *card)
{
    return card_index_de(stack, NULL, card);
}

/* Déclarés ici parce que resolve() en a besoin : un descripteur d'objet peut
 * porter une expression (« field f », « button (i+1) »), qu'il faut donc
 * pouvoir évaluer alors que les variables et l'analyseur ne sont définis que
 * bien plus bas. */
static const char *var_get(const char *name);
static void eval_expr(const char *s, char *out, int outlen);

/* Évalue le jeton qui désigne un objet, quand ce n'est ni un nombre littéral
 * ni une chaîne entre guillemets. Trois formes, dans cet ordre :
 *
 *   (expr)      évaluée par l'analyseur — « button (i + 1) »
 *   identif.    lue comme variable      — « field f »
 *   le reste    rendu tel quel          — « field toto », nom nu
 *
 * On n'appelle l'analyseur QUE sur la forme parenthésée. Un identificateur nu
 * passe par var_get directement : eval_expr repasserait par term_value, qui
 * commence justement par appeler resolve(), et l'on tournerait en rond sur
 * une tournure inattendue. La parenthèse, elle, est un signal explicite du
 * script, et son contenu ne peut pas se replier sur le jeton d'origine.
 *
 * Sortie vide si le jeton est composé de plusieurs mots : c'est alors une
 * tournure que resolve() ne sait pas lire, et mieux vaut ne rien prétendre. */
static void eval_id_token(const char *ref, char *out, int outlen)
{
    out[0] = '\0';
    ref = skip_spaces(ref);
    if (!*ref) return;

    if (*ref == '(') { eval_expr(ref, out, outlen); }
    else {
        char tok[128];
        const char *after = next_word(ref, tok, sizeof tok);
        if (*skip_spaces(after)) return;          /* plusieurs mots : on passe */
        const char *v = var_get(tok);
        snprintf(out, outlen, "%s", v ? v : tok);
    }

    /* Les espaces de bord fausseraient aussi bien le test « est-ce un
     * nombre ? » que la comparaison de nom. */
    int n = (int)strlen(out);
    while (n > 0 && isspace((unsigned char)out[n-1])) out[--n] = '\0';
    int lead = 0;
    while (out[lead] && isspace((unsigned char)out[lead])) lead++;
    if (lead) memmove(out, out + lead, strlen(out + lead) + 1);
}

/* Résout une référence du genre :
 *   button "ok"        (carte courante, puis fond)
 *   bg button "nav"    (fond de la carte courante)
 *   field "notes"
 *   field f            (rang ou nom pris dans une variable)
 *   button (i + 1)     (expression entre parenthèses)
 *   card "accueil"
 *   this card / next card / previous card / first card / last card
 *   me / the target
 *   stack
 */
/* LA PORTÉE D'UN DESCRIPTEUR : « <part> of <carte> of <pile> ».
 *
 * resolve travaille RELATIVEMENT à la carte courante, et ignorait purement et
 * simplement la queue « of … ». Mesuré : depuis une autre carte,
 * « card button "Bouton" of card id 101 » ne résolvait rien — la portée était
 * lue puis jetée.
 *
 * Ça n'avait pas d'importance tant que personne ne fabriquait de descripteur
 * complet. Ça en a maintenant : « the long name of me » n'a d'intérêt que s'il
 * se RE-RÉSOUT, sans quoi ce n'est qu'une décoration.
 *
 * Le mécanisme est celui du langage lui-même : on isole la DERNIÈRE portée, on
 * la résout d'abord, on se place dessus le temps de résoudre la tête, puis on
 * revient. La récursion gère les chaînes de plusieurs « of » sans rien ajouter.
 *
 * Un « of » entre GUILLEMETS n'en est pas un : une carte nommée « Table of
 * contents » se désigne toujours par son nom entier.
 *
 * Tout échec retombe sur la résolution locale, celle d'avant : une variable
 * dont le texte contient « of » sans être un descripteur ne doit rien changer. */
static Object *resolve_local(const char *ref);

/* La dernière occurrence de « of » hors guillemets, ou NULL. */
static const char *derniere_portee(const char *s)
{
    const char *trouve = NULL;
    int dans_guillemets = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '"') { dans_guillemets = !dans_guillemets; continue; }
        if (dans_guillemets) continue;
        if ((p == s || p[-1] == ' ' || p[-1] == '\t') &&
            (p[0] == 'o' || p[0] == 'O') && (p[1] == 'f' || p[1] == 'F') &&
            (p[2] == ' ' || p[2] == '\t'))
            trouve = p;
    }
    return trouve;
}

/* Le FOND dans lequel resolve travaille, quand « of bg … » en a nommé un.
 * NULL le reste du temps, et « NULL » veut dire « toute la pile » pour les
 * cinq recherches de carte. Voir leur commentaire commun, plus haut. */
static Object *g_portee_fond = NULL;

static Object *resolve(const char *ref)
{
    if (!ref) return NULL;
    const char *of = derniere_portee(skip_spaces(ref));
    if (!of) return resolve_local(ref);

    static int profondeur = 0;
    if (profondeur >= 8) return resolve_local(ref);

    const char *deb = skip_spaces(ref);
    size_t ltete = (size_t)(of - deb);
    while (ltete > 0 && (deb[ltete-1] == ' ' || deb[ltete-1] == '\t')) ltete--;
    /* Le même plafond que les descripteurs, et pour la même raison : un nom de
     * 300 caractères produisait une tête de 279, qui repartait ici en
     * resolve_local sur la référence ENTIÈRE — donc sans sa portée, donc en
     * échec. Mesuré : « the id of » un long name cessait de répondre entre 279
     * et 319 caractères de descripteur, sans un mot. */
    if (ltete == 0 || ltete >= HC_NOM_MAX + 64) return resolve_local(ref);

    char tete[HC_NOM_MAX + 64];
    memcpy(tete, deb, ltete); tete[ltete] = '\0';
    const char *queue = skip_spaces(of + 2);
    if (!*queue) return resolve_local(ref);

    profondeur++;
    Object *portee = resolve(queue);

    /* LA PORTÉE N'EXISTE PAS : ON S'ARRÊTE, ON NE RETOMBE PAS SUR LA CARTE
     * COURANTE.
     *
     * C'est le jumeau, dans l'ancien moteur, du défaut corrigé dans
     * hct_resout_corps. La dernière ligne de cette fonction disait
     * « return r ? r : resolve_local(ref) », et resolve_local IGNORE le
     * « of X » : elle lit la tête et la cherche là où l'on se trouve. Donc
     * « field "X" of card "Absente" », quand cette carte n'existe pas,
     * rendait le champ « X » DE LA CARTE COURANTE.
     *
     * La v3 étant corrigée la première, la lecture continuait pourtant de
     * mentir : l'échec de la v3 passe le relais au recours, et le recours
     * tombait ici. Une seule des deux portes réparée ne répare rien — c'est
     * le motif qu'on traque depuis des semaines, et il a mordu dans l'heure
     * même où on le nommait.
     *
     * Ce garde-fou ne vise QUE la portée introuvable. Les sorties anticipées
     * au-dessus — pas de « of », découpage impossible, profondeur épuisée —
     * gardent leur repli : là, la référence n'a pas été comprise comme ayant
     * une portée, et resolve_local reste la lecture honnête de ce qu'on a. */
    if (!portee) { profondeur--; return NULL; }

    Object *r = NULL;
    if (portee) {
        /* La portée est une CARTE, ou une pile — auquel cas on se place sur sa
         * carte courante, ou sur la première si elle n'en a pas encore. */
        Object *ou = NULL;
        if (portee->type == OBJ_CARD) ou = portee;
        else if (portee->type == OBJ_STACK) {
            ou = (g_current_card && g_current_card->owner == portee)
                 ? g_current_card : nth_card(portee, 0);
        }
        else if (portee->type == OBJ_BACKGROUND) {
            /* « bg button "x" of background "F" » : la première carte de ce
             * fond, faute de mieux — un fond n'est pas un lieu où se tenir. */
            Object *pile = portee->owner;
            for (int i = 0; pile && i < pile->nparts; i++)
                if (pile->parts[i]->type == OBJ_CARD && pile->parts[i]->bg == portee) {
                    ou = pile->parts[i]; break;
                }
        }
        if (ou) {
            Object *sauve = g_current_card;
            /* LE FOND RESTE UNE PORTÉE, PAS SEULEMENT UN POINT DE DÉPART.
             *
             * Se poser sur une carte du fond suffit pour « bg field "x" of
             * background "F" » — un champ de fond est le même partout. Ça ne
             * suffit PAS pour désigner une CARTE : « card "A" of bg 2 »
             * repartait en cherchant « A » dans toute la pile, et la trouvait
             * même posée sur un autre fond.
             *
             * Mesuré, sur A B (fond 1) C D (fond 2), une fois la v3
             * corrigée : elle refusait, l'ancien moteur rendait A. Deux
             * portes, deux réponses — le motif qu'on traque, et cette fois il
             * s'est vu tout de suite parce qu'on cherchait le jumeau avant de
             * commiter. */
            Object *sauve_fond = g_portee_fond;
            if (portee->type == OBJ_BACKGROUND) g_portee_fond = portee;
            g_current_card = ou;
            r = resolve(tete);
            g_portee_fond = sauve_fond;
            g_current_card = sauve;
        }
    }
    profondeur--;

    /* ET SI L'OBJET N'EST PAS DANS CETTE PORTÉE-LÀ, IL N'EST PAS AILLEURS.
     *
     * Le repli « r ? r : resolve_local(ref) » avait le même vice que le cas
     * ci-dessus, d'un cran plus fin : la carte existe, mais le champ n'y est
     * pas — et resolve_local, qui ignore le « of X », allait le chercher sur
     * la carte COURANTE. Mesuré, avec un champ « X » sur la carte Une
     * seulement :
     *
     *     put field "X" of card "Deux"   -> SUR UNE
     *
     * Le script nomme une carte et obtient le contenu d'une autre. Une
     * portée qu'on a su lire et où l'on a su entrer est une portée qu'il
     * faut respecter : l'échec dedans est un échec tout court. */
    return r;
}

static Object *resolve_local(const char *ref)
{
    ref = skip_spaces(ref);
    Object *card = g_current_card;
    Object *bg   = card ? card->bg : NULL;
    Object *stack = card ? card->owner : NULL;

    /* « the » est facultatif devant un descripteur : the field "notes",
       the target, the next card… */
    if (ci_word(ref, "the")) ref = skip_spaces(ref + 3);

    /* --- pronoms --- */
    if (ci_word(ref, "me")) return g_me;
    if (ci_word(ref, "target")) return g_target;

    int want_bg = 0;
    if (ci_word(ref, "bg") || ci_word(ref, "background") || ci_word(ref, "bkgnd")) {
        want_bg = 1;
        ref = skip_spaces(strchr(ref, ' ') ? strchr(ref, ' ') : ref + strlen(ref));
        if (!*ref) return bg;   /* « background » seul = le fond de la carte courante */

        /* « background "second" », « bg id 4 », « background 2 » : le fond
         * lui-même, et non un objet posé dessus. Sans ceci, « background »
         * n'est qu'un préfixe pour « bg button … » et un fond nommé reste
         * introuvable. */
        if (*ref == '"') {
            char nm[HC_NOM_MAX];
            descripteur_lit(ref, nm, sizeof nm);
            for (int i = 0; stack && i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_BACKGROUND &&
                    stack->parts[i]->name && ci_equal(stack->parts[i]->name, nm))
                    return stack->parts[i];
            return NULL;
        }
        if (ci_word(ref, "id")) {
            int wanted = hc_id(skip_spaces(ref + 2));
            for (int i = 0; stack && i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_BACKGROUND &&
                    stack->parts[i]->id == wanted)
                    return stack->parts[i];
            return NULL;
        }
          {
            const char *num = ref;
            char val[128];
            if (!isdigit((unsigned char)*ref)) {
                eval_id_token(ref, val, sizeof val);
                if (val[0] && isdigit((unsigned char)val[0])) num = val;
            }
            if (isdigit((unsigned char)*num)) {
                int n = hc_rang(num) - 1;          /* 1-based en HyperTalk */
                for (int i = 0; stack && i < stack->nparts; i++)
                    if (stack->parts[i]->type == OBJ_BACKGROUND && n-- == 0)
                        return stack->parts[i];
                return NULL;
            }
        }
        /* « go background commun » : nom de fond sans guillemets. */
        if (!ci_word(ref, "button") && !ci_word(ref, "btn") &&
            !ci_word(ref, "field")  && !ci_word(ref, "fld")  &&
            !ci_word(ref, "part")) {
            char nm[HC_NOM_MAX];
            int n = 0;
            while (ref[n] && n < (int)sizeof nm - 1) { nm[n] = ref[n]; n++; }
            while (n > 0 && isspace((unsigned char)nm[n-1])) n--;
            nm[n] = '\0';
            for (int i = 0; stack && i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_BACKGROUND &&
                    stack->parts[i]->name && ci_equal(stack->parts[i]->name, nm))
                    return stack->parts[i];
        }
    } else if (ci_word(ref, "card") || ci_word(ref, "cd")) {
        /* "card button" / "card field" / "card \"nom\"" / "card 3" */
        const char *after = skip_spaces(strchr(ref, ' ') ? strchr(ref, ' ') : ref + strlen(ref));
        if (*after == '"') {
            char nm[HC_NOM_MAX];
            descripteur_lit(after, nm, sizeof nm);
            return card_par_nom_de(stack, g_portee_fond, nm);
        }
        if (ci_word(after, "id")) {                    /* card id N */
            const char *a = skip_spaces(after + 2);
            int wanted;
            if (isdigit((unsigned char)*a)) wanted = hc_id(a);
            else { char v[128]; eval_id_token(a, v, sizeof v); wanted = hc_id(v); }
            return card_par_id_de(stack, g_portee_fond, wanted);
        }
        if (isdigit((unsigned char)*after))
            return nth_card_de(stack, g_portee_fond, hc_rang(after) - 1);

        /* « go card canard » : HyperCard accepte un nom de carte sans
         * guillemets. On ne tente le nom nu que si ce qui suit n'est pas un
         * objet posé sur la carte, sinon « card button "ok" » y passerait. */
        if (*after && !ci_word(after, "button") && !ci_word(after, "btn") &&
                      !ci_word(after, "field")  && !ci_word(after, "fld") &&
                      !ci_word(after, "part")   && !ci_word(after, "window")) {
            /* --- désignateur dynamique : « go cd v », « go card (i+1) » ------
             * Même règle que pour les champs et les boutons : le jeton est
             * évalué, puis on regarde ce qui en sort — un nombre désigne un
             * rang, autre chose un nom. C'est ce qui rend possible le
             * sommaire d'une pile, où le nom de la carte à ouvrir est calculé
             * à partir de la ligne cliquée :
             *
             *     put the selection into deb
             *     go cd deb
             *
             * Une variable jamais affectée vaut son propre nom, donc
             * « go card canard » continue de désigner la carte canard. */
            char nm[HC_NOM_MAX];
            eval_id_token(after, nm, sizeof nm);

            if (!nm[0]) {
                /* Plusieurs mots : eval_id_token s'abstient. Un nom de carte
                 * peut légitimement en contenir, sans guillemets — on reprend
                 * alors la chaîne entière, comme avant. */
                int n = 0;
                while (after[n] && n < (int)sizeof nm - 1) { nm[n] = after[n]; n++; }
                while (n > 0 && isspace((unsigned char)nm[n-1])) n--;
                nm[n] = '\0';
            }

            int nlen = (int)strlen(nm);
            if (nlen > 0 && (int)strspn(nm, "0123456789") == nlen) {
                Object *c = nth_card_de(stack, g_portee_fond, hc_rang(nm) - 1);
                if (c) return c;
            }
            Object *c = card_par_nom_de(stack, g_portee_fond, nm);
            if (c) return c;
        }
        ref = after;
    }

    /* --- cartes désignées par leur rang --- */
    if (ci_word(ref, "this")) {
        const char *w = skip_spaces(ref + 4);
        if (!*w || ci_word(w, "card") || ci_word(w, "cd")) return card;
        if (ci_word(w, "stack")) return stack;
        if (ci_word(w, "background") || ci_word(w, "bg")) return bg;
    }
    /* « next/previous background » : HyperCard ne se tient jamais sur un fond,
     * on rend donc une CARTE — la première de ce fond. On balaie l'ordre des
     * cartes jusqu'à en croiser une dont le fond diffère, avec bouclage comme
     * « go next card ». Sans ceci, « next » ignorait le mot qui le suit et
     * « go next background » se comportait comme « go next card ». */
    /* CE BLOC PASSE AVANT CELUI DES FONDS, et l'ordre n'est pas cosmétique.
     *
     * Celui d'en dessous reconnaît « last bg » et rend la première carte de
     * ce fond — sans regarder le mot d'APRÈS. « set the name of last bg
     * field to "x" » renommait donc une CARTE. C'est exactement la faute
     * qu'on corrige ici, un cran plus haut : un mot lu, le suivant ignoré.
     * Essayer d'abord la forme la plus LONGUE est la seule façon de ne pas
     * avoir à réparer les deux blocs séparément.
     *
     * Quand la forme n'est pas une part — « first background » —, ce bloc
     * ne rend rien et laisse celui d'en dessous faire son travail. */
    /* UN ORDINAL EST UN PRÉFIXE : CE QUI LE SUIT DÉCIDE DE CE QU'IL DÉSIGNE.
     *
     * Ces deux lignes disaient « first » -> première CARTE et « last » ->
     * dernière carte, SANS REGARDER LE MOT SUIVANT. Signalé à l'usage :
     *
     *     set the name of first field to "toto"   renommait la CARTE
     *
     * Une écriture qui se pose ailleurs que là où on l'envoie, en silence.
     * En lecture, « the name of first field » rendait « card "Une" » — une
     * mauvaise réponse, pas une erreur. Et « second field » disait « objet
     * introuvable », ce qui rendait le défaut plus trompeur encore : la forme
     * la plus courante était justement celle qui mentait sans rien dire.
     *
     * LE BLOC AU-DESSUS FAISAIT DÉJÀ CE TEST pour les fonds — « first
     * background » regarde le mot qui suit depuis longtemps. La règle était
     * connue trois lignes plus haut et ces deux-ci ne l'appliquaient pas.
     *
     * On traite donc les treize ordinaux, et pas seulement first et last :
     * l'exécuteur v3 les connaît tous, et n'en servir que deux ici ferait
     * répondre différemment à « the name of third field » selon le chemin
     * emprunté — lecture par v3, écriture par « set ». */
    {
        HctOrdinal o = HCT_ORD_AUCUN;
        const char *ap = NULL;
        if (ordinal_mot(ref, &o, &ap)) {
            int fond = want_bg, vu_fond = 0;
            if      (ci_word(ap, "card"))       { ap = skip_spaces(ap + 4);  fond = 0; }
            else if (ci_word(ap, "cd"))         { ap = skip_spaces(ap + 2);  fond = 0; }
            else if (ci_word(ap, "background")) { ap = skip_spaces(ap + 10); fond = 1; vu_fond = 1; }
            else if (ci_word(ap, "bkgnd"))      { ap = skip_spaces(ap + 5);  fond = 1; vu_fond = 1; }
            else if (ci_word(ap, "bg"))         { ap = skip_spaces(ap + 2);  fond = 1; vu_fond = 1; }

            int tp = -1;
            if      (ci_word(ap, "button") || ci_word(ap, "btn")) tp = OBJ_BUTTON;
            else if (ci_word(ap, "field")  || ci_word(ap, "fld")) tp = OBJ_FIELD;

            if (tp >= 0) {
                /* LE TOTAL SE COMPTE PAR COUCHE : « last field » n'a pas le
                 * même sens sur la carte et sur le fond. Sans portée écrite,
                 * on cherche sur la carte puis on se rabat sur le fond, comme
                 * partout ailleurs — en RECOMPTANT, sans quoi le repli
                 * chercherait un rang calculé pour l'autre couche. */
                Object *couche = fond ? bg : card;
                int r = v3_rang_ordinal(o, compte_parts(couche, tp));
                Object *p = r > 0 ? find_part_by_rank(couche, tp, r) : NULL;
                if (!p && !fond) {
                    int r2 = v3_rang_ordinal(o, compte_parts(bg, tp));
                    if (r2 > 0) p = find_part_by_rank(bg, tp, r2);
                }
                return p;
            }

            /* Pas une part : c'est une CARTE — « first card », ou « first »
             * tout seul, la forme des scripts d'époque. Les fonds sont déjà
             * partis plus haut ; s'il en reste un ici, on ne s'en mêle pas. */
            if (!vu_fond) {
                int r = v3_rang_ordinal(o, card_count(stack));
                if (r > 0) return nth_card(stack, r - 1);
                return NULL;
            }
        }
    }

    /* « next/previous/first/last background » : HyperCard ne se tient jamais
     * sur un fond, on rend donc une CARTE — la première de ce fond. Sans ceci,
     * « next » et consorts ignoraient le mot qui les suit, et « go next
     * background » se comportait comme « go next card ». */
    {
        const char *rel = NULL;
        int pas = 0, absolu = 0;    /* absolu : 1 = premier fond, -1 = dernier */
        if      (ci_word(ref, "next"))     { rel = skip_spaces(ref + 4); pas = +1; }
        else if (ci_word(ref, "previous")) { rel = skip_spaces(ref + 8); pas = -1; }
        else if (ci_word(ref, "prev"))     { rel = skip_spaces(ref + 4); pas = -1; }
        else if (ci_word(ref, "first"))    { rel = skip_spaces(ref + 5); absolu = +1; }
        else if (ci_word(ref, "last"))     { rel = skip_spaces(ref + 4); absolu = -1; }

        if (rel && (ci_word(rel, "background") || ci_word(rel, "bg"))) {

            /* first/last : le fond visé est absolu, pas relatif à la carte
             * courante. On prend le premier ou le dernier OBJ_BACKGROUND de
             * la pile, puis sa première carte. */
            if (absolu) {
                Object *cible = NULL;
                for (int i = 0; i < stack->nparts; i++) {
                    Object *o = stack->parts[i];
                    if (o->type != OBJ_BACKGROUND) continue;
                    if (absolu > 0) { cible = o; break; }
                    cible = o;                     /* on garde le dernier vu */
                }
                if (!cible) return NULL;
                for (int j = 0; j < card_count(stack); j++) {
                    Object *d = nth_card(stack, j);
                    if (d && d->bg == cible) return d;
                }
                return NULL;
            }

            /* next/previous : on balaie l'ordre des cartes jusqu'à en croiser
             * une dont le fond diffère, avec bouclage comme « go next card ». */
            int n = card_count(stack);
            int i = card_index(stack, card);
            if (n <= 0 || i < 0) return NULL;
            for (int k = 1; k <= n; k++) {
                Object *c = nth_card(stack, ((i + pas * k) % n + n) % n);
                if (!c || c->bg == bg) continue;
                for (int j = 0; j < n; j++) {      /* première carte de CE fond */
                    Object *d = nth_card(stack, j);
                    if (d && d->bg == c->bg) return d;
                }
                return c;
            }
            return card;    /* un seul fond : on reste sur place, sans erreur */
        }
    }
    if (ci_word(ref, "next") || ci_word(ref, "prev") || ci_word(ref, "previous")) {
        int pas = ci_word(ref, "next") ? +1 : -1;
        int nc = card_count(stack);
        int i  = card_index(stack, card);
        if (nc <= 0 || i < 0) return NULL;
        return nth_card(stack, ((i + pas) % nc + nc) % nc);
    }

    if (ci_word(ref, "stack")) {
        const char *after = skip_spaces(ref + 5);
        if (!*after) return stack;
        if (*after == '"') {                 /* « stack "Essai" » */
            char nm[HC_NOM_MAX];
            descripteur_lit(after, nm, sizeof nm);
            if (stack && stack->name && ci_equal(stack->name, nm)) return stack;
            /* Une AUTRE pile ouverte peut porter ce nom : c'est tout l'objet
             * du registre. Sans lui, « the name of stack "Autre" » ne pouvait
             * désigner que la pile courante. */
            return find_open_stack(nm);
        }
    }

    ObjType t;
    if (ci_word(ref, "button") || ci_word(ref, "btn")) { t = OBJ_BUTTON; }
    else if (ci_word(ref, "field") || ci_word(ref, "fld")) { t = OBJ_FIELD; }
    else return NULL;
    while (*ref && !isspace((unsigned char)*ref) && *ref != '"') ref++;
    ref = skip_spaces(ref);

    /* --- button id N / field id N --- */
    if (ci_word(ref, "id")) {
        const char *a = skip_spaces(ref + 2);
        int wanted;
        if (isdigit((unsigned char)*a)) {
            wanted = hc_id(a);
        } else {
            /* « field id n » : l'identifiant vient d'une variable. */
            char v[128];
            eval_id_token(a, v, sizeof v);
            wanted = hc_id(v);
        }
        Object *o = find_part_by_id(card, t, wanted);
        if (!o) o = find_part_by_id(bg, t, wanted);
        return o;
    }

    /* --- button N (par rang, 1-based) --- */
    if (isdigit((unsigned char)*ref)) {
        int n = hc_rang(ref);
        Object *o = find_part_by_rank(want_bg ? bg : card, t, n);
        if (!o && !want_bg) o = find_part_by_rank(bg, t, n);
        return o;
    }

    char nm[HC_NOM_MAX];
    nm[0] = '\0';

    if (*ref == '"') {
        descripteur_lit(ref, nm, sizeof nm);
    } else {
        /* --- désignateur dynamique : « field f », « button (i + 1) » ------
         * HyperCard accepte une expression là où l'on écrit d'ordinaire un
         * rang ou un nom. C'est ce qui rend les boucles possibles :
         *
         *     repeat with f = 1 to the number of fields
         *         set the textStyle of field f to plain
         *     end repeat
         *
         * Le jeton n'est ici ni un chiffre ni une chaîne entre guillemets :
         * on l'évalue, puis on regarde CE QUI EN SORT — un nombre désigne un
         * rang, autre chose un nom. La variable n'a donc pas à savoir laquelle
         * des deux formes elle porte, exactement comme « field 3 » et
         * « field "titre" » cohabitent.
         *
         * Une variable jamais affectée vaut son propre nom en HyperTalk :
         * « field toto » retombe naturellement sur le champ nommé toto, sans
         * cas particulier. Auparavant quoted() rendait une chaîne vide sur un
         * jeton nu et resolve() abandonnait aussitôt — les deux formes
         * échouaient ensemble. */
        eval_id_token(ref, nm, sizeof nm);

        /* Un rang, si tout ce qui sort est un nombre. */
        int nlen = (int)strlen(nm);
        if (nlen > 0 && (int)strspn(nm, "0123456789") == nlen) {
            int n = hc_rang(nm);
            Object *o = find_part_by_rank(want_bg ? bg : card, t, n);
            if (!o && !want_bg) o = find_part_by_rank(bg, t, n);
            return o;
        }
    }

    if (!nm[0]) return NULL;

    Object *o = NULL;
    if (want_bg) {
        o = find_part(bg, t, nm);
    } else {
        o = find_part(card, t, nm);
        if (!o) o = find_part(bg, t, nm);   /* repli sur le fond */
    }
    return o;
}

/* ==================== variables ==================== */

/* HyperTalk : une variable est locale au gestionnaire qui l'emploie,
 * sauf si celui-ci l'a déclarée `global`. La boîte de message, elle,
 * travaille directement dans l'espace global. */

typedef struct { char *name, *val; } Var;

typedef struct Frame {
    Var   *v;   int n,   cap;
    char **gl;  int ngl, capgl;   /* noms déclarés `global` dans ce gestionnaire */
} Frame;

static Frame  g_globals;        /* vit aussi longtemps que le programme */
static Frame *g_frame = NULL;   /* gestionnaire en cours ; NULL = boîte de message */

static int frame_has_global(Frame *f, const char *name)
{
    if (!f) return 0;
    for (int i = 0; i < f->ngl; i++)
        if (ci_equal(f->gl[i], name)) return 1;
    return 0;
}

/* la table où vit `name` */
static Frame *frame_for(const char *name)
{
    if (!g_frame) return &g_globals;
    if (frame_has_global(g_frame, name)) return &g_globals;
    return g_frame;
}

static const char *var_get(const char *name)
{
    Frame *f = frame_for(name);
    for (int i = 0; i < f->n; i++)
        if (ci_equal(f->v[i].name, name)) return f->v[i].val;
    return NULL;
}

static void var_set(const char *name, const char *val)
{
    Frame *f = frame_for(name);
    for (int i = 0; i < f->n; i++)
        if (ci_equal(f->v[i].name, name)) {
            /* LA COPIE D'ABORD, LA LIBÉRATION ENSUITE.
             *
             * L'ancienne valeur était libérée AVANT de savoir si la nouvelle
             * tenait en mémoire. En cas de pénurie la variable devenait NULL,
             * ce que var_get rend comme « pas de telle variable » : le mot
             * retombait sous la règle « identificateur inconnu = son propre
             * nom » et le script continuait avec son nom de variable en guise
             * de valeur. Silencieusement, et avec l'ancienne valeur déjà
             * perdue. Ici comme pour le realloc juste en dessous, une pénurie
             * se dit. */
            char *neuf = val ? dupstr(val) : NULL;
            if (val && !neuf) hc_memoire_epuisee("valeur d'une variable");
            free(f->v[i].val);
            f->v[i].val = neuf;
            return;
        }
    if (f->n == f->cap) {
        int cap = f->cap ? f->cap * 2 : 8;
        Var *p = realloc(f->v, (size_t)cap * sizeof *p);
        if (!p) hc_memoire_epuisee("variables locales d'un gestionnaire");
        f->v = p; f->cap = cap;
    }
    f->v[f->n].name = dupstr(name);
    f->v[f->n].val  = val ? dupstr(val) : NULL;
    if (!f->v[f->n].name || (val && !f->v[f->n].val))
        hc_memoire_epuisee("nom ou valeur d'une variable");
    f->n++;
}

static void frame_declare_global(Frame *f, const char *name)
{
    if (!f || !*name || frame_has_global(f, name)) return;
    if (f->ngl == f->capgl) {
        int cap = f->capgl ? f->capgl * 2 : 4;
        char **p = realloc(f->gl, (size_t)cap * sizeof *p);
        if (!p) hc_memoire_epuisee("déclarations « global » d'un gestionnaire");
        f->gl = p; f->capgl = cap;
    }
    f->gl[f->ngl++] = dupstr(name);

    /* Une globale DÉCLARÉE existe, et vaut la chaîne vide tant qu'on ne l'a
     * pas affectée. Sans cela elle tombait sous la règle « identificateur
     * inconnu = son propre nom », et « global lastclick » suivi de
     * « if lastclick is empty » répondait faux — la globale valant le texte
     * « lastclick ». Cette règle-là ne concerne que les mots jamais déclarés.
     *
     * On ne touche pas à une globale déjà posée par un autre gestionnaire :
     * c'est tout l'intérêt d'une globale que de survivre entre les appels. */
    for (int i = 0; i < g_globals.n; i++)
        if (ci_equal(g_globals.v[i].name, name)) return;

    if (g_globals.n == g_globals.cap) {
        int cap = g_globals.cap ? g_globals.cap * 2 : 8;
        Var *p = realloc(g_globals.v, (size_t)cap * sizeof *p);
        if (!p) hc_memoire_epuisee("table des variables globales");
        g_globals.v = p; g_globals.cap = cap;
    }
    g_globals.v[g_globals.n].name = dupstr(name);
    g_globals.v[g_globals.n].val  = dupstr("");
    g_globals.n++;
}

static void frame_clear(Frame *f)
{
    for (int i = 0; i < f->n; i++)   { free(f->v[i].name); free(f->v[i].val); }
    for (int i = 0; i < f->ngl; i++) { free(f->gl[i]); }
    free(f->v); free(f->gl);
    memset(f, 0, sizeof *f);
}

/* Défini plus bas, avec les commandes de fichier. */
static void file_close(const char *nom);

void hc_shutdown(void)
{
    /* Fermer les fichiers laissés ouverts par un script : sans cela, ce qui
     * attend dans les tampons du système ne serait jamais écrit. */
    file_close(NULL);
    /* Le presse-papiers survit volontairement à la pile qui l'a rempli :
     * c'est ce qui permet de coller d'une pile à l'autre. Il faut donc le
     * libérer ici, et pas dans hc_free. */
    hc_clipboard_clear();
    frame_clear(&g_globals);
    arena_shutdown();
}

/* ==================== évaluation d'expressions ==================== */

/*  expr     := ou
 *  ou       := et        { "or"  et }
 *  et       := non       { "and" non }
 *  non      := [ "not" ] compare
 *  compare  := concat [ (= <> is "is not" < > <= >= contains) concat ]
 *  concat   := somme     { (& | &&) somme }
 *  somme    := produit   { (+ | -) produit }
 *  produit  := facteur   { (* | / | mod | div) facteur }
 *  facteur  := "(" expr ")" | "-" facteur | nombre | "…" | référence
 */

static int ci_nequal(const char *a, const char *b, int len)
{
    for (int i = 0; i < len; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
    return 1;
}

/* Ajouter à la suite, SANS jamais dépasser. Rend la nouvelle position.
 *
 * Le motif naïf — « pos += snprintf(out + pos, outlen - pos, …) » — est un
 * piège : snprintf rend la longueur qu'il AURAIT voulu écrire, pas celle
 * qu'il a écrite. Dès que le tampon est plein, pos passe au-delà, et le tour
 * suivant calcule « out + pos » hors bornes et « outlen - pos » négatif, que
 * snprintf reçoit en size_t — donc énorme. Corruption mémoire, pas simple
 * troncature.
 *
 * Cinq des sept sites concernés bornaient déjà pos après chaque ajout ; deux
 * ne le faisaient pas, et ce sont ceux de « the params », qui concatène
 * jusqu'à seize valeurs d'un mégaoctet dans un tampon d'un mégaoctet. Plutôt
 * que de recopier une sixième fois le garde-fou, on le pose ici, une fois. */
static int ajoute_borne(char *out, int outlen, int pos,
                        const char *sep, const char *txt)
{
    if (!out || outlen <= 0) return 0;
    if (pos < 0) pos = 0;
    if (pos >= outlen - 1) return outlen - 1;   /* déjà plein */

    int n = snprintf(out + pos, (size_t)(outlen - pos), "%s%s",
                     sep ? sep : "", txt ? txt : "");
    if (n < 0) return pos;                      /* faute d'encodage */
    pos += n;
    if (pos > outlen - 1) pos = outlen - 1;     /* tronqué : on s'arrête au bord */
    return pos;
}


/* « a contient b », sans tenir compte de la casse */
static int ci_strstr(const char *hay, const char *needle)
{
    size_t nl = strlen(needle);
    if (nl == 0) return 1;
    size_t hl = strlen(hay);
    if (nl > hl) return 0;
    for (size_t i = 0; i + nl <= hl; i++)
        if (ci_nequal(hay + i, needle, (int)nl)) return 1;
    return 0;
}

/* une valeur est-elle un nombre ? */
/* ---- les valeurs : déléguées à hct_val ----
 *
 * Étape 2 de la reprise de l'interpréteur v3, et pour la même raison que les
 * morceaux : du texte entre, du texte sort, aucun état partagé.
 *
 * Deux corrections viennent avec, l'une invisible et l'autre non.
 *
 * La lecture s'appuyait sur strtod, qui accepte « 0x10 », « inf » et « nan ».
 * HyperCard n'a jamais lu que des chiffres, une décimale et un exposant : ces
 * trois-là redeviennent du texte ordinaire, ce qu'ils auraient toujours dû
 * être.
 *
 * L'écriture des non-entiers passe de « %.10g » — dix chiffres significatifs —
 * à six décimales avec les zéros de fin retirés, qui est le numberFormat par
 * défaut de HyperCard. « put 1/3 » rend donc 0.333333 et non 0.3333333333.
 * C'est plus fidèle, mais c'est visible : un script qui comparait des chaînes
 * de nombres verra la différence. */
static int as_num(const char *s, double *d)
{
    if (!hct_est_nombre(s)) return 0;
    *d = hct_vers_nombre(s);
    return 1;
}

static void put_num(double d, char *out, int outlen)
{
    hct_ecrit_nombre(d, out, outlen);
}

/* mots qui terminent une référence : ce sont des opérateurs ou des
   prépositions de commande, jamais des morceaux de nom d'objet */
static int is_stop_word(const char *s, int len)
{
    static const char *kw[] = { "is", "contains", "mod", "div", "and", "or",
                                "not", "then", "into", "after", "before", NULL };
    for (int i = 0; kw[i]; i++)
        if ((int)strlen(kw[i]) == len && ci_nequal(s, kw[i], len)) return 1;
    return 0;
}

/* Ramasse le texte d'une référence (« the name of me », « field "x" », « compteur »)
   en s'arrêtant au premier opérateur. */
static void collect_ref(const char **p, char *buf, int buflen)
{
    const char *s = *p;
    int n = 0;
    buf[0] = '\0';

    for (;;) {
        const char *w = skip_spaces(s);

        /* « word (i+1) of x » : le groupe parenthésé fait partie de la référence */
        if (*w == '(' && n > 0) {
            const char *q = w + 1;
            int depth = 1, inq2 = 0;
            while (*q && depth) {
                if (*q == '"') inq2 = !inq2;
                else if (!inq2 && *q == '(') depth++;
                else if (!inq2 && *q == ')') depth--;
                q++;
            }
            int len = (int)(q - w);
            if (n && n < buflen - 1) buf[n++] = ' ';
            if (n + len > buflen - 1) len = buflen - 1 - n;
            if (len > 0) { memcpy(buf + n, w, (size_t)len); n += len; }
            buf[n] = '\0';
            s = q;
            continue;
        }

        if (!*w || strchr("&+-*/^<>=(),", *w)) { s = w; break; }

        const char *st = w, *q = w;
        if (*w == '"') {
            q = w + 1;
            while (*q && *q != '"') q++;
            if (*q == '"') q++;
        } else {
            while (*q && !isspace((unsigned char)*q) && !strchr("&+-*/^<>=(),\"", *q)) q++;
            if (is_stop_word(st, (int)(q - st))) { s = w; break; }
        }

        int len = (int)(q - st);
        if (n && n < buflen - 1) buf[n++] = ' ';
        if (n + len > buflen - 1) len = buflen - 1 - n;
        if (len > 0) { memcpy(buf + n, st, (size_t)len); n += len; }
        buf[n] = '\0';
        s = q;
    }
    *p = s;
}

/* ==================== expressions de morceau (chunks) ==================== */

/* La signature d'HyperTalk : découper du texte sans effort.
 *     char 3 of x        char 1 to 5 of x
 *     word 2 of x        word 2 to 4 of x
 *     item 3 of x        line 2 of field "notes"
 *     the first word of x, the last line of x, the middle item of x
 *     the number of words in x
 * Les morceaux s'emboîtent : « word 2 of line 3 of field "notes" ».
 */

typedef enum { CH_NONE, CH_CHAR, CH_WORD, CH_ITEM, CH_LINE } ChunkType;

static void eval_expr(const char *s, char *out, int outlen);
static const char *find_kw(const char *s, const char *w);   /* défini plus bas */
static int hc_send_args(Object *target, const char *message,
                        char argv[][HC_VAL], int argc);   /* défini plus bas */
static int hc_call_user_function(Object *target, const char *name,
                                 char argv[][HC_VAL], int argc);  /* idem */

/* Reconnaît un nom de morceau et dit combien de caractères il occupe. */
static ChunkType chunk_kind(const char *s, int *used)
{
    static const struct { const char *w; ChunkType t; } tab[] = {
        { "characters", CH_CHAR }, { "character", CH_CHAR },
        { "chars", CH_CHAR }, { "char", CH_CHAR },
        { "words", CH_WORD }, { "word", CH_WORD },
        { "items", CH_ITEM }, { "item", CH_ITEM },
        { "lines", CH_LINE }, { "line", CH_LINE },
        { NULL, CH_NONE }
    };
    for (int i = 0; tab[i].w; i++)
        if (ci_word(s, tab[i].w)) { *used = (int)strlen(tab[i].w); return tab[i].t; }
    return CH_NONE;
}

/* Séparateur d'items, modifiable par « set the itemDelimiter to ";" ».
 *
 * HyperCard 2.2 l'a introduit pour découper autre chose que du CSV — des
 * chemins de fichiers séparés par « : », des lignes de tabulations. C'est une
 * propriété GLOBALE, et non celle d'un conteneur : elle vaut pour tout le
 * découpage tant qu'on ne la change pas, ce qui oblige les scripts prudents à
 * la remettre à la virgule après usage. */
/* LE DÉLIMITEUR D'ITEMS EST UNE CHAÎNE, DE LONGUEUR QUELCONQUE.
 *
 * Il tenait d'abord dans un char, donc dans un octet : « set the itemDelimiter
 * to "é" » n'en retenait que le premier et le découpage coupait au milieu de
 * la séquence UTF-8.
 *
 * Le remplacer par char[8] a déplacé le défaut sans le fermer. Le reste du
 * code accepte un délimiteur de PLUSIEURS CARACTÈRES — hct_chunk_* travaille
 * sur une chaîne et avance de sa longueur —, si bien que huit octets étaient
 * un compromis dangereux plutôt qu'une limite : quatre « é » font huit octets,
 * et « set the itemDelimiter to "éééé" » rendait « ééé\xc3 », de l'UTF-8
 * invalide. On recréait exactement la classe de défaut qu'on voulait fermer.
 *
 * Il est donc alloué. NULL vaut la virgule, ce qui évite d'allouer dans le cas
 * courant et donne au noyau un état initial sans allocation.
 *
 * Ce pointeur reste vivant dans une globale jusqu'à la fin du processus :
 * LeakSanitizer ne le signale pas, un bloc accessible depuis une racine
 * n'étant pas une fuite. */
static char *g_item_delim = NULL;

static const char *item_delim(void)
{
    return (g_item_delim && g_item_delim[0]) ? g_item_delim : ",";
}

static void item_delim_pose(const char *v)
{
    if (!v || !*v) { free(g_item_delim); g_item_delim = NULL; return; }
    char *neuf = dupstr(v);
    if (!neuf) hc_memoire_epuisee("délimiteur d'items");
    free(g_item_delim);
    g_item_delim = neuf;
}
/* Levé dès que le noyau change quelque chose de visible ; lu et remis à zéro
 * par l'hôte. Sans lui, cocoa_idle ne peut pas savoir s'il doit repeindre :
 * l'architecture reposait sur un redessin inconditionnel à chaque tour de
 * boucle, ce qui coûtait 16 ms d'attente même aux boucles de calcul pur. */


int hc_take_visual_dirty(void)
{
    int d = g_visual_dirty;
    g_visual_dirty = 0;
    return d;
}
/* Le séparateur d'un type de morceau, EN CHAÎNE.
 *
 * Il rendait un char, ce qui marchait tant que le délimiteur d'items en était
 * un. « sort items of "cébéa" » avec « é » pour délimiteur recollait donc le
 * résultat avec le seul octet \xc3 : « a\xc3b\xc3c », de l'UTF-8 invalide, et
 * « the number of items » retombait à un. Mesuré. */
static const char *chunk_sep(ChunkType t)
{
    if (t == CH_ITEM) return item_delim();
    if (t == CH_LINE) return "\n";
    if (t == CH_WORD) return " ";
    return "";
}

/* ---- les morceaux : délégués à hct_chunk ----
 *
 * Étape 1 de la reprise de l'interpréteur v3. Le découpage en morceaux est ce
 * qu'il y a de plus simple à confier : du texte entre, du texte sort, aucun
 * état partagé. Deux implémentations coexistaient, et chacune avait son
 * défaut.
 *
 * Celle-ci comptait « a,b, » pour DEUX items, retirant le morceau vide final
 * comme elle le fait — à juste titre — pour les lignes. HyperCard en compte
 * trois : un séparateur d'items final crée bien un item vide, alors qu'un
 * saut de ligne final ne crée pas de ligne. La dissymétrie est voulue, et
 * hct_chunk la respecte.
 *
 * Réciproquement, hct_chunk ne séparait les mots que sur l'espace et la
 * tabulation ; le saut de ligne y a été ajouté, faute de quoi les mots de
 * part et d'autre d'un retour se seraient collés.
 *
 * Le traducteur ci-dessous est la seule couture : les deux énumérations
 * décrivent la même chose dans un ordre différent. */
static HctSorteChunk vers_sorte_v3(ChunkType t)
{
    switch (t) {
        case CH_WORD: return HCT_CH_WORD;
        case CH_ITEM: return HCT_CH_ITEM;
        case CH_LINE: return HCT_CH_LINE;
        default:      return HCT_CH_CHAR;
    }
}

static int chunk_count(const char *s, ChunkType t)
{
    if (t == CH_NONE) return 0;
    return hct_chunk_compte(s, vers_sorte_v3(t), item_delim());
}

/* Bornes du n-ième morceau (1-based). Renvoie 0 s'il n'existe pas. */
static int chunk_span1(const char *s, ChunkType t, int n, int *b, int *e)
{
    if (t == CH_NONE) return 0;
    HctBornes r = hct_chunk_bornes(s, vers_sorte_v3(t), n, 0, item_delim());
    if (!r.trouve) return 0;
    *b = r.deb; *e = r.fin;
    return 1;
}

/* Bornes d'un intervalle a..b (b <= 0 : un seul morceau). */
static int chunk_span(const char *s, ChunkType t, int a, int b, int *st, int *en)
{
    int b1, e1, b2, e2;
    if (!chunk_span1(s, t, a, &b1, &e1)) return 0;
    if (b <= 0 || b == a) { *st = b1; *en = e1; return 1; }
    if (!chunk_span1(s, t, b, &b2, &e2)) { *st = b1; *en = (int)strlen(s); return 1; }
    *st = b1; *en = e2;
    return 1;
}

/* Décompose « [the] [ordinal] <type> [n [to m]] of <reste> ».
   Renvoie 1 si c'en est une ; `rest` pointe alors sur ce qui suit « of ». */
static int parse_chunk(const char *t, ChunkType *type,
                       char *ia, int lia, char *ib, int lib,
                       const char **rest, int *ordinal)
{
    static const char *ord[] = { "first", "second", "third", "fourth", "fifth",
                                 "sixth", "seventh", "eighth", "ninth", "tenth", NULL };
    const char *s = skip_spaces(t);
    ia[0] = ib[0] = '\0';
    *ordinal = 0;

    if (ci_word(s, "the")) s = skip_spaces(s + 3);

    for (int i = 0; ord[i]; i++)
        if (ci_word(s, ord[i])) { *ordinal = i + 1; s = skip_spaces(s + strlen(ord[i])); break; }
    if (!*ordinal) {
        if      (ci_word(s, "last"))   { *ordinal = -1; s = skip_spaces(s + 4); }
        else if (ci_word(s, "middle")) { *ordinal = -2; s = skip_spaces(s + 6); }
        else if (ci_word(s, "any"))    { *ordinal = -3; s = skip_spaces(s + 3); }
    }

    int used = 0;
    ChunkType ct = chunk_kind(s, &used);
    if (ct == CH_NONE) return 0;
    s = skip_spaces(s + used);

    const char *of = find_kw(s, "of");
    const char *in = find_kw(s, "in");
    if (in && (!of || in < of)) of = in;
    if (!of) return 0;

    if (!*ordinal) {
        /* indices explicites, éventuellement « a to b » */
        const char *to = find_kw(s, "to");
        if (to && to < of) {
            int n = (int)(to - s);
            if (n > lia - 1) n = lia - 1;
            memcpy(ia, s, (size_t)n); ia[n] = '\0';
            n = (int)(of - (to + 2));
            if (n > lib - 1) n = lib - 1;
            if (n < 0) n = 0;
            memcpy(ib, to + 2, (size_t)n); ib[n] = '\0';
        } else {
            int n = (int)(of - s);
            if (n > lia - 1) n = lia - 1;
            memcpy(ia, s, (size_t)n); ia[n] = '\0';
        }
        if (!*skip_spaces(ia)) return 0;   /* « word of x » n'a pas de sens */
    }

    *type = ct;
    *rest = of + 2;
    return 1;
}

/* Traduit ordinal/indices en bornes concrètes dans `src`. */
static void chunk_indices(const char *src, ChunkType ct, int ordinal,
                          const char *ia, const char *ib, int *a, int *b)
{
    *a = *b = 0;
    if (ordinal > 0)       { *a = ordinal; }
    else if (ordinal == -1) *a = chunk_count(src, ct);
    else if (ordinal == -2) *a = (chunk_count(src, ct) + 1) / 2;
    else if (ordinal == -3) {
        int n = chunk_count(src, ct);
        *a = n > 0 ? (rand() % n) + 1 : 0;
    } else {
        /* Bornés comme leurs jumeaux de la v3 : « (int)d » sur un double hors
         * plage est un comportement indéfini. */
        char v[128];
        eval_expr(ia, v, sizeof v); *a = hct_vers_rang(v, NULL);
        if (ib && *skip_spaces(ib)) {
            eval_expr(ib, v, sizeof v); *b = hct_vers_rang(v, NULL);
        }
    }
}

/* Lit un morceau. Renvoie 0 si `t` n'est pas une expression de morceau. */
static int chunk_read(const char *t, char *out, int outlen)
{
    ChunkType ct; char ia[128], ib[128]; const char *rest; int ordinal;
    if (!parse_chunk(t, &ct, ia, sizeof ia, ib, sizeof ib, &rest, &ordinal)) return 0;

    char *src = arena_buf();
    eval_expr(rest, src, HC_VAL);      /* récursif : les morceaux s'emboîtent */

    int a, b, st, en;
    chunk_indices(src, ct, ordinal, ia, ib, &a, &b);
    out[0] = '\0';
    if (chunk_span(src, ct, a, b, &st, &en)) {
        int len = en - st;
        if (len > outlen - 1) len = outlen - 1;
        if (len > 0) memcpy(out, src + st, (size_t)len);
        out[len > 0 ? len : 0] = '\0';
    }
    return 1;
}


/* ==================== plages de style ====================
 *
 * Une plage couvre [start, start+len) dans le texte du champ. Ce qu'aucune
 * plage ne couvre prend le style du champ entier. Les plages sont tenues
 * triées, sans recouvrement et sans trou vide : `runs_tidy` s'en charge après
 * chaque manipulation.
 */

/* Liste active d'un champ. Un champ de fond non partagé a un style PAR CARTE,
 * comme il a un texte par carte : sinon le gras posé sur une carte se
 * retrouverait sur toutes celles du même fond. */
static int field_is_percard(Object *field);   /* défini plus bas */

static struct RunList *runs_of(Object *field)
{
    if (!field || field->type != OBJ_FIELD) return NULL;
    if (field_is_percard(field) && g_current_card) {
        Object *cd = g_current_card;
        for (int i = 0; i < cd->nbgtexts; i++)
            if (cd->bgtexts[i].field_id == field->id)
                return &cd->bgtexts[i].runs;
        return NULL;          /* la carte n'a pas encore d'entrée : rien à styler */
    }
    return &field->runs;
}

static void runs_free(struct RunList *rl)
{
    if (!rl) return;
    for (int i = 0; i < rl->n; i++) free(rl->v[i].font);
    free(rl->v);
    rl->v = NULL; rl->n = rl->cap = 0;
}

int runs_room(struct RunList *rl, int need)
{
    if (rl->n + need <= rl->cap) return 1;
    int cap = rl->cap ? rl->cap * 2 : 8;
    while (cap < rl->n + need) cap *= 2;
    struct TextRun *v = (struct TextRun *)realloc(rl->v, (size_t)cap * sizeof *v);
    if (!v) return 0;
    rl->v = v; rl->cap = cap;
    return 1;
}

/* Une plage qui ne dit rien sur aucun des trois attributs : elle décrit
 * exactement le champ, autant ne pas la garder. Attention, `style == 0` n'est
 * PAS muet — c'est « plain », qui a le pouvoir d'effacer le gras du champ.
 * Seul HC_STYLE_INHERIT signifie « je ne me prononce pas ». */
static int run_is_mute(const struct TextRun *r)
{
    return r->style == HC_STYLE_INHERIT && r->size == 0 && !r->font &&
           r->color == HC_COLOR_INHERIT;
}

/* Deux plages voisines ne se fusionnent que si elles s'accordent sur les trois
 * attributs. Comparer le seul masque de style recollait « Geneva gras » et
 * « Monaco gras » en une plage, dont la police était celle de la première. */
static int run_same_attrs(const struct TextRun *a, const struct TextRun *b)
{
    /* La COULEUR compte, comme les trois autres attributs.
     *
     * Sans elle, runs_tidy refusionnait des plages voisines de couleurs
     * différentes : sur trois mots rouges, colorer celui du milieu en bleu
     * n'avait aucun effet visible — la plage était bien découpée, puis
     * aussitôt recollée parce que les deux morceaux paraissaient identiques.
     * Et c'est la couleur du PREMIER qui l'emportait, d'où « ça reste rouge ». */
    if (a->style != b->style || a->size != b->size) return 0;
    if (a->color != b->color) return 0;
    if (!a->font && !b->font) return 1;
    if (!a->font || !b->font) return 0;
    return strcmp(a->font, b->font) == 0;
}

static void runs_sort(struct RunList *rl)
{
    for (int i = 1; i < rl->n; i++) {                /* tri par insertion */
        struct TextRun t = rl->v[i];
        int k = i - 1;
        while (k >= 0 && rl->v[k].start > t.start) { rl->v[k+1] = rl->v[k]; k--; }
        rl->v[k+1] = t;
    }
}

/* Trie, jette les plages muettes, fusionne les voisines identiques. Appelé
 * après toute modification pour que la liste reste canonique — deux listes
 * équivalentes ont ainsi la même représentation. */
static void runs_tidy(struct RunList *rl)
{
    if (!rl) return;

    for (int i = 0; i < rl->n; ) {                   /* jeter les inutiles */
        if (rl->v[i].len <= 0 || run_is_mute(&rl->v[i])) {
            free(rl->v[i].font);
            for (int k = i; k + 1 < rl->n; k++) rl->v[k] = rl->v[k+1];
            rl->n--;
        } else i++;
    }
    runs_sort(rl);
    for (int i = 0; i + 1 < rl->n; ) {               /* fusionner les jointives */
        struct TextRun *a = &rl->v[i], *b = &rl->v[i+1];
        if (a->start + a->len == b->start && run_same_attrs(a, b)) {
            a->len += b->len;
            free(b->font);
            for (int k = i + 1; k + 1 < rl->n; k++) rl->v[k] = rl->v[k+1];
            rl->n--;
        } else i++;
    }
}

/* Recale les plages après une écriture : `oldlen` caractères à la position
 * `at` ont été remplacés par `newlen`. Les trois règles, vérifiées dans
 * HyperCard 2.4 :
 *   - plage entièrement recouverte  -> détruite
 *   - plage contenant `at`, ou finissant juste à `at` -> allongée
 *     (la frontière est collante : le caractère inséré hérite du style de
 *      son voisin de gauche, y compris juste après la fin d'une plage)
 *   - plage située après -> décalée de (newlen - oldlen)
 */
static void runs_edit(struct RunList *rl, int at, int oldlen, int newlen)
{
    if (!rl || rl->n == 0) return;
    int d = newlen - oldlen, end = at + oldlen;

    for (int i = 0; i < rl->n; i++) {
        struct TextRun *r = &rl->v[i];
        int rs = r->start, re = r->start + r->len;

        if (re < at)                { continue; }                 /* avant */
        if (rs >= end)              { r->start += d; continue; }  /* après */
        if (rs >= at && re <= end)  { r->len = 0; continue; }     /* recouverte */

        int keepL = (rs < at)  ? at - rs  : 0;                    /* survit à gauche */
        int keepR = (re > end) ? re - end : 0;                    /* survit à droite */

        if (keepL && !keepR)      { r->len = keepL + newlen; }    /* collante */
        else if (keepR && !keepL) { r->start = at + newlen; r->len = keepR; }
        else                      { r->len = keepL + newlen + keepR; }
    }
    runs_tidy(rl);
}

/* Quel(s) attribut(s) une écriture concerne. Les trois sont indépendants :
 * poser un style ne doit pas emporter la police avec lui. */
#define RA_STYLE 1
#define RA_SIZE  2
#define RA_FONT  4
#define RA_COLOR 8

static void run_apply(struct TextRun *r, int mask,
                      int style, int size, const char *font, int color)
{
    if (mask & RA_STYLE) r->style = style;
    if (mask & RA_SIZE)  r->size  = size;
    if (mask & RA_COLOR) r->color = color;
    if (mask & RA_FONT) {
        free(r->font);
        r->font = (font && *font) ? dupstr(font) : NULL;
    }
}

/* Coupe en deux la plage qui enjambe `pos`, s'il y en a une. Après un appel
 * en `start` puis en `end`, plus aucune plage ne chevauche la frontière :
 * chacune est entièrement dedans ou entièrement dehors. */
static int runs_split_at(struct RunList *rl, int pos)
{
    for (int i = 0; i < rl->n; i++) {
        struct TextRun *r = &rl->v[i];
        if (pos <= r->start || pos >= r->start + r->len) continue;

        if (!runs_room(rl, 1)) return 0;
        r = &rl->v[i];                            /* realloc a pu tout déplacer */
        struct TextRun tail = *r;
        tail.font  = r->font ? dupstr(r->font) : NULL;
        tail.start = pos;
        tail.len   = r->start + r->len - pos;
        r->len     = pos - r->start;
        rl->v[rl->n++] = tail;
        return 1;                                 /* les plages ne se recouvrent
                                                   * pas : une seule enjambe */
    }
    return 1;
}

/* Un canal de couleur, rabattu entre 0 et 255 comme l'écrêtage d'avant — mais
 * AVANT la conversion vers int, et non après : « %d » suivi d'un écrêtage
 * arrivait trop tard, le débordement avait déjà eu lieu et il est indéfini. */
static int canal(const char *s)
{
    char *fin = NULL;
    double d = strtod(s, &fin);
    if (fin == s || !(d == d)) return 0;      /* rien de lisible, ou NaN */
    if (d < 0.0)   return 0;
    if (d > 255.0) return 255;
    return (int)d;
}

/* Traduit un nom de couleur, ou « #RRGGBB », ou « r,v,b », en 0xRRGGBB.
 *
 * Les noms sont ceux qu'on écrit spontanément dans un script, en français
 * comme en anglais : une pile écrite ici doit rester lisible par qui la
 * relira. Renvoie HC_COLOR_INHERIT si le mot n'est pas une couleur — la plage
 * reste alors muette sur cet attribut, plutôt que de virer au noir. */
static int color_from_name_a(const char *v, int *alpha)
{
    if (alpha) *alpha = 255;
    if (!v || !*v) return HC_COLOR_INHERIT;
    while (*v == ' ' || *v == '\t') v++;

    static const struct { const char *nom; int rgb; } table[] = {
        { "black",   0x000000 }, { "noir",    0x000000 },
        { "white",   0xFFFFFF }, { "blanc",   0xFFFFFF },
        { "red",     0xFF0000 }, { "rouge",   0xFF0000 },
        { "green",   0x008000 }, { "vert",    0x008000 },
        { "blue",    0x0000FF }, { "bleu",    0x0000FF },
        { "yellow",  0xFFFF00 }, { "jaune",   0xFFFF00 },
        { "cyan",    0x00FFFF },
        { "magenta", 0xFF00FF },
        { "orange",  0xFF8000 },
        { "purple",  0x800080 }, { "violet",  0x800080 },
        { "brown",   0x804000 }, { "marron",  0x804000 },
        { "pink",    0xFFC0CB }, { "rose",    0xFFC0CB },
        { "gray",    0x808080 }, { "grey",    0x808080 }, { "gris", 0x808080 },
        /* Ajoutées quand la table est devenue celle de la PEINTURE aussi :
         * on nomme spontanément plus de couleurs quand on dessine que quand
         * on colore trois mots dans un champ. */
        { "turquoise", 0x40E0D0 },
        { "olive",     0x808000 },
        { "navy",      0x000080 }, { "marine",  0x000080 },
        { "gold",      0xFFD700 }, { "or",      0xFFD700 },
        { "silver",    0xC0C0C0 }, { "argent",  0xC0C0C0 },
        { "beige",     0xF5F5DC },
        { "indigo",    0x4B0082 },
        { "lime",      0x00FF00 },
    };
    for (unsigned i = 0; i < sizeof table / sizeof *table; i++)
        if (ci_equal(v, table[i].nom)) return table[i].rgb;

    /* strtol ne déborde pas — il SATURE à LONG_MAX —, mais le rabattre dans un
     * int est défini par l'implémentation, et « #FFFFFFFFFF » rendait alors
     * n'importe quelle couleur. On borne au domaine réel d'un RVB. */
    if (*v == '#') {
        long n = strtol(v + 1, NULL, 16);
        if (n < 0)          n = 0;
        if (n > 0xFFFFFF)   n = 0xFFFFFF;
        return (int)n;
    }

    /* « 255,128,0 » : la forme qu'emploient les scripts qui calculent leurs
     * couleurs, et celle que rend « the textColor ».
     *
     * « 255,128,0,64 » y ajoute l'opacité. Un quatrième nombre était jusqu'ici
     * lu puis JETÉ en silence : sscanf s'arrêtait à trois et rendait 3, donc
     * la conversion réussissait et l'on peignait opaque sans que rien ne le
     * dise. C'est le pire des cas — un script qui a l'air de marcher. */
    if (strchr(v, ',')) {
        /* Les quatre nombres se lisent en TEXTE puis par hc_entier, qui borne
         * avant de convertir : « %d » est indéfini sur ce qui dépasse un int,
         * et l'écrêtage qui suivait arrivait trop tard pour y changer quoi que
         * ce soit. Les bornes sont ici celles du canal, 0 à 255 ; hors d'elles
         * on prend le bout le plus proche, comme avant. */
        char q[4][32];
        int n = sscanf(v, "%31[^,],%31[^,],%31[^,],%31s",
                       q[0], q[1], q[2], q[3]);
        if (n >= 3) {
            int r = canal(q[0]), g = canal(q[1]), b = canal(q[2]);
            if (n >= 4 && alpha) *alpha = canal(q[3]);
            return (r << 16) | (g << 8) | b;
        }
    }
    if (isdigit((unsigned char)*v)) {
        long n = strtol(v, NULL, 0);
        if (n < 0)          n = 0;
        if (n > 0xFFFFFF)   n = 0xFFFFFF;
        return (int)n;
    }
    return HC_COLOR_INHERIT;
}

/* La forme sans opacité, pour tout ce qui n'en veut pas : les plages de
 * style d'un champ, qui n'ont pas de canal alpha. */
static int color_from_name(const char *v) { return color_from_name_a(v, NULL); }

/* Le même vocabulaire, ouvert à l'hôte.
 *
 * « set the paintColor to "vert" » doit comprendre exactement ce que comprend
 * « set the textColor to "vert" ». Deux tables de couleurs dans le même
 * programme, c'est la garantie qu'un jour l'une saura dire « turquoise » et
 * pas l'autre. */
int hc_color_from_name(const char *v) { return color_from_name(v); }

/* Avec l'opacité : `alpha` reçoit 0..255, et 255 quand la couleur n'en
 * mentionne pas. Seule la PEINTURE s'en sert — un calque a un canal alpha,
 * une plage de style de champ n'en a pas. */
int hc_color_from_name_alpha(const char *v, int *alpha)
{
    return color_from_name_a(v, alpha);
}

/* Pose un attribut sur [start, start+len) SANS toucher aux deux autres.
 *
 * L'ancienne version rasait toute plage recouverte pour en poser une neuve :
 * « set the textStyle of word 3 to bold » effaçait donc la police de ce mot.
 * On procède maintenant en trois temps : découper aux frontières, combler les
 * trous par des plages muettes pour que l'intervalle soit intégralement
 * couvert, puis n'écrire que l'attribut demandé sur chaque plage concernée. */
static int runs_set_attr(struct RunList *rl, int start, int len, int mask,
                         int style, int size, const char *font, int color)
{
    if (!rl || len <= 0 || start < 0) return 0;
    int end = start + len;

    if (!runs_split_at(rl, start)) return 0;
    if (!runs_split_at(rl, end))   return 0;
    runs_sort(rl);

    /* Combler : tout caractère de l'intervalle doit appartenir à une plage,
     * sinon l'attribut n'aurait nulle part où s'écrire. Les plages ajoutées
     * sont muettes — elles décrivent le champ — jusqu'à ce qu'on écrive
     * dedans juste après. */
    int cursor = start, n0 = rl->n;
    for (int i = 0; i < n0 && cursor < end; i++) {
        int rs = rl->v[i].start, re = rs + rl->v[i].len;
        if (re <= start) continue;
        if (rs >= end)   break;
        if (rs > cursor) {
            if (!runs_room(rl, 1)) return 0;
            struct TextRun g = { cursor, rs - cursor, HC_STYLE_INHERIT, 0, NULL, HC_COLOR_INHERIT };
            rl->v[rl->n++] = g;
        }
        if (re > cursor) cursor = re;
    }
    if (cursor < end) {
        if (!runs_room(rl, 1)) return 0;
        struct TextRun g = { cursor, end - cursor, HC_STYLE_INHERIT, 0, NULL, HC_COLOR_INHERIT };
        rl->v[rl->n++] = g;
    }

    for (int i = 0; i < rl->n; i++) {
        struct TextRun *r = &rl->v[i];
        if (r->start >= start && r->start + r->len <= end && r->len > 0)
            run_apply(r, mask, style, size, font, color);
    }

    runs_tidy(rl);
    return 1;
}

/* Style effectif de [start, start+len), sachant que ce qu'aucune plage ne
 * couvre vaut `dflt`. Renvoie HC_STYLE_MIXED si la plage n'est pas homogène —
 * c'est ce que le guide d'Apple appelle « mixed ». */
static int runs_get_style(struct RunList *rl, int start, int len, int dflt)
{
    if (len <= 0) return dflt;
    int first = -2;
    for (int c = start; c < start + len; c++) {
        int st = dflt;
        if (rl) {
            for (int i = 0; i < rl->n; i++)
                if (c >= rl->v[i].start && c < rl->v[i].start + rl->v[i].len) {
                    if (rl->v[i].style != HC_STYLE_INHERIT) st = rl->v[i].style;
                    break;
                }
        }
        if (first == -2) first = st;
        else if (st != first) return HC_STYLE_MIXED;
    }
    return first == -2 ? dflt : first;
}

/* Police effective de [start, start+len). Écrit « mixed » si la plage n'est
 * pas homogène, comme le fait la lecture du style. `dflt` est la police du
 * champ, qui s'applique partout où aucune plage ne se prononce. */
static void runs_get_font(struct RunList *rl, int start, int len,
                          const char *dflt, char *out, int outlen)
{
    if (!dflt) dflt = "";
    const char *first = NULL;
    for (int c = start; c < start + len; c++) {
        const char *fn = dflt;
        if (rl)
            for (int i = 0; i < rl->n; i++)
                if (c >= rl->v[i].start && c < rl->v[i].start + rl->v[i].len) {
                    if (rl->v[i].font) fn = rl->v[i].font;
                    break;
                }
        if (!first) first = fn;
        else if (strcmp(fn, first) != 0) { snprintf(out, outlen, "mixed"); return; }
    }
    snprintf(out, outlen, "%s", first ? first : dflt);
}

/* Corps effectif de [start, start+len). Renvoie -1 pour « mixed » : zéro est
 * déjà pris par « le champ n'a pas de taille explicite ». */
static int runs_get_size(struct RunList *rl, int start, int len, int dflt)
{
    int first = -2;
    for (int c = start; c < start + len; c++) {
        int sz = dflt;
        if (rl)
            for (int i = 0; i < rl->n; i++)
                if (c >= rl->v[i].start && c < rl->v[i].start + rl->v[i].len) {
                    if (rl->v[i].size) sz = rl->v[i].size;
                    break;
                }
        if (first == -2) first = sz;
        else if (sz != first) return -1;
    }
    return first == -2 ? dflt : first;
}

/* Couleur commune à [start, start+len), ou -2 si elle varie. HC_COLOR_INHERIT
 * si aucune plage ne se prononce. Même forme que runs_get_size : une lecture
 * sur un intervalle non homogène doit dire « mixed » plutôt que de choisir. */
static int runs_get_color(struct RunList *rl, int start, int len)
{
    int vu = 0, val = HC_COLOR_INHERIT;
    int end = start + len;
    for (int p = start; p < end; p++) {
        int c = HC_COLOR_INHERIT;
        if (rl) for (int i = 0; i < rl->n; i++) {
            struct TextRun *r = &rl->v[i];
            if (p >= r->start && p < r->start + r->len) { c = r->color; break; }
        }
        if (!vu) { val = c; vu = 1; }
        else if (c != val) return -2;
    }
    return val;
}

/* Un mot -> son bit. 0 si le mot n'est pas un nom de style (« plain » compris :
 * il ne vaut aucun bit, mais reste un nom légitime — voir style_is_names). */
static int style_bit_of_name(const char *w)
{
    if      (ci_equal(w, "bold"))      return HC_BOLD;
    else if (ci_equal(w, "italic"))    return HC_ITALIC;
    else if (ci_equal(w, "underline")) return HC_UNDERLINE;
    else if (ci_equal(w, "outline"))   return HC_OUTLINE;
    else if (ci_equal(w, "shadow"))    return HC_SHADOW;
    else if (ci_equal(w, "condensed") || ci_equal(w, "condense")
                                      || ci_equal(w, "condens")) return HC_CONDENSE;
    else if (ci_equal(w, "extend") || ci_equal(w, "extended")) return HC_EXTEND;
    else if (ci_equal(w, "group"))     return HC_GROUP;
    return 0;
}

/* « bold,condense » -> bits. `plain` n'est pas un bit mais l'absence de bits,
 * et il est écrasé par tout ce qui l'accompagne, comme le veut le guide.
 * Le guide écrit « condensed », le menu du Mac « Condense » et le script du
 * Calendrier « condense » : les trois sont acceptés. */
static int style_from_names(const char *s)
{
    int bits = 0;
    while (s && *s) {
        while (*s == ' ' || *s == '\t' || *s == ',' || *s == '"') s++;
        if (!*s) break;
        char w[32]; int k = 0;
        while (*s && *s != ',' && *s != ' ' && *s != '\t' && *s != '"'
               && k < (int)sizeof w - 1) w[k++] = *s++;
        w[k] = '\0';
        bits |= style_bit_of_name(w);
        /* « plain » et les mots inconnus n'ajoutent rien */
    }
    return bits;
}

/* Vrai si TOUS les mots de `s` nomment un style. C'est ce qui sépare la liste
 * de noms, qu'HyperCard écrit sans guillemets — « to bold,underline » — d'une
 * expression à évaluer — « to s & ",italic" ». Les guillemets ne sont pas des
 * séparateurs ici : une liste citée est traitée un cran plus haut. */
static int style_is_names(const char *s)
{
    int words = 0;
    while (s && *s) {
        while (*s == ' ' || *s == '\t' || *s == ',') s++;
        if (!*s) break;
        char w[32]; int k = 0;
        while (*s && *s != ',' && *s != ' ' && *s != '\t') {
            if (k < (int)sizeof w - 1) w[k++] = *s;
            s++;
        }
        w[k] = '\0';
        if (!style_bit_of_name(w) && !ci_equal(w, "plain")) return 0;
        words++;
    }
    return words > 0;
}

static void style_to_names(int bits, char *out, int outlen)
{
    static const struct { int b; const char *n; } T[] = {
        { HC_BOLD, "bold" }, { HC_ITALIC, "italic" }, { HC_UNDERLINE, "underline" },
        { HC_OUTLINE, "outline" }, { HC_SHADOW, "shadow" },
        { HC_CONDENSE, "condense" }, { HC_EXTEND, "extend" }, { HC_GROUP, "group" }
    };
    if (bits == HC_STYLE_MIXED) { snprintf(out, outlen, "mixed"); return; }
    int pos = 0;
    out[0] = '\0';
    for (int i = 0; i < (int)(sizeof T / sizeof T[0]); i++)
        if (bits & T[i].b)
            pos = ajoute_borne(out, outlen, pos, pos ? "," : "", T[i].n);
    if (!pos) snprintf(out, outlen, "plain");
}

/* Résout « <morceaux> of <champ> » en un intervalle ABSOLU de caractères dans
 * le texte du champ. container_set travaille en relatif à chaque niveau de sa
 * récursion, ce qui suffit pour écrire du texte mais pas pour situer une
 * plage de style. On refait donc la descente en cumulant les décalages.
 * Renvoie NULL si `ref` n'est pas un morceau de champ. */
static Object *chunk_target(const char *ref, int *st, int *en)
{
    ChunkType ct; char ia[128], ib[128]; const char *rest; int ordinal;
    if (!parse_chunk(ref, &ct, ia, sizeof ia, ib, sizeof ib, &rest, &ordinal))
        return NULL;

    Object *fld;
    int base_off = 0;
    const char *base;

    int inner_st, inner_en;
    Object *inner = chunk_target(rest, &inner_st, &inner_en);
    if (inner) {                                   /* morceau de morceau */
        fld = inner;
        base_off = inner_st;
        int len = inner_en - inner_st;
        if (len < 0) len = 0;
        /* Copie inévitable : il faut un sous-texte terminé par zéro. Le
         * morceau porte déjà sur un morceau, donc il est borné. */
        char *tampon = arena_buf();
        snprintf(tampon, HC_VAL, "%.*s", len, hc_field_text(fld) + inner_st);
        base = tampon;
    } else {
        fld = resolve(rest);
        if (!fld || fld->type != OBJ_FIELD) return NULL;
        /* Sans copie, comme v3_chunk_cible : le tampon faisait 64 Ko et un
         * champ peut porter bien davantage. C'est ce qui faisait échouer
         * « the textStyle of word 150 » dans un champ de 200 000 caractères,
         * alors que « the number of words » y comptait bien ses 200 mots. On
         * ne fait que LIRE ce texte. */
        base = hc_field_text(fld);
    }

    int a, b, s2, e2;
    chunk_indices(base, ct, ordinal, ia, ib, &a, &b);
    if (!chunk_span(base, ct, a, b, &s2, &e2)) return NULL;
    *st = base_off + s2;
    *en = base_off + e2;
    return fld;
}

/* Intervalle de la dernière écriture, posé par container_set et consommé par
 * hc_set_field_text : c'est le seul moyen de savoir si l'écriture portait sur
 * un morceau (les plages se recalent) ou sur le champ entier (elles meurent). */
static Object *g_edit_fld = NULL;
static int     g_edit_at = -1, g_edit_old = 0, g_edit_new = 0;

/* TOUTES LES RÉFÉRENCES INTERNES À UN OBJET QUI MEURT, EN UN SEUL ENDROIT.
 *
 * Appelée au tout début de hc_free. Elle n'appelle RIEN qui puisse déréférencer
 * `mort` : elle ne fait que comparer des adresses et remettre à zéro. En
 * particulier elle ne passe pas par hc_set_selection, qui préviendrait l'hôte
 * — celui-ci reçoit déjà object_gone trois lignes plus bas, et le prévenir
 * deux fois d'un même décès avec un pointeur en cours de libération serait
 * exactement le genre de finesse qui finit en plantage.
 *
 * La liste est celle des globales du noyau qui retiennent un Object* SANS EN
 * ÊTRE PROPRIÉTAIRES. Les propriétaires — g_stacks, g_clipboard, g_clip_bg_copy
 * — n'ont rien à faire ici : leur contenu ne meurt pas sous eux. */
static void oublie_objet_interne(Object *mort)
{
    if (!mort) return;

    /* la sélection de texte */
    if (g_sel_field == mort) { g_sel_field = NULL; g_sel_start = g_sel_len = 0; }

    /* le résultat de la dernière recherche */
    if (g_found_field == mort) {
        g_found_field = NULL;
        g_found_text[0] = '\0';
        g_found_line = g_found_start = g_found_len = 0;
    }
    if (g_found_card == mort) g_found_card = NULL;
    if (!g_found_field) g_found_montre = 0;

    /* la pile de navigation : on COMPACTE, sans quoi « pop card » descendrait
     * sur un trou. Tous les exemplaires partent, une carte pouvant être
     * empilée plusieurs fois. */
    {
        int k = 0;
        for (int i = 0; i < g_navtop; i++)
            if (g_navstack[i] != mort) g_navstack[k++] = g_navstack[i];
        g_navtop = k;
    }

    /* la carte courante : NULL est la seule valeur sûre. Les lecteurs la
     * testent déjà, puisqu'elle vaut NULL avant l'ouverture de la première
     * pile. */
    if (g_current_card == mort) g_current_card = NULL;

    /* les piles « start using », maillons de la chaîne de messages */
    {
        int k = 0;
        for (int i = 0; i < g_nusing; i++)
            if (g_using[i] != mort) g_using[k++] = g_using[i];
        g_nusing = k;
    }

    /* les champs à rafraîchir au déverrouillage de l'écran */
    {
        int k = 0;
        for (int i = 0; i < g_verrou_n; i++)
            if (g_verrou_champs[i] != mort) g_verrou_champs[k++] = g_verrou_champs[i];
        g_verrou_n = k;
    }

    /* le fond EMPRUNTÉ du presse-papiers, et la pile d'où il vient.
     *
     * Ces deux globales vivent dans hc_presse_papiers.c depuis qu'il est un
     * fichier à part, et elles y restent PRIVÉES : on lui demande d'oublier
     * plutôt que d'aller écrire dans son ventre. Un symbole partagé au lieu
     * de deux, et un seul endroit qui sache ce que le presse-papiers
     * retient. */
    hc_pp_oublie(mort);

    /* l'intervalle de la dernière écriture dans un champ */
    if (g_edit_fld == mort) { g_edit_fld = NULL; g_edit_at = -1; }

    /* `me` et `the target`. Le report de libération les protège pendant qu'un
     * gestionnaire tourne — c'est tout son objet — mais hors gestionnaire la
     * libération est immédiate et ces deux-là resteraient pendants. */
    if (g_me     == mort) g_me     = NULL;
    if (g_target == mort) g_target = NULL;
}

/* Écrit dans un conteneur : champ, variable, ou morceau de l'un des deux.
 * mode : 0 remplacer, 1 après, 2 avant, 3 supprimer.
 * L'appel est récursif, donc « word 2 of line 3 of field "notes" » marche.
 * Renvoie 1 si la destination a été reconnue.
 */
/* ═══ LES MENUS DE LA BARRE, CRÉÉS PAR SCRIPT ═══════════════════════════
 *
 * Une pile d'époque se donne ses propres menus :
 *
 *     create menu "3DEquations"
 *     put menuItems() into menu "3DEquations" with menuMsg menuMsgs()
 *
 * Chaque article porte SON message : choisir « Curves » envoie « goCurves »,
 * pas « doMenu "Curves" ». C'est ce qui distingue un menu de pile d'un menu
 * de l'application, et c'est pour cela que la liste des messages compte
 * autant que celle des articles.
 *
 * Le modèle vit ICI et non dans l'interface, pour trois raisons : un script
 * doit pouvoir l'interroger (« there is a menu "X" »), il se teste alors sans
 * Cocoa, et l'hôte n'a plus qu'à en construire le reflet. Un seul rappel le
 * prévient — menus_changed — et il relit tout par les accesseurs.
 *
 * Ces menus ne s'enregistrent PAS avec la pile : ils naissent d'un script et
 * meurent avec lui, comme dans HyperCard. C'est d'ailleurs pourquoi les
 * piles écrivent « if there is no menu "X" then createMenu » en tête de leur
 * openStack. */

#define HC_MENUS_MAX     16
#define HC_ARTICLES_MAX  64

/* Le nom d'un menu. Soixante-quatre octets, c'était court — et surtout la
 * troncature était SILENCIEUSE : un menu créé sous un nom trop long recevait
 * un nom amputé, et « menu "NomComplet" » ne le retrouvait plus jamais, puisque
 * menu_index compare le nom entier. Le script créait un menu et ne pouvait plus
 * le désigner.
 *
 * Deux-cent-cinquante-six laisse de la marge, et au-delà on REFUSE en le
 * disant, comme partout ailleurs ici. Un menu n'a pas de seconde forme — pas
 * de « menu id N » — donc contrairement à un descripteur d'objet il n'y a rien
 * sur quoi se rabattre : il ne reste qu'à prévenir. */
#define HC_MENU_NOM_MAX 256

/* Un chemin de fichier. PATH_MAX vaut 1024 sur macOS, et un seul composant
 * peut y prendre 255 octets — un dossier nommé par une phrase, ce qui arrive
 * tous les jours. */
#define HC_CHEMIN_MAX 1024

typedef struct {
    char  nom[HC_MENU_NOM_MAX];
    char *article[HC_ARTICLES_MAX];   /* le texte affiché             */
    char *message[HC_ARTICLES_MAX];   /* ce qu'on envoie, ou NULL     */
    char  actif[HC_ARTICLES_MAX];
    char  coche[HC_ARTICLES_MAX];     /* la marque à gauche du nom    */
    int   n;
    int   actif_menu;
} HcMenuBarre;

static HcMenuBarre g_menus[HC_MENUS_MAX];
static int         g_nmenus = 0;

static void menus_prevenir(void)
{
    if (g_host && g_host->menus_changed) g_host->menus_changed();
}

static int menu_index(const char *nom)
{
    if (!nom || !*nom) return -1;
    for (int i = 0; i < g_nmenus; i++)
        if (ci_equal(g_menus[i].nom, nom)) return i;
    return -1;
}

static void menu_vide(HcMenuBarre *m)
{
    for (int j = 0; j < m->n; j++) {
        free(m->article[j]); m->article[j] = NULL;
        free(m->message[j]); m->message[j] = NULL;
    }
    m->n = 0;
}

static int menu_creer(const char *nom)
{
    if (!nom || !*nom) return 0;
    if (menu_index(nom) >= 0) return 0;          /* déjà là */
    if (g_nmenus >= HC_MENUS_MAX) {
        emit(HC_ERR, "   !! trop de menus (%d au plus)", HC_MENUS_MAX);
        return 0;
    }
    if (strlen(nom) >= HC_MENU_NOM_MAX) {
        emit(HC_ERR, "   !! nom de menu trop long (%d caractères au plus)",
             HC_MENU_NOM_MAX - 1);
        return 0;
    }
    HcMenuBarre *m = &g_menus[g_nmenus++];
    memset(m, 0, sizeof *m);
    snprintf(m->nom, sizeof m->nom, "%s", nom);
    m->actif_menu = 1;
    menus_prevenir();
    return 1;
}

static int menu_supprimer(const char *nom)
{
    int i = menu_index(nom);
    if (i < 0) return 0;
    menu_vide(&g_menus[i]);
    for (int k = i; k < g_nmenus - 1; k++) g_menus[k] = g_menus[k + 1];
    g_nmenus--;
    menus_prevenir();
    return 1;
}

/* Découper une liste d'articles.
 *
 * HyperCard accepte le retour à la ligne ou la virgule. On choisit d'après ce
 * qu'on trouve : dès qu'il y a un retour, c'est lui qui sépare — sans quoi un
 * article contenant une virgule (« Export, s'il vous plaît ») serait coupé en
 * deux. Une liste d'une seule ligne se découpe aux virgules, comme le veut
 * l'usage.
 *
 * Les lignes VIDES comptent : la liste des messages est parallèle à celle des
 * articles, et un séparateur « - » n'a pas de message. Les sauter décalerait
 * tout le reste — chaque article enverrait le message du suivant.
 *
 * `deborde` dit qu'il RESTAIT du texte quand le plafond a été atteint. Sans
 * lui la troncature était muette : une liste de soixante-dix articles en
 * posait soixante-quatre et le script ne l'apprenait jamais. L'appelant en
 * fait ce qu'il veut — ici, refuser plutôt que d'amputer. */
static int liste_decoupe(const char *src, char sep, char **out, int max,
                         int *deborde)
{
    int n = 0;
    const char *p = src ? src : "";
    if (deborde) *deborde = 0;
    while (n < max) {
        const char *f = strchr(p, sep);
        int len = f ? (int)(f - p) : (int)strlen(p);
        while (len > 0 && (p[len-1] == '\r' || p[len-1] == ' ')) len--;
        char *t = malloc((size_t)len + 1);
        if (!t) break;
        memcpy(t, p, (size_t)len); t[len] = '\0';
        out[n++] = t;
        if (!f) return n;              /* c'était le dernier : rien ne reste */
        p = f + 1;
    }
    if (deborde) *deborde = 1;
    return n;
}

/* Poser des articles dans un menu.
 *
 * `pos` est l'indice où ils se posent, `remplace` le nombre d'articles déjà
 * là qu'ils chassent à partir de cet indice. Les trois prépositions
 * d'HyperTalk s'y ramènent, et c'est tout l'intérêt d'une seule fonction :
 *
 *     into   menu        pos = 0,   remplace = tout
 *     before menu        pos = 0,   remplace = 0
 *     after  menu        pos = n,   remplace = 0
 *     into   menuItem j  pos = j,   remplace = 1
 *     before menuItem j  pos = j,   remplace = 0
 *     after  menuItem j  pos = j+1, remplace = 0
 *
 * Les articles qui SURVIVENT gardent leur message, leur coche et leur état :
 * insérer en tête ne doit pas relever ce qui était désactivé plus bas. Seuls
 * les nouveaux naissent actifs et sans marque.
 *
 * Rend 0 — sans rien changer — si le résultat ne tiendrait pas. On refuse en
 * entier plutôt que de poser ce qui rentre : un menu à moitié écrit est plus
 * difficile à diagnostiquer qu'un menu inchangé et une erreur. */
static int menu_insere(int i, int pos, int remplace,
                       const char *articles, const char *messages)
{
    if (i < 0 || i >= g_nmenus) return 0;
    HcMenuBarre *m = &g_menus[i];

    if (pos < 0) pos = 0;
    if (pos > m->n) pos = m->n;
    if (remplace < 0) remplace = 0;
    if (pos + remplace > m->n) remplace = m->n - pos;

    char *art[HC_ARTICLES_MAX], *msg[HC_ARTICLES_MAX];
    int   deborde = 0;
    char  sep = strchr(articles ? articles : "", '\n') ? '\n' : ',';
    int   na  = liste_decoupe(articles, sep, art, HC_ARTICLES_MAX, &deborde);
    int   nm  = 0;

    if (messages && *messages) {
        char sepm = strchr(messages, '\n') ? '\n' : ',';
        /* Une liste de messages plus longue que celle des articles est sans
         * conséquence : le surplus se jette. Elle ne fait donc pas déborder. */
        nm = liste_decoupe(messages, sepm, msg, HC_ARTICLES_MAX, NULL);
    }

    int garde = m->n - remplace;
    if (deborde || garde + na > HC_ARTICLES_MAX) {
        emit(HC_ERR, "   !! trop d'articles de menu (%d au plus)",
             HC_ARTICLES_MAX);
        for (int k = 0; k < na; k++) free(art[k]);
        for (int k = 0; k < nm; k++) free(msg[k]);
        return 0;
    }

    for (int k = pos; k < pos + remplace; k++) {
        free(m->article[k]); m->article[k] = NULL;
        free(m->message[k]); m->message[k] = NULL;
    }

    int queue = m->n - (pos + remplace);
    if (queue > 0 && na != remplace) {
        size_t q = (size_t)queue;
        memmove(&m->article[pos + na], &m->article[pos + remplace],
                q * sizeof *m->article);
        memmove(&m->message[pos + na], &m->message[pos + remplace],
                q * sizeof *m->message);
        memmove(&m->actif[pos + na],   &m->actif[pos + remplace],
                q * sizeof *m->actif);
        memmove(&m->coche[pos + na],   &m->coche[pos + remplace],
                q * sizeof *m->coche);
    }

    for (int k = 0; k < na; k++) {
        m->article[pos + k] = art[k];
        m->message[pos + k] = (k < nm) ? msg[k] : NULL;
        m->actif[pos + k]   = 1;
        m->coche[pos + k]   = 0;
    }
    for (int k = na; k < nm; k++) free(msg[k]);   /* liste plus longue */

    /* La queue a pu descendre : les cases au-delà du nouveau compte tiennent
     * encore des pointeurs recopiés. menu_vide s'arrête à m->n et ne les
     * touchera pas, mais un double free n'attend qu'une boucle écrite sur
     * HC_ARTICLES_MAX. On ferme le piège plutôt que de compter dessus. */
    for (int k = garde + na; k < m->n; k++) {
        m->article[k] = NULL;
        m->message[k] = NULL;
    }

    m->n = garde + na;
    menus_prevenir();
    return 1;
}

static void menus_reset(void)
{
    for (int i = 0; i < g_nmenus; i++) menu_vide(&g_menus[i]);
    g_nmenus = 0;
    menus_prevenir();
}

/* ---- ce que l'hôte lit pour construire son reflet ---- */
int         hc_menu_nombre(void)          { return g_nmenus; }
const char *hc_menu_nom(int i)
{ return (i >= 0 && i < g_nmenus) ? g_menus[i].nom : NULL; }
int         hc_menu_est_actif(int i)
{ return (i >= 0 && i < g_nmenus) ? g_menus[i].actif_menu : 0; }
int         hc_menu_nb_articles(int i)
{ return (i >= 0 && i < g_nmenus) ? g_menus[i].n : 0; }
const char *hc_menu_article(int i, int j)
{ return (i >= 0 && i < g_nmenus && j >= 0 && j < g_menus[i].n)
         ? g_menus[i].article[j] : NULL; }
int         hc_menu_article_actif(int i, int j)
{ return (i >= 0 && i < g_nmenus && j >= 0 && j < g_menus[i].n)
         ? g_menus[i].actif[j] : 0; }
int         hc_menu_article_coche(int i, int j)
{ return (i >= 0 && i < g_nmenus && j >= 0 && j < g_menus[i].n)
         ? g_menus[i].coche[j] : 0; }

static int container_set_body(const char *ref, const char *val, int mode);

/* container_set est recursif : « char 2 of word 2 of me » se traite en trois
 * passes emboitees. Seule la PLUS EXTERNE connait l'intervalle que l'ecriture
 * vise reellement ; les suivantes voient un remplacement complet du morceau
 * englobant. Sans ce garde-fou, la note d'intervalle etait ecrasee par le
 * niveau interne et les plages de style se croyaient recouvertes. */
static int g_cset_depth = 0;

static int container_set(const char *ref, const char *val, int mode)
{
    if (g_cset_depth == 0) { g_edit_fld = NULL; g_edit_at = -1; }
    g_cset_depth++;
    int r = container_set_body(ref, val, mode);
    g_cset_depth--;
    return r;
}

/* La boîte de messages comme DESTINATION : « put x into msg »,
 * « put x into the message box ».
 *
 * Elle n'est ni un objet de la pile ni une variable, et rien ne la traitait.
 * « into msg » tombait donc sur la branche des variables et créait une
 * variable de ce nom : le message n'apparaissait nulle part, sans le moindre
 * signalement. « into the message box » n'y arrivait même pas — trois mots,
 * la branche n'en accepte qu'un — et donnait « destination invalide ».
 *
 * On ne prend « message » tout seul que suivi de « box » ou « window » : le
 * mot est un nom de variable trop banal pour le confisquer, et des scripts
 * s'en servent. « msg » ne désigne rien d'autre. C'est la même règle que
 * boite_message_ici() dans hct_expr.c ; les deux chemins doivent trancher
 * pareil, sinon un script change de sens selon l'exécuteur. */
static int est_boite_message(const char *ref)
{
    char m1[64], m2[64], m3[64];
    const char *p = next_word(ref, m1, sizeof m1);
    if (ci_equal(m1, "the")) p = next_word(p, m1, sizeof m1);
    if (!ci_equal(m1, "msg") && !ci_equal(m1, "message")) return 0;

    p = next_word(p, m2, sizeof m2);
    if (!m2[0]) return ci_equal(m1, "msg");            /* « msg » tout seul */
    if (!ci_equal(m2, "box") && !ci_equal(m2, "window")) return 0;
    next_word(p, m3, sizeof m3);
    return m3[0] == '\0';
}

static int container_set_body(const char *ref, const char *val, int mode)
{
    ChunkType ct; char ia[128], ib[128]; const char *rest; int ordinal;

    if (parse_chunk(ref, &ct, ia, sizeof ia, ib, sizeof ib, &rest, &ordinal)) {
        /* Noter l'intervalle absolu visé dans le champ, pour que les plages de
         * style puissent se recaler. Sans cette note, hc_set_field_text ne voit
         * qu'un texte entier remplacé et détruit tout le style. */
        if (g_cset_depth == 1) {
            int cst, cen;
            Object *cf = chunk_target(ref, &cst, &cen);
            if (cf) {
                g_edit_fld = cf;
                int vlen = (int)strlen(val);
                if      (mode == 1) { g_edit_at = cen; g_edit_old = 0; g_edit_new = vlen; }
                else if (mode == 2) { g_edit_at = cst; g_edit_old = 0; g_edit_new = vlen; }
                else if (mode == 3) {
                    /* Supprimer un morceau emporte aussi son separateur —
                     * « delete word 1 » retire « Sun » ET l'espace qui suit.
                     * Meme ajustement que plus bas, sinon les plages qui
                     * suivent se decalent d'un caractere de trop. */
                    const char *sep = chunk_sep(ct);
                    int ls = (int)strlen(sep);
                    const char *ft = hc_field_text(cf);
                    int fl = (int)strlen(ft);
                    /* Le separateur peut faire PLUSIEURS octets — « é » en
                     * fait deux — et meme plusieurs caracteres. On avance de
                     * sa longueur, pas d'un octet. */
                    if (ls) {
                        if      (cen + ls <= fl && memcmp(ft + cen, sep, (size_t)ls) == 0)
                            cen += ls;
                        else if (cst >= ls && memcmp(ft + cst - ls, sep, (size_t)ls) == 0)
                            cst -= ls;
                    }
                    g_edit_at = cst; g_edit_old = cen - cst; g_edit_new = 0;
                }
                else                { g_edit_at = cst; g_edit_old = cen - cst; g_edit_new = vlen; }
            }
        }
        char *base = arena_buf();
        eval_expr(rest, base, HC_VAL);

        int a, b, st, en;
        chunk_indices(base, ct, ordinal, ia, ib, &a, &b);

        const char *sepstr = chunk_sep(ct);
        size_t lsep = strlen(sepstr);
        char *neuf = arena_buf();

        if (!chunk_span(base, ct, a, b, &st, &en)) {
            if (mode == 3) return 1;            /* rien à supprimer */
            /* Le rang visé dépasse le contenu : compléter avec des éléments
             * vides jusqu'à ce rang, comme le fait HyperTalk. */
            if (lsep && a > 0) {
                /* On compte les separateurs PAR LEUR LONGUEUR : un « é » en
                 * vaut un, pas deux, et « -- » un aussi. */
                int have = 0;
                if (*base) {
                    have = 1;
                    for (const char *q = base; *q; )
                        if (strncmp(q, sepstr, lsep) == 0) { have++; q += lsep; }
                        else q++;
                }
                int need = (have == 0) ? (a - 1) : (a - have);
                int pos = 0;
                pos += snprintf(neuf + pos, HC_VAL - pos, "%s", base);
                for (int k = 0; k < need && pos + (int)lsep < (int)HC_VAL - 1; k++) {
                    memcpy(neuf + pos, sepstr, lsep); pos += (int)lsep;
                }
                neuf[pos] = '\0';
                snprintf(neuf + pos, HC_VAL - pos, "%s", val);
            } else {
                snprintf(neuf, HC_VAL, "%s%s%s", base,
                         (*base && lsep) ? sepstr : "", val);
            }
        } else {
            char *old = arena_buf();
            int len = en - st;
            if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
            if (len < 0) len = 0;
            memcpy(old, base + st, (size_t)len); old[len] = '\0';

            char *piece = arena_buf();
            if      (mode == 1) snprintf(piece, HC_VAL, "%s%s", old, val);
            else if (mode == 2) snprintf(piece, HC_VAL, "%s%s", val, old);
            else if (mode == 3) piece[0] = '\0';
            else                snprintf(piece, HC_VAL, "%s", val);

            if (mode == 3 && sepstr[0]) {       /* supprimer emporte un séparateur */
                int bl = (int)strlen(base);
                if      (en < bl && base[en] == sepstr[0]) en++;
                else if (st > 0  && base[st-1] == sepstr[0]) st--;
            }
            snprintf(neuf, HC_VAL, "%.*s%s%s", st, base, piece, base + en);
        }
        return container_set(rest, neuf, 0);
    }

    /* HC n'a pas de boîte persistante : la valeur part sur la sortie, comme
     * pour « put x » sans destination. `mode` est donc sans objet — on ne peut
     * rien ajouter à la suite de ce qui est déjà affiché. */
    if (est_boite_message(ref)) {
        if (mode != 3) emit(HC_MSG, "%s", val ? val : "");
        return 1;
    }

    char *merged = arena_buf();
    Object *o = resolve(ref);
    if (o && o->type == OBJ_FIELD) {
        /* passer par hc_field_text / hc_set_field_text : un champ de fond non
         * partagé a un texte propre à chaque carte */
        const char *old = hc_field_text(o);

        /* LE REMPLACEMENT SIMPLE NE RECOPIE PLUS.
         *
         * « snprintf(merged, HC_VAL, "%s", val) » pour ensuite poser merged
         * dans le champ ne servait à rien d'autre qu'à TRONQUER à soixante-
         * quatre kilo-octets. C'était le dernier des trois points de
         * troncature de « sort » : après avoir rendu la source et la
         * reconstruction dynamiques, le champ retombait quand même à 65 535
         * caractères ici.
         *
         * Les modes qui CONCATÈNENT gardent le tampon : la concaténation
         * illimitée demanderait une allocation dynamique à chaque « put
         * after », et ce n'est pas le chemin qui détruisait des données. */
        if (mode == 0) {
            hc_set_field_text(o, val ? val : "");
        } else {
            if      (mode == 1) snprintf(merged, HC_VAL, "%s%s", old, val);
            else if (mode == 2) snprintf(merged, HC_VAL, "%s%s", val, old);
            else                merged[0] = '\0';        /* mode 3 : effacer */
            hc_set_field_text(o, merged);
        }
        notify_field(o);
        return 1;
    }
    if (o) return 0;                            /* un bouton n'est pas un conteneur */

    char vname[128];
    const char *after = next_word(ref, vname, sizeof vname);
    if (vname[0] && vname[0] != '"' && !*skip_spaces(after)) {
        const char *old = var_get(vname);
        if (!old) old = "";
        /* Même chose pour une VARIABLE : le remplacement simple posait la
         * valeur telle quelle, pas une copie tronquée à HC_VAL. */
        if (mode == 0) {
            var_set(vname, val ? val : "");
        } else {
            if      (mode == 1) snprintf(merged, HC_VAL, "%s%s", old, val);
            else if (mode == 2) snprintf(merged, HC_VAL, "%s%s", val, old);
            else                merged[0] = '\0';        /* mode 3 : effacer */
            var_set(vname, merged);
        }
        return 1;
    }
    return 0;
}

/* ==================== fonctions intégrées ==================== */

/* Deux syntaxes, comme dans HyperTalk :
 *     the <fonction>              the date, the ticks
 *     the <fonction> of <expr>    the length of x
 *     <fonction>(<args>)          min(3,1,2), offset("b","abc")
 */

/* Découpe les arguments d'un appel : virgules de premier niveau seulement,
   les parenthèses et les guillemets protègent. */
static int split_args(const char *s, char args[][HC_VAL], int maxargs)
{
    int n = 0, depth = 0, inq = 0, len = 0;
    args[0][0] = '\0';
    for (const char *p = s; *p; p++) {
        if (*p == '"') inq = !inq;
        else if (!inq && *p == '(') depth++;
        else if (!inq && *p == ')') depth--;

        if (!inq && depth == 0 && *p == ',') {
            args[n][len] = '\0';
            if (++n >= maxargs) return n;
            len = 0; args[n][0] = '\0';
            continue;
        }
        /* HC_VAL - 1, et non 511 : args[][] fait HC_VAL. Le 511 était un
         * vestige d'un « char one[512] » remplacé par l'arène sans qu'on
         * enlève l'ancien plafond, et il coupait une expression un peu longue
         * au milieu. */
        if (len < HC_VAL - 1) args[n][len++] = *p;
    }
    args[n][len] = '\0';
    if (*skip_spaces(args[n])) n++;
    return n;
}

static time_t hc_maintenant(void);   /* l'horloge, gelable — définie plus bas */

static void format_date(char *out, int outlen, int mode)
{
    time_t now = hc_maintenant();
    struct tm *tm = localtime(&now);
    if (!tm) { out[0] = '\0'; return; }
    const char *fmt = "%m/%d/%y";
    if      (mode == 1) fmt = "%a, %b %d, %Y";     /* abbreviated date */
    else if (mode == 2) fmt = "%A, %B %d, %Y";     /* long date */
    else if (mode == 3) fmt = "%H:%M";             /* time */
    else if (mode == 4) fmt = "%H:%M:%S";          /* long time */
    strftime(out, (size_t)outlen, fmt, tm);
}

/* ==================== dates : analyse et mise en forme ==================== */

/* Tables en anglais, volontairement : HyperTalk n'est pas localisé, et un
 * script de 1990 compare ses résultats à « January » ou « Sun ». Passer par
 * strftime ferait dépendre le sens du script de la locale de la machine. */
static const char *k_month[12] = {
    "January","February","March","April","May","June",
    "July","August","September","October","November","December" };
static const char *k_day[7] = {
    "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday" };

/* Secondes du Macintosh : depuis le 1er janvier 1904, pas 1970. */
#define HC_MAC_EPOCH 2082844800LL

/* L'HEURE, EN UN SEUL POINT — et gelable.
 *
 * « the date », « the time », « the seconds » et le tirage aléatoire lisaient
 * tous time(NULL) directement. Un test qui affiche une date passe donc
 * aujourd'hui et échoue demain, ce qui revient à ne pas pouvoir le versionner.
 *
 * HC_HORLOGE, s'il porte un entier, le rend à la place : la suite de
 * non-régression devient reproductible, y compris l'an prochain. Sans lui,
 * rien ne change. Le semis du générateur aléatoire s'y accroche aussi, pour
 * que « random » soit reproductible sous le même réglage. */
static time_t hc_maintenant(void)
{
    static long fige = -1;
    if (fige == -1) {
        const char *e = getenv("HC_HORLOGE");
        fige = (e && *e) ? strtol(e, NULL, 10) : 0;
        if (fige < 0) fige = 0;
    }
    return fige ? (time_t)fige : time(NULL);
}

enum { DF_SECONDS, DF_DATEITEMS, DF_SHORTDATE, DF_LONGDATE,
       DF_ABBREVDATE, DF_SHORTTIME, DF_LONGTIME, DF_NONE };

static int name_index(const char *w, const char **tab, int n)
{
    for (int i = 0; i < n; i++) {
        if (ci_equal(w, tab[i])) return i;
        /* forme abrégée : les trois premières lettres suffisent, comme
         * « Aug » pour August ou « Wed » pour Wednesday. */
        if (strlen(w) == 3 && ci_nequal(w, tab[i], 3)) return i;
    }
    return -1;
}

/* Analyse une date, une heure, une liste dateItems ou des secondes.
 * Renvoie 1 si la chaîne a été comprise, et remplit *tm (normalisé par
 * mktime, donc avec le jour de la semaine correct).
 *
 * Le séparateur lève l'ambiguïté, comme chez Apple : les dateItems sont
 * toujours séparés par des virgules, la date courte toujours par des
 * barres obliques. « 2026,8,7 » est donc une année-mois-jour, tandis que
 * « 8/7/26 » est un mois-jour-année. */
static int jours_du_mois(int mon /* 0-11 */, int year)
{
    static const int t[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (mon < 0 || mon > 11) return 0;
    if (mon == 1)
        return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
    return t[mon];
}

/* La variante qui dit AUSSI si la chaîne était une date EXACTE.
 *
 * Les deux questions sont différentes et le restent :
 *
 *   « interprète ceci du mieux possible »  — ce que fait `convert`, qui doit
 *     accepter un 31 février et le normaliser en 3 mars ; c'est même tout
 *     l'intérêt de « add 1 to item 3 of d » suivi d'un convert.
 *   « ceci EST-il une date ? »             — ce que demande `is a date`, qui
 *     doit répondre non à 99/99/99.
 *
 * `strict` porte la seconde. Il vaut 1 seulement si rien d'incompris n'a été
 * rencontré ET si les composantes existent au calendrier — AVANT que mktime
 * ne les normalise, car après il est trop tard : tout devient valide. */
static int parse_datetime_ex(const char *s, struct tm *tm, int *strict)
{
    if (strict) *strict = 0;
    if (!s) return 0;
    int inconnu = 0;   /* un mot qui n'est ni AM/PM, ni un mois, ni un jour */

    /* Les nombres sont accumulés en LONG LONG, pas en int.
     *
     * Les secondes du Macintosh comptent depuis 1904 : elles ont dépassé
     * INT_MAX en 1972, et valent aujourd'hui près de quatre milliards. Sur un
     * int elles débordaient — comportement indéfini, en pratique une valeur
     * négative —, si bien que le test « nn == 1 && nums[0] > 100000 » plus bas
     * était faux et que la branche des secondes n'était JAMAIS prise. Toute
     * date passée en secondes était rejetée avec « date incomprise », y
     * compris celles que le noyau venait lui-même de produire : « put the
     * seconds into t » suivi de « convert t to dateItems » ne marchait pas,
     * alors que c'est l'idiome le plus courant pour dater quelque chose.
     *
     * Le plafond évite qu'une suite de chiffres démesurée déborde à son tour :
     * au-delà, ce n'est de toute façon plus une date. */
    long long nums[12];
    int nn = 0;
    int mon = -1, hh = -1, mi = 0, ss = 0, meridian = 0;  /* 1 = AM, 2 = PM */
    int sawslash = 0, sawcolon = 0, sawname = 0;

    const char *p = s;
    while (*p) {
        while (*p && !isalnum((unsigned char)*p)) {
            if (*p == '/') sawslash = 1;
            p++;
        }
        if (!*p) break;

        if (isdigit((unsigned char)*p)) {
            long long v = 0;
            while (isdigit((unsigned char)*p)) {
                if (v < 1000000000000LL) v = v * 10 + (*p - '0');
                p++;
            }
            if (*p == ':') {                       /* début d'une heure */
                sawcolon = 1;
                hh = (int)v; p++;
                mi = 0;
                while (isdigit((unsigned char)*p)) { mi = mi * 10 + (*p - '0'); p++; }
                if (*p == ':') {
                    p++; ss = 0;
                    while (isdigit((unsigned char)*p)) { ss = ss * 10 + (*p - '0'); p++; }
                }
            } else if (nn < 12) {
                nums[nn++] = v;
            }
        } else {
            char w[32]; int k = 0;
            while (isalpha((unsigned char)*p) && k < (int)sizeof w - 1) w[k++] = *p++;
            w[k] = '\0';
            if (ci_equal(w, "AM")) meridian = 1;
            else if (ci_equal(w, "PM")) meridian = 2;
            else {
                int m = name_index(w, k_month, 12);
                if (m >= 0) { mon = m; sawname = 1; }
                else if (name_index(w, k_day, 7) >= 0) sawname = 1;  /* jour : ignoré */
                else inconnu = 1;     /* « 12/25/96patate » : ce n'est pas une date */
            }
        }
    }

    memset(tm, 0, sizeof *tm);
    tm->tm_isdst = -1;

    /* --- secondes du Macintosh : un seul nombre, et il est énorme --- */
    if (!sawslash && !sawcolon && !sawname && nn == 1 && nums[0] > 100000) {
        time_t t = (time_t)(nums[0] - HC_MAC_EPOCH);
        struct tm *lt = localtime(&t);
        if (!lt) return 0;
        *tm = *lt;
        if (strict) *strict = 1;
        return 1;
    }

    int year = -1, day = -1;

    if (sawname) {                       /* « Friday, August 7, 2026 » */
        if (nn >= 1) day  = (int)nums[0];
        if (nn >= 2) year = (int)nums[1];
        if (mon < 0) return 0;           /* un nom de jour seul n'est pas une date */
        if (day < 0) day = 1;            /* « August » = le 1er août */
    } else if (sawslash) {               /* « 8/7/26 » : mois, jour, année */
        if (nn >= 1) mon  = (int)nums[0] - 1;
        if (nn >= 2) day  = (int)nums[1];
        if (nn >= 3) year = (int)nums[2];
    } else if (nn >= 3) {                /* dateItems : y, m, d, h, mn, s, dow */
        year = (int)nums[0]; mon = (int)nums[1] - 1; day = (int)nums[2];
        if (nn >= 4) hh = (int)nums[3];
        if (nn >= 5) mi = (int)nums[4];
        if (nn >= 6) ss = (int)nums[5];
        /* nums[6] est le jour de la semaine : recalculé, jamais lu */
    } else if (sawcolon) {               /* heure seule : on garde aujourd'hui */
        time_t now = hc_maintenant();
        struct tm *lt = localtime(&now);
        if (!lt) return 0;
        year = lt->tm_year + 1900; mon = lt->tm_mon; day = lt->tm_mday;
    } else {
        return 0;
    }

    if (mon < 0 || day < 0) return 0;
    if (year < 0) {                      /* année tue : celle en cours */
        time_t now = hc_maintenant();
        struct tm *lt = localtime(&now);
        if (!lt) return 0;
        year = lt->tm_year + 1900;
    }
    if (year < 100) year += (year < 70) ? 2000 : 1900;   /* comme le Macintosh */

    if (hh < 0) hh = 0;
    if (meridian == 2 && hh < 12) hh += 12;              /* 3 PM → 15 */
    if (meridian == 1 && hh == 12) hh = 0;               /* 12 AM → 0 */

    /* Le verdict strict se rend ICI, sur les composantes telles qu'elles ont
     * été lues. Après mktime, un 99/99/99 est devenu une date parfaitement
     * valide de 2007 et plus rien ne permet de dire qu'il n'en était pas une. */
    if (strict)
        *strict = !inconnu
               /* Une HEURE seule n'est pas une date, même si le code a comblé
                * le jour avec celui d'aujourd'hui pour pouvoir la convertir. */
               && (sawslash || sawname || nn >= 3)
               && mon >= 0 && mon <= 11
               && day >= 1 && day <= jours_du_mois(mon, year)
               && hh >= 0 && hh <= 23
               && mi >= 0 && mi <= 59
               && ss >= 0 && ss <= 59;

    tm->tm_year = year - 1900;
    tm->tm_mon  = mon;
    tm->tm_mday = day;
    tm->tm_hour = hh;
    tm->tm_min  = mi;
    tm->tm_sec  = ss;

    /* mktime normalise et remplit tm_wday. C'est lui qui fait marcher
     * « put 1 into item 3 of d » suivi de « convert d to dateItems » :
     * un 31 février devient un 3 mars, et le jour de la semaine suit. */
    if (mktime(tm) == (time_t)-1) return 0;
    return 1;
}

static int parse_datetime(const char *s, struct tm *tm)
{
    return parse_datetime_ex(s, tm, NULL);
}

/* « x is a date ». Une SEULE définition de ce qu'est une date : celle que
 * `convert` emploie, prise dans son acception stricte. Sans cela les deux se
 * contredisaient — la garde classique
 *
 *     if d is a date then convert d to seconds
 *
 * refusait « August 7, 2026 », que convert accepte très bien. */
int hc_est_date(const char *s)
{
    struct tm tm; int strict = 0;
    if (!parse_datetime_ex(s, &tm, &strict)) return 0;
    return strict;
}

/* Nom de format → constante DF_*. Renvoie DF_NONE si le mot est inconnu. */
static int date_format_code(const char *spec)
{
    const char *s = skip_spaces(spec);
    if (ci_word(s, "the")) s = skip_spaces(s + 3);

    int wantlong = 0, wantabbr = 0;
    for (;;) {
        if (ci_word(s, "long"))        { wantlong = 1; s = skip_spaces(s + 4); }
        else if (ci_word(s, "short"))  {               s = skip_spaces(s + 5); }
        else if (ci_word(s, "abbreviated")) { wantabbr = 1; s = skip_spaces(s + 11); }
        else if (ci_word(s, "abbrev")) { wantabbr = 1; s = skip_spaces(s + 6); }
        else if (ci_word(s, "abbr"))   { wantabbr = 1; s = skip_spaces(s + 4); }
        else if (ci_word(s, "english")){               s = skip_spaces(s + 7); }
        else break;
    }
    if (ci_word(s, "dateitems")) return DF_DATEITEMS;
    if (ci_word(s, "seconds") || ci_word(s, "secs")) return DF_SECONDS;
    if (ci_word(s, "date"))
        return wantlong ? DF_LONGDATE : wantabbr ? DF_ABBREVDATE : DF_SHORTDATE;
    if (ci_word(s, "time"))
        return wantlong ? DF_LONGTIME : DF_SHORTTIME;
    return DF_NONE;
}

static void emit_datetime(struct tm *tm, int fmt, char *out, int outlen)
{
    int h12 = tm->tm_hour % 12; if (h12 == 0) h12 = 12;
    const char *ampm = tm->tm_hour < 12 ? "AM" : "PM";

    switch (fmt) {
    case DF_DATEITEMS:
        snprintf(out, outlen, "%d,%d,%d,%d,%d,%d,%d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday + 1);
        break;
    case DF_SHORTDATE:
        snprintf(out, outlen, "%d/%d/%02d",
                 tm->tm_mon + 1, tm->tm_mday, (tm->tm_year + 1900) % 100);
        break;
    case DF_LONGDATE:
        snprintf(out, outlen, "%s, %s %d, %d", k_day[tm->tm_wday],
                 k_month[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
        break;
    case DF_ABBREVDATE:
        snprintf(out, outlen, "%.3s, %.3s %d, %d", k_day[tm->tm_wday],
                 k_month[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
        break;
    case DF_SHORTTIME:
        snprintf(out, outlen, "%d:%02d %s", h12, tm->tm_min, ampm);
        break;
    case DF_LONGTIME:
        snprintf(out, outlen, "%d:%02d:%02d %s", h12, tm->tm_min, tm->tm_sec, ampm);
        break;
    case DF_SECONDS: {
        struct tm copy = *tm;
        snprintf(out, outlen, "%lld", (long long)mktime(&copy) + HC_MAC_EPOCH);
        break;
    }
    default:
        out[0] = '\0';
    }
}

/* Renvoie 1 si `t` était bien un appel de fonction. */
static int call_function_body(const char *t, char *out, int outlen)
{
    v1_compte("v1 fonction", g_v1_porte);
    const char *s = skip_spaces(t);
    if (ci_word(s, "the")) s = skip_spaces(s + 3);

    /* formes composées : long date, short time, abbreviated date… */
    int datemode = -1;
    if (ci_word(s, "long")) {
        const char *w = skip_spaces(s + 4);
        if (ci_word(w, "date")) datemode = 2; else if (ci_word(w, "time")) datemode = 4;
    } else if (ci_word(s, "short")) {
        const char *w = skip_spaces(s + 5);
        if (ci_word(w, "date")) datemode = 0; else if (ci_word(w, "time")) datemode = 3;
    } else if (ci_word(s, "abbreviated") || ci_word(s, "abbrev") || ci_word(s, "abbr")) {
        const char *w = strchr(s, ' ');
        if (w && ci_word(skip_spaces(w), "date")) datemode = 1;
    }
    if (datemode >= 0) { format_date(out, outlen, datemode); return 1; }

    char name[64];
    const char *after = next_word(s, name, sizeof name);
    if (!name[0]) return 0;

    /* --- sans argument --- */
    if (!*skip_spaces(after)) {
        if (ci_equal(name, "date")) { format_date(out, outlen, 0); return 1; }
        if (ci_equal(name, "result")) { snprintf(out, outlen, "%s", g_result); return 1; }
        if (ci_equal(name, "foundtext")) { snprintf(out, outlen, "%s", g_found_text); return 1; }
        /* the stacksInUse : les bibliothèques déclarées, une par ligne, dans
         * l'ordre de déclaration. C'est ce que rend HyperCard, et ce qui
         * permet à un script de vérifier qu'une pile est bien en usage avant
         * d'appeler ses gestionnaires. */
        if (ci_equal(name, "stacksinuse")) {
            out[0] = '\0';
            size_t used = 0;
            for (int i = 0; i < g_nusing; i++) {
                const char *nm = g_using[i]->name ? g_using[i]->name : "";
                size_t l = strlen(nm);
                if (used + l + 2 >= (size_t)outlen) break;
                if (i) out[used++] = '\n';
                memcpy(out + used, nm, l); used += l;
                out[used] = '\0';
            }
            return 1;
        }
        if (ci_equal(name, "itemdelimiter")) {
            snprintf(out, outlen, "%s", item_delim()); return 1;
        }
        /* Le gabarit vide se rend tel quel : c'est ce que HyperCard rendait
         * avant qu'on y touche, et « if the numberFormat is empty » doit
         * pouvoir le constater. */
        if (ci_equal(name, "numberformat")) {
            snprintf(out, outlen, "%s", hct_format_nombre_lu()); return 1;
        }
        /* Les deux verrous se POSAIENT déjà — « lock screen », « set the
         * lockScreen to true » — mais ne se relisaient pas : « put the
         * lockScreen » rendait le mot « lockScreen », et personne ne s'en
         * apercevait puisque rien ne se plaignait. Depuis que « the » ne ment
         * plus, la même ligne lève une erreur ; la vraie réponse était de
         * toute façon due. Une propriété qu'on peut poser et pas relire est
         * une propriété à moitié. */
        if (ci_equal(name, "lockscreen")) {
            snprintf(out, outlen, "%s", g_ecran_verrouille ? "true" : "false");
            return 1;
        }
        if (ci_equal(name, "lockmessages")) {
            snprintf(out, outlen, "%s", g_messages_verrouilles ? "true" : "false");
            return 1;
        }
        if (reglage_lit(name, out, outlen)) return 1;
        /* the tool : l'outil courant, sous la forme « brush tool ». C'est
         * l'hôte qui le sait ; s'il ne répond pas, on annonce l'outil main,
         * celui d'HyperCard au repos. */
        if (ci_equal(name, "tool")) {
            const char *outil = host_global("tool");
            snprintf(out, outlen, "%s", (outil && *outil) ? outil : "browse tool");
            return 1;
        }
        if (ci_equal(name, "selection") || ci_equal(name, "selectedtext")) {
            selection_text(out, outlen); return 1;
        }
        if (ci_equal(name, "selectedfield")) {
            if (g_sel_field) hc_describe(g_sel_field, out, outlen);
            else snprintf(out, outlen, "%s", "");
            return 1;
        }
        /* Numéro de la ligne sélectionnée, ou vide. C'est ce que lisent les
         * sommaires pour savoir où aller ; le calculer ici évite à chaque
         * pile de le refaire à coups de « number of chars of line 1 to N ». */
        if (ci_equal(name, "selectedline")) {
            if (!g_sel_field) { snprintf(out, outlen, "%s", ""); return 1; }
            const char *texte = hc_field_text(g_sel_field);
            int line = 1;
            for (int i = 0; i < g_sel_start && texte[i]; i++)
                if (texte[i] == '\n') line++;
            snprintf(out, outlen, "%d", line);
            return 1;
        }
        /* ---- désignations de morceau ----
         * « char 5 to 12 of card field "notes" » : la forme qu'HyperCard rend
         * pour dire OÙ se trouve quelque chose. C'est une chaîne évaluable —
         * « put the value of the selectedChunk » relit le texte désigné — et
         * c'est ce qui permet à un script de retenir une position pour y
         * revenir plus tard.
         *
         * Les bornes sont en caractères, 1-based et inclusives : le contraire
         * des nôtres, qui sont 0-based et demi-ouvertes. D'où le +1 sur le
         * début et rien sur la fin. */
        if (ci_equal(name, "selectedchunk")) {
            if (!g_sel_field) { snprintf(out, outlen, "%s", ""); return 1; }
            char d[96];
            hc_describe(g_sel_field, d, sizeof d);
            snprintf(out, outlen, "char %d to %d of %s%s",
                     hct_utf8_compte_prefixe(hc_field_text(g_sel_field),
                                         g_sel_start) + 1,
                 hct_utf8_compte_prefixe(hc_field_text(g_sel_field),
                                         g_sel_start + g_sel_len),
                     hc_owner_is_bg(g_sel_field) ? "bg " : "card ", d);
            return 1;
        }
        if (ci_equal(name, "foundchunk")) {
            if (!g_found_field || g_found_len <= 0) {
                snprintf(out, outlen, "%s", ""); return 1;
            }
            char d[96];
            hc_describe(g_found_field, d, sizeof d);
            snprintf(out, outlen, "char %d to %d of %s%s",
                     hct_utf8_compte_prefixe(hc_field_text(g_found_field),
                                         g_found_start) + 1,
                 hct_utf8_compte_prefixe(hc_field_text(g_found_field),
                                         g_found_start + g_found_len),
                     hc_owner_is_bg(g_found_field) ? "bg " : "card ", d);
            return 1;
        }
        if (ci_equal(name, "foundfield")) {
            if (g_found_field) hc_describe(g_found_field, out, outlen);
            else snprintf(out, outlen, "%s", "");
            return 1;
        }
        if (ci_equal(name, "foundline")) {
            if (g_found_field && g_found_line > 0) {
                char d[96]; hc_describe(g_found_field, d, sizeof d);
                snprintf(out, outlen, "line %d of %s", g_found_line, d);
            } else snprintf(out, outlen, "%s", "");
            return 1;
        }
        if (ci_equal(name, "paramcount")) { snprintf(out, outlen, "%d", g_nparams - 1); return 1; }
        if (ci_equal(name, "params")) {
            /* tous les paramètres, nom du message inclus, séparés par des virgules */
            out[0] = '\0';
            int pos = 0;
            for (int i = 0; i < g_nparams; i++)
                pos = ajoute_borne(out, outlen, pos, i ? "," : "", g_params[i]);
            return 1;
        }
        if (ci_equal(name, "time")) { format_date(out, outlen, 3); return 1; }
        if (ci_equal(name, "seconds") || ci_equal(name, "secs")) {
            /* comme sur Macintosh : secondes depuis le 1er janvier 1904 */
            snprintf(out, outlen, "%lld", (long long)hc_maintenant() + 2082844800LL);
            return 1;
        }
        if (ci_equal(name, "ticks")) {
            /* Soixantièmes de seconde. D'abord l'hôte, s'il sait : lui seul a
             * une horloge fine et fiable.
             *
             * clock() mesurait le temps PROCESSEUR, pas le temps écoulé. Dans
             * une application graphique qui passe son temps à attendre
             * l'utilisateur, il n'avance presque pas — « the ticks » rendait 0
             * en boucle, et tout script comparant deux instants pour détecter
             * un double-clic voyait un écart nul, donc un double-clic à chaque
             * fois.
             *
             * Le repli compte depuis le PREMIER appel plutôt que depuis 1970 :
             * HyperCard comptait depuis le démarrage de la machine, et un
             * nombre qui reste petit évite d'éprouver l'arithmétique des
             * scripts sur des milliards. */
            const char *hv = host_global(name);
            if (hv && *hv) { snprintf(out, outlen, "%s", hv); return 1; }

            static time_t t0;
            static int t0_pris = 0;
            time_t now = hc_maintenant();
            if (!t0_pris) { t0 = now; t0_pris = 1; }
            snprintf(out, outlen, "%lld", (long long)(now - t0) * 60);
            return 1;
        }
    }

    /* --- arguments : « of <expr> » ou « (a, b, c) » --- */
    int nargs = 0;
    const char *q = skip_spaces(after);
    if (*q != '(' && !ci_word(q, "of")) return 0;

    char (*raw)[HC_VAL] = arena_rows(8);
    if (!raw) { out[0] = '\0'; return 1; }

    if (*q == '(') {
        const char *end = q + 1;
        int depth = 1, inq = 0;
        while (*end && depth) {
            if (*end == '"') inq = !inq;
            else if (!inq && *end == '(') depth++;
            else if (!inq && *end == ')') depth--;
            if (depth) end++;
        }
        char *inner = arena_buf();
        int len = (int)(end - (q + 1));
        if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
        if (len < 0) len = 0;
        memcpy(inner, q + 1, (size_t)len); inner[len] = '\0';
        nargs = split_args(inner, raw, 8);
    } else {
        snprintf(raw[0], sizeof raw[0], "%s", q + 2);
        nargs = 1;
    }

    char (*vals)[HC_VAL] = nargs ? arena_rows(nargs) : NULL;
    if (nargs && !vals) { out[0] = '\0'; return 1; }

    for (int i = 0; i < nargs; i++) eval_expr(raw[i], vals[i], sizeof vals[i]);

    double a = 0, b = 0;
    if (nargs > 0) as_num(vals[0], &a);
    if (nargs > 1) as_num(vals[1], &b);
    (void)b;

    /* --- une entrée --- */
    /* Des CARACTÈRES, pas des octets. Ce vieux pont n'est plus atteint que
     * par le recours, la v3 servant elle-même ces quatre fonctions — mais
     * deux implémentations qui divergent finissent toujours par se venger,
     * et il y a trois lignes à changer. */
    if (ci_equal(name, "length")) { snprintf(out, outlen, "%d", hct_utf8_compte(vals[0])); return 1; }
    if (ci_equal(name, "abs"))    { put_num(a < 0 ? -a : a, out, outlen); return 1; }
    /* trunc ET round PASSENT PAR LA BIBLIOTHEQUE, PAS PAR UN CAST.
     *
     * « (double)(long long)a » est un comportement INDEFINI dès que `a` sort
     * de la plage d'un long long — et as_num accepte « 1e300 » sans broncher.
     * Le chemin v3 a été corrigé en son temps ; ces trois-ci, dans l'ancien
     * moteur, étaient restés. Signalé par un audit extérieur.
     *
     * Je n'ai PAS su les atteindre : la v3 sert trunc, round, div, mod et
     * « is an integer » avant que l'ancien moteur n'en voie la couleur, et
     * quatre sondes n'ont pas trouvé d'entrée. On corrige quand même — une
     * conversion sûre ne coûte rien, et « je n'ai pas su l'atteindre » n'est
     * pas « c'est inatteignable ».
     *
     * trunc(3) tronque vers zéro, ce que faisait le cast ; le round
     * d'HyperTalk s'écarte de round(3) pour les négatifs à demi — il arrondit
     * -2.5 vers zéro, soit -2, quand round() rend -3 —, donc on garde la
     * formule d'origine et on remplace seulement la conversion. */
    if (ci_equal(name, "trunc"))  { put_num(trunc(a), out, outlen); return 1; }
    if (ci_equal(name, "round"))  { put_num(a < 0 ? -trunc(-a + 0.5)
                                                  :  trunc( a + 0.5), out, outlen); return 1; }
    if (ci_equal(name, "sqrt"))   { put_num(a >= 0 ? sqrt(a) : 0, out, outlen); return 1; }
    if (ci_equal(name, "exp"))    { put_num(exp(a), out, outlen); return 1; }
    if (ci_equal(name, "ln"))     { put_num(a > 0 ? log(a) : 0, out, outlen); return 1; }
    if (ci_equal(name, "log2"))   { put_num(a > 0 ? log(a) / log(2.0) : 0, out, outlen); return 1; }
    if (ci_equal(name, "sin"))    { put_num(sin(a), out, outlen); return 1; }
    if (ci_equal(name, "cos"))    { put_num(cos(a), out, outlen); return 1; }
    if (ci_equal(name, "tan"))    { put_num(tan(a), out, outlen); return 1; }
    if (ci_equal(name, "atan"))   { put_num(atan(a), out, outlen); return 1; }
    /* Variantes de précision d'HyperCard : exp1(x) = e^x − 1 et
     * ln1(x) = ln(1+x). Elles existent parce qu'aux alentours de zéro, calculer
     * exp(x)-1 fait perdre les chiffres significatifs — la soustraction annule
     * la partie utile du résultat. Les bibliothèques modernes fournissent
     * expm1 et log1p, qui font exactement cela. */
    if (ci_equal(name, "exp1"))   { put_num(expm1(a), out, outlen); return 1; }
    if (ci_equal(name, "exp2"))   { put_num(pow(2.0, a), out, outlen); return 1; }
    if (ci_equal(name, "ln1"))    { put_num(a > -1 ? log1p(a) : 0, out, outlen); return 1; }
    /* Ces trois-là convertissent un double en entier. Bornées comme leurs
     * jumelles de la v3 : sans le test, « random(10^300) » et
     * « numToChar(10^300) » convertissaient hors plage — comportement
     * indéfini. Ce chemin n'est plus l'ordinaire, mais il reste atteignable
     * par le recours, et un comportement indéfini ne se garde pas « au cas
     * où ». */
    if (ci_equal(name, "random")) {
        static int seeded = 0;
        if (!seeded) { srand((unsigned)hc_maintenant()); seeded = 1; }
        int n = (a >= 1 && a <= (double)HCT_RANG_MAX) ? (int)a : 0;
        snprintf(out, outlen, "%d", n > 0 ? (rand() % n) + 1 : 0); return 1;
    }
    if (ci_equal(name, "charToNum")) {
        const char *car = vals[0];      /* pas `t` : c'est un paramètre ici */
        int l = (int)strlen(car);
        long cp = 0;
        if (l > 0) {
            int nb = hct_utf8_octets(car, 0, l);
            unsigned char c0 = (unsigned char)car[0];
            cp = (nb == 1) ? c0 : (nb == 2) ? (c0 & 0x1F)
               : (nb == 3) ? (c0 & 0x0F) : (c0 & 0x07);
            for (int i = 1; i < nb; i++)
                cp = (cp << 6) | ((unsigned char)car[i] & 0x3F);
        }
        snprintf(out, outlen, "%ld", cp); return 1;
    }
    if (ci_equal(name, "numToChar")) {
        long code = (a >= 0 && a <= 0x10FFFF) ? (long)a : 0;
        if (code >= 0xD800 && code <= 0xDFFF) code = 0;
        char c[5]; int k = 0;
        if (code < 0x80) c[k++] = (char)code;
        else if (code < 0x800) {
            c[k++] = (char)(0xC0 | (code >> 6));
            c[k++] = (char)(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            c[k++] = (char)(0xE0 | (code >> 12));
            c[k++] = (char)(0x80 | ((code >> 6) & 0x3F));
            c[k++] = (char)(0x80 | (code & 0x3F));
        } else {
            c[k++] = (char)(0xF0 | (code >> 18));
            c[k++] = (char)(0x80 | ((code >> 12) & 0x3F));
            c[k++] = (char)(0x80 | ((code >> 6) & 0x3F));
            c[k++] = (char)(0x80 | (code & 0x3F));
        }
        c[k] = 0;
        snprintf(out, outlen, "%s", c); return 1;
    }

    /* value() : évalue une chaîne comme une expression. Le petit vertige
       d'HyperTalk — du texte qui redevient du calcul. */
    if (ci_equal(name, "value")) { eval_expr(vals[0], out, outlen); return 1; }

    /* param(n) : le n-ième paramètre. param(0) est le nom du message. */
    if (ci_equal(name, "param")) {
        int i = (int)a;
        snprintf(out, outlen, "%s", (i >= 0 && i < g_nparams) ? g_params[i] : "");
        return 1;
    }

    /* --- deux entrées --- */
    /* Fonctions financières d'HyperCard. Elles paraissent exotiques, mais les
     * piles de gestion des années 90 en sont truffées — calculs de prêts, de
     * placements. Deux formules, rien de plus :
     *   annuity(taux, périodes)  = (1 − (1+taux)^−n) / taux
     *   compound(taux, périodes) = (1+taux)^n
     * Le taux nul est un cas limite légitime : l'annuité vaut alors le nombre
     * de périodes, et la division ferait une erreur. */
    if (ci_equal(name, "annuity") && nargs >= 2) {
        double taux = 0, n = 0;
        as_num(vals[0], &taux); as_num(vals[1], &n);
        put_num(taux == 0 ? n : (1.0 - pow(1.0 + taux, -n)) / taux, out, outlen);
        return 1;
    }
    if (ci_equal(name, "compound") && nargs >= 2) {
        double taux = 0, n = 0;
        as_num(vals[0], &taux); as_num(vals[1], &n);
        put_num(pow(1.0 + taux, n), out, outlen);
        return 1;
    }

    if (ci_equal(name, "offset")) {
        int pos = 0, nl = (int)strlen(vals[0]), hl = (int)strlen(vals[1]);
        /* En caractères, comme la version v3 : les deux doivent s'accorder. */
        for (int i = 0, k = 0; nl && i + nl <= hl;
             i += hct_utf8_octets(vals[1], i, hl), k++)
            if (ci_nequal(vals[1] + i, vals[0], nl)) { pos = k + 1; break; }
        snprintf(out, outlen, "%d", pos);
        return 1;
    }

    /* --- nombre variable d'entrées --- */
    if (ci_equal(name, "min") || ci_equal(name, "max") ||
        ci_equal(name, "sum") || ci_equal(name, "average") || ci_equal(name, "avg")) {
        if (nargs == 0) { snprintf(out, outlen, "0"); return 1; }
        int wantmin = ci_equal(name, "min");
        double acc = 0, best = 0;
        for (int i = 0; i < nargs; i++) {
            double v = 0;
            as_num(vals[i], &v);
            acc += v;
            if (i == 0 || (wantmin ? v < best : v > best)) best = v;
        }
        if      (ci_equal(name, "sum")) put_num(acc, out, outlen);
        else if (ci_equal(name, "min") || ci_equal(name, "max")) put_num(best, out, outlen);
        else                            put_num(acc / nargs, out, outlen);
        return 1;
    }

    /* --- fonction définie par l'utilisateur ---
     * Dernier recours, après tous les noms intégrés : une pile qui définit
     * « function length » ne doit pas masquer celle du noyau, comme dans
     * HyperCard. La recherche part de l'objet dont le script tourne et
     * remonte la chaîne carte → fond → pile. */
    {
        Object *from = g_me ? g_me : g_current_card;
        char (*uargv)[HC_VAL] = nargs ? arena_rows(nargs) : NULL;
        if (nargs && !uargv) { out[0] = '\0'; return 1; }
        for (int i = 0; i < nargs; i++)
            snprintf(uargv[i], sizeof uargv[i], "%s", vals[i]);
        if (hc_call_user_function(from, name, uargv, nargs)) {
            snprintf(out, outlen, "%s", g_result);
            return 1;
        }
    }

    return 0;
}

static int call_function(const char *t, char *out, int outlen)
{
    ARENA_MARK;
    int r = call_function_body(t, out, outlen);
    ARENA_FREE;
    return r;
}

/* ==================== propriétés géométriques ==================== */

/* La pile qui contient cet objet (l'objet lui-même si c'en est une). */
Object *owning_stack(Object *o)
{
    while (o && o->type != OBJ_STACK) o = o->owner;
    return o;
}

/* Rectangle effectif. Une carte ou un fond n'a pas de géométrie propre :
 * c'est celle de sa pile, comme dans HyperCard, où « the rect of this card »
 * vaut « 0,0,largeur,hauteur ». Une pile sans taille explicite garde le
 * format d'origine du Macintosh, 512×342. */
static void obj_rect(Object *o, int *L, int *T, int *R, int *B)
{
    if (o->type == OBJ_CARD || o->type == OBJ_BACKGROUND || o->type == OBJ_STACK) {
        Object *st = owning_stack(o);
        *L = 0; *T = 0;
        *R = (st && st->w > 0) ? st->w : 512;
        *B = (st && st->h > 0) ? st->h : 342;
        return;
    }
    *L = o->x; *T = o->y; *R = o->x + o->w; *B = o->y + o->h;
}

/* Lit une propriété géométrique dans `out`. Renvoie 0 si `prop` n'en est pas. */
static int geom_read(Object *o, const char *prop, char *out, int outlen)
{
    int L, T, R, B;
    obj_rect(o, &L, &T, &R, &B);
    if (ci_equal(prop, "rect") || ci_equal(prop, "rectangle"))
        snprintf(out, outlen, "%d,%d,%d,%d", L, T, R, B);
    else if (ci_equal(prop, "topleft"))   snprintf(out, outlen, "%d,%d", L, T);
    else if (ci_equal(prop, "botright") || ci_equal(prop, "bottomright"))
                                          snprintf(out, outlen, "%d,%d", R, B);
    else if (ci_equal(prop, "left"))      snprintf(out, outlen, "%d", L);
    else if (ci_equal(prop, "top"))       snprintf(out, outlen, "%d", T);
    else if (ci_equal(prop, "right"))     snprintf(out, outlen, "%d", R);
    else if (ci_equal(prop, "bottom"))    snprintf(out, outlen, "%d", B);
    else if (ci_equal(prop, "width"))     snprintf(out, outlen, "%d", R - L);
    else if (ci_equal(prop, "height"))    snprintf(out, outlen, "%d", B - T);
    else if (ci_equal(prop, "loc") || ci_equal(prop, "location"))
                                          snprintf(out, outlen, "%d,%d", (L + R) / 2, (T + B) / 2);
    else return 0;
    return 1;
}

/* n premiers entiers d'une chaîne « a,b,c,d ». Renvoie le compte lu. */
static int parse_ints(const char *s, int *v, int maxn)
{
    int n = 0;
    while (*s && n < maxn) {
        while (*s == ' ' || *s == '\t' || *s == ',') s++;
        if (!*s) break;
        v[n++] = coord_champ(s, 0);
        while (*s && *s != ',') s++;
    }
    return n;
}

/* Écrit une propriété géométrique depuis une chaîne. Renvoie 0 si pas géométrique. */
static int geom_write(Object *o, const char *prop, const char *val)
{
    int p[4];
    /* Redimensionner une carte ou un fond, c'est redimensionner la pile :
     * c'est elle qui porte la taille, et l'interface la relit de là. */
    if (o->type == OBJ_CARD || o->type == OBJ_BACKGROUND) {
        Object *st = owning_stack(o);
        if (st) o = st;
    }
    if (ci_equal(prop, "rect") || ci_equal(prop, "rectangle")) {
        if (parse_ints(val, p, 4) == 4) { o->x = p[0]; o->y = p[1]; o->w = p[2]-p[0]; o->h = p[3]-p[1]; }
    } else if (ci_equal(prop, "topleft")) {
        if (parse_ints(val, p, 2) == 2) { o->w += o->x - p[0]; o->h += o->y - p[1]; o->x = p[0]; o->y = p[1]; }
    } else if (ci_equal(prop, "botright") || ci_equal(prop, "bottomright")) {
        if (parse_ints(val, p, 2) == 2) { o->w = p[0] - o->x; o->h = p[1] - o->y; }
    /* LES GÉOMÉTRIES PASSENT PAR hc_coord, ET NON PAR atoi.
     *
     * « set the width of card field "x" to 1e2 » donnait UN : atoi s'arrête au
     * « e ». Le même texte vaut mille dans « put 1e3 + 0 ». Un langage qui
     * répond deux choses différentes au même nombre selon le chemin qui le lit
     * est un langage dans lequel on ne peut pas raisonner.
     *
     * La valeur est bornée au passage : une largeur de deux milliards n'a pas
     * de sens et déborde dès qu'on l'additionne à une abscisse. En cas de
     * refus on garde la valeur actuelle plutôt que d'en inventer une. */
    } else if (ci_equal(prop, "left"))   { o->x = hc_coord(val, o->x); }
    else if (ci_equal(prop, "top"))      { o->y = hc_coord(val, o->y); }
    else if (ci_equal(prop, "right"))    { o->w = hc_coord(val, o->x + o->w) - o->x; }
    else if (ci_equal(prop, "bottom"))   { o->h = hc_coord(val, o->y + o->h) - o->y; }
    else if (ci_equal(prop, "width"))    { o->w = hc_coord(val, o->w); }
    else if (ci_equal(prop, "height"))   { o->h = hc_coord(val, o->h); }
    else if (ci_equal(prop, "loc") || ci_equal(prop, "location")) {
        if (parse_ints(val, p, 2) == 2) { o->x = p[0] - o->w/2; o->y = p[1] - o->h/2; }
    } else return 0;
    if (o->w < 0) o->w = 0;
    if (o->h < 0) o->h = 0;
    return 1;
}

/* Le mot est-il un nom de propriété ? Sert à accepter « loc of me » sans
 * « the ». La liste couvre exactement les propriétés que sait lire la
 * branche ci-dessous : y ajouter un nom sans l'ajouter là serait un piège. */
/* ---- résolution d'un nom d'icône ----
 * Déposé par la couche Cocoa au démarrage : ce noyau ne connaît ni le
 * catalogue d'icônes compilé dans l'application ni les en-têtes qui le
 * décrivent. Voir hc_core.h. */
static HCIconResolver gIconResolver = NULL;

void hc_set_icon_resolver(HCIconResolver fn) { gIconResolver = fn; }

/* Second crochet, même raison : transplanter des icônes d'une pile à l'autre
 * demande un numéro libre, et « libre » doit l'être aussi dans le catalogue
 * compilé dans l'application, que ce noyau ne connaît pas. Sans crochet on ne
 * vérifie que la pile — au pire on masque une icône d'origine. */
static HCIconTakenFn gIconBuiltinCheck = NULL;

void hc_set_icon_builtin_check(HCIconTakenFn fn) { gIconBuiltinCheck = fn; }

int icon_id_is_builtin(int id)
{
    return gIconBuiltinCheck ? gIconBuiltinCheck(id) : 0;
}

int hc_resolve_icon(const char *text)
{
    if (!text) return 0;
    if (gIconResolver) return gIconResolver(text);
    return hc_entier(text, -HC_ID_MAX, HC_ID_MAX, 0);   /* sans résolveur : les numéros seulement */
}

static int is_prop_name(const char *w, int len)
{
    static const char *tab[] = {
        "rect", "rectangle", "topleft", "botright", "bottomright",
        "left", "top", "right", "bottom", "width", "height",
        "loc", "location", "id", "name", "visible", "showname", "shownname",
        "enabled", "owner", "size", "freesize", "family", "titlewidth",
        "icon", "selectedline", "selectedlines", "locktext", "widemargins",
        "fixedlineheight", "showlines", "autotab", "dontsearch", "cantdelete",
        "sharedtext",
        "sharedhilite",
        "textalign", "autoselect", "multiplelines", "dontwrap", "textcolor",
        "marked",
        "selectedtext", "selectedchunk",
        "textfont", "scroll", "textstyle", "hilite", "highlight", "autohilite",
        "textsize", "textheight", "script", "text", "contents", "style",
        "partnumber", NULL
    };
    for (int i = 0; tab[i]; i++)
        if ((int)strlen(tab[i]) == len && ci_nequal(w, tab[i], len)) return 1;
    return 0;
}

/* « <propriété> of … » sans « the » devant ? */
static int prop_word_before_of(const char *t)
{
    const char *w = skip_spaces(t);
    const char *q = w;
    while (*q && !isspace((unsigned char)*q)) q++;
    if (q == w) return 0;
    if (!is_prop_name(w, (int)(q - w))) return 0;
    return ci_word(skip_spaces(q), "of");
}

/* Lecture d'une propriété sur un objet DÉJÀ RÉSOLU.
 *
 * Extrait tel quel de term_value, sans une ligne de changement : c'était le
 * seul lecteur de propriétés du programme, et il n'était atteignable qu'en
 * lui donnant du TEXTE à réanalyser. La v3 tient l'objet, pas son nom — il
 * lui fallait donc la même chaîne de tests, mais prise par l'autre bout.
 *
 * Rend 1 si `prop` a été reconnue et `out` renseigné, 0 sinon — auquel cas
 * l'appelant poursuit comme avant.
 *
 * `forme` est HC_NOM_COURT / ABREGE / LONG : seul `name` s'en sert. Il valait
 * un simple booléen « court ou non », si bien que « long » était lu, reconnu,
 * puis JETÉ — « the long name of me » rendait exactement « the name of me ». */
static int obj_prop_read(Object *o, const char *prop, int forme,
                         char *out, int outlen)
{
    if (geom_read(o, prop, out, outlen)) return 1;
    if (ci_equal(prop, "id"))      { snprintf(out, outlen, "%d", o->id); return 1; }
    /* « the number of this card », « the number of card field "x" » : le RANG
     * de l'objet parmi ses semblables, à ne pas confondre avec le comptage
     * qu'est « the number of cards ».
     *
     * term_value le traite bien avant d'arriver ici, mais l'évaluateur v3,
     * lui, passe par lit_prop : sans cette entrée, « the number of this
     * card » repartait à chaque fois vers l'ancien interpréteur par
     * reconstitution du texte source. */
    if (ci_equal(prop, "number")) {
        if (o->type == OBJ_CARD) {
            int n = card_index(o->owner, o);
            if (n < 0) return 0;
            snprintf(out, outlen, "%d", n + 1);
            return 1;
        }
        if (o->type == OBJ_BUTTON || o->type == OBJ_FIELD) {
            int n = hc_object_number(o);
            if (n <= 0) return 0;
            snprintf(out, outlen, "%d", n);
            return 1;
        }
        return 0;
    }
    if (ci_equal(prop, "name")) {
        hc_nom_de(o, forme, out, outlen);
        return 1;
    }
    /* « the owner of X » : le nom de l'objet qui le contient.
     *
     * Elle ne rendait rien — ni valeur ni erreur. Le recours reconstituait le
     * texte, l'ancien moteur ne la connaissait pas davantage, et la règle
     * « mot inconnu = son propre nom » rendait la chaîne « owner of me ». Un
     * script qui testait « if the owner of me is ... » comparait donc deux
     * textes et se trompait en silence. Mesurée au relevé du corpus, c'est
     * l'une des quatre propriétés encore dans ce cas.
     *
     * LA HIÉRARCHIE N'EST PAS CELLE DU MODÈLE. Chez nous card->owner est la
     * PILE et card->bg le fond ; pour HyperCard le propriétaire d'une carte
     * est son FOND. On suit HyperCard, qui décrit la superposition telle que
     * l'utilisateur la voit, et non notre chaînage interne.
     *
     * `forme` s'applique comme pour `name` : « the short owner of me » rend
     * « Une », « the owner of me » le descripteur long. Une pile n'a pas de
     * propriétaire et rend vide — pas une erreur : c'est le haut de la
     * hiérarchie, et « the owner of this stack » est une question légitime
     * dont la réponse juste est « rien ». */
    if (ci_equal(prop, "owner")) {
        Object *pro = (o->type == OBJ_CARD) ? o->bg : o->owner;
        if (!pro) { snprintf(out, outlen, "%s", ""); return 1; }
        /* SANS ADJECTIF, LE NOM LONG — et c'est le seul point de cette
         * propriété que je n'ai pas pu vérifier ici.
         *
         * HyperCard rend, d'après sa documentation, le nom long : « card id
         * 3517 of stack "Home" ». Je n'ai pas HyperCard sous la main pour le
         * confirmer, et le contraire se défend — `name` rend l'abrégé sans
         * adjectif, et l'on pourrait vouloir la même règle ici.
         *
         * J'ai tranché pour le long parce qu'un nom long se raccourcit dans
         * le script (« the short name of the owner of me ») alors qu'un nom
         * abrégé a PERDU la pile et ne se rallonge pas. Entre deux lectures
         * possibles, celle qui conserve l'information.
         *
         * LA VERRUE, dite plutôt que tue : l'analyseur ne distingue pas
         * « the owner » de « the abbr owner » — les deux arrivent en
         * HC_NOM_ABREGE. « the abbr owner of me » rend donc le nom long lui
         * aussi. « short » et « long » explicites, eux, sont respectés. */
        hc_nom_de(pro, forme == HC_NOM_ABREGE ? HC_NOM_LONG : forme,
                  out, outlen);
        return 1;
    }
    /* « the size of this stack » : la taille du FICHIER, en octets.
     *
     * La fonction du monde « the size » la servait déjà pour la pile
     * courante ; la forme « of <pile> », elle, partait au recours et rendait
     * « size of this stack ». Deux écritures de la même question, une seule
     * réponse — exactement le genre d'écart qui fait douter d'un langage.
     *
     * Une pile jamais enregistrée n'a pas de fichier : zéro, comme la
     * fonction du monde. Sur autre chose qu'une pile on ne répond pas (0),
     * et l'appelant poursuit : « the size of a button » n'est pas une
     * question dont nous connaissions la réponse, et inventer un nombre
     * serait pire que de laisser le chemin suivant s'exprimer. */
    if (ci_equal(prop, "size")) {
        if (o->type != OBJ_STACK) return 0;
        long t = hc_taille_fichier(hc_stack_path(o));
        snprintf(out, outlen, "%ld", t > 0 ? t : 0L);
        return 1;
    }
    /* « the freeSize of <pile> » : l'espace que les suppressions ont laissé
     * DANS le fichier. hc_save le réécrit entier à chaque fois et n'en laisse
     * jamais : zéro n'est pas un aveu d'ignorance, c'est la réponse juste, et
     * « if the freeSize > 0 then compact » ne compactera donc jamais pour
     * rien. Même réponse que la fonction du monde « the freeSize », qui la
     * servait déjà pour la pile courante.
     *
     * Elle manquait ici, et la forme « of <pile> » rendait donc la chaîne
     * « freeSize of this stack ». Le harnais bilan l'enregistrait comme « OK »,
     * sa logique comptant « pas d'erreur » pour « la propriété marche » : un
     * écho passait pour un résultat. C'est la troisième référence du jour dans
     * ce cas. */
    if (ci_equal(prop, "freesize")) {
        if (o->type != OBJ_STACK) return 0;
        snprintf(out, outlen, "0");
        return 1;
    }
    if (ci_equal(prop, "visible")) { snprintf(out, outlen, "%s", o->visible ? "true" : "false"); return 1; }
    if (ci_equal(prop, "showname") || ci_equal(prop, "shownname")) { snprintf(out, outlen, "%s", o->showname ? "true" : "false"); return 1; }
    if (ci_equal(prop, "enabled")) { snprintf(out, outlen, "%s", o->enabled ? "true" : "false"); return 1; }
    if (ci_equal(prop, "icon")) { snprintf(out, outlen, "%d", o->icon); return 1; }
    if (ci_equal(prop, "family")) { snprintf(out, outlen, "%d", o->family); return 1; }
    if (ci_equal(prop, "titlewidth")) { snprintf(out, outlen, "%d", o->titlewidth); return 1; }
    /* selectedLine : deux choses selon l'objet.
     *
     * Sur un BOUTON popup, c'est l'article choisi dans le menu
     * — un entier rangé dans l'objet. Sur un CHAMP, c'est la
     * ligne actuellement sélectionnée, qui n'appartient pas à
     * l'objet mais à l'état global de sélection : un seul
     * champ à la fois peut l'avoir.
     *
     * HyperCard rend « line N of card field X » pour un champ
     * et un simple numéro pour un bouton — deux formes, parce
     * que les deux ne servent pas à la même chose. */
    if (ci_equal(prop, "selectedline") || ci_equal(prop, "selectedlines")) {
        if (o->type == OBJ_FIELD) {
            if (g_sel_field != o) { snprintf(out, outlen, "%s", ""); return 1; }
            const char *t = hc_field_text(o);
            int line = 1;
            for (int i = 0; i < g_sel_start && t[i]; i++)
                if (t[i] == '\n') line++;
            char d[96];
            hc_describe(o, d, sizeof d);
            snprintf(out, outlen, "line %d of %s%s", line,
                     hc_owner_is_bg(o) ? "bg " : "card ", d);
            return 1;
        }
        snprintf(out, outlen, "%d", o->selectedline); return 1;
    }

    /* selectedText d'un objet : le texte sélectionné s'il
     * s'agit du champ qui porte la sélection ; pour un bouton
     * popup, l'article choisi — c'est ainsi qu'on lit ce que
     * l'utilisateur a pris dans le menu. */
    if (ci_equal(prop, "selectedtext")) {
        if (o->type == OBJ_FIELD) {
            if (g_sel_field != o) { snprintf(out, outlen, "%s", ""); return 1; }
            selection_text(out, outlen);
            return 1;
        }
        if (o->type == OBJ_BUTTON && o->selectedline > 0) {
            const char *t = o->contents ? o->contents : "";
            int n = 1, deb = 0;
            while (n < o->selectedline && t[deb]) {
                if (t[deb] == '\n') n++;
                deb++;
            }
            int fin = deb;
            while (t[fin] && t[fin] != '\n') fin++;
            snprintf(out, outlen, "%.*s", fin - deb, t + deb);
            return 1;
        }
        snprintf(out, outlen, "%s", "");
        return 1;
    }

    /* selectedChunk d'un champ : même désignation que la forme
     * globale, mais seulement si c'est bien ce champ-là. */
    if (ci_equal(prop, "selectedchunk")) {
        if (o->type == OBJ_FIELD && g_sel_field == o) {
            char d[96];
            hc_describe(o, d, sizeof d);
            snprintf(out, outlen, "char %d to %d of %s%s",
                     hct_utf8_compte_prefixe(hc_field_text(g_sel_field),
                                         g_sel_start) + 1,
                 hct_utf8_compte_prefixe(hc_field_text(g_sel_field),
                                         g_sel_start + g_sel_len),
                     hc_owner_is_bg(o) ? "bg " : "card ", d);
        } else snprintf(out, outlen, "%s", "");
        return 1;
    }
    if (ci_equal(prop, "locktext")) { snprintf(out, outlen, "%s", o->locktext ? "true" : "false"); return 1; }
    if (ci_equal(prop, "widemargins")) { snprintf(out, outlen, "%s", o->wide_margins ? "true" : "false"); return 1; }
    if (ci_equal(prop, "fixedlineheight")) { snprintf(out, outlen, "%s", o->fixed_lh ? "true" : "false"); return 1; }
    if (ci_equal(prop, "showlines")) { snprintf(out, outlen, "%s", o->show_lines ? "true" : "false"); return 1; }
    if (ci_equal(prop, "autotab")) { snprintf(out, outlen, "%s", o->auto_tab ? "true" : "false"); return 1; }
    if (ci_equal(prop, "dontsearch")) { snprintf(out, outlen, "%s", o->dont_search ? "true" : "false"); return 1; }
    if (ci_equal(prop, "cantdelete")) { snprintf(out, outlen, "%s", o->cant_delete ? "true" : "false"); return 1; }
    if (ci_equal(prop, "sharedtext")) { snprintf(out, outlen, "%s", o->shared_text ? "true" : "false"); return 1; }
    /* textAlign se lit en toutes lettres, comme HyperCard :
     * « left », « center », « right ». Un script compare la
     * chaîne, il n'a que faire de notre codage interne. */
    if (ci_equal(prop, "textalign")) {
        snprintf(out, outlen, "%s",
                 o->text_align == 1 ? "center" :
                 o->text_align == 2 ? "right"  : "left");
        return 1;
    }
    if (ci_equal(prop, "autoselect")) { snprintf(out, outlen, "%s", o->auto_select ? "true" : "false"); return 1; }
    if (ci_equal(prop, "multiplelines")) { snprintf(out, outlen, "%s", o->multiple_lines ? "true" : "false"); return 1; }
    if (ci_equal(prop, "dontwrap")) { snprintf(out, outlen, "%s", o->dont_wrap ? "true" : "false"); return 1; }
    if (ci_equal(prop, "marked")) { snprintf(out, outlen, "%s", o->marked ? "true" : "false"); return 1; }
    if (ci_equal(prop, "textfont")) { snprintf(out, outlen, "%s", o->textfont ? o->textfont : ""); return 1; }
    if (ci_equal(prop, "scroll")) { snprintf(out, outlen, "%d", o->scroll); return 1; }
    if (ci_equal(prop, "textstyle")) {
        /* Même formatage que la lecture sur un morceau : le
         * code d'origine n'écrivait que les trois premiers
         * bits, si bien qu'un objet en creux se relisait
         * « plain » et perdait son style à l'enregistrement. */
        style_to_names(o->textstyle, out, outlen);
        return 1;
    }
    if (ci_equal(prop, "hilite") || ci_equal(prop, "highlight")) { snprintf(out, outlen, "%s", hc_hilite_of(o, NULL) ? "true" : "false"); return 1; }
    if (ci_equal(prop, "sharedhilite")) { snprintf(out, outlen, "%s", o->shared_hilite ? "true" : "false"); return 1; }
    if (ci_equal(prop, "autohilite")) { snprintf(out, outlen, "%s", o->autohilite ? "true" : "false"); return 1; }
    if (ci_equal(prop, "textsize")) { snprintf(out, outlen, "%d", o->textsize); return 1; }
    /* textHeight n'est PAS textSize : c'est l'interligne, et
     * les scripts d'époque divisent par lui pour trouver la
     * ligne cliquée. Les confondre faussait le calcul d'un
     * tiers de ligne à chaque ligne. */
    if (ci_equal(prop, "textheight")) { snprintf(out, outlen, "%d", hc_text_height(o)); return 1; }
    if (ci_equal(prop, "script"))  {
        const char *sc = o->script ? o->script : "";
        /* Les valeurs du noyau tiennent dans des tampons fixes.
         * Un script plus long est tronqué : le dire, sinon un
         * « set script of me to it » réécrit la version mutilée
         * sans que personne ne s'en aperçoive. */
        if ((int)strlen(sc) >= outlen) {
            g_script_clipped = 1;
            emit(HC_ERR, "   !! script de %d octets tronqué à %d : "
                         "toute réécriture sera refusée",
                 (int)strlen(sc), outlen - 1);
        }
        snprintf(out, outlen, "%s", sc); return 1;
    }
    if (ci_equal(prop, "text") || ci_equal(prop, "contents"))
                                   { snprintf(out, outlen, "%s", hc_field_text(o)); return 1; }
    if (ci_equal(prop, "style"))   { snprintf(out, outlen, "%s", o->style ? o->style : "rectangle"); return 1; }
    /* partNumber : le rang parmi TOUTES les parts du propriétaire, boutons et
     * champs mêlés — et non le rang parmi les boutons, ni parmi les champs.
     * C'est ce que compte HyperCard, et ce que hc_part_number faisait déjà
     * pour l'interface sans que le langage sache le demander.
     *
     * ELLE S'ÉCRIT AUSSI, depuis qu'on sait déplacer une part : voir la
     * branche « partnumber » de v3_cmd_set. Le commentaire qui tenait ici
     * disait « en lecture seule », et justifiait ce refus par le travail de
     * « send farther et de ses voisins » — des commandes que ce projet n'a
     * jamais eues. Un manque déguisé en choix. */
    if (ci_equal(prop, "partnumber")) {
        snprintf(out, outlen, "%d", hc_part_number(o)); return 1;
    }
    return 0;
}

static void term_value_body(const char *t, char *out, int outlen)
{
    v1_compte("v1 terme", g_v1_porte);
    t = skip_spaces(t);
    out[0] = '\0';
    if (!*t) return;

    if (*t == '"') { quoted(t, out, outlen); return; }

    /* --- constantes --- */
    if (ci_equal(t, "return") || ci_equal(t, "linefeed")) { snprintf(out, outlen, "\n"); return; }
    if (ci_equal(t, "space"))  { snprintf(out, outlen, " ");  return; }
    if (ci_equal(t, "tab"))    { snprintf(out, outlen, "\t"); return; }
    if (ci_equal(t, "quote"))  { snprintf(out, outlen, "\""); return; }
    if (ci_equal(t, "comma"))  { snprintf(out, outlen, ",");  return; }
    if (ci_equal(t, "empty"))  { out[0] = '\0'; return; }

    /* --- expressions de morceau : word 2 of …, the last line of … --- */
    if (chunk_read(t, out, outlen)) return;

    /* --- the number of <morceaux> in|of <expr> --- */
    if (ci_word(t, "the") || ci_word(t, "number")) {
        const char *w = ci_word(t, "the") ? skip_spaces(t + 3) : t;
        if (ci_word(w, "number")) {
            const char *k = skip_spaces(w + 6);
            if (ci_word(k, "of")) k = skip_spaces(k + 2);

            /* the number of cards [of <fond|pile>] : sans complément, les
             * cartes de la pile courante ; avec un fond, les seules cartes
             * qui s'y appuient. Le complément était ignoré, si bien que
             * « the number of cards of bg 3 » rendait le total de la pile. */
            if (ci_word(k, "cards") || ci_word(k, "cds")) {
                const char *r = k;
                while (*r && !isspace((unsigned char)*r)) r++;
                r = skip_spaces(r);
                if (ci_word(r, "of") || ci_word(r, "in")) r = skip_spaces(r + 2);

                Object *p = g_current_card ? g_current_card->owner : NULL;
                while (p && p->type != OBJ_STACK) p = p->owner;

                if (*r) {
                    Object *o = resolve(r);
                    if (o && o->type == OBJ_BACKGROUND) {
                        int n = 0;
                        for (int i = 0; p && i < p->nparts; i++)
                            if (p->parts[i]->type == OBJ_CARD && p->parts[i]->bg == o) n++;
                        snprintf(out, outlen, "%d", n);
                        return;
                    }
                    if (o && o->type == OBJ_STACK) p = o;
                }
                snprintf(out, outlen, "%d", card_count(p));
                return;
            }
            /* the number of backgrounds : les fonds de la pile courante.
             * Placé avant la branche « bg buttons/fields » de plus bas, qui
             * reconnaît « background » comme une portée et non comme le type
             * à compter — sans cette priorité, « backgrounds » y tomberait. */
            if (ci_word(k, "backgrounds") || ci_word(k, "bkgnds") ||
                ci_word(k, "bgs")) {
                Object *p = g_current_card ? g_current_card->owner : NULL;
                while (p && p->type != OBJ_STACK) p = p->owner;
                int n = 0;
                if (p) for (int i = 0; i < p->nparts; i++)
                    if (p->parts[i]->type == OBJ_BACKGROUND) n++;
                snprintf(out, outlen, "%d", n);
                return;
            }
            /* the number of [card|bg] buttons|fields [of <carte>]
             *
             * La portée manquait : « the number of fields » additionnait la
             * carte ET le fond, alors que les rangs, eux, se comptent
             * séparément dans chacun. Sur une carte de 3 champs posée sur un
             * fond qui en porte 3, le compteur annonçait 6 et « field 4 » ne
             * désignait rien — toute boucle « repeat with f = 1 to the number
             * of fields » partait droit dans le mur. Le total reste la valeur
             * par défaut, par compatibilité, mais on peut désormais demander
             * l'un ou l'autre. */
            {
                const char *k2 = k;
                int scope = 0;              /* 0 = total, 1 = carte, 2 = fond */
                if (ci_word(k2, "card") || ci_word(k2, "cd")) {
                    const char *a = k2;
                    while (*a && !isspace((unsigned char)*a)) a++;
                    a = skip_spaces(a);
                    if (ci_word(a, "buttons") || ci_word(a, "btns") ||
                        ci_word(a, "fields")  || ci_word(a, "flds")) { scope = 1; k2 = a; }
                } else if (ci_word(k2, "bg") || ci_word(k2, "background")) {
                    const char *a = k2;
                    while (*a && !isspace((unsigned char)*a)) a++;
                    a = skip_spaces(a);
                    if (ci_word(a, "buttons") || ci_word(a, "btns") ||
                        ci_word(a, "fields")  || ci_word(a, "flds")) { scope = 2; k2 = a; }
                }

                /* the number of marked cards : combien de cartes sont
                 * désignées. Ici et non parmi les propriétés — « the number
                 * of » a son propre chemin d'analyse, et une propriété nommée
                 * « markedcards » n'y serait jamais consultée. */
                if (ci_word(k2, "marked")) {
                    const char *r2 = skip_spaces(k2 + 6);
                    if (ci_word(r2, "cards") || ci_word(r2, "cds")) {
                        Object *p = g_current_card ? g_current_card->owner : NULL;
                        while (p && p->type != OBJ_STACK) p = p->owner;
                        int m = 0;
                        if (p) for (int i = 0; i < p->nparts; i++)
                            if (p->parts[i]->type == OBJ_CARD && p->parts[i]->marked) m++;
                        snprintf(out, outlen, "%d", m);
                        return;
                    }
                }

                if (ci_word(k2, "buttons") || ci_word(k2, "btns") ||
                    ci_word(k2, "fields")  || ci_word(k2, "flds")) {
                    ObjType want = (k2[0]=='b' || k2[0]=='B') ? OBJ_BUTTON : OBJ_FIELD;
                    const char *r = k2;
                    while (*r && !isspace((unsigned char)*r)) r++;
                    r = skip_spaces(r);
                    if (ci_word(r, "of")) r = skip_spaces(r + 2);
                    Object *card = *r ? resolve(r) : g_current_card;
                    if (card && card->type != OBJ_CARD && card->type != OBJ_BACKGROUND)
                        card = g_current_card;
                    int n = 0;
                    if (card) {
                        /* Un fond désigné explicitement ne compte que le sien. */
                        int own = (scope != 2) || card->type == OBJ_BACKGROUND;
                        if (own)
                            for (int i = 0; i < card->nparts; i++)
                                if (card->parts[i]->type == want) n++;
                        if (scope != 1 && card->type == OBJ_CARD && card->bg)
                            for (int i = 0; i < card->bg->nparts; i++)
                                if (card->bg->parts[i]->type == want) n++;
                    }
                    snprintf(out, outlen, "%d", n);
                    return;
                }
            }
            int used = 0;
            ChunkType ct = chunk_kind(k, &used);
            if (ct != CH_NONE) {
                const char *r = skip_spaces(k + used);
                if (ci_word(r, "in") || ci_word(r, "of")) r = skip_spaces(r + 2);
                char *src = arena_buf();
                eval_expr(r, src, HC_VAL);
                snprintf(out, outlen, "%d", chunk_count(src, ct));
                return;
            }

            /* the number of <objet> : son RANG parmi ses semblables.
             *
             * « the number of field "test" » répond 2 si c'est le deuxième
             * champ de son propriétaire. C'est le pendant exact de la
             * désignation par rang, et donc de quoi savoir quel chiffre écrire
             * dans « field N » — ou constater qu'un champ vit sur le fond et
             * non sur la carte. En dernier recours : les morceaux et les
             * pluriels ont déjà eu leur tour, il ne reste qu'un objet. */
            {
                Object *ob = resolve(k);
                if (ob && (ob->type == OBJ_BUTTON || ob->type == OBJ_FIELD)) {
                    int n = hc_object_number(ob);
                    if (n > 0) { snprintf(out, outlen, "%d", n); return; }
                }
                if (ob && ob->type == OBJ_CARD) {
                    int n = card_index(ob->owner, ob);
                    if (n >= 0) { snprintf(out, outlen, "%d", n + 1); return; }
                }
            }
        }
    }

    /* --- fonctions intégrées --- */
    if (call_function(t, out, outlen)) return;

    /* --- [the] [short|long] <propriété> of <objet> ---
     * HyperCard tolère l'omission de « the » : « bottom of this cd »,
     * « loc of me ». On ne l'accepte que si le premier mot est bien un nom
     * de propriété connu, sinon « item 1 of x » ou une variable suivie de
     * « of » se feraient happer. */
    if (ci_word(t, "the") || prop_word_before_of(t)) {
        const char *w = ci_word(t, "the") ? skip_spaces(t + 3) : t;

        int forme = HC_NOM_ABREGE;
        if (ci_word(w, "short")) { forme = HC_NOM_COURT; w = skip_spaces(w + 5); }
        else if (ci_word(w, "long")) { forme = HC_NOM_LONG; w = skip_spaces(w + 4); }
        else if (ci_word(w, "abbreviated")) { w = skip_spaces(w + 11); }
        else if (ci_word(w, "abbrev")) { w = skip_spaces(w + 6); }
        else if (ci_word(w, "abbr")) { w = skip_spaces(w + 4); }

        const char *of = find_kw(w, "of");
        if (of) {
            char prop[32];
            int pl = (int)(of - w);
            while (pl > 0 && (w[pl-1] == ' ' || w[pl-1] == '\t')) pl--;
            if (pl > 0 && pl < (int)sizeof prop) {
                memcpy(prop, w, (size_t)pl); prop[pl] = '\0';

                /* Lecture sur une plage de texte :
                 *     if the textStyle of the clickChunk is bold
                 * La cible peut donc etre calculee : « of + 2 » est evalue
                 * comme expression si ce n'est pas deja un morceau litteral. */
                if (ci_equal(prop, "textstyle") || ci_equal(prop, "textfont") ||
                    ci_equal(prop, "textsize")  || ci_equal(prop, "textcolor")) {
                    int cst, cen;
                    Object *cf = chunk_target(of + 2, &cst, &cen);
                    if (cf) {
                        struct RunList *rl = runs_of(cf);
                        if (ci_equal(prop, "textcolor")) {
                            /* Rendue en « r,v,b » : c'est la forme qu'un
                             * script peut décomposer avec « item 1 of », et
                             * celle que « set the textColor » réaccepte. */
                            int col = runs_get_color(rl, cst, cen - cst);
                            if (col == HC_COLOR_INHERIT) snprintf(out, outlen, "0,0,0");
                            else if (col < 0)            snprintf(out, outlen, "mixed");
                            else snprintf(out, outlen, "%d,%d,%d",
                                          (col >> 16) & 255, (col >> 8) & 255, col & 255);
                        } else if (ci_equal(prop, "textstyle")) {
                            style_to_names(runs_get_style(rl, cst, cen - cst,
                                                          cf->textstyle),
                                           out, outlen);
                        } else if (ci_equal(prop, "textfont")) {
                            runs_get_font(rl, cst, cen - cst, cf->textfont,
                                          out, outlen);
                        } else {
                            int sz = runs_get_size(rl, cst, cen - cst, cf->textsize);
                            if (sz < 0) snprintf(out, outlen, "mixed");
                            else        snprintf(out, outlen, "%d", sz);
                        }
                        return;
                    }
                }

                Object *o = resolve(of + 2);
                if (o && obj_prop_read(o, prop, forme, out, outlen))
                    return;
            }
        }
    }

    /* --- un objet ? champ → contenu, autre → sa désignation --- */
    Object *o = resolve(t);
    if (o) {
        if (o->type == OBJ_FIELD) snprintf(out, outlen, "%s", hc_field_text(o));
        else                      hc_describe(o, out, outlen);
        return;
    }

    /* --- une variable ? --- */
    if (!strchr(t, ' ')) {
        const char *v = var_get(t);
        if (v) { snprintf(out, outlen, "%s", v); return; }
    }

    /* --- une propriété globale ? (« the mouse », « the mouseLoc »…) ---
     * Après les variables : un script qui nomme sa variable « mouse » garde
     * la priorité, comme dans HyperCard. */
    {
        const char *g = t;
        if (ci_word(g, "the")) g = skip_spaces(g + 3);
        if (*g && !strchr(g, ' ')) {
            const char *v = host_global(g);
            if (v) { snprintf(out, outlen, "%s", v); return; }
        }
    }
    /* --- « the width of card window », « the height of card window » ---
     *
     * La fenêtre de la pile. HyperCard la traite comme un objet à part
     * entière, avec ses propriétés de géométrie ; on n'implémente ici que
     * les quatre qui servent réellement dans les scripts d'époque, à partir
     * de la taille de la pile courante.
     *
     * Graph Maker s'en sert pour contraindre le déplacement de ses boutons
     * aux bords de la carte :
     *
     *     doDragBtn name of me,0,62,width of card window,height of card window
     */
    {
        const char *g = t;
        if (ci_word(g, "the")) g = skip_spaces(g + 3);

        char prop[32];
        const char *ap = next_word(g, prop, sizeof prop);
        ap = skip_spaces(ap);
        if (ci_word(ap, "of")) ap = skip_spaces(ap + 2);
        if (ci_word(ap, "card") || ci_word(ap, "cd"))
            ap = skip_spaces(ap + (ci_word(ap, "cd") ? 2 : 4));

        if (ci_word(ap, "window")) {
            Object *st = owning_stack(g_current_card);
            int w = st && st->w ? st->w : 512;
            int h = st && st->h ? st->h : 342;

            if      (ci_equal(prop, "width"))  { snprintf(out, outlen, "%d", w); return; }
            else if (ci_equal(prop, "height")) { snprintf(out, outlen, "%d", h); return; }
            else if (ci_equal(prop, "rect") || ci_equal(prop, "rectangle")) {
                snprintf(out, outlen, "0,0,%d,%d", w, h); return;
            }
            else if (ci_equal(prop, "loc") || ci_equal(prop, "location")) {
                snprintf(out, outlen, "%d,%d", w / 2, h / 2); return;
            }
        }
    }
    /* --- sinon littéral non quoté, comme le faisait HyperCard --- */
    snprintf(out, outlen, "%s", t);
}

static void term_value(const char *t, char *out, int outlen)
{
    ARENA_MARK;
    term_value_body(t, out, outlen);
    ARENA_FREE;
}

static void parse_expr(const char **p, char *out, int outlen);

static int truthy(const char *s);

static void parse_factor(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    const char *s = skip_spaces(*p);
    out[0] = '\0';

    if (*s == '(') {
        *p = s + 1;
        parse_expr(p, out, outlen);
        s = skip_spaces(*p);
        if (*s == ')') s++;
        *p = s;
        { ARENA_FREE; return; }
    }
    if (*s == '-') {
        char *v = arena_buf();
        double d = 0;
        *p = s + 1;
        parse_factor(p, v, HC_VAL);
        as_num(v, &d);
        put_num(-d, out, outlen);
        { ARENA_FREE; return; }
    }
    /* `not` est au niveau 2 chez Apple, aussi serré que le moins unaire :
       « not 5 > 2 » se lit « (not 5) > 2 ». */
    if (ci_word(s, "not")) {
        char *v = arena_buf();
        *p = s + 3;
        parse_factor(p, v, HC_VAL);
        snprintf(out, outlen, "%s", truthy(v) ? "false" : "true");
        { ARENA_FREE; return; }
    }
    /* « there is a <objet> » / « there is no <objet> » / « there is not a … »
     * Tout ce qui suit désigne l'objet : l'expression s'arrête là, ce qui
     * suffit puisque la forme n'apparaît jamais qu'en position de condition. */
    if (ci_word(s, "there")) {
        const char *q = skip_spaces(s + 5);
        if (ci_word(q, "is")) {
            q = skip_spaces(q + 2);
            int negate = 0;
            if (ci_word(q, "not"))     { negate = 1; q = skip_spaces(q + 3); }
            else if (ci_word(q, "no")) { negate = 1; q = skip_spaces(q + 2); }
            if (ci_word(q, "an"))      q = skip_spaces(q + 2);
            else if (ci_word(q, "a"))  q = skip_spaces(q + 1);

            char *ref = arena_buf();
            snprintf(ref, HC_VAL, "%s", q);
            int k = (int)strlen(ref);
            while (k > 0 && isspace((unsigned char)ref[k-1])) ref[--k] = '\0';

            int found = resolve(ref) != NULL;
            snprintf(out, outlen, "%s", (found != negate) ? "true" : "false");
            *p = q + strlen(q);
            { ARENA_FREE; return; }
        }
    }

    if (isdigit((unsigned char)*s) || (*s == '.' && isdigit((unsigned char)s[1]))) {
        char *e;
        double d = strtod(s, &e);
        *p = e;
        put_num(d, out, outlen);
        { ARENA_FREE; return; }
    }
    if (*s == '"') {                 /* littéral : on ne prend que lui */
        *p = quoted(s, out, outlen);
        { ARENA_FREE; return; }
    }

    char *ref = arena_buf();
    const char *before = s;
    collect_ref(&s, ref, HC_VAL);
    if (s == before && *s) s++;      /* jamais de sur-place : pas de boucle infinie */
    *p = s;
    term_value(ref, out, outlen);
    ARENA_FREE;
    ARENA_FREE;
}

/* niveau 3 : exponentiation, associative à droite */
static void parse_power(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_factor(p, out, outlen);
    const char *s = skip_spaces(*p);
    if (*s != '^') return;
    *p = s + 1;

    char *rhs = arena_buf();
    double a = 0, b = 0;
    parse_power(p, rhs, HC_VAL);      /* récursif : 2^3^2 = 2^(3^2) */
    as_num(out, &a); as_num(rhs, &b);
    put_num(pow(a, b), out, outlen);
    ARENA_FREE;
    ARENA_FREE;
}

static void parse_product(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_power(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        int op;
        if      (*s == '*')          { op = '*'; s += 1; }
        else if (*s == '/')          { op = '/'; s += 1; }
        else if (ci_word(s, "mod"))  { op = 'm'; s += 3; }
        else if (ci_word(s, "div"))  { op = 'd'; s += 3; }
        else break;
        *p = s;

        char *rhs = arena_buf();
        double a = 0, b = 0, r = 0;
        parse_power(p, rhs, HC_VAL);
        as_num(out, &a); as_num(rhs, &b);
        if      (op == '*') r = a * b;
        else if (op == '/') r = (b != 0) ? a / b : 0;
        /* Même raison que pour trunc plus haut : le cast est indéfini hors
         * plage. « div » tronque vers zéro et « mod » en découle. */
        else if (op == 'd') r = (b != 0) ? trunc(a / b) : 0;
        else                r = (b != 0) ? a - b * trunc(a / b) : 0;
        put_num(r, out, outlen);
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void parse_sum(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_product(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        if (*s != '+' && *s != '-') break;
        int op = *s++;
        *p = s;

        char *rhs = arena_buf();
    double a = 0, b = 0;
        parse_product(p, rhs, HC_VAL);
        as_num(out, &a); as_num(rhs, &b);
        put_num(op == '+' ? a + b : a - b, out, outlen);
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void parse_concat(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_sum(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        int space;
        if      (s[0] == '&' && s[1] == '&') { space = 1; s += 2; }
        else if (s[0] == '&')                { space = 0; s += 1; }
        else break;
        *p = s;

        char *rhs = arena_buf();
        parse_sum(p, rhs, HC_VAL);
        int n = (int)strlen(out);
        if (space && n < outlen - 1) { out[n++] = ' '; out[n] = '\0'; }
        snprintf(out + n, (size_t)(outlen - n), "%s", rhs);
    }
    ARENA_FREE;
    ARENA_FREE;
}

/* Compare deux valeurs. Numérique si les deux en sont, sinon texte
   sans tenir compte de la casse — comme HyperTalk. */
static int compare_vals(int op, const char *x, const char *y)
{
    double a, b;
    if (op == 'c') return ci_strstr(x, y);      /* contains */
    if (op == 'i') return ci_strstr(y, x);      /* is in : l'inverse */
    if (as_num(x, &a) && as_num(y, &b)) {
        switch (op) {
            case '=': return a == b;
            case '!': return a != b;
            case '<': return a <  b;
            case '>': return a >  b;
            case 'l': return a <= b;
            default:  return a >= b;
        }
    }
    /* Texte : hct_compare applique la même règle — numérique si les DEUX
     * opérandes le sont, sinon comparaison insensible à la casse. On n'arrive
     * ici que dans le second cas, les nombres ayant été traités au-dessus. */
    int c = hct_compare(x, y, NULL);
    switch (op) {
        case '=': return c == 0;
        case '!': return c != 0;
        case '<': return c <  0;
        case '>': return c >  0;
        case 'l': return c <= 0;
        default:  return c >= 0;
    }
}

static int truthy(const char *s)
{
    double d;
    if (as_num(s, &d)) return d != 0;
    return ci_equal(s, "true");
}

/* ═══ Les propriétés globales que le NOYAU tient lui-même ══════════════
 *
 * itemDelimiter découpe les items, numberFormat met en forme les calculs,
 * lockScreen retient l'affichage : ce sont des réglages de l'interprète, pas
 * de l'interface. L'hôte n'a rien à en savoir, et le lui passer l'obligerait
 * à nous les rendre ensuite.
 *
 * UNE SEULE FOIS, ici. Le même bloc était recopié dans le chemin v3 et dans
 * le chemin v1 — deux copies à modifier ensemble, donc deux copies vouées à
 * diverger, et l'ajout suivant n'aurait été fait que dans l'une des deux.
 *
 * Rend 1 si la propriété a été prise en charge, 0 si elle regarde l'hôte. */
/* ═══ Les réglages d'environnement ═════════════════════════════════════
 *
 * userLevel, dragSpeed, blindTyping, powerKeys, lockRecent, textArrows : six
 * réglages qu'une pile pose dans son openStack et relit ensuite. Ils se
 * ressemblent tous — un nom, un type, une valeur, des bornes —, ce qui est
 * exactement la raison d'en faire une TABLE et non six paires de branches.
 * Six paires, c'est douze endroits à tenir d'accord, et la septième propriété
 * n'aurait été ajoutée qu'à l'un des deux.
 *
 * CE QU'ILS FONT, ET CE QU'ILS NE FONT PAS. Il faut le dire, parce qu'un
 * réglage qu'on peut poser et relire donne l'impression d'agir.
 *
 * Ils sont RETENUS et RELUS fidèlement : « set the userLevel to 3 » puis
 * « if the userLevel < 4 » se comportent comme la pile l'attend, et c'est
 * déjà tout autre chose que le silence d'avant, où poser ne faisait rien et
 * relire rendait le mot « userLevel ».
 *
 * Ils ne sont pas encore CONSULTÉS par l'application. HC n'a pas de niveaux
 * d'utilisateur — tout y est toujours accessible, ce qui équivaut au niveau
 * 5 —, son « drag » est instantané, et il n'a pas de liste de cartes
 * récentes. Une pile qui compterait sur « set the userLevel to 1 » pour se
 * protéger de l'édition se tromperait donc. C'est écrit ici plutôt que tu
 * l'apprennes à tes dépens.
 *
 * Les bornes servent : « set the userLevel to 47 » se refuse au lieu de
 * ranger 47. Un réglage hors de son domaine ne veut rien dire, et le laisser
 * passer reporte la surprise à la lecture. */
typedef enum { REG_BOOL, REG_ENTIER } HcTypeReglage;

static struct {
    const char   *nom;       /* en minuscules, pour la comparaison    */
    const char   *affiche;   /* tel qu'HyperCard l'écrit, pour la trace */
    HcTypeReglage type;
    int           valeur;
    int           mini, maxi;
} G_REGLAGES[] = {
    /* 5 par défaut : HC n'impose aucune restriction, et annoncer un niveau
     * plus bas que ce qu'on autorise serait le mensonge inverse. */
    { "userlevel",   "userLevel",   REG_ENTIER, 5, 1, 5     },
    { "dragspeed",   "dragSpeed",   REG_ENTIER, 0, 0, 32767 },
    { "blindtyping", "blindTyping", REG_BOOL,   0, 0, 1     },
    { "powerkeys",   "powerKeys",   REG_BOOL,   0, 0, 1     },
    { "lockrecent",  "lockRecent",  REG_BOOL,   0, 0, 1     },
    { "textarrows",  "textArrows",  REG_BOOL,   0, 0, 1     },
    /* « set the lockErrorDialogs to true » : l'erreur ne va plus au dialogue,
     * elle part en message « errorDialog » à la carte courante. Voir
     * erreurs_vide. Remise à false en retombant au repos, comme lockScreen —
     * une pile qui la pose et sort par un « exit » resterait sinon muette
     * pour toujours. */
    { "lockerrordialogs", "lockErrorDialogs", REG_BOOL, 0, 0, 1 },
};
#define HC_NREGLAGES ((int)(sizeof G_REGLAGES / sizeof *G_REGLAGES))

static int reglage_index(const char *nom)
{
    for (int i = 0; i < HC_NREGLAGES; i++)
        if (ci_equal(nom, G_REGLAGES[i].nom)) return i;
    return -1;
}

/* La valeur courante d'un réglage, pour le noyau lui-même. Zéro si le nom
 * n'en est pas un — aucun appelant interne ne peut se tromper de nom sans
 * qu'un harnais le voie, et rendre zéro vaut mieux qu'un plantage. */
static int reglage_valeur(const char *nom)
{
    int i = reglage_index(nom);
    return i < 0 ? 0 : G_REGLAGES[i].valeur;
}

/* ÉTEINDRE un réglage booléen. Sert au déverrouillage automatique en
 * retombant au repos — voir hc_send_args_k.
 *
 * Pas de « remise au défaut » générale : la table ne garde pas les valeurs
 * d'origine, et en inventer une par nom donnerait un jour « remet userLevel à
 * 5 » sous couvert de propreté. Éteindre un booléen est ce dont on a besoin,
 * et c'est tout ce que cette fonction promet. */
static void reglage_eteint(const char *nom)
{
    int i = reglage_index(nom);
    if (i >= 0 && G_REGLAGES[i].type == REG_BOOL) G_REGLAGES[i].valeur = 0;
}

/* Poser. Rend 1 si le nom en est un — même quand la valeur est refusée :
 * la propriété a bien été reconnue, c'est la valeur qui ne convient pas, et
 * la passer ensuite à l'hôte l'induirait en erreur. */
static int reglage_pose(const char *prop, const char *val)
{
    int i = reglage_index(prop);
    if (i < 0) return 0;

    int v;
    if (G_REGLAGES[i].type == REG_BOOL) {
        v = truthy(val) ? 1 : 0;
    } else {
        double d;
        if (!as_num(val, &d)) {
            emit(HC_ERR, "   !! %s attend un nombre, reçu « %s »",
                 G_REGLAGES[i].affiche, val);
            return 1;
        }
        v = (int)d;
        if (v < G_REGLAGES[i].mini || v > G_REGLAGES[i].maxi) {
            emit(HC_ERR, "   !! %s va de %d à %d, reçu %d",
                 G_REGLAGES[i].affiche, G_REGLAGES[i].mini,
                 G_REGLAGES[i].maxi, v);
            return 1;
        }
    }
    G_REGLAGES[i].valeur = v;
    emit(HC_INFO, "   → %s ← %d", G_REGLAGES[i].affiche, v);
    return 1;
}

/* Relire. Rend 1 si le nom en est un ; `out` reçoit « true »/« false » pour
 * un booléen, le nombre sinon — les deux formes qu'HyperTalk sait comparer. */
static int reglage_lit(const char *nom, char *out, int outlen)
{
    int i = reglage_index(nom);
    if (i < 0) return 0;
    if (G_REGLAGES[i].type == REG_BOOL)
        snprintf(out, (size_t)outlen, "%s", G_REGLAGES[i].valeur ? "true" : "false");
    else
        snprintf(out, (size_t)outlen, "%d", G_REGLAGES[i].valeur);
    return 1;
}

static int prop_globale_noyau(const char *prop, const char *val)
{
    /* Une chaîne vide ou de plusieurs caractères ramène à la virgule —
     * HyperCard ne retenait qu'un caractère. */
    if (ci_equal(prop, "itemdelimiter")) {
        item_delim_pose(val);
        emit(HC_INFO, "   → itemDelimiter ← \"%s\"", item_delim());
        return 1;
    }

    /* « set the numberFormat to "0.00" ». Le gabarit vit dans hct_val.c, avec
     * l'écriture des nombres qu'il gouverne. */
    if (ci_equal(prop, "numberformat")) {
        hct_format_nombre(val);
        emit(HC_INFO, "   → numberFormat ← \"%s\"", hct_format_nombre_lu());
        return 1;
    }

    /* Les six réglages d'environnement, en une table — voir plus haut. */
    if (reglage_pose(prop, val)) return 1;

    /* lockScreen : « set lockScreen to true » est l'exact synonyme de « lock
     * screen », et notify_field s'appuie dessus pour ne pas redessiner mille
     * fois pour rien.
     *
     * Rend 0 : l'hôte doit le voir passer aussi — HCview s'en sert pour
     * retenir setNeedsDisplay:. C'est la seule des trois à être partagée. */
    if (ci_equal(prop, "lockscreen")) {
        g_ecran_verrouille = truthy(val);
        if (!g_ecran_verrouille) verrou_reveille();
        return 0;
    }

    return 0;
}


/* ---- tests de type pour « is a[n] <type> » ----
   Le guide (chapitre 7) donne : number, integer, point, rect, date, logical. */

static void trim_copy(const char *s, char *out, int outlen)
{
    s = skip_spaces(s);
    snprintf(out, outlen, "%s", s);
    int n = (int)strlen(out);
    while (n > 0 && (out[n-1] == ' ' || out[n-1] == '\t')) out[--n] = '\0';
}

static int is_int_str(const char *s)
{
    double d;
    if (!as_num(s, &d)) return 0;
    /* « d == (double)(long long)d » était indéfini dès que d sortait de la
     * plage d'un long long, et as_num accepte « 1e300 ». trunc() répond à la
     * même question — « ce nombre a-t-il une partie fractionnaire ? » — sans
     * jamais quitter le domaine des flottants. Un infini n'est pas un entier,
     * et un NaN n'est égal à rien, y compris à lui-même : les deux sont donc
     * traités au passage. */
    if (!(d == d) || d > 1e308 || d < -1e308) return 0;
    return d == trunc(d);
}

/* nombre d'items entiers séparés par des virgules ; -1 si l'un ne l'est pas */
static int int_items(const char *s)
{
    int n = 0;
    char buf[64];
    const char *p = s;
    for (;;) {
        const char *c = strchr(p, ',');
        int len = c ? (int)(c - p) : (int)strlen(p);
        if (len > (int)sizeof buf - 1) return -1;
        memcpy(buf, p, (size_t)len); buf[len] = '\0';
        if (!is_int_str(buf)) return -1;
        n++;
        if (!c) break;
        p = c + 1;
    }
    return n;
}

/* Forme numérique seulement (12/25/96, 1996-12-25). Sans horloge dans le
   noyau, on ne reconnaît pas encore « December 25, 1996 ». */
/* « is a date » et « convert » doivent s'accorder : ce que l'un accepte,
 * l'autre doit le reconnaître. Sans quoi un script qui valide une saisie
 * par « if it is a date then convert it » tourne en boucle sur une date
 * pourtant convertible — les dateItems, par exemple. */
static int looks_like_date(const char *s)
{
    struct tm tm;
    return parse_datetime(s, &tm);
}

static int is_of_type(const char *v, const char *ty)
{
    ARENA_MARK;
    char *t = arena_buf();
    double d;
    trim_copy(v, t, HC_VAL);

    if (ci_equal(ty, "number"))    return as_num(t, &d);
    if (ci_equal(ty, "integer"))   return is_int_str(t);
    if (ci_equal(ty, "logical") || ci_equal(ty, "boolean"))
        { ARENA_FREE; return ci_equal(t, "true") || ci_equal(t, "false"); }
    if (ci_equal(ty, "point"))     return int_items(t) == 2;
    if (ci_equal(ty, "rect") || ci_equal(ty, "rectangle")) return int_items(t) == 4;
    if (ci_equal(ty, "date"))      return looks_like_date(t);
    { ARENA_FREE; return 0; }
    ARENA_FREE;
    ARENA_FREE;
}

/* niveau 7 : comparaisons relationnelles, contains, is in.
   Attention : un « is » nu appartient au niveau 8, on le laisse passer. */
static void parse_relational(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_concat(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        int op = 0, neg = 0;

        if      (s[0] == '<' && s[1] == '=') { op = 'l'; s += 2; }
        else if (s[0] == '>' && s[1] == '=') { op = 'g'; s += 2; }
        else if (s[0] == '<' && s[1] == '>') break;      /* <> : niveau 8 */
        else if (s[0] == '<')                { op = '<'; s += 1; }
        else if (s[0] == '>')                { op = '>'; s += 1; }
        else if (ci_word(s, "contains"))     { op = 'c'; s += 8; }
        else if (ci_word(s, "is")) {
            const char *w = skip_spaces(s + 2);
            int notted = 0;
            if (ci_word(w, "not")) { notted = 1; w = skip_spaces(w + 3); }

            if (ci_word(w, "a") || ci_word(w, "an")) {      /* test de type */
                /* Lire le nom de type lettre à lettre, sans next_word : celui-ci
                 * s'arrête aux blancs et emporterait la parenthèse fermante de
                 * « (it is a date) », ce qui faisait échouer tous les tests de
                 * type placés entre parenthèses. */
                char ty[32]; int tk = 0;
                const char *q = skip_spaces(w + (ci_word(w, "an") ? 2 : 1));
                while (isalpha((unsigned char)*q) && tk < (int)sizeof ty - 1)
                    ty[tk++] = *q++;
                ty[tk] = '\0';
                *p = q;
                int r = is_of_type(out, ty);
                if (notted) r = !r;
                snprintf(out, outlen, "%s", r ? "true" : "false");
                continue;
            }
            if (ci_word(w, "within")) {                     /* point is within rect */
                *p = w + 6;
                char *rhs = arena_buf();
                parse_concat(p, rhs, HC_VAL);
                int pt[2], rc[4];
                int r = 0;
                if (parse_ints(out, pt, 2) == 2 && parse_ints(rhs, rc, 4) == 4)
                    r = (pt[0] >= rc[0] && pt[0] <= rc[2] &&
                         pt[1] >= rc[1] && pt[1] <= rc[3]);
                if (notted) r = !r;
                snprintf(out, outlen, "%s", r ? "true" : "false");
                continue;
            }
            if (ci_word(w, "in")) { op = 'i'; neg = notted; s = w + 2; }
            else break;                                     /* « is » / « is not » : niveau 8 */
        }
        else break;
        *p = s;

        char *rhs = arena_buf();
        parse_concat(p, rhs, HC_VAL);
        int r = compare_vals(op, out, rhs);
        if (neg) r = !r;
        snprintf(out, outlen, "%s", r ? "true" : "false");
    }
    ARENA_FREE;
    ARENA_FREE;
}

/* niveau 8 : égalités */
static void parse_equality(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_relational(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        int op = 0, neg = 0;

        if      (s[0] == '<' && s[1] == '>') { op = '!'; s += 2; }
        else if (s[0] == '=')                { op = '='; s += 1; }
        else if (ci_word(s, "is")) {
            const char *w = skip_spaces(s + 2);
            if (ci_word(w, "not")) { neg = 1; s = w + 3; }
            else s += 2;
            op = '=';
        }
        else break;
        *p = s;

        char *rhs = arena_buf();
        parse_relational(p, rhs, HC_VAL);
        int r = compare_vals(op, out, rhs);
        if (neg) r = !r;
        snprintf(out, outlen, "%s", r ? "true" : "false");
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void parse_and(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_equality(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        if (!ci_word(s, "and")) break;
        *p = s + 3;
        char *rhs = arena_buf();
        parse_equality(p, rhs, HC_VAL);
        snprintf(out, outlen, "%s", (truthy(out) && truthy(rhs)) ? "true" : "false");
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void parse_expr(const char **p, char *out, int outlen)
{
    ARENA_MARK;
    parse_and(p, out, outlen);
    for (;;) {
        const char *s = skip_spaces(*p);
        if (!ci_word(s, "or")) break;
        *p = s + 2;
        char *rhs = arena_buf();
        parse_and(p, rhs, HC_VAL);
        snprintf(out, outlen, "%s", (truthy(out) || truthy(rhs)) ? "true" : "false");
    }
    ARENA_FREE;
    ARENA_FREE;
}

/* ==================================================================
 * PONT VERS L'INTERPRÉTEUR v3
 *
 * RÉPARTITION DU TRAVAIL PENDANT LA TRANSITION
 *
 * La v3 prend : littéraux, constantes, variables, les dix rangs
 * d'opérateurs, les morceaux, l'arithmétique, et les fonctions purement
 * calculatoires (abs, sqrt, min, max, offset, round, length…).
 *
 * hc_core.c garde : les références d'objets, les propriétés, les
 * gestionnaires écrits en HyperTalk, et les fonctions du monde. La v3 les
 * lui renvoie par le rappel « recours », qui reconstitue le texte source
 * du sous-arbre et le confie à term_value — laquelle sait déjà tout faire.
 *
 * ------------------------------------------------------------------
 * LA FRAGILITÉ DE CE PONT, ET POURQUOI ELLE EST ACCEPTABLE
 *
 * La reconstitution du texte ne peut restituer que ce qui figure dans
 * l'arbre. Tout ce que l'analyseur consomme SANS le ranger dans un nœud
 * disparaît : les parenthèses d'un appel, le « the » facultatif. Chaque
 * cas se rattrape ici, au coup par coup — voir v3_recours.
 *
 * C'est le prix d'une greffe progressive. Le pont disparaîtra quand la v3
 * saura résoudre les objets et appeler les gestionnaires elle-même, sans
 * repasser par le texte.
 * ================================================================== */

/* Reconstitue le texte source d'un sous-arbre.
 *
 * Exact, parce que les jetons pointent DANS le script d'origine et qu'une
 * référence y est contiguë : du premier au dernier octet couvert. */
static void v3_source(const HctNoeud *n, char *out, int outlen)
{
    const char *deb = NULL, *fin = NULL;
    const HctNoeud *pile[256];
    int np = 0;

    out[0] = '\0';
    if (!n) return;
    pile[np++] = n;

    while (np) {
        const HctNoeud *c = pile[--np];
        if (c->jeton.deb && c->jeton.len >= 0) {
            const char *d = c->jeton.deb;
            const char *f = d + c->jeton.len;

            /* Une chaîne littérale est rangée SANS ses guillemets : deb
             * pointe après le premier, len s'arrête avant le second. Les
             * bornes du texte source les incluent, sinon la reconstitution
             * rend « bg field "Data » — chaîne non fermée que term_value ne
             * peut pas lire, et tout ce qui suit part en vrille sans le
             * moindre message. */
            if (c->jeton.genre == HCT_CHAINE) { d--; f++; }

            if (!deb || d < deb) deb = d;
            if (!fin || f > fin) fin = f;
        }
        for (int i = 0; i < c->nfils && np < 256; i++)
            if (c->fils[i]) pile[np++] = c->fils[i];
    }
    if (!deb || !fin || fin <= deb) return;

    int len = (int)(fin - deb);
    if (len >= outlen) len = outlen - 1;
    memcpy(out, deb, (size_t)len);
    out[len] = '\0';
}

/* --- variables ------------------------------------------------------ */

static int v3_lit_var(void *d, const char *nom, HctValeur *out)
{
    (void)d;
    const char *v = var_get(nom);
    if (!v) return 0;
    *out = hct_val_texte(v);
    return 1;
}

static int v3_ecrit_var(void *d, const char *nom, const char *val)
{
    (void)d;
    var_set(nom, val ? val : "");
    return 1;
}

/* --- recours : objets, propriétés, tout ce que la v3 ne fait pas ----- */

/* Profondeur du recours.
 *
 * Le recours appelle term_value, qui peut à son tour appeler eval_expr —
 * laquelle repasse par le recours. L'imbrication est légitime (une fonction
 * HyperTalk qui en appelle une autre), mais rien ne garantit qu'une tournure
 * inattendue ne bouclera pas sur elle-même. Le plafond transforme une boucle
 * infinie, qui gèlerait l'application, en une erreur visible. */
/* ==================================================================
 * RÉSOLUTION D'OBJETS DEPUIS L'ARBRE
 *
 * Jusqu'ici les références d'objets repassaient par le TEXTE : le pont
 * reconstituait « bg field "Data" of card 3 » à partir des jetons, puis le
 * confiait à resolve(). Or la reconstitution ne peut restituer que ce qui
 * figure dans l'arbre, et tout ce que l'analyseur consomme sans le ranger
 * disparaît. On en a perdu quatre en une semaine :
 *
 *   - le « the » facultatif     -> « target » au lieu de « the target »
 *   - les parenthèses d'un appel -> « dayNameData » au lieu de « … () »
 *   - le guillemet fermant      -> « bg field "Data » — chaîne ouverte
 *   - les adjectifs             -> « time » au lieu de « long time »
 *
 * Chaque perte donnait le même symptôme : une valeur rendue en clair, sans
 * le moindre message, et un calcul qui s'effondrait plus loin.
 *
 * Le nœud HCTN_OBJET porte déjà tout ce dont resolve a besoin — type,
 * portée, mode de désignation, cible — puisqu'il a été calqué sur elle. On
 * traduit donc directement, sans jamais repasser par le texte.
 *
 * Restent au recours : les propriétés, les fonctions du monde et les
 * gestionnaires écrits en HyperTalk, qui n'ont pas de nœud dédié.
 * ================================================================== */

/* Évalue un sous-arbre en texte, dans le contexte courant. */
static void v3_val_texte(HctContexte *ctx, const HctNoeud *n,
                         char *out, int outlen)
{
    out[0] = '\0';
    if (!n) return;
    HctValeur v = hct_evalue(ctx, n);
    snprintf(out, (size_t)outlen, "%s", v.txt ? v.txt : "");
    hct_val_libere(&v);
}

/* Le n-ième fond de la pile, 1-based. NULL si le rang dépasse. */
static Object *v3_nth_bg(Object *stack, int n)
{
    if (!stack || n < 1) return NULL;
    for (int i = 0; i < stack->nparts; i++)
        if (stack->parts[i]->type == OBJ_BACKGROUND && --n == 0)
            return stack->parts[i];
    return NULL;
}

static Object *v3_bg_par_nom(Object *stack, const char *nm)
{
    for (int i = 0; stack && i < stack->nparts; i++)
        if (stack->parts[i]->type == OBJ_BACKGROUND &&
            stack->parts[i]->name && ci_equal(stack->parts[i]->name, nm))
            return stack->parts[i];
    return NULL;
}

static Object *v3_bg_par_id(Object *stack, int id)
{
    for (int i = 0; stack && i < stack->nparts; i++)
        if (stack->parts[i]->type == OBJ_BACKGROUND &&
            stack->parts[i]->id == id)
            return stack->parts[i];
    return NULL;
}

/* Le rang que désigne un ordinal, pour un total donné.
 * « middle » vaut total/2 + 1, et non (total+1)/2 : sur quatre éléments
 * HyperCard rend le TROISIÈME — vérifié. */
static int v3_rang_ordinal(HctOrdinal o, int total)
{
    switch (o) {
        case HCT_ORD_PREMIER:    return 1;
        case HCT_ORD_DEUXIEME:   return 2;
        case HCT_ORD_TROISIEME:  return 3;
        case HCT_ORD_QUATRIEME:  return 4;
        case HCT_ORD_CINQUIEME:  return 5;
        case HCT_ORD_SIXIEME:    return 6;
        case HCT_ORD_SEPTIEME:   return 7;
        case HCT_ORD_HUITIEME:   return 8;
        case HCT_ORD_NEUVIEME:   return 9;
        case HCT_ORD_DIXIEME:    return 10;
        case HCT_ORD_MILIEU:     return total > 0 ? total / 2 + 1 : 0;
        case HCT_ORD_DERNIER:    return total;
        case HCT_ORD_QUELCONQUE: return total > 0 ? (rand() % total) + 1 : 0;
        default:                 return 0;
    }
}

static Object *hct_resout(HctContexte *ctx, const HctNoeud *n);

/* La cible d'un « of », si le nœud en porte une.
 *
 * Les enfants d'un HCTN_OBJET sont, dans l'ordre : le désignateur quand il
 * en faut un — nom, rang, id — puis la cible du « of ». On regarde donc le
 * DERNIER enfant, et seulement s'il est lui-même une référence d'objet. */
/* Le nœud du désignateur, ou NULL quand il n'y en a pas. */
static const HctNoeud *v3_designateur(const HctNoeud *n)
{
    if (n->designateur != HCT_DES_NOM &&
        n->designateur != HCT_DES_RANG &&
        n->designateur != HCT_DES_ID) return NULL;
    return n->nfils >= 1 ? n->fils[0] : NULL;
}

/* LE NŒUD de la cible explicite — « … of X » —, ou NULL s'il n'y en a pas.
 *
 * SÉPARÉ DE SA RÉSOLUTION, ET C'EST TOUT L'ENJEU. Ces deux questions ont
 * longtemps partagé une seule réponse :
 *
 *     y a-t-il un « of X » ?          -> NULL si non
 *     ce « of X » désigne-t-il quoi ? -> NULL si l'objet n'existe pas
 *
 * L'appelant recevait NULL dans les deux cas et, ne sachant pas les
 * distinguer, continuait sur la CARTE COURANTE. Mesuré, avec une carte
 * « Une » portant un champ « X » et aucune carte « Absente » :
 *
 *     put field "X" of card "Absente"           -> BONJOUR
 *     put "OUPS" into field "X" of card "Absente"
 *                     -> écrit OUPS dans le champ de la carte COURANTE
 *     put there is a field "X" of card "Absente" -> true
 *     put the name of card 1 of stack "PileAbsente" -> card "Une"
 *
 * La lecture ment ; l'écriture, elle, modifie des données dans un objet que
 * le script n'a jamais nommé, et paraît avoir réussi. C'est le défaut le
 * plus grave rencontré dans ce dépôt.
 *
 * LE DÉSIGNATEUR N'EST PAS UNE CIBLE. Quand le nœud n'a qu'un fils et que ce
 * fils EST le désignateur — « field (me) », où l'on nomme le champ par une
 * expression qui se trouve être un objet —, ce fils ne doit pas être pris
 * pour un « of X ». On le compare donc explicitement plutôt que de se fier à
 * son seul genre : sans cela, la correction ci-dessous transformerait une
 * méprise silencieuse en refus catégorique. */
static const HctNoeud *v3_noeud_cible(const HctNoeud *n)
{
    if (n->nfils < 1) return NULL;
    const HctNoeud *dernier = n->fils[n->nfils - 1];
    if (dernier->genre != HCTN_OBJET) return NULL;
    if (dernier == v3_designateur(n)) return NULL;
    return dernier;
}

/* ------------------------------------------------------------ l'entrée */

/* UN NŒUD D'OBJET QUI NE S'EST PAS RÉSOLU PENDANT CETTE LIGNE.
 *
 * LE DÉFAUT QUE CECI CORRIGE. Un gestionnaire de commande rend 0 dans DEUX
 * cas que rien ne distinguait : « ce n'est pas ma forme » — « show all
 * cards », qui n'est pas un objet — et « l'objet n'existe pas ». Le
 * répartiteur concluait dans les deux cas « ne sait pas faire », et posait
 * « Can't understand » dans le résultat.
 *
 * Or HC comprend parfaitement « show » : la commande marche dès que le champ
 * existe. Le verbe était compris, c'est l'OBJET qui manquait. Mesuré, même
 * cause et quatre diagnostics :
 *
 *   show card field "Absent"            ->  Can't understand    FAUX
 *   hide card field "Absent"            ->  Can't understand    FAUX
 *   put "x" into card field "Absent"    ->  Can't understand    FAUX
 *   set the visible of ... to false     ->  objet introuvable    juste
 *
 * Un diagnostic faux envoie chercher au mauvais endroit : il m'a fait
 * conclure que « show » n'était pas implémenté, alors qu'il ne manquait qu'un
 * champ dans mon banc d'essai.
 *
 * POURQUOI ICI. Trois routes différentes mènent au même message — les
 * gestionnaires de hc_core.c, le chemin d'écriture de hct_exec.c, et le
 * recours. Les rapiécer une à une laisserait la quatrième. hct_resout est le
 * passage obligé de toute résolution d'objet : c'est le seul endroit qui voit
 * les trois.
 *
 * UN BOOLÉEN NE SUFFISAIT PAS, ET LE COMMENTAIRE QUI EST ICI MENTAIT.
 *
 * Il annonçait « remis à zéro au début de chaque ligne exécutée ». Le code ne
 * le faisait pas : la remise à zéro n'a lieu que dans le répartiteur, et une
 * ligne servie entièrement par l'exécuteur v3 n'y passe jamais. Le drapeau
 * survivait donc à la ligne qui l'avait levé.
 *
 * Pire, il se lève pour des absences PARFAITEMENT NORMALES. « there is a
 * field "Absent" » est une question, pas une faute : l'évaluateur résout, ne
 * trouve pas, et répond false — mais la résolution ratée lève le drapeau au
 * passage. Mesuré :
 *
 *     put there is a field "Absent" into x
 *     help
 *     -> « objet introuvable : help »   et   the result = "No such object"
 *
 * alors que « help » ne contient pas le moindre objet. Même chose pour
 * « dial "555" », « open printing », « palette "x" ». Un diagnostic périmé
 * est pire qu'un diagnostic vague : il est faux avec assurance, et il envoie
 * chercher là où il n'y a rien.
 *
 * ON RETIENT DONC LE NŒUD, PAS UN OUI/NON. Le diagnostic ne s'en sert que si
 * l'échec APPARTIENT à la commande qu'il est en train d'expliquer — c'est
 * v3_noeud_contient, juste en dessous. « put x into card field "Absent" »
 * résout sa cible dans l'exécuteur, donc avant le répartiteur, mais ce nœud
 * est bien dans l'arbre de la commande : ce cas-là continue de marcher.
 *
 * Le pointeur n'est JAMAIS déréférencé — on ne compare que des adresses —,
 * de sorte qu'un nœud d'un arbre déjà libéré ne peut pas faire de dégât : au
 * pire il ne correspond à rien, ce qui est la réponse voulue. */
static const HctNoeud *g_objet_manque = NULL;

/* `cherche` est-il ce nœud, ou l'un de ses descendants ? */
static int v3_noeud_contient(const HctNoeud *racine, const HctNoeud *cherche)
{
    if (!racine || !cherche) return 0;
    if (racine == cherche) return 1;
    for (int i = 0; i < racine->nfils; i++)
        if (v3_noeud_contient(racine->fils[i], cherche)) return 1;
    return 0;
}

static Object *hct_resout_corps(HctContexte *ctx, const HctNoeud *n);

/* L'enveloppe, pour n'avoir qu'UN endroit à instrumenter. Le corps a une
 * dizaine de sorties ; les marquer une à une, c'est en oublier une. */
static Object *hct_resout(HctContexte *ctx, const HctNoeud *n)
{
    Object *o = hct_resout_corps(ctx, n);
    /* Seul un nœud d'OBJET compte. « show all cards » n'en est pas un : le
     * gestionnaire a raison de passer la main, et ce n'est pas un objet
     * manquant. C'est cette distinction qui manquait. */
    if (!o && n && n->genre == HCTN_OBJET) g_objet_manque = n;
    return o;
}

static Object *hct_resout_corps(HctContexte *ctx, const HctNoeud *n)
{
    if (!n || n->genre != HCTN_OBJET) return NULL;

    Object *card  = g_current_card;
    Object *bg    = card ? card->bg : NULL;
    Object *stack = card ? card->owner : NULL;

    /* Une cible explicite déplace le contexte : « bg field "x" of card 3 »
     * cherche le champ dans la carte 3, pas dans la carte courante.
     *
     * ET SI ELLE NE SE RÉSOUT PAS, ON S'ARRÊTE LÀ. « of X » où X n'existe
     * pas n'est PAS la même chose que « sans of » : voir v3_noeud_cible.
     * Retomber sur la carte courante faisait lire — et écrire — dans un
     * objet que le script n'avait pas nommé. On rend NULL, et l'appelant
     * dira « objet introuvable » en nommant la ligne. */
    /* LE FOND A-T-IL ÉTÉ NOMMÉ, ou est-ce seulement celui de la carte
     * courante ? La distinction décide de tout pour « card <n> » :
     *
     *     card 1            la première carte de la PILE
     *     card 1 of bg 2    la première carte QUI UTILISE le fond 2
     *
     * `bg` vaut par défaut le fond de la carte courante, et s'en servir pour
     * restreindre ferait de « card 1 » la première carte du fond courant —
     * ce qui n'est pas ce que dit HyperTalk. Seul un « of » explicite
     * restreint. */
    int fond_nomme = 0;

    const HctNoeud *cible_n = v3_noeud_cible(n);
    Object *cible = cible_n ? hct_resout(ctx, cible_n) : NULL;
    if (cible_n && !cible) return NULL;
    if (cible) {
        if (cible->type == OBJ_CARD) {
            card = cible;
            bg   = card->bg;
            /* La pile suit la carte : « field 1 of card 3 of stack "x" »
             * cherche dans la pile de CETTE carte. */
            stack = owning_stack(card);
        }
        else if (cible->type == OBJ_BACKGROUND) { bg = cible; fond_nomme = 1; }
        else if (cible->type == OBJ_STACK) {
            /* Une pile désignée explicitement gagne. Recalculer la pile
             * depuis g_current_card juste après, comme on le faisait, la
             * ramenait aussitôt à la pile COURANTE : « card 3 of stack "y" »
             * cherchait la carte 3 de la pile ouverte. */
            stack = cible;
        }
    }

    char val[256];
    const HctNoeud *des = v3_designateur(n);
    if (des) v3_val_texte(ctx, des, val, sizeof val);
    else     val[0] = '\0';

    switch (n->typeobj) {

        case HCT_OBJ_ME:     return g_me;
        case HCT_OBJ_TARGET: return g_target;

        case HCT_OBJ_STACK:
            if (n->designateur == HCT_DES_NOM) {
                if (stack && stack->name && ci_equal(stack->name, val))
                    return stack;
                return find_open_stack(val);
            }
            return stack;

        case HCT_OBJ_BACKGROUND:
            switch (n->designateur) {
                case HCT_DES_NOM:  return v3_bg_par_nom(stack, val);
                case HCT_DES_ID:   return v3_bg_par_id(stack, hc_id(val));
                case HCT_DES_RANG: {
                    /* Un désignateur peut être un nom aussi bien qu'un rang :
                     * « bg i » où i vaut 2, mais aussi « bg commun ». On
                     * regarde CE QUI SORT de l'évaluation, comme resolve. */
                    int l = (int)strlen(val);
                    if (l > 0 && (int)strspn(val, "0123456789") == l)
                        return v3_nth_bg(stack, hc_rang(val));
                    return v3_bg_par_nom(stack, val);
                }
                /* UN FOND DÉSIGNÉ REND UN FOND.
                 *
                 * Ces deux branches rendaient une CARTE — la première carte
                 * du fond visé pour l'ordinal, et pour le relatif la carte
                 * suivante de la pile, qui n'a rien à voir avec le fond
                 * suivant. Le désignateur seul changeait donc la nature de
                 * ce qu'on obtenait : « the short name of bg 2 » donnait
                 * « fondDeux », mais « the short name of first background »
                 * donnait « carteUn », et « the short name of this
                 * background » le nom de la carte courante.
                 *
                 * L'intention était bonne et l'endroit mauvais : on ne se
                 * tient jamais SUR un fond, donc « go to last background »
                 * doit mener à une carte. Mais c'est go qui fait cette
                 * conversion, pour toutes les formes à la fois, quelques
                 * centaines de lignes plus bas — et il la faisait déjà.
                 * Résoudre un fond en carte ici ne servait qu'à casser
                 * toutes les autres phrases. */
                case HCT_DES_ORDINAL: {
                    int total = 0;
                    for (int i = 0; stack && i < stack->nparts; i++)
                        if (stack->parts[i]->type == OBJ_BACKGROUND) total++;
                    return v3_nth_bg(stack, v3_rang_ordinal(n->ordinal, total));
                }
                case HCT_DES_RELATIF: {
                    if (n->relatif == HCT_REL_CE) return bg;
                    /* Le fond suivant, pas la carte suivante — et l'on boucle,
                     * comme les cartes le font juste en dessous : les piles
                     * d'époque feuillettent sans jamais tester les bords. */
                    int total = 0, rang = -1;
                    for (int i = 0; stack && i < stack->nparts; i++)
                        if (stack->parts[i]->type == OBJ_BACKGROUND) {
                            if (stack->parts[i] == bg) rang = total;
                            total++;
                        }
                    if (total <= 0 || rang < 0) return NULL;
                    int pas = (n->relatif == HCT_REL_SUIVANT) ? +1 : -1;
                    return v3_nth_bg(stack, ((rang + pas) % total + total) % total + 1);
                }
                default: return bg;
            }

        case HCT_OBJ_CARD: {
            /* Le fond ne restreint QUE s'il a été nommé : voir fond_nomme,
             * en tête de cette fonction. NULL veut dire « toute la pile ». */
            Object *ou = fond_nomme ? bg : NULL;
            switch (n->designateur) {
                case HCT_DES_NOM: return card_par_nom_de(stack, ou, val);
                case HCT_DES_ID:  return card_par_id_de(stack, ou, hc_id(val));
                case HCT_DES_RANG: {
                    int l = (int)strlen(val);
                    if (l > 0 && (int)strspn(val, "0123456789") == l)
                        return nth_card_de(stack, ou, hc_rang(val) - 1);
                    return card_par_nom_de(stack, ou, val);
                }
                case HCT_DES_ORDINAL:
                    return nth_card_de(stack, ou,
                        v3_rang_ordinal(n->ordinal, card_count_de(stack, ou)) - 1);
                case HCT_DES_RELATIF: {
                    if (n->relatif == HCT_REL_CE) return card;
                    /* « go next card » depuis la dernière mène à la PREMIÈRE,
                     * et « go previous card » depuis la première mène à la
                     * DERNIÈRE : HyperCard boucle. C'est déjà ce que fait la
                     * branche des FONDS, juste au-dessus — les deux doivent
                     * s'accorder, et jusqu'ici seuls les fonds bouclaient.
                     * Sans le modulo, nth_card rendait NULL sur le premier
                     * « go previous card », et la navigation s'arrêtait là,
                     * silencieusement : le résultat restait vide, aucun
                     * message d'erreur, juste plus rien qui bouge. */
                    int nc = card_count_de(stack, ou);
                    int i  = card_index_de(stack, ou, card);
                    if (nc <= 0 || i < 0) return NULL;
                    int pas = (n->relatif == HCT_REL_SUIVANT) ? +1 : -1;
                    return nth_card_de(stack, ou, ((i + pas) % nc + nc) % nc);
                }
                default: return card;
            }
        }

        case HCT_OBJ_BUTTON:
        case HCT_OBJ_FIELD:
        case HCT_OBJ_PART: {
            /* « part » prend les deux, et c'est tout le propos du mot. */
            int t = (n->typeobj == HCT_OBJ_PART)   ? HC_PART_QUELCONQUE
                  : (n->typeobj == HCT_OBJ_BUTTON) ? OBJ_BUTTON : OBJ_FIELD;

            /* La portée décide où chercher. Sans portée explicite, HyperCard
             * cherche d'abord sur la carte, puis se rabat sur le fond — c'est
             * ce que fait resolve, et beaucoup de piles en dépendent. */
            Object *premier  = (n->portee == HCT_PORTEE_FOND) ? bg : card;
            Object *repli    = (n->portee == HCT_PORTEE_AUCUNE) ? bg : NULL;

            switch (n->designateur) {
                case HCT_DES_ID: {
                    int w = hc_id(val);
                    Object *o = find_part_by_id(premier, t, w);
                    if (!o && repli) o = find_part_by_id(repli, t, w);
                    return o;
                }
                case HCT_DES_NOM: {
                    Object *o = find_part(premier, t, val);
                    if (!o && repli) o = find_part(repli, t, val);
                    return o;
                }
                /* « first field », « last button », « any part ».
                 *
                 * CETTE BRANCHE MANQUAIT, et son absence ne se voyait pas
                 * comme une absence. Le désignateur tombait sur le `default`
                 * juste en dessous, hct_resout rendait NULL, et le pont
                 * repassait la phrase à l'ancien moteur — qui ne connaît pas
                 * l'ordinal devant un type de part et rendait LA CARTE.
                 * Mesuré :
                 *
                 *     put the name of first field   ->  card "Une"
                 *     put the name of last field    ->  card "Deux"
                 *     set the name of first field to "titi"
                 *                                   ->  renomme la CARTE
                 *
                 * Une mauvaise réponse, pas une erreur, et une écriture qui
                 * va se poser ailleurs que là où on l'envoie. « second field »
                 * disait bien « objet introuvable », ce qui rendait le défaut
                 * encore plus trompeur : le cas le plus courant — first —
                 * était justement celui qui mentait en silence.
                 *
                 * LE TOTAL SE COMPTE PAR COUCHE, et c'est tout le soin à
                 * prendre ici : « last field » n'a pas le même sens sur la
                 * carte et sur le fond. On compte donc là où l'on cherche,
                 * puis on recompte pour le repli. */
                case HCT_DES_ORDINAL: {
                    int r = v3_rang_ordinal(n->ordinal, compte_parts(premier, t));
                    Object *o = r > 0 ? find_part_by_rank(premier, t, r) : NULL;
                    if (!o && repli) {
                        int r2 = v3_rang_ordinal(n->ordinal, compte_parts(repli, t));
                        if (r2 > 0) o = find_part_by_rank(repli, t, r2);
                    }
                    return o;
                }
                case HCT_DES_RANG: {
                    int l = (int)strlen(val);
                    if (l > 0 && (int)strspn(val, "0123456789") == l) {
                        int r = hc_rang(val);
                        Object *o = find_part_by_rank(premier, t, r);
                        if (!o && repli) o = find_part_by_rank(repli, t, r);
                        return o;
                    }
                    Object *o = find_part(premier, t, val);
                    if (!o && repli) o = find_part(repli, t, val);
                    return o;
                }
                default:
                    return NULL;
            }
        }

        default:
            return NULL;
    }
}

/* « the number of cards », « the number of card buttons », « the number of
 * backgrounds » : un COMPTAGE d'objets, que seul l'hôte peut faire.
 *
 * L'évaluateur sait déjà compter les morceaux — « the number of items of x »
 * — mais pas les objets, et ces formes repartaient donc en entier vers
 * l'ancien interpréteur par reconstitution du texte source. Avec les pertes
 * qui vont avec : « the number of card fields » se reconstituait en
 * « number of card », que term_value rendait tel quel, en clair, au lieu
 * d'un nombre.
 *
 * On ne traite que le PLURIEL nu, sans désignateur ni cible : « the number
 * of card 3 » désigne le RANG de cette carte, et « the number of cards of
 * bg 2 » porte une cible — les deux restent à l'ancien code.
 *
 * La portée absente compte la carte ET le fond, comme term_value : les rangs
 * se comptent séparément dans chacun, mais le total est resté la valeur par
 * défaut par compatibilité. */
static int v3_nombre_objets(const HctNoeud *obj, int *out)
{
    if (!obj || obj->genre != HCTN_OBJET) return 0;
    if (obj->designateur != HCT_DES_AUCUN) return 0;
    if (obj->nfils != 0) return 0;

    Object *card  = g_current_card;
    Object *stack = owning_stack(card);

    if (obj->typeobj == HCT_OBJ_CARD) {
        /* « the number of marked cards » : le même comptage, tamisé. Seules
         * les cartes se marquent — « marked buttons » n'existe pas —, donc le
         * drapeau est refusé plus bas pour tout autre type. */
        if (obj->marque) {
            int m = 0;
            for (int i = 0; stack && i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_CARD && stack->parts[i]->marked)
                    m++;
            *out = m;
            return 1;
        }
        *out = card_count(stack);
        return 1;
    }

    /* Le marquage ne concerne que les cartes. On REFUSE plutôt que d'ignorer
     * le mot : « the number of marked buttons » rendrait sinon le nombre de
     * boutons, ce qui n'est la réponse à aucune question. */
    if (obj->marque) return 0;

    if (obj->typeobj == HCT_OBJ_BACKGROUND) {
        int n = 0;
        for (int i = 0; stack && i < stack->nparts; i++)
            if (stack->parts[i]->type == OBJ_BACKGROUND) n++;
        *out = n;
        return 1;
    }

    if (obj->typeobj != HCT_OBJ_BUTTON && obj->typeobj != HCT_OBJ_FIELD &&
        obj->typeobj != HCT_OBJ_PART)
        return 0;

    Object *coins[2] = { NULL, NULL };
    if      (obj->portee == HCT_PORTEE_CARTE) coins[0] = card;
    else if (obj->portee == HCT_PORTEE_FOND)  coins[0] = card ? card->bg : NULL;
    else { coins[0] = card; coins[1] = card ? card->bg : NULL; }

    int n = 0;
    for (int k = 0; k < 2; k++) {
        Object *o = coins[k];
        for (int i = 0; o && i < o->nparts; i++) {
            int t = o->parts[i]->type;
            if (obj->typeobj == HCT_OBJ_PART) {
                if (t == OBJ_BUTTON || t == OBJ_FIELD) n++;
            } else if (t == (obj->typeobj == HCT_OBJ_BUTTON ? OBJ_BUTTON
                                                            : OBJ_FIELD)) n++;
        }
    }
    *out = n;
    return 1;
}

/* ═══ « card window » : la fenêtre de la pile ════════════════════════════
 *
 * HyperCard traite la fenêtre de la pile comme un objet à part entière, avec
 * ses propriétés de géométrie. L'arbre de la v3 n'a pas de type FENÊTRE, et
 * n'en a pas besoin : l'analyseur lit « card window » comme « la carte de rang
 * <window> » — un HCTN_OBJET de type carte, désigné par un rang qui se trouve
 * être l'identifiant « window ». Il suffit de reconnaître cette forme.
 *
 * Sans cela, hct_resout évaluait « window » comme un rang, n'y trouvait pas de
 * variable, et le DIFFUSAIT comme un message dans toute la hiérarchie avant
 * d'échouer : deux envois de « window » et un de « width » PAR EXPRESSION, et
 * un piège si la pile a par malchance un gestionnaire de ce nom. La valeur
 * finissait juste, l'ancien interpréteur la servant par term_value — mais au
 * prix d'un aller-retour par le texte et de cinq messages inutiles.
 *
 * On s'en tient aux quatre propriétés que term_value sert déjà : ce sont
 * celles qu'emploient les scripts d'époque, et inventer les autres reviendrait
 * à décider seul de ce que HyperCard aurait répondu. */
static int v3_est_fenetre(const HctNoeud *n)
{
    if (!n || n->genre != HCTN_OBJET)          return 0;
    if (n->typeobj != HCT_OBJ_CARD)            return 0;
    if (n->designateur != HCT_DES_RANG)        return 0;
    if (n->nfils != 1 || !n->fils[0])          return 0;
    if (n->fils[0]->genre != HCTN_IDENT)       return 0;

    char mot[16];
    hct_texte(&n->fils[0]->jeton, mot, sizeof mot);
    return ci_equal(mot, "window");
}

static int v3_fenetre_prop(const HctNoeud *n, HctValeur *out)
{
    if (!n || n->genre != HCTN_OF || n->nfils < 2)   return 0;
    if (!n->fils[0] || n->fils[0]->genre != HCTN_IDENT) return 0;
    if (!v3_est_fenetre(n->fils[1]))                 return 0;

    char prop[32];
    hct_texte(&n->fils[0]->jeton, prop, sizeof prop);

    Object *st = owning_stack(g_current_card);
    int w = st && st->w ? st->w : 512;
    int h = st && st->h ? st->h : 342;
    char b[48];

    if      (ci_equal(prop, "width"))  snprintf(b, sizeof b, "%d", w);
    else if (ci_equal(prop, "height")) snprintf(b, sizeof b, "%d", h);
    else if (ci_equal(prop, "rect") || ci_equal(prop, "rectangle"))
        snprintf(b, sizeof b, "0,0,%d,%d", w, h);
    else if (ci_equal(prop, "loc") || ci_equal(prop, "location"))
        snprintf(b, sizeof b, "%d,%d", w / 2, h / 2);
    else return 0;

    *out = hct_val_texte(b);
    return 1;
}

/* Définis plus bas, avec les autres commandes de menu ; v3_recours en a
 * besoin pour « there is a menu "X" ». */
static int v3_menu_index(HctContexte *ctx, const HctNoeud *n);
static int v3_famille_bouton_choisi(HctContexte *ctx, const HctNoeud *n,
                                    char *out, int outlen);
/* ═══ POURQUOI UNE PROPRIÉTÉ DE MENU N'A PAS PU ÊTRE SERVIE ═════════════
 *
 * LE DÉFAUT QUE CECI CORRIGE. Trois échecs bien distincts se disaient de la
 * même façon, et la façon était fausse deux fois sur trois :
 *
 *     set the checkMark of menu "Essai" to true
 *     set the checkMark of menuItem 9 of menu "Essai" to true
 *     set the checkMark of menuItem 1 of menu "Absent" to true
 *     -> « propriété de menu inconnue : checkMark », les trois fois
 *
 * Or checkMark EXISTE, et marche : sur un article qui existe, dans un menu
 * qui existe, elle se lit et s'écrit. Le message envoyait donc chercher du
 * côté du nom de la propriété — le seul endroit où il n'y avait rien.
 *
 * LA LECTURE MENTAIT AUSSI, dans l'autre sens : les mêmes trois cas, plus
 * « the zorglub of menuItem 1 », disaient tous « objet introuvable », même
 * quand le menu et l'article étaient là.
 *
 * C'est la troisième fois qu'on corrige cette forme-là — après
 * « Can't understand » posé sur un objet manquant, et « propriété inconnue »
 * posé sur une cible absente. Un diagnostic faux coûte plus cher qu'un
 * diagnostic vague : le vague fait chercher partout, le faux fait chercher
 * au mauvais endroit et donne confiance en le faisant.
 *
 * La raison est posée LÀ OÙ L'ÉCHEC SE PRODUIT — v3_menu_index sait que le
 * menu manque, v3_article_index que l'article manque, les deux tables de
 * propriétés que le nom est inconnu — et lue par les deux appelants. */
typedef enum {
    V3_MENU_RIEN = 0,          /* pas une forme de menu, ou pas d'échec */
    V3_MENU_MENU_ABSENT,
    V3_MENU_ARTICLE_ABSENT,
    V3_MENU_PROP_INCONNUE
} V3MenuEchec;
static V3MenuEchec g_menu_echec = V3_MENU_RIEN;

static const char *v3_menu_raison(void)
{
    switch (g_menu_echec) {
        case V3_MENU_MENU_ABSENT:    return "menu introuvable";
        case V3_MENU_ARTICLE_ABSENT: return "article de menu introuvable";
        default:                     return "propriété de menu inconnue";
    }
}

static int v3_article_index(HctContexte *ctx, const HctNoeud *n, int *imenu);
static int v3_menu_prop_lit(HctContexte *ctx, const HctNoeud *obj,
                            const char *prop, HctValeur *out);

static int g_v3_recours_prof = 0;

static void v3_note(const char *quoi, const char *nom);
static int v3_lit_prop(void *d, void *objet, const char *prop,
                       HctValeur *out);
static HctHote v3_hote(void);

static const HctNoeud *g_v3_cible_manquee;   /* voir v3_resout */

/* « the <propriété> of <objet> » : la même règle un cran plus loin.
 *
 * « the short name of field "absent" » rendait « short name of field
 * "absent" » — le texte de la demande. Un script qui teste ce nom le trouve
 * non vide et travaille sur une chaîne inventée.
 *
 * Le test est PUREMENT STRUCTUREL : on ne résout rien ici. Résoudre la cible
 * pour savoir si elle existe rappellerait l'évaluateur, donc ce recours, donc
 * cette fonction — la première version bouclait et six harnais dépassaient
 * leur délai. C'est « echo » qui fait le tri : il ne vaut 1 que si l'ancien
 * évaluateur a rendu son entrée inchangée, c'est-à-dire s'il n'a rien
 * reconnu. « the width of card window » et « the checkmark of menuItem 2 of
 * menu "X" », qu'il sait traiter, n'arrivent jamais jusqu'ici. */
static int v3_prop_exige_un_objet(const char *prop);

/* LA CIBLE S'EST RÉSOLUE, ET PERSONNE NE SAIT SERVIR LA PROPRIÉTÉ.
 *
 * L'autre moitié du même défaut, et celle que j'avais laissée : « the schrink
 * of me » rendait la chaîne « schrink of me », sans erreur. L'objet existe —
 * c'est « me » —, seul le nom de la propriété est inventé. Une faute de frappe
 * dans un script passait donc pour un résultat.
 *
 * hct_eval SAIT DÉJÀ dire la bonne chose :
 *
 *     if (!objet) hct_ctx_faute(ctx, n, "objet introuvable");
 *     else        hct_ctx_faute(ctx, n, "propriété inconnue");
 *
 * Il ne manquait que d'y arriver : le recours rendait l'écho au lieu de rendre
 * la main. On rend donc 0, et le message juste sort tout seul.
 *
 * ON SE LIMITE AUX NŒUDS D'OBJET ÉCRITS EN TOUTES LETTRES — « me », « this
 * card », « card field "Champ" ». Une cible en variable demanderait de
 * distinguer « elle s'est résolue » de « on ne l'a jamais essayée », et
 * g_v3_cible_manquee ne porte pas cette nuance : une valeur restée d'une
 * évaluation précédente ferait conclure à tort. Le cas est donc laissé, et
 * c'est dit ici plutôt que tu. */
static int v3_prop_inconnue_sur_objet(const HctNoeud *n)
{
    if (!n || n->genre != HCTN_OF || n->nfils < 2) return 0;
    const HctNoeud *sur = n->fils[1];
    if (!sur || sur->genre != HCTN_OBJET) return 0;
    /* Manquée : c'est « objet introuvable », pas « propriété inconnue ».
     * Les deux messages ne désignent pas le même coupable. */
    if (sur == g_v3_cible_manquee) return 0;
    return 1;
}

static int v3_prop_sur_objet(const HctNoeud *n)
{
    if (!n || n->genre != HCTN_OF || n->nfils < 2) return 0;
    const HctNoeud *sur = n->fils[1];
    if (!sur) return 0;
    /* C'est bien CETTE cible-ci que la résolution vient de manquer. Sans cette
     * égalité, « the owner of me » — cible présente, propriété que le noyau ne
     * sert pas — tomberait sous la même règle, alors qu'il ne s'agit pas du
     * même défaut et que le message serait faux. */
    if (sur != g_v3_cible_manquee) return 0;

    /* UN NŒUD D'OBJET SUFFIT ; UNE VARIABLE DEMANDE L'AVIS DE LA PROPRIÉTÉ.
     *
     * « field "menu" » écrit en toutes lettres désigne un champ : s'il n'y en
     * a pas, il faut le dire, quelle que soit la propriété demandée.
     *
     * Une VARIABLE, elle, ne dit rien d'elle-même — « the short name of z » et
     * « the number of chars of z » s'écrivent pareil. C'est alors la propriété
     * qui tranche : « short name » n'a de sens que sur un objet, « number of
     * chars » compte du texte. Trier ici plutôt que sur la tête de la valeur
     * couvre le cas où celle-ci ne ressemble à rien — et c'était précisément
     * le trou. */
    if (sur->genre == HCTN_OBJET) return 1;

    char prop[64];
    hct_texte(&n->fils[0]->jeton, prop, sizeof prop);
    return v3_prop_exige_un_objet(prop);
}

/* Ce texte s'écrit-il comme un DESCRIPTEUR d'objet ?
 *
 * Sert à distinguer deux échecs que la sonde confondrait :
 *
 *     the short name of ("card button " & quote & "Absent" & quote)
 *     the number of chars of ("abc" & "d")
 *
 * Dans les deux cas la sonde évalue son texte et resolve renonce. Mais le
 * premier DÉSIGNE un objet — l'auteur a écrit « card button », il en veut un,
 * et s'il n'y en a pas il faut le dire — tandis que le second ne désigne rien
 * du tout : c'est du texte, et « the number of chars » a parfaitement le droit
 * de le compter. Signaler « objet introuvable » sur « abcd » serait une erreur
 * inventée, exactement le défaut qu'on est en train de corriger, à l'envers.
 *
 * On ne teste donc que le PREMIER MOT, sur le vocabulaire dont resolve_local
 * fait ses branches. Un nom nu n'est pas un descripteur : « Bouton » tout seul
 * ne dit ni la couche ni la sorte. */
/* Cette propriété n'a-t-elle de sens QUE sur un objet ?
 *
 * LE MANQUE QUE CELA COMBLE. La garde finale ne regardait que la CIBLE : si
 * elle s'écrivait comme un descripteur et n'en désignait aucun, on levait
 * « objet introuvable ». Mais « the short name of z », où z contient le texte
 * « inconnu », ne ressemble à rien — et rendait donc la chaîne « short name
 * of z ». Mesuré sur quatre formes, toutes muettes :
 *
 *   the short name of z            (z = "inconnu")      -> short name of z
 *   the short name of jamaisPosee  (jamais posée)       -> short name of jamaisPosee
 *   the short name of o            (o = "o")            -> short name of o
 *   the short name of k            (objet inexistant)   -> short name of k
 *
 * C'est le même écho que « nExistePas(3) » et que « owner of me » avant sa
 * correction : la question rendue comme réponse, sans erreur.
 *
 * LA BONNE CLÉ EST LA PROPRIÉTÉ, PAS LA CIBLE. « short name » n'a de sens que
 * sur un objet, quelle que soit la tête de sa cible ; « number of chars » n'en
 * exige aucun, et doit continuer de compter « abcd ». Trier par la propriété
 * est donc plus juste que de deviner d'après le texte de la cible, et cela
 * couvre les cas où la cible ne ressemble à rien.
 *
 * CE QUI EN EST EXCLU, ET POURQUOI. Tout ce qui peut porter sur un MORCEAU de
 * texte plutôt que sur un objet : textFont, textSize, textStyle, textHeight,
 * textAlign, textColor — « the textFont of char 1 to 5 of field X » est
 * légitime. Et tout ce qui a un sens hors objet : text, contents, number,
 * length, size, scroll, selectedText, selectedChunk, selectedLine. En cas de
 * doute on EXCLUT : laisser passer un écho est désagréable, inventer une
 * erreur sur une tournure valide est pire. */
static int v3_prop_exige_un_objet(const char *prop)
{
    if (!prop) return 0;
    const char *p = skip_spaces(prop);
    /* Les adjectifs sont collés au nom par l'analyseur : « the short name of »
     * arrive en un seul jeton « short name ». On les détache comme le fait
     * v3_lit_prop, sinon « short name » ne serait jamais reconnu. */
    static const char *ADJ[] = { "short", "long", "abbreviated", "abbrev",
                                 "abbr", "english", "plain", "numeric", NULL };
    for (int i = 0; ADJ[i]; i++) {
        size_t l = strlen(ADJ[i]);
        if (strncasecmp(p, ADJ[i], l) == 0 && (p[l] == ' ' || p[l] == '\t')) {
            p = skip_spaces(p + l);
            break;
        }
    }
    static const char *OBJET_SEUL[] = {
        "name", "owner", "id", "partnumber",
        "rect", "rectangle", "topleft", "botright", "bottomright",
        "left", "top", "right", "bottom", "width", "height",
        "loc", "location",
        "visible", "showname", "shownname", "enabled", "marked",
        "style", "family", "titlewidth", "icon",
        "hilite", "highlight", "autohilite",
        "locktext", "widemargins", "fixedlineheight", "showlines",
        "autotab", "dontsearch", "cantdelete", "sharedtext", "sharedhilite",
        "autoselect", "multiplelines", "dontwrap",
        "script", NULL
    };
    for (int i = 0; OBJET_SEUL[i]; i++)
        if (ci_equal(p, OBJET_SEUL[i])) return 1;
    return 0;
}

static int v3_ressemble_a_un_objet(const char *t)
{
    if (!t) return 0;
    t = skip_spaces(t);
    if (ci_word(t, "the")) t = skip_spaces(t + 3);
    static const char *TETES[] = {
        "card", "cd", "bkgnd", "bg", "background", "stack",
        "button", "btn", "field", "fld", "part", NULL
    };
    for (int i = 0; TETES[i]; i++) if (ci_word(t, TETES[i])) return 1;
    return 0;
}

/* Une cible que la sonde de v3_recours a le droit d'évaluer.
 *
 * La liste est fermée plutôt qu'ouverte : on nomme ce qu'on accepte, et non
 * ce qu'on refuse. Un genre ajouté demain à l'arbre sera donc refusé par
 * défaut, ce qui est le bon sens pour une évaluation SPÉCULATIVE — elle a
 * lieu alors que rien ne dit encore qu'on en aura besoin.
 *
 * Ce qui en est exclu, et pourquoi :
 *   HCTN_APPEL      un appel de fonction peut avoir des effets. Une sonde
 *                   n'en déclenche pas.
 *   HCTN_OBJET      hct_resout s'en occupe, et c'est lui qui doit échouer
 *                   pour que « objet introuvable » nomme le bon coupable.
 *   HCTN_IDENT      une variable nue : v3_resout la lit sans rien évaluer,
 *                   ce qui est plus direct et bien moins cher.
 *
 * HCTN_BINAIRE est de la partie, et ce n'est pas un détail : c'est
 * l'idiome courant,
 *
 *     the short name of ("card button " & quote & "Bouton" & quote)
 *
 * qui rendait jusqu'ici son propre texte, tronqué à la parenthèse. */
static int v3_cible_calculable(const HctNoeud *t)
{
    if (!t) return 0;
    switch (t->genre) {
    case HCTN_CHAINE:
    case HCTN_CHUNK:
    case HCTN_OF:
    case HCTN_BINAIRE:
    case HCTN_UNAIRE:
        return 1;
    default:
        return 0;
    }
}

static int v3_recours(void *d, const HctNoeud *n, HctValeur *out,
                      HctContexte *ctx)
{
    /* Le recours d'EXPRESSION — distinct de v3_commande, qui rend une ligne
     * entière. C'est par ici que term_value et call_function, le vieux
     * moteur d'expressions, sont encore atteints. Sans cette porte ils
     * comptaient sous « ? », et c'était justement le plus gros total. */
    const char *sauve_porte = v1_porte("recours expr");
    int sonde_manquee = 0;
    (void)d;

    /* Étiquette fine : le genre seul ne dit rien quand la ligne monte à
     * plusieurs milliers, ni surtout à qui lit le bilan sans l'arbre sous
     * les yeux. Une pile de dessin en a produit 8919 en une session, et
     * « recours of » ne permettait de savoir ni laquelle coûtait, ni s'il
     * s'agissait d'une propriété qu'obj_prop_read ignore ou d'une cible que
     * hct_resout ne sait pas résoudre.
     *
     * « X of Y » se nomme par Y, le nom de la propriété — le cas le plus
     * fréquent, et celui où le nom seul suffit à savoir quoi chercher. Tout
     * le reste (« recours objet », « recours chunk »… ) se nomme par un bout
     * de son texte source : bien moins ambigu qu'un genre de nœud qui ne dit
     * pas QUEL objet ou QUELLE expression est en cause. */
    /* La géométrie de la fenêtre de la pile, servie sans repasser par le
     * texte. Avant le relevé : ce n'est plus un retour vers la v1. */
    if (v3_fenetre_prop(n, out)) return 1;

    /* « there is a menu "X" » / « there is no menu "X" ».
     *
     * hct_eval nous l'envoie ici plutôt qu'à resout : un menu n'a pas d'objet
     * derrière lui, et resout rendrait toujours NULL — la pile croirait son
     * menu absent à chaque ouverture et le recréerait sans fin. */
    if (n->genre == HCTN_UNAIRE && n->op && n->nfils >= 1 &&
        n->fils[0] && n->fils[0]->genre == HCTN_OBJET &&
        (n->fils[0]->typeobj == HCT_OBJ_MENU ||
         n->fils[0]->typeobj == HCT_OBJ_MENUITEM) &&
        (ci_equal(n->op, "there is a") || ci_equal(n->op, "there is an") ||
         ci_equal(n->op, "there is no"))) {

        int existe;
        if (n->fils[0]->typeobj == HCT_OBJ_MENU)
            existe = v3_menu_index(ctx, n->fils[0]) >= 0;
        else {
            int im = -1;
            existe = v3_article_index(ctx, n->fils[0], &im) >= 0;
        }
        if (ci_equal(n->op, "there is no")) existe = !existe;
        *out = hct_val_texte(existe ? "true" : "false");
        { g_v1_porte = sauve_porte; } return 1;
    }

    /* Les propriétés d'un menu et de ses articles : « the checkMark of
     * menuItem 2 of menu "X" », « the name of menu 1 ». Comme pour
     * « there is a menu », resout ne peut rien pour elles.
     *
     * Le contexte VIENT MAINTENANT avec le recours, si bien que les deux
     * résolveurs évaluent leur désignateur : « menu 1 » comme « menu i ».
     * Ils gardent leur repli littéral pour un appelant qui n'en aurait
     * pas. */
    if (n->genre == HCTN_OF && n->nfils >= 2 &&
        n->fils[0] && n->fils[0]->genre == HCTN_IDENT &&
        n->fils[1] && n->fils[1]->genre == HCTN_OBJET &&
        (n->fils[1]->typeobj == HCT_OBJ_MENU ||
         n->fils[1]->typeobj == HCT_OBJ_MENUITEM)) {
        char prop[64];
        hct_texte(&n->fils[0]->jeton, prop, sizeof prop);
        if (v3_menu_prop_lit(ctx, n->fils[1], prop, out)) return 1;
        /* ÉCHOUÉ, ET ON SAIT POURQUOI. Sans ceci, la ligne repartait jusqu'à
         * l'écho, et hct_eval concluait « objet introuvable » — même quand le
         * menu ET l'article existaient et que seul le nom de la propriété
         * était inventé. On pose la faute ici : hct_eval garde la PREMIÈRE,
         * donc la sienne, plus vague, ne la remplacera pas. */
        if (ctx && g_menu_echec != V3_MENU_RIEN) {
            if (g_menu_echec == V3_MENU_PROP_INCONNUE)
                hct_ctx_faute_nom(ctx, n, "propriété de menu inconnue", prop);
            else
                hct_ctx_faute(ctx, n, v3_menu_raison());
            { g_v1_porte = sauve_porte; } return 0;
        }
    }

    /* « the selectedButton of [card|bg] family <n> ». Même forme que les
     * menus juste au-dessus, et pour la même raison : une famille n'est pas
     * un Object, donc hct_resout ne peut rien en faire et c'est ici qu'on la
     * sert. */
    if (n->genre == HCTN_OF && n->nfils >= 2 &&
        n->fils[0] && n->fils[0]->genre == HCTN_IDENT &&
        n->fils[1] && n->fils[1]->genre == HCTN_OBJET &&
        n->fils[1]->typeobj == HCT_OBJ_FAMILY) {
        char prop[64];
        hct_texte(&n->fils[0]->jeton, prop, sizeof prop);
        if (ci_equal(prop, "selectedbutton")) {
            char nom[HC_NOM_MAX];
            if (v3_famille_bouton_choisi(ctx, n->fils[1], nom, sizeof nom)) {
                *out = hct_val_texte(nom);
                { g_v1_porte = sauve_porte; } return 1;
            }
        }
    }

    /* « the number of menuItems of menu "X" » : un comptage, dont l'arbre
     * emboîte deux « of ». */
    if (n->genre == HCTN_OF && n->nfils >= 2 &&
        n->fils[0] && n->fils[0]->genre == HCTN_IDENT &&
        n->fils[1] && n->fils[1]->genre == HCTN_OF &&
        n->fils[1]->nfils >= 2) {
        char quoi[32], sorte[32];
        hct_texte(&n->fils[0]->jeton, quoi, sizeof quoi);
        const HctNoeud *dedans = n->fils[1];
        if (ci_equal(quoi, "number") &&
            dedans->fils[0] && dedans->fils[0]->genre == HCTN_IDENT &&
            dedans->fils[1] && dedans->fils[1]->genre == HCTN_OBJET &&
            dedans->fils[1]->typeobj == HCT_OBJ_MENU) {
            hct_texte(&dedans->fils[0]->jeton, sorte, sizeof sorte);
            if (ci_equal(sorte, "menuitems") || ci_equal(sorte, "menuitem")) {
                int i = v3_menu_index(NULL, dedans->fils[1]);
                char b[24];
                snprintf(b, sizeof b, "%d", i >= 0 ? g_menus[i].n : 0);
                *out = hct_val_texte(b);
                return 1;
            }
        }
    }

    /* Un comptage d'objets se fait ici, sans repasser par le texte. Avant
     * le relevé : ce n'est plus un retour vers l'ancien interpréteur. */
    if (n->genre == HCTN_OF && n->nfils >= 2 &&
        n->fils[0] && n->fils[0]->genre == HCTN_IDENT) {
        char quoi[32];
        hct_texte(&n->fils[0]->jeton, quoi, sizeof quoi);

        /* « the number of menus » : les menus ne sont pas des objets de la
         * pile — pas de nœud HCTN_OBJET pour eux —, donc v3_nombre_objets ne
         * peut rien en dire. Le modèle est ici, la réponse aussi. */
        if (ci_equal(quoi, "number") && n->fils[1] &&
            n->fils[1]->genre == HCTN_IDENT) {
            char sorte[32];
            hct_texte(&n->fils[1]->jeton, sorte, sizeof sorte);
            if (ci_equal(sorte, "menus")) {
                char b[24];
                snprintf(b, sizeof b, "%d", hc_menu_nombre());
                *out = hct_val_texte(b);
                return 1;
            }
        }

        int compte;
        if (ci_equal(quoi, "number") && v3_nombre_objets(n->fils[1], &compte)) {
            char b[24];
            snprintf(b, sizeof b, "%d", compte);
            *out = hct_val_texte(b);
            return 1;
        }
    }

    /* UNE CIBLE QUI SE CALCULE.
     *
     *     put the short name of (the name of card button "Bouton")
     *
     * L'arbre est juste — of(short name, of(name, objet)) —, mais hct_resout
     * ne sait résoudre qu'un nœud OBJET. La cible étant elle-même un « of »,
     * la résolution renonçait, on arrivait ici, et l'ancien moteur rendait le
     * TEXTE DE LA DEMANDE : « short name of (the name of card button… ».
     * Sans erreur, comme toujours avec cette famille-là.
     *
     * C'est la même que « the short name of o » quand o est une variable,
     * corrigée dans v3_resout. Mais celle-ci NE PEUT PAS se corriger au même
     * endroit, et c'est la leçon de ce défaut :
     *
     * hct_eval appelle resout AVANT recours. Une sonde posée dans resout
     * s'exécute donc avant que les cas particuliers d'ici aient eu leur tour,
     * et elle évalue des cibles que le recours sait servir entières. Mesuré :
     * « the number of menuItems of menu "3DEquations" » descendait dans la
     * sonde, qui évaluait « menuItems of menu "3DEquations" » toute seule,
     * n'y arrivait pas, et laissait passer trois « objet introuvable » émis
     * directement vers l'hôte par eval_expr. menuprop et tortureh l'ont dit.
     *
     * La sonde a donc sa place ICI, après les cas particuliers et avant le
     * retour au texte — au dernier moment où il reste quelque chose à tenter.
     *
     * Un contexte NEUF, comme le fait eval_expr : les variables sont
     * globales, il ne manque donc rien, et une sonde qui échoue garde son
     * erreur pour elle au lieu de la poser sur l'évaluation en cours.
     *
     * On n'évalue QUE des formes de LECTURE — un « of », une chaîne, un
     * morceau — jamais un appel de fonction, dont l'évaluation spéculative
     * pourrait avoir des effets. Le garde de profondeur ferme la récursion :
     * la sonde rappelle l'évaluateur, donc peut revenir ici. */
    if (n->genre == HCTN_OF && n->nfils >= 2 &&
        n->fils[0] && n->fils[0]->genre == HCTN_IDENT &&
        n->fils[1] && v3_cible_calculable(n->fils[1])) {
        static int prof_sonde = 0;
        if (prof_sonde < 4) {
            prof_sonde++;
            HctContexte sonde;
            hct_ctx_init(&sonde, v3_hote());
            HctValeur v = hct_evalue(&sonde, n->fils[1]);
            Object *cible = NULL;
            int ressemblait = 0;
            if (!sonde.erreur && v.txt && v.txt[0]) {
                ressemblait = v3_ressemble_a_un_objet(v.txt);
                cible = resolve(v.txt);
            }
            hct_val_libere(&v);
            if (cible) {
                char prop[64];
                hct_texte(&n->fils[0]->jeton, prop, sizeof prop);
                if (v3_lit_prop(NULL, cible, prop, out)) {
                    prof_sonde--;
                    g_v1_porte = sauve_porte;
                    return 1;
                }
            } else {
                /* AUCUNE CIBLE. Deux raisons de le signaler plutôt que de
                 * laisser l'ancien moteur rendre le texte de la question :
                 *
                 *   - la cible s'ÉCRIVAIT comme un objet et n'en désigne
                 *     aucun — la tromperie de « field "menu" » ;
                 *   - ou la PROPRIÉTÉ en exige un, quelle que soit la tête de
                 *     la cible : « the short name of z » où z contient du
                 *     texte ordinaire n'a pas de réponse, et rendre « short
                 *     name of z » en tient lieu depuis trop longtemps.
                 *
                 * Le garde final rend alors 0, et hct_eval lève « objet
                 * introuvable » en nommant la ligne. */
                char prop[64];
                hct_texte(&n->fils[0]->jeton, prop, sizeof prop);
                if (ressemblait || v3_prop_exige_un_objet(prop))
                    sonde_manquee = 1;
            }
            prof_sonde--;
        }
    }

    if (n->genre == HCTN_OF && n->nfils >= 1 &&
        n->fils[0] && n->fils[0]->genre == HCTN_IDENT) {
        char nom[32], cle[40];
        hct_texte(&n->fils[0]->jeton, nom, sizeof nom);
        snprintf(cle, sizeof cle, "of %s", nom);
        v3_note("recours", cle);
    } else {
        char frag[32];
        v3_source(n, frag, sizeof frag);
        for (char *p = frag; *p; p++) if (*p == '\n' || *p == '\t') *p = ' ';
        char cle[40];
        if (*frag) snprintf(cle, sizeof cle, "%s: %s", hct_genre_noeud_nom(n->genre), frag);
        else       snprintf(cle, sizeof cle, "%s", hct_genre_noeud_nom(n->genre));
        v3_note("recours", cle);
    }

    if (g_v3_recours_prof > 64) {
        emit(HC_ERR, "   !! évaluation trop imbriquée");
        *out = hct_val_texte("");
        { g_v1_porte = sauve_porte; } return 1;
    }

    /* Les tampons vont dans l'ARÈNE, pas sur la pile.
     *
     * Deux tampons en HC_VAL déclarés en variables locales faisaient réserver
     * deux mégaoctets de pile à chaque appel — débordement dès qu'un script
     * enchaînait les évaluations, et le débogueur s'arrêtait sur un
     * « subq $0x200a10, %rsp » sans explication.
     *
     * Mais 8 Ko ne suffisent pas davantage : le calendrier LIT SON PROPRE
     * SCRIPT, qui fait 9 Ko, pour y réécrire ses données. Le garde-fou de
     * hc_core refusait alors toute réécriture — à juste titre, puisque écrire
     * un script tronqué l'aurait détruit.
     *
     * L'arène règle les deux d'un coup : pleine taille, aucune pression sur
     * la pile, et eval_expr la rembobine à chaque expression. */
    ARENA_MARK;
    char *txt = arena_buf();
    char *val = arena_buf();

    v3_source(n, txt, HC_VAL);
 
    if (!txt[0]) { ARENA_FREE; return 0; }

    /* Un appel de fonction demande DEUX rattrapages.
     *
     * 1. Les parenthèses ne sont dans aucun nœud : l'arbre ne retient que le
     *    nom et les arguments. La reconstitution rendait « dayNameData » au
     *    lieu de « dayNameData() », et « FindHandler("a","b",c » sans sa
     *    parenthèse fermante.
     *
     * 2. Et il faut un ESPACE avant la parenthèse ouvrante. next_word() de
     *    hc_core.c ne découpe que sur les blancs : avec « dayNameData() »
     *    elle croit que la fonction s'appelle « dayNameData() », parenthèses
     *    comprises, et ne trouve rien. Avec « dayNameData () » elle lit le
     *    nom, puis reconnaît la liste d'arguments.
     *
     * Sans ces deux corrections, le calendrier affichait « dayNameData() »
     * en toutes lettres à la place de ses jours de la semaine. */
    if (n->genre == HCTN_APPEL) {
        size_t l = strlen(txt);
        char *par = strchr(txt, '(');

        if (par && l + 2 < (size_t)HC_VAL) {
            size_t pos = (size_t)(par - txt);
            memmove(txt + pos + 1, txt + pos, l - pos + 1);  /* zéro compris */
            txt[pos] = ' ';
            l++;
            txt[l] = ')';
            txt[l + 1] = '\0';
        } else if (!par && l + 3 < (size_t)HC_VAL) {
            txt[l] = ' '; txt[l + 1] = '('; txt[l + 2] = ')'; txt[l + 3] = '\0';
        }
    }

    g_v3_recours_prof++;
    val[0] = '\0';
    term_value(txt, val, HC_VAL);

    /* L'analyseur consomme « the » sans le ranger dans aucun nœud : la
     * reconstitution rend « target » au lieu de « the target », et
     * « value of x » au lieu de « the value of x ». term_value ne reconnaît
     * pas ces formes tronquées et retombe sur son littéral non quoté — elle
     * rend LE TEXTE DE LA DEMANDE.
     *
     * On réessaie donc quand le résultat est identique à la demande, mais
     * SURTOUT PAS quand il est vide : « the result » vaut légitimement vide
     * lorsque tout s'est bien passé, et traiter ce vide comme un échec
     * rendait « result » en clair. Le calendrier voyait alors
     * « if the result <> empty » toujours vrai et refusait toutes les dates. */
    int echo = 0;
    /* Un nœud d'OBJET ne gagne rien à être redemandé avec « the » devant :
     * « the field "menu" » n'est pas une tournure d'HyperTalk. On s'épargne
     * ce second appel, qui doublait le coût de chaque référence absente. */
    if (n->genre == HCTN_OBJET && strcmp(val, txt) == 0) echo = 1;
    else if (strcmp(val, txt) == 0) {
        char *avec_the = arena_buf();
        snprintf(avec_the, HC_VAL, "the %s", txt);
        term_value(avec_the, val, HC_VAL);

        /* Si même avec « the » rien de neuf ne sort, on rend le texte
         * d'origine plutôt que « the value of x » : c'est ce que faisait
         * l'ancien évaluateur, et un script peut s'appuyer dessus. */
        if (strcmp(val, avec_the) == 0) {
            snprintf(val, HC_VAL, "%s", txt);
            echo = 1;
        }
    }

    /* UNE RÉFÉRENCE D'OBJET NE SE REND PAS ELLE-MÊME EN CLAIR.
     *
     * « put field "menu" », quand ce champ n'existe nulle part, affichait
     * field "menu" — le texte de la demande. C'est la même tromperie que
     * « the zorglub » rendant zorglub, corrigée en son temps pour les
     * propriétés : un mot nu peut légitimement valoir lui-même, une
     * référence d'objet jamais. L'auteur a écrit « field », il désigne un
     * champ, et s'il n'y en a pas il faut le dire.
     *
     * On rend 0 : hct_eval lève alors « objet introuvable » en nommant la
     * ligne. L'ancien moteur a déjà eu sa chance juste au-dessus — il peut
     * résoudre des formes que hct_resout ignore, et celles-là passent. Seul
     * l'ÉCHEC des deux change de comportement.
     *
     * Trouvé par le relevé d'un test de navigation : huit « recours objet:
     * field "menu" » qui ne se voyaient nulle part ailleurs, le script
     * travaillant tranquillement sur la chaîne « field "menu" ». */
    /* UN APPEL DE FONCTION NON PLUS NE SE REND PAS LUI-MÊME EN CLAIR.
     *
     * Même raisonnement que pour les références d'objet, et même défaut :
     *
     *     put maFonction("ok")      -- la fonction n'existe nulle part
     *     -> maFonction ("ok")
     *
     * Le script continuait sans broncher, et cette chaîne — avec l'espace
     * que la reconstitution insère avant la parenthèse — partait dans un
     * champ, dans une comparaison, dans un calcul. Une faute de frappe dans
     * un nom de fonction ne disait donc RIEN, et le résultat ressemblait
     * assez à du texte pour passer inaperçu longtemps.
     *
     * Un mot nu peut légitimement valoir lui-même ; une PARENTHÈSE
     * D'APPEL jamais. L'auteur qui écrit « maFonction("ok") » demande un
     * calcul, pas une citation. Si personne ne sait le faire, il faut le
     * dire — hct_eval lève « fonction inconnue : maFonction » en nommant la
     * ligne.
     *
     * Le cas où la fonction EXISTE ne passe pas par ici : term_value la
     * trouve, rend autre chose que la demande, et il n'y a pas d'écho. Seul
     * l'échec des deux moteurs change de comportement.
     *
     * Effet de bord mesuré, et bienvenu : l'échec coûtait QUATRE parcours
     * complets de la chaîne des messages — term_value, la reprise avec
     * « the », puis parse_expr — pour un nom que personne ne connaît. Sortir
     * ici en supprime la moitié. */
    if (echo && (n->genre == HCTN_OBJET || n->genre == HCTN_APPEL ||
                 v3_prop_sur_objet(n) ||
                 sonde_manquee || v3_prop_inconnue_sur_objet(n))) {
        ARENA_FREE;
        g_v3_recours_prof--;
        { g_v1_porte = sauve_porte; } return 0;
    }

    /* Dernier recours : l'ANCIEN analyseur.
     *
     * Certaines tournures ne vivent que dans parse_factor et n'ont jamais été
     * portées dans term_value — « there is a <objet> » en est une. term_value
     * rend alors le texte inchangé, ce qui est justement le signe qu'elle n'a
     * rien reconnu ; on passe la main à parse_expr, qui les connaît.
     *
     * Sans cela, « if there is a cd btn "Drawgraph" » rendait la chaîne
     * elle-même, jamais true ni false. */
    if (strcmp(val, txt) == 0) {
        const char *q = txt;
        char *essai = arena_buf();
        essai[0] = '\0';
        parse_expr(&q, essai, HC_VAL);
        if (essai[0] && strcmp(essai, txt) != 0)
            snprintf(val, HC_VAL, "%s", essai);
    }

    g_v3_recours_prof--;

    *out = hct_val_texte(val);
    ARENA_FREE;
    g_v1_porte = sauve_porte;
    return 1;
}
/* Fonction définie dans une pile — « function calData … ».
 *
 * Les arguments sont DÉJÀ évalués quand ils nous arrivent : on les passe tels
 * quels à la chaîne de messages, au lieu de fabriquer « calData(3) » pour le
 * faire relexer par call_function, qui rappelait la v3 aussitôt. C'est cette
 * boucle-là qui se referme.
 *
 * Le tampon vient de l'arène, comme dans call_function_body : huit lignes de
 * HC_VAL ne tiendraient pas sur la pile. */
static int v3_fonction_pile(const char *nom, HctValeur *args, int nargs)
{
    Object *from = g_me ? g_me : g_current_card;
    /* Pas de contrôle du nombre d'arguments ici : hc_send_args_k_body le fait
     * pour tout le monde, et il n'y a qu'une seule limite dans le noyau.
     * Celui qui se trouvait à cet endroit était juste, mais il était SEUL —
     * le message et « send » tronquaient en silence pendant qu'il refusait
     * proprement. */
    char (*uargv)[HC_VAL] = nargs ? arena_rows(nargs) : NULL;
    if (nargs && !uargv) return 0;
    for (int i = 0; i < nargs; i++)
        snprintf(uargv[i], HC_VAL, "%s", args[i].txt ? args[i].txt : "");
    return hc_call_user_function(from, nom, uargv, nargs);
}
/* Fonctions du monde sans argument que l'hôte sert d'une seule lecture.
 *
 * Elles passaient par term_value avec une chaîne fabriquée — « the mouseLoc »
 * relexée, réanalysée, pour finir sur le host_global qu'on appelle ici
 * directement. Trois cent dix-neuf allers-retours dans une seule boucle
 * « repeat while the mouse is down ».
 *
 * Une liste explicite plutôt qu'un appel à host_global pour tout nom inconnu :
 * term_value traite bien des choses AVANT d'en arriver là — les constantes,
 * les dates, « the result » —, et court-circuiter aveuglément déplacerait
 * l'ordre de priorité sans qu'on s'en aperçoive. Ajouter un nom ici est une
 * ligne, et c'est le bon prix pour ne pas casser une règle par accident.
 *
 * Ordre vérifié pour chacun de ces noms : dans term_value_body, seules les
 * constantes, la liste sans argument de call_function_body et les variables
 * passent avant host_global. resolve() n'attrape rien ici — un mot nu sans
 * mot-clé de type ne désigne aucun objet (vérifié : un champ nommé
 * « pattern » ne répond pas à « put pattern », ni en v1 ni en v3). Un nom
 * que l'hôte ignore rend NULL et reprend le chemin normal, si bien qu'en
 * lister un de trop ne coûte rien. */
static const char *V3_GLOBALES_HOTE[] = {
    "mouse", "mouseLoc", "mouseH", "mouseV",
    "clickLoc", "clickH", "clickV",
    "clickChunk", "clickLine", "clickText",
    "mouseClick", "mouseLine",
    "shiftKey", "optionKey", "commandKey", "cmdKey",
    "tool", "screenRect",
    /* réglages de peinture et de texte, tenus par l'hôte */
    "textHeight", "textSize", "textFont", "textStyle", "textAlign",
    "filled", "lineSize", "pattern", "brush", "grid", "polySides",
    "transparent", "centered",
    "cursor", "editBkgnd",
    "foreColor", "backColor", "foregroundColor", "backgroundColor",
    "paintColor", "paintBackColor", "inkColor",
    /* L'état de la MACHINE. Le noyau ne sait rien de l'espace disque ni de la
     * version du système, et inventer un chiffre serait pire que se taire :
     * un script qui vérifie « if the diskSpace < 100000 » mérite une vraie
     * réponse ou une vraie erreur, pas une valeur décorative. On les confie
     * donc à l'hôte, qui peut les connaître ; s'il ne répond pas, « the » dit
     * franchement qu'il ne sait pas.
     *
     * PAS « the size » ni « the freeSize » : chez HyperCard ce sont des
     * propriétés de la PILE — sa taille de fichier et l'espace que les
     * suppressions y ont laissé —, pas de la machine. Les mettre ici les
     * aurait détournées de leur sens. */
    "diskSpace", "heapSpace", "systemVersion", "windows", "programs",
    NULL
};

/* CELLES QU'UN SCRIPT PEUT ÉCRIRE, parmi les précédentes.
 *
 * La liste au-dessus dit ce qui EXISTE ; celle-ci dit ce qui se POSE. La
 * souris, les touches, le rectangle d'écran, l'espace disque se lisent et ne
 * s'écrivent pas — « set the screenRect to … » n'a aucun sens, et l'accepter
 * en silence serait exactement le défaut qu'on corrige, déplacé d'un nom à
 * l'autre.
 *
 * Elle recense ce que l'hôte accepte réellement : les douze branches de
 * cocoa_global_set, plus la famille des couleurs de peinture qu'il traite à
 * part. lockScreen y figure parce que le noyau le lit ET le laisse passer —
 * c'est la seule des trois propriétés du noyau à être partagée. */
static const char *V3_GLOBALES_ECRIVABLES[] = {
    "lockScreen", "editBkgnd", "cursor",
    "filled", "lineSize", "pattern", "brush", "grid", "polySides",
    "transparent", "centered",
    "textHeight", "textSize", "textFont", "textStyle", "textAlign",
    "foreColor", "backColor", "foregroundColor", "backgroundColor",
    "paintColor", "paintBackColor", "inkColor",
    NULL
};

static int dans_liste(const char *nom, const char **liste)
{
    for (int i = 0; liste[i]; i++)
        if (ci_equal(nom, liste[i])) return 1;
    return 0;
}

/* Les propriétés du monde sans argument que call_function_body servait en
 * relexant « the » + nom : the result, the date (et ses formes longues et
 * courtes), the selection, the paramCount... Toutes des lectures directes
 * de globales déjà accessibles ici — aucune n'a besoin de l'ancien
 * interpréteur, seulement de sa liste, reproduite terme à terme pour ne
 * rien oublier ni rien réordonner.
 *
 * `buf` vient de l'arène de l'appelant (HC_VAL, un mégaoctet) : seul
 * « stacksInUse » et « the params » peuvent approcher cette taille, mais
 * les deux se servent du même tampon plutôt que d'ajouter une variante.
 *
 * Rend 0 si nom n'est reconnu par rien ici : l'appelant retombe alors sur
 * term_value, exactement comme avant. */
static int v3_fonction_globale(const char *nom, char *buf, HctValeur *out)
{
    /* formes composées : long date, short time, abbreviated date… */
    int datemode = -1;
    if (ci_word(nom, "long")) {
        const char *w = skip_spaces(nom + 4);
        if (ci_word(w, "date")) datemode = 2; else if (ci_word(w, "time")) datemode = 4;
    } else if (ci_word(nom, "short")) {
        const char *w = skip_spaces(nom + 5);
        if (ci_word(w, "date")) datemode = 0; else if (ci_word(w, "time")) datemode = 3;
    } else if (ci_word(nom, "abbreviated") || ci_word(nom, "abbrev") || ci_word(nom, "abbr")) {
        const char *w = strchr(nom, ' ');
        if (w && ci_word(skip_spaces(w), "date")) datemode = 1;
    }
    if (datemode >= 0) {
        char petit[128];
        format_date(petit, sizeof petit, datemode);
        *out = hct_val_texte(petit);
        return 1;
    }
    /* « the stacks » : les piles ouvertes, une par ligne. Le noyau tient déjà
     * ce registre — c'est celui qui permet à « go to stack "X" » de trouver
     * une pile déjà ouverte. */
    if (ci_equal(nom, "stacks")) {
        buf[0] = '\0';
        size_t pris = 0;
        for (int i = 0; i < hc_stack_count(); i++) {
            Object *st = hc_stack_at(i);
            const char *nm = st && st->name ? st->name : "";
            size_t l = strlen(nm);
            if (pris + l + 2 >= (size_t)HC_VAL) break;
            if (pris) buf[pris++] = '\n';
            memcpy(buf + pris, nm, l); pris += l;
            buf[pris] = '\0';
        }
        *out = hct_val_texte(buf);
        return 1;
    }

    /* « the menus » : les menus de la barre créés par script, une par ligne.
     * Le modèle vit ici, l'hôte n'en tient qu'un reflet. */
    if (ci_equal(nom, "menus")) {
        buf[0] = '\0';
        size_t pris = 0;
        for (int i = 0; i < hc_menu_nombre(); i++) {
            const char *nm = hc_menu_nom(i);
            if (!nm) continue;
            size_t l = strlen(nm);
            if (pris + l + 2 >= (size_t)HC_VAL) break;
            if (pris) buf[pris++] = '\n';
            memcpy(buf + pris, nm, l); pris += l;
            buf[pris] = '\0';
        }
        *out = hct_val_texte(buf);
        return 1;
    }

    /* « the recent cards » : les cartes visitées, la plus récente d'abord,
     * une par ligne. « the recent names » rend les mêmes sous leur nom court.
     * L'historique est celui qu'alimente openCard ; le menu Recent d'HyperCard
     * le lisait de la même façon. */
    if (ci_equal(nom, "recent cards") || ci_equal(nom, "recent names")) {
        int courts = ci_equal(nom, "recent names");
        buf[0] = '\0';
        size_t pris = 0;
        /* Sans doublon, comme le menu Recent : « the recent cards » est la
         * même liste, et la répétition n'y apprend rien de plus. */
        Object *vues[HC_HISTO_MAX];
        int nv = hc_recent_distinct(vues, HC_HISTO_MAX);
        for (int i = 0; i < nv; i++) {
            Object *c = vues[i];
            char d[160];
            if (courts) snprintf(d, sizeof d, "%s", c->name ? c->name : "");
            else        hc_describe(c, d, sizeof d);
            size_t l = strlen(d);
            if (pris + l + 2 >= (size_t)HC_VAL) break;
            if (pris) buf[pris++] = '\n';
            memcpy(buf + pris, d, l); pris += l;
            buf[pris] = '\0';
        }
        *out = hct_val_texte(buf);
        return 1;
    }
    if (ci_equal(nom, "date")) {
        char petit[128]; format_date(petit, sizeof petit, 0);
        *out = hct_val_texte(petit); return 1;
    }
    if (ci_equal(nom, "time")) {
        char petit[128]; format_date(petit, sizeof petit, 3);
        *out = hct_val_texte(petit); return 1;
    }
    if (ci_equal(nom, "result")) { *out = hct_val_texte(g_result); return 1; }
    if (ci_equal(nom, "foundtext")) { *out = hct_val_texte(g_found_text); return 1; }
    if (ci_equal(nom, "stacksinuse")) {
        buf[0] = '\0';
        size_t used = 0;
        for (int i = 0; i < g_nusing; i++) {
            const char *nm = g_using[i]->name ? g_using[i]->name : "";
            size_t l = strlen(nm);
            if (used + l + 2 >= (size_t)HC_VAL) break;
            if (i) buf[used++] = '\n';
            memcpy(buf + used, nm, l); used += l;
            buf[used] = '\0';
        }
        *out = hct_val_texte(buf);
        return 1;
    }
    if (ci_equal(nom, "selection") || ci_equal(nom, "selectedtext")) {
        selection_text(buf, HC_VAL);
        *out = hct_val_texte(buf);
        return 1;
    }
    if (ci_equal(nom, "selectedfield")) {
        char petit[96];
        if (g_sel_field) hc_describe(g_sel_field, petit, sizeof petit);
        else petit[0] = '\0';
        *out = hct_val_texte(petit);
        return 1;
    }
    if (ci_equal(nom, "selectedline")) {
        if (!g_sel_field) { *out = hct_val_texte(""); return 1; }
        const char *t = hc_field_text(g_sel_field);
        int line = 1;
        for (int i = 0; i < g_sel_start && t[i]; i++)
            if (t[i] == '\n') line++;
        char petit[16]; snprintf(petit, sizeof petit, "%d", line);
        *out = hct_val_texte(petit);
        return 1;
    }
    if (ci_equal(nom, "selectedchunk")) {
        if (!g_sel_field) { *out = hct_val_texte(""); return 1; }
        char d[96];
        hc_describe(g_sel_field, d, sizeof d);
        char petit[160];
        snprintf(petit, sizeof petit, "char %d to %d of %s%s",
                 hct_utf8_compte_prefixe(hc_field_text(g_sel_field),
                                         g_sel_start) + 1,
                 hct_utf8_compte_prefixe(hc_field_text(g_sel_field),
                                         g_sel_start + g_sel_len),
                 hc_owner_is_bg(g_sel_field) ? "bg " : "card ", d);
        *out = hct_val_texte(petit);
        return 1;
    }
    if (ci_equal(nom, "foundchunk")) {
        if (!g_found_field || g_found_len <= 0) { *out = hct_val_texte(""); return 1; }
        char d[96];
        hc_describe(g_found_field, d, sizeof d);
        char petit[160];
        snprintf(petit, sizeof petit, "char %d to %d of %s%s",
                 hct_utf8_compte_prefixe(hc_field_text(g_found_field),
                                         g_found_start) + 1,
                 hct_utf8_compte_prefixe(hc_field_text(g_found_field),
                                         g_found_start + g_found_len),
                 hc_owner_is_bg(g_found_field) ? "bg " : "card ", d);
        *out = hct_val_texte(petit);
        return 1;
    }
    if (ci_equal(nom, "foundfield")) {
        char petit[96];
        if (g_found_field) hc_describe(g_found_field, petit, sizeof petit);
        else petit[0] = '\0';
        *out = hct_val_texte(petit);
        return 1;
    }
    if (ci_equal(nom, "foundline")) {
        if (g_found_field && g_found_line > 0) {
            char d[96]; hc_describe(g_found_field, d, sizeof d);
            char petit[128];
            snprintf(petit, sizeof petit, "line %d of %s", g_found_line, d);
            *out = hct_val_texte(petit);
        } else *out = hct_val_texte("");
        return 1;
    }
    if (ci_equal(nom, "paramcount")) {
        char petit[16]; snprintf(petit, sizeof petit, "%d", g_nparams - 1);
        *out = hct_val_texte(petit);
        return 1;
    }
    if (ci_equal(nom, "params")) {
        buf[0] = '\0';
        int pos = 0;
        for (int i = 0; i < g_nparams; i++)
            pos = ajoute_borne(buf, HC_VAL, pos, i ? "," : "", g_params[i]);
        *out = hct_val_texte(buf);
        return 1;
    }
    if (ci_equal(nom, "seconds") || ci_equal(nom, "secs")) {
        char petit[24];
        snprintf(petit, sizeof petit, "%lld", (long long)hc_maintenant() + HC_MAC_EPOCH);
        *out = hct_val_texte(petit);
        return 1;
    }
    if (ci_equal(nom, "ticks")) {
        /* comme dans call_function_body : l'hôte d'abord, lui seul a une
         * horloge fine ; le repli compte depuis le premier appel. */
        const char *hv = host_global(nom);
        if (hv && *hv) { *out = hct_val_texte(hv); return 1; }
        static time_t t0;
        static int t0_pris = 0;
        time_t now = hc_maintenant();
        if (!t0_pris) { t0 = now; t0_pris = 1; }
        char petit[24];
        snprintf(petit, sizeof petit, "%lld", (long long)(now - t0) * 60);
        *out = hct_val_texte(petit);
        return 1;
    }

    /* Les réglages que le noyau tient — voir prop_globale_noyau, qui les
     * POSE. Servis ici pour qu'ils soient rendus par l'exécuteur v3 et non
     * par un retour vers l'ancien : debug bilan reste (aucun). Le miroir de
     * l'écriture doit vivre du même côté qu'elle. */
    if (ci_equal(nom, "numberformat")) {
        *out = hct_val_texte(hct_format_nombre_lu()); return 1;
    }
    if (ci_equal(nom, "lockscreen")) {
        *out = hct_val_texte(g_ecran_verrouille ? "true" : "false"); return 1;
    }
    if (ci_equal(nom, "lockmessages")) {
        *out = hct_val_texte(g_messages_verrouilles ? "true" : "false"); return 1;
    }
    if (reglage_lit(nom, buf, HC_VAL)) { *out = hct_val_texte(buf); return 1; }

    /* ═══ LES FONCTIONS DU MONDE QUE LA v3 NE SAVAIT PAS NOMMER ═══════
     *
     * Mesuré avant d'écrire une ligne : sur 192 harnais, 556 entrées dans
     * term_value / call_function, dont 348 — 63 % — sont des SONDES DE NOM,
     * une par nom et par processus. Et sur 118 noms sondés un par un, l'ancien
     * moteur n'en sert qu'UN SEUL : « the tool ». Tous les autres renvoient le
     * mot qu'on leur a donné.
     *
     * Ce qui manquait n'était donc pas un portage : c'était une LISTE. Voici
     * la part que le noyau peut tenir lui-même.
     *
     * « the tool » d'abord, parce que c'est le seul que l'ancien moteur
     * servait vraiment, et pour une raison minuscule : il a un DÉFAUT — « browse
     * tool » — que la boucle sur V3_GLOBALES_HOTE n'avait pas. Quand l'hôte se
     * tait, elle renonçait et sondait. Le défaut vit maintenant des deux côtés,
     * et la dernière sonde utile disparaît. */
    if (ci_equal(nom, "tool")) {
        const char *t = host_global("tool");
        *out = hct_val_texte((t && *t) ? t : "browse tool");
        return 1;
    }

    /* La version de HC. L'hôte fait foi — c'est lui qui porte
     * MARKETING_VERSION —, le noyau répond pour tout ce qui tourne sans lui. */
    if (ci_equal(nom, "version")) {
        const char *v = host_global("version");
        *out = hct_val_texte((v && *v) ? v : HC_VERSION);
        return 1;
    }

    /* « the language » : la langue des SCRIPTS, pas celle de l'utilisatrice.
     * HyperCard rendait le nom du dialecte dans lequel on écrit, et ici on
     * écrit de l'HyperTalk anglais — « put », « repeat », « end ». Rendre
     * « French » parce que les commentaires le sont serait une jolie erreur :
     * un script qui teste cette valeur pour choisir ses mots-clés se
     * tromperait de mots. */
    if (ci_equal(nom, "language")) {
        const char *l = host_global("language");
        *out = hct_val_texte((l && *l) ? l : "English");
        return 1;
    }

    /* « the sound » : ce qui joue en ce moment, « done » quand rien ne joue.
     * Seul l'hôte a une carte son ; sans lui, rien ne joue, et c'est une
     * réponse exacte — pas une valeur décorative. */
    if (ci_equal(nom, "sound")) {
        const char *sn = host_global("sound");
        *out = hct_val_texte((sn && *sn) ? sn : "done");
        return 1;
    }

    /* « the destination » : la pile vers laquelle on va. Chez HyperCard elle
     * se lit pendant un openStack, pour savoir d'où l'on vient et où l'on va ;
     * hors navigation elle nomme la pile courante. C'est le nom LONG, celui
     * qui porte le chemin depuis que hc_set_stack_path existe — deux piles
     * peuvent porter le même nom, leur chemin non. */
    if (ci_equal(nom, "destination")) {
        Object *st = g_current_card ? owning_stack(g_current_card) : NULL;
        if (!st) { *out = hct_val_texte(""); return 1; }
        char nm[512];
        hc_nom_de(st, HC_NOM_LONG, nm, sizeof nm);
        *out = hct_val_texte(nm);
        return 1;
    }

    /* « the size » et « the freeSize » : des propriétés du FICHIER de la pile,
     * pas de la machine — c'est pourquoi elles ne sont pas dans
     * V3_GLOBALES_HOTE avec diskSpace et heapSpace.
     *
     * size est la taille du fichier ; -1 devient 0, une pile jamais
     * enregistrée n'occupant rien. freeSize est l'espace que les suppressions
     * ont laissé DANS le fichier : hc_save le réécrit entier à chaque fois, il
     * n'en laisse jamais. Zéro n'est pas ici un aveu d'ignorance, c'est la
     * réponse juste, et « if the freeSize > 0 then compact » ne compactera
     * donc jamais pour rien. */
    if (ci_equal(nom, "size") || ci_equal(nom, "freesize")) {
        char petit[24];
        long n = 0;
        if (ci_equal(nom, "size")) {
            Object *st = g_current_card ? owning_stack(g_current_card) : NULL;
            long t = st ? hc_taille_fichier(hc_stack_path(st)) : -1;
            if (t > 0) n = t;
        }
        snprintf(petit, sizeof petit, "%ld", n);
        *out = hct_val_texte(petit);
        return 1;
    }

    return 0;
}

/* ═══ Les noms dont l'ancien moteur n'a jamais rien su faire ═══════════
 *
 * « setFont geneva,10,center,bold » passe quatre mots nus en arguments.
 * Chacun est évalué, donc cherché comme fonction, donc confié à term_value —
 * qui ne le connaît pas et rend le mot lui-même. L'appel n'apprend rien, et
 * il recommence à chaque tour de boucle : dix-huit fois « geneva », seize
 * fois « plain », huit fois « left » sur une seule séance de Graph Maker.
 *
 * La réponse ne peut pas changer. Ce que term_value sait servir est fixé à
 * la compilation — des fonctions intégrées et des propriétés —, jamais des
 * noms que la pile inventerait en route : ceux-là sont servis APRÈS, par
 * v3_fonction_pile, et n'arrivent donc jamais ici. Un nom qui a échoué une
 * fois échouera toujours ; on le note et on ne redemande plus.
 *
 * On ne note QUE l'écho — le cas où term_value rend le mot qu'on lui a
 * donné. Une réponse vide légitime, « the selection » quand rien n'est
 * sélectionné, ne ressemble pas à un écho et n'est pas mise en cache.
 *
 * Table courte et bornée : les mots nus d'un script se comptent en dizaines,
 * et déborder ne coûte que de refaire l'emprunt comme avant. */
#define V3_MUETS_MAX 64
static char g_muets[V3_MUETS_MAX][40];
static int  g_nmuets = 0;

static int v1_est_muet(const char *nom)
{
    for (int i = 0; i < g_nmuets; i++)
        if (ci_equal(g_muets[i], nom)) return 1;
    return 0;
}

static void v1_note_muet(const char *nom)
{
    if (!nom || !*nom) return;
    if ((int)strlen(nom) >= (int)sizeof g_muets[0]) return;   /* trop long */
    if (g_nmuets >= V3_MUETS_MAX) return;
    if (v1_est_muet(nom)) return;
    snprintf(g_muets[g_nmuets], sizeof g_muets[0], "%s", nom);
    g_nmuets++;
}

/* ═══ CE QUE L'ANCIEN MOTEUR SAIT SERVIR SANS ARGUMENT ════════════════
 *
 * La liste est EXTRAITE de call_function_body et de G_REGLAGES, pas devinée :
 * ce sont tous les noms que « the <nom> » peut y trouver. Elle est fermée, et
 * c'est le but — un nom qui n'y figure pas n'a rien à aller demander là-bas.
 *
 * POURQUOI ELLE EXISTE. v3_fonction sondait l'ancien moteur pour TOUT nom
 * qu'elle ne servait pas elle-même : « est-ce que tu connais ça ? ». Mesuré
 * sur les 192 harnais, cela faisait 348 sondes, 63 % de toutes les entrées
 * dans term_value / call_function. Mesuré nom par nom sur 118 candidats,
 * l'ancien moteur en servait UN : « the tool » — et seulement parce qu'il
 * avait un défaut que la v3 n'avait pas. Ce défaut est maintenant des deux
 * côtés, juste au-dessus.
 *
 * Autrement dit : on posait 348 questions pour une réponse, et la réponse
 * tenait en une ligne. Le reste rendait le mot qu'on avait donné — un écho,
 * que v1_note_muet mettait ensuite en cache. Tout ce mécanisme de mise en
 * cache d'une non-réponse ne servait qu'à amortir une question qu'il ne
 * fallait pas poser.
 *
 * ELLE NE DISPARAÎT PAS POUR AUTANT. La v3 sert aujourd'hui chacun de ces
 * noms avant d'arriver ici, donc la sonde ne part plus jamais. Mais si
 * quelqu'un ajoute demain un nom à call_function_body sans l'ajouter à la v3,
 * cette liste le rattrape au lieu de le perdre en silence. Elle coûte une
 * comparaison de chaînes sur un chemin déjà froid.
 *
 * tests/harnais/mondenoms.c tient l'invariant : chacun de ces noms doit être
 * servi par la v3 SANS sonde. */
static const char *V3_V1_FONCTIONS_0[] = {
    /* call_function_body */
    "date", "time", "result", "seconds", "secs", "ticks",
    "foundchunk", "foundfield", "foundline", "foundtext",
    "selectedchunk", "selectedfield", "selectedline", "selectedtext",
    "selection", "stacksinuse", "params", "paramcount",
    "itemdelimiter", "numberformat", "lockscreen", "lockmessages", "tool",
    /* G_REGLAGES */
    "userlevel", "dragspeed", "blindtyping", "powerkeys",
    "lockrecent", "textarrows",
    NULL
};

/* ═══ CE QUE L'ANCIEN MOTEUR SAIT SERVIR AVEC UN ARGUMENT ════════════
 *
 * Le pendant de la liste au-dessus, pour l'autre porte. Le chemin à un
 * argument numérique n'en avait aucune : il sondait call_function pour TOUT
 * nom, et les noms qui arrivent là sont précisément ceux que la v3 ne sert
 * pas — c'est-à-dire les FONCTIONS DE L'UTILISATEUR. On demandait donc à
 * l'ancien moteur s'il connaissait « double », « spectre », « getPattern »,
 * pour s'entendre répondre non à chaque nouveau nom.
 *
 * Mesuré sur les 197 harnais : 8 sondes, soit 3 % des 252 entrées restantes.
 * Ce n'est plus le gros du trafic — la liste à zéro argument a déjà pris
 * 348 sondes sur 420 — mais c'est la totalité de ce qui reste sur cette
 * porte-là, et ça se ferme de la même façon.
 *
 * LA LISTE EST EXTRAITE, PAS ÉCRITE DE MÉMOIRE. C'est exactement l'erreur
 * commise pour la table des désignateurs : j'avais lu deux des trois sources
 * et « prev » a cessé de marcher. Ici la source est unique — call_function
 * n'appelle que call_function_body — et la liste est le résultat de :
 *
 *   awk 'NR>=5642 && NR<=6021' HC/hc_core.c \
 *     | grep -o 'ci_equal(name, *"[A-Za-z0-9]*"' | sed 's/.*"\(.*\)"/\1/' \
 *     | sort -u
 *
 * Elle contient donc AUSSI les noms sans argument. C'est volontaire : la
 * définition « tout ce que call_function_body connaît » se revérifie d'une
 * commande, alors qu'un tri à la main entre les deux familles serait à
 * refaire — et à rater — à chaque relecture. Un nom en trop coûte une sonde
 * qui serait partie de toute façon ; un nom en moins casse une fonction.
 *
 * tests/harnais/fonctions1.c tient l'invariant : une fonction utilisateur à
 * un argument numérique ne doit produire AUCUNE sonde. */
static const char *V3_V1_FONCTIONS_1[] = {
    "abs", "annuity", "atan", "average", "avg", "charToNum", "compound",
    "cos", "date", "exp", "exp1", "exp2", "foundchunk", "foundfield",
    "foundline", "foundtext", "itemdelimiter", "length", "ln", "ln1",
    "lockmessages", "lockscreen", "log2", "max", "min", "numToChar",
    "numberformat", "offset", "param", "paramcount", "params", "random",
    "result", "round", "seconds", "secs", "selectedchunk", "selectedfield",
    "selectedline", "selectedtext", "selection", "sin", "sqrt",
    "stacksinuse", "sum", "tan", "ticks", "time", "tool", "trunc", "value",
    NULL
};

static int v3_fonction(void *d, const char *nom, HctValeur *args, int nargs,
                       HctValeur *out)
{
    /* L'autre porte sur l'ancien moteur d'expressions : les FONCTIONS que la
     * v3 ne calcule pas elle-même. C'est ce qui restait sous « ? ». */
    const char *sauve_porte = v1_porte("fonction v3");
    (void)d;
    /* itemDelimiter : demandé avant chaque découpage en items. On le sert
     * directement, c'est une globale de hc_core.c. Avant toute allocation :
     * c'est le cas le plus fréquent, et il n'a besoin de rien. */
    if (ci_equal(nom, "itemDelimiter")) {
        const char *sep = item_delim();
        *out = hct_val_texte(sep);
        { g_v1_porte = sauve_porte; } return 1;
    }

    /* « x is a date » : la bibliothèque d'expressions ne connaît pas le
     * calendrier, et il n'est pas question d'y recopier l'analyseur de dates —
     * deux définitions de ce qu'est une date finiraient par diverger, comme
     * elles avaient déjà divergé. Elle nous pose donc la question par ce
     * rappel, sous un nom qui ne peut pas être écrit dans un script : il
     * contient des espaces, aucun identifiant HyperTalk n'en contient. */
    if (nargs == 1 && strcmp(nom, "is a date") == 0) {
        *out = hct_val_bool(hc_est_date(args[0].txt));
        { g_v1_porte = sauve_porte; } return 1;
    }

    /* Les fonctions du monde, servies sans fabriquer ni relexer de chaîne.
     * Comme itemDelimiter, ce chemin n'emprunte rien à l'arène.
     *
     * L'évaluateur a déjà essayé lit_var avant de nous appeler : une pile qui
     * nomme sa variable « mouse » garde donc la priorité, exactement comme
     * dans term_value. */
    if (nargs == 0) {
        for (int i = 0; V3_GLOBALES_HOTE[i]; i++) {
            if (!ci_equal(nom, V3_GLOBALES_HOTE[i])) continue;
            const char *v = host_global(nom);
            if (!v) break;          /* l'hôte l'ignore : chemin normal */
            *out = hct_val_texte(v);
            { g_v1_porte = sauve_porte; } return 1;
        }

        /* the result, the date, the selection... la même liste que
         * call_function_body servait, mais sans relexer « the » + nom.
         * ARENA_MARK/FREE encadrent juste cet essai : le tampon ne survit
         * pas à l'appel, hct_val_texte en a déjà fait une copie. */
        ARENA_MARK;
        char *gbuf = arena_buf();
        int servi = v3_fonction_globale(nom, gbuf, out);
        ARENA_FREE;
        if (servi) return 1;
    }

    /* param(n) : le n-ième paramètre du gestionnaire courant, param(0)
     * étant le nom du message. Une lecture de g_params, rien de plus —
     * elle n'avait aucune raison de repartir chez l'ancien interpréteur. */
    if (nargs == 1 && ci_equal(nom, "param") && hct_est_nombre(args[0].txt)) {
        /* param(10^300) : borné, sinon la conversion est indéfinie. Hors
         * bornes vaut « pas de tel paramètre », donc la chaîne vide — et pas
         * le rang 0, qui est le NOM DU MESSAGE et n'a rien à faire ici. */
        int hors;
        int i = hct_vers_rang(args[0].txt, &hors);
        *out = hct_val_texte((!hors && i >= 0 && i < g_nparams)
                             ? g_params[i] : "");
        { g_v1_porte = sauve_porte; } return 1;
    }

    /* Le tampon vient de l'ARÈNE, plus de la pile.
     *
     * HC_VAL vaut un mégaoctet : « char buf[HC_VAL] » posait tout cela sur la
     * pile à chaque appel. Un mégaoctet passe encore sur le fil principal, qui
     * en a huit — mais v3_fonction est rappelée par term_value, elle-même
     * rappelée par l'évaluateur, et trois ou quatre niveaux d'imbrication
     * suffisaient à toucher la page de garde. D'où un plantage qui ne
     * survenait que sur certains scripts, à la première écriture dans le
     * cadre.
     *
     * ARENA_MARK / ARENA_FREE encadrent l'emprunt : l'arène est une pile, on
     * la rembobine en sortant. */
    ARENA_MARK;
    char *buf = arena_buf();
    if (nargs == 0) {
        /* Une seule porte : term_value, qui appelle elle-même call_function.
         *
         * On distingue « reconnu » de « non reconnu » en comparant au texte
         * de la demande — term_value rendant le littéral quand elle ne sait
         * rien faire. Surtout PAS en testant si le résultat est vide : « the
         * result » vaut légitimement vide quand tout s'est bien passé, et le
         * rejeter comme un échec faisait rendre « result » en clair. Le
         * calendrier voyait alors « if the result <> empty » toujours vrai et
         * refusait toutes les dates. */
        char appel[160];
        snprintf(appel, sizeof appel, "the %s", nom);
        buf[0] = '\0';
        /* Hors de la liste, l'ancien moteur n'a rien à en dire : on ne le
         * dérange pas. Voir V3_V1_FONCTIONS_0 — 348 sondes pour une réponse. */
        if (!dans_liste(nom, V3_V1_FONCTIONS_0)) {
            if (v3_fonction_pile(nom, args, 0)) {
                *out = hct_val_texte(g_result);
                ARENA_FREE;
                { g_v1_porte = sauve_porte; } return 1;
            }
            ARENA_FREE;
            { g_v1_porte = sauve_porte; } return 0;
        }
        if (v1_est_muet(nom)) {
            /* Déjà demandé, déjà sans réponse : on passe directement à la
             * suite, qui est le vrai chemin pour ce nom-là. */
            if (v3_fonction_pile(nom, args, 0)) {
                *out = hct_val_texte(g_result);
                ARENA_FREE;
                { g_v1_porte = sauve_porte; } return 1;
            }
            ARENA_FREE;
            { g_v1_porte = sauve_porte; } return 0;
        }
        /* La porte prend le NOM de la fonction demandée, le temps de
         * l'emprunt. « v1 fonction 2 » ne disait pas laquelle porter ;
         * « v1 fonction the destination » le dit. Un compteur qui ne nomme
         * pas son sujet oblige à retrouver à la main ce qu'il vient de
         * mesurer, et c'est justement ce qu'on voulait éviter. */
        const char *sauve_nom = v1_porte(nom);
        term_value(appel, buf, HC_VAL);
        g_v1_porte = sauve_nom;
        if (strcmp(buf, appel) != 0 && strcmp(buf, nom) != 0) {
            v3_note("fonction", nom);      /* term_value a fourni la réponse */
            *out = hct_val_texte(buf);
            ARENA_FREE;
            { g_v1_porte = sauve_porte; } return 1;
        }
        v1_note_muet(nom);       /* l'écho : inutile de redemander */
        if (v3_fonction_pile(nom, args, 0)) {
            *out = hct_val_texte(g_result);
            ARENA_FREE;
            { g_v1_porte = sauve_porte; } return 1;
        }
        ARENA_FREE;
        { g_v1_porte = sauve_porte; } return 0;
    }
    /* Fonctions du monde à un argument NUMÉRIQUE — param(n) et consorts.
     * On s'en tient au numérique : reconstruire un argument textuel serait
     * fragile dès qu'il contient un guillemet. Le reste passe par le
     * recours, qui dispose du texte source exact. */
    if (nargs == 1 && hct_est_nombre(args[0].txt) && !v1_est_muet(nom) &&
        dans_liste(nom, V3_V1_FONCTIONS_1)) {
        char appel[160];
        snprintf(appel, sizeof appel, "%s(%s)", nom, args[0].txt);
        buf[0] = '\0';
        /* Même raison qu'au-dessus : la porte nomme la fonction demandée,
         * pour que le relevé désigne ce qu'il y a à porter. */
        const char *sauve_n2 = v1_porte(nom);
        int fait_v1 = call_function(appel, buf, HC_VAL);
        g_v1_porte = sauve_n2;
        if (fait_v1) {
            v3_note("fonction", nom);      /* call_function a fourni la réponse */
            *out = hct_val_texte(buf);
            ARENA_FREE;
            { g_v1_porte = sauve_porte; } return 1;
        }
        v1_note_muet(nom);
    }
    if (v3_fonction_pile(nom, args, nargs)) {
        *out = hct_val_texte(g_result);
        ARENA_FREE;
        { g_v1_porte = sauve_porte; } return 1;
    }
    ARENA_FREE;
    g_v1_porte = sauve_porte;
    return 0;
}

/* --- ponts vers l'hôte ----------------------------------------------- */

/* L'hôte reçoit le nœud, hct_resout rend l'objet. Plus aucun texte
 * reconstitué sur ce chemin. */
/* Dernier nœud d'objet que v3_resout n'a PAS su résoudre.
 *
 * C'est le seul endroit où l'information existe. v3_recours reçoit le nœud
 * « of » qui surplombe la cible, mais pas le verdict de la résolution ; et la
 * refaire depuis le recours rappellerait l'évaluateur, donc le recours — la
 * première version bouclait et six harnais dépassaient leur délai.
 *
 * On note donc l'échec au passage. C'est un pointeur COMPARÉ, jamais
 * déréférencé : l'arbre lui survit le temps d'une évaluation, et de toute
 * façon seule l'identité nous intéresse. */
static const HctNoeud *g_v3_cible_manquee = NULL;

static void *v3_resout(void *d, const HctNoeud *ref, HctContexte *ctx)
{
    (void)d;

    /* « card window » n'est pas une carte, c'est la fenêtre de la pile — voir
     * v3_est_fenetre. On refuse ici plutôt que de laisser hct_resout évaluer
     * « window » comme un rang : cette évaluation DIFFUSE le mot comme un
     * message dans toute la hiérarchie avant d'échouer. Le nœud « of » qui
     * nous surplombe est ensuite servi par v3_fenetre_prop, dans v3_recours. */
    if (v3_est_fenetre(ref)) return NULL;

    Object *o = hct_resout(ctx, ref);
    if (o) return o;

    /* UNE VARIABLE QUI CONTIENT UN DESCRIPTEUR D'OBJET EN DÉSIGNE UN.
     *
     * C'est l'idiome de toute fonction utilisateur qui prend un objet :
     *
     *     function nomme o
     *       return the short name of o
     *     end nomme
     *     put nomme(me)
     *
     * hct_resout ne connaît que les nœuds OBJET. Devant un identificateur nu,
     * il rendait NULL, le recours reconstituait le texte « the short name of
     * o », l'ancien évaluateur cherchait un objet NOMMÉ « o », n'en trouvait
     * pas, et la règle « identificateur inconnu = son propre nom » rendait le
     * littéral « short name of o ». Sans erreur : le script avait l'air de
     * marcher. Mesuré, avec un descripteur parfaitement formé dans la
     * variable — « button "Bouton" » — le résultat était le même.
     *
     * On lit donc la variable et l'on confie SA VALEUR au résolveur de texte
     * du noyau, qui sait lire « card button id 2915 » aussi bien que « button
     * "Bouton" ». C'est ce que fait HyperTalk, et c'est ce qui rend
     * « the name of me » utilisable comme on l'écrit partout.
     *
     * Seulement si l'identificateur EST une variable posée. Un mot inconnu
     * garde le chemin d'avant : le prendre pour un nom d'objet ferait résoudre
     * « the foo of bar » sur un objet nommé « bar » que personne n'a désigné.
     *
     * Le garde de profondeur n'est pas décoratif : resolve peut évaluer un
     * sous-terme — « card id x » —, donc rappeler l'évaluateur, donc revenir
     * ici. Une variable qui se désigne elle-même boucherait sans lui. */
    if (ref && ref->genre == HCTN_IDENT) {
        static int profondeur = 0;
        if (profondeur < 4) {
            char nom[64];
            hct_texte(&ref->jeton, nom, sizeof nom);
            const char *v = var_get(nom);
            if (v && *v) {
                profondeur++;
                o = resolve(v);
                profondeur--;
                if (o) return o;
            }
        }
    }

    /* ON RETIENT AUSSI LES IDENTIFICATEURS MANQUÉS, PAS SEULEMENT LES NŒUDS
     * D'OBJET.
     *
     * Seul HCTN_OBJET était noté, si bien que « the short name of z », où z
     * contient du texte ordinaire, n'était retenu par personne : la garde
     * finale ne voyait rien à signaler et l'ancien moteur rendait la chaîne
     * « short name of z ». Quatre formes muettes, toutes mesurées :
     *
     *   the short name of z            (z = "inconnu")
     *   the short name of jamaisPosee  (jamais posée)
     *   the short name of o            (o = "o", se désigne elle-même)
     *   the short name of k            (k = descripteur d'un objet absent)
     *
     * Noter l'identificateur ne décide de rien à lui seul : v3_prop_sur_objet
     * n'en tire une erreur que si la PROPRIÉTÉ exige un objet. « the number of
     * chars of txt » passe donc exactement comme avant. */
    if (ref && (ref->genre == HCTN_OBJET || ref->genre == HCTN_IDENT))
        g_v3_cible_manquee = ref;
    return NULL;
}

/* Le contenu d'un objet résolu : le texte d'un champ, le nom des autres,
 * comme dans HyperCard. */
static int v3_lit_objet(void *d, void *objet, HctValeur *out)
{
    (void)d;
    Object *o = objet;
    if (!o) return 0;
    if (o->type == OBJ_FIELD) *out = hct_val_texte(hc_field_text(o));
    else                      *out = hct_val_texte(o->name ? o->name : "");
    return 1;
}

/* Écrire dans un objet résolu, sans repasser par le texte.
 *
 * Seuls les champs sont des conteneurs : un bouton rend 0, et l'exécuteur
 * refuse alors la ligne en disant pourquoi.
 *
 * On refait ici la concaténation de container_set plutôt que de l'appeler :
 * container_set prend une RÉFÉRENCE TEXTUELLE, et c'est justement le texte
 * qu'on veut éviter de reconstituer. Attention en revanche, les deux
 * conventions de `mode` sont INVERSES — 1 vaut « after » pour container_set et
 * « before » pour l'exécuteur ; on s'en tient à celle de l'exécuteur.
 *
 * hc_set_field_text s'occupe seul des plages de style : sans intervalle noté,
 * un remplacement complet les détruit, ce qui est la règle de HyperCard 2.4. */
static int v3_ecrit_objet(void *d, void *objet, const char *val, int mode)
{
    (void)d;
    Object *o = objet;
    if (!o || o->type != OBJ_FIELD) return 0;
    if (!val) val = "";

    if (mode == 0) {
        hc_set_field_text(o, val);
    } else {
        ARENA_MARK;
        char *fusion = arena_buf();
        const char *ancien = hc_field_text(o);
        if (mode == 1) snprintf(fusion, HC_VAL, "%s%s", val, ancien);  /* before */
        else           snprintf(fusion, HC_VAL, "%s%s", ancien, val);  /* after  */
        hc_set_field_text(o, fusion);
        ARENA_FREE;
    }
    notify_field(o);
    set_result("");
    return 1;
}

/* --- respiration : l'hôte reprend la main entre deux tours de boucle ---
 *
 * L'ancien exécuteur de lignes appelait host_idle() à chaque tour de repeat.
 * Sans l'équivalent ici, « repeat while the mouse is down » ne
 * rendait jamais la main : la file d'événements n'était pas vidée, l'état de
 * la souris ne changeait plus, et rien ne se redessinait — le script ne
 * suivait pas la souris.
 *
 * Rend 0 pour interrompre la boucle. On garde le même plafond que l'ancien,
 * et le même message : une boucle emballée doit se voir. */
static int v3_respire(void *d)
{
    (void)d;

    /* Quelque chose de visible a changé : on rend la main TOUT DE SUITE.
     *
     * Une animation — le dé qui rebondit, un bouton qu'on traîne — dessine une
     * image par tour de boucle. L'étrangler à soixante hertz sautait les
     * images intermédiaires : le dé traversait l'écran d'un trait au lieu de
     * tomber. La cadence de l'animation, c'est celle du redessin, et c'est
     * l'hôte qui la donne en repeignant.
     *
     * hc_take_visual_dirty() remet le drapeau à zéro chez l'hôte : le tour
     * suivant, s'il n'a rien changé de visible, retombe sur l'étranglement.
     *
     * SAUF écran verrouillé. Il n'y a alors rien à montrer, donc rien qui
     * presse — et surtout, un hôte qui ne repeint pas pendant le verrou
     * n'appelle pas hc_take_visual_dirty() et ne remet donc JAMAIS le drapeau
     * à zéro. On restait bloqué sur cette branche à chaque tour, et
     * « lock screen » rendait la boucle plus LENTE qu'sans, exactement le
     * contraire de ce qu'il promet. */
    if (g_visual_dirty && !g_ecran_verrouille) { host_idle(); return 1; }

    /* Rien de visible : pas à CHAQUE tour, environ soixante fois par seconde.
     *
     * host_idle() fait redessiner l'hôte et vider sa file d'événements ; c'est
     * ce qui coûte, pas l'interprétation. Une boucle serrée qui l'appelle dix
     * mille fois paie dix mille rafraîchissements pour un seul écran visible.
     *
     * Soixante hertz suffisent : c'est la cadence de l'écran, et c'est
     * largement assez pour voir la souris se relever. HyperCard ne faisait pas
     * autrement — un « repeat » ne redessinait pas à chaque passage.
     *
     * Premier tour excepté : on souffle tout de suite, pour que l'hôte prenne
     * la main même sur une boucle qui ne fera qu'un ou deux tours. */
    static struct timespec dernier;
    static int amorce = 0;

    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) { host_idle(); return 1; }

    if (!amorce) { amorce = 1; dernier = t; host_idle(); return 1; }

    double ecoule = (double)(t.tv_sec - dernier.tv_sec)
                  + (double)(t.tv_nsec - dernier.tv_nsec) / 1e9;
    if (ecoule < 0 || ecoule >= 1.0 / 60.0) {
        dernier = t;
        host_idle();
    }
    return 1;      /* le plafond de tours est tenu par l'exécuteur */
}

/* --- l'hôte assemblé ------------------------------------------------- */

/* Une ligne isolée passée à la v3 — voir sa définition, tout en bas. Déclarée
 * ici parce que hc_do_menu s'en sert bien avant. */
static int v3_do_ligne(const char *line);

/* --- recours pour les COMMANDES ---
 *
 * hct_exec exécute lui-même ce qui ne touche pas au monde : put, get, global,
 * l'arithmétique, les structures de contrôle, les sorties. Tout le reste —
 * go, set, show, answer, visual, les soixante autres — lui revient ici.
 *
 * On ne reçoit pas des opérandes évalués mais le NŒUD, ce qui permettait de
 * retrouver la ligne d'origine et de la confier à l'ancien exécuteur — le
 * temps que la frontière se déplace, commande par commande, sans que rien ne
 * s'arrête. Elle a fini de se déplacer : ce qui arrive ici et que la v3 ne
 * sait pas faire est désormais REFUSÉ, avec un message.
 *
 * Évaluer les opérandes d'abord ne marcherait pas : « go to card 3 » n'a de
 * sens que si la référence d'objet parvient intacte. */
/* ---- relevé des retours vers l'ancien interpréteur -------------------
 *
 * Tant que la v3 ne fait pas tout, hc_core.c ne peut pas être élagué au
 * jugé : couper une fonction qu'un seul script d'une seule pile appelle une
 * fois par an, c'est casser cette pile-là sans le savoir.
 *
 * On compte donc, par nom, chaque passage de la v3 vers l'ancien code. Ce qui
 * n'apparaît jamais après avoir promené toutes les piles est un candidat à la
 * coupe ; le reste porte encore.
 *
 * hc_v3_bilan() vide le relevé sur la console. Coût : une comparaison de
 * chaînes par retour, négligeable.
 *
 * Ce relevé a fait son office pour les LIGNES : zéro passage sur 172 harnais,
 * et les 2 327 lignes de l'ancien exécuteur ont pu partir. Il reste braqué
 * sur les deux morceaux qui servent encore, les fonctions et les termes. */
#define V3_RELEVE_MAX 256
/* 64 et non 48 : un intitulé accentué suivi d'un nom de porte débordait, et
 * snprintf coupait au milieu d'un caractère UTF-8 — la ligne de bilan sortait
 * alors en octets invalides. Les intitulés ont maigri aussi, ci-dessous. */
struct V3Releve { char nom[64]; long n; };
static struct V3Releve g_releve[V3_RELEVE_MAX];
static int g_nreleve = 0;

/* ═══ Ce que l'ANCIEN interprète exécute encore, et par quelle porte ═════
 *
 * Le relevé ci-dessus compte les RECOURS : les fois où la v3 renonce et rend
 * la main. « (aucun) » veut donc dire « la v3 n'a jamais abandonné » — et
 * PAS « l'ancien interprète ne tourne plus ». Ce sont deux choses
 * différentes, et les confondre m'a fait croire le chantier plus avancé
 * qu'il ne l'est : cinq commandes v3 appellent eval_checked de l'intérieur,
 * sans que rien ne le signale, et la boîte de message passait entièrement par
 * l'ancien exécuteur de lignes.
 *
 * Ces deux compteurs-ci mesurent donc l'EXÉCUTION RÉELLE, quelle qu'en soit
 * l'origine. Ils répondent à la seule question qui décide de l'élagage :
 * qu'est-ce qui appellerait encore le code qu'on veut supprimer ?
 *
 * La PORTE dit d'où l'on vient. Elle est posée par les points d'entrée —
 * la boîte de message, les menus du noyau, le recours, et chacune des
 * commandes v3 qui emprunte l'évaluateur v1 — et rendue à sa valeur
 * précédente en sortant, parce que ces chemins s'imbriquent : un « do »
 * dans un gestionnaire appelé depuis un article de menu en traverse trois. */
static struct V3Releve g_v1[V3_RELEVE_MAX];
static int  g_nv1 = 0;
static void v1_compte(const char *quoi, const char *porte)
{
    /* ON ARME ICI, ET PAS À L'INSTALLATION DE L'HÔTE.
     *
     * La première version armait depuis hc_set_host, que je croyais être le
     * point de passage obligé de tout programme. Mesuré après coup : 56 des
     * 192 harnais ne l'appellent jamais — ils se contentent de l'hôte console
     * par défaut. Le relevé de la phase 0 ne couvrait donc que 136
     * programmes, pas 192, et je l'avais annoncé comme complet.
     *
     * Armer au premier comptage ne peut pas se manquer : s'il y a quelque
     * chose à relever, on est passé par ici. Et s'il n'y a rien, il n'y a rien
     * à armer. */
    hc_v3_releve_arme();

    char cle[64];
    snprintf(cle, sizeof cle, "%s %s", quoi, porte && *porte ? porte : "?");
    for (int i = 0; i < g_nv1; i++)
        if (!strcmp(g_v1[i].nom, cle)) { g_v1[i].n++; return; }
    if (g_nv1 >= V3_RELEVE_MAX) return;
    snprintf(g_v1[g_nv1].nom, sizeof g_v1[0].nom, "%s", cle);
    g_v1[g_nv1].n = 1;
    g_nv1++;
}


/* Poser la porte et la rendre. À employer par paires, dans la même fonction :
 *     const char *sauve = v1_porte("msg");
 *     ...
 *     g_v1_porte = sauve; */
static const char *v1_porte(const char *nom)
{
    const char *avant = g_v1_porte;
    g_v1_porte = nom;
    return avant;
}

static void v3_note(const char *quoi, const char *nom)
{
    hc_v3_releve_arme();          /* voir v1_compte : on arme au comptage */
    char cle[64];
    snprintf(cle, sizeof cle, "%s %s", quoi, nom && *nom ? nom : "?");
    for (int i = 0; i < g_nreleve; i++)
        if (!strcmp(g_releve[i].nom, cle)) { g_releve[i].n++; return; }
    if (g_nreleve >= V3_RELEVE_MAX) return;
    snprintf(g_releve[g_nreleve].nom, sizeof g_releve[0].nom, "%s", cle);
    g_releve[g_nreleve].n = 1;
    g_nreleve++;
}

/* Le relevé part sur la SORTIE D'ERREUR autant que par emit().
 *
 * emit(HC_INFO) traverse le rappel `line` de l'hôte, et l'interface est libre
 * de ne pas afficher cette famille — c'est celle des « → x ← … », qu'on ne
 * montre qu'en mode trace. Le bilan disparaissait donc en silence : la
 * commande s'exécutait, et rien n'apparaissait.
 *
 * Même raison que pour les lignes « [v3] » du branchement : le temps de la
 * mise au point, on veut ces lignes quoi qu'il arrive, sans dépendre de ce
 * que l'interface veut bien montrer. */
static void bilan_ligne(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    emit(HC_INFO, "%s", buf);
    fprintf(stderr, "[v3] %s\n", buf);
}

static void bilan_v1(void);

void hc_v3_bilan(void)
{
    bilan_ligne("— retours de la v3 vers l'ancien interpréteur —");
    if (!g_nreleve) { bilan_ligne("   (aucun)"); bilan_v1(); return; }
    /* tri décroissant, en place : la liste est courte */
    for (int i = 0; i < g_nreleve; i++)
        for (int j = i + 1; j < g_nreleve; j++)
            if (g_releve[j].n > g_releve[i].n) {
                struct V3Releve t = g_releve[i];
                g_releve[i] = g_releve[j]; g_releve[j] = t;
            }
    for (int i = 0; i < g_nreleve; i++)
        bilan_ligne("   %-40s %ld", g_releve[i].nom, g_releve[i].n);
    bilan_v1();
}

/* Le second relevé : ce que l'ancien interprète a réellement exécuté.
 *
 * Séparé du premier, et pas fondu dedans : « recours » et « exécution » ne
 * disent pas la même chose, et un tableau qui mélangerait les deux ferait
 * lire « aucun retour » là où l'ancien code tourne à plein. */
static void bilan_v1(void)
{
    bilan_ligne("— ce que l'ancien interprète exécute encore —");
    if (!g_nv1) { bilan_ligne("   (rien)"); return; }
    for (int i = 0; i < g_nv1; i++)
        for (int j = i + 1; j < g_nv1; j++)
            if (g_v1[j].n > g_v1[i].n) {
                struct V3Releve t = g_v1[i];
                g_v1[i] = g_v1[j]; g_v1[j] = t;
            }
    for (int i = 0; i < g_nv1; i++)
        bilan_ligne("   %-40s %ld", g_v1[i].nom, g_v1[i].n);
}

static void releve_fichier(void);

/* REMETTRE À ZÉRO NE DOIT PAS EFFACER CE QUE LE RELEVÉ DU CORPUS N'A PAS
 * ENCORE VU.
 *
 * Le relevé écrit dans son fichier à la SORTIE du processus. Un harnais qui
 * fait « debug bilan raz » en cours de route vidait donc les compteurs avant
 * que le fichier ne les voie, et tout ce qui précédait la remise à zéro
 * disparaissait de l'agrégat.
 *
 * Mesuré : le relevé annonçait 8 sondes de nom sur tout le corpus. Les
 * références des harnais en montraient d'autres, « aplat » et « carre », que
 * l'agrégat ne comptait pas — parce que test_exercice remet ses compteurs à
 * zéro entre deux bilans. Un instrument qui sous-compte fait croire le
 * chantier plus avancé qu'il n'est : c'est la deuxième fois que celui-ci s'y
 * prend, après l'armement depuis hc_set_host qui ne couvrait que 136 des 192
 * programmes.
 *
 * On vide donc dans le fichier avant de vider les compteurs. releve_fichier
 * ne fait rien si HC_V3_RELEVE n'est pas posé, et l'agrégat somme déjà les
 * lignes de même clé — deux enregistrements pour un processus s'additionnent
 * sans rien changer au script. */
void hc_v3_bilan_remise_a_zero(void)
{
    releve_fichier();
    g_nreleve = 0;
    g_nv1 = 0;
}

/* ═══ LE RELEVÉ DE TOUT LE CORPUS, ET POURQUOI IL NE PASSE PAS PAR LA SORTIE
 *
 * `debug bilan` écrit sur la sortie du harnais, donc dans sa référence. Pour
 * mesurer LE CORPUS il faudrait l'ajouter aux 189 harnais, c'est-à-dire
 * réenregistrer 189 références — et une référence qu'on réenregistre en masse
 * ne prouve plus rien. Mesurés : 71 harnais sur 189 impriment un bilan, et
 * chacun remet ses compteurs à zéro. Les 118 autres sont un angle mort, et
 * c'est justement là que les surprises se logent : il n'y avait AUCUN harnais
 * pour les icônes quand leur enregistrement s'est cassé.
 *
 * On écrit donc dans un FICHIER, en ajout, à la fin du processus, et
 * uniquement si HC_V3_RELEVE le nomme. Rien sur la sortie, donc aucune
 * référence touchée ; un enregistrement par processus, donc l'agrégat couvre
 * la suite entière. Une ligne par compteur, préfixée du nom du programme pour
 * qu'on puisse demander « qui » autant que « combien ».
 *
 * Éteint, cela coûte un getenv à la sortie du processus. */
static void releve_fichier(void)
{
    const char *chemin = getenv("HC_V3_RELEVE");
    if (!chemin || !*chemin) return;
    FILE *f = fopen(chemin, "a");
    if (!f) return;
    const char *qui = getenv("HC_V3_RELEVE_QUI");
    if (!qui) qui = "?";
    for (int i = 0; i < g_nreleve; i++)
        fprintf(f, "%s\trecours\t%s\t%ld\n", qui, g_releve[i].nom, g_releve[i].n);
    for (int i = 0; i < g_nv1; i++)
        fprintf(f, "%s\texec\t%s\t%ld\n", qui, g_v1[i].nom, g_v1[i].n);
    fclose(f);
}

void hc_v3_releve_arme(void)
{
    static int arme = 0;
    if (arme) return;
    arme = 1;
    if (getenv("HC_V3_RELEVE")) atexit(releve_fichier);
}

/* ==================== répartiteur des commandes ==================== *
 *
 * Chaque verbe porté est une fonction qui reçoit le NŒUD. Ses arguments sont
 * des sous-arbres qu'on évalue avec hct_evalue, ses références d'objets des
 * nœuds que hct_resout sait résoudre. Plus de texte reconstitué, plus de
 * réanalyse par l'ancien interpréteur, et surtout plus de double évaluation :
 * dans une boucle de dessin, les coordonnées d'un « drag » étaient jusqu'ici
 * calculées par la v3, jetées, puis recalculées par l'ancien exécuteur à
 * chaque tour.
 *
 * Une fonction rend 1 si elle a traité la commande, 0 sinon. Ce zéro servait à
 * laisser l'ancien chemin s'en charger : porter la forme courante d'un verbe
 * en laissant ses formes rares derrière, plutôt que de tout porter d'un coup
 * ou rien. Il n'y a plus d'ancien chemin — un zéro fait maintenant REFUSER la
 * ligne, avec un message — et ce qui sort par là reste compté par v3_note,
 * donc visible au bilan.
 *
 * La TABLE était le tableau d'avancement de la migration : ce qui n'y figure
 * pas est ce qui n'a jamais été porté, et qui est désormais refusé plutôt que
 * rendu à l'ancien exécuteur.
 */

/* Le fils `i` est-il le mot-clé `mot` ? Les motifs de hct_cmd.c gardent dans
 * l'arbre les mots qu'ils consomment, et c'est par eux qu'on distingue les
 * formes d'un même verbe — « drag from … to … » de « … with … ». */
static int v3_est_motcle(const HctNoeud *n, int i, const char *mot)
{
    if (!n || i < 0 || i >= n->nfils) return 0;
    const HctNoeud *f = n->fils[i];
    return f && f->genre == HCTN_MOTCLE && f->op && ci_equal(f->op, mot);
}

/* Rang du mot-clé `mot` à partir de `depuis`, ou -1. */
static int v3_indice_motcle(const HctNoeud *n, const char *mot, int depuis)
{
    for (int i = depuis; i < n->nfils; i++)
        if (v3_est_motcle(n, i, mot)) return i;
    return -1;
}

/* Le nœud qui SUIT un mot-clé de premier niveau, ou NULL s'il n'y est pas.
 * Les motifs de hct_cmd.c gardent les mots-clés dans l'arbre précisément
 * pour qu'on puisse s'y repérer sans redécouper du texte. */
static const HctNoeud *v3_apres_motcle(const HctNoeud *n, const char *mot)
{
    int i = v3_indice_motcle(n, mot, 0);
    return (i >= 0 && i + 1 < n->nfils) ? n->fils[i + 1] : NULL;
}

/* Le texte d'un fils TEL QU'ÉCRIT, sans l'évaluer.
 *
 * Certains arguments ont la forme d'une expression sans en être une :
 * « with shiftKey » nomme une touche, et l'évaluer appellerait la fonction du
 * même nom, qui rendrait « true » ou « false ». De même « choose line tool »,
 * où « line » est un nom d'outil et non une variable.
 *
 * On lit le jeton du nœud et non hct_noeud_etendue, qui étend délibérément
 * son résultat jusqu'à la fin de la LIGNE — ce qu'il faut pour rendre une
 * instruction entière à l'ancien interpréteur, jamais pour isoler un mot. */
static void v3_brut(const HctNoeud *f, char *out, int outlen)
{
    out[0] = '\0';
    if (!f || outlen < 1) return;
    int len = f->jeton.len;
    if (len < 0) len = 0;
    if (len > outlen - 1) len = outlen - 1;
    if (len) memcpy(out, f->jeton.deb, (size_t)len);
    out[len] = '\0';
}

/* Comme v3_brut, mais une variable LIÉE l'emporte sur le mot littéral.
 *
 * « choose tl tool », où tl est un paramètre qui vaut « browse », doit lire
 * tl ; « choose line tool », où rien ne lie « line », doit rendre « line »
 * telle quelle. C'est l'idiome de sauvegarde/restauration d'outil des
 * scripts HyperCard classiques : « on clearScreen tl … if tl is not empty
 * then choose tl tool ».
 *
 * On s'arrête à lit_var, sans passer par l'évaluateur complet : celui-ci
 * essaierait ensuite hote.fonction, qui — pour un mot que rien ne connaît —
 * DIFFUSE un message de ce nom via hc_call_user_function plutôt que de
 * rendre le mot littéral. « choose select tool » enverrait alors un message
 * « select » à travers toute la hiérarchie avant de choisir l'outil, un
 * effet de bord que ce mot brut n'attend pas. */
static void v3_mot_ou_var(HctContexte *ctx, const HctNoeud *f, char *out, int outlen)
{
    out[0] = '\0';
    if (!f || outlen < 1) return;
    char nom[128];
    v3_brut(f, nom, sizeof nom);
    if (!*nom) return;
    HctValeur v;
    if (ctx->hote.lit_var && ctx->hote.lit_var(ctx->hote.donnees, nom, &v)) {
        snprintf(out, (size_t)outlen, "%s", v.txt ? v.txt : "");
        hct_val_libere(&v);
        return;
    }
    snprintf(out, (size_t)outlen, "%s", nom);
}

/* Les touches d'un « with … », telles qu'écrites : l'hôte attend la même
 * chaîne que lui donnait l'ancien exécuteur. */
static void v3_touches(const HctNoeud *n, int deb, char *out, int outlen)
{
    int pos = 0;
    out[0] = '\0';
    for (int i = deb; i >= 0 && i < n->nfils; i++) {
        char m[64];
        v3_brut(n->fils[i], m, sizeof m);
        if (!*m) continue;
        pos += snprintf(out + pos, (size_t)(outlen - pos), "%s%s",
                        pos ? ", " : "", m);
        if (pos >= outlen) { pos = outlen - 1; break; }
    }
}

/* Un point : les fils [deb, fin[ évalués et joints par une virgule.
 *
 * Le découpage est déjà fait par l'analyseur — le motif « * » sépare les
 * expressions aux virgules, si bien que « drag from 10,20 to … » donne deux
 * fils. eval_point, qui redécoupait le texte en comptant les parenthèses et
 * les guillemets, n'a plus lieu d'être. */
static void v3_point(HctContexte *ctx, const HctNoeud *n, int deb, int fin,
                     char *out, int outlen)
{
    int pos = 0;
    out[0] = '\0';
    if (fin > n->nfils) fin = n->nfils;
    for (int i = deb; i < fin; i++) {
        char v[128];
        v3_val_texte(ctx, n->fils[i], v, sizeof v);
        if (ctx->erreur) return;
        pos += snprintf(out + pos, (size_t)(outlen - pos), "%s%s",
                        pos ? "," : "", v);
        if (pos >= outlen) { pos = outlen - 1; break; }
    }
}

/* Le texte de toute une commande, depuis le nœud, moins son premier mot (le
 * verbe) : ce que valait `rest` dans l'ancien exécuteur, pour les commandes
 * portées qui refont son analyse mot à mot (visual, sort, find, set,
 * convert, print, read, write).
 *
 * hct_noeud_etendue, jamais v3_source, pour cet usage précis. v3_source
 * s'arrête au dernier JETON retenu dans le sous-arbre — et les parenthèses
 * ne sont le jeton d'AUCUN nœud, qu'elles ferment un appel ou groupent une
 * expression : l'analyseur les consomme et les oublie. « set icon of me to
 * (2100 + random(6)) » se reconstituait donc « (2100 + random(6 », les DEUX
 * parenthèses fermantes perdues, et l'évaluation de la valeur échouait sur
 * « parenthèse fermante attendue ». Msg d'erreur trouvé par test réel dans
 * Xcode. hct_noeud_etendue étend jusqu'au saut de ligne (ou au point-
 * virgule, ou à « else », ou à un commentaire) plutôt qu'au dernier jeton :
 * elle rend donc tout ce que l'auteur a écrit, ponctuation comprise. C'est
 * elle que v3_commande utilise déjà pour son propre repli vers l'ancien
 * chemin — même remède, ici pour ne PAS avoir à y retomber. */
static void v3_reste(const HctNoeud *n, char *out, int outlen)
{
    out[0] = '\0';
    const char *deb; int len;
    if (!hct_noeud_etendue(n, &deb, &len)) return;
    if (len > outlen - 1) len = outlen - 1;
    memcpy(out, deb, (size_t)len);
    out[len] = '\0';

    /* Sauter le premier mot (le verbe) et les blancs qui le suivent. */
    char *p = out;
    while (*p && !isspace((unsigned char)*p)) p++;
    while (*p == ' ' || *p == '\t') p++;
    memmove(out, p, strlen(p) + 1);
}

/* ---- tri : mécanique commune aux deux exécuteurs -------------------------
 * Déplacé ici (l'original vivait juste avant l'ancien gestionnaire de
 * « sort », documenté plus bas) parce que v3_cmd_sort en a besoin et que
 * rien, avant ce point du fichier, n'en dépendait déjà. Le tri est STABLE :
 * deux éléments de clé égale gardent leur ordre d'origine — HyperCard le
 * garantissait, et des piles s'en servent pour trier sur deux critères en
 * triant deux fois, du moins important au plus important. */
static void eval_checked(const char *s, char *out, int outlen);   /* défini plus bas */

typedef enum { SORT_TEXT, SORT_NUM, SORT_DATE } SortStyle;
typedef struct { char *cle; int rang; Object *card; } SortItem;

static int       g_sort_desc  = 0;
static SortStyle g_sort_style = SORT_TEXT;

static int sort_cmp(const void *pa, const void *pb)
{
    const SortItem *a = pa, *b = pb;
    int r = 0;

    if (g_sort_style == SORT_NUM || g_sort_style == SORT_DATE) {
        double x = 0, y = 0;
        int nx = as_num(a->cle, &x), ny = as_num(b->cle, &y);
        /* Ce qui n'est pas un nombre passe après, plutôt que de valoir zéro et
         * de venir se mêler aux valeurs légitimes. */
        if (nx && ny) r = (x < y) ? -1 : (x > y) ? 1 : 0;
        else if (nx)  r = -1;
        else if (ny)  r =  1;
    } else {
        const char *x = a->cle, *y = b->cle;      /* insensible à la casse */
        while (*x && *y) {
            int cx = tolower((unsigned char)*x), cy = tolower((unsigned char)*y);
            if (cx != cy) { r = cx < cy ? -1 : 1; break; }
            x++; y++;
        }
        if (!r) r = (*x ? 1 : 0) - (*y ? 1 : 0);
    }

    if (g_sort_desc) r = -r;
    if (r == 0) r = a->rang - b->rang;            /* stabilité */
    return r;
}

/* Lit sens et style, et rend ce qui reste de la ligne. */
static const char *sort_options(const char *s, int *desc, SortStyle *style)
{
    for (;;) {
        s = skip_spaces(s);
        if      (ci_word(s, "ascending"))     { *desc = 0; s += 9;  }
        else if (ci_word(s, "descending"))    { *desc = 1; s += 10; }
        else if (ci_word(s, "text"))          { *style = SORT_TEXT; s += 4; }
        else if (ci_word(s, "numeric"))       { *style = SORT_NUM;  s += 7; }
        else if (ci_word(s, "datetime"))      { *style = SORT_DATE; s += 8; }
        else if (ci_word(s, "international")) { *style = SORT_TEXT; s += 13; }
        else return s;
    }
}

/* sort [this] stack [asc|desc] [style] by <clé>
 * sort [lines|items of] <conteneur> [asc|desc] [style] [by <clé avec each>]
 *
 * Motif hct_cmd.c : « * [ascending|descending] [text|numeric|international|
 * datetime] [by e] ». Le « * » découpe la cible mot à mot — « cards », « of »,
 * « this », « stack » deviennent chacun leur propre fils, une référence
 * d'objet comme « field 1 » en fait parfois un seul — et rien ici n'a besoin
 * de savoir lequel : v3_reste (voir sa définition) rend le texte EXACT que
 * lisait l'ancien exécuteur, ponctuation et « the » compris. On lui reprend
 * alors son analyse mot à mot telle quelle, sans y toucher — seule la
 * source du texte a changé.
 *
 * Pas d'évaluation à ce stade : « cards », « stack », « lines » restent des
 * MOTS, pas des expressions. Une pile qui aurait une variable nommée
 * « cards » ne doit pas voir sa valeur s'y substituer — exactement le
 * comportement de l'ancien chemin, qui travaillait déjà sur du texte brut. */
/* select — la sélection de texte dans un champ.
 *
 *   select empty                     rien de sélectionné
 *   select text of <champ>           tout le contenu
 *   select <champ>                   idem, forme courte
 *   select char 3 to 5 of <champ>    un morceau
 *   select before|after <ce qui précède>   point d'insertion à l'une des bornes
 *
 * Tout se fait depuis l'ARBRE : la cible est résolue par hct_resout et les
 * bornes du morceau par hct_chunk_bornes, sans reconstituer de texte.
 *
 * Rend 0 sur les formes non couvertes — un ordinal (« select last line
 * of… »), une cible qui n'est pas un champ —, et l'ancien exécuteur les
 * reprend intactes. */
static int v3_cmd_select(HctContexte *ctx, const HctNoeud *n)
{
    int i = 0, avant = 0, apres = 0;

    if (i < n->nfils && n->fils[i] && n->fils[i]->genre == HCTN_IDENT) {
        char m[16];
        v3_brut(n->fils[i], m, sizeof m);
        if      (ci_equal(m, "before")) { avant = 1; i++; }
        else if (ci_equal(m, "after"))  { apres = 1; i++; }
    }

    /* « select » nu : plus rien de sélectionné. */
    if (i >= n->nfils || !n->fils[i]) { hc_set_selection(NULL, 0, 0); return 1; }

    const HctNoeud *c = n->fils[i];

    if (c->genre == HCTN_IDENT) {
        char m[16];
        v3_brut(c, m, sizeof m);
        if (ci_equal(m, "empty")) { hc_set_selection(NULL, 0, 0); return 1; }
        return 0;                       /* une variable : ancien chemin */
    }

    Object *f = NULL;
    int st = 0, en = 0;

    if (c->genre == HCTN_OBJET) {
        f = hct_resout(ctx, c);
        if (!f || f->type != OBJ_FIELD) return 0;
        st = 0; en = (int)strlen(hc_field_text(f));
    } else if (c->genre == HCTN_OF && c->nfils >= 2 &&
               c->fils[0] && c->fils[0]->genre == HCTN_IDENT &&
               c->fils[1] && c->fils[1]->genre == HCTN_OBJET) {
        char m[16];
        v3_brut(c->fils[0], m, sizeof m);
        if (!ci_equal(m, "text")) return 0;
        f = hct_resout(ctx, c->fils[1]);
        if (!f || f->type != OBJ_FIELD) return 0;
        st = 0; en = (int)strlen(hc_field_text(f));
    } else if (c->genre == HCTN_CHUNK && c->nfils >= 1) {
        const HctNoeud *cible = c->fils[c->nfils - 1];
        if (!cible || cible->genre != HCTN_OBJET) return 0;
        f = hct_resout(ctx, cible);
        if (!f || f->type != OBJ_FIELD) return 0;

        int n1 = 0, n2 = 0;
        if (c->ordinal) {
            /* « select last word of … », « select middle line of … ».
             *
             * L'ordinal REMPLACE les bornes : le nœud n'a plus qu'un enfant,
             * sa cible, et le rang se calcule sur le nombre d'éléments. C'est
             * pourquoi cette branche exigeait « !c->ordinal » et rendait la
             * main — toutes ces formes repartaient à l'ancien interprète,
             * alors que « select char 2 of … » passait.
             *
             * hct_rang_ordinal vient de hct_eval.c, où l'évaluateur s'en sert
             * déjà : en écrire une seconde copie ici aurait donné deux tables
             * d'ordinaux à tenir d'accord, et « middle » a déjà été faux une
             * fois — total/2+1 et non (total+1)/2. */
            int total = hct_chunk_compte(hc_field_text(f), c->sorte,
                                         item_delim());
            n1 = hct_rang_ordinal(c->ordinal, total);
            if (n1 <= 0) return 0;
        } else if (c->nfils >= 2) {
            char b1[64], b2[64];
            v3_val_texte(ctx, c->fils[0], b1, sizeof b1);
            if (ctx->erreur) return 1;
            int hors;
            /* Même exigence que partout ailleurs : un rang doit être un
             * nombre, pas seulement quelque chose que strtod avale. */
            if (!hct_est_nombre(b1)) {
                hct_ctx_faute(ctx, c->fils[0],
                              "un rang numérique est attendu ici");
                return 1;
            }
            n1 = hct_vers_rang(b1, &hors);
            if (hors) { hct_ctx_faute(ctx, c->fils[0],
                                      "rang de morceau hors limites");
                        return 1; }
            if (c->nfils >= 3) {
                v3_val_texte(ctx, c->fils[1], b2, sizeof b2);
                if (ctx->erreur) return 1;
                if (!hct_est_nombre(b2)) {
                    hct_ctx_faute(ctx, c->fils[1],
                                  "un rang numérique est attendu ici");
                    return 1;
                }
                n2 = hct_vers_rang(b2, &hors);
                if (hors) { hct_ctx_faute(ctx, c->fils[1],
                                          "rang de morceau hors limites");
                            return 1; }
            }
        } else {
            return 0;               /* ni ordinal ni borne : rien à viser */
        }

        HctBornes bo = hct_chunk_bornes(hc_field_text(f), c->sorte,
                                        n1, n2, item_delim());
        if (!bo.trouve) return 0;
        st = bo.deb; en = bo.fin;
    } else {
        return 0;
    }

    if      (avant) hc_set_selection(f, st, 0);
    else if (apres) hc_set_selection(f, en, 0);
    else            hc_set_selection(f, st, en - st);
    set_result("");
    return 1;
}

static int v3_cmd_sort(HctContexte *ctx, const HctNoeud *n)
{
    /* LA CLÉ DE TRI EST UN SOUS-ARBRE, et on l'évalue une fois par élément.
     * La relire à chaque tour — c'est ce que faisait eval_checked — refait
     * l'analyse autant de fois qu'il y a de cartes ou de lignes : « v3 relit
     * sort 4 » au relevé pour quatre éléments, et quatre cents pour quatre
     * cents. Le reste de la commande (quel conteneur, quelles options) se lit
     * toujours dans le texte : il n'est analysé qu'une fois, il ne coûte
     * rien.
     *
     * Le nœud se prend ICI, avant les boucles, et sert dans les deux — le tri
     * de cartes et celui d'un conteneur. Quand la forme de l'arbre surprend,
     * `cle` reste le repli par le texte. */
    const HctNoeud *ncle = v3_apres_motcle(n, "by");
    size_t sauve = g_atop;             /* nommée à part : pas de collision
                                         * avec les ARENA_MARK imbriqués ci-
                                         * dessous, qui doivent pouvoir
                                         * rembobiner À LEUR PROPRE MARQUE
                                         * sans toucher à `mots`. */
    char *mots = arena_buf();
    v3_reste(n, mots, HC_VAL);

    const char *a = skip_spaces(mots);
    int cartes = 0;                 /* trie-t-on des cartes ? */
    ChunkType morceau = CH_LINE;    /* pour un conteneur */

    if (ci_word(a, "this")) a = skip_spaces(a + 4);
    /* « MARKED » EST LU, ET IL COMPTE DÉSORMAIS.
     *
     * Il était « accepté, ignoré » — le mot passait, le tri portait sur
     * TOUTES les cartes. Mesuré, avec quatre cartes D B C A dont deux
     * marquées : « sort marked cards by the short name of this card » rendait
     * A B C D, c'est-à-dire tout trié. Un script qui marque un sous-ensemble
     * pour le ranger réordonnait la pile entière.
     *
     * La règle d'HyperCard est un tri EN PLACE : les cartes marquées se
     * redistribuent entre les seules positions qu'elles occupaient déjà, et
     * les autres ne bougent pas d'un cran. */
    int marquees = 0;
    if (ci_word(a, "marked")) { marquees = 1; a = skip_spaces(a + 6); }

    if (ci_word(a, "stack")) { cartes = 1; a = skip_spaces(a + 5); }
    else if (ci_word(a, "cards")) {
        cartes = 1; a = skip_spaces(a + 5);
        if (ci_word(a, "of")) {
            a = skip_spaces(a + 2);
            if (ci_word(a, "this")) a = skip_spaces(a + 4);
            if (ci_word(a, "stack")) a = skip_spaces(a + 5);
        }
    }
    else if (ci_word(a, "lines")) { morceau = CH_LINE; a = skip_spaces(a + 5);
                                    if (ci_word(a, "of")) a = skip_spaces(a + 2); }
    else if (ci_word(a, "items")) { morceau = CH_ITEM; a = skip_spaces(a + 5);
                                    if (ci_word(a, "of")) a = skip_spaces(a + 2); }

    int desc = 0; SortStyle style = SORT_TEXT;

    if (cartes) {
        a = sort_options(a, &desc, &style);
        const char *cle = NULL;
        if (ci_word(a, "by")) cle = skip_spaces(a + 2);

        Object *stack = g_current_card ? g_current_card->owner : NULL;
        if (!stack) { emit(HC_ERR, "   !! sort : pas de pile");
                      g_atop = sauve; return 1; }

        int n2 = 0;
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i]->type == OBJ_CARD &&
                (!marquees || stack->parts[i]->marked)) n2++;
        if (n2 < 2) { g_atop = sauve; return 1; }

        SortItem *tab = calloc((size_t)n2, sizeof *tab);
        char **cles = calloc((size_t)n2, sizeof *cles);
        if (!tab || !cles) { free(tab); free(cles); g_atop = sauve; return 1; }

        Object *avant = g_current_card;
        int k = 0;
        for (int i = 0; i < stack->nparts; i++) {
            Object *c = stack->parts[i];
            if (c->type != OBJ_CARD) continue;
            if (marquees && !c->marked) continue;
            /* Se placer SUR la carte pour évaluer sa clé : « field "nom" »
             * doit désigner le champ de celle-ci, pas de la carte de
             * départ. C'est tout le sens du tri par contenu. */
            g_current_card = c;
            ARENA_MARK;
            char *tmp = arena_buf();
            tmp[0] = '\0';
            if (ncle)     v3_val_texte(ctx, ncle, tmp, HC_VAL);
            else if (cle) eval_checked(cle, tmp, HC_VAL);
            cles[k] = dupstr(tmp);
            ARENA_FREE;
            tab[k].cle = cles[k]; tab[k].rang = k; tab[k].card = c;
            k++;
        }
        g_current_card = avant;

        g_sort_desc = desc; g_sort_style = style;
        qsort(tab, (size_t)n2, sizeof *tab, sort_cmp);

        /* Réécrire les cartes dans leur nouvel ordre, en laissant les
         * fonds à leur place : ils occupent aussi parts[].
         *
         * Et, pour « sort marked », en ne touchant QUE les emplacements qui
         * portaient une carte marquée : c'est ce qui fait du tri un tri en
         * place. Les cartes non marquées gardent leur rang exact. */
        k = 0;
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i]->type == OBJ_CARD &&
                (!marquees || stack->parts[i]->marked))
                stack->parts[i] = tab[k++].card;

        for (int i = 0; i < n2; i++) free(cles[i]);
        free(cles); free(tab);
        set_result("");
        g_atop = sauve;
        return 1;
    }

    /* --- tri d'un conteneur --- */
    {
        const char *by = find_kw(a, "by");
        ARENA_MARK;
        char *cible = arena_buf();
        {
            int len = by ? (int)(by - a) : (int)strlen(a);
            if (len > HC_VAL - 1) len = HC_VAL - 1;
            memcpy(cible, a, (size_t)len); cible[len] = '\0';
        }

        /* Les options peuvent suivre la cible : « sort field 1 descending ». */
        char *fin = cible + strlen(cible);
        while (fin > cible && isspace((unsigned char)fin[-1])) *--fin = '\0';
        for (;;) {
            char *mot = fin;
            while (mot > cible && !isspace((unsigned char)mot[-1])) mot--;
            if (mot == cible) break;
            int d2 = desc; SortStyle s2 = style;
            const char *apres = sort_options(mot, &d2, &s2);
            if (apres == mot) break;            /* pas une option */
            desc = d2; style = s2;
            while (mot > cible && isspace((unsigned char)mot[-1])) mot--;
            *mot = '\0'; fin = mot;
        }

        const char *cle = by ? skip_spaces(by + 2) : NULL;

        /* LE CONTENEUR AUSSI EST DANS L'ARBRE. Quand fils[0] est un morceau
         * — « lines of card field "L" » —, le conteneur est sa BASE ; sinon
         * c'est fils[0] lui-même. Une lecture par commande, pas par élément,
         * mais c'était la dernière de « sort » à repasser par le texte. */
        const HctNoeud *ncible = NULL;
        if (n->nfils >= 1) {
            const HctNoeud *f0 = n->fils[0];
            if (f0->genre == HCTN_CHUNK && f0->nfils >= 1)
                ncible = f0->fils[f0->nfils - 1];
            else if (f0->genre == HCTN_OBJET || f0->genre == HCTN_IDENT)
                ncible = f0;
        }

        /* LA SOURCE NE PASSE PLUS PAR UN TAMPON FIXE.
         *
         * v3_val_texte recopiait la valeur dans HC_VAL — soixante-quatre
         * kilo-octets — et le résultat était ensuite RÉÉCRIT dans le champ.
         * Mesuré : « sort lines of card field "gros" » sur un champ de
         * 200 000 caractères le ramenait à 65 525, et ses 20 000 lignes à
         * 6 553. Sans un mot. Ce n'est pas une troncature d'affichage, c'est
         * une destruction de données.
         *
         * HctValeur alloue son texte à sa taille : on en prend la propriété par
         * hct_val_prend — d'où le free et non hct_val_libere. Reprendre v.txt
         * à la main, comme on le faisait, explosait sur une valeur VIDE : son
         * texte est une sentinelle statique, et free() n'en veut pas. */
        char *src_dyn = NULL;
        const char *src;
        if (ncible) {
            HctValeur v = hct_evalue(ctx, ncible);
            if (ctx->erreur) { hct_val_libere(&v); ARENA_FREE;
                               g_atop = sauve; return 1; }
            src_dyn = hct_val_prend(&v);     /* propriété reprise */
            src = src_dyn ? src_dyn : "";
        } else {
            char *tmp = arena_buf();
            tmp[0] = '\0';
            eval_checked(cible, tmp, HC_VAL);
            src = tmp;
        }

        int n2 = chunk_count(src, morceau);
        if (n2 < 2) { free(src_dyn); ARENA_FREE; g_atop = sauve; return 1; }

        SortItem *tab = calloc((size_t)n2, sizeof *tab);
        char **cles = calloc((size_t)n2, sizeof *cles);
        char **elems = calloc((size_t)n2, sizeof *elems);
        if (!tab || !cles || !elems) { free(tab); free(cles); free(elems);
                                       free(src_dyn);
                                       ARENA_FREE; g_atop = sauve; return 1; }

        for (int i = 0; i < n2; i++) {
            int b = 0, e = 0;
            chunk_span1(src, morceau, i + 1, &b, &e);
            elems[i] = malloc((size_t)(e - b) + 1);
            /* Ce malloc n'était pas testé, et le memcpy qui suit écrivait donc
             * dans NULL sous pression mémoire — en contournant justement le
             * point unique qu'on vient de mettre en place. */
            if (!elems[i]) hc_memoire_epuisee("éléments d'un tri");
            memcpy(elems[i], src + b, (size_t)(e - b));
            elems[i][e - b] = '\0';

            /* `each` : la variable que la clé interroge. Sans clé, on trie
             * directement sur l'élément. */
            if (ncle || cle) {
                var_set("each", elems[i]);
                /* Marque POSÉE À LA MAIN : la fonction en a déjà une, et
                 * ARENA_MARK déclare toujours la même variable — imbriquées,
                 * la seconde masquerait la première, ce que -Wshadow signale
                 * à juste titre puisque l'ordre de libération en dépendrait. */
                size_t marque_cle = g_atop;
                char *v = arena_buf();
                v[0] = '\0';
                if (ncle) v3_val_texte(ctx, ncle, v, HC_VAL);
                else      eval_checked(cle, v, HC_VAL);
                cles[i] = dupstr(v);
                g_atop = marque_cle;
            } else {
                cles[i] = dupstr(elems[i]);
            }
            /* LA CLÉ AUSSI, pas seulement l'élément.
             *
             * elems[i] était gardé deux lignes plus haut, cles[i] non — et
             * c'est POURTANT la clé que sort_cmp déréférence, sans vérifier.
             * Mesuré en faisant échouer un seul malloc : c'est le plantage
             * le plus fréquent de tout le balayage. */
            if (!cles[i]) hc_memoire_epuisee("clés d'un tri");
            tab[i].cle = cles[i]; tab[i].rang = i; tab[i].card = NULL;
        }

        g_sort_desc = desc; g_sort_style = style;
        qsort(tab, (size_t)n2, sizeof *tab, sort_cmp);

        /* LE RÉSULTAT NON PLUS. Le « break » quand le tampon était plein
         * jetait les éléments restants — la seconde moitié de la destruction.
         * On mesure d'abord, on alloue juste, on remplit sans garde : la
         * somme des longueurs plus un séparateur par élément ne peut pas
         * déborder ce qu'on vient de calculer. */
        const char *sep = chunk_sep(morceau);
        size_t lsep = strlen(sep);
        size_t total = 0;
        for (int i = 0; i < n2; i++) total += strlen(elems[i]) + lsep;
        char *res = malloc(total + 1);
        if (!res) hc_memoire_epuisee("résultat d'un tri");
        size_t used = 0;
        for (int i = 0; i < n2; i++) {
            const char *el = elems[tab[i].rang];
            size_t l = strlen(el);
            /* Le séparateur ENTIER : un seul octet recollait « cébéa » trié en
             * « a\xc3b\xc3c », de l'UTF-8 invalide. */
            if (i) { memcpy(res + used, sep, lsep); used += lsep; }
            memcpy(res + used, el, l); used += l;
        }
        res[used] = '\0';

        container_set(cible, res, 0);
        free(res);

        for (int i = 0; i < n2; i++) { free(cles[i]); free(elems[i]); }
        free(cles); free(elems); free(tab); free(src_dyn);
        ARENA_FREE;
        set_result("");
        g_atop = sauve;
        return 1;
    }
}

/* find [string|chars|whole|word] "motif" [in <champ>]
 *
 * Motif hct_cmd.c : « * », sans borne — l'analyseur découpe la ligne entière
 * mot à mot, exactement comme pour « sort » et « visual ». Même remède :
 * v3_reste rend le texte EXACT que lisait l'ancien exécuteur, et son
 * algorithme — lecture du mode, recherche de « in » hors des guillemets,
 * balayage carte par carte puis champ par champ — s'applique tel quel au
 * résultat. */
static int v3_cmd_find(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
    size_t sauve = g_atop;
    char *mots = arena_buf();
    v3_reste(n, mots, HC_VAL);

    const char *r = skip_spaces(mots);
    int mode = 0;                  /* 0 = debut de mot, 1 = n'importe ou, 2 = mot entier */
    if      (ci_word(r, "string") || ci_word(r, "chars")) { mode = 1; r = skip_spaces(r + 6); }
    else if (ci_word(r, "whole"))  { mode = 1; r = skip_spaces(r + 5); }
    else if (ci_word(r, "word"))   { mode = 2; r = skip_spaces(r + 4); }

    /* separer le motif de l'eventuel « in <champ> » */
    const char *kw = NULL;
    int inq = 0;
    for (const char *q = r; *q; q++) {
        if (*q == '"') { inq = !inq; continue; }
        if (inq) continue;
        if ((q == r || isspace((unsigned char)q[-1])) && ci_word(q, "in")) { kw = q; break; }
    }
    char pat[256] = "", where[128] = "";
    if (kw) {
        char e[256];
        int len = (int)(kw - r);
        if (len > (int)sizeof e - 1) len = (int)sizeof e - 1;
        memcpy(e, r, (size_t)len); e[len] = 0;
        eval_expr(e, pat, sizeof pat);
        snprintf(where, sizeof where, "%s", skip_spaces(kw + 2));
    } else {
        eval_expr(r, pat, sizeof pat);
    }
    if (!pat[0]) { set_result("not found"); g_atop = sauve; return 1; }

    Object *stack = g_current_card ? g_current_card->owner : NULL;
    while (stack && stack->type != OBJ_STACK) stack = stack->owner;
    if (!stack) { set_result("not found"); g_atop = sauve; return 1; }

    int total = card_count(stack);
    int start = card_index(stack, g_current_card);
    if (start < 0) start = 0;

    for (int k = 0; k < total; k++) {
        Object *cd = nth_card(stack, (start + k) % total);
        if (!cd) continue;

        /* « Don't Search This Card » : on saute la carte ENTIÈRE, et pas
         * seulement tel ou tel champ. Le verrou du FOND vaut pour toutes ses
         * cartes — c'est ainsi qu'on tient un mode d'emploi ou une carte
         * d'index hors des résultats sans avoir à cocher chaque champ.
         *
         * La carte COURANTE n'échappe pas à la règle : HyperCard non plus, et
         * une exception ici ferait qu'une recherche trouve sur place ce
         * qu'elle ne retrouvera jamais en repassant. */
        if (cd->dont_search) continue;
        if (cd->bg && cd->bg->dont_search) continue;

        /* champs de la carte puis du fond */
        Object *layers[2] = { cd, cd->bg };
        for (int L = 0; L < 2; L++) {
            Object *lay = layers[L];
            if (!lay) continue;
            for (int i = 0; i < lay->nparts; i++) {
                Object *fl = lay->parts[i];
                if (fl->type != OBJ_FIELD || fl->dont_search) continue;
                if (where[0]) {          /* recherche restreinte a un champ */
                    Object *only = resolve(where);
                    if (only != fl) continue;
                }

                Object *saved = g_current_card;
                g_current_card = cd;                 /* pour le texte par carte */
                const char *tx = hc_field_text(fl);
                const char *hit = NULL;
                int line = 1;

                for (const char *q = tx; *q; q++) {
                    if (*q == '\n') { line++; continue; }
                    int atword = (q == tx) || isspace((unsigned char)q[-1]);
                    if (mode == 1 || atword) {
                        size_t plen = strlen(pat);
                        if (strncasecmp(q, pat, plen) == 0) {
                            if (mode == 2) {         /* mot entier */
                                char nx = q[plen];
                                if (nx && !isspace((unsigned char)nx) &&
                                    !ispunct((unsigned char)nx)) continue;
                            }
                            hit = q;
                            break;
                        }
                    }
                }
                g_current_card = saved;

                if (hit) {
                    snprintf(g_found_text, sizeof g_found_text, "%s", pat);
                    g_found_field = fl;
                    g_found_line = line;
                    g_found_start = (int)(hit - tx);
                    g_found_len   = (int)strlen(pat);
                    g_found_card  = cd;
                    g_found_montre = 1;

                    if (cd != g_current_card) {      /* naviguer si besoin */
                        Object *old = g_current_card;
                        Object *oldbg = old ? old->bg : NULL;
                        if (old) hc_send_systeme(old, "closeCard");
                        if (oldbg && oldbg != cd->bg) hc_send_systeme(oldbg, "closeBackground");
                        g_current_card = cd;
                        if (cd->bg && cd->bg != oldbg) hc_send_systeme(cd->bg, "openBackground");
                        hc_send_systeme(cd, "openCard");
                    }
                    set_result("");
                    emit(HC_INFO, "   ⇒ trouvé \"%s\" dans la carte \"%s\"",
                         pat, cd->name ? cd->name : "?");
                    g_atop = sauve;
                    return 1;
                }
            }
        }
    }
    g_found_text[0] = 0; g_found_field = NULL; g_found_line = 0;
    g_found_start = g_found_len = 0; g_found_card = NULL;
    g_found_montre = 0;
    set_result("not found");
    g_atop = sauve;
    return 1;
}

/* send "<message>" to <objet>
 *
 * Motif hct_cmd.c : « e [to r] ». Sans « to r » c'est une forme fautive que
 * l'ancien exécuteur refuse avec son propre message ; on lui laisse ce cas
 * plutôt que de le reproduire à côté.
 *
 * Le message reste un TEXTE à redécouper après évaluation — nom du
 * gestionnaire, puis arguments séparés par des virgules — exactement comme
 * le faisait l'ancien exécuteur : « send "carre" & n to bouton » ne sait pas
 * d'avance combien d'arguments son résultat portera. La cible, elle, n'a
 * plus besoin de repasser par le texte : c'est un vrai nœud HCTN_OBJET, que
 * hct_resout résout directement — avec un repli sur le texte, comme dans
 * v3_cmd_go, pour les formes qu'il ne couvre pas encore. */
static int v3_cmd_send(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 3 || !v3_est_motcle(n, 1, "to")) return 0;

    ARENA_MARK;
    char *msgline = arena_buf();
    v3_val_texte(ctx, n->fils[0], msgline, HC_VAL);
    if (ctx->erreur) { ARENA_FREE; return 1; }

    Object *target = hct_resout(ctx, n->fils[2]);
    if (!target) {
        char v[256];
        v3_val_texte(ctx, n->fils[2], v, sizeof v);
        if (ctx->erreur) { ARENA_FREE; return 1; }
        if (v[0]) target = resolve(v);
    }
    if (!target) {
        set_result("destinataire introuvable");
        emit(HC_ERR, "   !! destinataire introuvable");
        ARENA_FREE;
        return 1;
    }

    /* découper le message en nom + arguments */
    char msg[128];
    const char *a = next_word(skip_spaces(msgline), msg, sizeof msg);
    /* Une ligne de PLUS que le plafond : on range le premier argument de trop
     * pour que hc_send_args_k_body le compte et refuse. S'arrêter au plafond
     * le ferait disparaître en silence — c'est ce qui arrivait. */
    char (*argv)[HC_VAL] = arena_rows(HC_ARGS_MAX + 1);
    if (!argv) {
        set_result("mémoire insuffisante");
        ARENA_FREE;
        return 1;
    }
    int argc = 0;
    a = skip_spaces(a);
    while (*a && argc <= HC_ARGS_MAX) {
        char *one = arena_buf();
        int len = 0, depth = 0, inq = 0;
        while (*a && !(depth == 0 && !inq && *a == ',')) {
            if (*a == '"') inq = !inq;
            else if (!inq && *a == '(') depth++;
            else if (!inq && *a == ')') depth--;
            /* Même vestige qu'à split_args : « one » vient de l'arène et fait
             * HC_VAL. « send "traite " & uneTresLongueExpression to me »
             * voyait son argument coupé à 511 caractères. */
            if (len < HC_VAL - 1) one[len++] = *a;
            a++;
        }
        one[len] = '\0';
        /* Le texte du message est CONSTRUIT à l'exécution — « send "carre" & n
         * to bouton » ne sait pas d'avance combien d'arguments il portera —,
         * donc il n'y a aucun arbre antérieur à réemployer : il faut bien
         * analyser ce texte-ci. Mais c'est la v3 qui doit le faire.
         * hct_evalue_texte est le moteur de « the value of » ; il refuse
         * proprement ce qui n'est pas une expression complète, et l'ancien
         * évaluateur reste le repli pour ces cas-là. */
        {
            HctValeur v;
            if (hct_evalue_texte(ctx, one, n, &v)) {
                snprintf(argv[argc], sizeof argv[argc], "%s", v.txt ? v.txt : "");
                hct_val_libere(&v);
            } else {
                eval_expr(one, argv[argc], sizeof argv[argc]);
            }
        }
        argc++;
        if (*a == ',') a = skip_spaces(a + 1); else break;
    }

    set_result("");     /* un `return` dans le gestionnaire le remplira */
    hc_send_args(target, msg, argv, argc);
    ARENA_FREE;
    return 1;
}

/* Les deux énumérations de sortes de morceau disent la même chose, chacune
 * dans sa maison : celle de l'arbre, celle du noyau. */
static ChunkType v3_sorte_chunk(HctSorteChunk s)
{
    switch (s) {
        case HCT_CH_WORD: return CH_WORD;
        case HCT_CH_ITEM: return CH_ITEM;
        case HCT_CH_LINE: return CH_LINE;
        default:          return CH_CHAR;
    }
}

/* Le champ visé par une référence de morceau, et l'intervalle exact de
 * caractères qu'elle couvre — lus dans l'ARBRE.
 *
 * chunk_target fait déjà cela depuis le TEXTE de la référence, mais il la
 * redécoupe à la main et confie chaque indice à eval_expr : « set the
 * textStyle of word 3 of line 3 to 6 of me to bold » faisait ainsi relire
 * trois expressions que l'analyseur avait réduites en arbre quelques
 * instants plus tôt. C'était le dernier « v3 relit set » du relevé, sur le
 * calendrier d'HyperCard 2.x, dont markToday souligne le jour courant.
 *
 * La récursion suit celle de chunk_target — un morceau peut porter sur un
 * morceau — et s'arrête sur un nœud d'OBJET, seul socle qui ait un texte et
 * des plages de style.
 *
 * Rendre NULL n'est pas une faute : l'appelant repart alors par le texte,
 * qui sait lire des formes que l'arbre ne présente pas ainsi. */
static Object *v3_chunk_cible(HctContexte *ctx, const HctNoeud *ch,
                              int *st, int *en)
{
    if (!ch || ch->genre != HCTN_CHUNK || ch->nfils < 1) return NULL;

    const HctNoeud *base = ch->fils[ch->nfils - 1];

    Object *fld;
    int base_off = 0;
    const char *texte;

    int dedans_st = 0, dedans_en = 0;
    Object *dedans = v3_chunk_cible(ctx, base, &dedans_st, &dedans_en);
    if (ctx->erreur) return NULL;
    if (dedans) {                                   /* morceau de morceau */
        fld = dedans;
        base_off = dedans_st;
        int len = dedans_en - dedans_st;
        if (len < 0) len = 0;
        /* Ici une copie est inévitable : on a besoin d'un SOUS-TEXTE terminé
         * par zéro, et il n'existe nulle part. Le morceau porte déjà sur un
         * morceau, donc il est borné par construction. */
        char *tampon = arena_buf();
        snprintf(tampon, HC_VAL, "%.*s", len, hc_field_text(fld) + dedans_st);
        texte = tampon;
    } else {
        if (base->genre != HCTN_OBJET) return NULL;
        fld = hct_resout(ctx, base);
        if (ctx->erreur) return NULL;
        if (!fld || fld->type != OBJ_FIELD) return NULL;
        /* Le TEXTE DU CHAMP DIRECTEMENT, sans copie.
         *
         * On le recopiait dans un tampon de HC_VAL — soixante-quatre kilo-
         * octets — alors qu'un champ peut en porter bien davantage : mesuré,
         * un champ de 200 000 caractères a bien ses 200 mots, mais
         * « set the textStyle of word 150 » répondait « morceau hors
         * limites » parce que le mot 150 tombe après le tampon. La copie
         * n'apportait rien : on ne fait que LIRE ce texte pour compter des
         * morceaux. */
        texte = hc_field_text(fld);
    }

    ChunkType ct = v3_sorte_chunk(ch->sorte);
    int a = 0, b = 0;
    if (ch->ordinal != HCT_ORD_AUCUN) {
        a = hct_rang_ordinal(ch->ordinal, chunk_count(texte, ct));
    } else {
        /* Les bornes passent par hct_vers_rang comme partout ailleurs : le
         * « (int)d » qui se trouvait ici était le dernier de sa famille, et
         * convertir un double hors plage est un comportement indéfini. Le
         * rang doit aussi être un NOMBRE — strtod rend zéro pour « canard ». */
        char v[128];
        int nbornes = ch->nfils - 1;
        if (nbornes >= 1) {
            v3_val_texte(ctx, ch->fils[0], v, sizeof v);
            if (ctx->erreur) return NULL;
            int hors = 0;
            if (!hct_est_nombre(v)) {
                hct_ctx_faute(ctx, ch->fils[0],
                              "un rang numérique est attendu ici");
                return NULL;
            }
            a = hct_vers_rang(v, &hors);
            if (hors) { hct_ctx_faute(ctx, ch->fils[0],
                                      "rang de morceau hors limites");
                        return NULL; }
        }
        if (nbornes >= 2) {
            v3_val_texte(ctx, ch->fils[1], v, sizeof v);
            if (ctx->erreur) return NULL;
            int hors = 0;
            if (!hct_est_nombre(v)) {
                hct_ctx_faute(ctx, ch->fils[1],
                              "un rang numérique est attendu ici");
                return NULL;
            }
            b = hct_vers_rang(v, &hors);
            if (hors) { hct_ctx_faute(ctx, ch->fils[1],
                                      "rang de morceau hors limites");
                        return NULL; }
        }
    }

    int s2, e2;
    if (!chunk_span(texte, ct, a, b, &s2, &e2)) return NULL;
    *st = base_off + s2;
    *en = base_off + e2;
    return fld;
}

/* set [the] <propriété> [of <cible>] to <valeur>
 *
 * Motif hct_cmd.c : « e to * ». Le « e » couvre déjà « [the] <propriété> [of
 * <cible>] » comme UNE SEULE expression — chunk_ou_of, qui fabrique un nœud
 * HCTN_OF pour « textStyle of word 3 of field 1 » aussi bien qu'un
 * HCTN_IDENT nu pour « cursor » — et le « to *» qui suit redevient du texte
 * brut, comme pour « visual », « sort » et « find ». Même remède : v3_reste
 * rend exactement ce que lisait l'ancien exécuteur — « the » et parenthèses
 * compris, voir sa définition — et son algorithme — bascule propriété
 * globale/objet/morceau, textStyle et textColor en noms nus, plage de style
 * versus objet — s'applique tel quel au résultat. Rien de cet algorithme
 * n'a été touché ; seule la source du texte a changé. */
static int v3_menu_prop_ecrit(HctContexte *ctx, const HctNoeud *obj,
                              const char *prop, const char *val);
static const HctNoeud *v3_set_cible_menu(const HctNoeud *n, char *prop, int len);


/* UNE VALEUR EN LISTE — « to 1,2,30,40 » — EST DÉJÀ DANS L'ARBRE.
 *
 * Le motif de `set` est « e to * », et l'étoile vaut « zéro à N expressions
 * séparées par des virgules » : l'analyseur rend donc un enfant PAR ÉLÉMENT.
 * v3_cmd_set ne lisait le sous-arbre que lorsqu'il y en avait exactement un,
 * et retombait sinon sur eval_checked, qui relit le texte brut « 1,2,30,40 »
 * comme UNE expression — hct_expression s'arrête à la première virgule.
 *
 * Résultat : parse_ints n'en comptait qu'un au lieu de quatre, l'écriture
 * était abandonnée, et rien ne bougeait. Sans un mot, comme toujours.
 *
 *     set the rect of button "Ok" to 1,2,30,40     ne faisait rien
 *     set the rect of button "Ok" to "1,2,30,40"   marchait
 *     set the loc of button "Ok" to 200,200        ne faisait rien
 *
 * On évalue donc chaque élément et on les rejoint par des virgules, ce qui
 * est très exactement ce que l'étoile a découpé. Chaque élément est une
 * expression à part entière : « set the loc of X to item 1 of p, item 2 of p »
 * marche pour la même raison.
 *
 * Rend le nombre d'éléments écrits ; s'arrête à la première erreur. */
static int v3_val_liste(HctContexte *ctx, const HctNoeud *n, int premier,
                        char *out, int outlen)
{
    int pos = 0, compte = 0;
    out[0] = '\0';
    for (int i = premier; i < n->nfils; i++) {
        char un[256];
        v3_val_texte(ctx, n->fils[i], un, sizeof un);
        if (ctx->erreur) return compte;
        int ecrit = snprintf(out + pos, (size_t)(outlen - pos),
                             "%s%s", compte ? "," : "", un);
        if (ecrit < 0 || ecrit >= outlen - pos) { out[outlen - 1] = '\0'; break; }
        pos += ecrit;
        compte++;
    }
    return compte;
}

static int v3_cmd_set(HctContexte *ctx, const HctNoeud *n)
{
    /* Les propriétés de menu d'abord : leur cible n'est pas un objet de la
     * pile, et le chemin ordinaire — reconstitution du texte puis résolution
     * — n'en ferait rien. */
    {
        char prop[64];
        const HctNoeud *obj = v3_set_cible_menu(n, prop, sizeof prop);
        if (obj && n->nfils >= 3) {
            /* Même marge que pour la création : un nom trop long doit
             * arriver trop long pour que v3_menu_prop_ecrit le refuse. */
            char val[HC_MENU_NOM_MAX + 2];
            v3_val_texte(ctx, n->fils[n->nfils - 1], val, sizeof val);
            if (ctx->erreur) return 1;
            if (v3_menu_prop_ecrit(ctx, obj, prop, val)) { set_result(""); return 1; }
            /* LAQUELLE DES TROIS ? Voir V3MenuEchec : le menu, l'article, ou
             * le nom de la propriété. Seule la troisième a une raison de
             * nommer `prop` — les deux autres l'accuseraient à tort. */
            if (g_menu_echec == V3_MENU_PROP_INCONNUE || g_menu_echec == V3_MENU_RIEN) {
                emit(HC_ERR, "   !! propriété de menu inconnue : %s", prop);
                set_result("propriété inconnue");
            } else {
                emit(HC_ERR, "   !! %s", v3_menu_raison());
                set_result(g_menu_echec == V3_MENU_MENU_ABSENT
                           ? "No such menu" : "No such menu item");
            }
            return 1;
        }
    }

    size_t sauve = g_atop;             /* nommée à part, comme dans v3_cmd_sort :
                                         * les ARENA_MARK imbriqués ci-dessous
                                         * doivent rembobiner à LEUR marque sans
                                         * toucher à `mots`. */
    char *mots = arena_buf();
    v3_reste(n, mots, HC_VAL);

    g_visual_dirty = 1;
    const char *s = skip_spaces(mots);
    if (ci_word(s, "the")) s = skip_spaces(s + 3);

    char prop[64];
    const char *q = next_word(s, prop, sizeof prop);
    q = skip_spaces(q);

    /* Pas de « of » : c'est une propriété globale, pas celle d'un objet.
     * « set cursor to none », « set lockScreen to true »… */
    if (!ci_word(q, "of")) {
        /* « set word 2 of field "x" to bold » : le nom de propriete manque.
         * Sans ce garde-fou, « word » partait comme propriete globale chez
         * l'hote et l'ecriture disparaissait sans un mot. */
        if (ci_equal(prop, "char") || ci_equal(prop, "character")
         || ci_equal(prop, "word") || ci_equal(prop, "item")
         || ci_equal(prop, "line")) {
            set_result("propriete manquante");
            emit(HC_ERR, "   !! set : quelle propriete ? essayez "
                         "« set the textStyle of %s … »", skip_spaces(mots));
            g_atop = sauve; return 1;
        }
        const char *to = find_kw(s, "to");
        if (!to) {
            emit(HC_ERR, "   !! set mal formé : %s", skip_spaces(mots));
            g_atop = sauve; return 1;
        }
        char *val = arena_buf();

        /* La VALEUR est déjà un sous-arbre : l'évaluer, plutôt que relire son
         * texte.
         *
         * eval_checked relexe et réanalyse une expression que l'analyseur
         * avait déjà réduite en arbre quelques instants plus tôt. Sur une
         * séance de Graph Maker, « set » à lui seul refaisait ce travail
         * 114 fois — « set pattern to getPattern(i) » dans une boucle de
         * tracé, « set cursor to watch », « set filled to true ».
         *
         * TROIS ENFANTS SEULEMENT, et une cible qui soit un simple nom. Une
         * valeur en liste — « set the textStyle to bold,condense » — en donne
         * quatre, un par élément, et il n'y a plus de sous-arbre unique à
         * évaluer : ce cas garde le chemin par le texte, qui sait le lire.
         * Mieux vaut porter la forme courante et laisser la rare derrière que
         * de tout porter mal. */
        if (n->nfils == 3 && n->fils[0]->genre == HCTN_IDENT) {
            v3_val_texte(ctx, n->fils[2], val, HC_VAL);
            if (ctx->erreur) { g_atop = sauve; return 1; }
        } else {
            eval_checked(to + 2, val, HC_VAL);
        }

        if (prop_globale_noyau(prop, val)) {
            set_result("");
            g_atop = sauve; return 1;
        }

        /* UNE PROPRIÉTÉ GLOBALE INCONNUE SE DIT, ELLE NE S'AVALE PAS.
         *
         * L'écriture partait droit chez l'hôte, quel que soit le nom. Une
         * coquille ne produisait donc RIEN : ni effet, ni message. Trouvé dans
         * Graph Maker 2.2, où l'auteur avait écrit
         *
         *     set the foreclor to green
         *
         * et où le trait est resté noir pendant trente-huit ans.
         *
         * L'incohérence était complète : « put the foreclor » rendait déjà
         * « propriété ou fonction inconnue », et « set the widht of card
         * button 1 » aussi. Seule l'écriture d'une GLOBALE passait. Un langage
         * qui refuse de lire ce qu'il accepte d'écrire ment sur l'un des deux.
         *
         * La liste de référence est celle que la LECTURE emploie déjà : le
         * noyau ne peut pas connaître ces propriétés seul — elles vivent chez
         * l'hôte —, mais il sait lesquelles existent, et c'est tout ce qu'il
         * faut pour refuser le reste. Un seul endroit, donc, et les deux sens
         * du même nom ne peuvent plus diverger. */
        if (!dans_liste(prop, V3_GLOBALES_ECRIVABLES)) {
            /* Deux refus BIEN DISTINCTS, parce qu'ils appellent deux gestes
             * différents : corriger une coquille, ou renoncer à écrire ce qui
             * ne s'écrit pas. */
            if (dans_liste(prop, V3_GLOBALES_HOTE)) {
                set_result("propriété en lecture seule");
                emit(HC_ERR, "   !! propriété en lecture seule : %s", prop);
            } else {
                set_result("propriété inconnue");
                emit(HC_ERR, "   !! propriété inconnue : %s", prop);
            }
            g_atop = sauve; return 1;
        }

        host_global_set(prop, val);
        set_result("");
        emit(HC_INFO, "   → %s ← \"%s\"", prop, val);
        g_atop = sauve; return 1;
    }
    q = skip_spaces(q + 2);

    /* Le « to » qui separe la cible de la valeur est le DERNIER de premier
     * niveau : la cible peut en contenir elle-meme, comme dans
     *     set the textStyle of word d of line 3 to numLines of me to bold
     * Couper au premier tronquait la cible a « line 3 ». */
    const char *to = NULL, *scan = q;
    for (const char *k = find_kw(scan, "to"); k; k = find_kw(scan, "to")) {
        to = k; scan = k + 2;
    }
    if (!to) {
        emit(HC_ERR, "   !! set sans « to » : %s", skip_spaces(mots));
        g_atop = sauve; return 1;
    }

    char refbuf[256];
    int nref = (int)(to - q);
    if (nref > (int)sizeof refbuf - 1) nref = (int)sizeof refbuf - 1;
    memcpy(refbuf, q, (size_t)nref); refbuf[nref] = '\0';
    while (nref > 0 && (refbuf[nref-1] == ' ' || refbuf[nref-1] == '\t')) refbuf[--nref] = '\0';

    char *val = arena_buf();
    /* « to bold,condense » n'est pas une expression : c'est une liste de
     * noms de styles, qu'HyperCard accepte sans guillemets. On prend donc
     * le texte brut, sauf s'il nomme une variable — auquel cas on lit sa
     * valeur, pour que « set the textStyle of X to myStyle » marche. */
    /* Même problème pour la couleur : « to white » n'est pas une
     * expression, c'est un nom que HyperCard accepte nu. L'évaluer le
     * traitait comme un identifiant inconnu, et le résultat vide se
     * changeait en HC_COLOR_INHERIT — rien ne bougeait, sans un mot.
     *
     * On ne prend le texte brut que s'il NOMME une couleur : « to
     * theColor » ou « to item 1 of liste » restent des expressions. */
    if (ci_equal(prop, "textcolor")) {
        const char *raw = skip_spaces(to + 2);
        int rl = (int)strlen(raw);
        while (rl > 0 && isspace((unsigned char)raw[rl-1])) rl--;

        char brut[64];
        snprintf(brut, sizeof brut, "%.*s", rl, raw);

        /* Déguillemeter d'abord : « to "white" » nomme aussi une couleur. */
        int nb = (int)strlen(brut);
        if (nb > 1 && brut[0] == '"' && brut[nb-1] == '"') {
            memmove(brut, brut + 1, (size_t)(nb - 2));
            brut[nb - 2] = '\0';
        }

        if (color_from_name(brut) != HC_COLOR_INHERIT)
            snprintf(val, HC_VAL, "%s", brut);
        else
            eval_checked(to + 2, val, HC_VAL);
    }
    else if (ci_equal(prop, "textstyle")) {
        const char *raw = skip_spaces(to + 2);
        int rl = (int)strlen(raw);
        while (rl > 0 && isspace((unsigned char)raw[rl-1])) rl--;
        snprintf(val, HC_VAL, "%.*s", rl, raw);

        /* Trancher sur le CONTENU, pas sur la ponctuation : une suite de
         * noms de style reste littérale, tout le reste est évalué comme
         * l'expression que c'est. */
        if (!style_is_names(val)) {
            int nv = (int)strlen(val);
            int quoted = 0;

            /* Une liste de noms peut arriver citée : « to "bold,italic" ».
             * On la déguillemete pour la re-tester. */
            if (nv > 1 && val[0] == '"' && val[nv-1] == '"') {
                char *inner = arena_buf();
                snprintf(inner, HC_VAL, "%.*s", nv - 2, val + 1);
                if (style_is_names(inner)) {
                    snprintf(val, HC_VAL, "%s", inner);
                    quoted = 1;
                }
            }
            if (!quoted) eval_checked(to + 2, val, HC_VAL);
        }
    } else {
        /* Le cas général — « set the rect of bg btn "L" to r ». Comme pour
         * les propriétés globales plus haut : la valeur est déjà un
         * sous-arbre, on l'évalue au lieu de relire son texte.
         *
         * Pas pour textColor ni textStyle, traités juste au-dessus : ceux-là
         * acceptent une liste de noms nus — « to bold,condense » — qui n'est
         * pas une expression et que l'arbre découpe en plusieurs enfants. Le
         * test sur nfils == 3 les écarterait de toute façon ; le dire ici
         * évite qu'on se demande pourquoi dans six mois. */
        if (n->nfils == 3) {
            v3_val_texte(ctx, n->fils[2], val, HC_VAL);
            if (ctx->erreur) { g_atop = sauve; return 1; }
        } else if (n->nfils > 3) {
            /* « to 1,2,30,40 » : un enfant par élément. Voir v3_val_liste. */
            v3_val_liste(ctx, n, 2, val, HC_VAL);
            if (ctx->erreur) { g_atop = sauve; return 1; }
        } else {
            eval_checked(to + 2, val, HC_VAL);
        }
    }

    /* La cible peut designer une PLAGE DE TEXTE et non un objet :
     *     set the textStyle of word 3 of line 2 of field "cal" to bold
     * resolve() ne sait chercher que des objets, d'ou l'echec historique
     * « objet introuvable : word theNewDay of line 3 ». On essaie donc
     * d'abord le morceau, avant de retomber sur la resolution d'objet. */
    {
        int cst = 0, cen = 0;
        /* La cible est déjà dans l'arbre : le premier fils est un OF dont le
         * second enfant est le morceau. La lire évite les eval_expr que
         * chunk_target fait sur chaque indice. Le texte reste le recours. */
        Object *cf = NULL;
        if (n->fils[0]->genre == HCTN_OF && n->fils[0]->nfils == 2)
            cf = v3_chunk_cible(ctx, n->fils[0]->fils[1], &cst, &cen);
        if (ctx->erreur) { g_atop = sauve; return 1; }
        if (!cf) cf = chunk_target(refbuf, &cst, &cen);
        if (cf) {
            /* Les trois attributs de texte se posent par plage, comme dans
             * HyperCard 2.x ; le reste (rect, visible…) décrit un objet et
             * n'a aucun sens sur un morceau. */
            int mask = 0;
            if      (ci_equal(prop, "textstyle")) mask = RA_STYLE;
            else if (ci_equal(prop, "textfont"))  mask = RA_FONT;
            else if (ci_equal(prop, "textcolor")) mask = RA_COLOR;
            else if (ci_equal(prop, "textsize")) mask = RA_SIZE;

            if (!mask) {
                if (!is_prop_name(prop, (int)strlen(prop))) {
                    set_result("propriete inconnue");
                    emit(HC_ERR, "   !! propriete inconnue : %s"
                                 " (les proprietes de texte sont textFont,"
                                 " textSize, textStyle)", prop);
                } else {
                    set_result("propriete non applicable a un morceau");
                    emit(HC_ERR, "   !! %s decrit un objet, pas un morceau"
                                 " de texte", prop);
                }
                g_atop = sauve; return 1;
            }
            struct RunList *rl = runs_of(cf);
            if (!rl) {
                set_result("champ sans stockage de style");
                emit(HC_ERR, "   !! ce champ n'a pas encore de texte sur cette carte");
                g_atop = sauve; return 1;
            }
            runs_set_attr(rl, cst, cen - cst, mask,
                          (mask & RA_STYLE) ? style_from_names(val) : 0,
                          (mask & RA_SIZE)  ? hc_entier(val, 0, HC_TEXTE_MAX, 0) : 0,
                          (mask & RA_FONT)  ? val : NULL,
                          (mask & RA_COLOR) ? color_from_name(val)
                                            : HC_COLOR_INHERIT);
            notify_field(cf);
            set_result("");
            emit(HC_INFO, "   → %s de [%d..%d[ ← \"%s\"", prop, cst, cen, val);
            g_atop = sauve; return 1;
        }
    }

    Object *o = resolve(refbuf);
    if (!o) {
        /* Distinguer les deux echecs : une reference d'objet inconnue, ou
         * un morceau de texte qui n'existe pas (champ vide, rang au-dela
         * du contenu). « objet introuvable » sur « word 8 of line 3 of me »
         * envoyait chercher au mauvais endroit. */
        ChunkType xt; char xa[128], xb[128]; const char *xr; int xo;
        if (parse_chunk(refbuf, &xt, xa, sizeof xa, xb, sizeof xb, &xr, &xo)) {
            set_result("morceau hors limites");
            emit(HC_ERR, "   !! morceau hors limites : %s", refbuf);
        } else {
            set_result("objet introuvable");
            emit(HC_ERR, "   !! objet introuvable : %s", refbuf);
        }
        g_atop = sauve; return 1;
    }

    char d[64]; hc_describe(o, d, sizeof d);   /* avant modification */

    if (ci_equal(prop, "name")) {
        free(o->name);
        o->name = dupstr(val);
    } else if (ci_equal(prop, "visible")) {
        o->visible = truthy(val);
    } else if (ci_equal(prop, "enabled")) {
        o->enabled = truthy(val);
        notify_field(o);
    } else if (ci_equal(prop, "showname") || ci_equal(prop, "shownname")) {
        o->showname = truthy(val);
        notify_field(o);
    } else if (ci_equal(prop, "icon")) {
        /* Un numéro ou un nom : « set the icon of me to 3071 » comme
         * « ... to "Close Box" ». atoi seul rendait 0 sur tout nom, donc
         * silencieusement « aucune icône ». */
        o->icon = hc_resolve_icon(val);
        notify_field(o);
    } else if (ci_equal(prop, "selectedline") || ci_equal(prop, "selectedlines")) {
        o->selectedline = hc_entier(val, 0, HC_COORD_MAX, o->selectedline);
        notify_field(o);
    } else if (ci_equal(prop, "locktext")) {
        o->locktext = truthy(val); notify_field(o);
    } else if (ci_equal(prop, "widemargins")) {
        o->wide_margins = truthy(val); notify_field(o);
    } else if (ci_equal(prop, "fixedlineheight")) {
        o->fixed_lh = truthy(val); notify_field(o);
    } else if (ci_equal(prop, "showlines")) {
        o->show_lines = truthy(val); notify_field(o);
    } else if (ci_equal(prop, "autotab")) {
        o->auto_tab = truthy(val); notify_field(o);
    } else if (ci_equal(prop, "dontsearch")) {
        o->dont_search = truthy(val);
        /* notify_field ne vaut que pour un CHAMP : c'est un rafraîchissement
         * d'affichage, et une carte ou un fond n'en a que faire ici. */
        if (o->type == OBJ_FIELD) notify_field(o);
    } else if (ci_equal(prop, "cantdelete")) {
        o->cant_delete = truthy(val);
    } else if (ci_equal(prop, "textalign")) {
        /* Accepte aussi « centre » et « centered », qu'on rencontre dans
         * les scripts, et retombe à gauche sur un mot inconnu plutôt que
         * d'échouer : HyperCard est indulgent sur cette propriété. */
        if (ci_equal(val, "center") || ci_equal(val, "centre") ||
            ci_equal(val, "centered"))      o->text_align = 1;
        else if (ci_equal(val, "right"))    o->text_align = 2;
        else                                o->text_align = 0;
        notify_field(o);
    } else if (ci_equal(prop, "autoselect")) {
        o->auto_select = truthy(val);
        /* Un champ à sélection de lignes est forcément verrouillé : on ne
         * tape pas dans une liste de choix. HyperCard verrouillait de même,
         * et sans cela le clic ouvrirait l'éditeur au lieu de sélectionner. */
        if (o->auto_select) o->locktext = 1;
        notify_field(o);
    } else if (ci_equal(prop, "multiplelines")) {
        o->multiple_lines = truthy(val); notify_field(o);
    } else if (ci_equal(prop, "marked")) {
        o->marked = truthy(val);
    } else if (ci_equal(prop, "dontwrap")) {
        o->dont_wrap = truthy(val); notify_field(o);
    } else if (ci_equal(prop, "sharedhilite")) {
        o->shared_hilite = truthy(val); notify_field(o);
    } else if (ci_equal(prop, "sharedtext")) {
        /* Par la même porte que la case de l'Info : basculer le drapeau
         * seul rendrait le contenu inaccessible. */
        hc_set_shared_text(o, truthy(val));
    } else if (ci_equal(prop, "scroll")) {
        o->scroll = hc_coord(val, o->scroll);
        if (o->scroll < 0) o->scroll = 0;
        notify_field(o);
    } else if (ci_equal(prop, "textfont")) {
        free(o->textfont);
        o->textfont = (*val) ? dupstr(val) : NULL;
        notify_field(o);
    } else if (ci_equal(prop, "textstyle")) {
        /* Passe par style_from_names, comme la pose sur un morceau. */
        o->textstyle = style_from_names(val);
        notify_field(o);
    } else if (ci_equal(prop, "hilite") || ci_equal(prop, "highlight")) {
        hc_set_hilite(o, NULL, truthy(val));
        notify_field(o);
    } else if (ci_equal(prop, "family")) {
        /* HORS BORNES, ON REFUSE — on n'écrête pas.
         *
         * La famille va de 0 à 15. Ramener 20 à 15 rangerait silencieusement
         * le bouton dans le groupe 15, avec des frères qu'il n'a pas choisis,
         * et « set the family to 20 » suivi de « get the family » rendrait
         * 15 sans que rien ne l'ait dit. Un refus visible vaut mieux qu'un
         * groupement inventé. */
        int v = hc_entier(val, -1, 1000000, -1);
        if (v < 0 || v > 15) {
            emit(HC_ERR, "   !! la famille d'un bouton va de 0 à 15 "
                         "(0 = aucune) ; reçu « %s »", val);
            set_result("famille hors bornes");
            g_atop = sauve; return 1;
        }
        /* Une seule porte, partagée avec le dialogue Infos bouton : c'est
         * elle qui éteint les frères quand on entre dans une famille en
         * étant allumé. */
        hc_set_family(o, v);
        notify_field(o);
    } else if (ci_equal(prop, "titlewidth")) {
        o->titlewidth = hc_entier(val, 0, HC_TEXTE_MAX, o->titlewidth);
        notify_field(o);
    } else if (ci_equal(prop, "autohilite")) {
        o->autohilite = truthy(val);
    } else if (ci_equal(prop, "textsize")) {
        o->textsize = hc_entier(val, 0, HC_TEXTE_MAX, o->textsize);
        notify_field(o);
    } else if (ci_equal(prop, "textheight")) {
        /* Zéro rétablit la valeur déduite du corps. */
        int v = hc_entier(val, 0, HC_TEXTE_MAX, 0);
        o->textheight = v > 0 ? v : 0;
        notify_field(o);
    } else if (ci_equal(prop, "script")) {
        /* Une valeur qui touche exactement le plafond a presque
         * certainement été tronquée en chemin. L'écrire telle quelle
         * détruirait le script. On refuse : mieux vaut un gestionnaire
         * qui échoue qu'une pile mutilée. */
        if (g_script_clipped || (int)strlen(val) >= HC_VAL - 1) {
            emit(HC_ERR, "   !! écriture refusée : ce gestionnaire a lu un "
                         "script tronqué ; l'écrire le détruirait");
            set_result("script tronqué");
            g_atop = sauve; return 1;
        }
        hc_set_script(o, val);
    } else if (ci_equal(prop, "style")) {
        free(o->style);
        o->style = dupstr(val);
    } else if (ci_equal(prop, "text") || ci_equal(prop, "contents")) {
        if (o->type != OBJ_FIELD && o->type != OBJ_BUTTON) {
            emit(HC_ERR, "   !! seul un champ ou un bouton a un contenu");
            g_atop = sauve; return 1;
        }
        hc_set_field_text(o, val);
        notify_field(o);
    } else if (ci_equal(prop, "partnumber")) {
        /* LE RANG SE POSE, il ne se constate pas seulement.
         *
         * Signalé à l'usage : « set the partNumber of button "pin pon" to 1 »
         * répondait « propriété inconnue : partNumber » — alors que la
         * lecture la sert depuis toujours. Le message envoyait donc chercher
         * du côté du nom, le seul endroit où il n'y avait rien ; c'est la
         * quatrième fois qu'on corrige cette forme-là.
         *
         * Le refus lui-même se justifiait, dans le commentaire de la lecture,
         * par « c'est le travail de send farther et de ses voisins ». Or ces
         * voisins n'existent nulle part dans le projet. Un commentaire qui
         * renvoie à une commande absente ne justifie rien : il déguise un
         * manque en choix.
         *
         * SEULES LES PARTS ONT UN RANG. Une carte, un fond, une pile n'en ont
         * pas, et le dire vaut mieux que d'écrire dans le vide : hc_part_number
         * leur rend déjà 0 en lecture. */
        if (o->type != OBJ_BUTTON && o->type != OBJ_FIELD) {
            emit(HC_ERR, "   !! seul un bouton ou un champ a un rang de part");
            /* « propriété inconnue » mentirait ici, et ce serait la faute
             * même qu'on répare : la propriété est connue, c'est la CIBLE qui
             * n'en a pas. Un script qui lit `the result` doit pouvoir faire
             * la différence. */
            set_result("pas une part");
            g_atop = sauve; return 1;
        }
        /* UN NOMBRE, OU RIEN. hc_entier écrête et refuse d'un même geste, ce
         * qui confondrait « 999 » — légitime, et qu'on écrête — avec
         * « patate », qui est une faute et doit se dire. hc_entier_lu les
         * sépare, et l'écrêtage se fait dans hc_set_part_number, où vit la
         * seule borne qui compte : le nombre de parts. */
        int lu = 0;
        int v = hc_entier_lu(val, -HC_COORD_MAX, HC_COORD_MAX, 0, &lu);
        if (!lu) {
            emit(HC_ERR, "   !! le rang d'une part est un nombre entier ; "
                         "reçu « %s »", val);
            set_result("rang de part invalide");
            g_atop = sauve; return 1;
        }
        hc_set_part_number(o, v);
        /* TOUTE LA COUCHE EST À REDESSINER, et pas seulement l'objet déplacé :
         * celui qui le recouvrait doit se redessiner lui aussi, sans quoi le
         * bouton passé au-dessus resterait caché à l'écran alors que le modèle
         * l'a déjà remonté. On prévient donc pour chaque part du propriétaire. */
        for (int i = 0; i < o->owner->nparts; i++) notify_field(o->owner->parts[i]);
    } else if (geom_write(o, prop, val)) {
        notify_field(o);
    } else {
        set_result("propriété inconnue");
        emit(HC_ERR, "   !! propriété inconnue : %s", prop);
        g_atop = sauve; return 1;
    }

    set_result("");
    emit(HC_INFO, "   → %s de %s ← \"%s\"", prop, d, val);
    g_atop = sauve;
    return 1;
}

/* Un appel de fonction n'est pas un conteneur, où qu'il se trouve dans la
 * référence : « item 1 to 7 of calData() » se lit mais ne s'écrit pas.
 * L'ancien test cherchait une parenthèse dans le texte de la ligne ; celui-ci
 * regarde l'arbre, qui sait de quoi il parle. */
static int v3_contient_appel(const HctNoeud *n)
{
    if (!n) return 0;
    if (n->genre == HCTN_APPEL) return 1;
    for (int i = 0; i < n->nfils; i++)
        if (v3_contient_appel(n->fils[i])) return 1;
    return 0;
}

/* convert <conteneur> [from <format>] to <format> [and <format>]
 *
 * Motif hct_cmd.c : « c [from *] to * ». L'arbre a déjà fait le découpage :
 * fils[0] est la source, et « from » comme « to » y sont des nœuds MOTCLE,
 * chacun suivi de son format. On le lit, au lieu de reconstituer le texte de
 * la ligne pour le recouper soi-même au dernier « to » de premier niveau.
 *
 * Ce recoupage à la main était la dernière raison de relire la source : la v3
 * l'avait déjà analysée, et eval_expr la relexait pour rien. Il était de plus
 * fragile — « convert item 1 to 7 of calData() to dateItems » contient deux
 * « to », et il fallait un paragraphe pour expliquer pourquoi on gardait le
 * dernier. L'arbre, lui, ne s'y trompe pas : le « to » de la commande est un
 * MOTCLE, celui du morceau ne l'est pas.
 *
 * C'est ICI qu'avait été trouvé, par test réel dans Xcode, le bug du « the »
 * avalé sans être rangé dans un nœud : « convert the date to dateItems » se
 * reconstituait « convert date to dateItems », et le choix entre écrire dans
 * `it` ou dans un conteneur se trompait de branche — « date » ressemblait à
 * une variable, une variable de ce nom naissait, et `it` ne recevait rien. Le
 * drapeau `article`, posé depuis par l'analyseur, tranche maintenant sans
 * regarder le texte.
 *
 * « from <format> » reste ignoré, comme dans l'ancien exécuteur : un format
 * de date se reconnaît à la lecture, on n'a pas besoin qu'on l'annonce. */
static int v3_cmd_convert(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return 0;

    /* Le « to » de la commande : le dernier MOTCLE « to » de premier niveau. */
    const HctNoeud *nfmt = NULL;
    for (int i = 1; i + 1 < n->nfils; i++)
        if (n->fils[i]->genre == HCTN_MOTCLE && n->fils[i]->op &&
            ci_equal(n->fils[i]->op, "to"))
            nfmt = n->fils[i + 1];
    if (!nfmt) {
        emit(HC_ERR, "   !! convert sans « to »");
        set_result("invalid date");
        return 1;
    }

    /* Le format est une suite de mots-clés — « dateItems », « short date »,
     * « long date and long time » —, pas une expression : l'évaluer rendrait
     * la valeur d'une variable qui porterait ce nom. On lit donc le source du
     * nœud, brut. */
    int f1 = DF_NONE, f2 = DF_NONE;
    char m1[64], m2[64];
    if (nfmt->genre == HCTN_BINAIRE && nfmt->op && ci_equal(nfmt->op, "and") &&
        nfmt->nfils == 2) {
        v3_brut(nfmt->fils[0], m1, sizeof m1);
        v3_brut(nfmt->fils[1], m2, sizeof m2);
        f2 = date_format_code(m2);
    } else {
        v3_brut(nfmt, m1, sizeof m1);
    }
    f1 = date_format_code(m1);
    if (f1 == DF_NONE) {
        emit(HC_ERR, "   !! format de date inconnu : %s", m1);
        set_result("invalid date");
        return 1;
    }

    size_t sauve = g_atop;
    char *val = arena_buf();
    v3_val_texte(ctx, n->fils[0], val, HC_VAL);
    if (ctx->erreur) { g_atop = sauve; return 1; }

    struct tm tm;
    if (!parse_datetime(val, &tm)) {
        emit(HC_ERR, "   !! date incomprise : « %s »", val);
        set_result("invalid date");
        g_atop = sauve; return 1;
    }

    char *outv = arena_buf();
    char part2[256];
    emit_datetime(&tm, f1, outv, HC_VAL);
    if (f2 != DF_NONE) {
        emit_datetime(&tm, f2, part2, sizeof part2);
        int L = (int)strlen(outv);
        snprintf(outv + L, HC_VAL - L, " %s", part2);
    }

    /* Destination : la source, quand c'en est un conteneur. Une variable nue
     * s'écrit directement — plus besoin de refabriquer son nom en texte pour
     * que container_set le réanalyse. */
    const HctNoeud *src = n->fils[0];
    if (src->article || v3_contient_appel(src)) {
        var_set("it", outv);
    } else if (src->genre == HCTN_IDENT) {
        char nom[128];
        v3_brut(src, nom, sizeof nom);
        var_set(nom, outv);
    } else {
        char ref[512];
        v3_source(src, ref, sizeof ref);
        if (!ref[0] || !container_set(ref, outv, 0)) var_set("it", outv);
    }

    set_result("");
    emit(HC_INFO, "   → %s", outv);
    g_atop = sauve;
    return 1;
}

/* print [this] card|stack|all|marked [<n>] [to <n>] [with <rapport>]
 *
 * Motif hct_cmd.c : « * [with e] », sans borne — comme pour find/sort/
 * visual, v3_reste rend le texte EXACT que lisait l'ancien exécuteur.
 * « with <rapport> » en fait partie mais reste ignoré : l'ancien exécuteur
 * ne le lisait déjà pas, une mise en page de rapport demandant un
 * imprimeur que le noyau n'a jamais eu. */
static int v3_cmd_print(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
    size_t sauve = g_atop;
    char *mots = arena_buf();
    v3_reste(n, mots, HC_VAL);

    const char *a = skip_spaces(mots);
    if (ci_word(a, "this")) a = skip_spaces(a + 4);

    Object *pile = g_current_card ? g_current_card->owner : NULL;
    while (pile && pile->type != OBJ_STACK) pile = pile->owner;

    /* La liste est ALLOUÉE à la taille de la pile, et non fixée à 512.
     *
     * Le tableau fixe tronquait en silence : « print all cards » sur une pile
     * de sept cents cartes en imprimait cinq cent douze, sans un mot. Une
     * pile n'a jamais plus de cartes qu'elle n'a de parties, donc une
     * allocation à cette taille suffit toujours et ne peut pas déborder. */
    int cap = pile ? pile->nparts + 1 : 1;
    Object **liste = calloc((size_t)cap, sizeof *liste);
    if (!liste) {
        set_result("mémoire insuffisante");
        emit(HC_ERR, "   !! print : mémoire insuffisante");
        g_atop = sauve; return 1;
    }
    int np = 0;

    if (ci_word(a, "stack") || ci_word(a, "all")) {
        if (pile)
            for (int i = 0; i < pile->nparts && np < cap; i++)
                if (pile->parts[i]->type == OBJ_CARD) liste[np++] = pile->parts[i];
    }
    else if (ci_word(a, "marked")) {
        if (pile)
            for (int i = 0; i < pile->nparts && np < cap; i++)
                if (pile->parts[i]->type == OBJ_CARD && pile->parts[i]->marked)
                    liste[np++] = pile->parts[i];
    }
    else if (ci_word(a, "card") || ci_word(a, "cd") || !*a) {
        const char *r = *a ? skip_spaces(a + (ci_word(a, "cd") ? 2 : 4)) : "";
        if (!*r) {
            /* « print card » nu : la carte courante. */
            if (g_current_card) liste[np++] = g_current_card;
        } else {
            /* « print card 3 », « print card "index" », « print card 2 to 7 » */
            const char *to = find_kw(r, "to");
            if (to) {
                char *v1 = arena_buf(), *v2 = arena_buf();
                int len = (int)(to - r);
                char brut[256];
                if (len > (int)sizeof brut - 1) len = (int)sizeof brut - 1;
                memcpy(brut, r, (size_t)len); brut[len] = '\0';
                eval_checked(brut, v1, HC_VAL);
                eval_checked(skip_spaces(to + 2), v2, HC_VAL);
                /* LES BORNES SONT RAMENÉES À LA PILE avant de boucler.
                 *
                 * « print card 1 to 2147483647 » sur une pile de dix cartes
                 * GELAIT l'application : passé la dixième, nth_card rendait
                 * NULL, np cessait d'augmenter, la garde « np < 512 » restait
                 * donc vraie, et la boucle parcourait deux milliards d'indices
                 * pour rien. Au bout, « i++ » sur INT_MAX est en plus un
                 * débordement signé, soit un comportement indéfini.
                 *
                 * Une faute de frappe suffisait. On borne donc à ce qui
                 * existe, et le compte de cartes est la seule borne juste. */
                int hors1 = 0, hors2 = 0;
                int d = hct_vers_rang(v1, &hors1);
                int f = hct_vers_rang(v2, &hors2);
                int total = card_count(pile);
                if (hors1) d = 1;
                if (hors2) f = total;
                if (d < 1) d = 1;
                if (f > total) f = total;
                for (int i = d; i <= f && np < cap; i++) {
                    Object *c = nth_card(pile, i - 1);
                    if (c) liste[np++] = c;
                }
            } else {
                Object *c = resolve(r);
                if (!c) {
                    char *v = arena_buf();
                    eval_checked(r, v, HC_VAL);
                    c = resolve(v);
                    if (!c) {
                        char ref[128];
                        snprintf(ref, sizeof ref, "card %s", v);
                        c = resolve(ref);
                    }
                }
                if (c && c->type == OBJ_CARD) liste[np++] = c;
            }
        }
    }

    if (np == 0) {
        set_result("No cards to print");
        emit(HC_ERR, "   !! print : rien à imprimer");
        free(liste); g_atop = sauve; return 1;
    }
    if (g_host && g_host->print_cards) {
        g_host->print_cards(liste, np);
        set_result("");
    } else {
        set_result("Can't print");
        emit(HC_ERR, "   !! print : l'hôte ne sait pas imprimer");
    }
    free(liste);
    g_atop = sauve;
    return 1;
}

static FILE *file_find(const char *nom);          /* défini plus bas */
static int   file_constant(const char *s);         /* défini plus bas */

/* Plafond d'une lecture de fichier.
 *
 * Ce n'est plus une taille de tampon — la lecture s'agrandit à la demande —
 * mais un garde-fou : « read from file "/dev/zero" » n'a pas de fin, et sans
 * plafond il mangerait la mémoire de la machine jusqu'à l'arrêt du
 * programme. Soixante-quatre mégaoctets sont au-delà de tout fichier qu'une
 * pile lit raisonnablement d'un seul coup, et très loin des 65 535 octets
 * d'avant. */
#define HC_LECTURE_MAX (64u * 1024u * 1024u)

/* Le tampon de lecture s'agrandit par doublement. hc_memoire_epuisee est la
 * sortie unique sur échec d'allocation : on ne rend pas une lecture tronquée
 * en la faisant passer pour complète. */
static void lecture_pousse(char **buf, size_t *cap, size_t *n, int c)
{
    if (*n + 1 >= *cap) {
        size_t nc = *cap ? *cap * 2 : 1024;
        char *p = realloc(*buf, nc);
        if (!p) hc_memoire_epuisee("lecture d'un fichier");
        *buf = p; *cap = nc;
    }
    (*buf)[(*n)++] = (char)c;
}

/* « at <pos> » : se placer dans le fichier, pour « read » comme pour « write ».
 *
 * atol lisait ce texte sans jamais demander si c'en était un. « read from
 * file f at canard for 3 » se plaçait donc au début et rendait trois octets
 * pris ailleurs que là où le script croyait, sans un mot. Et fseek peut
 * refuser — un tube, un terminal, une position absurde — sans que personne
 * regardât son verdict : la lecture suivante portait alors sur la position
 * courante, pas sur celle demandée.
 *
 * Rend 0 après avoir posé « the result » si la position est refusée. */
static int v3_fichier_place(FILE *f, const char *quoi, const char *pv)
{
    if (!hct_est_nombre(pv)) {
        set_result("Bad file position");
        emit(HC_ERR, "   !! %s : position illisible : %s", quoi, pv);
        return 0;
    }
    double d = hct_vers_nombre(pv);
    /* Comparaisons STRICTES : (double)LONG_MAX vaut 2^63 sur une machine 64
     * bits, soit LONG_MAX + 1 — accepter l'égalité rendrait la conversion en
     * long indéfinie, exactement le défaut qu'on ferme ici. */
    if (!(d > -(double)LONG_MAX && d < (double)LONG_MAX)) {
        set_result("Bad file position");
        emit(HC_ERR, "   !! %s : position hors bornes : %s", quoi, pv);
        return 0;
    }
    long lpos = (long)d;
    /* Positif : depuis le début, et 1-based comme tout HyperTalk.
     * Négatif : depuis la fin. */
    int rc = (lpos >= 0) ? fseek(f, lpos > 0 ? lpos - 1 : 0, SEEK_SET)
                         : fseek(f, lpos, SEEK_END);
    if (rc != 0) {
        set_result("Bad file position");
        emit(HC_ERR, "   !! %s : position refusée : %s", quoi, pv);
        return 0;
    }
    return 1;
}

/* read from file <nom> [at <pos>] [for <n> | until <car>]
 *
 * Motif hct_cmd.c : « from file e [at e] [for|until e] ». Le « at » y a été
 * AJOUTÉ : il manquait, si bien que « read … at 8 for 6 » échouait à
 * l'analyse et faisait retomber le GESTIONNAIRE ENTIER sur l'ancien
 * exécuteur — qui savait la lire, mais emportait avec lui les lignes saines
 * d'à côté. La branche `at` ci-dessous existait déjà, prête et jamais
 * empruntée.
 *
 * Les arguments se lisent dans l'arbre, chacun repéré par son mot-clé. La
 * version précédente reconstituait le texte de la ligne et le recoupait sur
 * « at », « for » et « until » avant d'évaluer chaque morceau — quatre
 * relectures possibles par commande, et une heuristique de plus à tenir.
 *
 * LE TEXTE LU NE PASSE PLUS PAR UN TAMPON FIXE. Il tenait dans HC_VAL, et
 * toute lecture plus longue était coupée là. Pire : le contrôle de troncature
 * comparait la longueur lue au bord du tampon, si bien qu'un fichier de
 * très exactement 65 535 octets — lu en entier, sans rien perdre — se voyait
 * annoncer « Value too large ». Le script prenait une lecture complète pour
 * un échec. */
static int v3_cmd_read(HctContexte *ctx, const HctNoeud *n)
{
    const HctNoeud *nfic = v3_apres_motcle(n, "file");
    if (!nfic) {
        emit(HC_ERR, "   !! read : « file » attendu");
        return 1;
    }

    size_t sauve = g_atop;
    char *nom = arena_buf();
    v3_val_texte(ctx, nfic, nom, HC_VAL);
    if (ctx->erreur) { g_atop = sauve; return 1; }

    FILE *f = file_find(nom);
    if (!f) {
        set_result("File is not open");
        emit(HC_ERR, "   !! read : fichier non ouvert : %s", nom);
        g_atop = sauve; return 1;
    }

    const HctNoeud *nat  = v3_apres_motcle(n, "at");
    const HctNoeud *nfor = v3_apres_motcle(n, "for");
    const HctNoeud *nunt = v3_apres_motcle(n, "until");

    if (nat) {
        char pv[64];
        v3_val_texte(ctx, nat, pv, sizeof pv);
        if (ctx->erreur) { g_atop = sauve; return 1; }
        if (!v3_fichier_place(f, "read", pv)) { g_atop = sauve; return 1; }
    }

    char  *out = NULL;
    size_t cap = 0, no = 0;
    int    coupe = 0;          /* arrêté par le plafond, pas par le fichier */

    if (nfor) {
        char cv[64];
        v3_val_texte(ctx, nfor, cv, sizeof cv);
        if (ctx->erreur) { g_atop = sauve; return 1; }
        /* « for » sans nombre valait zéro octet et « End of file » : un
         * silence, là où le script a manifestement écrit une bêtise. */
        if (!hct_est_nombre(cv)) {
            set_result("Bad parameter");
            emit(HC_ERR, "   !! read : « for » attend un nombre, pas : %s", cv);
            free(out); g_atop = sauve; return 1;
        }
        double dc = hct_vers_nombre(cv);
        if (dc < 0) dc = 0;
        size_t combien = (dc > (double)HC_LECTURE_MAX)
                       ? (size_t)HC_LECTURE_MAX : (size_t)dc;
        while (no < combien) {
            int c = fgetc(f);
            if (c == EOF) break;
            lecture_pousse(&out, &cap, &no, c);
        }
        if (no == (size_t)HC_LECTURE_MAX && dc > (double)HC_LECTURE_MAX) coupe = 1;
    } else if (nunt) {
        /* « until return », « until tab » : des noms de caractères, pas des
         * expressions — les évaluer rendrait la valeur d'une variable qui
         * porterait ce nom. On lit donc le source d'abord, et on n'évalue
         * que si ce n'en est pas un. */
        char mot[64];
        v3_brut(nunt, mot, sizeof mot);
        int stop = file_constant(mot);
        if (stop == -1) {
            char cv[64];
            v3_val_texte(ctx, nunt, cv, sizeof cv);
            if (ctx->erreur) { g_atop = sauve; return 1; }
            stop = cv[0] ? (unsigned char)cv[0] : '\n';
        }
        for (;;) {
            int c;
            if (no >= (size_t)HC_LECTURE_MAX) { coupe = 1; break; }
            c = fgetc(f);
            if (c == EOF) break;
            lecture_pousse(&out, &cap, &no, c);
            /* Le caractère d'arrêt fait PARTIE du texte lu : c'est ce que
             * fait HyperCard, et ce qui permet d'enchaîner les lectures
             * ligne à ligne sans perdre les séparateurs. */
            if (stop >= 0 && c == stop) break;
        }
    } else {
        /* Ni « for » ni « until » : tout le fichier. */
        for (;;) {
            int c;
            if (no >= (size_t)HC_LECTURE_MAX) { coupe = 1; break; }
            c = fgetc(f);
            if (c == EOF) break;
            lecture_pousse(&out, &cap, &no, c);
        }
    }
    /* Réserver l'octet nul : lecture_pousse garde toujours la place, mais
     * une lecture vide n'a rien alloué du tout. */
    if (!out) {
        out = malloc(1);
        if (!out) hc_memoire_epuisee("lecture d'un fichier");
    }
    out[no] = '\0';

    /* Une ERREUR de lecture ne se distingue pas d'une fin de fichier par le
     * seul EOF de fgetc : les deux rendent la même valeur. Sans ferror, un
     * fichier illisible à mi-parcours rendait « End of file » ou même la
     * chaîne vide comme succès, et le script prenait un fichier tronqué pour
     * un fichier complet. */
    if (ferror(f)) {
        set_result("Read error");
        emit(HC_ERR, "   !! read : erreur de lecture après %zu octets", no);
    } else if (coupe) {
        /* Dire la troncature plutôt que de couper en silence : un script qui
         * lit un fichier trop gros doit pouvoir s'en apercevoir, et découper
         * sa lecture en plusieurs « read ... for N ». */
        set_result("Value too large");
        emit(HC_ERR, "   !! read : texte tronqué à %zu octets", no);
    } else {
        set_result(no > 0 ? "" : "End of file");
    }
    var_set("it", out);
    free(out);
    g_atop = sauve;
    return 1;
}

/* write <texte> to file <nom> [at <pos>|end|eof]
 *
 * Motif hct_cmd.c : « e to file e [at e] ». Même ajout du « at » que pour
 * read, et pour la même raison : sans lui, « write … at end » — la forme
 * qu'emploie tout script qui ajoute à la fin d'un journal — condamnait le
 * gestionnaire entier à l'ancien chemin. */
static int v3_cmd_write(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return 0;
    const HctNoeud *nfic = v3_apres_motcle(n, "file");
    if (!nfic) {
        emit(HC_ERR, "   !! write : « to file » attendu");
        return 1;
    }

    size_t sauve = g_atop;
    /* LA VALEUR À ÉCRIRE NE PASSE PLUS PAR UN TAMPON FIXE.
     *
     * On la recopiait dans HC_VAL avant d'écrire : « write field "gros" to
     * file "donnees" » avec 200 Ko de texte en écrivait 65 535, et le
     * contrôle d'erreur d'entrée-sortie ajouté la semaine dernière annonçait
     * un SUCCÈS — puisque l'écriture, elle, s'était parfaitement passée. La
     * perte avait eu lieu avant de toucher le disque.
     *
     * HctValeur alloue son texte à sa taille, et porte sa longueur : on écrit
     * exactement ce qu'on a, y compris d'éventuels octets nuls. */
    HctValeur vtxt = hct_evalue(ctx, n->fils[0]);
    if (ctx->erreur) { hct_val_libere(&vtxt); g_atop = sauve; return 1; }
    const char *txt = vtxt.txt ? vtxt.txt : "";

    char *nom = arena_buf();
    v3_val_texte(ctx, nfic, nom, HC_VAL);
    if (ctx->erreur) { hct_val_libere(&vtxt); g_atop = sauve; return 1; }

    FILE *f = file_find(nom);
    if (!f) {
        set_result("File is not open");
        emit(HC_ERR, "   !! write : fichier non ouvert : %s", nom);
        hct_val_libere(&vtxt); g_atop = sauve; return 1;
    }

    const HctNoeud *nat = v3_apres_motcle(n, "at");
    if (nat) {
        /* « at end » et « at eof » sont des mots, pas des expressions. */
        char mot[32];
        v3_brut(nat, mot, sizeof mot);
        if (ci_equal(mot, "end") || ci_equal(mot, "eof")) fseek(f, 0, SEEK_END);
        else {
            char pv[64];
            v3_val_texte(ctx, nat, pv, sizeof pv);
            if (ctx->erreur) { hct_val_libere(&vtxt); g_atop = sauve; return 1; }
            /* Une position refusée n'écrit PAS ailleurs : elle renonce.
             * Écrire à la position courante parce que le déplacement a
             * échoué, c'est écraser un endroit du fichier au hasard. */
            if (!v3_fichier_place(f, "write", pv)) {
                hct_val_libere(&vtxt); g_atop = sauve; return 1;
            }
        }
    }

    /* L'ÉCRITURE PEUT ÉCHOUER, ET IL FAUT LE DIRE.
     *
     * fwrite rend le nombre d'éléments écrits et fflush rend EOF sur erreur —
     * ni l'un ni l'autre n'était regardé. Disque plein, quota dépassé, erreur
     * d'entrée-sortie : le fichier était tronqué et « the result » restait
     * vide, donc le script croyait son journal ou ses données en sécurité.
     *
     * C'est la même exigence que pour l'enregistrement d'une pile, qui la
     * respecte depuis longtemps ; « write to file » avait été oublié. */
    size_t lt = (size_t)vtxt.len;
    size_t ecrit = fwrite(txt, 1, lt, f);
    int rince = fflush(f);   /* pour qu'un autre programme voie le texte tout de suite */

    if (ecrit != lt) {
        set_result("Disk full or write error");
        emit(HC_ERR, "   !! write : écriture incomplète, %zu octets sur %zu",
             ecrit, lt);
    } else if (rince != 0 || ferror(f)) {
        /* fwrite a tout pris — dans son TAMPON. C'est le vidage qui a buté
         * sur le disque. Dire « incomplète, 37 sur 37 » se contredirait ;
         * le nombre d'octets réellement parvenus au fichier n'est pas
         * connaissable ici, et le dire est plus honnête que l'inventer. */
        set_result("Disk full or write error");
        emit(HC_ERR, "   !! write : le disque a refusé l'écriture "
                     "(les %zu octets ne sont pas tous arrivés)", lt);
    } else {
        set_result("");
    }
    hct_val_libere(&vtxt);
    g_atop = sauve;
    return 1;
}

/* ------------------------------------------------------------- les verbes */

/* answer "invite" [with "a" [or "b" [or "c"]]]
 *
 * Motif hct_cmd.c : « b [with b [or b [or b]]] ». Les fils, dans l'ordre :
 * l'invite, puis — si « with » est là — un HCTN_MOTCLE "with", un bouton,
 * et chaque bouton suivant précédé de son propre HCTN_MOTCLE "or". Aucune
 * chaîne à redécouper sur « with »/« or » hors des guillemets, comme le
 * faisait l'ancien exécuteur : l'analyseur a déjà fait ce travail, et n'a
 * pas cette faiblesse-là. */
static int v3_cmd_reponse(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return 0;

    /* Les tampons vont dans l'ARÈNE, pas sur la pile : une invite peut citer
     * le contenu d'un champ entier, et HC_VAL (un mégaoctet) ne tiendrait
     * pas dans une fonction que le répartiteur de commandes appelle pour
     * chaque ligne. Même raison que dans v3_recours. */
    ARENA_MARK;
    char *prompt = arena_buf();
    v3_val_texte(ctx, n->fils[0], prompt, HC_VAL);
    if (ctx->erreur) { ARENA_FREE; return 1; }

    char (*btn)[HC_VAL] = arena_rows(3);
    if (!btn) {
        set_result("mémoire insuffisante");
        ARENA_FREE;
        return 1;
    }
    int nb = 0;
    if (v3_est_motcle(n, 1, "with")) {
        int i = 2;
        while (nb < 3 && i < n->nfils) {
            v3_val_texte(ctx, n->fils[i], btn[nb], HC_VAL);
            if (ctx->erreur) { ARENA_FREE; return 1; }
            nb++; i++;
            if (i < n->nfils && v3_est_motcle(n, i, "or")) i++;
            else break;
        }
    }
    if (nb == 0) { snprintf(btn[0], HC_VAL, "OK"); nb = 1; }

    const char *rep = (g_host && g_host->answer)
        ? g_host->answer(prompt, btn[0], nb > 1 ? btn[1] : NULL,
                                          nb > 2 ? btn[2] : NULL)
        : btn[0];
    var_set("it", rep ? rep : "");
    set_result("");
    ARENA_FREE;
    return 1;
}

/* answer file "invite" [of type t] : panneau d'ouverture, pas une boîte à
 * boutons. « of type … » ne sert déjà à rien dans l'ancien exécuteur — il
 * n'en tirait que la coupure de l'invite — et le nœud n'en garde de toute
 * façon que les mots-clés, pas un fils à lire. */
static int v3_cmd_reponse_fichier(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return 0;
    ARENA_MARK;
    char *inv = arena_buf();
    v3_val_texte(ctx, n->fils[0], inv, HC_VAL);
    if (ctx->erreur) { ARENA_FREE; return 1; }

    const char *chemin = (g_host && g_host->answer_file)
                        ? g_host->answer_file(inv) : NULL;
    var_set("it", chemin ? chemin : "");
    set_result(chemin ? "" : "Cancel");
    ARENA_FREE;
    return 1;
}

/* ask "invite" [with "défaut"] */
static int v3_cmd_demande(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return 0;
    ARENA_MARK;
    char *prompt = arena_buf();
    v3_val_texte(ctx, n->fils[0], prompt, HC_VAL);
    if (ctx->erreur) { ARENA_FREE; return 1; }

    char *deflt = arena_buf();
    if (v3_est_motcle(n, 1, "with") && n->nfils >= 3) {
        v3_val_texte(ctx, n->fils[2], deflt, HC_VAL);
        if (ctx->erreur) { ARENA_FREE; return 1; }
    }

    const char *rep = (g_host && g_host->ask) ? g_host->ask(prompt, deflt) : NULL;
    if (rep) { var_set("it", rep); set_result(""); }
    else     { var_set("it", "");  set_result("Cancel"); }
    ARENA_FREE;
    return 1;
}

/* ask file "invite" [with "nom par défaut"] : le pendant en écriture
 * d'« answer file ». Le chemin choisi va dans « it », vide si l'on annule. */
static int v3_cmd_demande_fichier(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return 0;
    ARENA_MARK;
    char *inv = arena_buf();
    v3_val_texte(ctx, n->fils[0], inv, HC_VAL);
    if (ctx->erreur) { ARENA_FREE; return 1; }

    char *def = arena_buf();
    if (v3_est_motcle(n, 1, "with") && n->nfils >= 3) {
        v3_val_texte(ctx, n->fils[2], def, HC_VAL);
        if (ctx->erreur) { ARENA_FREE; return 1; }
    }

    const char *chemin = (g_host && g_host->ask_file)
                        ? g_host->ask_file(inv, def) : NULL;
    var_set("it", chemin ? chemin : "");
    set_result(chemin ? "" : "Cancel");
    ARENA_FREE;
    return 1;
}

/* beep : l'ancien exécuteur ignore le nombre de bips et se contente de la
 * ligne. On garde ce comportement tel quel — la migration ne doit rien
 * changer d'observable, sinon on ne saura plus si une régression vient du
 * portage ou d'une correction glissée dedans. */
static int v3_cmd_beep(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx; (void)n;
    emit(HC_INFO, "   ♪ beep");
    return 1;
}

static int v3_cmd_debug(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
    char quoi[32];
    quoi[0] = '\0';
    if (n->nfils >= 1) v3_brut(n->fils[0], quoi, sizeof quoi);
    if (ci_equal(quoi, "raz")) { hc_v3_bilan_remise_a_zero(); return 1; }
    hc_v3_bilan();
    return 1;
}

/* lock/unlock screen et lock/unlock messages. « lock recent » repart à
 * l'ancien chemin. */
static int v3_cmd_verrou(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
    char mot[32];
    if (n->nfils < 1) return 0;
    v3_brut(n->fils[0], mot, sizeof mot);

    if (ci_equal(mot, "messages")) {
        g_messages_verrouilles = ci_equal(n->op, "lock");
        set_result("");
        return 1;
    }

    if (!ci_equal(mot, "screen")) return 0;

    g_ecran_verrouille = ci_equal(n->op, "lock");
    host_global_set("lockScreen", g_ecran_verrouille ? "true" : "false");
    /* Au déverrouillage, on réveille les champs modifiés pendant le verrou —
     * eux seulement, pas l'écran entier. */
    if (!g_ecran_verrouille) verrou_reveille();
    set_result("");
    return 1;
}

static int v3_cmd_montre(HctContexte *ctx, const HctNoeud *n)
{
    int montrer = ci_equal(n->op, "show");
    if (n->nfils < 1) return 0;

    /* « show all cards », « hide menuBar » : pas des objets. hct_resout rend
     * NULL et l'ancien chemin s'en charge, avec son message d'erreur. */
    Object *o = hct_resout(ctx, n->fils[0]);
    if (!o) return 0;

    g_visual_dirty = 1;
    o->visible = montrer;

    char d[64];
    hc_describe(o, d, sizeof d);

    int iat = montrer ? v3_indice_motcle(n, "at", 1) : -1;
    if (iat >= 0) {
        char pt[128];
        v3_point(ctx, n, iat + 1, n->nfils, pt, sizeof pt);
        if (ctx->erreur) return 1;
        if (!geom_write(o, "loc", pt)) {
            emit(HC_ERR, "   !! show … at : point mal formé : %s", pt);
            return 1;
        }
        notify_field(o);
        set_result("");
        emit(HC_INFO, "   → %s : visible en %s", d, pt);
        return 1;
    }

    notify_field(o);
    set_result("");
    emit(HC_INFO, "   → %s : %s", d, montrer ? "visible" : "caché");
    return 1;
}

/* delete <cible> : supprime un morceau de conteneur — mot, ligne, item,
 * caractère —, ou vide un champ ou une variable entière. container_set sait
 * déjà tout cela (mode 3), à condition de recevoir le TEXTE de la référence :
 * hct_cmd.c l'analyse comme une expression ordinaire (« e »), sans nœud dédié
 * pour un morceau. v3_reste (voir sa définition) la reconstitue donc, comme
 * le fait v3_recours pour une expression qu'elle ne sait pas évaluer
 * elle-même — sur le nœud ENTIER, pas sur n->fils[0] seul : un ordinal comme
 * « last » dans « delete the last word of X » n'est le jeton d'AUCUN nœud —
 * juste un ordinal numérique posé sur le nœud CHUNK, dont le jeton propre
 * commence à « word ». Reconstruire depuis n->fils[0] seul, avec v3_source,
 * rendait donc « word of X », sans « the last » : container_set n'y voyait
 * plus de rang du tout.
 *
 * Une faute d'analyse dans la cible — n->fils[0] devenu lui-même une
 * HCTN_ERREUR — rendrait un texte tronqué : container_set effacerait alors
 * autre chose que ce que le script demande, en silence. On rend 0 plutôt que
 * de risquer ça ; l'ancien chemin sait dire pourquoi la ligne est fautive.
 *
 * container_set ne connaît que les morceaux, la boîte de messages, les
 * champs et les variables — jamais les boutons, cartes ou menus : « delete
 * button 1 » lui échappe déjà, et continuera de repartir à l'ancien chemin,
 * comme avant ce portage. */
static int v3_cmd_delete(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1 || n->fils[0]->genre == HCTN_ERREUR) return 0;

    /* « delete menu "X" » et « delete menuItem 4 of menu "X" ». Avant le
     * chemin des conteneurs : un menu n'en est pas un, et container_set
     * répondrait « rien à supprimer ». */
    if (n->fils[0]->genre == HCTN_OBJET) {
        const HctNoeud *o = n->fils[0];
        if (o->typeobj == HCT_OBJ_MENU) {
            int i = v3_menu_index(ctx, o);
            if (ctx->erreur) return 1;
            if (i < 0) { set_result("menu introuvable"); return 1; }
            char nom[64];
            snprintf(nom, sizeof nom, "%s", g_menus[i].nom);
            menu_supprimer(nom);
            set_result("");
            return 1;
        }
        if (o->typeobj == HCT_OBJ_MENUITEM) {
            int im = -1;
            int j = v3_article_index(ctx, o, &im);
            if (ctx->erreur) return 1;
            if (j < 0) { set_result("article introuvable"); return 1; }
            HcMenuBarre *m = &g_menus[im];
            free(m->article[j]);
            free(m->message[j]);
            for (int k = j; k < m->n - 1; k++) {
                m->article[k] = m->article[k + 1];
                m->message[k] = m->message[k + 1];
                m->actif[k]   = m->actif[k + 1];
            }
            m->n--;
            m->article[m->n] = NULL;
            m->message[m->n] = NULL;
            menus_prevenir();
            set_result("");
            return 1;
        }

        /* Les objets du modèle C se suppriment comme objets, pas comme
         * conteneurs. C'était le trou qui avalait « delete this card » :
         * container_set répondait non, puis v3_cmd_delete rendait quand même
         * 1, donc hc_delete_card n'était jamais appelée. */
        Object *obj = hct_resout(ctx, o);
        if (ctx->erreur) return 1;
        if (obj && obj->type == OBJ_CARD) {
            /* On vide « the result » AVANT : hc_delete_card y pose sa propre
             * raison quand elle en a une — « Can't delete background » pour le
             * verrou du fond —, et l'écraser par un message plus vague ferait
             * chercher le verrou au mauvais endroit. Le message générique ne
             * sert que quand elle n'a rien dit (dernière carte, suppression
             * déjà en cours). */
            set_result("");
            if (hc_delete_card(obj)) return 1;
            if (!g_result[0]) set_result("Can't delete card");
            return 1;
        }
        if (obj && (obj->type == OBJ_BUTTON || obj->type == OBJ_FIELD)) {
            if (hc_delete_part(obj)) set_result("");
            else set_result("Can't delete object");
            return 1;
        }
        if (obj) return 0;              /* objet connu mais non supprimable ici */
    }


    char d[256];
    v3_reste(n, d, sizeof d);
    if (!d[0]) return 0;

    if (container_set(d, "", 3)) {
        set_result("");
        emit(HC_INFO, "   → supprimé : %s", d);
    } else {
        set_result("rien à supprimer");
        emit(HC_ERR, "   !! rien à supprimer : %s", d);
    }
    return 1;
}

/* play : HyperCard accepte une suite de notes derrière le nom du son
 * (« play "boing" tempo 200 c4 e4 »). Comme l'ancien exécuteur, on ne retient
 * que le nom : le reste demande un synthétiseur, pas un lecteur. */
static int v3_cmd_play(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return 0;
    char nom[256];
    const HctNoeud *f = n->fils[0];
    if (f->genre == HCTN_IDENT) v3_brut(f, nom, sizeof nom);
    else {
        v3_val_texte(ctx, f, nom, sizeof nom);
        if (ctx->erreur) return 1;
    }
    if (g_host && g_host->play_sound) g_host->play_sound(nom);
    set_result("");
    emit(HC_INFO, "   ♪ play \"%s\"", nom);
    return 1;
}

static int v3_cmd_push(HctContexte *ctx, const HctNoeud *n)
{
    Object *dst = g_current_card;
    if (n->nfils >= 1) {
        Object *o = hct_resout(ctx, n->fils[0]);
        if (!o) return 0;
        dst = o;
    }
    if (!dst || dst->type != OBJ_CARD) dst = g_current_card;
    if (!dst) { set_result("aucune carte a empiler"); return 1; }

    if (g_navtop >= NAVSTACK_MAX) {          /* pile pleine : on decale */
        for (int i = 1; i < NAVSTACK_MAX; i++) g_navstack[i-1] = g_navstack[i];
        g_navtop = NAVSTACK_MAX - 1;
    }
    g_navstack[g_navtop++] = dst;
    set_result("");
    emit(HC_INFO, "   ⇒ empile la carte \"%s\"", dst->name ? dst->name : "?");
    return 1;
}

/* pop [card] [into conteneur].
 *
 * La forme « into » garde l'ancien chemin : écrire dans un conteneur
 * quelconque — variable, champ, morceau — est le travail de `put`, et le
 * refaire ici en dupliquerait la mécanique. Elle reviendra quand `put` sera
 * un service partagé plutôt qu'une ligne fabriquée pour l'ancien exécuteur. */
static int v3_cmd_pop(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
    if (v3_indice_motcle(n, "into", 0) >= 0) return 0;
    if (g_navtop <= 0) { set_result("pile de navigation vide"); return 1; }

    Object *dst = g_navstack[--g_navtop];
    Object *old = g_current_card;
    Object *oldbg = old ? old->bg : NULL;
    Object *newbg = dst->bg;

    if (old) hc_send_systeme(old, "closeCard");
    if (oldbg && oldbg != newbg) hc_send_systeme(oldbg, "closeBackground");
    g_current_card = dst;
    if (newbg && newbg != oldbg) hc_send_systeme(newbg, "openBackground");
    hc_send_systeme(dst, "openCard");
    set_result("");
    emit(HC_INFO, "   ⇒ depile vers \"%s\"", dst->name ? dst->name : "?");
    return 1;
}

/* reset paint : remet les propriétés de dessin à leur valeur par défaut. */
static int v3_cmd_reset(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
    char quoi[32];
    quoi[0] = '\0';
    if (n->nfils >= 1) v3_brut(n->fils[0], quoi, sizeof quoi);

    /* « reset menuBar » : la barre revient à celle de l'application, tous
     * les menus créés par script disparaissant d'un coup. HyperCard s'en sert
     * pour faire le ménage quand on ne sait plus ce qui traîne. */
    if (ci_equal(quoi, "menubar")) {
        menus_reset();
        set_result("");
        return 1;
    }

    if (ci_equal(quoi, "paint")) {
        host_global_set("filled",    "false");
        host_global_set("pattern",   "2");   /* le noir de HyperCard */
        host_global_set("lineSize",  "1");
        host_global_set("brush",     "8");
        host_global_set("textFont",  "geneva");
        host_global_set("textSize",  "12");
        host_global_set("textStyle", "plain");
        host_global_set("textAlign", "left");
        host_global_set("textHeight","16");
        set_result("");
        return 1;
    }
    emit(HC_ERR, "   !! reset : seul « reset paint » est reconnu");
    set_result("");
    return 1;
}

/* doMenu : HyperCard scriptait par les menus tout ce qui n'avait pas de
 * commande propre. On route vers l'hôte, seul à connaître ses menus. */
static int v3_cmd_domenu(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils != 1) return 0;      /* « doMenu X without dialog » : ancien */

    char item[256];
    v3_val_texte(ctx, n->fils[0], item, sizeof item);
    if (ctx->erreur) return 1;

    hc_do_menu(item);                 /* message d'abord, action ensuite */
    set_result("");
    return 1;
}

/* choose <outil> [tool].
 *
 * Le motif « W [tool] » a déjà séparé les mots du nom et le suffixe « tool ».
 * Un mot brut (HCTN_IDENT) se lit par v3_mot_ou_var : une variable liée de ce
 * nom l'emporte, sinon c'est le mot lui-même — « choose tl tool » lit tl,
 * « choose line tool » sans variable « line » rend « line ». Le reste (une
 * chaîne, « choose "Select Tool" ») s'évalue normalement. */
static int v3_cmd_choose(HctContexte *ctx, const HctNoeud *n)
{
    char nom[128];
    int pos = 0;
    nom[0] = '\0';

    for (int i = 0; i < n->nfils; i++) {
        const HctNoeud *f = n->fils[i];
        if (f->genre == HCTN_MOTCLE) continue;      /* le « tool » du motif */
        char m[128];
        if (f->genre == HCTN_IDENT) v3_mot_ou_var(ctx, f, m, sizeof m);
        else {
            v3_val_texte(ctx, f, m, sizeof m);
            if (ctx->erreur) return 1;
        }
        if (!*m) continue;
        pos += snprintf(nom + pos, sizeof nom - (size_t)pos, "%s%s",
                        pos ? " " : "", m);
        if (pos >= (int)sizeof nom) { pos = (int)sizeof nom - 1; break; }
    }

    /* Le suffixe « tool » peut aussi être venu de la chaîne elle-même. */
    int k = (int)strlen(nom);
    if (k > 4 && ci_equal(nom + k - 4, "tool")) {
        k -= 4;
        while (k > 0 && isspace((unsigned char)nom[k-1])) k--;
        nom[k] = '\0';
    }

    g_visual_dirty = 1;
    emit(HC_TRACE, "   ✎ choose « %s »", nom);
    if (g_host && g_host->choose_tool) g_host->choose_tool(nom);
    else emit(HC_ERR, "   !! choose : l'hôte ne gère pas les outils");
    set_result("");
    return 1;
}

/* UNE COORDONNÉE QUI N'EN EST PAS UNE NE SE DESSINE PAS — ET NE SE PLAINT PAS.
 *
 * coord_champ passe par hc_coord, qui exige que TOUT le champ soit un nombre
 * et rend le DÉFAUT sinon — zéro, ici. C'est la faute que le commentaire de
 * hc_entier_tete décrit quelques milliers de lignes plus haut : « il rend le
 * défaut, et l'appelant croit avoir lu ».
 *
 * Mesuré sur une pile réelle. Un traceur de courbes calcule
 * « round(-(y-cy)*yScale + 171) » avec y valant NAN(004) : la coordonnée
 * devenait zéro, drag traçait jusqu'au bord supérieur de la carte, puis en
 * revenait au point suivant. Deux segments quasi verticaux, et une barre en
 * travers de la courbe — pour un point sur mille, et sans un mot.
 *
 * Zéro est une coordonnée PARFAITEMENT VALIDE : rien ne distingue le bord de
 * la carte d'une valeur qu'on n'a pas su lire. C'est ce qui rendait ce défaut
 * muet, et c'est pourquoi il fallait cesser de dessiner.
 *
 * MAIS PAS LEVER UNE ERREUR, et c'est une décision que j'avais prise par
 * raisonnement avant de la mesurer. Sous HyperCard, le même traceur en mode
 * point — qui clique AVANT son test de bornes, donc avec un NaN en main —
 * n'ouvre AUCUNE alerte. Il ne dessine rien et passe au point suivant.
 *
 * HC faisait autrement : il levait une faute, qui remontait au dialogue et
 * interrompait. Sur une courbe de mille points, cent quarante-deux fois.
 *
 * On s'aligne : rien n'est dessiné, rien n'est interrompu, et la ligne part
 * au MONITEUR seulement — HC_INFO, pas HC_ERR. Le dialogue reste muet comme
 * chez HyperCard, et qui regarde la trace voit quand même passer les points
 * écartés. Le silence mesuré ne vaut que pour l'utilisateur, pas pour qui
 * cherche un défaut.
 *
 * Rend 1 si le point est lisible, 0 après l'avoir noté. */
static int coord_finie(const char *champ)
{
    if (!hct_est_nombre(champ)) return 0;
    double d = hct_vers_nombre(champ);
    return !isnan(d) && !isinf(d);
}

static int v3_point_lisible(const char *p, const char *verbe)
{
    const char *virgule = strchr(p, ',');
    char gx[64];
    size_t l = virgule ? (size_t)(virgule - p) : strlen(p);
    if (l >= sizeof gx) l = sizeof gx - 1;
    memcpy(gx, p, l); gx[l] = '\0';

    const char *mauvais = NULL;
    if (!coord_finie(gx))                          mauvais = gx;
    else if (virgule && !coord_finie(virgule + 1)) mauvais = virgule + 1;
    if (!mauvais) return 1;

    emit(HC_INFO, "   · %s : « %s » n'est pas une coordonnée, point écarté",
         verbe, mauvais);
    return 0;
}

static int v3_cmd_drag(HctContexte *ctx, const HctNoeud *n)
{
    int ito = v3_indice_motcle(n, "to", 0);
    if (ito < 0) { emit(HC_ERR, "   !! drag : « to » manquant"); return 1; }

    int iwith = v3_indice_motcle(n, "with", ito + 1);
    int deb1  = v3_est_motcle(n, 0, "from") ? 1 : 0;

    char p1[128], p2[128], mods[128];
    v3_point(ctx, n, deb1, ito, p1, sizeof p1);
    if (ctx->erreur) return 1;
    v3_point(ctx, n, ito + 1, iwith >= 0 ? iwith : n->nfils, p2, sizeof p2);
    if (ctx->erreur) return 1;
    v3_touches(n, iwith >= 0 ? iwith + 1 : n->nfils, mods, sizeof mods);

    if (!v3_point_lisible(p1, "drag") || !v3_point_lisible(p2, "drag")) return 1;

    int x1 = coord_champ(p1, 0), y1 = 0, x2 = coord_champ(p2, 0), y2 = 0;
    const char *c1 = strchr(p1, ','), *c2 = strchr(p2, ',');
    if (c1) y1 = coord_champ(c1 + 1, 0);
    if (c2) y2 = coord_champ(c2 + 1, 0);

    g_visual_dirty = 1;
    { const char *t = host_global("tool");
      emit(HC_TRACE, "   ✎ drag %d,%d -> %d,%d (%s)",
           x1, y1, x2, y2, t ? t : "?"); }
    if (g_host && g_host->drag) g_host->drag(x1, y1, x2, y2, mods);
    else emit(HC_ERR, "   !! drag : l'hôte ne gère pas la souris");
    set_result("");
    return 1;
}

static int v3_cmd_click(HctContexte *ctx, const HctNoeud *n)
{
    int iwith = v3_indice_motcle(n, "with", 0);
    int deb   = v3_est_motcle(n, 0, "at") ? 1 : 0;

    char pt[128], mods[128];
    v3_point(ctx, n, deb, iwith >= 0 ? iwith : n->nfils, pt, sizeof pt);
    if (ctx->erreur) return 1;
    v3_touches(n, iwith >= 0 ? iwith + 1 : n->nfils, mods, sizeof mods);

    if (!v3_point_lisible(pt, "click")) return 1;

    int x = coord_champ(pt, 0), y = 0;
    const char *c = strchr(pt, ',');
    if (c) y = coord_champ(c + 1, 0);

    g_visual_dirty = 1;
    { const char *t = host_global("tool");
      emit(HC_TRACE, "   ✎ click at %d,%d (%s)", x, y, t ? t : "?"); }
    if (g_host && g_host->click_at) g_host->click_at(x, y, mods);
    else emit(HC_ERR, "   !! click : l'hôte ne gère pas la souris");
    set_result("");
    return 1;
}

static int v3_cmd_type(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return 0;

    int iwith = v3_indice_motcle(n, "with", 0);
    if (iwith == 0) return 0;

    /* Le texte tapé peut être long : tampon d'arène, comme l'ancien code, et
     * non sur la pile. */
    ARENA_MARK;
    char *txt = arena_buf();
    char mods[128];
    v3_val_texte(ctx, n->fils[0], txt, HC_VAL);
    if (ctx->erreur) { ARENA_FREE; return 1; }
    v3_touches(n, iwith >= 0 ? iwith + 1 : n->nfils, mods, sizeof mods);

    g_visual_dirty = 1;
    if (g_host && g_host->type_text) g_host->type_text(txt, mods);
    else emit(HC_ERR, "   !! type : l'hôte ne gère pas le clavier");
    ARENA_FREE;
    set_result("");
    return 1;
}

/* mark / unmark.
 *
 * « mark cards where <condition> » garde l'ancien chemin : « where » n'est
 * pas un mot du motif, il arrive donc en identificateur ordinaire et la
 * condition qui le suit n'est pas isolée de façon sûre. C'est le genre de
 * forme qui mérite son propre motif dans hct_cmd.c plutôt qu'un découpage
 * ici. */
static int v3_cmd_marque(HctContexte *ctx, const HctNoeud *n)
{
    int poser = ci_equal(n->op, "mark");

    for (int i = 0; i < n->nfils; i++) {
        char m[16];
        v3_brut(n->fils[i], m, sizeof m);
        if (ci_equal(m, "where")) return 0;
    }

    Object *pile = g_current_card ? g_current_card->owner : NULL;
    while (pile && pile->type != OBJ_STACK) pile = pile->owner;
    if (!pile) { set_result("No stack"); return 1; }

    char m0[16];
    m0[0] = '\0';
    if (n->nfils >= 1) v3_brut(n->fils[0], m0, sizeof m0);

    if (ci_equal(m0, "all")) {
        for (int i = 0; i < pile->nparts; i++)
            if (pile->parts[i]->type == OBJ_CARD)
                pile->parts[i]->marked = poser;
        set_result("");
        return 1;
    }

    Object *c = n->nfils >= 1 ? hct_resout(ctx, n->fils[0]) : g_current_card;
    if (!c || c->type != OBJ_CARD) return 0;
    c->marked = poser;
    set_result("");
    return 1;
}

/* wait [while|until] <condition> | wait [for] <durée> [ticks|seconds].
 *
 * La condition est réévaluée depuis L'ARBRE à chaque tour, non depuis son
 * texte : « wait until the mouse is up » ne relexe plus rien. */
static int v3_cmd_wait(HctContexte *ctx, const HctNoeud *n)
{
    int i = 0, condition = 0, jusqua = 0;

    if      (v3_est_motcle(n, i, "until")) { jusqua = condition = 1; i++; }
    else if (v3_est_motcle(n, i, "while")) { condition = 1; i++; }
    if (v3_est_motcle(n, i, "for")) i++;

    if (i >= n->nfils) return 0;
    const HctNoeud *e = n->fils[i++];

    if (condition) {
        long tours = 0;
        for (;;) {
            char v[64];
            v3_val_texte(ctx, e, v, sizeof v);
            if (ctx->erreur) return 1;
            int vrai = truthy(v);
            if (jusqua ? vrai : !vrai) break;
            if (++tours > HC_MAX_LOOP) {
                emit(HC_ERR, "!! wait interrompu après %d tours", HC_MAX_LOOP);
                break;
            }
            /* Sans le sommeil de l'hôte, cette boucle tournerait à plein
             * régime en attendant un clic. */
            attends(1.0 / 60.0);
        }
        return 1;
    }

    char v[64];
    v3_val_texte(ctx, e, v, sizeof v);
    if (ctx->erreur) return 1;

    double nb = hct_est_nombre(v) ? hct_vers_nombre(v) : 0.0;
    double ticks = nb;               /* le tick est l'unité par défaut */

    if (v3_est_motcle(n, i, "seconds") || v3_est_motcle(n, i, "second") ||
        v3_est_motcle(n, i, "secs")    || v3_est_motcle(n, i, "sec"))
        ticks = nb * 60.0;

    if (ticks > HC_MAX_LOOP) ticks = HC_MAX_LOOP;
    attends(ticks / 60.0);
    return 1;
}

/* =========== navigation, fichiers, et piles « en usage » ============ *
 *
 * Les verbes qui font CHANGER DE LIEU ou qui touchent au disque :
 *
 *     go            se déplacer de carte en carte, ou de pile en pile
 *     open / close  un fichier texte
 *     save          une pile, sous un autre nom
 *     start/stop using   une pile ajoutée à la chaîne des messages
 *
 * Ils partagent un besoin que les autres n'ont pas : désigner une pile QUI
 * N'EST PAS ENCORE LÀ. hct_resout ne peut rien pour elle — il n'y a pas
 * d'objet à trouver —, d'où v3_nom_pile, qui rend le NOM plutôt que l'objet.
 * C'est ce qui les regroupe ici.
 * ==================================================================== */

static int     file_open(const char *nom);          /* défini plus bas */
static Object *marked_card_ref(const char *r, int *concerne);

/* Le NOM d'une pile désignée par un nœud.
 *
 * On ne peut pas se contenter de hct_resout : « go to stack "Index" » doit
 * pouvoir OUVRIR une pile qui n'est pas encore là, et il faut donc son nom,
 * pas un objet qui n'existe pas. Quand la référence est déjà résolue —
 * « this stack » — on rend le nom de l'objet trouvé. */
static void v3_nom_pile(HctContexte *ctx, const HctNoeud *ref,
                        char *out, int outlen)
{
    out[0] = '\0';
    if (!ref) return;

    if (ref->genre == HCTN_OBJET && ref->typeobj == HCT_OBJ_STACK) {
        const HctNoeud *des = v3_designateur(ref);
        if (des) { v3_val_texte(ctx, des, out, outlen); return; }
        Object *s = hct_resout(ctx, ref);
        snprintf(out, (size_t)outlen, "%s", s && s->name ? s->name : "");
        return;
    }
    v3_val_texte(ctx, ref, out, outlen);
}

/* ---------------------------------------------------------------- go
 *
 * Le seul verbe du groupe qui déplace l'utilisateur, et le seul dont une
 * erreur laisse la pile dans un état bancal plutôt que de ne rien faire.
 * D'où le parti pris : dès que la cible n'est pas une carte, on rend 0 et
 * l'ancien exécuteur reprend la ligne entière, avec ses messages d'erreur.
 * Il refait le travail, mais seulement quand la v3 a échoué.
 */
/* Se rendre dans une pile, par son nom : sa première carte.
 *
 * Une pile déjà ouverte : on s'y rend. Sinon on demande à l'hôte de l'ouvrir
 * — lui seul sait où chercher le fichier et comment lui donner une fenêtre.
 * C'est ce qui permet à une pile d'en appeler une autre.
 *
 * Extrait de v3_cmd_go pour que « go home » l'emploie aussi : Home n'est pas
 * une destination à part, c'est une PILE qui porte ce nom. L'utilisatrice l'a
 * fait remarquer en une phrase — « on a déjà go to stack xxx » — et il n'y
 * avait en effet rien d'autre à écrire. */
static int v3_va_a(Object *dst);   /* la navigation, définie juste après */

static int v3_va_pile(const char *nom)
{
    Object *cible = find_open_stack(nom);
    if (!cible && g_host && g_host->open_stack)
        cible = g_host->open_stack(nom);

    /* Pile introuvable : on le DIT, au lieu de rendre 0.
     *
     * Rendre 0 renvoyait la ligne à l'ancien interprète, qui posait bien
     * « No such stack ». Depuis que cette porte est fermée, le script
     * recevait « ne sait pas faire : go to stack "X" » et « the result »
     * valait « Can't understand » — un diagnostic faux : la commande était
     * parfaitement comprise, c'est la pile qui manque. Le nom du call site ne
     * laisse aucun doute, il n'y a rien d'autre à essayer.
     *
     * Trouvé par la batterie d'hôte minimal : sans open_stack, TOUT « go to
     * stack » tombait dans ce cas. */
    if (!cible) {
        set_result("No such stack");
        emit(HC_ERR, "   !! go : pile introuvable : %s", nom);
        return 1;
    }

    Object *prem = NULL;
    for (int k = 0; k < cible->nparts; k++)
        if (cible->parts[k]->type == OBJ_CARD) { prem = cible->parts[k]; break; }
    if (!prem) { set_result("No such card"); return 1; }

    return v3_va_a(prem);
}

/* Le déplacement proprement dit, une fois la carte connue : l'effet armé,
 * les quatre messages de couche dans l'ordre d'HyperCard, l'arrivée.
 *
 * Extrait de v3_cmd_go pour que « go back » l'emploie aussi. Le recopier
 * aurait donné deux navigations à tenir d'accord — et c'est exactement ainsi
 * qu'un effet visuel ou un closeBackground finit par manquer d'un côté. */
static int v3_va_a(Object *dst)
{
    if (!dst || dst->type != OBJ_CARD) return 0;

    set_result("");

    /* Jouer l'effet armé, s'il y en a un, AVANT de changer de carte : l'hôte
     * a besoin de photographier l'écran de départ. Puis on l'oublie —
     * « visual » ne vaut que pour le prochain « go ». */
    if (g_visual_effect[0]) {
        if (g_host && g_host->visual_effect)
            g_host->visual_effect(g_visual_effect, g_visual_speed,
                                  g_visual_image);
        g_visual_effect[0] = g_visual_speed[0] = g_visual_image[0] = '\0';
    }

    /* LES SIX MESSAGES, dans l'ordre d'HyperCard : ceux de PILE encadrent
     * ceux de COUCHE, et chacun n'est envoyé que si la chose change vraiment.
     *
     *     closeCard, closeBackground, closeStack
     *     openStack, openBackground, openCard
     *
     * Le changement de pile se décide ICI, en comparant les propriétaires,
     * plutôt que dans le chemin qui appelle. « go to stack "X" » n'en
     * envoyait que deux — closeStack et openStack — et jamais openCard : il
     * n'annonçait donc pas son arrivée sur une carte, l'historique de
     * navigation ne voyait pas le déplacement, et le « go back » qui suivait
     * un « go home » revenait dans le vide. Une seule navigation pour tout
     * le monde, et la question ne se repose plus. */
    Object *old    = g_current_card;
    Object *oldbg  = old ? old->bg : NULL;
    Object *pile   = owning_stack(dst);
    Object *vpile  = old ? owning_stack(old) : NULL;
    int change_pile = (pile != vpile);

    if (old) hc_send_systeme(old, "closeCard");
    if (oldbg && oldbg != dst->bg) hc_send_systeme(oldbg, "closeBackground");
    if (change_pile && vpile) hc_send_systeme(vpile, "closeStack");

    g_current_card = dst;
    if (change_pile && g_host && g_host->stack_changed) g_host->stack_changed(pile);

    if (change_pile && pile) hc_send_systeme(pile, "openStack");
    if (dst->bg && dst->bg != oldbg) hc_send_systeme(dst->bg, "openBackground");
    emit(HC_INFO, "   ⇒ va à la carte \"%s\"", dst->name ? dst->name : "?");
    hc_send_systeme(dst, "openCard");
    return 1;
}

/* Revenir à la carte précédente, depuis l'interface : c'est l'article Back du
 * menu Go. Le même geste que « go back », et par le même chemin. */
int hc_go_back(void)
{
    Object *avant = histo_recule();
    if (!avant) return 0;
    int gele = g_histo_gele;
    g_histo_gele = 1;
    int r = v3_va_a(avant);
    g_histo_gele = gele;
    return r;
}

/* Aller DIRECTEMENT à une carte de l'historique, par son rang.
 *
 * C'est ce que fait l'article Recent du menu Go : la palette de vignettes
 * d'HyperCard désignait une carte visitée, on la désigne par son nom.
 *
 * Le rang est celui de la liste SANS DOUBLON — la même que montre le menu.
 * Compter sur la pile brute ferait désigner par le cinquième article une
 * autre carte que la cinquième affichée, dès qu'un aller-retour a inscrit
 * deux fois la même.
 *
 * L'historique n'est PAS gelé ici, contrairement à « go back ». Sauter à la
 * cinquième carte visitée est une navigation ordinaire — elle doit s'inscrire,
 * sans quoi un « Back » juste après repartirait d'où l'on venait et non d'où
 * l'on est. Seul « Back », qui retrace ses pas, doit s'en abstenir. */
/* Aller à une carte DÉSIGNÉE, sous réserve qu'elle vive encore.
 *
 * Le menu Recent retenait un rang, et reconstruisait la liste au clic : entre
 * l'ouverture du menu et le clic, une navigation — un « on idle » suffit —
 * pouvait changer l'historique, et le rang ne désignait plus la même carte.
 * Un pointeur ne souffre pas de ce décalage, et hc_object_is_live répond pour
 * le cas où la carte a disparu entre-temps. */
int hc_go_card(Object *card)
{
    if (!card || card->type != OBJ_CARD) return 0;
    if (!hc_object_is_live(card)) return 0;
    return v3_va_a(card);
}

int hc_go_recent(int i)
{
    Object *vues[HC_HISTO_MAX];
    int n = hc_recent_distinct(vues, HC_HISTO_MAX);
    if (i < 0 || i >= n) return 0;
    return v3_va_a(vues[i]);
}

static int v3_cmd_go(HctContexte *ctx, const HctNoeud *n)
{
    int i = v3_est_motcle(n, 0, "to") ? 1 : 0;
    if (i >= n->nfils) return 0;

    /* « go to next marked card » : le marquage FILTRE la navigation. La carte
     * visée n'est pas la suivante de la pile, mais la suivante qui porte la
     * marque — et hct_resout n'en sait rien.
     *
     * Cette ligne partait à l'ancien interprète. Depuis que la porte est
     * fermée elle ne partait plus nulle part : « ne sait pas faire : go to
     * next marked card », alors que « mark this card » et « the marked of
     * card 3 » marchaient très bien. Un marquage qu'on peut poser et lire mais
     * pas parcourir ne sert à rien.
     *
     * On la traite donc ici. La SÉLECTION reste celle de marked_card_ref — du
     * parcours de cartes, rien de plus, et déjà éprouvé — mais la NAVIGATION
     * est celle de la v3 : v3_va_a envoie les six messages dans l'ordre
     * d'HyperCard et inscrit l'arrivée dans l'historique, ce que l'ancien
     * chemin ne faisait pas.
     *
     * Le texte se reconstitue depuis l'arbre : les jetons pointent dans le
     * script d'origine, et « next marked card » y est contigu. On saute le
     * verbe et le « to » facultatif, que marked_card_ref n'attend pas. */
    for (int k = i; k < n->nfils; k++) {
        /* Le mot peut se présenter de deux façons. Quand il précède un type
         * d'objet — « marked card » — l'analyseur l'absorbe dans la référence
         * et pose son drapeau ; seul reste un nœud dont le drapeau parle.
         * Ailleurs il subsiste comme un mot nu. Les deux comptent : ne
         * chercher que le mot nu laissait « go to next marked card » repartir
         * sur le chemin ordinaire, qui ne retenait que « next » et changeait
         * de carte sans regarder la marque. */
        char m[24];
        v3_brut(n->fils[k], m, sizeof m);
        if (!(n->fils[k] && n->fils[k]->marque) && !ci_equal(m, "marked"))
            continue;

        ARENA_MARK;
        char *txt = arena_buf();
        v3_source(n, txt, HC_VAL);
        const char *a = skip_spaces(txt);
        char verbe[16];
        a = skip_spaces(next_word(a, verbe, sizeof verbe));   /* « go » */
        if (ci_word(a, "to")) a = skip_spaces(a + 2);

        int concerne = 0;
        Object *cible = marked_card_ref(a, &concerne);
        ARENA_FREE;

        /* concerne = 0 : ce n'était pas une référence de carte marquée malgré
         * le mot — on laisse le chemin ordinaire s'en occuper. */
        if (!concerne) break;
        if (!cible) { set_result("No such card"); return 1; }
        set_result("");
        v3_va_a(cible);
        return 1;
    }

    const HctNoeud *ref = n->fils[i];

    /* ---- go back ----
     * On retrace les pas : le sommet de l'historique est la carte courante,
     * celle d'en dessous est la destination. L'arrivée ne s'inscrit PAS —
     * sans quoi deux « go back » de suite feraient la navette entre deux
     * cartes au lieu de continuer à remonter.
     *
     * « go recent » est le même geste : c'est ainsi qu'HyperCard nommait
     * l'article de menu qui ramène à la carte précédente. */
    if (n->nfils == i + 1 && ref->genre == HCTN_IDENT) {
        char mot[16];
        v3_brut(ref, mot, sizeof mot);
        /* « go home » : la pile nommée « Home », par le chemin de « go to
         * stack "Home" ». Pas de destination magique — HyperCard non plus
         * n'en avait pas : Home était un fichier de pile comme un autre, que
         * l'application savait retrouver. Ici c'est l'hôte qui sait, et il
         * répond déjà pour « go to stack ». */
        if (ci_equal(mot, "home")) return v3_va_pile("Home");

        if (ci_equal(mot, "back") || ci_equal(mot, "recent")) {
            Object *avant = histo_recule();
            if (!avant) {
                set_result("No such card");
                emit(HC_ERR, "   !! rien où revenir : l'historique est vide");
                return 1;
            }
            int gele = g_histo_gele;
            g_histo_gele = 1;
            int r = v3_va_a(avant);
            g_histo_gele = gele;
            return r;
        }
    }

    /* ---- go to stack "X" ----
     * Une pile déjà ouverte : on s'y rend. Sinon on demande à l'hôte de
     * l'ouvrir — lui seul sait où chercher le fichier et comment lui donner
     * une fenêtre. C'est ce qui permet à une pile d'en appeler une autre. */
    if (ref->genre == HCTN_OBJET && ref->typeobj == HCT_OBJ_STACK) {
        char nom[256];
        v3_nom_pile(ctx, ref, nom, sizeof nom);
        if (ctx->erreur) return 1;

        return v3_va_pile(nom);
    }

    Object *dst = NULL;

    /* « go card » nu mène à la PREMIÈRE carte. hct_resout, lui, rendrait la
     * carte courante : un nœud objet sans désignateur, c'est « this ». La
     * distinction n'existe que pour go, on la fait donc ici. */
    if (ref->genre == HCTN_OBJET && ref->typeobj == HCT_OBJ_CARD &&
        ref->designateur == HCT_DES_AUCUN && ref->nfils == 0) {
        dst = nth_card(owning_stack(g_current_card), 0);
    } else {
        dst = hct_resout(ctx, ref);

        /* UN MOT NU APRÈS « go » EST UNE DESTINATION, PAS UNE EXPRESSION.
         *
         * « go prev », « go next », « go first ». hct_resout ne connaît que
         * les nœuds OBJET et rendait NULL ; on tombait alors dans l'évaluation
         * juste en dessous, qui traite `prev` comme une expression — donc
         * comme une variable, puis comme une fonction. Ni l'une ni l'autre :
         * c'est un désignateur de carte, et resolve sait le lire depuis
         * toujours.
         *
         * Ce que cela coûtait, mesuré : la v3 ne sachant pas répondre,
         * v3_fonction sondait l'ancien moteur, qui RÉSOLVAIT le mot et rendait
         * la carte. La navigation marchait donc — par le plus long chemin
         * possible, et au prix d'une entrée dans term_value par mot. Pire :
         * quand j'ai fermé cette sonde, le mot est parti comme MESSAGE dans
         * toute la hiérarchie. Une pile qui définit « on prev » l'aurait
         * intercepté, et « go prev » aurait appelé son gestionnaire au lieu de
         * changer de carte.
         *
         * LA VARIABLE GARDE LA PRIORITÉ, comme avant : « put "card 2" into p »
         * puis « go p » doit suivre la variable, pas chercher une carte
         * nommée p. On ne prend donc ce chemin que si le mot n'en nomme
         * aucune — c'est exactement l'ordre qu'avait l'évaluation, lit_var
         * passant avant tout le reste.
         *
         * Un mot qui ne désigne rien laisse dst à NULL et retombe sur
         * l'évaluation : « go zorglub » se plaint comme avant. */
        if (!dst && ref->genre == HCTN_IDENT && n->nfils == i + 1) {
            char mot[64];
            v3_brut(ref, mot, sizeof mot);
            const char *v = var_get(mot);
            if (!(v && *v)) dst = resolve(mot);
        }

        /* « go x » : la variable porte la référence. On évalue, puis on
         * résout le texte obtenu — resolve fait ce que la v3 ne sait pas
         * faire à partir d'une chaîne. */
        if (!dst) {
            char v[256];
            v3_val_texte(ctx, ref, v, sizeof v);
            if (ctx->erreur) return 1;
            if (v[0]) dst = resolve(v);
        }
    }

    /* « go background "x" » mène à la PREMIÈRE CARTE de ce fond, et
     * « go stack "x" » à la première carte de la pile : dans HyperCard on ne
     * se tient jamais sur un fond, seulement sur une carte. */
    if (dst && (dst->type == OBJ_BACKGROUND || dst->type == OBJ_STACK)) {
        Object *stk = owning_stack(dst);
        Object *trouve = NULL;
        for (int k = 0; stk && k < stk->nparts; k++) {
            Object *c = stk->parts[k];
            if (c->type != OBJ_CARD) continue;
            if (dst->type == OBJ_STACK || c->bg == dst) { trouve = c; break; }
        }
        dst = trouve;
    }

    if (!dst || dst->type != OBJ_CARD) return 0;   /* ancien chemin */
    return v3_va_a(dst);
}

/* ------------------------------------------------- open / close file
 *
 * Les autres emplois d'open et de close — une application, une fenêtre — ne
 * sont pas traités par l'ancien exécuteur non plus : sans le mot « file »,
 * on rend 0 et la ligne suit son cours. */
static int v3_cmd_fichier(HctContexte *ctx, const HctNoeud *n)
{
    if (!v3_est_motcle(n, 0, "file")) return 0;
    if (n->nfils != 2) return 0;

    /* Un chemin macOS va jusqu'à 1024 octets, et un seul dossier peut en
     * prendre 255 : 512 amputait le nom AVANT même que l'invite soit
     * composée, si bien qu'agrandir l'invite seule n'y changeait rien.
     * Mesuré : « open file » sur un nom de 600 caractères produisait une
     * invite de 538. */
    char nom[HC_CHEMIN_MAX + 2];
    v3_val_texte(ctx, n->fils[1], nom, sizeof nom);
    if (ctx->erreur) return 1;

    if (ci_equal(n->op, "close")) {
        file_close(nom);
        set_result("");
        return 1;
    }

    if (file_open(nom)) set_result("");
    else {
        set_result("Can't open file");
        emit(HC_ERR, "   !! open file : %s", nom);
    }
    return 1;
}

/* ------------------------------------------------------------- save
 *
 * « save this stack as "chemin" ». Un nom explicite ne peut désigner que la
 * pile ouverte — nous n'en tenons qu'une à la fois —, et l'on vérifie plutôt
 * que de copier silencieusement la mauvaise. */
static int v3_cmd_save(HctContexte *ctx, const HctNoeud *n)
{
    int ias = v3_indice_motcle(n, "as", 0);
    if (ias < 1 || ias + 1 >= n->nfils) return 0;

    Object *pile = hct_resout(ctx, n->fils[0]);
    if (ctx->erreur) return 1;
    if (pile && pile->type != OBJ_STACK) pile = owning_stack(pile);
    if (!pile) return 0;                  /* pile introuvable : ancien chemin */

    const HctNoeud *ou = n->fils[ias + 1];
    /* « as stack "x" » : le mot est décoratif, la cible est un chemin. */
    if (ou->genre == HCTN_OBJET && ou->typeobj == HCT_OBJ_STACK) {
        const HctNoeud *des = v3_designateur(ou);
        if (!des) return 0;
        ou = des;
    }

    char chemin[512];
    v3_val_texte(ctx, ou, chemin, sizeof chemin);
    if (ctx->erreur) return 1;
    if (!chemin[0]) { set_result("Bad parameter"); return 1; }

    /* Distinguer « l'hôte ne sait pas enregistrer » de « l'écriture a
     * échoué ». Les deux donnaient « échec de l'écriture », ce qui accuse le
     * disque alors que rien n'a été tenté — et envoie chercher un problème de
     * permissions ou de place là où il n'y en a pas. Les autres rappels
     * absents le disent déjà ainsi (« l'hôte ne gère pas les menus »). */
    if (!g_host || !g_host->save_stack) {
        set_result("Can't save stack");
        emit(HC_ERR, "   !! save : l'hôte ne sait pas enregistrer");
        return 1;
    }

    if (g_host->save_stack(pile, chemin))
        set_result("");
    else {
        emit(HC_ERR, "   !! save : échec de l'écriture : %s", chemin);
        set_result("Can't save stack");
    }
    return 1;
}

/* --------------------------------------------- start / stop using
 *
 * Une pile en usage s'insère dans la chaîne de messages : ses gestionnaires
 * deviennent appelables de partout. Redéclarer une pile déjà en usage la
 * DÉPLACE en tête plutôt que de l'ajouter deux fois.
 *
 * load_stack et non open_stack : une pile en usage reste INVISIBLE. Seul
 * « go to stack » affiche. */
static int v3_cmd_using(HctContexte *ctx, const HctNoeud *n)
{
    if (!v3_est_motcle(n, 0, "using")) return 0;
    if (n->nfils < 2) return 0;

    int demarrer = ci_equal(n->op, "start");

    char nom[256];
    v3_nom_pile(ctx, n->fils[1], nom, sizeof nom);
    if (ctx->erreur) return 1;

    Object *pile = find_open_stack(nom);

    /* Le plafond se vérifie AVANT de charger, et non après.
     *
     * load_stack ne rend pas seulement une pile : côté Cocoa elle l'ouvre, la
     * fait enregistrer et la retient dans la liste des piles en usage. Refuser
     * ensuite laissait donc une NEUVIÈME pile chargée et vivante jusqu'à la
     * fermeture, invisible à l'utilisateur comme au script — on lui répondait
     * « Too many stacks in use » et elle occupait quand même la mémoire.
     *
     * Une pile DÉJÀ ouverte échappe à ce garde : elle passera par le test
     * d'en bas, après que la boucle de retrait ait décrémenté le compte, de
     * sorte que redéclarer une pile déjà en usage reste possible — c'est le
     * geste qui la remet en tête. */
    if (!pile && demarrer && g_nusing >= HC_MAX_USING) {
        set_result("Too many stacks in use");
        emit(HC_ERR, "   !! trop de piles en usage (%d au plus) : "
                     "« %s » n'a pas été chargée", HC_MAX_USING, nom);
        return 1;
    }

    if (!pile && demarrer && g_host && g_host->load_stack)
        pile = g_host->load_stack(nom);

    /* Même raison qu'à v3_va_pile : l'ancien chemin disait « No such stack »,
     * et depuis qu'il est fermé le script recevait « Can't understand ». Le
     * « stop using » d'une pile qu'on n'utilisait pas n'est pas une erreur —
     * la demande est satisfaite — d'où le silence dans ce cas. */
    if (!pile) {
        if (demarrer) {
            set_result("No such stack");
            emit(HC_ERR, "   !! using : pile introuvable : %s", nom);
        } else {
            set_result("");
        }
        return 1;
    }

    /* La retirer d'abord, dans les deux cas : « stop » n'a que cela à faire,
     * et « start » s'en sert pour la remettre en tête. */
    for (int i = 0; i < g_nusing; i++) {
        if (g_using[i] != pile) continue;
        for (int k = i; k + 1 < g_nusing; k++) g_using[k] = g_using[k+1];
        g_nusing--;
        break;
    }
    if (demarrer) {
        if (g_nusing >= HC_MAX_USING) {
            set_result("Too many stacks in use");
            emit(HC_ERR, "   !! trop de piles en usage (%d au plus) : "
                         "« %s » n'a pas été ajoutée", HC_MAX_USING, nom);
            return 1;
        }
        g_using[g_nusing++] = pile;
    }

    set_result("");
    return 1;
}

/* ============ menus, effet visuel, appel d'un gestionnaire ============ *
 *
 * Trois familles que le répartiteur sert et qui n'ont pas trouvé de meilleur
 * voisinage :
 *
 *   - l'appel d'un GESTIONNAIRE écrit dans une pile, le pendant exact de
 *     v3_fonction_pile pour les commandes ;
 *   - l'effet VISUEL, qui ne fait que retenir trois réglages pour le « go »
 *     qui suivra ;
 *   - et surtout les MENUS : un menu n'est pas un Object, donc hct_resout ne
 *     peut rien pour lui, et toute sa mécanique — index, propriétés,
 *     articles — vit ici. « the selectedButton of family » leur tient
 *     compagnie pour la même raison : une famille n'est pas un objet non
 *     plus.
 *
 * La TABLE des verbes, elle, est cinq cents lignes plus bas. Cet en-tête
 * annonçait « la table » et couvrait tout ce qui précède : on cherchait un
 * tableau et on tombait sur les menus.
 * ====================================================================== */
/* Gestionnaire écrit dans une pile, appelé comme commande —
 * « selectline it, the name of me ».
 *
 * Le pendant exact de v3_fonction_pile. La ligne repartait à l'ancien
 * exécuteur, qui
 * la redécoupait aux virgules, réévaluait chaque argument, trouvait le
 * gestionnaire et rappelait la v3 pour l'exécuter : six cents allers-retours
 * dans une seule boucle, pour un travail que nous avions déjà fait.
 *
 * Les fils du nœud HCTN_MESSAGE sont les arguments, déjà séparés par
 * l'analyseur. On les évalue une fois et on les passe tels quels.
 *
 * Rend 0 si aucun objet de la chaîne ne définit ce gestionnaire — la ligne
 * suit alors son cours vers l'ancien chemin, qui saura dire pourquoi. */
static int v3_message_pile(HctContexte *ctx, const HctNoeud *n)
{
    /* Le nom du gestionnaire est dans le JETON, pas dans op : pour un nœud
     * HCTN_MESSAGE, l'analyseur met le mot littéral « message » dans op. */
    char nom[64];
    hct_texte(&n->jeton, nom, sizeof nom);
    if (!nom[0]) return 0;

    Object *start = g_me ? g_me : g_current_card;
    /* Même dimension qu'au dispatch : quatre maillons pour l'objet, la carte,
     * le fond et la pile, puis une place par pile en usage.
     *
     * Ce tableau faisait huit entrées. Les quatre premières étant prises,
     * build_chain n'y logeait que les QUATRE piles en usage les plus
     * récemment déclarées, et ce test — qui décide si quelqu'un répond au
     * message — déclarait introuvable un gestionnaire vivant dans une
     * bibliothèque plus ancienne. Huit « start using » puis un appel : les
     * quatre premières déclarées ne répondaient plus, sans manquer de mémoire
     * et sans qu'aucun message ne dise pourquoi. */
    Object *chain[4 + HC_MAX_USING];
    int nc = build_chain(start, chain, (int)(sizeof chain / sizeof *chain));

    int trouve = 0;
    for (int i = 0; i < nc && !trouve; i++) {
        const char *end = NULL, *hdr = NULL;
        if (find_handler(chain[i]->script, nom, &end, &hdr)) trouve = 1;
    }
    if (!trouve) return 0;

    /* Une ligne de plus que le plafond, pour la même raison qu'à « send » :
     * le seizième argument doit ARRIVER au centre pour s'y faire refuser. */
    char (*argv)[HC_VAL] = arena_rows(HC_ARGS_MAX + 1);
    if (!argv) {
        set_result("mémoire insuffisante");
        return 1;
    }
    int argc = 0;
    for (int i = 0; i < n->nfils && argc <= HC_ARGS_MAX; i++) {
        const HctNoeud *f = n->fils[i];
        if (f->genre == HCTN_MOTCLE) continue;
        char *v = arena_buf();
        v3_val_texte(ctx, f, v, HC_VAL);
        if (ctx->erreur) return 1;
        snprintf(argv[argc], HC_VAL, "%s", v);
        argc++;
    }

    set_result("");
    hc_send_args(start, nom, argv, argc);
    return 1;
}

/* visual [effect] <nom> [<vitesse>] [to <image>] : arme l'effet du PROCHAIN
 * « go » (voir v3_cmd_go, plus haut) — elle ne joue rien elle-même.
 *
 * hct_cmd.c ne motive la commande que par « [effect] W » : rien dans l'arbre
 * ne distingue déjà le nom de l'effet, sa vitesse et l'image cible — ce sont
 * juste des HCTN_IDENT à la file, un par mot. On les rejoint donc par un
 * espace, ce qui reconstitue exactement le texte que lisait l'ancien
 * exécuteur, et on lui reprend son analyse telle quelle : couper la vitesse
 * en QUEUE, puis « to » en tête de ce qui restait. Même ambiguïté qu'avant,
 * juste plus aucun texte à reconstruire depuis les jetons de la ligne
 * entière. */
static int v3_cmd_visuel(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
    int deb = (n->nfils >= 1 && n->fils[0]->genre == HCTN_MOTCLE) ? 1 : 0;

    char mots[192];
    int pos = 0;
    mots[0] = '\0';
    for (int i = deb; i < n->nfils; i++) {
        char m[64];
        v3_brut(n->fils[i], m, sizeof m);
        if (!*m) continue;
        pos += snprintf(mots + pos, sizeof(mots) - (size_t)pos, "%s%s",
                        pos ? " " : "", m);
        if (pos >= (int)sizeof mots) { pos = (int)sizeof mots - 1; break; }
    }

    g_visual_effect[0] = g_visual_speed[0] = g_visual_image[0] = '\0';

    const char *to = find_kw(mots, "to");
    char reste[192];
    int len = to ? (int)(to - mots) : (int)strlen(mots);
    if (len > (int)sizeof reste - 1) len = (int)sizeof reste - 1;
    memcpy(reste, mots, (size_t)len);
    reste[len] = '\0';
    while (len > 0 && isspace((unsigned char)reste[len-1])) reste[--len] = '\0';

    if (to) {
        const char *img = skip_spaces(to + 2);
        snprintf(g_visual_image, sizeof g_visual_image, "%s", img);
        int ni = (int)strlen(g_visual_image);
        while (ni > 0 && isspace((unsigned char)g_visual_image[ni-1]))
            g_visual_image[--ni] = '\0';
    }

    /* La vitesse est en QUEUE, et peut faire deux mots : « very fast ». On la
     * retire par la fin, ce qui laisse le nom de l'effet — lui aussi parfois
     * en plusieurs mots, d'où l'impossibilité de découper par la gauche. */
    static const char *vitesses[] = { "very fast", "very slow", "very slowly",
                                      "fast", "slow", "slowly", NULL };
    for (int i = 0; vitesses[i]; i++) {
        int lv = (int)strlen(vitesses[i]);
        int lr = (int)strlen(reste);
        if (lr > lv && ci_equal(reste + lr - lv, vitesses[i]) &&
            isspace((unsigned char)reste[lr - lv - 1])) {
            snprintf(g_visual_speed, sizeof g_visual_speed, "%s", vitesses[i]);
            int k = lr - lv - 1;
            while (k > 0 && isspace((unsigned char)reste[k-1])) k--;
            reste[k] = '\0';
            break;
        }
    }

    snprintf(g_visual_effect, sizeof g_visual_effect, "%.63s", reste);
    if (!g_visual_effect[0])
        snprintf(g_visual_effect, sizeof g_visual_effect, "%s", "dissolve");
    set_result("");
    return 1;
}

typedef int (*V3Verbe)(HctContexte *ctx, const HctNoeud *n);

/* ═══ Les commandes de menu ═════════════════════════════════════════════
 *
 * Toutes travaillent sur l'ARBRE : « menu "X" » et « menuItem 4 of menu "X" »
 * sont des nœuds d'objet depuis que la grammaire les connaît, et il n'y a
 * donc plus rien à redécouper dans du texte. */

/* Un nœud « menu <désignateur> » → son indice dans la barre, ou -1. */
static int v3_menu_index(HctContexte *ctx, const HctNoeud *n)
{
    if (!n || n->genre != HCTN_OBJET || n->typeobj != HCT_OBJ_MENU) return -1;
    if (n->nfils < 1) return -1;

    /* Le contexte peut manquer : v3_recours n'en reçoit pas, et c'est lui
     * qui sert « there is a menu "X" ». Un désignateur littéral — le cas de
     * toutes les piles — se lit alors directement dans le jeton. Une
     * expression, elle, exige un contexte : sans lui on renonce, et la ligne
     * repart par le chemin ordinaire. */
    /* LE MÊME PLAFOND QUE LE NOM POSÉ, ET C'EST TOUT L'ENJEU.
     *
     * C'était « char b[64] ». Le nom CHERCHÉ était donc amputé exactement
     * comme le nom POSÉ l'était, et les deux amputations se correspondaient :
     * « there is a menu "<200 caractères>" » répondait true — par accident.
     * Deux menus longs et différents se seraient confondus de la même façon.
     *
     * Corriger la pose sans corriger la recherche a fait tomber ce faux vrai,
     * et c'est le harnais qui l'a dit : il ne suffit pas d'agrandir un tampon,
     * il faut agrandir LES DEUX BOUTS de la comparaison. */
    char b[HC_MENU_NOM_MAX + 2];
    if (ctx) {
        v3_val_texte(ctx, n->fils[0], b, sizeof b);
        if (ctx->erreur) return -1;
    } else {
        HctGenreNoeud g = n->fils[0]->genre;
        if (g != HCTN_CHAINE && g != HCTN_NOMBRE) return -1;
        hct_texte(&n->fils[0]->jeton, b, sizeof b);
    }

    if (n->designateur == HCT_DES_RANG) {
        int r = hc_rang(b);
        return (r >= 1 && r <= g_nmenus) ? r - 1 : -1;
    }
    return menu_index(b);
}

/* LE BOUTON ALLUMÉ D'UNE FAMILLE : « the selectedButton of card family 6 ».
 *
 * Le compagnon indispensable de « family ». Sans lui, on peut grouper des
 * boutons radio mais pas savoir lequel est choisi — le groupement se voit à
 * l'écran et reste illisible depuis un script, ce qui lui ôte l'essentiel de
 * son intérêt. HyperCard rend le nom du bouton allumé, ou vide si le groupe
 * n'en a aucun.
 *
 * LA PORTÉE DÉCIDE DE LA COUCHE. « card family 6 » interroge les boutons de la
 * carte, « bg family 6 » ceux du fond : deux groupes distincts, comme
 * « card field 1 » et « bg field 1 » sont deux objets distincts. Sans portée,
 * on prend la carte — c'est là que sont les boutons dans le cas courant.
 *
 * On rend le nom ABRÉGÉ, « card button "Oui" », comme « the name of ». Un nom
 * qui porte sa couche se re-résout ; un nom nu ne dirait ni la sorte ni la
 * couche, et « the selectedButton » ne servirait qu'à l'affichage.
 *
 * FAMILLE 0 RÉPOND VIDE, ET NE LÈVE PAS D'ERREUR. Zéro est une valeur légale
 * de la propriété — « set the family to 0 » retire un bouton de son groupe —
 * mais ce n'est pas un groupe : personne n'en est membre. « Aucun bouton
 * choisi » est donc la réponse juste, et non une question mal posée.
 *
 * La propriété est ainsi TOTALE sur 0 à 15 : elle répond toujours, et un
 * script n'a pas à distinguer « le groupe est vide » de « ce numéro n'est pas
 * un groupe ». Hors de ces bornes — « family 99 » — on ne sert pas, et
 * l'appelant lève son erreur : là, la question n'a effectivement pas de sens. */
static int v3_famille_bouton_choisi(HctContexte *ctx, const HctNoeud *n,
                                    char *out, int outlen)
{
    if (!n || n->genre != HCTN_OBJET || n->typeobj != HCT_OBJ_FAMILY) return 0;
    if (n->nfils < 1 || !n->fils[0]) return 0;

    /* LE DÉSIGNATEUR EST ÉVALUÉ QUAND ON A UN CONTEXTE.
     *
     * Il ne l'était pas : on lisait le jeton littéral, et « family 6 »
     * marchait quand « family n » ou « family (1+0) » ne marchaient pas —
     * le premier rendait un ÉCHO suivi d'une faute de syntaxe, le second une
     * erreur d'analyse. La référence HyperTalk donne pourtant un intExpr :
     * une EXPRESSION entière, pas un chiffre écrit à la main.
     *
     * Le commentaire disait « v3_recours ne reçoit pas de contexte » — c'était
     * vrai, et c'était la vraie cause. Elle est corrigée à sa source : le
     * rappel `recours` porte maintenant le contexte, comme `commande` l'a
     * toujours porté. Le repli littéral reste pour un appelant qui n'en a
     * pas. */
    char b[32];
    if (ctx) {
        v3_val_texte(ctx, n->fils[0], b, sizeof b);
        if (ctx->erreur) return 0;
    } else {
        HctGenreNoeud g = n->fils[0]->genre;
        if (g != HCTN_NOMBRE && g != HCTN_CHAINE) return 0;
        hct_texte(&n->fils[0]->jeton, b, sizeof b);
    }
    int fam = hc_entier(b, 0, 15, -1);
    if (fam < 0) return 0;              /* hors bornes : pas notre affaire */

    Object *carte = g_current_card;
    if (!carte) return 0;
    Object *couche = (n->portee == HCT_PORTEE_FOND) ? carte->bg : carte;
    if (!couche) return 0;

    /* Famille 0 : aucun membre par définition, donc la boucle ne peut rien
     * trouver. On la saute plutôt que de compter sur elle — un bouton dont
     * family vaut 0 ET qui serait allumé ne doit pas être rendu comme « le
     * choix du groupe 0 ». */
    for (int i = 0; fam > 0 && i < couche->nparts; i++) {
        Object *b2 = couche->parts[i];
        if (b2->type != OBJ_BUTTON || b2->family != fam) continue;
        if (!hc_hilite_of(b2, carte)) continue;
        hc_nom_de(b2, HC_NOM_ABREGE, out, outlen);
        return 1;
    }
    /* Aucun allumé : la réponse est VIDE, et c'est une réponse — un groupe
     * sans choix est un état légitime, pas une erreur. */
    snprintf(out, (size_t)outlen, "%s", "");
    return 1;
}

/* Un nœud « menuItem <désignateur> of menu <…> » → l'indice de l'article, et
 * par `imenu` celui de son menu. -1 si l'un des deux est introuvable. */
static int v3_article_index(HctContexte *ctx, const HctNoeud *n, int *imenu)
{
    if (!n || n->genre != HCTN_OBJET || n->typeobj != HCT_OBJ_MENUITEM) return -1;
    if (n->nfils < 2) return -1;

    int im = v3_menu_index(ctx, n->fils[n->nfils - 1]);
    if (im < 0) { g_menu_echec = V3_MENU_MENU_ABSENT; return -1; }
    *imenu = im;

    char b[HC_MENU_NOM_MAX + 2];   /* même plafond : voir v3_menu_index */
    if (ctx) {
        v3_val_texte(ctx, n->fils[0], b, sizeof b);
        if (ctx->erreur) return -1;
    } else {
        HctGenreNoeud g = n->fils[0]->genre;
        if (g != HCTN_CHAINE && g != HCTN_NOMBRE) return -1;
        hct_texte(&n->fils[0]->jeton, b, sizeof b);
    }

    if (n->designateur == HCT_DES_RANG) {
        int r = hc_rang(b);
        if (r >= 1 && r <= g_menus[im].n) return r - 1;
        g_menu_echec = V3_MENU_ARTICLE_ABSENT;
        return -1;
    }
    for (int j = 0; j < g_menus[im].n; j++)
        if (ci_equal(g_menus[im].article[j], b)) return j;
    g_menu_echec = V3_MENU_ARTICLE_ABSENT;
    return -1;
}

/* Les propriétés d'un menu et de ses articles.
 *
 * L'arbre nous les donne toutes faites : « set the checkMark of menuItem 2 of
 * menu "X" to true » est un nœud « of » dont le fils gauche nomme la
 * propriété et le droit désigne l'article. Rien à redécouper.
 *
 * Rendent 1 si la propriété est de leur ressort, 0 sinon — auquel cas la
 * ligne repart par le chemin ordinaire, comme pour n'importe quel objet. */
static int v3_menu_prop_lit(HctContexte *ctx, const HctNoeud *obj,
                            const char *prop, HctValeur *out)
{
    char b[128];
    g_menu_echec = V3_MENU_RIEN;

    if (obj->typeobj == HCT_OBJ_MENU) {
        int i = v3_menu_index(ctx, obj);
        if (i < 0) { g_menu_echec = V3_MENU_MENU_ABSENT; return 0; }
        if (ci_equal(prop, "name")) { *out = hct_val_texte(g_menus[i].nom); return 1; }
        if (ci_equal(prop, "enabled")) {
            *out = hct_val_texte(g_menus[i].actif_menu ? "true" : "false");
            return 1;
        }
        if (ci_equal(prop, "number")) {
            snprintf(b, sizeof b, "%d", i + 1);
            *out = hct_val_texte(b);
            return 1;
        }
        g_menu_echec = V3_MENU_PROP_INCONNUE;
        return 0;
    }

    if (obj->typeobj == HCT_OBJ_MENUITEM) {
        int im = -1;
        int j = v3_article_index(ctx, obj, &im);
        if (j < 0) return 0;          /* v3_article_index a dit laquelle */          /* v3_article_index a dit laquelle */
        HcMenuBarre *m = &g_menus[im];

        if (ci_equal(prop, "checkmark")) {
            *out = hct_val_texte(m->coche[j] ? "true" : "false"); return 1;
        }
        if (ci_equal(prop, "enabled")) {
            *out = hct_val_texte(m->actif[j] ? "true" : "false"); return 1;
        }
        if (ci_equal(prop, "name")) {
            *out = hct_val_texte(m->article[j] ? m->article[j] : ""); return 1;
        }
        if (ci_equal(prop, "menumessage") || ci_equal(prop, "menumsg")) {
            *out = hct_val_texte(m->message[j] ? m->message[j] : ""); return 1;
        }
        if (ci_equal(prop, "number")) {
            snprintf(b, sizeof b, "%d", j + 1);
            *out = hct_val_texte(b); return 1;
        }
        g_menu_echec = V3_MENU_PROP_INCONNUE;
        return 0;
    }
    return 0;
}

static int v3_menu_prop_ecrit(HctContexte *ctx, const HctNoeud *obj,
                              const char *prop, const char *val)
{
    int vrai = truthy(val);
    g_menu_echec = V3_MENU_RIEN;

    if (obj->typeobj == HCT_OBJ_MENU) {
        int i = v3_menu_index(ctx, obj);
        if (i < 0) { g_menu_echec = V3_MENU_MENU_ABSENT; return 0; }
        if (ci_equal(prop, "name")) {
            /* Même règle qu'à la création : on refuse plutôt que d'amputer.
             * Un menu renommé trop long deviendrait introuvable sous son
             * nouveau nom ET perdu sous l'ancien. */
            if (strlen(val) >= HC_MENU_NOM_MAX) {
                hct_ctx_faute(ctx, obj, "nom de menu trop long");
                return 1;
            }
            snprintf(g_menus[i].nom, sizeof g_menus[i].nom, "%s", val);
            menus_prevenir(); return 1;
        }
        if (ci_equal(prop, "enabled")) {
            g_menus[i].actif_menu = vrai;
            menus_prevenir(); return 1;
        }
        g_menu_echec = V3_MENU_PROP_INCONNUE;
        return 0;
    }

    if (obj->typeobj == HCT_OBJ_MENUITEM) {
        int im = -1;
        int j = v3_article_index(ctx, obj, &im);
        if (j < 0) return 0;          /* v3_article_index a dit laquelle */
        HcMenuBarre *m = &g_menus[im];

        if (ci_equal(prop, "checkmark")) {
            m->coche[j] = (char)vrai; menus_prevenir(); return 1;
        }
        if (ci_equal(prop, "enabled")) {
            m->actif[j] = (char)vrai; menus_prevenir(); return 1;
        }
        if (ci_equal(prop, "name")) {
            char *t = malloc(strlen(val) + 1);
            if (!t) return 1;
            strcpy(t, val);
            free(m->article[j]); m->article[j] = t;
            menus_prevenir(); return 1;
        }
        if (ci_equal(prop, "menumessage") || ci_equal(prop, "menumsg")) {
            char *t = malloc(strlen(val) + 1);
            if (!t) return 1;
            strcpy(t, val);
            free(m->message[j]); m->message[j] = t;
            menus_prevenir(); return 1;
        }
        g_menu_echec = V3_MENU_PROP_INCONNUE;
        return 0;
    }
    return 0;
}

/* Le nœud « of » d'un set : « set the X of <objet> to … ». Rend le nœud
 * d'objet et le nom de la propriété, ou 0 si ce n'est pas cette forme. */
static const HctNoeud *v3_set_cible_menu(const HctNoeud *n, char *prop, int len)
{
    if (!n || n->nfils < 1) return NULL;
    const HctNoeud *of = n->fils[0];
    if (!of || of->genre != HCTN_OF || of->nfils < 2) return NULL;
    if (!of->fils[0] || of->fils[0]->genre != HCTN_IDENT) return NULL;

    const HctNoeud *obj = of->fils[1];
    if (!obj || obj->genre != HCTN_OBJET) return NULL;
    if (obj->typeobj != HCT_OBJ_MENU && obj->typeobj != HCT_OBJ_MENUITEM)
        return NULL;

    hct_texte(&of->fils[0]->jeton, prop, len);
    return obj;
}

/* create menu <nom> */
static int v3_cmd_create(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1 || !n->fils[0]) return 0;
    const HctNoeud *o = n->fils[0];
    if (o->genre != HCTN_OBJET || o->typeobj != HCT_OBJ_MENU) return 0;
    if (o->nfils < 1) return 0;

    /* UN OCTET DE MARGE, ET C'EST TOUT L'INTÉRÊT.
     *
     * C'était « char nom[64] » : le nom arrivait déjà amputé à 63 caractères,
     * bien avant le garde de menu_creer — qui ne voyait donc jamais de nom
     * trop long et ne refusait jamais rien. Pire, deux noms longs et
     * différents se ramenaient aux mêmes 63 octets et devenaient le même menu.
     *
     * Le tampon dépasse maintenant la limite d'un cran : un nom trop long
     * ARRIVE trop long, et menu_creer peut le dire. Corriger le plafond sans
     * cette marge n'aurait fait que déplacer le silence. */
    char nom[HC_MENU_NOM_MAX + 2];
    v3_val_texte(ctx, o->fils[0], nom, sizeof nom);
    if (ctx->erreur) return 1;

    if (!menu_creer(nom))
        emit(HC_INFO, "   → le menu « %s » existe déjà", nom);
    set_result("");
    return 1;
}

/* enable | disable  menu <nom> | menuItem <n> of menu <nom> */
static int v3_cmd_menu_actif(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1 || !n->fils[0]) return 0;
    const HctNoeud *o = n->fils[0];
    if (o->genre != HCTN_OBJET) return 0;

    int actif = ci_equal(n->op, "enable");

    if (o->typeobj == HCT_OBJ_MENU) {
        int i = v3_menu_index(ctx, o);
        if (ctx->erreur) return 1;
        if (i < 0) { set_result("menu introuvable"); return 1; }
        g_menus[i].actif_menu = actif;
        menus_prevenir();
        set_result("");
        return 1;
    }
    if (o->typeobj == HCT_OBJ_MENUITEM) {
        int im = -1;
        int j = v3_article_index(ctx, o, &im);
        if (ctx->erreur) return 1;
        if (j < 0) { set_result("article introuvable"); return 1; }
        g_menus[im].actif[j] = (char)actif;
        menus_prevenir();
        set_result("");
        return 1;
    }
    return 0;                       /* enable d'autre chose : pas pour nous */
}

/* put <articles> into|before|after  menu <nom> | menuItem <d> of menu <nom>
 *                [with menuMsg <messages>]
 *
 * L'exécuteur nous confie la ligne ENTIÈRE, non évaluée, parce que
 * cible_connue refuse les menus : on évalue donc soi-même, et une seule fois.
 * L'arbre est « e <préposition> <objet> [with menumsg e] ».
 *
 * DEUX MANQUES, ET LE SECOND ÉTAIT LE PIRE.
 *
 * « menuItem N of menu "X" » n'était pas reconnu comme cible : on rendait 0,
 * et la ligne finissait en « ne sait pas faire ». Un refus, au moins.
 *
 * Mais LA PRÉPOSITION ÉTAIT IGNORÉE — on lisait fils[2] sans jamais regarder
 * fils[1]. « put "Aide" after menu "X" » remplaçait le menu entier par cet
 * unique article, en silence et en rendant un résultat vide. Une commande
 * d'ajout qui efface : le script croit compléter son menu et le détruit. On
 * ne le voyait pas parce que la forme `into`, la seule écrite dans les
 * harnais, donne justement le bon résultat par accident.
 *
 * Les trois prépositions se ramènent à un (pos, remplace) que menu_insere
 * applique ; le tableau des six cas est chez elle. */
static int v3_cmd_put_menu(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 3) return 0;
    const HctNoeud *cible = n->fils[2];
    if (!cible || cible->genre != HCTN_OBJET) return 0;
    if (cible->typeobj != HCT_OBJ_MENU &&
        cible->typeobj != HCT_OBJ_MENUITEM) return 0;

    int avant = v3_est_motcle(n, 1, "before");
    int apres = v3_est_motcle(n, 1, "after");

    int i = -1, pos = 0, remplace = 0;
    if (cible->typeobj == HCT_OBJ_MENU) {
        i = v3_menu_index(ctx, cible);
        if (ctx->erreur) return 1;
        if (i < 0) {
            emit(HC_ERR, "   !! menu introuvable");
            set_result("menu introuvable");
            return 1;
        }
        pos      = apres ? g_menus[i].n : 0;
        remplace = (!avant && !apres) ? g_menus[i].n : 0;
    } else {
        g_menu_echec = V3_MENU_RIEN;
        int j = v3_article_index(ctx, cible, &i);
        if (ctx->erreur) return 1;
        if (j < 0) {
            /* Dire LEQUEL manque : « menuItem 2 of menu "Absent" » échoue sur
             * le menu, pas sur l'article, et v3_article_index le sait. */
            const char *raison = v3_menu_raison();
            emit(HC_ERR, "   !! %s", raison);
            set_result(raison);
            return 1;
        }
        pos      = apres ? j + 1 : j;
        remplace = (!avant && !apres) ? 1 : 0;
    }

    ARENA_MARK;
    char *articles = arena_buf();
    char *messages = arena_buf();
    articles[0] = messages[0] = '\0';

    v3_val_texte(ctx, n->fils[0], articles, HC_VAL);

    /* « with menuMsg <e> » : le mot-clé et son expression sont les derniers
     * fils. On prend la dernière expression, quel que soit le nombre de
     * mots-clés que le motif a posés devant elle. */
    if (!ctx->erreur && n->nfils >= 5) {
        const HctNoeud *d = n->fils[n->nfils - 1];
        if (d && d->genre != HCTN_MOTCLE)
            v3_val_texte(ctx, d, messages, HC_VAL);
    }

    int pose = 1;
    if (!ctx->erreur) pose = menu_insere(i, pos, remplace, articles, messages);
    ARENA_FREE;
    set_result(pose ? "" : "trop d'articles de menu");
    return 1;
}

/* ==================== la table des verbes portés ==================== *
 *
 * Elle fut le tableau d'avancement de la migration : ce qui n'y figure pas
 * est ce qui n'a jamais été porté. Ce n'est plus un état des lieux mais une
 * FRONTIÈRE — depuis que l'ancien exécuteur de lignes a disparu, un verbe
 * absent d'ici est refusé, avec un message, au lieu d'être rendu à personne.
 * ==================================================================== */
static const struct { const char *verbe; V3Verbe fn; } V3_VERBES[] = {
    { "answer",      v3_cmd_reponse         },
    { "answer file", v3_cmd_reponse_fichier },
    { "ask",         v3_cmd_demande         },
    { "ask file",    v3_cmd_demande_fichier },
    { "beep",   v3_cmd_beep    },
    { "choose", v3_cmd_choose  },
    { "click",  v3_cmd_click   },
    { "close",  v3_cmd_fichier },
    { "convert", v3_cmd_convert },
    { "create",  v3_cmd_create  },
    { "disable", v3_cmd_menu_actif },
    { "enable",  v3_cmd_menu_actif },
    { "put",     v3_cmd_put_menu },
    { "debug",  v3_cmd_debug   },
    { "delete", v3_cmd_delete  },
    { "domenu", v3_cmd_domenu  },
    { "drag",   v3_cmd_drag    },
    { "find",   v3_cmd_find    },
    { "go",     v3_cmd_go      },
    { "hide",   v3_cmd_montre  },
    { "lock",   v3_cmd_verrou  },
    { "mark",   v3_cmd_marque  },
    { "open",   v3_cmd_fichier },
    { "play",   v3_cmd_play    },
    { "pop",    v3_cmd_pop     },
    { "print",  v3_cmd_print   },
    { "push",   v3_cmd_push    },
    { "read",   v3_cmd_read    },
    { "reset",  v3_cmd_reset   },
    { "save",   v3_cmd_save    },
    { "select", v3_cmd_select  },
    { "send",   v3_cmd_send    },
    { "set",    v3_cmd_set     },
    { "show",   v3_cmd_montre  },
    { "sort",   v3_cmd_sort    },
    { "start",  v3_cmd_using   },
    { "stop",   v3_cmd_using   },
    { "type",   v3_cmd_type    },
    { "unlock", v3_cmd_verrou  },
    { "unmark", v3_cmd_marque  },
    { "visual", v3_cmd_visuel  },
    { "wait",   v3_cmd_wait    },
    { "write",  v3_cmd_write   },
    { NULL, NULL }
};
static int v3_commande(void *d, const HctNoeud *n, HctContexte *ctx)
{
    (void)d;
    /* LA REMISE À ZÉRO EST APRÈS LE VERDICT, PAS AVANT.
     *
     * Ma première version l'avait mise ici, à l'entrée. Elle effaçait alors ce
     * qu'il fallait justement lire : « put x into card field "Absent" » résout
     * sa cible dans l'EXÉCUTEUR, donc AVANT d'arriver au répartiteur, et le
     * drapeau posé par cette résolution était balayé en entrant. La commande
     * redisait « Can't understand ».
     *
     * On efface donc quand on a conclu — soit qu'une commande ait réussi,
     * soit qu'on ait rendu le diagnostic. Un diagnostic périmé est pire qu'un
     * diagnostic vague : il est faux avec assurance. */
    if (n->genre == HCTN_COMMANDE && n->op)
        for (int i = 0; V3_VERBES[i].verbe; i++)
            if (ci_equal(V3_VERBES[i].verbe, n->op)) {
                /* La porte prend le nom du VERBE le temps de son exécution.
                 * Une commande portée qui réanalyse encore du texte se
                 * dénonce alors elle-même — « texte réanalysé : set » vaut
                 * mieux qu'un « ? » qu'il faudrait aller débusquer. */
                const char *sauve = v1_porte(n->op);
                int fait = V3_VERBES[i].fn(ctx, n);
                g_v1_porte = sauve;
                if (fait) { g_objet_manque = NULL; return 1; }
                break;                 /* forme non portée : ancien chemin */
            }

    /* Un gestionnaire de la pile : on l'appelle avec nos arguments plutôt que
     * de rendre la ligne à l'ancien exécuteur, qui les réévaluerait. */
    if (n->genre == HCTN_MESSAGE) {
        ARENA_MARK;
        int fait = v3_message_pile(ctx, n);
        ARENA_FREE;
        if (fait) return 1;
    }

    /* --- pas encore porté : l'ancien chemin, intact --- */
    const char *deb; int len;
    if (!hct_noeud_etendue(n, &deb, &len)) return 0;   /* rien à reconstituer */
    /* Le verbe seul ne suffit pas quand il a plusieurs formes : « select »
     * revenant trois cents fois ne dit pas LAQUELLE de ses cinq syntaxes
     * n'est pas portée. On joint donc un bout de la ligne, comme pour les
     * recours. Les blancs sont aplatis pour tenir sur une ligne de bilan. */
    if (n->genre == HCTN_MESSAGE) {
        v3_note("commande", "<message>");
    } else {
        char frag[28], cle[40];
        int f = len < (int)sizeof frag - 1 ? len : (int)sizeof frag - 1;
        memcpy(frag, deb, (size_t)f);
        frag[f] = '\0';
        for (char *p = frag; *p; p++)
            if (*p == '\n' || *p == '\t' || *p == '\r') *p = ' ';
        snprintf(cle, sizeof cle, "%s: %s", n->op ? n->op : "?", frag);
        v3_note("commande", cle);
    }
    ARENA_MARK;
    char *ligne = arena_buf();
    int m = len < HC_VAL - 1 ? len : HC_VAL - 1;
    memcpy(ligne, deb, (size_t)m);
    ligne[m] = '\0';
#if HC_TRACE_V3
    fprintf(stderr, "[v3->ancien] « %s »\n", ligne);
#endif
    /* LE recours : la v3 a renoncé et rend la ligne à l'ancien interprète.
     *
     * La porte se pose ICI et pas en tête de la fonction : au-dessus, les
     * verbes portés s'exécutent en v3 et sortent par leur propre return —
     * les compter comme du recours attribuerait à l'ancien code du travail
     * que la v3 vient de faire. Un compteur qui exagère est aussi inutile
     * qu'un compteur muet. */
    /* PORTE 2 : la v3 ne sait pas exécuter cette ligne. L'ancien interprète
     * ne le savait pas non plus dans tous les cas mesurés — il rendait
     * seulement un message différent. On écrit le nôtre.
     *
     * Un MESSAGE sans gestionnaire est distingué d'une COMMANDE inconnue :
     * ce n'est pas la même faute pour qui lit, et HyperCard les nommait
     * différemment aussi. */
    v1_compte("v1 refusée", "recours v3");
    if (n->genre == HCTN_MESSAGE) {
        emit(HC_ERR, "   !! personne ne répond à « %s »", ligne);
        set_result("Can't understand");
    }
    /* LE VERBE ÉTAIT-IL COMPRIS, OU L'OBJET MANQUAIT-IL ?
     *
     * Deux fautes distinctes, qui recevaient le même message. « show card
     * field "Absent" » disait « ne sait pas faire » alors que HC comprend
     * parfaitement « show » — la commande marche dès que le champ existe.
     * Le diagnostic envoyait chercher du côté du verbe, où il n'y avait
     * rien.
     *
     * g_objet_manque est posé par hct_resout quand un nœud d'OBJET ne s'est
     * pas résolu pendant cette ligne. S'il est levé, c'est l'objet qu'il faut
     * nommer, pas le verbe — et « the result » doit dire autre chose que
     * « Can't understand », que HyperTalk réserve à une ligne incomprise. */
    /* ET SEULEMENT SI CET ÉCHEC EST LE SIEN. Le drapeau porte le nœud qui
     * n'a pas su se résoudre ; s'il n'est pas dans l'arbre de CETTE commande,
     * il vient d'une ligne précédente et n'explique rien ici. */
    else if (v3_noeud_contient(n, g_objet_manque)) {
        emit(HC_ERR, "   !! objet introuvable : %s", ligne);
        set_result("No such object");
    }
    /* Quelle qu'ait été la conclusion, la ligne suivante repart à neuf. */
    else {
        emit(HC_ERR, "   !! ne sait pas faire : %s", ligne);
        set_result("Can't understand");
    }
    g_objet_manque = NULL;
    ARENA_FREE;
    return 1;
}

/* Défini juste après ce bloc, qui s'en sert : l'ordre de lecture met
 * l'exécution avant la table des rappels, plus lisible ainsi. */
static HctHote v3_hote(void);

/* ---- exécution d'un gestionnaire par la v3 ----
 *
 * Le pari de la transition : l'exécuteur v3 déroule l'arbre, et tout ce qu'il
 * ne savait pas faire lui-même repartait vers l'ancien exécuteur par
 * v3_commande, si bien que rien ne s'arrêtait en chemin quelle que soit la
 * commande. Le pari est tenu et l'ancien exécuteur supprimé : ce qui n'est
 * pas porté est maintenant refusé, avec un message.
 *
 * On n'appelle PAS hct_appelle : il ouvre sa propre portée et y lie les
 * paramètres, alors que hc_send_args_k_body a déjà posé son cadre et lié les
 * siens par var_set. Deux liaisons dans deux magasins différents, et les
 * paramètres seraient introuvables. On exécute donc le corps directement.
 *
 * Rend 0 si le gestionnaire n'est pas dans l'arbre — l'appelant se rabat
 * le dit alors à l'utilisateur, faute de second exécuteur à qui le confier. */
static int v3_actif(void)
{
    /* Interrupteur, pour comparer les deux exécuteurs sans recompiler.
     * Lu une fois : le relire à chaque message coûterait un getenv par appel. */
    /* Défaut à la compilation, que l'environnement peut renverser dans les
     * deux sens : HC_V3=1 allume, HC_V3=0 éteint, absence laisse ce défaut.
     *
     * Passer par le seul environnement s'est révélé peu commode — une
     * variable de schéma Xcode n'arrive pas toujours jusqu'au programme, et
     * l'on cherche alors un bug là où il n'y en a pas. Une constante ici se
     * change en une compilation, et reste vraie quel que soit le lanceur. */
#ifndef HC_V3_DEFAUT
#define HC_V3_DEFAUT 1
#endif

    static int etat = -1;
    if (etat < 0) {
        const char *e = getenv("HC_V3");
        if (e && *e) etat = (*e != '0');
        else         etat = HC_V3_DEFAUT;
        /* fprintf et non emit : emit passe par l'hôte, qui filtre selon le
         * niveau. Le temps de la mise au point on veut cette ligne quoi qu'il
         * arrive, y compris quand la réponse est « non ». */
        fprintf(stderr, "[v3] HC_V3=%s (défaut %d) -> exécuteur v3 %s\n",
                (e && *e) ? e : "absent", HC_V3_DEFAUT,
                etat ? "ACTIF" : "éteint");

        /* IL N'Y A PLUS DE SECOND EXÉCUTEUR À QUI PASSER LA MAIN.
         *
         * « HC_V3=0 » éteignait la v3 en comptant sur l'ancien interprète de
         * lignes pour prendre le relais. Celui-ci n'existe plus : obéir
         * laisserait une application qui se lance et n'exécute plus rien,
         * sans que rien ne dise pourquoi. On refuse donc, et on le dit.
         *
         * La variable reste LUE plutôt qu'ignorée : quelqu'un l'a peut-être
         * dans un schéma Xcode depuis des mois, et un avertissement vaut
         * mieux qu'un silence pour l'apprendre. */
        if (!etat) {
            fprintf(stderr, "[v3] HC_V3=0 n'est plus tenable : l'ancien "
                            "exécuteur de lignes a été supprimé. On continue "
                            "avec la v3.\n");
            etat = 1;
        }
    }
    return etat;
}

/* Une faute d'analyse quelque part dans ce sous-arbre ? */
static int v3_porte_une_faute(const HctNoeud *n)
{
    if (!n) return 0;
    if (n->genre == HCTN_ERREUR) return 1;
    for (int i = 0; i < n->nfils; i++)
        if (v3_porte_une_faute(n->fils[i])) return 1;
    return 0;
}

/* Le gestionnaire est-il inexécutable ?
 *
 * Une faute dans son CADRE — l'en-tête, ou le « end » qui manque — l'est : on
 * ne sait plus où il commence ni où il finit, et l'exécuter reviendrait à
 * deviner. Il repart à l'ancien interpréteur, plus indulgent.
 *
 * Une faute dans son CORPS ne condamne qu'une INSTRUCTION. L'exécuteur la
 * signale quand il l'atteint — hct_exec traite déjà HCTN_ERREUR ainsi —, et
 * le gestionnaire s'arrête là, comme HyperCard s'arrête sur une erreur.
 * Écarter le gestionnaire entier pour une coquille sur une ligne, c'était
 * renvoyer quinze lignes saines à l'ancien interpréteur pour un guillemet
 * oublié : mesuré sur un script de test, quinze lignes pour un caractère.
 *
 * fils[0] est le nom, fils[1] les paramètres, fils[2] le corps ; tout enfant
 * au-delà est une faute de fermeture, posée là par l'analyseur. Le lexeur
 * n'endommage jamais plus d'une ligne — une chaîne non fermée s'arrête au
 * saut de ligne —, si bien que la structure qui suit la faute reste sûre. */
static int v3_cadre_fautif(const HctNoeud *n)
{
    if (!n || n->nfils < 3) return 1;
    if (v3_porte_une_faute(n->fils[0])) return 1;
    if (v3_porte_une_faute(n->fils[1])) return 1;
    for (int i = 3; i < n->nfils; i++)
        if (v3_porte_une_faute(n->fils[i])) return 1;
    return 0;
}

static const HctNoeud *trouve_gestionnaire(const HctNoeud *racine,
                                           const char *nom, int isfunc)
{
    if (!racine) return NULL;
    const char *kw = isfunc ? "function" : "on";

    for (int i = 0; i < racine->nfils; i++) {
        const HctNoeud *f = racine->fils[i];
        if (f->genre != HCTN_GESTIONNAIRE || f->nfils < 3) continue;
        /* Une faute dans le CADRE seulement l'écarte : voir v3_cadre_fautif. */
        if (v3_cadre_fautif(f)) continue;
        if (!f->op || strcasecmp(f->op, kw) != 0) continue;

        const HctJeton *j = &f->fils[0]->jeton;
        if ((int)strlen(nom) != j->len) continue;
        if (ci_nequal(j->deb, nom, j->len)) return f;
    }
    return NULL;
}

/* Trace du branchement. Mettre à 0 quand la bascule sera acquise : elle
 * imprime une ligne par MESSAGE, ce qui devient vite illisible. */
#define HC_TRACE_V3 0

static int v3_execute(Object *o, const char *message, int isfunc)
{
    if (!v3_actif()) return 0;

    /* Pourquoi la v3 renonce, le cas échéant. Trois raisons possibles, et
     * elles n'appellent pas le même remède :
     *
     *   arbre refusé   — l'analyse du script a signalé une faute, et l'on
     *                    préfère confier le script entier à l'ancien
     *                    exécuteur plutôt que d'en exécuter la moitié ;
     *   absent         — le script s'analyse, mais ce gestionnaire n'y est
     *                    pas sous cette forme ;
     *   pris en charge — la v3 exécute.
     *
     * Sur HC_ERR et non HC_TRACE : le temps de la mise au point, on veut ces
     * lignes sans avoir à lever le drapeau de trace. */
    const HctNoeud *racine = script_arbre(o);
    if (!racine) {
#if HC_TRACE_V3
        /* Une fois par objet : l'analyse ne sera pas retentée, inutile de le
         * répéter à chaque message. */
        if (!o->arbre_signale) {
            o->arbre_signale = 1;
            char d[64]; hc_describe(o, d, sizeof d);
            if (o->arbre_faute_ligne)
                fprintf(stderr, "[v3] arbre refusé pour %s "
                                "(première faute ligne %d)\n", d, o->arbre_faute_ligne);
            else
                fprintf(stderr, "[v3] arbre refusé pour %s (analyse non propre)\n", d);
        }
#endif
        return 0;
    }

    const HctNoeud *g = trouve_gestionnaire(racine, message, isfunc);
    if (!g) {
#if HC_TRACE_V3
        if (!strcasecmp(message, "idle")) return 0;
        fprintf(stderr, "[v3] « %s »%s absent de l'arbre (%d gestionnaire(s) vus)\n",
             message, isfunc ? " (fonction)" : "", racine->nfils);
        for (int i = 0; i < racine->nfils; i++) {
            const HctNoeud *f = racine->fils[i];
            if (f->genre != HCTN_GESTIONNAIRE) {
                fprintf(stderr, "[v3]   fils %d : %s (pas un gestionnaire)\n",
                     i, hct_genre_noeud_nom(f->genre));
                continue;
            }
            char nom[64];
            if (f->nfils >= 1) hct_texte(&f->fils[0]->jeton, nom, sizeof nom);
            else snprintf(nom, sizeof nom, "?");
            fprintf(stderr, "[v3]   fils %d : %s %s (%d fils)\n",
                 i, f->op ? f->op : "?", nom, f->nfils);
        }
#endif
        return 0;
    }

#if HC_TRACE_V3
    /* « idle » part à chaque tour de la boucle d'événements, plusieurs fois
     * par seconde : le tracer noie tout le reste. Les autres messages, eux,
     * sont assez rares pour qu'une ligne chacun reste lisible. */
    if (strcasecmp(message, "idle") != 0)
        fprintf(stderr, "[v3] « %s » pris en charge\n", message);
#endif

    HctExec x;
    hct_exec_init(&x, v3_hote());
    x.script = racine;

    /* Marquer l'arbre comme en cours : un script qui se réécrit lui-même
     * appellerait sinon hc_arbre_oublie et libérerait le sol sous nos pieds. */
    o->arbre_usage++;
    hct_exec(&x, g->fils[2]);          /* fils 2 = le corps */
    o->arbre_usage--;

    /* Les signaux redeviennent les drapeaux de hc_core : « pass » doit faire
     * remonter le message, et l'ancien code les lit sous cette forme. */
    if (x.signal == HCT_SIG_PASS) g_pass = 1;

    /* La valeur d'un `return` va dans `the result` — pour une FONCTION comme
     * pour un gestionnaire de message, comme le faisait l'ancien exécuteur
     * (« if the result <> empty » après un appel en dépend).
     *
     * On ne pose le résultat que si un `return` a vraiment été exécuté :
     * l'écraser systématiquement effacerait celui qu'une commande du corps
     * vient d'y déposer — « go to card 99 » y met sa plainte. */
    if (x.a_rendu && x.retour.txt) set_result(x.retour.txt);

    /* L'erreur nomme le SCRIPT, pas seulement la ligne. « objet introuvable
     * (v3, ligne 5) » laissait chercher dans quel gestionnaire de quel objet
     * — et sur une pile qui en compte trente, cela veut dire tout ouvrir.
     * « ligne 5 de card "Menu".openCard » désigne le fichier et l'endroit. */
    if (x.ctx.erreur) {
        char qui[64];
        hc_describe(o, qui, sizeof qui);
        emit(HC_ERR, "   !! %s (v3, ligne %d de %s.%s)", x.ctx.erreur,
             x.ctx.fautif ? x.ctx.fautif->jeton.ligne : 0, qui, message);
    }

    hct_exec_libere(&x);

    /* Sortie du dernier niveau : on peut enfin jeter ce qui a été marqué. */
    if (o->arbre_usage == 0 && o->arbre_perime) hc_arbre_oublie(o);
    return 1;
}

/* --- lecture d'une propriété, depuis l'arbre -------------------------
 *
 * L'évaluateur tient l'objet DÉJÀ résolu et le nom de la propriété : il n'y
 * a plus rien à reconstituer, et les pertes de la reconstitution — le « the »
 * avalé, les adjectifs — ne peuvent plus se produire ici.
 *
 * Le nom arrive tel que l'analyseur l'a fusionné. Les adjectifs de l'annexe I
 * sont collés au nom de la propriété (voir ADJECTIFS dans hct_expr.c) :
 * « the short name of me » donne « short name », en un seul jeton. On détache
 * donc l'adjectif avant de consulter obj_prop_read, qui attend le nom nu et
 * un drapeau, exactement comme le faisait term_value.
 *
 * Rend 0 pour tout ce qu'obj_prop_read ne connaît pas — les propriétés
 * globales, les formes calculées, celles d'un morceau de texte. Le recours
 * prend alors la suite, comme avant. */
static int v3_lit_prop(void *d, void *objet, const char *prop, HctValeur *out)
{
    (void)d;
    Object *o = objet;
    if (!o || !prop) return 0;

    static const char *ADJECTIFS[] = {
        "short", "long", "abbreviated", "abbrev", "abbr",
        "english", "plain", "numeric", NULL
    };

    const char *p = prop;
    int forme = HC_NOM_ABREGE;

    while (*p == ' ' || *p == '\t') p++;
    for (int i = 0; ADJECTIFS[i]; i++) {
        size_t l = strlen(ADJECTIFS[i]);
        if (strncasecmp(p, ADJECTIFS[i], l) != 0) continue;
        if (p[l] != ' ' && p[l] != '\t') continue;   /* « shortcut » n'est pas « short » */
        if (i == 0) forme = HC_NOM_COURT;
        if (i == 1) forme = HC_NOM_LONG;   /* « long » était lu puis JETÉ */
        p += l;
        while (*p == ' ' || *p == '\t') p++;
        break;
    }
    if (!*p) return 0;

    /* Le tampon vient de l'arène : « the script of me » peut peser plusieurs
     * kilo-octets, et HC_VAL sur la pile coûterait un mégaoctet par appel. */
    ARENA_MARK;
    char *buf = arena_buf();
    buf[0] = '\0';

    int ok = obj_prop_read(o, p, forme, buf, HC_VAL);
    if (ok) *out = hct_val_texte(buf);

    ARENA_FREE;
    return ok;
}

/* --- la boîte de messages ---
 *
 * « put X » sans destination, et « put X into the message box ». HC n'a pas
 * de boîte persistante : la valeur part sur la sortie, comme le fait
 * l'ancien exécuteur, qui émettait HC_MSG au même endroit.
 *
 * `mode` est donc sans objet ici — on ne peut rien ajouter à la suite de ce
 * qui a déjà été affiché. On l'ignore et l'on rend 1 : refuser renverrait la
 * ligne à l'ancien interpréteur, qui n'en ferait pas davantage. */
static int v3_ecrit_message(void *d, const char *val, int mode)
{
    (void)d; (void)mode;
    emit(HC_MSG, "%s", val ? val : "");
    return 1;
}
/* « global a, b » : l'exécuteur nous passe les noms un par un, puisque c'est
 * nous qui tenons les cadres. Même travail que l'ancien exécuteur, sans la
 * ligne à réanalyser. */
static int v3_globale(void *d, const char *nom)
{
    (void)d;
    if (!nom || !*nom) return 0;
    frame_declare_global(g_frame, nom);
    return 1;
}
/* Vider « the result » après une commande que l'exécuteur a traitée seul.
 * l'ancien exécuteur le faisait déjà pour put et l'arithmétique ; sans cela
 * la v3 laissait survivre un résultat déposé bien plus tôt. */
static void v3_resultat_vide(void *d)
{
    (void)d;
    set_result("");
}

static HctHote v3_hote(void)
{
    HctHote h;
    memset(&h, 0, sizeof h);
    h.lit_var   = v3_lit_var;
    h.ecrit_var = v3_ecrit_var;
    h.globale   = v3_globale;
    h.fonction  = v3_fonction;
    h.recours   = v3_recours;
    h.resout    = v3_resout;
    h.lit_objet = v3_lit_objet;
    h.lit_prop  = v3_lit_prop;
    h.commande  = v3_commande;
    h.respire   = v3_respire;
    h.ecrit_objet = v3_ecrit_objet;
    h.ecrit_message = v3_ecrit_message;
    h.resultat_vide = v3_resultat_vide;

    return h;
}

/* ==================== évaluation d'une expression ==================== *
 *
 * eval_expr est la porte par laquelle TOUT le noyau évalue une expression
 * écrite en texte : les commandes non portées, les désignateurs, les clés de
 * tri. Elle lexe, analyse et évalue par la v3.
 *
 * Une réserve par appel : l'arbre naît et meurt avec l'expression. Plus
 * coûteux qu'un arbre mis en cache, mais correct — on optimisera quand on
 * aura MESURÉ, pas avant.
 * ==================================================================== */

static void eval_expr(const char *s, char *out, int outlen)
{
    v1_compte("v3 relit", g_v1_porte);
    out[0] = '\0';
    if (!s || !*s) return;

    /* Rembobiner l'arène du noyau après l'évaluation.
     *
     * L'arène est une pile : ARENA_MARK retient le sommet, ARENA_FREE l'y
     * ramène. L'ancienne eval_checked le faisait, la v3 l'avait perdu — et
     * comme le recours appelle term_value, qui alloue par arena_buf, le
     * sommet montait sans jamais redescendre.
     *
     * À saturation, arena_buf rend g_apanic : un tampon STATIQUE PARTAGÉ.
     * Toutes les expressions suivantes écrivent alors au même endroit et
     * s'écrasent mutuellement. D'où des zéros là où l'on attendait des
     * pourcentages — et seulement dans les gestionnaires qui enchaînent
     * beaucoup d'évaluations, les essais isolés passant sans encombre. */
    ARENA_MARK;

    HctLot lot;
    HctReserve reserve;
    memset(&reserve, 0, sizeof reserve);

    hct_lex(s, &lot);

    HctAnalyseur a;
    hct_analyseur_init(&a, &lot, &reserve);
    HctNoeud *n = hct_expression(&a);

    HctContexte ctx;
    hct_ctx_init(&ctx, v3_hote());
    HctValeur v = hct_evalue(&ctx, n);

    if (ctx.erreur) {
        /* L'expression fautive et l'objet, pas seulement la colonne.
         *
         * « un nombre est attendu ici (colonne 5) » ne mène nulle part : on ne
         * sait ni ce qui était évalué, ni depuis quel script. Ces deux
         * renseignements sont ici sous la main — autant les donner.
         *
         * L'expression est tronquée : une expression peut faire des lignes, et
         * la console n'a pas à les recevoir en entier. */
        char apercu[80];
        snprintf(apercu, sizeof apercu, "%s", s);
        if (strlen(s) >= sizeof apercu - 1)
            snprintf(apercu + sizeof apercu - 4, 4, "...");

        char qui[128];
        qui[0] = '\0';
        if (g_me) hc_describe(g_me, qui, (int)sizeof qui);

        emit(HC_ERR, "   !! %s (colonne %d) dans « %s »%s%s",
             ctx.erreur,
             ctx.fautif ? ctx.fautif->jeton.col : 0,
             apercu,
             qui[0] ? " — objet : " : "",
             qui);
    } else {
        snprintf(out, (size_t)outlen, "%s", v.txt);
    }

    hct_val_libere(&v);
    hct_reserve_libere(&reserve);
    hct_lot_libere(&lot);

    ARENA_FREE;
}

static void eval_checked(const char *s, char *out, int outlen)
{
    /* La v3 consomme l'expression entière et pose elle-même un nœud d'erreur
     * sur ce qui reste : le contrôle séparé de l'ancienne version n'a plus
     * lieu d'être, et cette fonction devient un simple alias.
     *
     * Elle était restée branchée sur parse_expr après la greffe, si bien que
     * la moitié de l'exécution — dont « put », « get », les conditions de
     * « if » et les bornes de « repeat » — employait encore l'ancien
     * évaluateur. D'où des divergences invisibles : « item (3 mod 14) + 1 of
     * "13,11,22,14" » rendait 1 par « put » et 14 partout ailleurs. */
    eval_expr(s, out, outlen);
}



/* ==================== structures de contrôle ==================== */

/* Drapeaux de sortie, tous remis à zéro par hc_send. */
static int g_exit_handler = 0;   /* exit <gestionnaire> */
static int g_exit_repeat  = 0;   /* exit repeat */
static int g_next_repeat  = 0;   /* next repeat */

 


/* Cherche le mot `w` dans `s`, hors guillemets. Renvoie NULL sinon. */
static const char *find_kw(const char *s, const char *w)
{
    int inq = 0;
    size_t wl = strlen(w);
    for (const char *q = s; *q; q++) {
        if (*q == '"') { inq = !inq; continue; }
        if (inq) continue;
        if (q != s && !isspace((unsigned char)q[-1])) continue;
        if (ci_nequal(q, w, (int)wl)) {
            char c = q[wl];
            if (!c || isspace((unsigned char)c)) return q;
        }
    }
    return NULL;
}


/* Une ligne ouvre-t-elle un bloc ? (« if … then » sans suite, « repeat … ») */
static int opens_if(const char *s)
{
    if (!ci_word(s, "if")) return 0;
    const char *th = find_kw(s, "then");
    return th && !*skip_spaces(th + 4);
}

static int opens_repeat(const char *s)
{
    return ci_word(s, "repeat");
}

/* « else <instruction> » referme un if à lui seul : pas de « end if ».
   En revanche « else » seul, ou « else if … then » en bloc, ouvre une suite. */
static int else_closes(const char *s)
{
    if (!ci_word(s, "else")) return 0;
    const char *rest = skip_spaces(s + 4);
    if (!*rest) return 0;           /* « else » seul : le bloc continue */
    if (opens_if(rest)) return 0;   /* « else if … then » en bloc */
    return 1;
}

static int match_end(char **L, int from, int to, const char *what);

/* Formes hybrides « if … then <instruction> » + « else » à la ligne :
 * définies plus bas, mais les trois explorateurs ci-dessous doivent déjà
 * savoir les enjamber, sans quoi le « else » de l'if interne passe pour la
 * fin de l'if externe. */
static int is_inline_if(const char *s);
static int chain_end(char **L, int i, int to);

/* Vrai si L[i] ouvre une construction hybride qu'il faut enjamber d'un bloc. */
static int opens_inline_chain(char **L, int i, int to)
{
    const char *th;
    if (!is_inline_if(L[i])) return 0;

    /* Une ligne qui porte déjà son propre « else » est complète en elle-même :
     *     if the result <> empty then get error() else exit repeat
     *     else get error()          <- appartient à un if ENGLOBANT
     * Sans ce garde-fou on s'empare du « else » du dessous, et la branche
     * exécutée n'est pas celle que le script demande. */
    th = find_kw(L[i], "then");
    if (th && find_kw(th + 4, "else")) return 0;

    return i + 1 < to && ci_word(L[i+1], "else");
}

/* Index de la dernière ligne de la construction ouverte par L[open]. */
static int skip_block(char **L, int open, int to)
{
    if (opens_inline_chain(L, open, to)) return chain_end(L, open, to);
    if (opens_if(L[open]))     return match_end(L, open + 1, to, "if");
    if (opens_repeat(L[open])) return match_end(L, open + 1, to, "repeat");
    return open;
}

/* Index de la ligne fermante correspondante. Pour un « if », ce peut être
   « end if » ou l'« else <instruction> » qui le referme. Les constructions
   imbriquées sont sautées par récursion : plus de compteur de profondeur,
   qui ne survit pas à un bloc ayant deux fermetures possibles. */
static int match_end(char **L, int from, int to, const char *what)
{
    int isif = (strcmp(what, "if") == 0);
    for (int i = from; i < to; i++) {
        const char *s = L[i];
        if (opens_inline_chain(L, i, to) ||
            opens_if(s) || opens_repeat(s)) { i = skip_block(L, i, to); continue; }
        if (isif && else_closes(s)) return i;
        if (ci_word(s, "end")) {
            const char *w = skip_spaces(s + 3);
            if (isif  && ci_word(w, "if"))     return i;
            if (!isif && ci_word(w, "repeat")) return i;
        }
    }
    return to;
}





/* Un `if` dont le « then » porte déjà une instruction, mais dont le « else »
 * ouvre la ligne suivante. HyperCard admet cette forme hybride, et la chaîne :
 *
 *     if (it is in "1,3,5,7,8,10") or (it = 12) then return 31
 *     else if (it is in "4,6,9,11") then return 30
 *     else
 *       ...
 *     end if
 *
 * Ni opens_if (qui exige un « then » en fin de ligne) ni un exécuteur qui ne
 * regarde qu'une ligne ne savent la lire. On la traite en deux temps :
 * d'abord mesurer l'étendue de la construction, ensuite l'exécuter. */

/* Vrai si la ligne est « if <cond> then <instruction> », then non terminal. */
static int is_inline_if(const char *s)
{
    if (!ci_word(s, "if")) return 0;
    const char *th = find_kw(s, "then");
    return th && *skip_spaces(th + 4);
}

/* Indice de la dernière ligne de la construction ouverte en `i`. */
static int chain_end(char **L, int i, int to)
{
    int j = i;
    while (j + 1 < to && ci_word(L[j+1], "else")) {
        const char *rest = skip_spaces(L[j+1] + 4);
        if (!*rest)                                  /* « else » seul : bloc */
            return match_end(L, j + 2, to, "if");
        j++;
        if (!is_inline_if(rest)) break;              /* « else <instruction> » */
    }
    return j;
}



/* Découpe le corps d'un gestionnaire en lignes utiles, puis l'exécute. */


/* ---- tri ------------------------------------------------------------------
 *   sort [this] stack [ascending|descending] [text|numeric|dateTime] by <clé>
 *   sort [lines|items of] <conteneur> [sens] [style] [by <clé avec each>]
 *
 * Deux familles sous un même verbe, et la même mécanique dessous : pour chaque
 * élément on évalue une CLÉ, puis on trie sur ces clés.
 *
 * Ce qui change d'une famille à l'autre, c'est le contexte d'évaluation. Pour
 * les cartes, une clé comme « field "nom" » doit désigner le champ de LA carte
 * examinée — on déplace donc la carte courante le temps du calcul. Pour un
 * conteneur, la clé porte sur la variable `each`, qui vaut tour à tour chaque
 * ligne ou chaque item.
 *
 * Le tri est STABLE : deux éléments de clé égale gardent leur ordre d'origine.
 * HyperCard le garantissait, et des piles s'en servent pour trier sur deux
 * critères en triant deux fois, du moins important au plus important. */
/* SortStyle, SortItem, g_sort_desc, g_sort_style, sort_cmp et sort_options
 * ont migré avant la table V3_VERBES, juste après v3_point : v3_cmd_sort en
 * a besoin, et rien ici n'en dépendait plus tôt dans le fichier. Le tri
 * lui-même reste documenté ci-dessus ; seule sa mécanique a bougé. */

/* ---- effets de transition ------------------------------------------------
 *   visual [effect] <nom> [vitesse] [to <image>]
 *
 * « visual » ne dessine rien : elle ARME un effet qui se jouera au prochain
 * changement de carte, puis s'oublie. C'est ce qui permet d'écrire
 *
 *     visual effect dissolve slowly
 *     go to next card
 *
 * et non l'inverse. Un effet armé mais jamais suivi d'un « go » se perd sans
 * bruit, exactement comme dans HyperCard.
 *
 * Le noyau ne sait pas animer : il analyse, retient, et passe le tout à
 * l'hôte au moment du changement. Les noms restent ceux d'HyperCard, y compris
 * les composés en deux mots — « barn door open », « iris close ». */

/* ---- fichiers ouverts par script -----------------------------------------
 *   open file "notes"      read from file "notes" for 20
 *   write x to file "sortie"                     close file "notes"
 *
 * HyperCard désigne les fichiers par leur NOM, pas par un descripteur : c'est
 * « read from file "notes" » et non « read from handle 3 ». On tient donc une
 * petite table nom → FILE*, et chaque commande y retrouve son fichier.
 *
 * La position de lecture est celle du flux, sauf quand « at » l'impose. Un
 * « at » négatif compte depuis la FIN, ce qui permet de relire une queue de
 * fichier sans en connaître la taille.
 *
 * Le noyau s'en charge lui-même plutôt que de déléguer à l'hôte : fopen est du
 * C standard, et une pile qui importe des données doit fonctionner aussi bien
 * dans une version sans interface graphique. */
#define HC_MAX_FILES 8
/* `nom` est celui qu'emploie le SCRIPT, `chemin` celui où le fichier se trouve
 * vraiment. Les deux diffèrent dès que l'utilisateur a désigné le fichier dans
 * un dialogue : le script continue d'écrire « read from file "notes" », et
 * c'est bien ce nom-là qui doit le retrouver. */
static struct { char *nom; char *chemin; FILE *f; } g_files[HC_MAX_FILES];

static FILE *file_find(const char *nom)
{
    for (int i = 0; i < HC_MAX_FILES; i++) {
        if (!g_files[i].nom) continue;
        if (ci_equal(g_files[i].nom, nom)) return g_files[i].f;
        if (g_files[i].chemin && ci_equal(g_files[i].chemin, nom)) return g_files[i].f;
    }
    return NULL;
}

/* Ouvre en lecture-écriture, en créant au besoin : HyperCard n'a qu'un seul
 * « open file » pour les deux usages, et un script peut lire puis écrire dans
 * le même fichier sans le rouvrir.
 *
 * Si le nom ne mène à rien, on DEMANDE où se trouve le fichier plutôt que d'en
 * fabriquer un vide. C'est ce que faisait HyperCard, et c'est ce qui rend les
 * scripts d'époque utilisables : ils écrivent « open file "notes" », sans
 * chemin, en comptant sur le dialogue pour la suite. Sous le bac à sable de
 * macOS, c'est aussi la seule façon d'atteindre un fichier — le désigner vaut
 * autorisation. */
static int file_open(const char *nom)
{
    if (!nom || !*nom) return 0;
    if (file_find(nom)) return 1;              /* déjà ouvert : sans effet */
    int libre = -1;
    for (int i = 0; i < HC_MAX_FILES; i++)
        if (!g_files[i].nom) { libre = i; break; }
    if (libre < 0) return 0;

    FILE *f = fopen(nom, "r+b");

    /* Introuvable : deux cas bien distincts.
     *
     * Un nom PORTANT UN CHEMIN dit où l'on veut écrire — on crée le fichier,
     * sans rien demander. C'est ce qu'attend « ask file » suivi d'« open
     * file » : le panneau a déjà servi à choisir l'emplacement, et redemander
     * serait absurde.
     *
     * Un nom SEUL, en revanche, est celui d'un script d'époque qui écrit
     * « open file "notes" » et compte sur le dialogue pour la suite. On
     * demande alors où se trouve le fichier, comme HyperCard.
     *
     * L'ordre inverse — demander avant de créer — faisait réapparaître le
     * panneau juste après « ask file », pour un fichier qu'on venait de
     * nommer. */
    if (!f && strchr(nom, '/')) f = fopen(nom, "w+b");

    if (!f) {
        const char *reel = NULL;
        if (g_host && g_host->answer_file) {
            /* L'INVITE DOIT TENIR LE NOM ENTIER.
             *
             * Elle était composée dans un tampon de 256 octets. Un chemin
             * macOS va jusqu'à 1024, et un dossier peut à lui seul en prendre
             * 255 : la question « Où est le fichier « /Users/…/Docum » ? »
             * perdait précisément le renseignement qu'elle apportait.
             *
             * On mesure d'abord, on alloue seulement s'il le faut : le cas
             * courant — un nom court — ne touche pas au tas. Et si
             * l'allocation échoue, l'invite tronquée vaut mieux que pas
             * d'invite du tout ; on la pose, elle est seulement moins
             * bavarde. */
            static const char *FMT = "Où est le fichier « %s » ?";
            char  court[256];
            char *inv = court;
            char *dyn = NULL;
            int   besoin = snprintf(NULL, 0, FMT, nom);

            if (besoin >= (int)sizeof court) {
                dyn = malloc((size_t)besoin + 1);
                if (dyn) inv = dyn;
            }
            snprintf(inv, dyn ? (size_t)besoin + 1 : sizeof court, FMT, nom);
            reel = g_host->answer_file(inv);
            free(dyn);            /* `reel` appartient à l'hôte, pas à `inv` */
        }
        if (reel && *reel) {
            f = fopen(reel, "r+b");
            if (!f) f = fopen(reel, "w+b");
            if (f) g_files[libre].chemin = dupstr(reel);
        }
    }
    if (!f) return 0;

    g_files[libre].nom = dupstr(nom);
    g_files[libre].f   = f;
    return 1;
}

static void file_close(const char *nom)
{
    for (int i = 0; i < HC_MAX_FILES; i++) {
        if (!g_files[i].nom) continue;
        if (nom && !ci_equal(g_files[i].nom, nom) &&
            !(g_files[i].chemin && ci_equal(g_files[i].chemin, nom))) continue;
        fclose(g_files[i].f);
        free(g_files[i].nom);
        free(g_files[i].chemin);
        g_files[i].nom    = NULL;
        g_files[i].chemin = NULL;
        g_files[i].f      = NULL;
        if (nom) return;
    }
}

/* Traduit les constantes de caractère d'HyperTalk. Renvoie -1 si le mot n'en
 * est pas une, auquel cas c'est le premier caractère qui compte. */
static int file_constant(const char *s)
{
    if (ci_equal(s, "return"))   return '\n';
    if (ci_equal(s, "tab"))      return '\t';
    if (ci_equal(s, "space"))    return ' ';
    if (ci_equal(s, "quote"))    return '"';
    if (ci_equal(s, "formfeed")) return '\f';
    if (ci_equal(s, "linefeed")) return '\n';
    if (ci_equal(s, "end") || ci_equal(s, "eof")) return -2;   /* jusqu'au bout */
    return -1;
}
/* ── go … marked card ────────────────────────────────────────────────────────
 *
 * « go next marked card », « go first marked card », « go marked card 3 ».
 * Les cartes non marquées doivent être sautées comme si elles n'existaient
 * pas, et « next » boucle en fin de pile comme le fait « go next card ».
 *
 * Renvoie NULL si la référence ne parle pas de cartes marquées : resolve()
 * reprend alors la main, et rien du comportement existant ne bouge. */
static Object *marked_card_ref(const char *r, int *concerne)
{
    enum { REL_NONE, REL_NEXT, REL_PREV, REL_FIRST, REL_LAST, REL_ANY };
    *concerne = 0;
    int quoi = REL_NONE;
    const char *a = skip_spaces(r);

    if      (ci_word(a, "next"))     { quoi = REL_NEXT;  a = skip_spaces(a + 4); }
    else if (ci_word(a, "previous")) { quoi = REL_PREV;  a = skip_spaces(a + 8); }
    else if (ci_word(a, "prev"))     { quoi = REL_PREV;  a = skip_spaces(a + 4); }
    else if (ci_word(a, "first"))    { quoi = REL_FIRST; a = skip_spaces(a + 5); }
    else if (ci_word(a, "last"))     { quoi = REL_LAST;  a = skip_spaces(a + 4); }
    else if (ci_word(a, "any"))      { quoi = REL_ANY;   a = skip_spaces(a + 3); }

    if (!ci_word(a, "marked")) return NULL;
    a = skip_spaces(a + 6);
    if      (ci_word(a, "cards")) a = skip_spaces(a + 5);
    else if (ci_word(a, "card"))  a = skip_spaces(a + 4);
    else if (ci_word(a, "cds"))   a = skip_spaces(a + 3);
    else if (ci_word(a, "cd"))    a = skip_spaces(a + 2);
    else return NULL;             /* « marked » seul ne désigne pas une carte */

    /* Passé ce point, la référence parle bien de cartes marquées. Ne rien
     * trouver signifie alors qu'il n'y en a aucune, et « go » doit rester sur
     * place — surtout pas retomber sur resolve(), qui ne retiendrait que le
     * « next » et changerait de carte. */
    *concerne = 1;

    Object *pile = g_current_card ? g_current_card->owner : NULL;
    while (pile && pile->type != OBJ_STACK) pile = pile->owner;
    if (!pile) return NULL;

    int n = pile->nparts, ici = -1;
    for (int i = 0; i < n; i++)
        if (pile->parts[i] == g_current_card) { ici = i; break; }

    /* Relatif : on avance d'un cran à la fois depuis la carte courante, et
     * l'on fait au plus un tour complet avant d'abandonner. */
    if (quoi == REL_NEXT || quoi == REL_PREV) {
        int pas = (quoi == REL_NEXT) ? 1 : -1;
        for (int k = 1; k <= n; k++) {
            int i = ((ici + pas * k) % n + n) % n;
            Object *c = pile->parts[i];
            if (c->type == OBJ_CARD && c->marked) return c;
        }
        return NULL;
    }

    /* Absolu : le rang compte parmi les seules cartes marquées, la troisième
     * marquée pouvant très bien être la neuvième de la pile. */
    int m = 0;
    for (int i = 0; i < n; i++)
        if (pile->parts[i]->type == OBJ_CARD && pile->parts[i]->marked) m++;
    if (m == 0) return NULL;

    int rang = 1;
    if      (quoi == REL_LAST) rang = m;
    else if (quoi == REL_ANY)  rang = (rand() % m) + 1;
    else if (quoi == REL_NONE && *a) {
        char v[128]; double d = 0;
        eval_expr(a, v, sizeof v); as_num(v, &d);
        rang = (int)d;
    }
    if (rang < 1 || rang > m) return NULL;

    for (int i = 0; i < n; i++) {
        Object *c = pile->parts[i];
        if (c->type != OBJ_CARD || !c->marked) continue;
        if (--rang == 0) return c;
    }
    return NULL;
}


/* ==================== envoi d'un message ==================== */

/* Envoie un message accompagné d'une liste d'arguments déjà évalués.
   argv[0..argc-1] sont les valeurs des arguments (sans le nom du message). */
static int hc_send_args_k_body(Object *target, const char *message,
                          char argv[][HC_VAL], int argc, int isfunc)
{
    if (g_depth >= HC_MAX_DEPTH) {
        emit(HC_ERR, "!! trop de récursion : message \"%s\" abandonné", message);
        return 0;
    }

    /* TROP D'ARGUMENTS : refusé ICI, et nulle part ailleurs.
     *
     * La table des paramètres en tient quinze. Les appelants se contentaient
     * de s'arrêter à ce nombre, donc un seizième argument disparaissait en
     * silence — et chacun d'eux devait y penser, ce qui faisait autant de
     * vérités sur la limite qu'il y a de chemins d'appel. L'appel de fonction
     * refusait bien ; le message et « send » tronquaient.
     *
     * Le contrôle est donc au point de passage obligé. Aucun appelant, présent
     * ou futur, ne peut plus l'oublier : il lui suffit de compter ses
     * arguments honnêtement et de laisser passer le compte. */
    if (argc > HC_ARGS_MAX) {
        set_result("Too many arguments");
        emit(HC_ERR, "   !! %s : trop d'arguments (%d, %d au plus)",
             message, argc, HC_ARGS_MAX);
        return 1;                       /* traité : l'appel a échoué */
    }

    /* Quatre maillons pour l'objet, la carte, le fond et la pile ; le reste
     * pour les piles en usage, qui viennent après. Une chaîne trop courte les
     * écarterait silencieusement. */
    Object *chain[4 + HC_MAX_USING];
    int n = build_chain(target, chain, (int)(sizeof chain / sizeof *chain));

    /* Sauver les paramètres AVANT de modifier le moindre global. Auparavant
     * on réservait 16 Mo même quand l'appelant n'avait qu'un paramètre ; on
     * réserve maintenant exactement ce qui est vivant. Si l'arène refuse,
     * aucun état global n'a encore changé. */
    int saved_nparams = g_nparams;
    char (*saved_params)[HC_VAL] =
        saved_nparams ? arena_rows(saved_nparams) : NULL;
    if (saved_nparams && !saved_params) return 0;
    for (int i = 0; i < saved_nparams; i++)
        memcpy(saved_params[i], g_params[i], sizeof saved_params[i]);

    /* `the target` vaut le destinataire initial pendant toute la remontée ;
       on empile l'ancien pour les envois imbriqués. */
    int saved_clipped = g_script_clipped;
    g_script_clipped = 0;
    Object *saved_target = g_target;
    Object *saved_me     = g_me;
    g_target = target;

    /* g_params[0] = nom du message, puis les arguments */
    snprintf(g_params[0], sizeof g_params[0], "%s", message);
    g_nparams = 1;
    for (int i = 0; i < argc && g_nparams <= HC_ARGS_MAX; i++)
        snprintf(g_params[g_nparams++], sizeof g_params[0], "%s", argv[i]);

    if (g_trace) {
        char d[64]; hc_describe(target, d, sizeof d);
        emit(HC_TRACE, "→ message \"%s\" à %s", message, d);
    }

    g_depth++;

    int handled = 0;
    for (int i = 0; i < n; i++) {
        Object *o = chain[i];
        const char *end = NULL, *hdr = NULL;
        const char *body = find_handler_k(o->script, message, isfunc, &end, &hdr);

        if (body) {
            if (g_trace) {
                char d[64]; hc_describe(o, d, sizeof d);
                emit(HC_TRACE, "· traité par %s", d);
            }
            /* chaque gestionnaire a ses propres variables locales */
            Frame  frame;   memset(&frame, 0, sizeof frame);
            Frame *savedf = g_frame;
            g_frame = &frame;

            /* lier les paramètres formels de l'en-tête aux arguments :
               « on carre n » → la variable locale n reçoit argv[0]. */
            if (hdr) {
                char pname[64];
                /* sauter le mot-clé (« on » / « function ») puis le nom :
                 * un décalage fixe casserait dès qu'on change de mot-clé. */
                const char *q = next_word(hdr, pname, sizeof pname);
                q = next_word(q, pname, sizeof pname);
                int idx = 0;
                for (;;) {
                    q = skip_spaces(q);
                    if (*q == ',') { q++; continue; }
                    if (!*q || *q == '\n') break;
                    /* Lire le nom sans next_word : celui-ci ne s'arrête qu'aux
                     * blancs et avalerait la virgule dans « on markToday
                     * theNewDay,theOldDay » — forme sans espace qu'emploient
                     * tous les scripts d'origine. Aucun paramètre n'était
                     * alors lié. */
                    int pk = 0;
                    while (*q && *q != ',' && *q != '\n' && !isspace((unsigned char)*q)
                           && pk < (int)sizeof pname - 1)
                        pname[pk++] = *q++;
                    pname[pk] = '\0';
                    if (!pname[0]) break;
                    var_set(pname, idx < argc ? argv[idx] : "");
                    idx++;
                }
            }

            g_pass = 0;
            g_exit_handler = g_exit_repeat = g_next_repeat = 0;
            g_me   = o;          /* `me` = l'objet dont le script tourne */
            /* La v3 d'abord si elle est active ET si elle a su analyser ce
             * script. Sinon l'ancien exécuteur, inchangé. */
            /* La v3 d'abord ; à défaut, le gestionnaire ENTIER retombe sur
             * l'ancien exécuteur. C'est de loin le plus gros emprunt, et il
             * ne se voyait nulle part : le relevé des recours reste « aucun »
             * puisque la v3 n'a même pas commencé.
             *
             * La porte nomme donc l'objet et le message — « v1 ligne
             * button "Tracer".mouseUp » désigne le script à regarder, là où
             * un total anonyme n'apprenait rien. Le tampon est LOCAL : un
             * gestionnaire peut en appeler un autre, et un tampon partagé se
             * ferait écraser par l'appel imbriqué. */
            char porte[96];
            {
                char qui[64];
                hc_describe(o, qui, sizeof qui);
                snprintf(porte, sizeof porte, "%s.%s", qui, message);
            }
            const char *sauve_v1 = v1_porte(porte);
            if (!v3_execute(o, message, isfunc)) {
                /* PORTE 1 vers l'ancien exécuteur : un gestionnaire que la v3
                 * refuse — en-tête illisible, « end » manquant. HyperCard ne
                 * l'exécutait pas davantage : il refusait d'enregistrer un
                 * script fautif. Le confier à un moteur plus permissif, qui en
                 * devinerait la moitié, est pire que de le dire. */
                v1_compte("v1 refusée", porte);
                emit(HC_ERR, "   !! gestionnaire illisible : %s "
                             "(son en-tête ou son « end »)", porte);
            }
            g_v1_porte = sauve_v1;
            g_exit_handler = g_exit_repeat = g_next_repeat = 0;

            g_frame = savedf;
            frame_clear(&frame);

            /* « pass » veut dire JE NE L'AI PAS TRAITÉ : le message repart vers
             * le maillon suivant, et s'il n'en reste aucun il revient à
             * HyperCard lui-même, qui applique le comportement par défaut.
             *
             * handled était posé AVANT ce test, si bien qu'un gestionnaire qui
             * passait comptait quand même comme preneur. L'appelant ne pouvait
             * donc pas distinguer « la pile s'en est chargée » de « personne
             * n'en a voulu » — exactement ce dont doMenu a besoin pour savoir
             * s'il doit exécuter l'article de menu. */
            if (g_pass) { g_pass = 0; continue; }
            handled = 1;
            break;
        } else if (g_trace) {
            char d[64]; hc_describe(o, d, sizeof d);
            emit(HC_TRACE, "  (pas de gestionnaire dans %s)", d);
        }
    }

    if (!handled && g_trace) {
        /* Muet pour une recherche de fonction : ne pas trouver « numLines »
         * est le cas normal quand le nom est en fait une variable ou un
         * littéral. Seul un message resté sans preneur mérite la trace. */
        if (!isfunc)
            emit(HC_TRACE, "  ✗ message \"%s\" non traité", message);
    }

    g_depth--;
    g_me     = saved_me;
    g_target = saved_target;
    g_script_clipped = saved_clipped;

    /* LE GESTIONNAIRE LE PLUS EXTÉRIEUR SE TERMINE : ON AVERTIT. */
    if (g_depth == 0) erreurs_vide();

    /* dépiler les paramètres de l'appelant */
    for (int i = 0; i < saved_nparams; i++)
        memcpy(g_params[i], saved_params[i], sizeof g_params[i]);
    g_nparams = saved_nparams;

    return handled;
}

/* Frontière de message : chaque envoi rend ses tampons en sortant. Sans cela
 * une pile qui envoie des milliers de messages verrait l'arène croître sans
 * fin, puisque seul parse_factor libère en dessous. */
static int hc_send_args_k(Object *target, const char *message,
                          char argv[][HC_VAL], int argc, int isfunc)
{
    ARENA_MARK;
    int r = hc_send_args_k_body(target, message, argv, argc, isfunc);
    ARENA_FREE;

    /* DÉVERROUILLAGE AUTOMATIQUE en retombant au repos.
     *
     * HyperCard déverrouille l'écran de lui-même dès qu'il a fini de traiter
     * un message : « lock screen » ne vaut que pour le gestionnaire en cours.
     * Sans cette remise à zéro, un gestionnaire qui verrouille puis sort avant
     * son « unlock screen » — un `exit`, une erreur, une branche oubliée —
     * laissait l'écran verrouillé POUR TOUJOURS. Plus aucun champ ne se
     * rafraîchissait, et rien ne disait pourquoi.
     *
     * Uniquement au niveau le plus extérieur : un gestionnaire qui en appelle
     * un autre doit garder son verrou pendant l'appel. */
    if (g_depth == 0 && g_ecran_verrouille) {
        g_ecran_verrouille = 0;
        verrou_reveille();
        host_global_set("lockScreen", "false");
    }
    /* Même règle pour « lock messages », et pour la même raison : un
     * gestionnaire qui verrouille puis sort avant son « unlock messages »
     * laisserait la pile MUETTE pour toujours — plus un seul openCard, et
     * rien pour dire pourquoi. */
    if (g_depth == 0) g_messages_verrouilles = 0;

    /* Et pour lockErrorDialogs. La remise à zéro vient APRÈS erreurs_vide,
     * qui s'exécute à la fin du corps : le détournement doit encore valoir
     * pour l'erreur qui termine le gestionnaire — c'est même le cas
     * principal. HypoGraph pose la serrure en tête de son « on mouseUp » et
     * ne la retire jamais : sans cette ligne, tout le reste de la session
     * partirait en errorDialog. */
    if (g_depth == 0) reglage_eteint("lockerrordialogs");

    /* Une commande « delete this card » peut avoir détaché l'objet dont le
     * gestionnaire vient juste de finir. C'est seulement ici que plus aucun
     * code du message n'a besoin de lui. Une suppression C extérieure garde
     * son propre garde actif et repoussera encore ce nettoyage. */
    libere_differees();
    return r;
}

static int hc_send_args(Object *target, const char *message,
                        char argv[][HC_VAL], int argc)
{
    return hc_send_args_k(target, message, argv, argc, 0);
}

/* Appel d'une fonction utilisateur : même remontée de la chaîne, mais on
 * cherche « function <nom> ». La valeur est déposée dans `the result` par
 * l'instruction `return` ; on la vide d'abord pour qu'une fonction sans
 * `return` rende bien la chaîne vide plutôt que le reliquat de l'appel
 * précédent. */
static int hc_call_user_function(Object *target, const char *name,
                                 char argv[][HC_VAL], int argc)
{
    if (!target) return 0;
    set_result("");
    return hc_send_args_k(target, name, argv, argc, 1);
}

/* ═══ doMenu ════════════════════════════════════════════════════════════
 *
 * Dans HyperCard, choisir un article de menu ENVOIE d'abord le message
 * « doMenu <article> » à la carte courante. Le comportement natif n'a lieu
 * que si personne ne l'intercepte, ou si un gestionnaire le rend par
 * « pass doMenu ». C'est de cette façon qu'une pile détourne un article :
 *
 *     on doMenu quoi
 *       if quoi is "Clear Picture" then effaceProprement
 *       else pass doMenu
 *     end doMenu
 *
 * HC exécutait l'article DIRECTEMENT, sans jamais envoyer le message : aucun
 * « on doMenu » d'une pile d'époque ne se déclenchait, en silence.
 *
 * La récursion reste possible — un gestionnaire qui rappelle doMenu pour le
 * même article se rappelle lui-même — mais c'est le comportement d'HyperCard,
 * et le garde-fou de profondeur (HC_MAX_DEPTH) l'arrête avec un message clair
 * plutôt que d'inventer une règle qu'HyperCard n'avait pas. */
/* Les articles standards qu'HyperCard exécutait lui-même et que le NOYAU sait
 * faire sans rien demander à l'hôte : le menu Aller.
 *
 * Une pile d'époque écrit « doMenu \"Next\" » aussi naturellement que
 * « go next ». Sans cette table, l'article partait à l'hôte, qui ne connaît
 * que ses propres menus — en français, et sans menu Aller : il ne se passait
 * rien du tout, sans un mot.
 *
 * On passe par une LIGNE DE HYPERTALK plutôt que de refaire le changement de
 * carte à la main : la séquence closeCard / closeBackground / openBackground /
 * openCard est écrite en toutes lettres à vingt-quatre endroits de ce fichier,
 * et en ajouter un vingt-cinquième exemplaire serait absurde.
 *
 * « Back » et « Home » n'y sont pas parce que « go back » et « go home »
 * n'existent pas : les inscrire ne ferait que déplacer le silence d'un cran.
 * Il y faudrait d'abord un historique de navigation. */
/* La forme longue — « go next CARD » et non « go next ».
 *
 * Les deux marchent, mais elles ne se lisent pas pareil : « go next » laisse
 * l'analyseur sur un simple nom, qu'il faut alors évaluer comme expression,
 * ce qui finit en recours vers l'ancien interprète. « go next card » donne
 * une référence d'objet en bonne et due forme, que la v3 traite seule. Le
 * relevé l'a dit dès que ces lignes sont passées à la v3 : « fonction next »
 * apparaissait là où il n'y avait rien avant. */
static const struct { const char *article; const char *ligne; } MENUS_NOYAU[] = {
    { "Next",     "go next card"  },
    { "Prev",     "go prev card"  },
    { "Previous", "go prev card"  },
    { "First",    "go first card" },
    { "Last",     "go last card"  },
    /* Back suit l'historique de navigation, comme dans HyperCard. « doMenu
     * "Back" » depuis un script y arrive donc aussi, et un « on doMenu » de
     * la pile peut le détourner — c'est tout l'intérêt de passer par ici
     * plutôt que d'appeler hc_go_back depuis l'interface. */
    { "Back",     "go back"       },
    { NULL, NULL }
};

/* Comparaison d'un nom d'article, à la tolérance près qui sépare ce qu'écrit
 * un script de ce qu'affiche un menu : la casse, et les points de suspension
 * finaux. HyperCard affiche « Find… » avec le vrai caractère « … » ; les
 * scripts écrivent aussi bien « Find… » que « Find... » ou « Find ». Les trois
 * doivent désigner le même article. */
static int menu_meme_article(const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    while (la && (a[la-1] == '.' || a[la-1] == ' ')) la--;
    while (lb && (b[lb-1] == '.' || b[lb-1] == ' ')) lb--;
    /* « … » en UTF-8 : E2 80 A6 */
    while (la >= 3 && (unsigned char)a[la-3] == 0xE2 &&
           (unsigned char)a[la-2] == 0x80 && (unsigned char)a[la-1] == 0xA6) {
        la -= 3;
        while (la && a[la-1] == ' ') la--;
    }
    while (lb >= 3 && (unsigned char)b[lb-3] == 0xE2 &&
           (unsigned char)b[lb-2] == 0x80 && (unsigned char)b[lb-1] == 0xA6) {
        lb -= 3;
        while (lb && b[lb-1] == ' ') lb--;
    }
    return la == lb && strncasecmp(a, b, la) == 0;
}

/* Proposer un article à la pile, sans rien exécuter ensuite.
 *
 * C'est la moitié « message » de hc_do_menu, isolée pour l'interface : quand
 * l'utilisateur CLIQUE un article, l'action native est déjà écrite et sait se
 * faire toute seule ; il ne lui manque que de demander d'abord à la pile si
 * elle veut s'en charger. Rend 1 si un gestionnaire l'a pris — l'appelant n'a
 * alors plus rien à faire. */
/* Envoyer un message accompagné d'UN argument.
 *
 * hc_send n'en accepte aucun, hc_send_args est interne : l'hôte n'avait donc
 * aucun moyen d'envoyer « arrowKey left » ou « functionKey 3 », alors que ce
 * sont précisément les messages du clavier, tous porteurs d'un argument.
 * Rend 1 si un gestionnaire l'a pris — un « pass » ne compte pas, voir
 * hc_send_args_k_body. */
int hc_send_arg(Object *target, const char *message, const char *arg)
{
    if (!target || !message) return 0;

    ARENA_MARK;
    char (*argv)[HC_VAL] = NULL;
    if (arg) {
        argv = arena_rows(1);
        if (!argv) { ARENA_FREE; return 0; }
        snprintf(argv[0], HC_VAL, "%s", arg);
    }
    int pris = hc_send_args(target, message, argv, arg ? 1 : 0);
    ARENA_FREE;
    return pris;
}

/* L'utilisateur a choisi un article d'un menu de pile.
 *
 * S'il porte un message, c'est LUI qui part, et non doMenu : « Curves »
 * envoie « goCurves ». Sinon on retombe sur doMenu, ce qui laisse un menu
 * construit sans messages se comporter comme les menus de l'application.
 *
 * On passe par hc_do plutôt que par hc_send : le message d'un article peut
 * porter des arguments (« markCard 3 »), et hc_do sait analyser une ligne
 * là où hc_send_arg n'accepte qu'un nom. Sa remise à zéro de la profondeur
 * est ici exacte : un choix de menu est une action de l'utilisateur, au
 * repos, exactement comme une ligne tapée dans la boîte de message. */
void hc_menu_choisi(int i, int j)
{
    if (i < 0 || i >= g_nmenus) return;
    HcMenuBarre *m = &g_menus[i];
    if (j < 0 || j >= m->n) return;
    if (!m->actif_menu || !m->actif[j]) return;

    const char *art = m->article[j];
    if (!art || !*art || !strcmp(art, "-")) return;   /* un séparateur */

    if (m->message[j] && *m->message[j]) { hc_do(m->message[j]); return; }
    hc_do_menu(art);
}

int hc_menu_trappe(const char *item)
{
    if (!item) return 0;

    g_visual_dirty = 1;               /* touche à l'écran : voir v3_respire */

    return g_current_card ? hc_send_arg(g_current_card, "doMenu", item) : 0;
}

/* ═══ Les messages du cycle de vie ═════════════════════════════════════
 *
 * startUp, quit, suspend, resume : les quatre que HyperCard envoyait à
 * l'ENVIRONNEMENT et non à une pile en particulier. Ils partent donc à la
 * carte courante, d'où ils remontent la hiérarchie jusqu'à la pile — c'est
 * ce que faisait HyperCard, qui les adressait à la pile Home, c'est-à-dire à
 * celle où l'on se trouve.
 *
 * suspendStack et resumeStack, eux, désignent une pile précise et vivent
 * dans Hcdocument.m, où l'on sait quelle fenêtre gagne ou perd le premier
 * plan.
 *
 * hc_send et non hc_send_systeme : « lock messages » sert à parcourir une
 * pile sans réveiller les gestionnaires de chaque carte. Aucun de ces quatre
 * ne survient pendant un parcours — ils viennent du système : un lancement,
 * une extinction, un changement d'application. Les retenir n'épargnerait
 * rien et masquerait un départ.
 *
 * Sans carte courante, rien : il n'y a personne à qui parler, et ce n'est pas
 * une erreur — l'application peut tourner sans pile ouverte. */
void hc_env_message(const char *message)
{
    if (!message || !g_current_card) return;
    hc_send(g_current_card, message);
}

void hc_do_menu(const char *item)
{
    if (!item) return;

    if (hc_menu_trappe(item)) return; /* la pile s'en est chargée */

    for (int i = 0; MENUS_NOYAU[i].article; i++)
        if (menu_meme_article(MENUS_NOYAU[i].article, item)) {
            /* « go next card » et ses quatre voisines. La v3 d'abord, comme
             * pour la boîte de message ; l'ancien ne sert plus que de repli.
             *
             * On pose `me` avant : l'ancien exécuteur le recevait en
             * argument, la v3
             * le lit dans g_me par l'hôte. Sans cette ligne, un « go next »
             * déclenché depuis un menu n'aurait plus le même `me` qu'avant —
             * la sorte de différence qui ne se voit qu'un mois plus tard,
             * dans un script qui lit « the short name of me ». */
            const char *sauve = v1_porte("menu du noyau");
            Object *cible    = g_me ? g_me : g_current_card;
            Object *sauve_me = g_me;
            g_me = cible;
            if (!v3_do_ligne(MENUS_NOYAU[i].ligne))
                {
                    /* PORTE 3 : l'article de menu du noyau, si la v3 n'a pas
                     * su lire sa ligne. Mesuré : sur 138 harnais, elle n'a
                     * jamais servi — les cinq lignes de MENUS_NOYAU sont du
                     * HyperTalk que la v3 lit sans peine. */
                    v1_compte("v1 refusée", "menu du noyau");
                    emit(HC_ERR, "   !! article de menu non compris : %s",
                         MENUS_NOYAU[i].ligne);
                }
            g_me = sauve_me;
            g_v1_porte = sauve;
            return;
        }

    if (g_host && g_host->do_menu) g_host->do_menu(item);
    else emit(HC_ERR, "   !! doMenu : l'hôte ne gère pas les menus");
}

int hc_send(Object *target, const char *message)
{
    ARENA_MARK;
    int r = hc_send_args(target, message, NULL, 0);
    ARENA_FREE;
    return r;
}

/* ==================== boîte de message ==================== */

Object *hc_resolve(const char *ref) { return resolve(ref); }

const char *hc_script_of(Object *o) { return o ? o->script : NULL; }

/* Un champ de fond non partagé a un texte propre à chaque carte. */
static int field_is_percard(Object *field)
{
    return field && field->type == OBJ_FIELD
        && field->owner && field->owner->type == OBJ_BACKGROUND
        && !field->shared_text;
}

/* Plage du dernier « find » : renvoie 1 si ce champ, sur cette carte, porte
 * le texte trouve, et remplit start et len. */
int hc_found_range(Object *field, int *start, int *len)
{
    if (!g_found_montre) return 0;
    if (!field || field != g_found_field || g_found_len <= 0) return 0;
    if (g_found_card && g_found_card != g_current_card) return 0;
    if (start) *start = g_found_start;
    if (len)   *len   = g_found_len;
    return 1;
}

/* Retirer l'encadré du texte trouvé, comme HyperCard au premier clic.
 *
 * Ne touche PAS à « the foundChunk » ni à « the foundText » : voir
 * g_found_montre. Rend 1 si quelque chose était montré — l'hôte sait alors
 * qu'il doit redessiner, et seulement alors. */
int hc_found_cache(void)
{
    if (!g_found_montre) return 0;
    g_found_montre = 0;
    return 1;
}

const char *hc_field_text(Object *field)
{
    if (!field) return "";
    if (field_is_percard(field) && g_current_card) {
        Object *cd = g_current_card;
        for (int i = 0; i < cd->nbgtexts; i++)
            if (cd->bgtexts[i].field_id == field->id)
                return cd->bgtexts[i].text ? cd->bgtexts[i].text : "";
        return "";                       /* pas encore rempli sur cette carte */
    }
    return field->contents ? field->contents : "";
}

void hc_set_field_text(Object *field, const char *text)
{
    /* Recalage des plages de style. Si container_set a noté un intervalle pour
     * CE champ, l'écriture portait sur un morceau et les plages se recalent ;
     * sinon elle porte sur le champ entier et elles sont détruites — c'est la
     * règle du remplacement complet, observée dans HyperCard 2.4. */
    if (field && field->type == OBJ_FIELD) {
        struct RunList *rl = runs_of(field);
        if (rl) {
            if (g_edit_fld == field && g_edit_at >= 0)
                runs_edit(rl, g_edit_at, g_edit_old, g_edit_new);
            else
                runs_free(rl);
        }
    }
    g_edit_fld = NULL; g_edit_at = -1;

    if (!field || (field->type != OBJ_FIELD && field->type != OBJ_BUTTON)) return;

    if (field_is_percard(field) && g_current_card) {
        Object *cd = g_current_card;
        for (int i = 0; i < cd->nbgtexts; i++)
            if (cd->bgtexts[i].field_id == field->id) {
                free(cd->bgtexts[i].text);
                cd->bgtexts[i].text = dupstr(text ? text : "");
                return;
            }
        if (cd->nbgtexts == cd->capbgtexts) {
            int cap = cd->capbgtexts ? cd->capbgtexts * 2 : 4;
            struct BgText *p = realloc(cd->bgtexts, (size_t)cap * sizeof *p);
            if (!p) return;
            cd->bgtexts = p;
            cd->capbgtexts = cap;
        }
        cd->bgtexts[cd->nbgtexts].field_id = field->id;
        cd->bgtexts[cd->nbgtexts].text = dupstr(text ? text : "");
        /* realloc ne nettoie rien : sans ce memset la liste de plages
         * démarrerait sur un pointeur bidon, et le premier hc_run_add
         * (ou hc_free) partirait dessus. */
        memset(&cd->bgtexts[cd->nbgtexts].runs, 0,
               sizeof cd->bgtexts[cd->nbgtexts].runs);
        cd->nbgtexts++;
        return;
    }

    free(field->contents);
    field->contents = dupstr(text ? text : "");
}

/* Bascule « Shared Text », en emportant le contenu.
 *
 * Un champ de fond non partagé range son texte ET SES PLAGES DE STYLE dans la
 * carte ; partagé, il les range dans l'objet. Basculer le seul drapeau faisait
 * donc lire un magasin vide : les plages n'étaient pas détruites, elles
 * devenaient inaccessibles — et la couleur qu'on venait de poser disparaissait
 * sans un mot.
 *
 * On déménage donc, dans le sens de la bascule. Sans effet si le drapeau ne
 * change pas, ou si le champ n'appartient pas à un fond.
 *
 * Une limite assumée dans le sens « devient partagé » : seule la carte
 * COURANTE fournit le texte retenu. Les autres cartes gardent le leur en
 * réserve, invisible tant que le champ reste partagé, et le retrouvent si l'on
 * décoche. C'est ce que faisait HyperCard, et c'est moins destructeur que de
 * choisir à la place de l'utilisateur laquelle des cartes fait foi. */
void hc_set_shared_text(Object *field, int shared)
{
    if (!field || field->type != OBJ_FIELD) return;
    if (!field->owner || field->owner->type != OBJ_BACKGROUND) {
        field->shared_text = shared ? 1 : 0;
        return;
    }
    if (!!field->shared_text == !!shared) return;      /* rien ne change */

    Object *cd = g_current_card;

    if (shared) {
        /* La carte courante fournit ce qui devient le contenu partagé.
         *
         * À défaut, la PREMIÈRE carte du fond qui en a un. Sans ce repli, un
         * champ dont une seule carte porte le texte passait en partagé avec
         * son ancien contenu — souvent une plage périmée — et la mise en forme
         * paraissait détruite alors qu'elle dormait dans une autre carte.
         *
         * C'est un choix par défaut, pas une certitude : si plusieurs cartes
         * ont un texte, celle qu'on affiche l'emporte, et à défaut la première
         * rencontrée. Les autres gardent le leur en réserve et le retrouvent
         * si l'on décoche. */
        if (cd) {
            int a_entree = 0;
            for (int i = 0; i < cd->nbgtexts && !a_entree; i++)
                if (cd->bgtexts[i].field_id == field->id) a_entree = 1;

            if (!a_entree && field->owner && field->owner->owner) {
                Object *pile = field->owner->owner;
                for (int k = 0; k < pile->nparts && !a_entree; k++) {
                    Object *autre = pile->parts[k];
                    if (autre->type != OBJ_CARD || autre->bg != field->owner) continue;
                    for (int i = 0; i < autre->nbgtexts; i++)
                        if (autre->bgtexts[i].field_id == field->id) {
                            cd = autre; a_entree = 1; break;
                        }
                }
            }
        }

        if (cd) {
            for (int i = 0; i < cd->nbgtexts; i++) {
                if (cd->bgtexts[i].field_id != field->id) continue;

                free(field->contents);
                field->contents = dupstr(cd->bgtexts[i].text ? cd->bgtexts[i].text : "");

                runs_free(&field->runs);
                struct RunList *sr = &cd->bgtexts[i].runs;
                if (sr->n > 0 && runs_room(&field->runs, sr->n)) {
                    for (int k = 0; k < sr->n; k++) {
                        field->runs.v[k]      = sr->v[k];
                        field->runs.v[k].font = dupstr(sr->v[k].font);
                    }
                    field->runs.n = sr->n;
                }
                break;
            }
        }
        field->shared_text = 1;
    } else {
        /* Le contenu partagé descend dans la carte courante, pour qu'elle
         * garde à l'écran ce qu'elle affichait à l'instant. */
        field->shared_text = 0;
        if (cd) {
            hc_set_field_text(field, field->contents ? field->contents : "");

            for (int i = 0; i < cd->nbgtexts; i++) {
                if (cd->bgtexts[i].field_id != field->id) continue;

                runs_free(&cd->bgtexts[i].runs);
                struct RunList *sr = &field->runs;
                if (sr->n > 0 && runs_room(&cd->bgtexts[i].runs, sr->n)) {
                    for (int k = 0; k < sr->n; k++) {
                        cd->bgtexts[i].runs.v[k]      = sr->v[k];
                        cd->bgtexts[i].runs.v[k].font = dupstr(sr->v[k].font);
                    }
                    cd->bgtexts[i].runs.n = sr->n;
                }
                break;
            }
        }
    }
    notify_field(field);
}

/* ---- Plages de style : lecture par l'hote ---- */
int hc_run_count(Object *field)
{
    struct RunList *rl = runs_of(field);
    return rl ? rl->n : 0;
}

/* Les sentinelles ne sortent jamais du noyau : l'hôte reçoit des valeurs
 * effectives, sinon HC_STYLE_INHERIT (-2) allumerait, bit à bit, l'italique,
 * le souligné et tout le reste au premier « & HC_ITALIC » de la vue. */
int hc_run_attrs_color(Object *field, int i, int *start, int *len,
                       int *style, int *size, const char **font, int *color)
{
    struct RunList *rl = runs_of(field);
    if (!rl || i < 0 || i >= rl->n) return 0;
    struct TextRun *r = &rl->v[i];
    if (start) *start = r->start;
    if (len)   *len   = r->len;
    if (style) *style = (r->style == HC_STYLE_INHERIT) ? field->textstyle
                                                       : r->style;
    if (size)  *size  = r->size ? r->size : field->textsize;
    if (font)  *font  = r->font ? r->font : field->textfont;
    /* La couleur garde sa sentinelle, contrairement aux trois autres : le
     * champ n'a pas de couleur propre où se rabattre, et c'est à l'hôte de
     * décider ce que « pas de couleur » veut dire — du noir, d'ordinaire. */
    if (color) *color = r->color;
    return 1;
}

int hc_run_attrs(Object *field, int i, int *start, int *len,
                 int *style, int *size, const char **font)
{
    return hc_run_attrs_color(field, i, start, len, style, size, font, NULL);
}

int hc_run_at(Object *field, int i, int *start, int *len, int *style)
{
    return hc_run_attrs(field, i, start, len, style, NULL, NULL);
}

void hc_runs_clear(Object *field)
{
    struct RunList *rl = runs_of(field);
    if (rl) runs_free(rl);
}

int hc_run_add_full(Object *field, int start, int len,
                    int style, int size, const char *font)
{
    return hc_run_add_color(field, start, len, style, size, font,
                            HC_COLOR_INHERIT);
}

int hc_run_add_color(Object *field, int start, int len,
                     int style, int size, const char *font, int color)
{
    struct RunList *rl = runs_of(field);
    if (!rl || len <= 0 || start < 0) return 0;

    /* La vue passe des valeurs effectives ; celles qui coïncident avec le
     * champ redeviennent des sentinelles, sinon un simple gras figerait au
     * passage la police du champ dans chaque plage. */
    int fsz = field->textsize;
    const char *ffn = field->textfont;
    if (size == fsz) size = 0;
    if (font && ffn && strcmp(font, ffn) == 0) font = NULL;
    if (font && !*font) font = NULL;

    if (style == 0 && size == 0 && !font && color == HC_COLOR_INHERIT)
        return 0;                                     /* rien à dire */

    if (!runs_room(rl, 1)) return 0;
    struct TextRun n;
    n.start = start; n.len = len; n.style = style; n.size = size;
    n.font  = font ? dupstr(font) : NULL;
    n.color = color;
    rl->v[rl->n++] = n;
    runs_tidy(rl);
    return 1;
}

int hc_run_add(Object *field, int start, int len, int style)
{
    return hc_run_add_full(field, start, len, style, 0, NULL);
}

const char *hc_paint_of(Object *o)
{
    return o ? o->paint : NULL;
}

void hc_set_paint(Object *o, const char *base64)
{
    if (!o) return;
    if (o->type != OBJ_CARD && o->type != OBJ_BACKGROUND) return;
    free(o->paint);
    o->paint = (base64 && *base64) ? dupstr(base64) : NULL;
}

/* ═══ Une ligne isolée, exécutée par la v3 ══════════════════════════════
 *
 * La boîte de message, la commande « do », et la répartition des articles de
 * menu que les piles créent : trois usages, un seul point d'entrée, et le
 * plus fréquenté de tous ceux qui restaient sur l'ancien interprète.
 *
 * Le portage est simple parce que le rattrapage est déjà là : ce que la v3
 * ne savait pas exécuter, v3_commande le rendait à l'ancien exécuteur
 * COMMANDE PAR COMMANDE. On n'avait donc pas besoin qu'elle comprenne tout
 * pour lui confier la ligne. Elle comprend maintenant tout ce qui reste, et
 * ce qu'elle ignore est refusé au lieu de repartir.
 *
 * hct_bloc_script et non une seule instruction : la boîte de message accepte
 * plusieurs lignes collées, et même un « repeat … end repeat » entier. Un
 * analyseur d'instruction unique aurait refusé ce que l'ancien acceptait.
 *
 * LES VARIABLES SURVIVENT d'une ligne à l'autre — « put 1 into x » puis
 * « put x » —, et il fallait le vérifier avant de porter : c'est l'hôte qui
 * les tient, pas l'exécuteur, si bien qu'un HctExec neuf à chaque ligne n'en
 * perd aucune.
 *
 * Rend 1 si la v3 s'en est chargée, 0 pour laisser l'ancien faire. Une faute
 * d'analyse renvoie à l'ancien, qui est plus indulgent : on ne veut pas
 * qu'une tournure rare tapée dans la boîte cesse de marcher. */
static int v3_do_ligne(const char *line)
{
    if (!v3_actif() || !line || !*line) return 0;

    HctLot lot;
    HctReserve res;
    memset(&lot, 0, sizeof lot);
    memset(&res, 0, sizeof res);

    hct_lex(line, &lot);
    HctAnalyseur a;
    hct_analyseur_init(&a, &lot, &res);
    HctNoeud *bloc = hct_bloc_script(&a);

    if (!bloc || a.nerreurs || bloc->nfils == 0) {
        hct_reserve_libere(&res);
        hct_lot_libere(&lot);
        return 0;
    }

    HctExec x;
    hct_exec_init(&x, v3_hote());
    x.script = bloc;
    hct_exec(&x, bloc);

    if (x.a_rendu && x.retour.txt) set_result(x.retour.txt);
    if (x.ctx.erreur)
        emit(HC_ERR, "   !! %s (v3, ligne %d)", x.ctx.erreur,
             x.ctx.fautif ? x.ctx.fautif->jeton.ligne : 0);

    hct_exec_libere(&x);
    hct_reserve_libere(&res);
    hct_lot_libere(&lot);
    return 1;
}

void hc_do(const char *line)
{
    /* La boîte de message, la commande « do », et la répartition des
     * articles de menu que les piles créent : trois usages, une seule
     * porte, et de loin la plus fréquentée. */
    const char *sauve_porte = v1_porte("msg/do");
    ARENA_MARK;
    /* Le temps de cette ligne, les erreurs s'accumulent comme dans un
     * gestionnaire — voir emit_v. Un compteur et non un booléen : « do » peut
     * s'appeler lui-même, et le premier à sortir ne doit pas éteindre la
     * collecte du suivant. */
    g_msg_box++;
    g_depth  = 0;
    g_pass   = 0;
    g_me     = g_current_card;   /* dans la boîte de message, `me` = la carte */
    g_target = g_current_card;
    g_exit_handler = g_exit_repeat = g_next_repeat = 0;
    if (!v3_do_ligne(line)) {
        /* PORTE 4 : la boîte de message, « do », les articles de menu créés
         * par script. Jamais empruntée non plus dans la mesure. */
        v1_compte("v1 refusée", "msg/do");
        emit(HC_ERR, "   !! ne sait pas lire : %s", line);
    }
    ARENA_FREE;
    g_v1_porte = sauve_porte;
    /* Ce qui reste ici est à NOUS : un gestionnaire appelé depuis cette
     * ligne a déjà vidé le sien en redescendant à g_depth == 0. */
    if (--g_msg_box == 0) erreurs_vide();
}
