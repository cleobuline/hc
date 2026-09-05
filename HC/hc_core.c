/* hc_core.c — Noyau du clone HyperCard.
 * Modèle d'objets, chaîne de messages hiérarchique, mini-interpréteur.
 * C99 portable, sans dépendance.
 */
#include "hc_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <strings.h>
#include "hct_bloc.h"
#include "hct_chunk.h"   /* morceaux : étape 1 de la reprise v3 */
#include "hct_val.h"     /* valeurs  : étape 2 */
#include "hct_exec.h"    /* exécuteur : étape 3 */
#include "hct_eval.h"
#define HC_MAX_LOOP 1000000
/* ==================== outils chaînes ==================== */
#define HC_V3_DEFAUT 1
static char *dupstr(const char *s)
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

/* ==================== état global ==================== */

/* Garde-fou : profondeur maximale d'imbrication des messages.
 * Sans lui, un gestionnaire qui se renvoie son propre message
 * (`on boum / send "boum" to me / end boum`) épuise la pile C
 * et le programme meurt. Le vrai HyperCard répondait
 * « Too much recursion » ; on fait pareil, en douceur. */
/* Taille d'une valeur manipulee par l'interpreteur : variables, arguments,
 * proprietes. C'est ce plafond qui limite « get the script of me » ; un
 * script plus long est tronque, et le reecrire le mutile.
 *
 * Les tampons vivent desormais dans l'arene (voir plus bas), pas sur la
 * pile. Ce reglage ne coute donc plus de profondeur de recursion, seulement
 * de la memoire. Mesures sur le champ Calendrier et sur une recursion nue :
 *
 *                      pile / niveau     arene au pic (profondeur 63)
 *   avant l'arene        306 752 o        --        (plafond : 26 niveaux)
 *   apres, HC_VAL 16 Ko    6 720 o        49 Mo
 *   apres, HC_VAL 64 Ko    6 720 o       202 Mo
 *
 * 16 Ko couvre tres largement les scripts d'epoque (celui du Calendrier fait
 * 9,1 Ko) et garde le pic d'arene raisonnable. L'arene retombe a zero entre
 * deux commandes, et une boucle de 20 000 tours n'y consomme que 700 Ko :
 * la liberation est bien par instruction, pas par gestionnaire.
 *
 * Le vrai correctif reste a venir : allouer chaque valeur a sa taille reelle
 * plutot qu'au plafond. La plupart des valeurs font quelques octets ; ce sont
 * les ~50 tampons vivants par niveau, tous dimensionnes au maximum, qui font
 * le pic. Cela demande de remplacer les signatures (char *out, int outlen)
 * par un type chaine dynamique — un chantier a part entiere. */
/* Taille d'une valeur HyperTalk : variable, champ, résultat d'expression.
 *
 * 16 Ko à l'origine, ce qui tronquait tout fichier plus gros dès l'arrivée de
 * « read from file ». L'arène qui sert ces tampons est allouée à la demande,
 * par blocs de 4 Mo : monter cette limite ne coûte donc rien tant que les
 * valeurs restent petites, et ne se paie qu'au moment où l'on manipule
 * vraiment un gros texte.
 *
 * 256 Ko couvre les usages réels — un fichier de données, un champ de plusieurs
 * milliers de lignes — sans permettre à un script emballé d'épuiser la mémoire
 * en quelques tours de boucle. */
#define HC_VAL 1048576

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
#define HC_MAX_STACKS 16
static Object *g_stacks[HC_MAX_STACKS];
static int     g_nstacks = 0;
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
#define HC_MAX_USING 8
static Object *g_using[HC_MAX_USING];
static int     g_nusing = 0;

void hc_register_stack(Object *stack)
{
    if (!stack || stack->type != OBJ_STACK) return;
    for (int i = 0; i < g_nstacks; i++)
        if (g_stacks[i] == stack) return;             /* déjà connue */
    if (g_nstacks < HC_MAX_STACKS) g_stacks[g_nstacks++] = stack;
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
static char    g_params[16][HC_VAL];
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
    int c = atoi(g_console_buf);
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
static void emit(HcLineKind kind, const char *fmt, ...)
{
    /* Tampon propre, hors arène : arena_buf() appelle emit() en cas de
     * saturation, et une récursion mutuelle entre l'allocateur et le
     * rapporteur d'erreurs serait fatale. Les messages sont courts. */
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (g_host && g_host->line) g_host->line(kind, g_depth, buf);
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
 * Les fonctions à sorties multiples (term_value, call_function, exec_line :
 * 40, 36 et 57 return) ne sont volontairement pas instrumentées. Leurs
 * appelants directs — parse_factor et exec_stmt — le sont, et cela suffit :
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
    n = (n + 15u) & ~(size_t)15;                    /* alignement confortable */
    if (n == 0 || n > HC_ARENA_BLOCK) return NULL;

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

/* n tampons contigus, pour les tableaux d'arguments. */
static char (*arena_rows(int n))[HC_VAL]
{
    char (*p)[HC_VAL] = (char (*)[HC_VAL])arena_alloc((size_t)n * HC_VAL);
    if (!p) {
        emit(HC_ERR, "   !! arène de tampons saturée");
        return (char (*)[HC_VAL])g_apanic;      /* dégradé, mais borné */
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

/* Un message SYSTÈME, retenu quand les messages sont verrouillés. Tous les
 * envois automatiques de changement de carte passent par ici — et eux seuls,
 * pour que « send » continue de partir. */
static void hc_send_systeme(Object *o, const char *message)
{
    if (!o || g_messages_verrouilles) return;
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

/* Propriété globale lue chez l'hôte. NULL = nom inconnu. */
static const char *host_global(const char *name)
{
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

/* ---- sélection de texte ----
 * « select char 3 to 9 of field "toc" » pose une plage ; « the selection » la
 * relit. Le noyau ne fait que la RETENIR : c'est l'hôte qui la montre à
 * l'écran, via le callback selection_changed, parce que la surbrillance
 * appartient à l'éditeur de champ de l'interface et non au modèle.
 *
 * Les bornes sont en caractères, dans le texte rendu par hc_field_text, et
 * demi-ouvertes : [start, start+len). Une longueur nulle est un simple point
 * d'insertion, ce qui est exactement ce que veut dire « select before char N »
 * dans HyperTalk. */
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
void hc_set_id(Object *o, int id)
{
    if (!o || id <= 0) return;
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
static void runs_free(struct RunList *rl);

static Object *new_object(ObjType type, Object *owner, const char *name)
{
    Object *o = calloc(1, sizeof(Object));
    if (!o) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
    o->type    = type;
    o->id      = g_next_id++;
    o->name    = dupstr(name);
    o->owner   = owner;
    o->visible = 1;
    o->showname = 1;   /* le nom s'affiche par défaut */
    o->enabled  = 1;   /* et le bouton est actif */
    o->shared_hilite = 1;  /* allumage partagé entre cartes du même fond */
    return o;
}

static void add_part(Object *parent, Object *child)
{
    if (parent->nparts == parent->capparts) {
        int cap = parent->capparts ? parent->capparts * 2 : 4;
        Object **p = realloc(parent->parts, (size_t)cap * sizeof(Object *));
        if (!p) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
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

/* Normalise les fins de ligne : « \r\n » (Windows) et « \r » seul (Mac
 * classique) deviennent « \n ». Sans ça, un script en « \r » n'est qu'une
 * seule ligne géante : find_handler reconnaît bien « on mouseUp », mais le
 * corps — cherché après le premier « \n » — est vide. Le gestionnaire est
 * alors annoncé comme traité et ne fait rien, ce qui est très déroutant.
 * Les scripts d'HyperCard d'origine sont tous en « \r ». */
/* Caractères spéciaux du Macintosh. Les scripts d'origine sont encodés en
 * MacRoman ; recopiés depuis un navigateur ils arrivent en UTF-8. On accepte
 * les deux, car on ne peut pas savoir d'où vient la pile :
 *
 *     ¬   0xC2   /  0xC2 0xAC        continuation : la ligne suivante suit
 *     ≠   0xAD   /  0xE2 0x89 0xA0   devient « <> »
 *     ≤   0xB2   /  0xE2 0x89 0xA4   devient « <= »
 *     ≥   0xB3   /  0xE2 0x89 0xA5   devient « >= »
 *
 * L'ambiguïté du 0xC2 se lève seule : en UTF-8 il est toujours suivi d'un
 * octet de continuation (0x80-0xBF), et l'unique séquence qui nous intéresse
 * est 0xC2 0xAC. Un 0xC2 suivi d'autre chose est un ¬ MacRoman ; un 0xC2
 * suivi d'un octet de continuation autre que 0xAC est un caractère UTF-8
 * quelconque (« ² », « ° »…) qu'on recopie intact.
 *
 * La sortie peut être plus longue que l'entrée (un octet « ≠ » devient deux),
 * d'où l'allocation au double. */
static int is_utf8_cont(unsigned char c) { return c >= 0x80 && c <= 0xBF; }

static char *dup_script(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = (char *)malloc(2 * n + 2);
    if (!d) return NULL;
    char *w = d;

    for (const unsigned char *p = (const unsigned char *)s; *p; ) {
        /* --- continuation de ligne --- */
        int cont = 0;
        if (p[0] == 0xC2 && p[1] == 0xAC)              { cont = 1; p += 2; }
        else if (p[0] == 0xC2 && !is_utf8_cont(p[1]))  { cont = 1; p += 1; }
        if (cont) {
            /* avaler les blancs puis la fin de ligne : les deux lignes n'en
             * font plus qu'une, séparées par une espace. */
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\r') { p++; if (*p == '\n') p++; }
            else if (*p == '\n') p++;
            *w++ = ' ';
            continue;
        }

        /* --- opérateurs de comparaison --- */
        if (p[0] == 0xE2 && p[1] == 0x89) {
            if (p[2] == 0xA0) { *w++ = '<'; *w++ = '>'; p += 3; continue; }
            if (p[2] == 0xA4) { *w++ = '<'; *w++ = '='; p += 3; continue; }
            if (p[2] == 0xA5) { *w++ = '>'; *w++ = '='; p += 3; continue; }
        }
        if (p[0] == 0xAD) { *w++ = '<'; *w++ = '>'; p++; continue; }
        if (p[0] == 0xB2) { *w++ = '<'; *w++ = '='; p++; continue; }
        if (p[0] == 0xB3) { *w++ = '>'; *w++ = '='; p++; continue; }

        /* --- caractère UTF-8 multi-octets : recopie intégrale --- */
        if (p[0] >= 0xC2 && p[0] <= 0xF4) {
            int len = p[0] >= 0xF0 ? 4 : p[0] >= 0xE0 ? 3 : 2;
            for (int i = 0; i < len && p[i]; i++) *w++ = (char)p[i];
            while (len-- && *p) p++;
            continue;
        }

        /* --- fins de ligne --- */
        if (p[0] == '\r') {
            p++;
            if (*p == '\n') p++;          /* \r\n : une seule fin de ligne */
            *w++ = '\n';
            continue;
        }

        *w++ = (char)*p++;
    }
    *w = '\0';
    return d;
}

/* Jette l'arbre d'un objet. Appelé dès que son script change, et par hc_free.
 *
 * Les jetons pointent DANS le texte du script : libérer l'un sans l'autre
 * laisserait des pointeurs dans de la mémoire rendue. Les trois morceaux
 * naissent et meurent donc ensemble. */
void hc_arbre_oublie(Object *o)
{
    if (!o) return;

    /* L'arbre tourne : on ne peut rien libérer sous ses pieds. On marque, et
     * v3_execute nettoiera en sortant. */
    if (o->arbre_usage > 0) { o->arbre_perime = 1; return; }

    if (o->reserve) { hct_reserve_libere((HctReserve *)o->reserve); free(o->reserve); }
    if (o->lot)     { hct_lot_libere((HctLot *)o->lot);             free(o->lot); }
    o->arbre = NULL;
    o->reserve = NULL;
    o->lot = NULL;
    o->arbre_sain = 0;
    o->arbre_signale = 0;
    o->arbre_perime = 0;

    /* Les anciens textes mis de côté ne servent plus : plus aucun jeton n'y
     * pointe, puisque le lot vient d'être libéré. */
    for (int i = 0; i < o->ntextes_gardes; i++) free(o->textes_gardes[i]);
    free(o->textes_gardes);
    o->textes_gardes = NULL;
    o->ntextes_gardes = 0;
}

/* Met un texte de script de côté au lieu de le libérer.
 *
 * Les jetons de l'arbre en cours d'exécution pointent dedans : le libérer
 * maintenant les ferait viser de la mémoire rendue. On le garde jusqu'à ce
 * que l'arbre lui-même parte. */
static void garde_texte(Object *o, char *texte)
{
    if (!texte) return;
    char **t = realloc(o->textes_gardes,
                       (size_t)(o->ntextes_gardes + 1) * sizeof *t);
    if (!t) { free(texte); return; }   /* faute de mieux */
    o->textes_gardes = t;
    o->textes_gardes[o->ntextes_gardes++] = texte;
}

/* Parcourt l'arbre et signale chaque nœud d'erreur, avec la ligne, la colonne
 * et le texte de la ligne fautive.
 *
 * L'analyseur ne s'arrête pas à la première faute : il en pose une et
 * continue, pour pouvoir toutes les montrer d'un coup. On les montre donc
 * toutes — mais pas plus de vingt, un script vraiment cassé en produirait
 * autant que de lignes. */
static void v3_dis_les_fautes_r(Object *o, const HctNoeud *n, int *reste)
{
    if (!n || *reste <= 0) return;

    if (n->genre == HCTN_ERREUR) {
        if (!o->arbre_faute_ligne) o->arbre_faute_ligne = n->jeton.ligne;

        /* La ligne du script telle qu'elle est écrite, pour n'avoir pas à
         * compter les lignes dans l'éditeur. */
        char ligne[160] = "";
        if (o->script) {
            const char *p = o->script;
            for (int l = 1; l < n->jeton.ligne && *p; p++)
                if (*p == '\n') l++;
            int k = 0;
            while (*p && *p != '\n' && *p != '\r' && k < (int)sizeof ligne - 1)
                ligne[k++] = *p++;
            ligne[k] = '\0';
        }

        emit(HC_ERR, "   !! script, ligne %d colonne %d : %s",
             n->jeton.ligne, n->jeton.col, n->msg ? n->msg : "forme non comprise");
        if (*ligne) emit(HC_ERR, "      %s", ligne);
        (*reste)--;
    }

    for (int i = 0; i < n->nfils; i++)
        v3_dis_les_fautes_r(o, n->fils[i], reste);
}

static void v3_dis_les_fautes(Object *o, const HctNoeud *racine)
{
    int reste = 20;
    v3_dis_les_fautes_r(o, racine, &reste);
}

/* L'arbre du script, analysé à la première demande et gardé ensuite.
 *
 * Rend NULL si le script est vide, ou si l'analyse a signalé la moindre
 * faute. Ce dernier point est délibéré : mieux vaut confier tout le script à
 * l'ancien interpréteur que d'en exécuter la moitié avec le nouveau et de
 * s'arrêter au milieu sur une forme mal comprise. La frontière se déplacera
 * quand la v3 saura tout lire, pas avant. */
static const HctNoeud *script_arbre(Object *o)
{
    if (!o || !o->script || !*o->script) return NULL;
    if (o->arbre) return o->arbre_sain ? (const HctNoeud *)o->arbre : NULL;
    if (o->lot) return NULL;          /* déjà tenté, et rejeté */

    HctLot *lot = calloc(1, sizeof *lot);
    HctReserve *res = calloc(1, sizeof *res);
    if (!lot || !res) { free(lot); free(res); return NULL; }

    o->lot = lot;
    o->reserve = res;

    int sain = hct_lex(o->script, lot);

    HctAnalyseur a;
    hct_analyseur_init(&a, lot, res);
    HctNoeud *racine = hct_bloc_script(&a);

    /* Une faute ne condamne plus TOUT le script, seulement le gestionnaire qui
     * la porte — trouve_gestionnaire l'écartera, et ce message-là repartira à
     * l'ancien interpréteur.
     *
     * Ce qu'on exige encore, c'est que la STRUCTURE tienne : chaque enfant de
     * la racine doit être un gestionnaire. Une faute qui déborde de son
     * gestionnaire laisse des nœuds nus à ce niveau, signe qu'un « end » a été
     * mal attribué et que le découpage n'est plus fiable — là, on refuse tout.
     *
     * Ce que ça change en pratique : une coquille dans une fonction
     * inutilisée — il en traîne dans les piles réelles — ne prive plus les
     * trente autres gestionnaires du script de la v3. */
    int structure_ok = (racine != NULL);
    for (int i = 0; racine && i < racine->nfils; i++)
        if (racine->fils[i]->genre != HCTN_GESTIONNAIRE) { structure_ok = 0; break; }

    if (sain && racine && (a.nerreurs == 0 || structure_ok)) {
        o->arbre = racine;
        o->arbre_sain = 1;
        if (a.nerreurs) {
            /* On le dit quand même : la faute est réelle, et son gestionnaire
             * n'aura pas la v3. */
            o->arbre_faute_ligne = 0;
            v3_dis_les_fautes(o, racine);
        }
        return racine;
    }

    /* Analyse douteuse : on garde le lot et la réserve pour ne pas
     * recommencer à chaque message, mais on ne rendra jamais l'arbre.
     *
     * On dit AUSSI pourquoi, et où. « analyse non propre » tout court
     * n'apprenait rien : un script de trois cents lignes refusé pour une
     * virgule se cherchait à la main. Une faute suffit à écarter le script
     * entier, donc la première ligne signalée est celle à corriger. */
    o->arbre_sain = 0;
    o->arbre_faute_ligne = 0;

    for (int i = 0; i < lot->n; i++)
        if (lot->jetons[i].genre == HCT_ERREUR) {
            if (!o->arbre_faute_ligne) o->arbre_faute_ligne = lot->jetons[i].ligne;
            emit(HC_ERR, "   !! script, ligne %d colonne %d : %s",
                 lot->jetons[i].ligne, lot->jetons[i].col,
                 lot->jetons[i].msg ? lot->jetons[i].msg : "jeton mal formé");
        }
    v3_dis_les_fautes(o, racine);

    return NULL;
}

void hc_set_script(Object *o, const char *script)
{
    if (o->arbre_usage > 0) {
        /* Le script se réécrit pendant qu'il s'exécute — le calendrier
         * d'Apple range ses données dans le sien. On ne libère donc ni
         * l'arbre ni son texte : le premier est marqué périmé, le second mis
         * de côté, et tout partira quand l'exécution sera finie.
         *
         * Le gestionnaire en cours continue sur l'ANCIEN texte, ce qui est le
         * comportement de HyperCard : la réécriture ne prend effet qu'au
         * prochain appel. */
        o->arbre_perime = 1;
        garde_texte(o, o->script);
        o->script = dup_script(script);
        return;
    }

    hc_arbre_oublie(o);          /* AVANT de libérer le texte : les jetons y pointent */
    free(o->script);
    o->script = dup_script(script);
}

void hc_free(Object *o)
{
    if (!o) return;
    for (int i = 0; i < o->nparts; i++) hc_free(o->parts[i]);
    free(o->parts);
    free(o->name);
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
    free(o);
}

/* Retire un objet (bouton/champ) de la couche de son propriétaire et le libère.
 * Renvoie 1 si trouvé et supprimé, 0 sinon. */
/* Définis plus bas, mais la suppression de carte en a besoin ici. */
static Object *nth_card(Object *stack, int n);
static int     card_index(Object *stack, Object *card);

/* Supprime une carte de sa pile.
 *
 * Refuse la DERNIÈRE carte : HyperCard faisait de même, et une pile sans carte
 * n'a pas de sens — le chargement en fabriquerait une d'office, donnant
 * l'impression que la suppression n'a rien fait.
 *
 * La carte courante est déplacée AVANT la libération, sur la suivante ou, à
 * défaut, sur la précédente. Sans cela g_current_card pointerait dans de la
 * mémoire rendue, et la première évaluation de script partirait dans le vide.
 *
 * Le texte des champs de fond non partagés meurt avec la carte : il vivait
 * dans ses bgtexts, ce que hc_free libère déjà. */
int hc_delete_card(Object *card)
{
    if (!card || card->type != OBJ_CARD || !card->owner) return 0;
    Object *stack = card->owner;

    int total = 0;
    for (int i = 0; i < stack->nparts; i++)
        if (stack->parts[i]->type == OBJ_CARD) total++;
    if (total <= 1) return 0;              /* on ne supprime pas la dernière */

    int idx = card_index(stack, card);
    if (idx < 0) return 0;

    /* HyperCard prévient la carte AVANT de la faire disparaître : c'est la
     * dernière occasion qu'a un script de sauver ce qu'elle contient. Après
     * hc_free, il n'y a plus personne à qui parler.
     *
     * Le pendant de newCard, qui existait déjà côté interface. Envoyé ici et
     * non dans hc_free : celui-ci sert aussi à libérer une pile entière, et
     * une pile qui se ferme n'a pas à voir défiler un deleteCard par carte. */
    hc_send_systeme(card, "deleteCard");

    Object *bg = card->bg;                 /* retenu AVANT la libération */

    for (int i = 0; i < stack->nparts; i++) {
        if (stack->parts[i] != card) continue;
        for (int j = i; j < stack->nparts - 1; j++)
            stack->parts[j] = stack->parts[j + 1];
        stack->nparts--;
        break;
    }

    if (g_current_card == card) {
        Object *suiv = nth_card(stack, idx);          /* celle qui a pris la place */
        if (!suiv) suiv = nth_card(stack, idx - 1);   /* c'était la dernière */
        g_current_card = suiv;
    }

    hc_free(card);

    /* Un fond n'existe que par ses cartes : HyperCard n'en crée jamais un seul
     * et le fait disparaître avec la dernière carte qui s'y appuie. Sans ce
     * ménage, le fond survit en mémoire — « the number of backgrounds » le
     * compte encore, et « go background "x" » peut désigner cette coquille
     * vide plutôt que le fond homonyme réellement utilisé. */
    if (bg && bg->type == OBJ_BACKGROUND) {
        int reste = 0;
        for (int i = 0; i < stack->nparts && !reste; i++)
            if (stack->parts[i]->type == OBJ_CARD && stack->parts[i]->bg == bg)
                reste = 1;
        if (!reste) {
            for (int i = 0; i < stack->nparts; i++) {
                if (stack->parts[i] != bg) continue;
                for (int j = i; j < stack->nparts - 1; j++)
                    stack->parts[j] = stack->parts[j + 1];
                stack->nparts--;
                break;
            }
            hc_free(bg);
        }
    }

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

void hc_set_hilite(Object *btn, Object *card, int on)
{
    if (!btn) return;
    if (!hilite_par_carte(btn)) { btn->hilite = on ? 1 : 0; return; }

    if (!card) card = g_current_card;
    hc_set_hilite_raw(card, btn->id, on);
}

int hc_card_count(Object *stack){
    if (!stack) return 0;
    int n = 0;
    for (int i = 0; i < stack->nparts; i++)
        if (stack->parts[i]->type == OBJ_CARD) n++;
    return n;
}

int hc_delete_part(Object *o)
{
    if (!o || !o->owner) return 0;
    Object *parent = o->owner;
    for (int i = 0; i < parent->nparts; i++) {
        if (parent->parts[i] == o) {
            /* décaler les suivants pour combler le trou */
            for (int j = i; j < parent->nparts - 1; j++)
                parent->parts[j] = parent->parts[j + 1];
            parent->nparts--;
            hc_free(o);
            return 1;
        }
    }
    return 0;
}

/* ==================== presse-papiers d'objets ====================
 *
 * HyperCard copie un OBJET, pas des pixels ni du texte : un bouton collé
 * emporte son script, son icône, sa police, son style. Le presse-papiers
 * garde donc un Object complet, simplement DÉTACHÉ — owner à NULL, absent
 * du tableau parts[] de qui que ce soit. Il survit ainsi à la suppression de
 * sa carte d'origine, et même au chargement d'une autre pile.
 *
 * La liste de ce qu'un clone doit emporter est celle de put_part() dans
 * hc_file.c : si le format de fichier le sérialise, le clone le copie. Les
 * deux doivent rester d'accord, sans quoi un objet collé perdrait à l'écran
 * ce qu'il aurait gardé sur disque. */
static Object *g_clipboard = NULL;

/* Définis plus bas, mais le presse-papiers en a besoin ici. */
static int runs_room(struct RunList *rl, int need);

/* Clone profond d'un bouton ou d'un champ. Le clone n'a PAS de propriétaire
 * et garde l'identifiant de l'original : c'est hc_paste_part qui en attribue
 * un neuf au moment de la pose, parce que c'est là seulement qu'on sait dans
 * quelle pile il atterrit. */
static Object *clone_part(Object *o)
{
    if (!o || (o->type != OBJ_BUTTON && o->type != OBJ_FIELD)) return NULL;

    Object *c = calloc(1, sizeof(Object));
    if (!c) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }

    c->type = o->type;
    c->id   = o->id;              /* remplacé à la pose */
    c->owner = NULL;              /* détaché : c'est tout l'intérêt */

    c->name     = dupstr(o->name);
    c->script   = dupstr(o->script);
    c->contents = dupstr(o->contents);
    c->style    = dupstr(o->style);
    c->textfont = dupstr(o->textfont);

    c->x = o->x; c->y = o->y; c->w = o->w; c->h = o->h;

    c->visible      = o->visible;
    c->hilite       = o->hilite;
    c->autohilite   = o->autohilite;
    c->textsize     = o->textsize;
    c->showname     = o->showname;
    c->enabled      = o->enabled;
    c->shared_hilite = o->shared_hilite;
    c->icon         = o->icon;
    c->selectedline = o->selectedline;
    c->locktext     = o->locktext;
    c->wide_margins = o->wide_margins;
    c->fixed_lh     = o->fixed_lh;
    c->show_lines   = o->show_lines;
    c->auto_tab     = o->auto_tab;
    c->dont_search  = o->dont_search;
    c->shared_text  = o->shared_text;
    c->textstyle    = o->textstyle;
    c->scroll       = o->scroll;

    /* Les plages de style : chaque nom de police est duppé à son tour, sinon
     * deux objets partageraient le même pointeur et le second hc_free()
     * libérerait une seconde fois. */
    if (o->runs.n > 0 && runs_room(&c->runs, o->runs.n)) {
        for (int i = 0; i < o->runs.n; i++) {
            c->runs.v[i] = o->runs.v[i];
            c->runs.v[i].font = dupstr(o->runs.v[i].font);
            /* .color part avec la copie de structure ci-dessus. */
        }
        c->runs.n = o->runs.n;
    }

    return c;
}

int hc_copy_part(Object *o)
{
    Object *c = clone_part(o);
    if (!c) return 0;

    /* Un champ de fond NON PARTAGÉ garde son texte dans chaque carte, pas
     * dans l'objet. Ce qu'on voit à l'écran vient donc de la carte courante :
     * c'est ce texte-là qu'il faut emporter, et non le contenu par défaut de
     * l'objet, qui est souvent vide. */
    if (o->type == OBJ_FIELD && !o->shared_text &&
        o->owner && o->owner->type == OBJ_BACKGROUND) {
        const char *seen = hc_field_text(o);
        if (seen && *seen) { free(c->contents); c->contents = dupstr(seen); }
    }

    if (g_clipboard) hc_free(g_clipboard);
    g_clipboard = c;
    return 1;
}

int hc_cut_part(Object *o)
{
    if (!hc_copy_part(o)) return 0;
    return hc_delete_part(o);
}

Object *hc_paste_part(Object *owner)
{
    if (!g_clipboard || !owner) return NULL;
    if (owner->type != OBJ_CARD && owner->type != OBJ_BACKGROUND) return NULL;

    Object *c = clone_part(g_clipboard);
    if (!c) return NULL;

    /* Identifiant NEUF. Deux objets de même id rendraient « field id 42 »
     * ambigu, et hc_save écrirait deux fois la même clé. */
    c->id = g_next_id++;
    c->owner = owner;

    /* Le propriétaire décide de la nature : coller sur une carte un bouton
     * pris sur un fond en fait un bouton de carte. C'est le comportement
     * d'HyperCard, et la seule lecture cohérente — l'objet vit désormais là.
     * Un champ ne peut être « partagé » que sur un fond. */
    if (owner->type == OBJ_CARD) c->shared_text = 0;

    /* Décalage si la place est déjà prise, pour que le collé ne se cache pas
     * exactement derrière l'original. HyperCard fait de même. */
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (p->type == c->type && p->x == c->x && p->y == c->y) {
            c->x += 8; c->y += 8;
            i = -1;                     /* re-vérifier depuis le début */
        }
    }

    add_part(owner, c);
    return c;
}

/* ==================== copier-coller de CARTES ====================
 *
 * Une carte emporte plus qu'un bouton : ses boutons et champs de carte, son
 * calque de peinture, sa marque, et surtout ses bgtexts — le texte non partagé
 * des champs de FOND, qui appartient à la carte et non au fond. L'oublier
 * donnerait une copie visuellement identique mais vide de son contenu propre.
 *
 * Le FOND, lui, n'est pas copié : une carte s'appuie dessus, elle ne le
 * possède pas. Le clone retient donc son IDENTIFIANT, jamais son pointeur —
 * le presse-papiers promet de survivre à la fermeture d'une pile, et un
 * pointeur de fond y deviendrait mort. On résout au collage.
 *
 * Conséquence assumée : coller dans une autre pile échoue, faute d'y trouver
 * ce fond. C'est le périmètre choisi ; cloner un fond manquant se ferait ici,
 * et nulle part ailleurs.
 */

/* Le fond de la carte au presse-papiers, sous DEUX formes.
 *
 * `copie` est une copie détachée, que le presse-papiers possède : c'est elle
 * qui permet de recréer le fond dans une pile qui ne l'a pas. `vivant` est le
 * fond réel, emprunté, avec la pile où il se trouve — il sert à reconnaître
 * qu'on colle là où l'on a copié, et donc à réutiliser plutôt qu'à dupliquer.
 *
 * `vivant` n'est jamais DÉRÉFÉRENCÉ, seulement comparé aux fonds de la pile
 * visée. Et hc_clipboard_stack_closing l'efface quand sa pile disparaît, ce
 * qui interdit même la comparaison avec un pointeur mort. */
static Object *g_clip_bg_copy  = NULL;   /* possédée */
static Object *g_clip_bg_live  = NULL;   /* empruntée, ou NULL */
static Object *g_clip_bg_stack = NULL;   /* pile de g_clip_bg_live */

/* Les icônes que la carte copiée utilise, et qui appartiennent à SA pile.
 *
 * Un bouton ne retient qu'un numéro, et ce numéro désigne le catalogue de sa
 * pile d'origine : collé ailleurs, il ne montrerait plus rien. Les icônes
 * d'origine, elles, existent partout — inutile de les transporter, et c'est
 * automatique puisqu'on ne prend que ce que hc_icon_get trouve dans la pile. */
static struct StackIcon *g_clip_icons  = NULL;
static int               g_clip_nicons = 0;

/* Défini plus bas, avec les autres crochets d'icônes, mais la transplantation
 * en a besoin ici. */
static int icon_id_is_builtin(int id);

/* Copie profonde des bgtexts d'une carte vers une autre. */
static void clone_bgtexts(Object *dst, Object *src)
{
    if (src->nbgtexts <= 0) return;

    dst->bgtexts = calloc((size_t)src->nbgtexts, sizeof *dst->bgtexts);
    if (!dst->bgtexts) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
    dst->capbgtexts = src->nbgtexts;

    for (int i = 0; i < src->nbgtexts; i++) {
        dst->bgtexts[i].field_id = src->bgtexts[i].field_id;
        dst->bgtexts[i].text     = dupstr(src->bgtexts[i].text);
        memset(&dst->bgtexts[i].runs, 0, sizeof dst->bgtexts[i].runs);

        /* Chaque nom de police est duppé à son tour : partager le pointeur
         * ferait libérer deux fois au second hc_free. Même raison que dans
         * clone_part. */
        struct RunList *sr = &src->bgtexts[i].runs;
        if (sr->n > 0 && runs_room(&dst->bgtexts[i].runs, sr->n)) {
            for (int k = 0; k < sr->n; k++) {
                dst->bgtexts[i].runs.v[k]      = sr->v[k];
                dst->bgtexts[i].runs.v[k].font = dupstr(sr->v[k].font);
            }
            dst->bgtexts[i].runs.n = sr->n;
        }
    }
    dst->nbgtexts = src->nbgtexts;
}

/* Clone d'une couche — carte ou fond — avec ses boutons et ses champs.
 * Détachée, identifiants inchangés : c'est la pose qui en attribue de neufs. */
static Object *clone_layer(Object *o, ObjType type)
{
    if (!o || o->type != type) return NULL;

    Object *c = calloc(1, sizeof(Object));
    if (!c) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }

    c->type   = type;
    c->id     = o->id;             /* remplacé à la pose */
    c->owner  = NULL;              /* détachée */
    c->bg     = NULL;              /* résolu à la pose */

    c->name   = dupstr(o->name);
    c->script = dupstr(o->script);
    /* Pas de recopie de l'arbre : ses jetons pointeraient dans le script de
     * l'ORIGINAL. Le clone réanalysera le sien à la première demande. */
    c->arbre = c->reserve = c->lot = NULL;
    c->arbre_sain = 0;
    c->paint  = dupstr(o->paint);
    c->marked = o->marked;

    for (int i = 0; i < o->nparts; i++) {
        Object *p = clone_part(o->parts[i]);
        if (!p) continue;          /* clone_part ne prend que boutons et champs */
        p->owner = c;
        add_part(c, p);
    }

    if (type == OBJ_CARD) {
        clone_bgtexts(c, o);
        /* L'allumage par carte suit la carte, comme son texte non partagé.
         * Copie plate : ni chaînes ni plages de style à dupliquer. */
        if (o->nbghilites > 0) {
            c->bghilites = calloc((size_t)o->nbghilites, sizeof *c->bghilites);
            if (c->bghilites) {
                memcpy(c->bghilites, o->bghilites,
                       (size_t)o->nbghilites * sizeof *c->bghilites);
                c->nbghilites = c->capbghilites = o->nbghilites;
            }
        }
    }
    return c;
}

/* Réétiquette les bgtexts d'une carte quand son fond a changé d'exemplaire.
 *
 * Les bgtexts désignent les champs de fond par IDENTIFIANT, et un fond cloné
 * en reçoit de neufs : sans cette étape, le texte non partagé de la carte
 * n'appartiendrait plus à personne et disparaîtrait de l'écran.
 *
 * On travaille par POSITION, non par identifiant : un clone conserve l'ordre
 * de ses parts, donc l'indice i de l'un désigne le même champ que l'indice i
 * de l'autre. C'est la seule correspondance qui tienne, les identifiants
 * étant précisément ce qui a changé. */
static void remap_bgtexts(Object *card, Object *bgsrc, Object *bgdst)
{
    if (!card || !bgsrc || !bgdst || bgsrc == bgdst) return;

    for (int i = 0; i < card->nbgtexts; i++) {
        int idx = -1;
        for (int k = 0; k < bgsrc->nparts; k++)
            if (bgsrc->parts[k]->id == card->bgtexts[i].field_id) { idx = k; break; }

        if (idx >= 0 && idx < bgdst->nparts)
            card->bgtexts[i].field_id = bgdst->parts[idx]->id;
    }

    /* L'allumage par carte désigne les boutons de fond de la même façon, et se
     * perdrait tout autant : une case cochée redeviendrait vide. */
    for (int i = 0; i < card->nbghilites; i++) {
        int idx = -1;
        for (int k = 0; k < bgsrc->nparts; k++)
            if (bgsrc->parts[k]->id == card->bghilites[i].button_id) { idx = k; break; }

        if (idx >= 0 && idx < bgdst->nparts)
            card->bghilites[i].button_id = bgdst->parts[idx]->id;
    }
}

/* Pose un clone de couche dans une pile, avec des identifiants neufs.
 *
 * Deux objets de même id rendraient « card id 12 » ambigu, et hc_save
 * écrirait deux fois la même clé. */
static Object *place_layer_clone(Object *stack, Object *modele, ObjType type,
                                 Object *bg, Object *apres)
{
    Object *c = clone_layer(modele, type);
    if (!c) return NULL;

    c->id    = g_next_id++;
    c->owner = stack;
    c->bg    = bg;
    for (int i = 0; i < c->nparts; i++) c->parts[i]->id = g_next_id++;

    add_part(stack, c);            /* d'abord en fin, puis on le remonte */

    /* Insertion juste après `apres`, comme HyperCard qui colle derrière la
     * carte courante. parts[] mêle fonds et cartes : on décale bêtement, la
     * position relative des fonds n'ayant aucune importance. */
    if (apres) {
        int ia = -1;
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i] == apres) { ia = i; break; }
        if (ia >= 0) {
            for (int i = stack->nparts - 1; i > ia + 1; i--)
                stack->parts[i] = stack->parts[i - 1];
            stack->parts[ia + 1] = c;
        }
    }
    return c;
}

static void clip_bg_clear(void)
{
    if (g_clip_bg_copy) hc_free(g_clip_bg_copy);
    g_clip_bg_copy  = NULL;
    g_clip_bg_live  = NULL;
    g_clip_bg_stack = NULL;

    for (int i = 0; i < g_clip_nicons; i++) free(g_clip_icons[i].name);
    free(g_clip_icons);
    g_clip_icons  = NULL;
    g_clip_nicons = 0;
}

/* Trace du transport d'icônes. Mettre à 0 pour la faire taire. */
#define HC_TRACE_ICONS 1

/* Ramasse dans `layer` les icônes de pile qu'utilisent ses boutons. */
static void clip_collect_icons(Object *stack, Object *layer)
{
    if (!stack || !layer) return;

#if HC_TRACE_ICONS
    fprintf(stderr, "[icone] examen de %s \"%s\" : %d part(s)\n",
            layer->type == OBJ_CARD ? "la carte" : "le fond",
            layer->name ? layer->name : "", layer->nparts);
    for (int i = 0; i < layer->nparts; i++)
        fprintf(stderr, "[icone]   part %d type=%d icon=%d \"%s\"\n",
                i, layer->parts[i]->type, layer->parts[i]->icon,
                layer->parts[i]->name ? layer->parts[i]->name : "");
#endif

    for (int i = 0; i < layer->nparts; i++) {
        Object *p = layer->parts[i];
        if (p->type != OBJ_BUTTON || p->icon == 0) continue;

        struct StackIcon *src = hc_icon_get(stack, p->icon);
#if HC_TRACE_ICONS
        if (!src)
            fprintf(stderr, "[icone] bouton icon=%d absent de la pile source"
                            " (icone d'origine ?)\n", p->icon);
#endif
        if (!src) continue;                     /* icône d'origine : partout */

        int deja = 0;
        for (int k = 0; k < g_clip_nicons && !deja; k++)
            if (g_clip_icons[k].id == p->icon) deja = 1;
        if (deja) continue;

        struct StackIcon *t = realloc(g_clip_icons,
                                      (size_t)(g_clip_nicons + 1) * sizeof *t);
        if (!t) return;
        g_clip_icons = t;

        memset(&g_clip_icons[g_clip_nicons], 0, sizeof *g_clip_icons);
        g_clip_icons[g_clip_nicons].id   = src->id;
        g_clip_icons[g_clip_nicons].name = dupstr(src->name);
        memcpy(g_clip_icons[g_clip_nicons].bits, src->bits, HC_ICON_BYTES);
        g_clip_nicons++;
#if HC_TRACE_ICONS
        fprintf(stderr, "[icone] ramassee %d \"%s\"\n",
                src->id, src->name ? src->name : "");
#endif
    }
}

/* Un numéro libre dans cette pile, hors du catalogue d'origine, ET hors des
 * numéros que le presse-papiers n'a pas encore posés.
 *
 * Ce dernier point n'est pas un détail : si l'icône 1500 doit être renumérotée
 * en 1000 alors qu'une autre du même lot porte déjà 1000 et attend son tour,
 * la seconde écraserait la première — hc_icon_add remplace sur numéro égal. */
static int icon_free_id_in(Object *stack)
{
    for (int id = 1000; id < 100000; id++) {
        if (hc_icon_get(stack, id))  continue;
        if (icon_id_is_builtin(id))  continue;

        int reserve = 0;
        for (int k = 0; k < g_clip_nicons && !reserve; k++)
            if (g_clip_icons[k].id == id) reserve = 1;
        if (reserve) continue;

        return id;
    }
    return 0;
}

static void remap_button_icons(Object *layer, int oldid, int newid)
{
    if (!layer || oldid == newid) return;
    for (int i = 0; i < layer->nparts; i++)
        if (layer->parts[i]->type == OBJ_BUTTON && layer->parts[i]->icon == oldid)
            layer->parts[i]->icon = newid;
}

/* Installe dans la pile d'arrivée les icônes du presse-papiers.
 *
 * Trois cas par icône :
 *   — le numéro est libre : on la pose telle quelle ;
 *   — le numéro est pris par une icône IDENTIQUE : on réutilise, rien à faire.
 *     C'est le cas ordinaire quand on colle là où l'on a copié, et aussi quand
 *     on colle deux fois de suite dans la même pile ;
 *   — le numéro est pris par une AUTRE icône : on en prend un libre et l'on
 *     réétiquette les boutons. Deux piles chargées de fichiers différents
 *     peuvent parfaitement numéroter deux dessins distincts de la même façon ;
 *     réutiliser aveuglément mettrait la mauvaise image sur le bouton. */
/* `bg` ne doit être passé que si le fond vient d'être RECRÉÉ.
 *
 * Un fond réutilisé est partagé par toutes les cartes de la pile : réétiqueter
 * ses boutons changerait les icônes de chacune d'elles. Le cas ne se présente
 * pas tant que réutilisation rime avec icônes identiques — mais il suffit
 * d'avoir retouché une icône entre le copier et le coller pour que les bits
 * diffèrent, et l'on abîmerait la pile entière pour une carte collée. */
static void transplant_icons(Object *stack, Object *card, Object *bg)
{
#if HC_TRACE_ICONS
    fprintf(stderr, "[icone] transplantation de %d icone(s)\n", g_clip_nicons);
#endif
    for (int i = 0; i < g_clip_nicons; i++) {
        int oldid = g_clip_icons[i].id;
        int newid = oldid;

        struct StackIcon *ex = hc_icon_get(stack, oldid);
        int identique = ex &&
            memcmp(ex->bits, g_clip_icons[i].bits, HC_ICON_BYTES) == 0;

        if (!identique) {
            if (ex) {
                newid = icon_free_id_in(stack);
#if HC_TRACE_ICONS
                fprintf(stderr, "[icone] %d deja pris par un autre dessin"
                                " -> %d\n", oldid, newid);
#endif
                if (!newid) continue;           /* on laisse le numéro mort */
            }
            struct StackIcon *e = hc_icon_add(stack, newid, g_clip_icons[i].name);
#if HC_TRACE_ICONS
            fprintf(stderr, "[icone] pose %d \"%s\" -> %s\n",
                    newid, g_clip_icons[i].name ? g_clip_icons[i].name : "",
                    e ? "ok" : "ECHEC");
#endif
            if (!e) continue;
            memcpy(e->bits, g_clip_icons[i].bits, HC_ICON_BYTES);
        }
#if HC_TRACE_ICONS
        else fprintf(stderr, "[icone] %d deja presente a l'identique\n", oldid);
#endif

        remap_button_icons(card, oldid, newid);
        remap_button_icons(bg,   oldid, newid);
    }
}

int hc_copy_card(Object *card)
{
    if (!card || card->type != OBJ_CARD) return 0;

    Object *c = clone_layer(card, OBJ_CARD);
    if (!c) return 0;

    /* Le fond part AUSSI au presse-papiers, en copie. C'est ce qui rend le
     * collage dans une autre pile possible : la carte s'appuie sur un fond
     * qui, là-bas, n'existe pas. */
    Object *bgc = card->bg ? clone_layer(card->bg, OBJ_BACKGROUND) : NULL;

    if (g_clipboard) hc_free(g_clipboard);
    clip_bg_clear();

    /* Après clip_bg_clear, qui remet la table à zéro. */
    clip_collect_icons(card->owner, card);
    clip_collect_icons(card->owner, card->bg);

    g_clipboard     = c;
    g_clip_bg_copy  = bgc;
    g_clip_bg_live  = card->bg;
    g_clip_bg_stack = card->owner;
    return 1;
}

/* Couper : copier puis supprimer.
 *
 * hc_delete_card fait disparaître le fond avec sa dernière carte. Le fond
 * vivant est donc oublié ici, mais sa COPIE reste au presse-papiers : coller
 * le recréera. C'est précisément ce que la copie du fond apporte. */
int hc_cut_card(Object *card)
{
    if (!hc_copy_card(card)) return 0;
    g_clip_bg_live  = NULL;
    g_clip_bg_stack = NULL;
    return hc_delete_card(card);
}

Object *hc_paste_card(Object *stack)
{
    if (!g_clipboard || g_clipboard->type != OBJ_CARD) return NULL;
    if (!stack || stack->type != OBJ_STACK) return NULL;

    /* Le fond existe-t-il déjà ici ?
     *
     * On le reconnaît par IDENTITÉ, pas par identifiant : deux piles chargées
     * de fichiers différents peuvent porter le même numéro sans rien avoir de
     * commun, et rattacher la carte au mauvais fond lui ferait perdre sa mise
     * en page sans le moindre avertissement. */
    Object *bg = NULL;
    int bg_recree = 0;               /* le fond a-t-il été créé à l'instant ? */
    if (g_clip_bg_live && g_clip_bg_stack == stack) {
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i] == g_clip_bg_live) { bg = g_clip_bg_live; break; }
    }

    /* Absent : on le recrée depuis la copie. Et on le retient comme fond
     * vivant de cette pile, pour que coller une deuxième fois la même carte
     * réutilise ce fond au lieu d'en empiler un second. */
    if (!bg && g_clip_bg_copy) {
        bg = place_layer_clone(stack, g_clip_bg_copy, OBJ_BACKGROUND, NULL, NULL);
        if (!bg) return NULL;
        bg_recree       = 1;
        g_clip_bg_live  = bg;
        g_clip_bg_stack = stack;
    }
    if (!bg) return NULL;

    Object *cur = g_current_card;
    if (cur && cur->owner != stack) cur = NULL;

    Object *c = place_layer_clone(stack, g_clipboard, OBJ_CARD, bg, cur);
    if (!c) return NULL;

    /* Les bgtexts du clone désignent les champs de la COPIE du fond ; il faut
     * les faire pointer sur ceux du fond réellement utilisé. Sans effet quand
     * les deux se confondent. */
    remap_bgtexts(c, g_clip_bg_copy, bg);

    /* Les icônes ensuite : elles ne dépendent pas du fond, mais leurs numéros
     * peuvent changer. Le fond n'est réétiqueté QUE s'il vient d'être recréé :
     * réutilisé, il appartient aussi aux autres cartes de la pile, et le
     * toucher changerait leurs icônes à toutes. */
    transplant_icons(stack, c, bg_recree ? bg : NULL);
    return c;
}

/* Dupliquer : ne passe pas par le presse-papiers, qui garde donc ce qu'il
 * avait. C'est ce qu'on attend d'une commande « Dupliquer ». Le fond est
 * partagé, jamais dupliqué : la carte reste dans sa pile. */
Object *hc_duplicate_card(Object *card)
{
    if (!card || card->type != OBJ_CARD || !card->owner) return NULL;
    return place_layer_clone(card->owner, card, OBJ_CARD, card->bg, card);
}

/* Une pile se ferme : oublier ce que le presse-papiers y emprunte.
 *
 * Seul le fond VIVANT est concerné — la carte et la copie du fond sont
 * possédées et survivent. Sans cet oubli, un pointeur mort resterait comparé
 * aux fonds d'une pile future, et une adresse réemployée le ferait passer
 * pour un fond qu'il n'est pas. */
void hc_clipboard_stack_closing(Object *stack)
{
    if (!stack || g_clip_bg_stack != stack) return;
    g_clip_bg_live  = NULL;
    g_clip_bg_stack = NULL;
}

/* Le presse-papiers, tel quel — carte ou part. Ne pas libérer. */
Object *hc_clipboard_part(void) { return g_clipboard; }

/* Un seul presse-papiers pour deux natures : ces deux fonctions disent laquelle
 * il porte. Sans elles, « Coller » devrait déduire du contexte ce qu'il pose,
 * et se tromperait dès qu'une carte a été copiée puis l'outil Bouton choisi. */
int hc_clipboard_has_card(void)
{
    return (g_clipboard && g_clipboard->type == OBJ_CARD) ? 1 : 0;
}

int hc_clipboard_has_part(void)
{
    return (g_clipboard && (g_clipboard->type == OBJ_BUTTON ||
                            g_clipboard->type == OBJ_FIELD)) ? 1 : 0;
}

void hc_clipboard_clear(void)
{
    if (g_clipboard) hc_free(g_clipboard);
    g_clipboard = NULL;
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

static Object *find_part(Object *owner, ObjType type, const char *name)
{
    if (!owner) return NULL;
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (p->type == type && p->name && ci_equal(p->name, name)) return p;
    }
    return NULL;
}

/* part par id absolu */
static Object *find_part_by_id(Object *owner, ObjType type, int id)
{
    if (!owner) return NULL;
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (p->type == type && p->id == id) return p;
    }
    return NULL;
}

/* part par rang parmi les objets de ce type (1-based) */
static Object *find_part_by_rank(Object *owner, ObjType type, int rank)
{
    if (!owner || rank < 1) return NULL;
    int n = 0;
    for (int i = 0; i < owner->nparts; i++) {
        Object *p = owner->parts[i];
        if (p->type == type && ++n == rank) return p;
    }
    return NULL;
}

static Object *find_card_by_name(Object *stack, const char *name)
{
    if (!stack) return NULL;
    for (int i = 0; i < stack->nparts; i++) {
        Object *p = stack->parts[i];
        if (p->type == OBJ_CARD && p->name && ci_equal(p->name, name)) return p;
    }
    return NULL;
}

/* ---- cartes : l'ordre, pour « go next card » ---- */

static int card_count(Object *stack)
{
    int n = 0;
    if (!stack) return 0;
    for (int i = 0; i < stack->nparts; i++)
        if (stack->parts[i]->type == OBJ_CARD) n++;
    return n;
}

/* n-ième carte, 0-based, en ne comptant que les cartes */
static Object *nth_card(Object *stack, int n)
{
    if (!stack || n < 0) return NULL;
    for (int i = 0; i < stack->nparts; i++) {
        if (stack->parts[i]->type != OBJ_CARD) continue;
        if (n-- == 0) return stack->parts[i];
    }
    return NULL;
}

static int card_index(Object *stack, Object *card)
{
    int n = 0;
    if (!stack || !card) return -1;
    for (int i = 0; i < stack->nparts; i++) {
        if (stack->parts[i]->type != OBJ_CARD) continue;
        if (stack->parts[i] == card) return n;
        n++;
    }
    return -1;
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
static Object *resolve(const char *ref)
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
    if (ci_word(ref, "bg") || ci_word(ref, "background")) {
        want_bg = 1;
        ref = skip_spaces(strchr(ref, ' ') ? strchr(ref, ' ') : ref + strlen(ref));
        if (!*ref) return bg;   /* « background » seul = le fond de la carte courante */

        /* « background "second" », « bg id 4 », « background 2 » : le fond
         * lui-même, et non un objet posé dessus. Sans ceci, « background »
         * n'est qu'un préfixe pour « bg button … » et un fond nommé reste
         * introuvable. */
        if (*ref == '"') {
            char nm[128];
            quoted(ref, nm, sizeof nm);
            for (int i = 0; stack && i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_BACKGROUND &&
                    stack->parts[i]->name && ci_equal(stack->parts[i]->name, nm))
                    return stack->parts[i];
            return NULL;
        }
        if (ci_word(ref, "id")) {
            int wanted = atoi(skip_spaces(ref + 2));
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
                int n = atoi(num) - 1;          /* 1-based en HyperTalk */
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
            char nm[128];
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
            char nm[128];
            quoted(after, nm, sizeof nm);
            return find_card_by_name(stack, nm);
        }
        if (ci_word(after, "id")) {                    /* card id N */
            const char *a = skip_spaces(after + 2);
            int wanted;
            if (isdigit((unsigned char)*a)) wanted = atoi(a);
            else { char v[128]; eval_id_token(a, v, sizeof v); wanted = atoi(v); }
            for (int i = 0; i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_CARD && stack->parts[i]->id == wanted)
                    return stack->parts[i];
            return NULL;
        }
        if (isdigit((unsigned char)*after))
            return nth_card(stack, atoi(after) - 1);   /* 1-based en HyperTalk */

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
            char nm[256];
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
                Object *c = nth_card(stack, atoi(nm) - 1);
                if (c) return c;
            }
            Object *c = find_card_by_name(stack, nm);
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
    if (ci_word(ref, "first")) return nth_card(stack, 0);
    if (ci_word(ref, "last"))  return nth_card(stack, card_count(stack) - 1);

    if (ci_word(ref, "stack")) {
        const char *after = skip_spaces(ref + 5);
        if (!*after) return stack;
        if (*after == '"') {                 /* « stack "Essai" » */
            char nm[128];
            quoted(after, nm, sizeof nm);
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
            wanted = atoi(a);
        } else {
            /* « field id n » : l'identifiant vient d'une variable. */
            char v[128];
            eval_id_token(a, v, sizeof v);
            wanted = atoi(v);
        }
        Object *o = find_part_by_id(card, t, wanted);
        if (!o) o = find_part_by_id(bg, t, wanted);
        return o;
    }

    /* --- button N (par rang, 1-based) --- */
    if (isdigit((unsigned char)*ref)) {
        int n = atoi(ref);
        Object *o = find_part_by_rank(want_bg ? bg : card, t, n);
        if (!o && !want_bg) o = find_part_by_rank(bg, t, n);
        return o;
    }

    char nm[256];
    nm[0] = '\0';

    if (*ref == '"') {
        quoted(ref, nm, sizeof nm);
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
            int n = atoi(nm);
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
            free(f->v[i].val);
            f->v[i].val = dupstr(val);
            return;
        }
    if (f->n == f->cap) {
        int cap = f->cap ? f->cap * 2 : 8;
        Var *p = realloc(f->v, (size_t)cap * sizeof *p);
        if (!p) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
        f->v = p; f->cap = cap;
    }
    f->v[f->n].name = dupstr(name);
    f->v[f->n].val  = dupstr(val);
    f->n++;
}

static void frame_declare_global(Frame *f, const char *name)
{
    if (!f || !*name || frame_has_global(f, name)) return;
    if (f->ngl == f->capgl) {
        int cap = f->capgl ? f->capgl * 2 : 4;
        char **p = realloc(f->gl, (size_t)cap * sizeof *p);
        if (!p) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
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
        if (!p) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
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

static int ci_cmp(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
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
static char g_item_delim = ',';
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
static char chunk_sep(ChunkType t)
{
    if (t == CH_ITEM) return g_item_delim;
    if (t == CH_LINE) return '\n';
    if (t == CH_WORD) return ' ';
    return '\0';
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
    return hct_chunk_compte(s, vers_sorte_v3(t), g_item_delim);
}

/* Bornes du n-ième morceau (1-based). Renvoie 0 s'il n'existe pas. */
static int chunk_span1(const char *s, ChunkType t, int n, int *b, int *e)
{
    if (t == CH_NONE) return 0;
    HctBornes r = hct_chunk_bornes(s, vers_sorte_v3(t), n, 0, g_item_delim);
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
        char v[128]; double d = 0;
        eval_expr(ia, v, sizeof v); as_num(v, &d); *a = (int)d;
        if (ib && *skip_spaces(ib)) {
            eval_expr(ib, v, sizeof v); as_num(v, &d); *b = (int)d;
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

static int runs_room(struct RunList *rl, int need)
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

/* Traduit un nom de couleur, ou « #RRGGBB », ou « r,v,b », en 0xRRGGBB.
 *
 * Les noms sont ceux qu'on écrit spontanément dans un script, en français
 * comme en anglais : une pile écrite ici doit rester lisible par qui la
 * relira. Renvoie HC_COLOR_INHERIT si le mot n'est pas une couleur — la plage
 * reste alors muette sur cet attribut, plutôt que de virer au noir. */
static int color_from_name(const char *v)
{
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
    };
    for (unsigned i = 0; i < sizeof table / sizeof *table; i++)
        if (ci_equal(v, table[i].nom)) return table[i].rgb;

    if (*v == '#') return (int)strtol(v + 1, NULL, 16);

    /* « 255,128,0 » : la forme qu'emploient les scripts qui calculent leurs
     * couleurs, et celle que rend « the textColor ». */
    if (strchr(v, ',')) {
        int r = 0, g = 0, b = 0;
        if (sscanf(v, "%d , %d , %d", &r, &g, &b) == 3) {
            if (r < 0)   r = 0;
            if (r > 255) r = 255;
            if (g < 0)   g = 0;
            if (g > 255) g = 255;
            if (b < 0)   b = 0;
            if (b > 255) b = 255;
            return (r << 16) | (g << 8) | b;
        }
    }
    if (isdigit((unsigned char)*v)) return (int)strtol(v, NULL, 0);
    return HC_COLOR_INHERIT;
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
            pos += snprintf(out + pos, outlen - pos, "%s%s", pos ? "," : "", T[i].n);
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
    char *base = arena_buf();

    int inner_st, inner_en;
    Object *inner = chunk_target(rest, &inner_st, &inner_en);
    if (inner) {                                   /* morceau de morceau */
        fld = inner;
        base_off = inner_st;
        int len = inner_en - inner_st;
        if (len < 0) len = 0;
        snprintf(base, HC_VAL, "%.*s", len, hc_field_text(fld) + inner_st);
    } else {
        fld = resolve(rest);
        if (!fld || fld->type != OBJ_FIELD) return NULL;
        snprintf(base, HC_VAL, "%s", hc_field_text(fld));
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

/* Écrit dans un conteneur : champ, variable, ou morceau de l'un des deux.
 * mode : 0 remplacer, 1 après, 2 avant, 3 supprimer.
 * L'appel est récursif, donc « word 2 of line 3 of field "notes" » marche.
 * Renvoie 1 si la destination a été reconnue.
 */
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
                    char sep = chunk_sep(ct);
                    const char *ft = hc_field_text(cf);
                    int fl = (int)strlen(ft);
                    if (sep) {
                        if      (cen < fl && ft[cen] == sep)  cen++;
                        else if (cst > 0  && ft[cst-1] == sep) cst--;
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

        char sepstr[2] = { chunk_sep(ct), '\0' };
        char *neuf = arena_buf();

        if (!chunk_span(base, ct, a, b, &st, &en)) {
            if (mode == 3) return 1;            /* rien à supprimer */
            /* Le rang visé dépasse le contenu : compléter avec des éléments
             * vides jusqu'à ce rang, comme le fait HyperTalk. */
            if (sepstr[0] && a > 0) {
                int have = 0;
                if (*base) {
                    have = 1;
                    for (const char *q = base; *q; q++)
                        if (*q == sepstr[0]) have++;
                }
                int need = (have == 0) ? (a - 1) : (a - have);
                int pos = 0;
                pos += snprintf(neuf + pos, HC_VAL - pos, "%s", base);
                for (int k = 0; k < need && pos < (int)HC_VAL - 2; k++)
                    neuf[pos++] = sepstr[0];
                neuf[pos] = '\0';
                snprintf(neuf + pos, HC_VAL - pos, "%s", val);
            } else {
                snprintf(neuf, HC_VAL, "%s%s%s", base,
                         (*base && sepstr[0]) ? sepstr : "", val);
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
        if      (mode == 1) snprintf(merged, HC_VAL, "%s%s", old, val);
        else if (mode == 2) snprintf(merged, HC_VAL, "%s%s", val, old);
        else if (mode == 3) merged[0] = '\0';
        else                snprintf(merged, HC_VAL, "%s", val);
        hc_set_field_text(o, merged);
        notify_field(o);
        return 1;
    }
    if (o) return 0;                            /* un bouton n'est pas un conteneur */

    char vname[128];
    const char *after = next_word(ref, vname, sizeof vname);
    if (vname[0] && vname[0] != '"' && !*skip_spaces(after)) {
        const char *old = var_get(vname);
        if (!old) old = "";
        if      (mode == 1) snprintf(merged, HC_VAL, "%s%s", old, val);
        else if (mode == 2) snprintf(merged, HC_VAL, "%s%s", val, old);
        else if (mode == 3) merged[0] = '\0';
        else                snprintf(merged, HC_VAL, "%s", val);
        var_set(vname, merged);
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
        if (len < 511) args[n][len++] = *p;
    }
    args[n][len] = '\0';
    if (*skip_spaces(args[n])) n++;
    return n;
}

static void format_date(char *out, int outlen, int mode)
{
    time_t now = time(NULL);
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
static int parse_datetime(const char *s, struct tm *tm)
{
    if (!s) return 0;

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
        time_t now = time(NULL);
        struct tm *lt = localtime(&now);
        if (!lt) return 0;
        year = lt->tm_year + 1900; mon = lt->tm_mon; day = lt->tm_mday;
    } else {
        return 0;
    }

    if (mon < 0 || day < 0) return 0;
    if (year < 0) {                      /* année tue : celle en cours */
        time_t now = time(NULL);
        struct tm *lt = localtime(&now);
        if (!lt) return 0;
        year = lt->tm_year + 1900;
    }
    if (year < 100) year += (year < 70) ? 2000 : 1900;   /* comme le Macintosh */

    if (hh < 0) hh = 0;
    if (meridian == 2 && hh < 12) hh += 12;              /* 3 PM → 15 */
    if (meridian == 1 && hh == 12) hh = 0;               /* 12 AM → 0 */

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
            snprintf(out, outlen, "%c", g_item_delim); return 1;
        }
        /* the tool : l'outil courant, sous la forme « brush tool ». C'est
         * l'hôte qui le sait ; s'il ne répond pas, on annonce l'outil main,
         * celui d'HyperCard au repos. */
        if (ci_equal(name, "tool")) {
            const char *t = host_global("tool");
            snprintf(out, outlen, "%s", (t && *t) ? t : "browse tool");
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
            const char *t = hc_field_text(g_sel_field);
            int line = 1;
            for (int i = 0; i < g_sel_start && t[i]; i++)
                if (t[i] == '\n') line++;
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
                     g_sel_start + 1, g_sel_start + g_sel_len,
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
                     g_found_start + 1, g_found_start + g_found_len,
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
                pos += snprintf(out + pos, outlen - pos, "%s%s", i ? "," : "", g_params[i]);
            return 1;
        }
        if (ci_equal(name, "time")) { format_date(out, outlen, 3); return 1; }
        if (ci_equal(name, "seconds") || ci_equal(name, "secs")) {
            /* comme sur Macintosh : secondes depuis le 1er janvier 1904 */
            snprintf(out, outlen, "%lld", (long long)time(NULL) + 2082844800LL);
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
            time_t now = time(NULL);
            if (!t0_pris) { t0 = now; t0_pris = 1; }
            snprintf(out, outlen, "%lld", (long long)(now - t0) * 60);
            return 1;
        }
    }

    /* --- arguments : « of <expr> » ou « (a, b, c) » --- */
    char (*raw)[HC_VAL]  = arena_rows(8);
    char (*vals)[HC_VAL] = arena_rows(8);
    int nargs = 0;
    const char *q = skip_spaces(after);

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
    } else if (ci_word(q, "of")) {
        snprintf(raw[0], sizeof raw[0], "%s", q + 2);
        nargs = 1;
    } else {
        return 0;
    }

    for (int i = 0; i < nargs; i++) eval_expr(raw[i], vals[i], sizeof vals[i]);

    double a = 0, b = 0;
    if (nargs > 0) as_num(vals[0], &a);
    if (nargs > 1) as_num(vals[1], &b);
    (void)b;

    /* --- une entrée --- */
    if (ci_equal(name, "length")) { snprintf(out, outlen, "%d", (int)strlen(vals[0])); return 1; }
    if (ci_equal(name, "abs"))    { put_num(a < 0 ? -a : a, out, outlen); return 1; }
    if (ci_equal(name, "trunc"))  { put_num((double)(long long)a, out, outlen); return 1; }
    if (ci_equal(name, "round"))  { put_num(a < 0 ? -(double)(long long)(-a + 0.5)
                                                  :  (double)(long long)( a + 0.5), out, outlen); return 1; }
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
    if (ci_equal(name, "random")) {
        static int seeded = 0;
        if (!seeded) { srand((unsigned)time(NULL)); seeded = 1; }
        int n = (int)a;
        snprintf(out, outlen, "%d", n > 0 ? (rand() % n) + 1 : 0); return 1;
    }
    if (ci_equal(name, "charToNum")) { snprintf(out, outlen, "%d", (unsigned char)vals[0][0]); return 1; }
    if (ci_equal(name, "numToChar")) { snprintf(out, outlen, "%c", (int)a); return 1; }

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
        for (int i = 0; nl && i + nl <= hl; i++)
            if (ci_nequal(vals[1] + i, vals[0], nl)) { pos = i + 1; break; }
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
        char (*uargv)[HC_VAL] = arena_rows(8);
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
static Object *owning_stack(Object *o)
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
        v[n++] = atoi(s);
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
    } else if (ci_equal(prop, "left"))   { o->x = atoi(val); }
    else if (ci_equal(prop, "top"))      { o->y = atoi(val); }
    else if (ci_equal(prop, "right"))    { o->w = atoi(val) - o->x; }
    else if (ci_equal(prop, "bottom"))   { o->h = atoi(val) - o->y; }
    else if (ci_equal(prop, "width"))    { o->w = atoi(val); }
    else if (ci_equal(prop, "height"))   { o->h = atoi(val); }
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

static int icon_id_is_builtin(int id)
{
    return gIconBuiltinCheck ? gIconBuiltinCheck(id) : 0;
}

int hc_resolve_icon(const char *text)
{
    if (!text) return 0;
    if (gIconResolver) return gIconResolver(text);
    return atoi(text);   /* sans résolveur : les numéros seulement */
}

static int is_prop_name(const char *w, int len)
{
    static const char *tab[] = {
        "rect", "rectangle", "topleft", "botright", "bottomright",
        "left", "top", "right", "bottom", "width", "height",
        "loc", "location", "id", "name", "visible", "showname", "shownname",
        "enabled",
        "icon", "selectedline", "selectedlines", "locktext", "widemargins",
        "fixedlineheight", "showlines", "autotab", "dontsearch", "sharedtext",
        "sharedhilite",
        "textalign", "autoselect", "multiplelines", "dontwrap", "textcolor",
        "marked",
        "selectedtext", "selectedchunk",
        "textfont", "scroll", "textstyle", "hilite", "highlight", "autohilite",
        "textsize", "textheight", "script", "text", "contents", "style", NULL
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
 * `shortf` vaut 1 pour « the short name of » : seul `name` s'en sert. */
static int obj_prop_read(Object *o, const char *prop, int shortf,
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
        if (shortf) snprintf(out, outlen, "%s", o->name ? o->name : "");
        else        hc_describe(o, out, outlen);
        return 1;
    }
    if (ci_equal(prop, "visible")) { snprintf(out, outlen, "%s", o->visible ? "true" : "false"); return 1; }
    if (ci_equal(prop, "showname") || ci_equal(prop, "shownname")) { snprintf(out, outlen, "%s", o->showname ? "true" : "false"); return 1; }
    if (ci_equal(prop, "enabled")) { snprintf(out, outlen, "%s", o->enabled ? "true" : "false"); return 1; }
    if (ci_equal(prop, "icon")) { snprintf(out, outlen, "%d", o->icon); return 1; }
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
                     g_sel_start + 1, g_sel_start + g_sel_len,
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
    return 0;
}

static void term_value_body(const char *t, char *out, int outlen)
{
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

        int shortf = 0;
        if (ci_word(w, "short")) { shortf = 1; w = skip_spaces(w + 5); }
        else if (ci_word(w, "long")) { w = skip_spaces(w + 4); }

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
                if (o && obj_prop_read(o, prop, shortf, out, outlen))
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
        else if (op == 'd') r = (b != 0) ? (double)(long long)(a / b) : 0;
        else                r = (b != 0) ? a - b * (double)(long long)(a / b) : 0;
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
    return d == (double)(long long)d;
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

/* La première carte d'un fond, dans l'ordre de la pile.
 * HyperCard ne se tient jamais SUR un fond : « go bg 2 » mène à sa
 * première carte, et c'est ce que rend cette fonction pour les formes de
 * navigation. */
static Object *v3_carte_du_bg(Object *stack, Object *bgcible)
{
    if (!bgcible) return NULL;
    int n = card_count(stack);
    for (int j = 0; j < n; j++) {
        Object *d = nth_card(stack, j);
        if (d && d->bg == bgcible) return d;
    }
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
static Object *v3_cible(HctContexte *ctx, const HctNoeud *n)
{
    if (n->nfils < 1) return NULL;
    const HctNoeud *dernier = n->fils[n->nfils - 1];
    if (dernier->genre != HCTN_OBJET) return NULL;
    return hct_resout(ctx, dernier);
}

/* Le nœud du désignateur, ou NULL quand il n'y en a pas. */
static const HctNoeud *v3_designateur(const HctNoeud *n)
{
    if (n->designateur != HCT_DES_NOM &&
        n->designateur != HCT_DES_RANG &&
        n->designateur != HCT_DES_ID) return NULL;
    return n->nfils >= 1 ? n->fils[0] : NULL;
}

/* ------------------------------------------------------------ l'entrée */

static Object *hct_resout(HctContexte *ctx, const HctNoeud *n)
{
    if (!n || n->genre != HCTN_OBJET) return NULL;

    Object *card  = g_current_card;
    Object *bg    = card ? card->bg : NULL;
    Object *stack = card ? card->owner : NULL;

    /* Une cible explicite déplace le contexte : « bg field "x" of card 3 »
     * cherche le champ dans la carte 3, pas dans la carte courante. */
    Object *cible = v3_cible(ctx, n);
    if (cible) {
        if (cible->type == OBJ_CARD) {
            card = cible;
            bg   = card->bg;
            /* La pile suit la carte : « field 1 of card 3 of stack "x" »
             * cherche dans la pile de CETTE carte. */
            stack = owning_stack(card);
        }
        else if (cible->type == OBJ_BACKGROUND) { bg = cible; }
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
                case HCT_DES_ID:   return v3_bg_par_id(stack, atoi(val));
                case HCT_DES_RANG: {
                    /* Un désignateur peut être un nom aussi bien qu'un rang :
                     * « bg i » où i vaut 2, mais aussi « bg commun ». On
                     * regarde CE QUI SORT de l'évaluation, comme resolve. */
                    int l = (int)strlen(val);
                    if (l > 0 && (int)strspn(val, "0123456789") == l)
                        return v3_nth_bg(stack, atoi(val));
                    return v3_bg_par_nom(stack, val);
                }
                case HCT_DES_ORDINAL: {
                    int total = 0;
                    for (int i = 0; stack && i < stack->nparts; i++)
                        if (stack->parts[i]->type == OBJ_BACKGROUND) total++;
                    Object *b = v3_nth_bg(stack, v3_rang_ordinal(n->ordinal, total));
                    return v3_carte_du_bg(stack, b);
                }
                case HCT_DES_RELATIF: {
                    if (n->relatif == HCT_REL_CE) return card;
                    /* « go next card » depuis la dernière mène à la PREMIÈRE :
                     * HyperCard boucle, et les piles d'époque s'en servent pour
                     * feuilleter sans jamais tester les bords. Sans le modulo,
                     * nth_card rendait NULL et la commande échouait en silence
                     * sur les deux extrémités.
                     *
                     * C'est déjà ce que fait la branche des FONDS, quelques
                     * lignes plus haut — les deux doivent s'accorder. */
                    int nc = card_count(stack);
                    int i  = card_index(stack, card);
                    if (nc <= 0 || i < 0) return NULL;
                    int pas = (n->relatif == HCT_REL_SUIVANT) ? +1 : -1;
                    return nth_card(stack, ((i + pas) % nc + nc) % nc);
                }
                default: return bg;
            }

        case HCT_OBJ_CARD:
            switch (n->designateur) {
                case HCT_DES_NOM: return find_card_by_name(stack, val);
                case HCT_DES_ID: {
                    int w = atoi(val);
                    for (int i = 0; stack && i < stack->nparts; i++)
                        if (stack->parts[i]->type == OBJ_CARD &&
                            stack->parts[i]->id == w)
                            return stack->parts[i];
                    return NULL;
                }
                case HCT_DES_RANG: {
                    int l = (int)strlen(val);
                    if (l > 0 && (int)strspn(val, "0123456789") == l)
                        return nth_card(stack, atoi(val) - 1);
                    return find_card_by_name(stack, val);
                }
                case HCT_DES_ORDINAL:
                    return nth_card(stack,
                        v3_rang_ordinal(n->ordinal, card_count(stack)) - 1);
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
                    int nc = card_count(stack);
                    int i  = card_index(stack, card);
                    if (nc <= 0 || i < 0) return NULL;
                    int pas = (n->relatif == HCT_REL_SUIVANT) ? +1 : -1;
                    return nth_card(stack, ((i + pas) % nc + nc) % nc);
                }
                default: return card;
            }

        case HCT_OBJ_BUTTON:
        case HCT_OBJ_FIELD:
        case HCT_OBJ_PART: {
            ObjType t = (n->typeobj == HCT_OBJ_BUTTON) ? OBJ_BUTTON : OBJ_FIELD;

            /* La portée décide où chercher. Sans portée explicite, HyperCard
             * cherche d'abord sur la carte, puis se rabat sur le fond — c'est
             * ce que fait resolve, et beaucoup de piles en dépendent. */
            Object *premier  = (n->portee == HCT_PORTEE_FOND) ? bg : card;
            Object *repli    = (n->portee == HCT_PORTEE_AUCUNE) ? bg : NULL;

            switch (n->designateur) {
                case HCT_DES_ID: {
                    int w = atoi(val);
                    Object *o = find_part_by_id(premier, t, w);
                    if (!o && repli) o = find_part_by_id(repli, t, w);
                    return o;
                }
                case HCT_DES_NOM: {
                    Object *o = find_part(premier, t, val);
                    if (!o && repli) o = find_part(repli, t, val);
                    return o;
                }
                case HCT_DES_RANG: {
                    int l = (int)strlen(val);
                    if (l > 0 && (int)strspn(val, "0123456789") == l) {
                        int r = atoi(val);
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

    if (obj->typeobj == HCT_OBJ_CARD) { *out = card_count(stack); return 1; }

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

static int g_v3_recours_prof = 0;

static void v3_note(const char *quoi, const char *nom);

static int v3_recours(void *d, const HctNoeud *n, HctValeur *out)
{
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

    /* Un comptage d'objets se fait ici, sans repasser par le texte. Avant
     * le relevé : ce n'est plus un retour vers l'ancien interpréteur. */
    if (n->genre == HCTN_OF && n->nfils >= 2 &&
        n->fils[0] && n->fils[0]->genre == HCTN_IDENT) {
        char quoi[32];
        hct_texte(&n->fils[0]->jeton, quoi, sizeof quoi);
        int compte;
        if (ci_equal(quoi, "number") && v3_nombre_objets(n->fils[1], &compte)) {
            char b[24];
            snprintf(b, sizeof b, "%d", compte);
            *out = hct_val_texte(b);
            return 1;
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
        return 1;
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
    if (strcmp(val, txt) == 0) {
        char *avec_the = arena_buf();
        snprintf(avec_the, HC_VAL, "the %s", txt);
        term_value(avec_the, val, HC_VAL);

        /* Si même avec « the » rien de neuf ne sort, on rend le texte
         * d'origine plutôt que « the value of x » : c'est ce que faisait
         * l'ancien évaluateur, et un script peut s'appuyer dessus. */
        if (strcmp(val, avec_the) == 0)
            snprintf(val, HC_VAL, "%s", txt);
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
    char (*uargv)[HC_VAL] = arena_rows(8);
    if (nargs > 8) nargs = 8;
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
    "filled", "lineSize", "pattern", "brush",
    NULL
};

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
                 g_sel_start + 1, g_sel_start + g_sel_len,
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
                 g_found_start + 1, g_found_start + g_found_len,
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
            pos += snprintf(buf + pos, (size_t)HC_VAL - pos, "%s%s", i ? "," : "", g_params[i]);
        *out = hct_val_texte(buf);
        return 1;
    }
    if (ci_equal(nom, "seconds") || ci_equal(nom, "secs")) {
        char petit[24];
        snprintf(petit, sizeof petit, "%lld", (long long)time(NULL) + HC_MAC_EPOCH);
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
        time_t now = time(NULL);
        if (!t0_pris) { t0 = now; t0_pris = 1; }
        char petit[24];
        snprintf(petit, sizeof petit, "%lld", (long long)(now - t0) * 60);
        *out = hct_val_texte(petit);
        return 1;
    }
    return 0;
}

static int v3_fonction(void *d, const char *nom, HctValeur *args, int nargs,
                       HctValeur *out)
{
    (void)d;
    /* itemDelimiter : demandé avant chaque découpage en items. On le sert
     * directement, c'est une globale de hc_core.c. Avant toute allocation :
     * c'est le cas le plus fréquent, et il n'a besoin de rien. */
    if (ci_equal(nom, "itemDelimiter")) {
        char sep[2] = { g_item_delim, 0 };
        *out = hct_val_texte(sep);
        return 1;
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
            return 1;
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
        int i = (int)hct_vers_nombre(args[0].txt);
        *out = hct_val_texte((i >= 0 && i < g_nparams) ? g_params[i] : "");
        return 1;
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
        term_value(appel, buf, HC_VAL);
        if (strcmp(buf, appel) != 0 && strcmp(buf, nom) != 0) {
            v3_note("fonction", nom);      /* term_value a fourni la réponse */
            *out = hct_val_texte(buf);
            ARENA_FREE;
            return 1;
        }
        if (v3_fonction_pile(nom, args, 0)) {
            *out = hct_val_texte(g_result);
            ARENA_FREE;
            return 1;
        }
        ARENA_FREE;
        return 0;
    }
    /* Fonctions du monde à un argument NUMÉRIQUE — param(n) et consorts.
     * On s'en tient au numérique : reconstruire un argument textuel serait
     * fragile dès qu'il contient un guillemet. Le reste passe par le
     * recours, qui dispose du texte source exact. */
    if (nargs == 1 && hct_est_nombre(args[0].txt)) {
        char appel[160];
        snprintf(appel, sizeof appel, "%s(%s)", nom, args[0].txt);
        buf[0] = '\0';
        if (call_function(appel, buf, HC_VAL)) {
            v3_note("fonction", nom);      /* call_function a fourni la réponse */
            *out = hct_val_texte(buf);
            ARENA_FREE;
            return 1;
        }
    }
    if (v3_fonction_pile(nom, args, nargs)) {
        *out = hct_val_texte(g_result);
        ARENA_FREE;
        return 1;
    }
    ARENA_FREE;
    return 0;
}

/* --- ponts vers l'hôte ----------------------------------------------- */

/* L'hôte reçoit le nœud, hct_resout rend l'objet. Plus aucun texte
 * reconstitué sur ce chemin. */
static void *v3_resout(void *d, const HctNoeud *ref, HctContexte *ctx)
{
    (void)d;

    /* « card window » n'est pas une carte, c'est la fenêtre de la pile — voir
     * v3_est_fenetre. On refuse ici plutôt que de laisser hct_resout évaluer
     * « window » comme un rang : cette évaluation DIFFUSE le mot comme un
     * message dans toute la hiérarchie avant d'échouer. Le nœud « of » qui
     * nous surplombe est ensuite servi par v3_fenetre_prop, dans v3_recours. */
    if (v3_est_fenetre(ref)) return NULL;

    return hct_resout(ctx, ref);
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
 * Seuls les champs sont des conteneurs : un bouton rend 0, et l'exécuteur se
 * rabat alors sur exec_stmt, qui saura dire pourquoi.
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
 * L'ancien exécuteur appelle host_idle() à chaque tour de repeat (voir
 * exec_block). Sans l'équivalent ici, « repeat while the mouse is down » ne
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

static void exec_stmt(Object *me, const char *s);   /* défini plus bas */

/* --- recours pour les COMMANDES ---
 *
 * hct_exec exécute lui-même ce qui ne touche pas au monde : put, get, global,
 * l'arithmétique, les structures de contrôle, les sorties. Tout le reste —
 * go, set, show, answer, visual, les soixante autres — lui revient ici.
 *
 * On ne reçoit pas des opérandes évalués mais le NŒUD, ce qui permet de
 * retrouver la ligne d'origine dans le script et de la confier à exec_stmt.
 * L'ancien interpréteur garde donc tout ce qu'il sait faire, et la frontière
 * se déplacera commande par commande sans que rien ne s'arrête.
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
 * chaînes par retour, négligeable devant l'exec_stmt qui suit. */
#define V3_RELEVE_MAX 256
struct V3Releve { char nom[48]; long n; };
static struct V3Releve g_releve[V3_RELEVE_MAX];
static int g_nreleve = 0;

static void v3_note(const char *quoi, const char *nom)
{
    char cle[48];
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

void hc_v3_bilan(void)
{
    bilan_ligne("— retours de la v3 vers l'ancien interpréteur —");
    if (!g_nreleve) { bilan_ligne("   (aucun)"); return; }
    /* tri décroissant, en place : la liste est courte */
    for (int i = 0; i < g_nreleve; i++)
        for (int j = i + 1; j < g_nreleve; j++)
            if (g_releve[j].n > g_releve[i].n) {
                struct V3Releve t = g_releve[i];
                g_releve[i] = g_releve[j]; g_releve[j] = t;
            }
    for (int i = 0; i < g_nreleve; i++)
        bilan_ligne("   %-40s %ld", g_releve[i].nom, g_releve[i].n);
}

void hc_v3_bilan_remise_a_zero(void) { g_nreleve = 0; }

/* ===================================================================
 * Remplace le v3_commande actuel de hc_core.c (le bloc qui va de
 * « static int v3_commande » jusqu'à sa fermeture, juste avant la
 * déclaration de v3_hote).
 *
 * Un seul changement ailleurs : « #define HC_MAX_LOOP 1000000 » est
 * aujourd'hui à la ligne 6344, donc APRÈS ce bloc, qui s'en sert pour
 * borner « wait until ». Il faut le remonter avant — près des autres
 * constantes du haut du fichier — et retirer la définition d'origine.
 * =================================================================== */

/* ------------------------------------------------- répartiteur des commandes
 *
 * Chaque verbe porté est une fonction qui reçoit le NŒUD. Ses arguments sont
 * des sous-arbres qu'on évalue avec hct_evalue, ses références d'objets des
 * nœuds que hct_resout sait résoudre. Plus de texte reconstitué, plus de
 * réanalyse par l'ancien interpréteur, et surtout plus de double évaluation :
 * dans une boucle de dessin, les coordonnées d'un « drag » étaient jusqu'ici
 * calculées par la v3, jetées, puis recalculées par exec_stmt à chaque tour.
 *
 * Une fonction rend 1 si elle a traité la commande, 0 pour laisser l'ancien
 * chemin s'en charger. Ce zéro n'est pas un échec : il sert à porter la forme
 * courante d'un verbe en laissant ses formes rares derrière, plutôt que de
 * tout porter d'un coup ou rien. Ce qui repart ainsi reste compté par
 * v3_note, donc visible au bilan.
 *
 * La TABLE est le tableau d'avancement de la migration : ce qui n'y figure
 * pas est exactement ce qu'il reste à porter avant de pouvoir supprimer
 * exec_stmt.
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
    } else if (c->genre == HCTN_CHUNK && c->nfils >= 2 && !c->ordinal) {
        const HctNoeud *cible = c->fils[c->nfils - 1];
        if (!cible || cible->genre != HCTN_OBJET) return 0;
        f = hct_resout(ctx, cible);
        if (!f || f->type != OBJ_FIELD) return 0;

        char b1[64], b2[64];
        v3_val_texte(ctx, c->fils[0], b1, sizeof b1);
        if (ctx->erreur) return 1;
        int n1 = (int)hct_vers_nombre(b1), n2 = 0;
        if (c->nfils >= 3) {
            v3_val_texte(ctx, c->fils[1], b2, sizeof b2);
            if (ctx->erreur) return 1;
            n2 = (int)hct_vers_nombre(b2);
        }
        HctBornes bo = hct_chunk_bornes(hc_field_text(f), c->sorte,
                                        n1, n2, g_item_delim);
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
    (void)ctx;
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
    if (ci_word(a, "marked")) a = skip_spaces(a + 6);   /* accepté, ignoré */

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
            if (stack->parts[i]->type == OBJ_CARD) n2++;
        if (n2 < 2) { g_atop = sauve; return 1; }

        SortItem *tab = calloc((size_t)n2, sizeof *tab);
        char **cles = calloc((size_t)n2, sizeof *cles);
        if (!tab || !cles) { free(tab); free(cles); g_atop = sauve; return 1; }

        Object *avant = g_current_card;
        int k = 0;
        for (int i = 0; i < stack->nparts; i++) {
            Object *c = stack->parts[i];
            if (c->type != OBJ_CARD) continue;
            /* Se placer SUR la carte pour évaluer sa clé : « field "nom" »
             * doit désigner le champ de celle-ci, pas de la carte de
             * départ. C'est tout le sens du tri par contenu. */
            g_current_card = c;
            ARENA_MARK;
            char *tmp = arena_buf();
            if (cle) eval_checked(cle, tmp, HC_VAL);
            cles[k] = dupstr(tmp);
            ARENA_FREE;
            tab[k].cle = cles[k]; tab[k].rang = k; tab[k].card = c;
            k++;
        }
        g_current_card = avant;

        g_sort_desc = desc; g_sort_style = style;
        qsort(tab, (size_t)n2, sizeof *tab, sort_cmp);

        /* Réécrire les cartes dans leur nouvel ordre, en laissant les
         * fonds à leur place : ils occupent aussi parts[]. */
        k = 0;
        for (int i = 0; i < stack->nparts; i++)
            if (stack->parts[i]->type == OBJ_CARD)
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

        char *src = arena_buf();
        eval_checked(cible, src, HC_VAL);

        int n2 = chunk_count(src, morceau);
        if (n2 < 2) { ARENA_FREE; g_atop = sauve; return 1; }

        SortItem *tab = calloc((size_t)n2, sizeof *tab);
        char **cles = calloc((size_t)n2, sizeof *cles);
        char **elems = calloc((size_t)n2, sizeof *elems);
        if (!tab || !cles || !elems) { free(tab); free(cles); free(elems);
                                       ARENA_FREE; g_atop = sauve; return 1; }

        for (int i = 0; i < n2; i++) {
            int b = 0, e = 0;
            chunk_span1(src, morceau, i + 1, &b, &e);
            elems[i] = malloc((size_t)(e - b) + 1);
            memcpy(elems[i], src + b, (size_t)(e - b));
            elems[i][e - b] = '\0';

            /* `each` : la variable que la clé interroge. Sans clé, on trie
             * directement sur l'élément. */
            if (cle) {
                var_set("each", elems[i]);
                ARENA_MARK;
                char *v = arena_buf();
                eval_checked(cle, v, HC_VAL);
                cles[i] = dupstr(v);
                ARENA_FREE;
            } else {
                cles[i] = dupstr(elems[i]);
            }
            tab[i].cle = cles[i]; tab[i].rang = i; tab[i].card = NULL;
        }

        g_sort_desc = desc; g_sort_style = style;
        qsort(tab, (size_t)n2, sizeof *tab, sort_cmp);

        char sep[2] = { chunk_sep(morceau), '\0' };
        char *res = arena_buf();
        res[0] = '\0';
        size_t used = 0;
        for (int i = 0; i < n2; i++) {
            const char *el = elems[tab[i].rang];
            size_t l = strlen(el);
            if (used + l + 2 >= HC_VAL) break;
            if (i) { res[used++] = sep[0]; }
            memcpy(res + used, el, l); used += l;
            res[used] = '\0';
        }

        container_set(cible, res, 0);

        for (int i = 0; i < n2; i++) { free(cles[i]); free(elems[i]); }
        free(cles); free(elems); free(tab);
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
    char (*argv)[HC_VAL] = arena_rows(16);
    int argc = 0;
    a = skip_spaces(a);
    while (*a && argc < 16) {
        char *one = arena_buf();
        int len = 0, depth = 0, inq = 0;
        while (*a && !(depth == 0 && !inq && *a == ',')) {
            if (*a == '"') inq = !inq;
            else if (!inq && *a == '(') depth++;
            else if (!inq && *a == ')') depth--;
            if (len < 511) one[len++] = *a;
            a++;
        }
        one[len] = '\0';
        eval_expr(one, argv[argc], sizeof argv[argc]);   /* contexte appelant */
        argc++;
        if (*a == ',') a = skip_spaces(a + 1); else break;
    }

    set_result("");     /* un `return` dans le gestionnaire le remplira */
    hc_send_args(target, msg, argv, argc);
    ARENA_FREE;
    return 1;
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
static int v3_cmd_set(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
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
        eval_checked(to + 2, val, HC_VAL);

        /* itemDelimiter est traité par le noyau, pas par l'hôte : c'est
         * lui qui découpe les items, et l'interface n'a rien à en savoir.
         * Une chaîne vide ou de plusieurs caractères ramène à la virgule —
         * HyperCard ne retenait qu'un caractère. */
        if (ci_equal(prop, "itemdelimiter")) {
            g_item_delim = val[0] ? val[0] : ',';
            set_result("");
            emit(HC_INFO, "   → itemDelimiter ← \"%c\"", g_item_delim);
            g_atop = sauve; return 1;
        }

        /* lockScreen suivi ici aussi : « set lockScreen to true » est
         * l'exact synonyme de « lock screen », et notify_field s'appuie
         * dessus pour ne pas redessiner mille fois pour rien. */
        if (ci_equal(prop, "lockscreen")) {
            g_ecran_verrouille = truthy(val);
            if (!g_ecran_verrouille) verrou_reveille();
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
        eval_checked(to + 2, val, HC_VAL);
    }

    /* La cible peut designer une PLAGE DE TEXTE et non un objet :
     *     set the textStyle of word 3 of line 2 of field "cal" to bold
     * resolve() ne sait chercher que des objets, d'ou l'echec historique
     * « objet introuvable : word theNewDay of line 3 ». On essaie donc
     * d'abord le morceau, avant de retomber sur la resolution d'objet. */
    {
        int cst, cen;
        Object *cf = chunk_target(refbuf, &cst, &cen);
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
                          (mask & RA_SIZE)  ? atoi(val) : 0,
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
        o->selectedline = atoi(val);
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
        o->dont_search = truthy(val); notify_field(o);
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
        o->scroll = atoi(val);
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
    } else if (ci_equal(prop, "autohilite")) {
        o->autohilite = truthy(val);
    } else if (ci_equal(prop, "textsize")) {
        o->textsize = atoi(val);
        notify_field(o);
    } else if (ci_equal(prop, "textheight")) {
        /* Zéro rétablit la valeur déduite du corps. */
        int v = atoi(val);
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

/* convert <conteneur> [from <format>] to <format> [and <format>]
 *
 * Motif hct_cmd.c : « c [from *] to * ». Comme pour set/sort/find/visual,
 * v3_reste (voir sa définition) rend le texte EXACT que lisait l'ancien
 * exécuteur — y compris son silence sur « from » : il ne l'a jamais traité
 * spécialement, ne coupant qu'au dernier « to » de premier niveau, et
 * « from » se retrouve donc dans la source, comme avant ce portage. Pas une
 * ligne de l'algorithme n'a changé.
 *
 * C'est ICI qu'a été trouvé, par test réel dans Xcode, le bug du « the »
 * avalé sans être rangé dans un nœud : « convert the date to dateItems »
 * se reconstituait « convert date to dateItems », et to_it — qui décide si
 * le résultat va dans `it` ou dans un conteneur — se trompait de branche :
 * « date » ressemblait à un conteneur, une variable de ce nom naissait, et
 * `it` ne recevait jamais rien. */
static int v3_cmd_convert(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
    size_t sauve = g_atop;
    char *mots = arena_buf();
    v3_reste(n, mots, HC_VAL);

    /* Le « to » qui compte est le dernier de premier niveau : la source
     * peut en contenir un elle-même, comme dans
     * « convert item 1 to 7 of calData() to dateItems ». */
    const char *to = NULL, *scan = mots;
    for (const char *k = find_kw(scan, "to"); k; k = find_kw(scan, "to")) {
        to = k; scan = k + 2;
    }
    if (!to) {
        emit(HC_ERR, "   !! convert sans « to » : %s", skip_spaces(mots));
        set_result("invalid date");
        g_atop = sauve; return 1;
    }

    char *src = arena_buf();
    int len = (int)(to - mots);
    if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
    memcpy(src, mots, (size_t)len); src[len] = '\0';
    while (len > 0 && isspace((unsigned char)src[len-1])) src[--len] = '\0';
    const char *srcp = skip_spaces(src);

    /* le format cible, éventuellement double : « short date and long time » */
    char spec[256];
    snprintf(spec, sizeof spec, "%s", skip_spaces(to + 2));
    char *andkw = (char *)find_kw(spec, "and");
    int f1 = DF_NONE, f2 = DF_NONE;
    if (andkw) { *andkw = '\0'; f2 = date_format_code(andkw + 4); }
    f1 = date_format_code(spec);
    if (f1 == DF_NONE) {
        emit(HC_ERR, "   !! format de date inconnu : %s", skip_spaces(to + 2));
        set_result("invalid date");
        g_atop = sauve; return 1;
    }

    char *val = arena_buf();
    eval_expr(srcp, val, HC_VAL);

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

    /* Destination : le conteneur source s'il en est un. « the date »,
     * « the long time » et les appels de fonction n'en sont pas. */
    int to_it = ci_word(srcp, "the") || strchr(srcp, '(') != NULL;
    if (to_it || !container_set(srcp, outv, 0))
        var_set("it", outv);

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

    Object *liste[512];
    int np = 0;

    if (ci_word(a, "stack") || ci_word(a, "all")) {
        if (pile)
            for (int i = 0; i < pile->nparts && np < 512; i++)
                if (pile->parts[i]->type == OBJ_CARD) liste[np++] = pile->parts[i];
    }
    else if (ci_word(a, "marked")) {
        if (pile)
            for (int i = 0; i < pile->nparts && np < 512; i++)
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
                int d = atoi(v1), f = atoi(v2);
                if (d < 1) d = 1;
                for (int i = d; i <= f && np < 512; i++) {
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
        g_atop = sauve; return 1;
    }
    if (g_host && g_host->print_cards) {
        g_host->print_cards(liste, np);
        set_result("");
    } else {
        set_result("Can't print");
        emit(HC_ERR, "   !! print : l'hôte ne sait pas imprimer");
    }
    g_atop = sauve;
    return 1;
}

static FILE *file_find(const char *nom);          /* défini plus bas */
static int   file_constant(const char *s);         /* défini plus bas */

/* read from file <nom> [at <pos>] for <n> | until <car>
 *
 * Motif hct_cmd.c : « from file e [for|until e] » — pas de place pour « at
 * <pos> », qui vit pourtant dans l'algorithme ci-dessous. Ce n'est pas un
 * oubli de ce portage : la table ne l'a jamais prévu, et « read … at … »
 * échoue donc déjà à l'analyse — toute la LIGNE devient une HCTN_ERREUR, ce
 * qui fait retomber le gestionnaire entier à l'ancien exécuteur, lequel sait
 * la lire. v3_cmd_read ne voit donc jamais de « at » dans ses fils ; sa
 * branche `at` reste correcte et prête, juste jamais empruntée tant que la
 * table n'aura pas gagné cet élément.
 *
 * Comme pour set/sort/find/print, v3_reste rend le texte EXACT que lisait
 * l'ancien exécuteur ; l'algorithme n'a pas changé d'une ligne. */
static int v3_cmd_read(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
    size_t sauve = g_atop;
    char *mots = arena_buf();
    v3_reste(n, mots, HC_VAL);

    const char *a = skip_spaces(mots);
    if (ci_word(a, "from")) a = skip_spaces(a + 4);
    if (!ci_word(a, "file")) {
        emit(HC_ERR, "   !! read : « file » attendu");
        g_atop = sauve; return 1;
    }
    a = skip_spaces(a + 4);

    /* Découper avant d'évaluer : le nom du fichier s'arrête au premier
     * mot-clé, et lui passer « at 4 for 20 » ne donnerait rien de bon. */
    const char *at  = find_kw(a, "at");
    const char *fo  = find_kw(a, "for");
    const char *unt = find_kw(a, "until");
    const char *fin = at ? at : (fo ? fo : unt);

    char *nom = arena_buf();
    {
        char brut[512];
        int len = fin ? (int)(fin - a) : (int)strlen(a);
        if (len > (int)sizeof brut - 1) len = (int)sizeof brut - 1;
        memcpy(brut, a, (size_t)len); brut[len] = '\0';
        eval_checked(brut, nom, HC_VAL);
    }

    FILE *f = file_find(nom);
    if (!f) {
        set_result("File is not open");
        emit(HC_ERR, "   !! read : fichier non ouvert : %s", nom);
        g_atop = sauve; return 1;
    }

    if (at) {
        char *pv = arena_buf();
        const char *bornes = fo ? fo : unt;
        char brut[256];
        const char *deb = skip_spaces(at + 2);
        int len = bornes ? (int)(bornes - deb) : (int)strlen(deb);
        if (len > (int)sizeof brut - 1) len = (int)sizeof brut - 1;
        memcpy(brut, deb, (size_t)len); brut[len] = '\0';
        eval_checked(brut, pv, HC_VAL);
        long lpos = atol(pv);
        /* Positif : depuis le début, et 1-based comme tout HyperTalk.
         * Négatif : depuis la fin. */
        if (lpos >= 0) fseek(f, lpos > 0 ? lpos - 1 : 0, SEEK_SET);
        else           fseek(f, lpos, SEEK_END);
    }

    char *out = arena_buf();
    int no = 0;

    if (fo) {
        char *cv = arena_buf();
        eval_checked(skip_spaces(fo + 3), cv, HC_VAL);
        long combien = atol(cv);
        while (no < HC_VAL - 1 && no < combien) {
            int c = fgetc(f);
            if (c == EOF) break;
            out[no++] = (char)c;
        }
    } else if (unt) {
        char mot[64];
        next_word(skip_spaces(unt + 5), mot, sizeof mot);
        int stop = file_constant(mot);
        if (stop == -1) {
            /* Pas une constante : un caractère, éventuellement calculé. */
            char *cv = arena_buf();
            eval_checked(skip_spaces(unt + 5), cv, HC_VAL);
            stop = cv[0] ? (unsigned char)cv[0] : '\n';
        }
        while (no < HC_VAL - 1) {
            int c = fgetc(f);
            if (c == EOF) break;
            out[no++] = (char)c;
            /* Le caractère d'arrêt fait PARTIE du texte lu : c'est ce que
             * fait HyperCard, et ce qui permet d'enchaîner les lectures
             * ligne à ligne sans perdre les séparateurs. */
            if (stop >= 0 && c == stop) break;
        }
    } else {
        /* Ni « for » ni « until » : tout le fichier. */
        while (no < HC_VAL - 1) {
            int c = fgetc(f);
            if (c == EOF) break;
            out[no++] = (char)c;
        }
    }
    out[no] = '\0';

    /* Dire la troncature plutôt que de couper en silence : un script qui
     * lit un fichier trop gros doit pouvoir s'en apercevoir, et découper
     * sa lecture en plusieurs « read ... for N ». */
    if (no >= HC_VAL - 1) {
        set_result("Value too large");
        emit(HC_ERR, "   !! read : texte tronqué à %d octets", HC_VAL - 1);
    } else {
        set_result(no > 0 ? "" : "End of file");
    }
    var_set("it", out);
    g_atop = sauve;
    return 1;
}

/* write <texte> to file <nom> [at <pos>|end|eof]
 *
 * Motif hct_cmd.c : « e to file e » — pas de « at » non plus, même remède
 * que v3_cmd_read : « write … at … » échoue à l'analyse et fait retomber le
 * gestionnaire entier à l'ancien exécuteur, donc jamais de « at » dans les
 * fils qu'on reçoit ici. */
static int v3_cmd_write(HctContexte *ctx, const HctNoeud *n)
{
    (void)ctx;
    size_t sauve = g_atop;
    char *mots = arena_buf();
    v3_reste(n, mots, HC_VAL);

    const char *a = skip_spaces(mots);
    const char *to = find_kw(a, "to");
    if (!to) {
        emit(HC_ERR, "   !! write : « to file » attendu");
        g_atop = sauve; return 1;
    }

    char *txt = arena_buf();
    {
        char *brut = arena_buf();
        int len = (int)(to - a);
        if (len > HC_VAL - 1) len = HC_VAL - 1;
        memcpy(brut, a, (size_t)len); brut[len] = '\0';
        eval_checked(brut, txt, HC_VAL);
    }

    const char *r2 = skip_spaces(to + 2);
    if (ci_word(r2, "file")) r2 = skip_spaces(r2 + 4);
    const char *at = find_kw(r2, "at");

    char *nom = arena_buf();
    {
        char brut[512];
        int len = at ? (int)(at - r2) : (int)strlen(r2);
        if (len > (int)sizeof brut - 1) len = (int)sizeof brut - 1;
        memcpy(brut, r2, (size_t)len); brut[len] = '\0';
        eval_checked(brut, nom, HC_VAL);
    }

    FILE *f = file_find(nom);
    if (!f) {
        set_result("File is not open");
        emit(HC_ERR, "   !! write : fichier non ouvert : %s", nom);
        g_atop = sauve; return 1;
    }

    if (at) {
        const char *p = skip_spaces(at + 2);
        if (ci_word(p, "end") || ci_word(p, "eof")) fseek(f, 0, SEEK_END);
        else {
            char *pv = arena_buf();
            eval_checked(p, pv, HC_VAL);
            long lpos = atol(pv);
            if (lpos >= 0) fseek(f, lpos > 0 ? lpos - 1 : 0, SEEK_SET);
            else           fseek(f, lpos, SEEK_END);
        }
    }

    fwrite(txt, 1, strlen(txt), f);
    fflush(f);      /* pour qu'un autre programme voie le texte tout de suite */
    set_result("");
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
    (void)ctx;
    if (n->nfils < 1 || n->fils[0]->genre == HCTN_ERREUR) return 0;

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
 * un service partagé plutôt qu'une ligne fabriquée pour exec_line. */
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

    int x1 = atoi(p1), y1 = 0, x2 = atoi(p2), y2 = 0;
    const char *c1 = strchr(p1, ','), *c2 = strchr(p2, ',');
    if (c1) y1 = atoi(c1 + 1);
    if (c2) y2 = atoi(c2 + 1);

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

    int x = atoi(pt), y = 0;
    const char *c = strchr(pt, ',');
    if (c) y = atoi(c + 1);

    g_visual_dirty = 1;
    { const char *t = host_global("tool");
      emit(HC_TRACE, "   ✎ click at %d,%d (%s)", x, y, t ? t : "?"); }
    if (g_host && g_host->click_at) g_host->click_at(x, y, mods);
    else emit(HC_ERR, "   !! click : l'hôte ne gère pas le clavier");
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

/* ===================================================================
 * Groupe B — à insérer dans hc_core.c juste AVANT la table V3_VERBES,
 * c'est-à-dire après v3_cmd_wait et avant « typedef int (*V3Verbe) ».
 * Les entrées à ajouter à la table sont données à la fin.
 *
 * Deux ajustements ailleurs dans le fichier :
 *
 *   - les trois variables de l'effet visuel sont définies ligne 6923,
 *     donc APRÈS ce bloc, qui s'en sert dans « go ». Il faut remonter
 *     leurs trois lignes au-dessus du répartiteur :
 *         static char g_visual_effect[64] = "";
 *         static char g_visual_speed[16]  = "";
 *         static char g_visual_image[16]  = "";
 *
 *   - file_open et marked_card_ref sont définis plus bas : les deux
 *     déclarations anticipées sont incluses ci-dessous, rien à faire.
 * =================================================================== */

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
static int v3_cmd_go(HctContexte *ctx, const HctNoeud *n)
{
    int i = v3_est_motcle(n, 0, "to") ? 1 : 0;
    if (i >= n->nfils) return 0;

    /* « go to next marked card » : le marquage filtre la navigation, et
     * hct_resout n'en sait rien — c'est marked_card_ref qui s'en charge, sur
     * le texte. Ancien chemin. */
    for (int k = i; k < n->nfils; k++) {
        char m[16];
        v3_brut(n->fils[k], m, sizeof m);
        if (ci_equal(m, "marked")) return 0;
    }

    const HctNoeud *ref = n->fils[i];

    /* ---- go to stack "X" ----
     * Une pile déjà ouverte : on s'y rend. Sinon on demande à l'hôte de
     * l'ouvrir — lui seul sait où chercher le fichier et comment lui donner
     * une fenêtre. C'est ce qui permet à une pile d'en appeler une autre. */
    if (ref->genre == HCTN_OBJET && ref->typeobj == HCT_OBJ_STACK) {
        char nom[256];
        v3_nom_pile(ctx, ref, nom, sizeof nom);
        if (ctx->erreur) return 1;

        Object *cible = find_open_stack(nom);
        if (!cible && g_host && g_host->open_stack)
            cible = g_host->open_stack(nom);
        if (!cible) return 0;              /* introuvable : ancien chemin */

        Object *prem = NULL;
        for (int k = 0; k < cible->nparts; k++)
            if (cible->parts[k]->type == OBJ_CARD) { prem = cible->parts[k]; break; }
        if (!prem) { set_result("No such card"); return 1; }

        Object *old = g_current_card;
        if (old && old->owner != cible) hc_send(old, "closeStack");
        g_current_card = prem;
        if (g_host && g_host->stack_changed) g_host->stack_changed(cible);
        hc_send(prem, "openStack");
        set_result("");
        return 1;
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

    Object *old   = g_current_card;
    Object *oldbg = old ? old->bg : NULL;
    if (old) hc_send_systeme(old, "closeCard");
    /* Changement de fond : les quatre messages, dans l'ordre d'HyperCard. */
    if (oldbg && oldbg != dst->bg) hc_send_systeme(oldbg, "closeBackground");
    g_current_card = dst;
    if (dst->bg && dst->bg != oldbg) hc_send_systeme(dst->bg, "openBackground");
    emit(HC_INFO, "   ⇒ va à la carte \"%s\"", dst->name ? dst->name : "?");
    hc_send_systeme(dst, "openCard");
    return 1;
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

    char nom[512];
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

    if (g_host && g_host->save_stack && g_host->save_stack(pile, chemin))
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
    if (!pile && demarrer && g_host && g_host->load_stack)
        pile = g_host->load_stack(nom);
    if (!pile) return 0;                  /* introuvable : ancien chemin */

    /* La retirer d'abord, dans les deux cas : « stop » n'a que cela à faire,
     * et « start » s'en sert pour la remettre en tête. */
    for (int i = 0; i < g_nusing; i++) {
        if (g_using[i] != pile) continue;
        for (int k = i; k + 1 < g_nusing; k++) g_using[k] = g_using[k+1];
        g_nusing--;
        break;
    }
    if (demarrer && g_nusing < HC_MAX_USING) g_using[g_nusing++] = pile;

    set_result("");
    return 1;
}

/* ================= entrées à ajouter à la table V3_VERBES =================
 * Dans l'ordre alphabétique de la table existante :
 *
 *     { "close",  v3_cmd_fichier },
 *     { "go",     v3_cmd_go      },
 *     { "open",   v3_cmd_fichier },
 *     { "save",   v3_cmd_save    },
 *     { "start",  v3_cmd_using   },
 *     { "stop",   v3_cmd_using   },
 * ========================================================================= */
/* ------------------------------------------------------------- la table */
/* Gestionnaire écrit dans une pile, appelé comme commande —
 * « selectline it, the name of me ».
 *
 * Le pendant exact de v3_fonction_pile. La ligne repartait à exec_stmt, qui
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
    Object *chain[8];
    int nc = build_chain(start, chain, 8);

    int trouve = 0;
    for (int i = 0; i < nc && !trouve; i++) {
        const char *end = NULL, *hdr = NULL;
        if (find_handler(chain[i]->script, nom, &end, &hdr)) trouve = 1;
    }
    if (!trouve) return 0;

    char (*argv)[HC_VAL] = arena_rows(16);
    int argc = 0;
    for (int i = 0; i < n->nfils && argc < 16; i++) {
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
    if (n->genre == HCTN_COMMANDE && n->op)
        for (int i = 0; V3_VERBES[i].verbe; i++)
            if (ci_equal(V3_VERBES[i].verbe, n->op)) {
                if (V3_VERBES[i].fn(ctx, n)) return 1;
                break;                 /* forme non portée : ancien chemin */
            }

    /* Un gestionnaire de la pile : on l'appelle avec nos arguments plutôt que
     * de rendre la ligne à exec_stmt, qui les réévaluerait. */
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
    exec_stmt(g_me, ligne);
    ARENA_FREE;
    return 1;
}

/* Défini juste après ce bloc, qui s'en sert : l'ordre de lecture met
 * l'exécution avant la table des rappels, plus lisible ainsi. */
static HctHote v3_hote(void);

/* ---- exécution d'un gestionnaire par la v3 ----
 *
 * Le pari de la transition : l'exécuteur v3 déroule l'arbre, et tout ce qu'il
 * ne sait pas faire lui-même repart vers exec_stmt par v3_commande. Rien ne
 * s'arrête donc en chemin, quelle que soit la commande rencontrée.
 *
 * On n'appelle PAS hct_appelle : il ouvre sa propre portée et y lie les
 * paramètres, alors que hc_send_args_k_body a déjà posé son cadre et lié les
 * siens par var_set. Deux liaisons dans deux magasins différents, et les
 * paramètres seraient introuvables. On exécute donc le corps directement.
 *
 * Rend 0 si le gestionnaire n'est pas dans l'arbre — l'appelant se rabat
 * alors sur exec_body. */
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

static const HctNoeud *trouve_gestionnaire(const HctNoeud *racine,
                                           const char *nom, int isfunc)
{
    if (!racine) return NULL;
    const char *kw = isfunc ? "function" : "on";

    for (int i = 0; i < racine->nfils; i++) {
        const HctNoeud *f = racine->fils[i];
        if (f->genre != HCTN_GESTIONNAIRE || f->nfils < 3) continue;
        /* Gestionnaire mal analysé : à l'ancien interpréteur, qui est plus
         * indulgent. Les autres gestionnaires du script restent à la v3. */
        if (v3_porte_une_faute(f)) continue;
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
     * pour un gestionnaire de message, exactement comme le fait exec_stmt
     * (« if the result <> empty » après un appel en dépend).
     *
     * On ne pose le résultat que si un `return` a vraiment été exécuté :
     * l'écraser systématiquement effacerait celui qu'une commande du corps
     * vient d'y déposer — « go to card 99 » y met sa plainte. */
    if (x.a_rendu && x.retour.txt) set_result(x.retour.txt);

    if (x.ctx.erreur)
        emit(HC_ERR, "   !! %s (v3, ligne %d)", x.ctx.erreur,
             x.ctx.fautif ? x.ctx.fautif->jeton.ligne : 0);

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
    int shortf = 0;

    while (*p == ' ' || *p == '\t') p++;
    for (int i = 0; ADJECTIFS[i]; i++) {
        size_t l = strlen(ADJECTIFS[i]);
        if (strncasecmp(p, ADJECTIFS[i], l) != 0) continue;
        if (p[l] != ' ' && p[l] != '\t') continue;   /* « shortcut » n'est pas « short » */
        if (i == 0) shortf = 1;                       /* seul « short » change la lecture */
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

    int ok = obj_prop_read(o, p, shortf, buf, HC_VAL);
    if (ok) *out = hct_val_texte(buf);

    ARENA_FREE;
    return ok;
}

/* --- la boîte de messages ---
 *
 * « put X » sans destination, et « put X into the message box ». HC n'a pas
 * de boîte persistante : la valeur part sur la sortie, comme le fait
 * exec_stmt, qui émet HC_MSG au même endroit.
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
 * exec_stmt le fait depuis toujours pour put et l'arithmétique ; sans cela
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

/* ==================================================================
 * eval_expr, version v3 — remplace l'ancienne
 *
 * Une réserve par appel : l'arbre naît et meurt avec l'expression. Plus
 * coûteux qu'un arbre mis en cache, mais correct — on optimisera quand on
 * aura MESURÉ, pas avant.
 * ================================================================== */

static void eval_expr(const char *s, char *out, int outlen)
{
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
/* Comme eval_expr, mais râle si l'analyseur n'a pas tout mangé.
 * C'est le garde-fou contre les fautes de frappe : sans lui, une
 * expression mal formée retombe silencieusement en littéral. */
static void eval_checked_old(const char *s, char *out, int outlen)
{
    ARENA_MARK;
    const char *p = s;
    parse_expr(&p, out, outlen);
    const char *left = skip_spaces(p);
    if (*left) {
        char shown[256];
        snprintf(shown, sizeof shown, "%s", left);
        int n = (int)strlen(shown);
        while (n > 0 && (shown[n-1] == ' ' || shown[n-1] == '\t')) shown[--n] = '\0';
        emit(HC_ERR, "   !! texte incompris, ignoré : « %s »", shown);
    }
    ARENA_FREE;
    ARENA_FREE;
}

static void exec_line(Object *me, const char *line);

/* ==================== structures de contrôle ==================== */

/* Drapeaux de sortie, tous remis à zéro par hc_send. */
static int g_exit_handler = 0;   /* exit <gestionnaire> */
static int g_exit_repeat  = 0;   /* exit repeat */
static int g_next_repeat  = 0;   /* next repeat */

 

/* faut-il interrompre le fil d'exécution courant ? */
static int flow_broken(void)
{
    return g_pass || g_exit_handler || g_exit_repeat || g_next_repeat;
}

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

/* Évalue un point pour « show … at ». HyperCard accepte les deux formes :
 *   show me at 100,120          → deux expressions séparées par une virgule
 *   show me at the mouseLoc     → une seule expression rendant « x,y »
 * On découpe donc aux virgules de premier niveau (hors guillemets et
 * parenthèses), on évalue chaque morceau, et on recolle avec une virgule.
 * Ainsi « show me at horz,vert » comme « show me at item 1 of p, 20 »
 * arrivent tous deux sous la forme « x,y » attendue par geom_write. */
static void eval_point(const char *s, char *out, int outlen)
{
    int pos = 0, depth = 0, inq = 0, n = 0;
    const char *part = skip_spaces(s);
    out[0] = '\0';

    for (const char *q = part; ; q++) {
        if (*q == '"') inq = !inq;
        else if (!inq && *q == '(') depth++;
        else if (!inq && *q == ')') depth--;

        if (*q && !(!inq && depth <= 0 && *q == ',')) continue;

        char *expr = arena_buf();
        char *val = arena_buf();
        int len = (int)(q - part);
        while (len > 0 && isspace((unsigned char)part[len-1])) len--;
        if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
        memcpy(expr, part, (size_t)len); expr[len] = '\0';
        eval_checked(expr, val, HC_VAL);

        pos += snprintf(out + pos, (size_t)(outlen - pos), "%s%s",
                        n ? "," : "", val);
        if (pos >= outlen) pos = outlen - 1;
        n++;

        if (!*q) break;
        part = skip_spaces(q + 1);
    }
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

/* Premier « else » de même niveau entre `from` et `to`. */
static int find_else(char **L, int from, int to)
{
    for (int i = from; i < to; i++) {
        const char *s = L[i];
        if (opens_inline_chain(L, i, to) ||
            opens_if(s) || opens_repeat(s)) { i = skip_block(L, i, to); continue; }
        if (ci_word(s, "else")) return i;
    }
    return -1;
}

static void exec_block(Object *me, char **L, int from, int to);
static void exec_stmt(Object *me, const char *s);

/* `head` vaut « if <condition> then » ; le corps va de `from` à `end_idx`. */
static void exec_if(Object *me, const char *head, char **L, int from, int end_idx)
{
    ARENA_MARK;
    const char *th = find_kw(head, "then");
    char *cond = arena_buf();
    char *val = arena_buf();
    const char *c0 = skip_spaces(head + 2);   /* après « if » */
    int n = th ? (int)(th - c0) : (int)strlen(c0);
    if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
    memcpy(cond, c0, (size_t)n); cond[n] = '\0';
    eval_checked(cond, val, HC_VAL);

    int m = find_else(L, from, end_idx);
    if (truthy(val)) {
        exec_block(me, L, from, m >= 0 ? m : end_idx);
        { ARENA_FREE; return; }
    }
    /* Chaque sortie rend ses tampons. Celle-ci — condition fausse, pas de
     * « else » — les gardait, et c'est la plus fréquente de toutes : un
     * « if … then … end if » non pris dans une boucle serrée perdait deux
     * tampons par tour. exec_block n'a pas de marque à lui pour les
     * récupérer, donc g_atop ne faisait que monter jusqu'à « arène de
     * tampons saturée », des milliers de tours plus loin. Le script
     * s'arrêtait alors en plein milieu, sans rapport visible avec la
     * condition qui l'avait déclenché.
     *
     * `rest` pointe dans L[m], pas dans l'arène : libérer après l'appel est
     * sans danger. */
    if (m < 0) { ARENA_FREE; return; }

    const char *rest = skip_spaces(L[m] + 4);   /* après « else » */
    if (!*rest) { exec_block(me, L, m + 1, end_idx); ARENA_FREE; return; }
    if (opens_if(rest)) { exec_if(me, rest, L, m + 1, end_idx); ARENA_FREE; return; }
    exec_stmt(me, rest);          /* « else <instruction> », if en ligne compris */
    ARENA_FREE;
}

/* Exécute une instruction simple, ou un `if` tenant sur une seule ligne. */
static void exec_stmt(Object *me, const char *s)
{
    ARENA_MARK;
    if (ci_word(s, "if")) {
        const char *th = find_kw(s, "then");
        if (th) {
            char *cond = arena_buf();
            char *val = arena_buf();
            const char *c0 = skip_spaces(s + 2);
            int n = (int)(th - c0);
            if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
            memcpy(cond, c0, (size_t)n); cond[n] = '\0';
            eval_checked(cond, val, HC_VAL);

            const char *body = skip_spaces(th + 4);
            const char *el   = find_kw(body, "else");
            char *yes = arena_buf();
            int ny = el ? (int)(el - body) : (int)strlen(body);
            if (ny > (int)HC_VAL - 1) ny = (int)HC_VAL - 1;
            memcpy(yes, body, (size_t)ny); yes[ny] = '\0';

            if (truthy(val)) exec_stmt(me, yes);
            else if (el)     exec_stmt(me, skip_spaces(el + 4));
            { ARENA_FREE; return; }
        }
    }
    exec_line(me, s);
    ARENA_FREE;
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
 * Ni opens_if (qui exige un « then » en fin de ligne) ni exec_stmt (qui ne
 * regarde qu'une ligne) ne savent la lire. On la traite en deux temps :
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

static void exec_if_chain(Object *me, char **L, int i, int to, int end)
{
    ARENA_MARK;
    const char *head = L[i];
    for (;;) {
        const char *th = find_kw(head, "then");
        char *cond = arena_buf();
        char *val = arena_buf();
        const char *c0 = skip_spaces(head + 2);      /* après « if » */
        int n = (int)(th - c0);
        if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
        memcpy(cond, c0, (size_t)n); cond[n] = '\0';
        eval_checked(cond, val, HC_VAL);

        /* On ne sort d'ici que par `return` : les deux ARENA_FREE qui
         * suivaient la boucle n'ont jamais été atteints. Chaque sortie porte
         * donc la sienne. `rest` et `head` pointent dans L, pas dans l'arène :
         * libérer avant de rendre la main ne les invalide pas. */
        if (truthy(val)) { exec_stmt(me, skip_spaces(th + 4)); ARENA_FREE; return; }

        if (i + 1 >= to || !ci_word(L[i+1], "else")) { ARENA_FREE; return; }
        const char *rest = skip_spaces(L[i+1] + 4);
        if (!*rest)          { exec_block(me, L, i + 2, end); ARENA_FREE; return; }
        if (is_inline_if(rest)) { head = rest; i++; continue; }
        exec_stmt(me, rest); ARENA_FREE; return;
    }
}

static void exec_block(Object *me, char **L, int from, int to)
{
    for (int i = from; i < to; i++) {
        const char *s = L[i];

        /* --- if … then <instruction> avec un « else » à la ligne --- */
        if (opens_inline_chain(L, i, to)) {
            int e = chain_end(L, i, to);
            exec_if_chain(me, L, i, to, e);
            i = e;
            if (flow_broken()) return;
            continue;
        }

        /* --- if … then / end if --- */
        if (opens_if(s)) {
            int e = match_end(L, i + 1, to, "if");
            /* si c'est un « else <instruction> » qui referme, il fait
               partie de la construction : exec_if doit le voir */
            int body_end = (e < to && else_closes(L[e])) ? e + 1 : e;
            exec_if(me, s, L, i + 1, body_end);
            i = e;
            if (flow_broken()) return;
            continue;
        }

        /* --- repeat … / end repeat --- */
        if (opens_repeat(s)) {
            int e = match_end(L, i + 1, to, "repeat");
            const char *h = skip_spaces(s + 6);

            /* variantes de l'en-tête */
            int    kind = 0;            /* 0 sans fin, 1 n fois, 2 while, 3 until, 4 with */
            long   times = 0;
            char   var[128] = "", from_e[256] = "", to_e[256] = "", cond[256] = "";
            int    down = 0;

            if (!*h || ci_word(h, "forever")) kind = 0;
            else if (ci_word(h, "while")) { kind = 2; snprintf(cond, sizeof cond, "%s", h + 5); }
            else if (ci_word(h, "until")) { kind = 3; snprintf(cond, sizeof cond, "%s", h + 5); }
            else if (ci_word(h, "with")) {
                kind = 4;
                const char *q = next_word(skip_spaces(h + 4), var, sizeof var);
                q = skip_spaces(q);
                if (*q == '=') q++;
                const char *tt = find_kw(q, "to");
                const char *dn = find_kw(q, "down");
                if (dn && (!tt || dn < tt)) { down = 1; tt = find_kw(dn, "to"); }
                int n = tt ? (int)(tt - q) : (int)strlen(q);
                if (down && dn) n = (int)(dn - q);
                if (n > (int)sizeof from_e - 1) n = (int)sizeof from_e - 1;
                memcpy(from_e, q, (size_t)n); from_e[n] = '\0';
                snprintf(to_e, sizeof to_e, "%s", tt ? tt + 2 : "0");
            } else {
                kind = 1;
                const char *q = h;
                if (ci_word(q, "for")) q = skip_spaces(q + 3);

                /* Retirer le « times » final avant d'évaluer : sans ça,
                 * « repeat n times » fait lire « n times » comme une seule
                 * référence, qui ne vaut rien — donc zéro tour. Avec un
                 * littéral le problème ne se voyait pas, parse_factor
                 * s'arrêtant tout seul après le nombre. */
                char cnt[256];
                const char *tm = find_kw(q, "times");
                int cn = tm ? (int)(tm - q) : (int)strlen(q);
                while (cn > 0 && isspace((unsigned char)q[cn-1])) cn--;
                if (cn > (int)sizeof cnt - 1) cn = (int)sizeof cnt - 1;
                memcpy(cnt, q, (size_t)cn); cnt[cn] = '\0';

                char v[256];
                eval_expr(cnt, v, sizeof v);
                double d = 0; as_num(v, &d);
                times = (long)d;
            }

            double cur = 0, last = 0;
            if (kind == 4) {
                char v[256];
                eval_expr(from_e, v, sizeof v); as_num(v, &cur);
                eval_expr(to_e,   v, sizeof v); as_num(v, &last);
            }

            long iter = 0;
            for (;;) {
                if (kind == 1 && iter >= times) break;
                if (kind == 2 || kind == 3) {
                    char v[256];
                    eval_expr(cond, v, sizeof v);
                    int t = truthy(v);
                    if (kind == 2 && !t) break;
                    if (kind == 3 &&  t) break;
                }
                if (kind == 4) {
                    if (!down && cur > last) break;
                    if ( down && cur < last) break;
                    char v[64]; put_num(cur, v, sizeof v);
                    var_set(var, v);
                }
                if (++iter > HC_MAX_LOOP) {
                    emit(HC_ERR, "!! boucle interrompue après %d tours", HC_MAX_LOOP);
                    break;
                }

                exec_block(me, L, i + 1, e);
                host_idle();      /* l'hôte redessine et souffle */

                if (g_next_repeat) g_next_repeat = 0;
                if (g_exit_repeat) { g_exit_repeat = 0; break; }
                if (g_pass || g_exit_handler) break;

                if (kind == 4) cur += down ? -1 : 1;
            }
            i = e;
            if (flow_broken()) return;
            continue;
        }

        /* --- fermetures orphelines : on les laisse filer --- */
        if (ci_word(s, "end")) {
            const char *w = skip_spaces(s + 3);
            if (ci_word(w, "if") || ci_word(w, "repeat")) continue;
        }
        if (ci_word(s, "else")) continue;

        exec_stmt(me, s);
        if (flow_broken()) return;
    }
}

/* Découpe le corps d'un gestionnaire en lignes utiles, puis l'exécute. */
/* HyperCard autorise le « then » à ouvrir la ligne suivante :
 *
 *     if (theDayOfWeek - 1) <> 0
 *     then put char 1 to n of spaces() into theDayNumbers
 *
 * On recolle ces deux lignes avant toute analyse : exec_if ignore alors
 * complètement cette variante. Le recollage n'a lieu que si la ligne suivante
 * commence vraiment par « then », donc un `if` mal formé ne peut pas avaler
 * la suite du gestionnaire. */
static void join_split_then(char **L, int *n)
{
    for (int i = 0; i + 1 < *n; i++) {
        if (!ci_word(L[i], "if")) continue;
        if (find_kw(L[i], "then")) continue;
        if (!ci_word(L[i+1], "then")) continue;

        size_t need = strlen(L[i]) + strlen(L[i+1]) + 2;
        char *j = (char *)malloc(need);
        if (!j) return;
        snprintf(j, need, "%s %s", L[i], L[i+1]);
        free(L[i]); free(L[i+1]);
        L[i] = j;
        for (int k = i + 1; k + 1 < *n; k++) L[k] = L[k+1];
        (*n)--;
    }
}

static void exec_body(Object *me, const char *body, const char *end)
{
    int cap = 32, n = 0;
    char **L = malloc((size_t)cap * sizeof *L);
    if (!L) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }

    const char *p = body;
    while (p < end && *p) {
        const char *nl = strchr(p, '\n');
        const char *stop = (nl && nl < end) ? nl : end;
        int len = (int)(stop - p);

        char *line = arena_buf();
        if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
        memcpy(line, p, (size_t)len);
        line[len] = '\0';

        int L2 = (int)strlen(line);

        /* commentaire « -- » en fin de ligne, hors guillemets */
        int inq = 0;
        for (int k = 0; k < L2; k++) {
            if (line[k] == '"') inq = !inq;
            else if (!inq && line[k] == '-' && line[k+1] == '-') { line[k] = '\0'; L2 = k; break; }
        }

        while (L2 > 0 && (line[L2-1] == '\r' || line[L2-1] == ' ' || line[L2-1] == '\t'))
            line[--L2] = '\0';

        const char *t = skip_spaces(line);
        if (*t && !(t[0] == '-' && t[1] == '-')) {   /* -- commentaire */
            if (n == cap) {
                cap *= 2;
                char **q = realloc(L, (size_t)cap * sizeof *L);
                if (!q) { fprintf(stderr, "mémoire épuisée\n"); exit(1); }
                L = q;
            }
            L[n++] = dupstr(t);
        }

        if (!nl || nl >= end) break;
        p = nl + 1;
    }

    join_split_then(L, &n);
    exec_block(me, L, 0, n);

    for (int i = 0; i < n; i++) free(L[i]);
    free(L);
}

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
            char inv[256];
            snprintf(inv, sizeof inv, "Où est le fichier « %s » ?", nom);
            reel = g_host->answer_file(inv);
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
static void exec_line_body(Object *me, const char *line)
{
    (void)me;   /* servira pour `the target` / `me` dans les expressions */
    char verb[64];
    const char *rest = next_word(line, verb, sizeof verb);

    /* Une parenthèse ouvrante termine le verbe aussi sûrement qu'un espace.
     * « return(x) », sans espace, est légal en HyperTalk et fréquent dans les
     * piles d'époque — next_word en faisait un seul mot, et l'interpréteur
     * annonçait « verbe inconnu : return(trunc(… ». On recoupe donc au
     * premier '(' et l'on rend le reste à l'analyseur d'expression, qui sait
     * très bien traiter une parenthèse en tête. */
    {
        char *par = strchr(verb, '(');
        if (par && par != verb) {
            rest = line + (par - verb);
            /* Retrouver la position réelle dans la ligne : verb a été copié
             * depuis le premier caractère non blanc. */
            const char *deb = skip_spaces(line);
            rest = deb + (par - verb);
            *par = '\0';
        }
    }

    /* --- send "<message> [args]" to <objet> ---
     * Le message est une ligne de HyperTalk : un nom de gestionnaire suivi
     * d'arguments éventuels. « send "carre 6" to card X » appelle on carre
     * sur X avec l'argument 6. Les arguments sont évalués côté appelant. */
    if (ci_equal(verb, "send")) {
        char *msgline = arena_buf();
        rest = skip_spaces(rest);
        if (*rest == '"') {
            const char *after = quoted(rest, msgline, HC_VAL);
            char probe[8];
            next_word(after, probe, sizeof probe);
            if (ci_equal(probe, "to")) {
                rest = after;                       /* littéral simple */
            } else {
                /* Le message est une expression : « send "go " & n to … ».
                 * Elle court jusqu'au dernier « to » de premier niveau, les
                 * « to » enfermés dans les guillemets étant ignorés. */
                const char *last = NULL, *scan = rest;
                for (const char *k = find_kw(scan, "to"); k; k = find_kw(scan, "to")) {
                    last = k; scan = k + 2;
                }
                if (!last) { emit(HC_ERR, "?? send mal formé : %s", line); return; }
                char *expr = arena_buf();
                int n = (int)(last - rest);
                if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
                memcpy(expr, rest, (size_t)n); expr[n] = '\0';
                eval_expr(expr, msgline, HC_VAL);
                rest = last;
            }
        }
        else {
            /* message nu : tout jusqu'à « to » (au niveau supérieur) */
            const char *to_kw = find_kw(rest, "to");
            int len = to_kw ? (int)(to_kw - rest) : (int)strlen(rest);
            if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
            memcpy(msgline, rest, (size_t)len); msgline[len] = '\0';
            rest = to_kw ? to_kw : rest + strlen(rest);
        }

        char to[8];
        const char *r2 = next_word(rest, to, sizeof to);
        if (!ci_equal(to, "to")) { emit(HC_ERR, "?? send mal formé : %s", line); return; }

        Object *target = resolve(r2);
        if (!target) {
            set_result("destinataire introuvable");
            emit(HC_ERR, "   !! destinataire introuvable : %s", skip_spaces(r2));
            return;
        }

        /* découper le message en nom + arguments */
        char msg[128];
        const char *a = next_word(skip_spaces(msgline), msg, sizeof msg);
        char (*argv)[HC_VAL] = arena_rows(16);
        int argc = 0;
        a = skip_spaces(a);
        while (*a && argc < 16) {
            char *one = arena_buf();
            int len = 0, depth = 0, inq = 0;
            while (*a && !(depth == 0 && !inq && *a == ',')) {
                if (*a == '"') inq = !inq;
                else if (!inq && *a == '(') depth++;
                else if (!inq && *a == ')') depth--;
                if (len < 511) one[len++] = *a;
                a++;
            }
            one[len] = '\0';
            eval_expr(one, argv[argc], sizeof argv[argc]);   /* contexte appelant */
            argc++;
            if (*a == ',') a = skip_spaces(a + 1); else break;
        }

        set_result("");     /* un `return` dans le gestionnaire le remplira */
        hc_send_args(target, msg, argv, argc);
        return;
    }

    /* --- put <expr> [into|after|before <champ|variable>] --- */
    /* --- debug : le relevé des retours de la v3 vers ce fichier ---
     *
     * « debug bilan » l'imprime, « debug raz » le remet à zéro. Le verbe
     * existe dans la table de hct_cmd.c mais n'avait aucun traitant : il
     * partait en envoi de message et se perdait.
     *
     * Le passer par une commande plutôt que par un appel depuis l'interface
     * permet de le déclencher DEPUIS un script, donc de mesurer une pile
     * précise : « debug raz » en début de gestionnaire, « debug bilan » à la
     * fin. */
    if (ci_equal(verb, "debug")) {
        char quoi[64];
        next_word(rest, quoi, sizeof quoi);
        if (ci_equal(quoi, "raz")) { hc_v3_bilan_remise_a_zero(); return; }
        hc_v3_bilan();
        return;
    }

    if (ci_equal(verb, "put")) {
        /* repérer la préposition, hors des chaînes entre guillemets */
        const char *kw = NULL;
        int kwlen = 0, mode = 0;    /* 0 = into (remplace), 1 = after, 2 = before */
        int inq = 0;
        for (const char *q = rest; *q; q++) {
            if (*q == '"') { inq = !inq; continue; }
            if (inq) continue;
            if (!(q == rest || isspace((unsigned char)q[-1]))) continue;
            if      (ci_word(q, "into"))   { kw = q; kwlen = 4; mode = 0; break; }
            else if (ci_word(q, "after"))  { kw = q; kwlen = 5; mode = 1; break; }
            else if (ci_word(q, "before")) { kw = q; kwlen = 6; mode = 2; break; }
        }

        char *val = arena_buf();
        if (!kw) {                       /* pas de destination : la boîte de message */
            eval_checked(rest, val, HC_VAL);
            emit(HC_MSG, "%s", val);
            return;
        }

        char *expr = arena_buf();
        int n = (int)(kw - rest);
        if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
        memcpy(expr, rest, (size_t)n); expr[n] = '\0';
        eval_checked(expr, val, HC_VAL);

        const char *dsttext = skip_spaces(kw + kwlen);
        if (container_set(dsttext, val, mode)) {
            set_result("");
            char *shown = arena_buf();
            eval_expr(dsttext, shown, HC_VAL);
            if (!*shown && *val) snprintf(shown, HC_VAL, "%s", val);
            emit(HC_INFO, "   → %s ← \"%s\"", dsttext, shown);
        } else {
            set_result("destination invalide");
            emit(HC_ERR, "   !! destination invalide : %s", dsttext);
        }
        return;
    }

    /* --- delete <morceau> of <conteneur> --- */
    if (ci_equal(verb, "delete")) {
        const char *d = skip_spaces(rest);
        if (container_set(d, "", 3)) {
            set_result("");
            emit(HC_INFO, "   → supprimé : %s", d);
        } else {
            set_result("rien à supprimer");
            emit(HC_ERR, "   !! rien à supprimer : %s", d);
        }
        return;
    }

    /* --- lock screen | unlock screen [with visual …] ---
     * Ce sont les alias de « set lockScreen to true/false », et c'est bien
     * ainsi qu'on les traite : l'hôte reçoit une propriété globale et décide
     * seul de suspendre ou non son rafraîchissement. Le noyau, lui, ne sait
     * toujours pas ce qu'est un écran. L'effet visuel éventuel est ignoré. */
    if (ci_equal(verb, "lock") || ci_equal(verb, "unlock")) {
        if (ci_word(skip_spaces(rest), "messages")) {
            g_messages_verrouilles = ci_equal(verb, "lock");
            set_result("");
            return;
        }
        if (ci_word(skip_spaces(rest), "screen")) {
            g_ecran_verrouille = ci_equal(verb, "lock");
            host_global_set("lockScreen", g_ecran_verrouille ? "true" : "false");
            /* Au déverrouillage, on réveille les champs modifiés pendant le
             * verrou — eux seulement, pas l'écran entier. */
            if (!g_ecran_verrouille) verrou_reveille();
            set_result("");
            return;
        }
    }

    /* --- convert <conteneur> to <format> [and <format>] ---
     * Le résultat retourne dans le conteneur d'origine ; si la source n'en
     * est pas un (« convert the date to dateItems »), il atterrit dans `it`,
     * comme sur Macintosh. En cas d'échec, `the result` vaut « invalid date »
     * — c'est ce que les scripts d'époque testent. */
    if (ci_equal(verb, "convert")) {
        /* Le « to » qui compte est le dernier de premier niveau : la source
         * peut en contenir un elle-même, comme dans
         * « convert item 1 to 7 of calData() to dateItems ». */
        const char *to = NULL, *scan = rest;
        for (const char *k = find_kw(scan, "to"); k; k = find_kw(scan, "to")) {
            to = k; scan = k + 2;
        }
        if (!to) {
            emit(HC_ERR, "   !! convert sans « to » : %s", skip_spaces(rest));
            set_result("invalid date");
            return;
        }

        char *src = arena_buf();
        int n = (int)(to - rest);
        if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
        memcpy(src, rest, (size_t)n); src[n] = '\0';
        while (n > 0 && isspace((unsigned char)src[n-1])) src[--n] = '\0';
        const char *srcp = skip_spaces(src);

        /* le format cible, éventuellement double : « short date and long time » */
        char spec[256];
        snprintf(spec, sizeof spec, "%s", skip_spaces(to + 2));
        char *andkw = (char *)find_kw(spec, "and");
        int f1 = DF_NONE, f2 = DF_NONE;
        if (andkw) { *andkw = '\0'; f2 = date_format_code(andkw + 4); }
        f1 = date_format_code(spec);
        if (f1 == DF_NONE) {
            emit(HC_ERR, "   !! format de date inconnu : %s", skip_spaces(to + 2));
            set_result("invalid date");
            return;
        }

        char *val = arena_buf();
        eval_expr(srcp, val, HC_VAL);

        struct tm tm;
        if (!parse_datetime(val, &tm)) {
            emit(HC_ERR, "   !! date incomprise : « %s »", val);
            set_result("invalid date");
            return;
        }

        char *outv = arena_buf();
        char part2[256];
        emit_datetime(&tm, f1, outv, HC_VAL);
        if (f2 != DF_NONE) {
            emit_datetime(&tm, f2, part2, sizeof part2);
            int L = (int)strlen(outv);
            snprintf(outv + L, HC_VAL - L, " %s", part2);
        }

        /* Destination : le conteneur source s'il en est un. « the date »,
         * « the long time » et les appels de fonction n'en sont pas. */
        int to_it = ci_word(srcp, "the") || strchr(srcp, '(') != NULL;
        if (to_it || !container_set(srcp, outv, 0))
            var_set("it", outv);

        set_result("");
        emit(HC_INFO, "   → %s", outv);
        return;
    }

    /* --- return <expr> : dépose une valeur dans `the result` et sort --- */
    if (ci_equal(verb, "return")) {
        char *val = arena_buf();
        eval_checked(rest, val, HC_VAL);
        set_result(val);
        g_exit_handler = 1;
        return;
    }

    /* --- exit repeat | exit <gestionnaire> | next repeat --- */
    if (ci_equal(verb, "exit")) {
        if (ci_word(skip_spaces(rest), "repeat")) g_exit_repeat = 1;
        else                                      g_exit_handler = 1;
        return;
    }
    if (ci_equal(verb, "next")) {
        if (ci_word(skip_spaces(rest), "repeat")) g_next_repeat = 1;
        return;
    }

    /* --- set [the] <propriété> of <objet> to <expr> --- */
    if (ci_equal(verb, "set")) {
        g_visual_dirty = 1;
        const char *s = skip_spaces(rest);
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
                             "« set the textStyle of %s … »", skip_spaces(rest));
                return;
            }
            const char *to = find_kw(s, "to");
            if (!to) {
                emit(HC_ERR, "   !! set mal formé : %s", skip_spaces(rest));
                return;
            }
            char *val = arena_buf();
            eval_checked(to + 2, val, HC_VAL);

            /* itemDelimiter est traité par le noyau, pas par l'hôte : c'est
             * lui qui découpe les items, et l'interface n'a rien à en savoir.
             * Une chaîne vide ou de plusieurs caractères ramène à la virgule —
             * HyperCard ne retenait qu'un caractère. */
            if (ci_equal(prop, "itemdelimiter")) {
                g_item_delim = val[0] ? val[0] : ',';
                set_result("");
                emit(HC_INFO, "   → itemDelimiter ← \"%c\"", g_item_delim);
                return;
            }

            /* lockScreen suivi ici aussi : « set lockScreen to true » est
             * l'exact synonyme de « lock screen », et notify_field s'appuie
             * dessus pour ne pas redessiner mille fois pour rien. */
            if (ci_equal(prop, "lockscreen")) {
                g_ecran_verrouille = truthy(val);
                if (!g_ecran_verrouille) verrou_reveille();
            }

            host_global_set(prop, val);
            set_result("");
            emit(HC_INFO, "   → %s ← \"%s\"", prop, val);
            return;
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
            emit(HC_ERR, "   !! set sans « to » : %s", skip_spaces(rest));
            return;
        }

        char refbuf[256];
        int n = (int)(to - q);
        if (n > (int)sizeof refbuf - 1) n = (int)sizeof refbuf - 1;
        memcpy(refbuf, q, (size_t)n); refbuf[n] = '\0';
        while (n > 0 && (refbuf[n-1] == ' ' || refbuf[n-1] == '\t')) refbuf[--n] = '\0';

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

            /* Court exprès : on ne cherche qu'à savoir si le texte NOMME une
             * couleur, et « white », « #FF00AA » ou « 255,128,0 » tiennent
             * tous là-dedans. Un texte plus long ne peut pas être un nom de
             * couleur — il est tronqué, color_from_name le refuse, et l'on
             * retombe sur l'évaluation, qui est bien ce qu'il faut faire.
             *
             * Surtout pas char[HC_VAL] : ce serait un mégaoctet sur la pile,
             * dans une fonction que l'évaluateur appelle en cascade. */
            char brut[64];
            snprintf(brut, sizeof brut, "%.*s", rl, raw);

            /* Déguillemeter d'abord : « to "white" » nomme aussi une couleur. */
            int n = (int)strlen(brut);
            if (n > 1 && brut[0] == '"' && brut[n-1] == '"') {
                memmove(brut, brut + 1, (size_t)(n - 2));
                brut[n - 2] = '\0';
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

            /* Trancher sur le CONTENU, pas sur la ponctuation. L'ancien test
             * — « ni virgule ni espace, alors peut-être une variable » —
             * classait « s & ",italic" » parmi les listes de noms : il n'en
             * retenait que « italic » et jetait le reste, si bien qu'on ne
             * pouvait pas relire un style pour lui en ajouter un. Désormais
             * une suite de noms de style reste littérale, et tout le reste
             * est évalué comme l'expression que c'est. */
            if (!style_is_names(val)) {
                int n = (int)strlen(val);
                int quoted = 0;

                /* Une liste de noms peut arriver citée : « to "bold,italic" ».
                 * On la déguillemete pour la re-tester. L'ancienne écriture
                 * glissait le snprintf dans la condition via l'opérateur
                 * virgule, ce que le compilateur signale à juste titre : c'est
                 * le motif habituel d'un « f(a, b) » mal parenthésé. */
                if (n > 1 && val[0] == '"' && val[n-1] == '"') {
                    char *inner = arena_buf();
                    snprintf(inner, HC_VAL, "%.*s", n - 2, val + 1);
                    if (style_is_names(inner)) {
                        snprintf(val, HC_VAL, "%s", inner);
                        quoted = 1;
                    }
                }
                if (!quoted) eval_checked(to + 2, val, HC_VAL);
            }
        } else {
            eval_checked(to + 2, val, HC_VAL);
        }

        /* La cible peut designer une PLAGE DE TEXTE et non un objet :
         *     set the textStyle of word 3 of line 2 of field "cal" to bold
         * resolve() ne sait chercher que des objets, d'ou l'echec historique
         * « objet introuvable : word theNewDay of line 3 ». On essaie donc
         * d'abord le morceau, avant de retomber sur la resolution d'objet. */
        {
            int cst, cen;
            Object *cf = chunk_target(refbuf, &cst, &cen);
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
                    /* Deux échecs bien différents, qu'un seul message
                     * confondait : « font » n'est pas une propriété du tout
                     * (c'est « textFont »), alors que « rect » en est une,
                     * mais qui décrit un objet et non un morceau. Annoncer
                     * « ne s'applique pas à un morceau » sur un nom inconnu
                     * envoyait chercher l'erreur au mauvais endroit. */
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
                    return;
                }
                struct RunList *rl = runs_of(cf);
                if (!rl) {
                    set_result("champ sans stockage de style");
                    emit(HC_ERR, "   !! ce champ n'a pas encore de texte sur cette carte");
                    return;
                }
                runs_set_attr(rl, cst, cen - cst, mask,
                              (mask & RA_STYLE) ? style_from_names(val) : 0,
                              (mask & RA_SIZE)  ? atoi(val) : 0,
                              (mask & RA_FONT)  ? val : NULL,
                              (mask & RA_COLOR) ? color_from_name(val)
                                                : HC_COLOR_INHERIT);
                notify_field(cf);
                set_result("");
                emit(HC_INFO, "   → %s de [%d..%d[ ← \"%s\"", prop, cst, cen, val);
                return;
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
            return;
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
            o->selectedline = atoi(val);
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
            o->dont_search = truthy(val); notify_field(o);
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
            o->scroll = atoi(val);
            if (o->scroll < 0) o->scroll = 0;
            notify_field(o);
        } else if (ci_equal(prop, "textfont")) {
            free(o->textfont);
            o->textfont = (*val) ? dupstr(val) : NULL;
            notify_field(o);
        } else if (ci_equal(prop, "textstyle")) {
            /* Passe par style_from_names, comme la pose sur un morceau. Le
             * code d'origine cherchait trois sous-chaînes à la main : il
             * ignorait outline, shadow, condense, extend et group — d'où un
             * « to bold,outline » sur un bouton qui se relisait « bold » —
             * et un mot comme « underlined » l'aurait déclenché à tort. */
            o->textstyle = style_from_names(val);
            notify_field(o);
        } else if (ci_equal(prop, "hilite") || ci_equal(prop, "highlight")) {
            hc_set_hilite(o, NULL, truthy(val));
            notify_field(o);
        } else if (ci_equal(prop, "autohilite")) {
            o->autohilite = truthy(val);
        } else if (ci_equal(prop, "textsize")) {
            o->textsize = atoi(val);
            notify_field(o);
        } else if (ci_equal(prop, "textheight")) {
            /* Zéro rétablit la valeur déduite du corps. */
            int v = atoi(val);
            o->textheight = v > 0 ? v : 0;
            notify_field(o);
        } else if (ci_equal(prop, "script")) {
            /* Une valeur qui touche exactement le plafond a presque
             * certainement été tronquée en chemin (« get the script of me »
             * sur un script plus long que HC_VAL). L'écrire telle quelle
             * détruirait le script. On refuse : mieux vaut un gestionnaire
             * qui échoue qu'une pile mutilée.
             *
             * Un script faisant pile HC_VAL-1 octets serait refusé à tort ;
             * c'est un prix négligeable face à la perte de l'original. */
            if (g_script_clipped || (int)strlen(val) >= HC_VAL - 1) {
                emit(HC_ERR, "   !! écriture refusée : ce gestionnaire a lu un "
                             "script tronqué ; l'écrire le détruirait");
                set_result("script tronqué");
                return;
            }
            hc_set_script(o, val);
        } else if (ci_equal(prop, "style")) {
            free(o->style);
            o->style = dupstr(val);
        } else if (ci_equal(prop, "text") || ci_equal(prop, "contents")) {
            if (o->type != OBJ_FIELD && o->type != OBJ_BUTTON) {
                emit(HC_ERR, "   !! seul un champ ou un bouton a un contenu");
                return;
            }
            hc_set_field_text(o, val);
            notify_field(o);
        } else if (geom_write(o, prop, val)) {
            notify_field(o);
        } else {
            set_result("propriété inconnue");
            emit(HC_ERR, "   !! propriété inconnue : %s", prop);
            return;
        }

        set_result("");
        emit(HC_INFO, "   → %s de %s ← \"%s\"", prop, d, val);
        return;
    }

    /* --- do <expr> : exécute du texte comme une instruction ---
     * Le pendant de value() : celle-ci rend du calcul, celle-là de l'action.
     * Un compteur évite qu'un « do » qui se relance ne mange la pile C. */
    if (ci_equal(verb, "do")) {
        static int do_depth = 0;
        if (do_depth >= 16) {
            emit(HC_ERR, "   !! do : trop d'imbrications");
            return;
        }
        char *val = arena_buf();
        eval_checked(rest, val, HC_VAL);

        do_depth++;
        /* plusieurs lignes : on les exécute l'une après l'autre */
        char *copie = dupstr(val), *p = copie;
        while (p && *p) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            const char *ligne = skip_spaces(p);
            if (*ligne && !(ligne[0] == '-' && ligne[1] == '-')) exec_stmt(me, ligne);
            if (!nl || flow_broken()) break;
            p = nl + 1;
        }
        free(copie);
        do_depth--;
        return;
    }

    /* --- get <expr> : le résultat va dans `it` --- */
    if (ci_equal(verb, "get")) {
        char *val = arena_buf();
        eval_checked(rest, val, HC_VAL);
        var_set("it", val);
        emit(HC_INFO, "   → it ← \"%s\"", val);
        return;
    }

    /* --- global a, b, c --- */
    if (ci_equal(verb, "global")) {
        const char *q = skip_spaces(rest);
        while (*q) {
            char nm[128];
            int i = 0;
            while (*q && *q != ',' && !isspace((unsigned char)*q) && i < (int)sizeof nm - 1)
                nm[i++] = *q++;
            nm[i] = '\0';
            if (nm[0]) frame_declare_global(g_frame, nm);
            while (*q == ',' || *q == ' ' || *q == '\t') q++;
        }
        return;
    }

    /* --- add / subtract / multiply / divide ---
     *   add 1 to i            subtract 2 from x
     *   multiply x by 3       divide t by 2
     * Le conteneur peut etre une variable, un champ ou un chunk. */
    if (ci_equal(verb, "add") || ci_equal(verb, "subtract") ||
        ci_equal(verb, "multiply") || ci_equal(verb, "divide")) {

        const char *sep = NULL; int seplen = 0;
        if      (ci_equal(verb, "add"))      { sep = "to";   seplen = 2; }
        else if (ci_equal(verb, "subtract")) { sep = "from"; seplen = 4; }
        else                                  { sep = "by";   seplen = 2; }

        const char *r = skip_spaces(rest);
        const char *kw = NULL;
        int inq = 0;
        for (const char *q = r; *q; q++) {
            if (*q == '"') { inq = !inq; continue; }
            if (inq) continue;
            if ((q == r || isspace((unsigned char)q[-1])) && ci_word(q, sep)) { kw = q; break; }
        }
        if (!kw) {
            emit(HC_ERR, "   !! syntaxe : %s <valeur> %s <conteneur>", verb, sep);
            set_result("syntax error");
            return;
        }

        /* add/subtract : <valeur> to|from <conteneur>
         * multiply/divide : <conteneur> by <valeur>   (ordre inverse) */
        int valueFirst = (ci_equal(verb, "add") || ci_equal(verb, "subtract"));

        char *left = arena_buf();
        char amount[128];
        char *cur = arena_buf();
        int n = (int)(kw - r);
        if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
        memcpy(left, r, n); left[n] = 0;
        /* retirer les espaces de fin */
        for (int k = (int)strlen(left) - 1; k >= 0 && isspace((unsigned char)left[k]); k--)
            left[k] = 0;

        const char *right = skip_spaces(kw + seplen);
        const char *dst;
        if (valueFirst) {
            eval_expr(left, amount, sizeof amount);
            dst = right;
        } else {
            eval_expr(right, amount, sizeof amount);
            dst = left;
        }
        eval_expr(dst, cur, HC_VAL);

        double a = 0, b = 0;
        as_num(cur, &a);
        as_num(amount, &b);
        double res = a;
        if      (ci_equal(verb, "add"))      res = a + b;
        else if (ci_equal(verb, "subtract")) res = a - b;
        else if (ci_equal(verb, "multiply")) res = a * b;
        else {
            if (b == 0) { emit(HC_ERR, "   !! division par zero"); set_result("divide by zero"); return; }
            res = a / b;
        }

        char out[64];
        put_num(res, out, sizeof out);
        if (!container_set(dst, out, 0)) {
            emit(HC_ERR, "   !! destination inconnue : %s", dst);
            set_result("no such container");
            return;
        }
        set_result("");
        return;
    }

    /* --- find [string|word|chars|whole] "texte" [in <champ>] ---
     * Parcourt les cartes à partir de la courante, en bouclant. Ignore les
     * champs marqués dontSearch. Le mode par défaut d'HyperTalk cherche un
     * mot commençant par le motif. */
    if (ci_equal(verb, "find")) {
        const char *r = skip_spaces(rest);
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
            int n = (int)(kw - r);
            if (n > (int)sizeof e - 1) n = (int)sizeof e - 1;
            memcpy(e, r, n); e[n] = 0;
            eval_expr(e, pat, sizeof pat);
            snprintf(where, sizeof where, "%s", skip_spaces(kw + 2));
        } else {
            eval_expr(r, pat, sizeof pat);
        }
        if (!pat[0]) { set_result("not found"); return; }

        Object *stack = g_current_card ? g_current_card->owner : NULL;
        while (stack && stack->type != OBJ_STACK) stack = stack->owner;
        if (!stack) { set_result("not found"); return; }

        int total = card_count(stack);
        int start = card_index(stack, g_current_card);
        if (start < 0) start = 0;

        for (int k = 0; k < total; k++) {
            Object *cd = nth_card(stack, (start + k) % total);
            if (!cd) continue;

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
                        return;
                    }
                }
            }
        }
        g_found_text[0] = 0; g_found_field = NULL; g_found_line = 0;
        g_found_start = g_found_len = 0; g_found_card = NULL;
        set_result("not found");
        return;
    }

    /* --- ask "invite" [with "défaut"] --- */
    if (ci_equal(verb, "ask")) {
        const char *r = skip_spaces(rest);

        /* ---- ask file <invite> [with <nom par défaut>] ----
         * Le panneau d'enregistrement, pendant d'« answer file ». Même
         * mécanique : le chemin choisi dans « it », vide si l'on annule. */
        if (ci_word(r, "file")) {
            ARENA_MARK;
            char *inv = arena_buf(), *def = arena_buf();
            const char *a = skip_spaces(r + 4);
            const char *w = find_kw(a, "with");
            if (w) {
                char brut[512];
                int n = (int)(w - a);
                if (n > (int)sizeof brut - 1) n = (int)sizeof brut - 1;
                memcpy(brut, a, (size_t)n); brut[n] = '\0';
                eval_checked(brut, inv, HC_VAL);
                eval_checked(skip_spaces(w + 4), def, HC_VAL);
            } else {
                eval_checked(a, inv, HC_VAL);
            }
            const char *chemin = (g_host && g_host->ask_file)
                               ? g_host->ask_file(inv, def) : NULL;
            var_set("it", chemin ? chemin : "");
            set_result(chemin ? "" : "Cancel");
            ARENA_FREE;
            return;
        }

        char *prompt = arena_buf();
        char *deflt = arena_buf();

        /* repérer « with » hors des chaînes */
        const char *kw = NULL;
        int inq = 0;
        for (const char *q = r; *q; q++) {
            if (*q == '"') { inq = !inq; continue; }
            if (inq) continue;
            if ((q == r || isspace((unsigned char)q[-1])) && ci_word(q, "with")) { kw = q; break; }
        }
        if (kw) {
            char *expr = arena_buf();
            int n = (int)(kw - r);
            if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
            memcpy(expr, r, n); expr[n] = '\0';
            eval_expr(expr, prompt, HC_VAL);
            eval_expr(skip_spaces(kw + 4), deflt, HC_VAL);
        } else {
            eval_expr(r, prompt, HC_VAL);
        }

        const char *rep = (g_host && g_host->ask) ? g_host->ask(prompt, deflt) : NULL;
        if (rep) { var_set("it", rep); set_result(""); }
        else     { var_set("it", "");  set_result("Cancel"); }
        return;
    }

    /* --- answer "invite" [with "a" [or "b" [or "c"]]] --- */
    if (ci_equal(verb, "answer")) {
        const char *r = skip_spaces(rest);

        /* ---- answer file <invite> [of type <t>] ----
         * Le panneau d'ouverture, et non une boîte à boutons. Le chemin choisi
         * va dans « it », l'annulation y laisse la chaîne vide.
         *
         * C'est aussi la parade au bac à sable de macOS : un fichier désigné
         * par l'utilisateur devient accessible du seul fait de ce choix, là où
         * un chemin écrit dans un script se heurte à « Operation not
         * permitted ». */
        if (ci_word(r, "file")) {
            ARENA_MARK;
            char *inv = arena_buf();
            const char *a = skip_spaces(r + 4);
            const char *oft = find_kw(a, "of");     /* « of type … » : ignoré */
            if (oft) {
                char brut[512];
                int n = (int)(oft - a);
                if (n > (int)sizeof brut - 1) n = (int)sizeof brut - 1;
                memcpy(brut, a, (size_t)n); brut[n] = '\0';
                eval_checked(brut, inv, HC_VAL);
            } else {
                eval_checked(a, inv, HC_VAL);
            }
            const char *chemin = (g_host && g_host->answer_file)
                               ? g_host->answer_file(inv) : NULL;
            var_set("it", chemin ? chemin : "");
            set_result(chemin ? "" : "Cancel");
            ARENA_FREE;
            return;
        }

        char *prompt = arena_buf();
        char btn[3][128] = { "", "", "" };
        int nb = 0;

        const char *kw = NULL;
        int inq = 0;
        for (const char *q = r; *q; q++) {
            if (*q == '"') { inq = !inq; continue; }
            if (inq) continue;
            if ((q == r || isspace((unsigned char)q[-1])) && ci_word(q, "with")) { kw = q; break; }
        }
        if (kw) {
            char *expr = arena_buf();
            int n = (int)(kw - r);
            if (n > (int)HC_VAL - 1) n = (int)HC_VAL - 1;
            memcpy(expr, r, n); expr[n] = '\0';
            eval_expr(expr, prompt, HC_VAL);

            /* découper sur « or », hors des chaînes */
            const char *p2 = skip_spaces(kw + 4);
            while (*p2 && nb < 3) {
                const char *cut = NULL;
                inq = 0;
                for (const char *q = p2; *q; q++) {
                    if (*q == '"') { inq = !inq; continue; }
                    if (inq) continue;
                    if ((q == p2 || isspace((unsigned char)q[-1])) && ci_word(q, "or")) { cut = q; break; }
                }
                char one[256];
                int len = cut ? (int)(cut - p2) : (int)strlen(p2);
                if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
                memcpy(one, p2, len); one[len] = '\0';
                eval_expr(one, btn[nb], sizeof btn[nb]);
                nb++;
                if (!cut) break;
                p2 = skip_spaces(cut + 2);
            }
        } else {
            eval_expr(r, prompt, HC_VAL);
        }
        if (nb == 0) { snprintf(btn[0], sizeof btn[0], "OK"); nb = 1; }

        const char *rep = (g_host && g_host->answer)
            ? g_host->answer(prompt, btn[0], nb > 1 ? btn[1] : NULL,
                                              nb > 2 ? btn[2] : NULL)
            : btn[0];
        var_set("it", rep ? rep : "");
        set_result("");
        return;
    }

    /* --- push [card] : mémorise la carte courante --- */
    if (ci_equal(verb, "push")) {
        const char *r = skip_spaces(rest);
        Object *dst = *r ? resolve(r) : g_current_card;
        if (!dst || dst->type != OBJ_CARD) dst = g_current_card;
        if (!dst) { set_result("aucune carte a empiler"); return; }
        if (g_navtop >= NAVSTACK_MAX) {          /* pile pleine : on decale */
            for (int i = 1; i < NAVSTACK_MAX; i++) g_navstack[i-1] = g_navstack[i];
            g_navtop = NAVSTACK_MAX - 1;
        }
        g_navstack[g_navtop++] = dst;
        set_result("");
        emit(HC_INFO, "   ⇒ empile la carte \"%s\"", dst->name ? dst->name : "?");
        return;
    }

    /* --- pop [card] [into conteneur] : depile et y va --- */
    if (ci_equal(verb, "pop")) {
        if (g_navtop <= 0) { set_result("pile de navigation vide"); return; }
        Object *dst = g_navstack[--g_navtop];
        const char *r = skip_spaces(rest);
        if (ci_word(r, "card") || ci_word(r, "cd")) r = skip_spaces(r + (ci_word(r,"cd") ? 2 : 4));

        if (ci_word(r, "into")) {                /* pop cd into x : sans y aller */
            char cmd[320];
            snprintf(cmd, sizeof cmd, "put \"card id %d\" into %s",
                     dst->id, skip_spaces(r + 4));
            exec_line(me, cmd);
            set_result("");
            return;
        }

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
        return;
    }

    /* --- go [to] card "nom" | next | previous | first | last | card 3 --- */
    if (ci_equal(verb, "go")) {
        const char *r = skip_spaces(rest);
        if (ci_word(r, "to")) r = skip_spaces(r + 2);

        /* ---- go to stack "X" ----
         * Une pile déjà ouverte : on s'y rend. Sinon on demande à l'hôte de
         * l'ouvrir — lui seul sait où chercher le fichier et comment lui
         * donner une fenêtre. C'est ce qui permet à une pile d'en appeler une
         * autre, le mécanisme sur lequel reposaient les piles à index et les
         * bibliothèques de l'époque. */
        if (ci_word(r, "stack")) {
            const char *a = skip_spaces(r + 5);
            ARENA_MARK;
            char *nom = arena_buf();
            eval_checked(a, nom, HC_VAL);

            Object *cible = find_open_stack(nom);
            if (!cible && g_host && g_host->open_stack)
                cible = g_host->open_stack(nom);

            if (cible) {
                Object *prem = NULL;
                for (int i = 0; i < cible->nparts; i++)
                    if (cible->parts[i]->type == OBJ_CARD) { prem = cible->parts[i]; break; }
                if (prem) {
                    Object *old = g_current_card;
                    if (old && old->owner != cible) hc_send(old, "closeStack");
                    g_current_card = prem;
                    if (g_host && g_host->stack_changed) g_host->stack_changed(cible);
                    hc_send(prem, "openStack");
                    set_result("");
                } else set_result("No such card");
            } else {
                set_result("No such stack");
                emit(HC_ERR, "   !! go : pile introuvable : %s", nom);
            }
            ARENA_FREE;
            return;
        }

        /* Le marquage filtre la navigation : resolve() n'y connaît rien, on
         * lui laisse tout le reste. */
        int marque = 0;
        Object *dst = marked_card_ref(r, &marque);
        if (marque && !dst) { set_result("No such card"); return; }
        if (!dst) dst = resolve(r);
        if (!dst) {                          /* « go x » : evaluer d'abord */
            char v[256];
            eval_expr(r, v, sizeof v);
            if (v[0] && strcmp(v, r) != 0) dst = resolve(v);
        }
        if (!dst && ci_word(r, "card")) {   /* « go card » nu : la première */
            const char *w = skip_spaces(r + 4);
            if (!*w) dst = resolve("first card");
        }
        /* « go background "x" » mène à la PREMIÈRE CARTE de ce fond, et
         * « go stack "x" » à la première carte de la pile : dans HyperCard
         * on ne se tient jamais sur un fond, seulement sur une carte. */
        if (dst && (dst->type == OBJ_BACKGROUND || dst->type == OBJ_STACK)) {
            Object *stk = owning_stack(dst);
            Object *found = NULL;
            for (int i = 0; stk && i < stk->nparts; i++) {
                Object *c = stk->parts[i];
                if (c->type != OBJ_CARD) continue;
                if (dst->type == OBJ_STACK || c->bg == dst) { found = c; break; }
            }
            if (!found)
                emit(HC_ERR, "   !! aucune carte dans %s", hc_typename(dst->type));
            dst = found;
        }

        if (dst && dst->type == OBJ_CARD) {
            set_result("");
            /* Jouer l'effet armé, s'il y en a un, AVANT de changer de carte :
             * l'hôte a besoin de photographier l'écran de départ. Puis on
             * l'oublie — « visual » ne vaut que pour le prochain « go ». */
            if (g_visual_effect[0]) {
                if (g_host && g_host->visual_effect)
                    g_host->visual_effect(g_visual_effect, g_visual_speed,
                                          g_visual_image);
                g_visual_effect[0] = g_visual_speed[0] = g_visual_image[0] = '\0';
            }
            Object *old   = g_current_card;
            Object *oldbg = old ? old->bg : NULL;
            if (old) hc_send_systeme(old, "closeCard");
            /* Changement de fond : les quatre messages, dans l'ordre
             * d'HyperCard. « find » le faisait déjà, « go » l'oubliait. */
            if (oldbg && oldbg != dst->bg) hc_send_systeme(oldbg, "closeBackground");
            g_current_card = dst;
            if (dst->bg && dst->bg != oldbg) hc_send_systeme(dst->bg, "openBackground");
            emit(HC_INFO, "   ⇒ va à la carte \"%s\"", dst->name ? dst->name : "?");
            hc_send_systeme(dst, "openCard");
        } else {
            set_result("carte introuvable");
            emit(HC_ERR, "   !! carte introuvable : %s", r);
        }
        return;
    }

    /* --- show <objet> [at <point>] / hide <objet> ---
     * « show me at h,v » est la primitive d'animation d'HyperCard : elle
     * déplace l'objet ET le rend visible, le centre allant au point donné
     * (comme « the loc », pas comme « the topLeft »). */
    if (ci_equal(verb, "show") || ci_equal(verb, "hide")) {
        int showing = ci_equal(verb, "show");
        g_visual_dirty = 1;
        const char *at = showing ? find_kw(rest, "at") : NULL;

        char refbuf[256];
        const char *ref = rest;
        if (at) {
            int n = (int)(at - rest);
            if (n > (int)sizeof refbuf - 1) n = (int)sizeof refbuf - 1;
            memcpy(refbuf, rest, (size_t)n); refbuf[n] = '\0';
            while (n > 0 && isspace((unsigned char)refbuf[n-1])) refbuf[--n] = '\0';
            ref = refbuf;
        }

        Object *o = resolve(ref);
        if (!o) {
            set_result("objet introuvable");
            emit(HC_ERR, "   !! objet introuvable : %s", ref);
            return;
        }

        o->visible = showing;
        char d[64]; hc_describe(o, d, sizeof d);

        if (at) {
            char pt[256];
            eval_point(skip_spaces(at + 2), pt, sizeof pt);
            if (!geom_write(o, "loc", pt)) {
                emit(HC_ERR, "   !! show … at : point mal formé : %s", pt);
                return;
            }
            notify_field(o);
            set_result("");
            emit(HC_INFO, "   → %s : visible en %s", d, pt);
            return;
        }

        notify_field(o);
        set_result("");
        emit(HC_INFO, "   → %s : %s", d, showing ? "visible" : "caché");
        return;
    }

    /* --- play <son> ---
     * HyperCard accepte une suite de notes derrière le nom du son
     * (« play "boing" tempo 200 c4 e4 »). On ne retient que le nom : le reste
     * demande un synthétiseur, pas un lecteur d'échantillon. */
    if (ci_equal(verb, "play")) {
        char val[256];
        const char *s = skip_spaces(rest);
        if (*s == '"') quoted(s, val, HC_VAL);
        else           next_word(s, val, HC_VAL);
        if (g_host && g_host->play_sound) g_host->play_sound(val);
        set_result("");
        emit(HC_INFO, "   ♪ play \"%s\"", val);
        return;
    }

    /* --- pass <message> --- */
    if (ci_equal(verb, "pass")) {
        g_pass = 1;
        emit(HC_INFO, "   ↑ pass");
        return;
    }

    /* --- beep : utile pour tester --- */
    if (ci_equal(verb, "beep")) {
        emit(HC_INFO, "   ♪ beep");
        return;
    }

    /* --- appel direct d'un gestionnaire : « carre 7 » → on carre n ---
     * Si un objet de la chaîne courante définit « on <verb> », on le lui
     * envoie avec les arguments (séparés par des virgules) évalués. */
    {
        Object *start = g_me ? g_me : g_current_card;
        Object *chain[8];
        int nc = build_chain(start, chain, 8);
        for (int i = 0; i < nc; i++) {
            const char *end = NULL, *hdr = NULL;
            if (find_handler(chain[i]->script, verb, &end, &hdr)) {
                char (*argv)[HC_VAL] = arena_rows(16);
                int argc = 0;
                const char *a = skip_spaces(rest);
                while (*a && argc < 16) {
                    /* découpe au niveau des virgules de premier niveau */
                    char *one = arena_buf();
            int len = 0, depth = 0, inq = 0;
                    while (*a && !(depth == 0 && !inq && *a == ',')) {
                        if (*a == '"') inq = !inq;
                        else if (!inq && *a == '(') depth++;
                        else if (!inq && *a == ')') depth--;
                        if (len < 511) one[len++] = *a;
                        a++;
                    }
                    one[len] = '\0';
                    eval_expr(one, argv[argc], sizeof argv[argc]);
                    argc++;
                    if (*a == ',') a = skip_spaces(a + 1); else break;
                }
                set_result("");
                hc_send_args(start, verb, argv, argc);
                return;
            }
        }
    }

    /* ---- select <morceau> of <champ> ----
     * « select line 2 of field "toc" », « select char 5 to 12 of card field 1 »,
     * et les formes vides « select empty » / « select » qui ne sélectionnent
     * rien. HyperCard accepte aussi « select before/after <morceau> » : ce sont
     * des points d'insertion, donc une plage de longueur nulle.
     *
     * Le travail de découpe est déjà fait par chunk_target, qui rend le champ
     * et les bornes du morceau — le même code que celui qui sert à « put X
     * into line 2 of field Y ». Il n'y a rien à réécrire ici, seulement à
     * mémoriser le résultat et à prévenir l'hôte. */
    if (ci_word(verb, "select")) {
        const char *a = skip_spaces(rest);

        if (!*a || ci_word(a, "empty")) {
            hc_set_selection(NULL, 0, 0);
            return;
        }

        int point_avant = 0, point_apres = 0;
        if (ci_word(a, "before"))     { point_avant = 1; a = skip_spaces(a + 6); }
        else if (ci_word(a, "after")) { point_apres = 1; a = skip_spaces(a + 5); }

        /* « select text of field X » : tout le contenu.
         *
         * before / after valent ici aussi : « select before text of fld x »
         * pose le point d'insertion au début, « after » à la fin. Cette
         * branche sortait sans les consulter, si bien que les deux formes
         * sélectionnaient tout le champ — le contraire de ce qu'elles
         * demandent, et la façon habituelle de placer le curseur avant de
         * taper. */
        if (ci_word(a, "text")) {
            const char *r = skip_spaces(a + 4);
            if (ci_word(r, "of")) r = skip_spaces(r + 2);
            Object *f = resolve(r);
            if (f && f->type == OBJ_FIELD) {
                int fin = (int)strlen(hc_field_text(f));
                if      (point_avant) hc_set_selection(f, 0, 0);
                else if (point_apres) hc_set_selection(f, fin, 0);
                else                  hc_set_selection(f, 0, fin);
            } else emit(HC_ERR, "   !! select : champ introuvable : %s", r);
            return;
        }

        int st, en;
        Object *f = chunk_target(a, &st, &en);
        if (!f) {
            /* Pas de morceau : peut-être le champ entier, « select field 1 ». */
            f = resolve(a);
            if (f && f->type == OBJ_FIELD) {
                const char *t = hc_field_text(f);
                st = 0; en = (int)strlen(t);
            } else {
                emit(HC_ERR, "   !! select : cible introuvable : %s", a);
                return;
            }
        }
        if (point_avant)      hc_set_selection(f, st, 0);
        else if (point_apres) hc_set_selection(f, en, 0);
        else                  hc_set_selection(f, st, en - st);
        return;
    }

    /* ---- wait ----
     *   wait 30 ticks / wait 2 seconds / wait until <condition>
     *   wait while <condition> / wait for 10 ticks
     *
     * Omniprésente dans les piles : elle rythme les animations et laisse le
     * temps de lire un message. L'attente passe par host_idle à chaque tour,
     * sans quoi l'écran resterait figé pendant toute la durée — c'est déjà ce
     * que fait la boucle repeat. Une attente sans redessin donnerait un
     * programme qui semble planté. */
    if (ci_equal(verb, "wait")) {
        /* voir attends() plus haut */
        const char *a = skip_spaces(rest);
        if (ci_word(a, "for")) a = skip_spaces(a + 3);

        if (ci_word(a, "until") || ci_word(a, "while")) {
            int jusqua = ci_word(a, "until");
            const char *cond = skip_spaces(a + 5);
            int tours = 0;
            for (;;) {
                /* Marquer et rendre l'arène à CHAQUE tour : sans cela une
                 * attente un peu longue la saturait, et la condition cessait
                 * d'être évaluée — l'attente devenait infinie. */
                ARENA_MARK;
                char *v = arena_buf();
                eval_checked(cond, v, HC_VAL);
                int vrai = truthy(v);
                ARENA_FREE;
                if (jusqua ? vrai : !vrai) break;
                if (++tours > HC_MAX_LOOP) {
                    emit(HC_ERR, "!! wait interrompu après %d tours", HC_MAX_LOOP);
                    break;
                }
                /* Même raison : sans le sommeil de l'hôte, cette boucle
                 * tournerait à plein régime en attendant un clic. */
                attends(1.0 / 60.0);
            }
            return;
        }

        /* Une durée, suivie de son unité. Le tick — un soixantième de seconde —
         * est l'unité par défaut d'HyperCard.
         *
         * L'unité est retirée AVANT d'évaluer : « wait 5 ticks » passé tel quel
         * à l'analyseur d'expression lui laissait un « ticks » orphelin, qu'il
         * signalait comme du texte incompris. */
        char nb[128];
        nb[0] = '\0';
        const char *ap = a;
        int i = 0;
        while (*ap && i < (int)sizeof nb - 1) {
            /* S'arrêter au mot d'unité, pas au premier espace : la durée peut
             * être une expression, comme « wait 2 * 30 ticks ». */
            const char *w = skip_spaces(ap);
            if (ci_word(w, "tick")   || ci_word(w, "ticks") ||
                ci_word(w, "second") || ci_word(w, "seconds") ||
                ci_word(w, "sec")    || ci_word(w, "secs")) break;
            nb[i++] = *ap++;
        }
        nb[i] = '\0';

        ARENA_MARK;
        char *ev = arena_buf();
        eval_checked(nb, ev, HC_VAL);
        double n = 0;
        as_num(ev, &n);
        ARENA_FREE;

        const char *unite = skip_spaces(ap);
        double ticks = n;
        if (ci_word(unite, "second") || ci_word(unite, "seconds") ||
            ci_word(unite, "sec")    || ci_word(unite, "secs"))
            ticks = n * 60.0;

        if (ticks > HC_MAX_LOOP) ticks = HC_MAX_LOOP;
        attends(ticks / 60.0);
        return;
    }

    /* ---- sort : voir les helpers plus haut pour la mécanique ---- */
    if (ci_equal(verb, "sort")) {
        const char *a = skip_spaces(rest);
        int cartes = 0;                 /* trie-t-on des cartes ? */
        ChunkType morceau = CH_LINE;    /* pour un conteneur */

        if (ci_word(a, "this")) a = skip_spaces(a + 4);
        if (ci_word(a, "marked")) a = skip_spaces(a + 6);   /* accepté, ignoré */

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
            if (!stack) { emit(HC_ERR, "   !! sort : pas de pile"); return; }

            int n = 0;
            for (int i = 0; i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_CARD) n++;
            if (n < 2) return;

            SortItem *tab = calloc((size_t)n, sizeof *tab);
            char **cles = calloc((size_t)n, sizeof *cles);
            if (!tab || !cles) { free(tab); free(cles); return; }

            Object *avant = g_current_card;
            int k = 0;
            for (int i = 0; i < stack->nparts; i++) {
                Object *c = stack->parts[i];
                if (c->type != OBJ_CARD) continue;
                /* Se placer SUR la carte pour évaluer sa clé : « field "nom" »
                 * doit désigner le champ de celle-ci, pas de la carte de
                 * départ. C'est tout le sens du tri par contenu. */
                g_current_card = c;
                /* Par l'arène et non par la pile : HC_VAL vaut un mégaoctet,
                 * et trois tampons de cette taille dans une même fonction
                 * réservaient trois mégaoctets sur une pile d'appel qui en
                 * fait huit. exec_line_body débordait à l'entrée, avant même
                 * sa première instruction. */
                ARENA_MARK;
                char *tmp = arena_buf();
                if (cle) eval_checked(cle, tmp, HC_VAL);
                cles[k] = dupstr(tmp);
                ARENA_FREE;
                tab[k].cle = cles[k]; tab[k].rang = k; tab[k].card = c;
                k++;
            }
            g_current_card = avant;

            g_sort_desc = desc; g_sort_style = style;
            qsort(tab, (size_t)n, sizeof *tab, sort_cmp);

            /* Réécrire les cartes dans leur nouvel ordre, en laissant les
             * fonds à leur place : ils occupent aussi parts[]. */
            k = 0;
            for (int i = 0; i < stack->nparts; i++)
                if (stack->parts[i]->type == OBJ_CARD)
                    stack->parts[i] = tab[k++].card;

            for (int i = 0; i < n; i++) free(cles[i]);
            free(cles); free(tab);
            set_result("");
            return;
        }

        /* --- tri d'un conteneur --- */
        {
            /* La clé est facultative ; sans elle on trie sur l'élément même. */
            const char *by = find_kw(a, "by");
            /* Par l'arène : voir plus haut, la pile ne supporte pas des
             * tampons de HC_VAL. ARENA_MARK est déjà posé plus bas pour la
             * lecture du conteneur ; celui-ci vit jusqu'au ARENA_FREE. */
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

            /* Un seul ARENA_MARK pour toute la branche : il a été posé plus
             * haut avec le tampon `cible`, et les deux se libèrent ensemble. */
            char *src = arena_buf();
            eval_checked(cible, src, HC_VAL);

            int n = chunk_count(src, morceau);
            if (n < 2) { ARENA_FREE; return; }

            SortItem *tab = calloc((size_t)n, sizeof *tab);
            char **cles = calloc((size_t)n, sizeof *cles);
            char **elems = calloc((size_t)n, sizeof *elems);
            if (!tab || !cles || !elems) { free(tab); free(cles); free(elems);
                                           ARENA_FREE; return; }

            for (int i = 0; i < n; i++) {
                int b = 0, e = 0;
                chunk_span1(src, morceau, i + 1, &b, &e);
                elems[i] = malloc((size_t)(e - b) + 1);
                memcpy(elems[i], src + b, (size_t)(e - b));
                elems[i][e - b] = '\0';

                /* `each` : la variable que la clé interroge. Sans clé, on trie
                 * directement sur l'élément. */
                if (cle) {
                    var_set("each", elems[i]);
                    ARENA_MARK;
                    char *v = arena_buf();
                    eval_checked(cle, v, HC_VAL);
                    cles[i] = dupstr(v);
                    ARENA_FREE;
                } else {
                    cles[i] = dupstr(elems[i]);
                }
                tab[i].cle = cles[i]; tab[i].rang = i; tab[i].card = NULL;
            }

            /* Ranger les éléments dans un tableau parallèle indexé par rang,
             * pour les retrouver après le tri. */
            g_sort_desc = desc; g_sort_style = style;
            qsort(tab, (size_t)n, sizeof *tab, sort_cmp);

            char sep[2] = { chunk_sep(morceau), '\0' };
            char *res = arena_buf();
            res[0] = '\0';
            size_t used = 0;
            for (int i = 0; i < n; i++) {
                const char *el = elems[tab[i].rang];
                size_t l = strlen(el);
                if (used + l + 2 >= HC_VAL) break;
                if (i) { res[used++] = sep[0]; }
                memcpy(res + used, el, l); used += l;
                res[used] = '\0';
            }

            container_set(cible, res, 0);

            for (int i = 0; i < n; i++) { free(cles[i]); free(elems[i]); }
            free(cles); free(elems); free(tab);
            ARENA_FREE;
            set_result("");
            return;
        }
    }

    /* ---- choose <outil> tool ----
     * « choose brush tool », « choose line tool ». Le mot « tool » final est
     * facultatif chez certains auteurs ; on le retire s'il est là. */
    if (ci_equal(verb, "choose")) {
        g_visual_dirty = 1;   /* touche à l'écran : voir v3_respire */
        ARENA_MARK;
        char *nom = arena_buf();
        snprintf(nom, HC_VAL, "%s", skip_spaces(rest));
        int n = (int)strlen(nom);
        while (n > 0 && isspace((unsigned char)nom[n-1])) nom[--n] = '\0';

        /* Guillemets d'abord, suffixe ensuite.
         *
         * « choose "Select Tool" » arrivait tel quel jusqu'à l'hôte, qui ne
         * reconnaissait aucun outil de ce nom. Et comme la chaîne se terminait
         * par un guillemet, le retrait du mot « tool » ne se déclenchait pas
         * davantage : les deux problèmes n'en faisaient qu'un.
         *
         * Un seul niveau, et seulement si les deux bouts se répondent : on ne
         * touche pas à un nom qui contiendrait un guillemet isolé. */
        if (n >= 2 && ((nom[0] == '"'  && nom[n-1] == '"') ||
                       (nom[0] == '\'' && nom[n-1] == '\''))) {
            memmove(nom, nom + 1, (size_t)(n - 2));
            n -= 2;
            nom[n] = '\0';
            while (n > 0 && isspace((unsigned char)nom[n-1])) nom[--n] = '\0';
        }

        if (n > 4 && ci_equal(nom + n - 4, "tool")) {
            n -= 4;
            while (n > 0 && isspace((unsigned char)nom[n-1])) n--;
            nom[n] = '\0';
        }
        /* « choose tool 3 » : par numéro, comme la palette. On laisse l'hôte
         * décider, il connaît l'ordre de ses cases. */
        emit(HC_TRACE, "   ✎ choose « %s »", nom);
        if (g_host && g_host->choose_tool) g_host->choose_tool(nom);
        else emit(HC_ERR, "   !! choose : l'hôte ne gère pas les outils");
        ARENA_FREE;
        set_result("");
        return;
    }
    /* « reset paint » : remet toutes les propriétés de dessin à leur valeur
     * par défaut. HyperCard l'a prévue justement parce qu'un gestionnaire qui
     * change filled, pattern ou lineSize les laisse volontiers derrière lui.
     *
     * Sans elle, doLegend de Graph Maker laissait « filled » à true, et au
     * tracé suivant doPieChart dessinait son cercle PLEIN au lieu de vide :
     * le seau partait alors d'un pixel noir et noyait le camembert entier.
     * Le premier tracé passait, le second non — un état qui survit d'un
     * appel à l'autre, et rien pour le signaler. */
    if (ci_equal(verb, "reset")) {
        const char *quoi = skip_spaces(rest);
        if (ci_word(quoi, "paint")) {
            host_global_set("filled",   "false");
            host_global_set("pattern",  "2");   /* le noir de HyperCard */
            host_global_set("lineSize", "1");
            host_global_set("brush",    "8");
            host_global_set("textFont", "geneva");
            host_global_set("textSize", "12");
            host_global_set("textStyle","plain");
            host_global_set("textAlign","left");
            host_global_set("textHeight","16");
            set_result("");
            return;
        }
        emit(HC_ERR, "   !! reset : seul « reset paint » est reconnu");
        set_result("");
        return;
    }
    /* « doMenu » : HyperCard scriptait par les menus tout ce qui n'avait pas
     * de commande propre — effacer l'image, tout sélectionner, copier. Les
     * piles d'époque s'en servent constamment.
     *
     * On route vers l'hôte, seul à connaître ses menus. Sans cette commande,
     * le clearScreen de Graph Maker ne faisait rien du tout, et chaque tracé
     * se superposait au précédent. */
    if (ci_equal(verb, "domenu")) {
        ARENA_MARK;
        char *item = arena_buf();
        eval_checked(rest, item, HC_VAL);
        hc_do_menu(item);          /* message d'abord, action ensuite */
        ARENA_FREE;
        set_result("");
        return;
    }
    /* ---- drag from <point> to <point> [with <touches>] ---- */
    if (ci_equal(verb, "drag")) {
        g_visual_dirty = 1;   /* touche à l'écran : voir v3_respire */
        const char *a = skip_spaces(rest);
        if (ci_word(a, "from")) a = skip_spaces(a + 4);
        const char *to = find_kw(a, "to");
        if (!to) { emit(HC_ERR, "   !! drag : « to » manquant"); return; }

        ARENA_MARK;
        char *p1 = arena_buf(), *p2 = arena_buf();
        int len = (int)(to - a);
        if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
        memcpy(p1, a, (size_t)len); p1[len] = '\0';

        const char *reste = skip_spaces(to + 2);
        const char *with = find_kw(reste, "with");
        char *mods = arena_buf();
        mods[0] = '\0';
        if (with) {
            snprintf(mods, HC_VAL, "%s", skip_spaces(with + 4));
            len = (int)(with - reste);
            if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
            memcpy(p2, reste, (size_t)len); p2[len] = '\0';
        } else {
            snprintf(p2, HC_VAL, "%s", reste);
        }

        char *v1 = arena_buf(), *v2 = arena_buf();
        eval_point(p1, v1, HC_VAL);
        eval_point(p2, v2, HC_VAL);
        int x1 = atoi(v1), y1 = 0, x2 = atoi(v2), y2 = 0;
        const char *c1 = strchr(v1, ','), *c2 = strchr(v2, ',');
        if (c1) y1 = atoi(c1 + 1);
        if (c2) y2 = atoi(c2 + 1);

        { const char *t = host_global("tool");
          emit(HC_TRACE, "   ✎ drag %d,%d -> %d,%d (%s)",
               x1, y1, x2, y2, t ? t : "?"); }
        if (g_host && g_host->drag) g_host->drag(x1, y1, x2, y2, mods);
        else emit(HC_ERR, "   !! drag : l'hôte ne gère pas la souris");
        ARENA_FREE;
        set_result("");
        return;
    }

    /* ---- click at <point> [with <touches>] ---- */
    if (ci_equal(verb, "click")) {
        g_visual_dirty = 1;   /* touche à l'écran : voir v3_respire */
        const char *a = skip_spaces(rest);
        if (ci_word(a, "at")) a = skip_spaces(a + 2);

        ARENA_MARK;
        const char *with = find_kw(a, "with");
        char *pt = arena_buf(), *mods = arena_buf();
        mods[0] = '\0';
        if (with) {
            snprintf(mods, HC_VAL, "%s", skip_spaces(with + 4));
            int len = (int)(with - a);
            if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
            memcpy(pt, a, (size_t)len); pt[len] = '\0';
        } else {
            snprintf(pt, HC_VAL, "%s", a);
        }

        char *v = arena_buf();
        eval_point(pt, v, HC_VAL);
        int x = atoi(v), y = 0;
        const char *c = strchr(v, ',');
        if (c) y = atoi(c + 1);

        { const char *t = host_global("tool");
          emit(HC_TRACE, "   ✎ click at %d,%d (%s)", x, y, t ? t : "?"); }
        if (g_host && g_host->click_at) g_host->click_at(x, y, mods);
        else emit(HC_ERR, "   !! click : l'hôte ne gère pas la souris");
        ARENA_FREE;
        set_result("");
        return;
    }

    /* ---- type <texte> [with <touches>] ---- */
    if (ci_equal(verb, "type")) {
        g_visual_dirty = 1;   /* touche à l'écran : voir v3_respire */
        const char *a = skip_spaces(rest);
        const char *with = find_kw(a, "with");

        ARENA_MARK;
        char *expr = arena_buf(), *mods = arena_buf();
        mods[0] = '\0';
        if (with) {
            snprintf(mods, HC_VAL, "%s", skip_spaces(with + 4));
            int len = (int)(with - a);
            if (len > (int)HC_VAL - 1) len = (int)HC_VAL - 1;
            memcpy(expr, a, (size_t)len); expr[len] = '\0';
        } else {
            snprintf(expr, HC_VAL, "%s", a);
        }

        char *txt = arena_buf();
        eval_checked(expr, txt, HC_VAL);
        if (g_host && g_host->type_text) g_host->type_text(txt, mods);
        else emit(HC_ERR, "   !! type : l'hôte ne gère pas le clavier");
        ARENA_FREE;
        set_result("");
        return;
    }

    /* ---- visual : arme un effet pour le prochain « go » ---- */
    if (ci_equal(verb, "visual")) {
        const char *a = skip_spaces(rest);
        if (ci_word(a, "effect")) a = skip_spaces(a + 6);

        g_visual_effect[0] = g_visual_speed[0] = g_visual_image[0] = '\0';

        /* « to <image> » se détache en premier : il termine la commande, et le
         * reste appartient au nom de l'effet et à sa vitesse. */
        const char *to = find_kw(a, "to");
        char reste[192];
        int len = to ? (int)(to - a) : (int)strlen(a);
        if (len > (int)sizeof reste - 1) len = (int)sizeof reste - 1;
        memcpy(reste, a, (size_t)len);
        reste[len] = '\0';
        while (len > 0 && isspace((unsigned char)reste[len-1])) reste[--len] = '\0';

        if (to) {
            const char *img = skip_spaces(to + 2);
            snprintf(g_visual_image, sizeof g_visual_image, "%s", img);
            int n = (int)strlen(g_visual_image);
            while (n > 0 && isspace((unsigned char)g_visual_image[n-1]))
                g_visual_image[--n] = '\0';
        }

        /* La vitesse est en QUEUE, et peut faire deux mots : « very fast ».
         * On la retire par la fin, ce qui laisse le nom de l'effet — lui aussi
         * en plusieurs mots parfois, d'où l'impossibilité de découper par la
         * gauche. */
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

        /* %.63s plutôt que %s : `reste` fait 192 octets, la cible 64. La
         * troncature est volontaire — aucun nom d'effet d'HyperCard n'atteint
         * cette longueur — mais il faut la dire, sinon le compilateur la
         * signale à juste titre. */
        snprintf(g_visual_effect, sizeof g_visual_effect, "%.63s", reste);
        if (!g_visual_effect[0])
            snprintf(g_visual_effect, sizeof g_visual_effect, "%s", "dissolve");
        set_result("");
        return;
    }

    /* ---- open file <nom> ---- */
    if (ci_equal(verb, "open") && ci_word(skip_spaces(rest), "file")) {
        ARENA_MARK;
        char *nom = arena_buf();
        eval_checked(skip_spaces(skip_spaces(rest) + 4), nom, HC_VAL);
        if (file_open(nom)) set_result("");
        else { set_result("Can't open file");
               emit(HC_ERR, "   !! open file : %s", nom); }
        ARENA_FREE;
        return;
    }

    /* ---- close file <nom> ---- */
    if (ci_equal(verb, "close") && ci_word(skip_spaces(rest), "file")) {
        ARENA_MARK;
        char *nom = arena_buf();
        eval_checked(skip_spaces(skip_spaces(rest) + 4), nom, HC_VAL);
        file_close(nom);
        set_result("");
        ARENA_FREE;
        return;
    }

    /* ---- read from file <nom> [at <pos>] for <n> | until <car> ----
     * Le texte lu va dans « it », comme toute lecture en HyperTalk. */
    if (ci_equal(verb, "read")) {
        const char *a = skip_spaces(rest);
        if (ci_word(a, "from")) a = skip_spaces(a + 4);
        if (!ci_word(a, "file")) { emit(HC_ERR, "   !! read : « file » attendu"); return; }
        a = skip_spaces(a + 4);

        /* Découper avant d'évaluer : le nom du fichier s'arrête au premier
         * mot-clé, et lui passer « at 4 for 20 » ne donnerait rien de bon. */
        const char *at  = find_kw(a, "at");
        const char *fo  = find_kw(a, "for");
        const char *unt = find_kw(a, "until");
        const char *fin = at ? at : (fo ? fo : unt);

        ARENA_MARK;
        char *nom = arena_buf();
        {
            char brut[512];
            int len = fin ? (int)(fin - a) : (int)strlen(a);
            if (len > (int)sizeof brut - 1) len = (int)sizeof brut - 1;
            memcpy(brut, a, (size_t)len); brut[len] = '\0';
            eval_checked(brut, nom, HC_VAL);
        }

        FILE *f = file_find(nom);
        if (!f) {
            set_result("File is not open");
            emit(HC_ERR, "   !! read : fichier non ouvert : %s", nom);
            ARENA_FREE; return;
        }

        if (at) {
            char *pv = arena_buf();
            const char *bornes = fo ? fo : unt;
            char brut[256];
            const char *deb = skip_spaces(at + 2);
            int len = bornes ? (int)(bornes - deb) : (int)strlen(deb);
            if (len > (int)sizeof brut - 1) len = (int)sizeof brut - 1;
            memcpy(brut, deb, (size_t)len); brut[len] = '\0';
            eval_checked(brut, pv, HC_VAL);
            long pos = atol(pv);
            /* Positif : depuis le début, et 1-based comme tout HyperTalk.
             * Négatif : depuis la fin. */
            if (pos >= 0) fseek(f, pos > 0 ? pos - 1 : 0, SEEK_SET);
            else          fseek(f, pos, SEEK_END);
        }

        char *out = arena_buf();
        int n = 0;

        if (fo) {
            char *cv = arena_buf();
            eval_checked(skip_spaces(fo + 3), cv, HC_VAL);
            long combien = atol(cv);
            while (n < HC_VAL - 1 && n < combien) {
                int c = fgetc(f);
                if (c == EOF) break;
                out[n++] = (char)c;
            }
        } else if (unt) {
            char mot[64];
            next_word(skip_spaces(unt + 5), mot, sizeof mot);
            int stop = file_constant(mot);
            if (stop == -1) {
                /* Pas une constante : un caractère, éventuellement calculé. */
                char *cv = arena_buf();
                eval_checked(skip_spaces(unt + 5), cv, HC_VAL);
                stop = cv[0] ? (unsigned char)cv[0] : '\n';
            }
            while (n < HC_VAL - 1) {
                int c = fgetc(f);
                if (c == EOF) break;
                out[n++] = (char)c;
                /* Le caractère d'arrêt fait PARTIE du texte lu : c'est ce que
                 * fait HyperCard, et ce qui permet d'enchaîner les lectures
                 * ligne à ligne sans perdre les séparateurs. */
                if (stop >= 0 && c == stop) break;
            }
        } else {
            /* Ni « for » ni « until » : tout le fichier. */
            while (n < HC_VAL - 1) {
                int c = fgetc(f);
                if (c == EOF) break;
                out[n++] = (char)c;
            }
        }
        out[n] = '\0';

        /* Dire la troncature plutôt que de couper en silence : un script qui
         * lit un fichier trop gros doit pouvoir s'en apercevoir, et découper
         * sa lecture en plusieurs « read ... for N ». */
        if (n >= HC_VAL - 1) {
            set_result("Value too large");
            emit(HC_ERR, "   !! read : texte tronqué à %d octets", HC_VAL - 1);
        } else {
            set_result(n > 0 ? "" : "End of file");
        }
        var_set("it", out);
        ARENA_FREE;
        return;
    }

    /* ---- write <texte> to file <nom> [at <pos>|end|eof] ---- */
    if (ci_equal(verb, "write")) {
        const char *a = skip_spaces(rest);
        const char *to = find_kw(a, "to");
        if (!to) { emit(HC_ERR, "   !! write : « to file » attendu"); return; }

        ARENA_MARK;
        char *txt = arena_buf();
        {
            /* Par l'arène : voir le commentaire du tri. */
            char *brut = arena_buf();
            int len = (int)(to - a);
            if (len > HC_VAL - 1) len = HC_VAL - 1;
            memcpy(brut, a, (size_t)len); brut[len] = '\0';
            eval_checked(brut, txt, HC_VAL);
        }

        const char *r2 = skip_spaces(to + 2);
        if (ci_word(r2, "file")) r2 = skip_spaces(r2 + 4);
        const char *at = find_kw(r2, "at");

        char *nom = arena_buf();
        {
            char brut[512];
            int len = at ? (int)(at - r2) : (int)strlen(r2);
            if (len > (int)sizeof brut - 1) len = (int)sizeof brut - 1;
            memcpy(brut, r2, (size_t)len); brut[len] = '\0';
            eval_checked(brut, nom, HC_VAL);
        }

        FILE *f = file_find(nom);
        if (!f) {
            set_result("File is not open");
            emit(HC_ERR, "   !! write : fichier non ouvert : %s", nom);
            ARENA_FREE; return;
        }

        if (at) {
            const char *p = skip_spaces(at + 2);
            if (ci_word(p, "end") || ci_word(p, "eof")) fseek(f, 0, SEEK_END);
            else {
                char *pv = arena_buf();
                eval_checked(p, pv, HC_VAL);
                long pos = atol(pv);
                if (pos >= 0) fseek(f, pos > 0 ? pos - 1 : 0, SEEK_SET);
                else          fseek(f, pos, SEEK_END);
            }
        }

        fwrite(txt, 1, strlen(txt), f);
        fflush(f);      /* pour qu'un autre programme voie le texte tout de suite */
        set_result("");
        ARENA_FREE;
        return;
    }

    /* ---- save [this] stack [<nom>] as [stack] <fichier> ----
     * Duplique la pile sous un autre nom. HyperCard n'a pas de commande
     * « enregistrer » : il écrivait en continu, et « save » sert donc à faire
     * une copie, pas à valider un travail en cours. */
    if (ci_equal(verb, "save")) {
        const char *a = skip_spaces(rest);
        if (ci_word(a, "this")) a = skip_spaces(a + 4);
        if (ci_word(a, "stack")) a = skip_spaces(a + 5);

        const char *as = find_kw(a, "as");
        if (!as) {
            emit(HC_ERR, "   !! save : « as » attendu");
            set_result("Bad parameter");
            return;
        }

        ARENA_MARK;
        /* La pile à copier : celle qu'on nomme, ou la courante. */
        Object *pile = g_current_card ? g_current_card->owner : NULL;
        if (as > a) {
            char brut[512];
            int n = (int)(as - a);
            if (n > (int)sizeof brut - 1) n = (int)sizeof brut - 1;
            memcpy(brut, a, (size_t)n); brut[n] = '\0';
            char *nom = arena_buf();
            eval_checked(brut, nom, HC_VAL);
            if (nom[0]) {
                /* Un nom explicite ne peut désigner que la pile ouverte : nous
                 * n'en tenons qu'une à la fois. On vérifie plutôt que de
                 * copier silencieusement la mauvaise. */
                if (!pile || !pile->name || !ci_equal(pile->name, nom)) {
                    emit(HC_ERR, "   !! save : pile introuvable : %s", nom);
                    set_result("No such stack");
                    ARENA_FREE;
                    return;
                }
            }
        }

        const char *dest = skip_spaces(as + 2);
        if (ci_word(dest, "stack")) dest = skip_spaces(dest + 5);
        char *chemin = arena_buf();
        eval_checked(dest, chemin, HC_VAL);

        if (!pile || !chemin[0]) {
            set_result("Bad parameter");
            ARENA_FREE;
            return;
        }
        if (g_host && g_host->save_stack && g_host->save_stack(pile, chemin))
            set_result("");
        else {
            emit(HC_ERR, "   !! save : échec de l'écriture : %s", chemin);
            set_result("Can't save stack");
        }
        ARENA_FREE;
        return;
    }

    /* ---- start using stack "X" / stop using stack "X" ----
     *
     * Une pile en usage s'insère dans la chaîne de messages : ses gestionnaires
     * deviennent appelables de partout. C'est ainsi qu'on partageait du code
     * entre piles avant les greffons — une bibliothèque déclarée une fois.
     *
     * Redéclarer une pile déjà en usage la DÉPLACE en tête plutôt que de
     * l'ajouter deux fois : c'est ce que dit le manuel, et cela permet de
     * changer la priorité d'une bibliothèque sans la retirer d'abord. */
    if (ci_equal(verb, "start") || ci_equal(verb, "stop")) {
        const char *a = skip_spaces(rest);
        if (ci_word(a, "using")) {
            int demarrer = ci_equal(verb, "start");
            a = skip_spaces(a + 5);
            if (ci_word(a, "stack")) a = skip_spaces(a + 5);

            ARENA_MARK;
            char *nom = arena_buf();
            eval_checked(a, nom, HC_VAL);

            /* load_stack et non open_stack : une pile en usage reste INVISIBLE.
             *
             * C'est le comportement d'HyperCard, et il a sa logique — une
             * bibliothèque n'a rien à montrer, et lui ouvrir une fenêtre
             * encombrerait l'écran à chaque « start using ». Seul « go to
             * stack » affiche. */
            Object *pile = find_open_stack(nom);
            if (!pile && demarrer && g_host && g_host->load_stack)
                pile = g_host->load_stack(nom);

            if (!pile) {
                set_result("No such stack");
                emit(HC_ERR, "   !! using : pile introuvable : %s", nom);
                ARENA_FREE;
                return;
            }

            /* La retirer d'abord, dans les deux cas : « stop » n'a que cela à
             * faire, et « start » s'en sert pour la remettre en tête. */
            for (int i = 0; i < g_nusing; i++) {
                if (g_using[i] != pile) continue;
                for (int k = i; k + 1 < g_nusing; k++) g_using[k] = g_using[k+1];
                g_nusing--;
                break;
            }
            if (demarrer && g_nusing < HC_MAX_USING) g_using[g_nusing++] = pile;

            set_result("");
            ARENA_FREE;
            return;
        }
    }

    /* ---- mark / unmark ----
     *   mark card 3            mark cards where <condition>
     *   unmark all cards       unmark this card
     *
     * Marquer désigne un sous-ensemble d'une pile sans la modifier : on trie
     * une fois, puis « print marked cards » ou « go next marked card »
     * travaille dessus. C'est la façon dont les piles de données filtraient
     * leur contenu, avant les bases de données.
     *
     * « where » évalue la condition SUR chaque carte : on s'y déplace le temps
     * du calcul, comme le fait déjà le tri. Sans cela « field "ville" »
     * désignerait toujours le champ de la carte de départ. */
    if (ci_equal(verb, "mark") || ci_equal(verb, "unmark")) {
        int poser = ci_equal(verb, "mark");
        const char *a = skip_spaces(rest);

        Object *pile = g_current_card ? g_current_card->owner : NULL;
        while (pile && pile->type != OBJ_STACK) pile = pile->owner;
        if (!pile) { set_result("No stack"); return; }

        if (ci_word(a, "all")) {
            for (int i = 0; i < pile->nparts; i++)
                if (pile->parts[i]->type == OBJ_CARD)
                    pile->parts[i]->marked = poser;
            set_result("");
            return;
        }

        const char *wh = find_kw(a, "where");
        if (wh) {
            const char *cond = skip_spaces(wh + 5);
            Object *avant = g_current_card;
            for (int i = 0; i < pile->nparts; i++) {
                Object *c = pile->parts[i];
                if (c->type != OBJ_CARD) continue;
                g_current_card = c;
                ARENA_MARK;
                char *v = arena_buf();
                eval_checked(cond, v, HC_VAL);
                if (truthy(v)) c->marked = poser;
                ARENA_FREE;
            }
            g_current_card = avant;
            set_result("");
            return;
        }

        /* Une carte désignée, ou la courante.
         *
         * « this card » se résout très bien, mais « mark card 2 » demande que
         * `a` porte encore le mot « card » — on ne le retire donc pas, et l'on
         * laisse resolve faire son travail. Le cas vide, lui, vise la carte
         * courante. */
        Object *c = *a ? resolve(a) : g_current_card;
        if (!c) {
            ARENA_MARK;
            char *v = arena_buf();
            eval_checked(a, v, HC_VAL);
            c = resolve(v);
            ARENA_FREE;
        }
        if (c && c->type == OBJ_CARD) { c->marked = poser; set_result(""); }
        else { set_result("No such card");
               emit(HC_ERR, "   !! %s : carte introuvable", verb); }
        return;
    }

    /* ---- print card / print this stack / print marked cards ----
     *
     * Le noyau compose la LISTE des cartes à sortir et la passe à l'hôte, qui
     * seul connaît le papier. Une carte par page.
     *
     * « print field » et « print <expression> » d'HyperCard ne sont pas ici :
     * ils impriment du texte, ce qui est un autre chemin — et l'on imprime
     * plus souvent une carte que le contenu d'un champ. */
    if (ci_equal(verb, "print")) {
        const char *a = skip_spaces(rest);
        if (ci_word(a, "this")) a = skip_spaces(a + 4);

        Object *pile = g_current_card ? g_current_card->owner : NULL;
        while (pile && pile->type != OBJ_STACK) pile = pile->owner;

        Object *liste[512];
        int n = 0;

        if (ci_word(a, "stack") || ci_word(a, "all")) {
            /* toute la pile */
            if (pile)
                for (int i = 0; i < pile->nparts && n < 512; i++)
                    if (pile->parts[i]->type == OBJ_CARD) liste[n++] = pile->parts[i];
        }
        else if (ci_word(a, "marked")) {
            if (pile)
                for (int i = 0; i < pile->nparts && n < 512; i++)
                    if (pile->parts[i]->type == OBJ_CARD && pile->parts[i]->marked)
                        liste[n++] = pile->parts[i];
        }
        else if (ci_word(a, "card") || ci_word(a, "cd") || !*a) {
            const char *r = *a ? skip_spaces(a + (ci_word(a, "cd") ? 2 : 4)) : "";
            if (!*r) {
                /* « print card » nu : la carte courante. */
                if (g_current_card) liste[n++] = g_current_card;
            } else {
                /* « print card 3 », « print card "index" », « print card 2 to 7 » */
                const char *to = find_kw(r, "to");
                if (to) {
                    ARENA_MARK;
                    char brut[256];
                    int len = (int)(to - r);
                    if (len > (int)sizeof brut - 1) len = (int)sizeof brut - 1;
                    memcpy(brut, r, (size_t)len); brut[len] = '\0';
                    char *v1 = arena_buf(), *v2 = arena_buf();
                    eval_checked(brut, v1, HC_VAL);
                    eval_checked(skip_spaces(to + 2), v2, HC_VAL);
                    int d = atoi(v1), f = atoi(v2);
                    if (d < 1) d = 1;
                    for (int i = d; i <= f && n < 512; i++) {
                        Object *c = nth_card(pile, i - 1);
                        if (c) liste[n++] = c;
                    }
                    ARENA_FREE;
                } else {
                    Object *c = resolve(r);
                    if (!c) {
                        ARENA_MARK;
                        char *v = arena_buf();
                        eval_checked(r, v, HC_VAL);
                        c = resolve(v);
                        if (!c) {
                            char ref[128];
                            snprintf(ref, sizeof ref, "card %s", v);
                            c = resolve(ref);
                        }
                        ARENA_FREE;
                    }
                    if (c && c->type == OBJ_CARD) liste[n++] = c;
                }
            }
        }

        if (n == 0) {
            set_result("No cards to print");
            emit(HC_ERR, "   !! print : rien à imprimer");
            return;
        }
        if (g_host && g_host->print_cards) {
            g_host->print_cards(liste, n);
            set_result("");
        } else {
            set_result("Can't print");
            emit(HC_ERR, "   !! print : l'hôte ne sait pas imprimer");
        }
        return;
    }

    emit(HC_ERR, "   ?? verbe inconnu : %s", verb);
}

/* Enveloppe de libération. exec_line a 57 points de sortie : les instrumenter
 * un par un serait une invitation à l'erreur. On renomme le corps et on pose la
 * marque autour — une seule fois, sans toucher à sa logique. Même recette pour
 * term_value et call_function, qui en ont 40 et 36. */
static void exec_line(Object *me, const char *line)
{
    ARENA_MARK;
    exec_line_body(me, line);
    ARENA_FREE;
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

    /* Quatre maillons pour l'objet, la carte, le fond et la pile ; le reste
     * pour les piles en usage, qui viennent après. Une chaîne trop courte les
     * écarterait silencieusement. */
    Object *chain[4 + HC_MAX_USING];
    int n = build_chain(target, chain, (int)(sizeof chain / sizeof *chain));

    /* `the target` vaut le destinataire initial pendant toute la remontée ;
       on empile l'ancien pour les envois imbriqués. */
    int saved_clipped = g_script_clipped;
    g_script_clipped = 0;
    Object *saved_target = g_target;
    Object *saved_me     = g_me;
    g_target = target;

    /* on empile les paramètres du gestionnaire appelant */
    char (*saved_params)[HC_VAL] = arena_rows(16);
    int  saved_nparams = g_nparams;
    for (int i = 0; i < g_nparams; i++)
        memcpy(saved_params[i], g_params[i], sizeof saved_params[i]);

    /* g_params[0] = nom du message, puis les arguments */
    snprintf(g_params[0], sizeof g_params[0], "%s", message);
    g_nparams = 1;
    for (int i = 0; i < argc && g_nparams < 16; i++)
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
            if (!v3_execute(o, message, isfunc))
                exec_body(o, body, end);
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

    /* dépiler les paramètres de l'appelant */
    for (int i = 0; i < saved_nparams; i++)
        memcpy(g_params[i], saved_params[i], sizeof g_params[i]);
    g_nparams = saved_nparams;

    return handled;
}

/* Frontière de message : chaque envoi rend ses tampons en sortant. Sans cela
 * une pile qui envoie des milliers de messages verrait l'arène croître sans
 * fin, puisque seuls parse_factor et exec_stmt libèrent en dessous. */
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
 * On passe par exec_stmt plutôt que de refaire le changement de carte à la
 * main : la séquence closeCard / closeBackground / openBackground / openCard
 * est écrite en toutes lettres à vingt-quatre endroits de ce fichier, et en
 * ajouter un vingt-cinquième exemplaire serait absurde.
 *
 * « Back » et « Home » n'y sont pas parce que « go back » et « go home »
 * n'existent pas : les inscrire ne ferait que déplacer le silence d'un cran.
 * Il y faudrait d'abord un historique de navigation. */
static const struct { const char *article; const char *ligne; } MENUS_NOYAU[] = {
    { "Next",     "go next"  },
    { "Prev",     "go prev"  },
    { "Previous", "go prev"  },
    { "First",    "go first" },
    { "Last",     "go last"  },
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
int hc_menu_trappe(const char *item)
{
    if (!item) return 0;

    g_visual_dirty = 1;               /* touche à l'écran : voir v3_respire */

    ARENA_MARK;
    char (*argv)[HC_VAL] = arena_rows(1);
    snprintf(argv[0], HC_VAL, "%s", item);
    int pris = g_current_card
             ? hc_send_args(g_current_card, "doMenu", argv, 1) : 0;
    ARENA_FREE;
    return pris;
}

void hc_do_menu(const char *item)
{
    if (!item) return;

    if (hc_menu_trappe(item)) return; /* la pile s'en est chargée */

    for (int i = 0; MENUS_NOYAU[i].article; i++)
        if (menu_meme_article(MENUS_NOYAU[i].article, item)) {
            exec_stmt(g_me ? g_me : g_current_card, MENUS_NOYAU[i].ligne);
            return;
        }

    if (g_host && g_host->do_menu) g_host->do_menu(item);
    else emit(HC_ERR, "   !! doMenu : l'hôte ne gère pas les menus");
}

int hc_send(Object *target, const char *message)
{
    ARENA_MARK;
    char (*none)[HC_VAL] = arena_rows(1);
    int r = hc_send_args(target, message, none, 0);
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
    if (!field || field != g_found_field || g_found_len <= 0) return 0;
    if (g_found_card && g_found_card != g_current_card) return 0;
    if (start) *start = g_found_start;
    if (len)   *len   = g_found_len;
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

void hc_do(const char *line)
{
    ARENA_MARK;
    g_depth  = 0;
    g_pass   = 0;
    g_me     = g_current_card;   /* dans la boîte de message, `me` = la carte */
    g_target = g_current_card;
    g_exit_handler = g_exit_repeat = g_next_repeat = 0;
    exec_stmt(g_current_card, line);
    ARENA_FREE;
}
