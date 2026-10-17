/* hc_wasm.c — les modules d'une pile : des XCMD et des XFCN en WebAssembly.
 *
 * Le contrat est dans hc_wasm.h, côté HC, et dans xcmd/hc_xcmd.h, côté
 * module. Les mesures qui ont fait les choix : docs/mesures/xcmd.txt.
 *
 * L'interprète est WAMR (HC/tiers/wamr), dans son mode « fast » seul. Trois
 * choses à savoir, toutes mesurées le 16 octobre :
 *
 *   · LES RAPPELS SONT « BRUTS » (register_natives_raw). Les rappels
 *     ordinaires de WAMR passent par invokeNative, en assembleur par
 *     processeur ; sa version en C suppose des cases de 32 bits et, sur
 *     nos machines 64 bits, TRONQUAIT les pointeurs : SendCardMessage
 *     recevait une chaîne vide. Les rappels bruts reçoivent leurs arguments
 *     dans un tableau, en C pur, et WAMR vérifie encore lui-même qu'une
 *     chaîne « $ » tient dans la mémoire du module — un module qui passe
 *     un pointeur hors de sa mémoire est arrêté AVANT le rappel ;
 *   · APRÈS UNE FAUTE, L'INSTANCE RESTE EN FAUTE : chaque appel suivant
 *     échouait sur la même exception. On jette donc l'instance, et la
 *     suivante repart de zéro ;
 *   · LE MODULE CALCULE DANS UN AUTRE FIL que celui de HC, qui respire
 *     pendant ce temps et l'arrête sur Cmd-. (« un appel, entre deux
 *     fils », plus bas) : wasm_runtime_terminate arrête en 200 ms un module
 *     qui boucle — à condition que WAMR soit construit avec son
 *     gestionnaire de fils (wamr_hc.h). */

#include "hc_wasm.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "wasm_export.h"

struct HcModule {
    char          *nom;
    unsigned char *octets;      /* tels qu'on les a reçus, et réécrits */
    size_t         n;
    /* L'exécution, créée au besoin. WAMR lit dans `lu` et peut le modifier :
     * c'est une copie, qui vit autant que `module`. */
    unsigned char     *lu;
    wasm_module_t      module;
    wasm_module_inst_t inst;
    wasm_exec_env_t    env;
    int                en_cours;
};

/* LE BLOC DE PARAMÈTRES, tel que le module le voit (hc_xcmd.h, qui en
 * vérifie les places à la compilation) : 32 bits par pointeur, petit-
 * boutiste, comme toute mémoire WebAssembly. */
enum {
    BLOC_TAILLE  = 76,
    BLOC_PARAMS  = 4,
    BLOC_RETOUR  = 68,
    BLOC_PASSE   = 72,
    /* L'XWEventInfo d'un événement de fenêtre (hc_xcmd.h, qui en vérifie
     * aussi les places) : what, eventWindow, eventParams[9], eventResult. */
    EV_TAILLE    = 48,
    EV_FENETRE   = 4,
    EV_PARAMS    = 8,
    EV_RESULTAT  = 44,
    PILE_MODULE  = 64 * 1024,
    NIDS_MAX     = 8,          /* modules en cours les uns dans les autres */
    /* La pile du FIL où calcule un module. Celle du module lui-même est à
     * part (PILE_MODULE) : l'interprète « fast » n'empile ici que son
     * propre cadre, et les rappels. */
    PILE_FIL     = 1024 * 1024
};

/* ---- un appel, entre deux fils ---- */

/* LE MODULE CALCULE DANS UN FIL À PART. Sur le fil de HC, il bloquait tout :
 * la boucle d'événements de l'hôte ne tournait plus, et Cmd-. restait dans
 * la file jusqu'à la fin du calcul — jamais, s'il bouclait. Un fil de
 * l'interface qui guettait le clavier lui-même n'a rien arrêté DANS HC
 * (l'application), le 16 octobre.
 *
 * Le fil de HC, lui, attend le module comme il attend dans un « wait » :
 * il respire (hc_respire_module), et entend Cmd-. par le chemin de tous les
 * scripts. Il garde aussi pour lui tout ce qui touche à la pile : un rappel
 * (SendCardMessage, EvalExpr, SetGlobal…) lui CONFIE son travail, et le fil
 * du module attend qu'il soit fait. L'interface n'est donc jamais touchée
 * d'ailleurs que de son fil.
 *
 * LE FIL EST GARDÉ d'un appel à l'autre : un par profondeur, puisque les
 * appels s'emboîtent (un module qui en appelle un autre par SendCardMessage).
 * En créer un à chaque appel coûtait 70 µs, contre 5 µs pour l'appel sur le
 * fil de HC (mesuré dans le conteneur : docs/mesures/xcmd.txt) ; le
 * reprendre, 7 µs, avec l'attente active plus bas.
 *
 * Tout ce qui est partagé est sous `verrou`. Le fil du module ne lit ce qui
 * décrit l'appel qu'entre `a_faire` et `fini`, et le fil de HC ne le relit
 * qu'après `fini`. */
/* UNE DEMANDE d'un rappel au fil de HC. Les chaînes sont dans la mémoire du
 * module : elles y restent en place le temps de la demande, puisque son fil
 * attend, et qu'aucun autre ne touche à cette mémoire. */
typedef enum {
    D_LIGNE = 1,            /* SendCardMessage */
    D_EVALUE,               /* EvalExpr */
    D_GLOBALE,              /* GetGlobal */
    D_POSE_GLOBALE,         /* SetGlobal */
    D_CHAMP,                /* GetFieldByName, ByNum, ByID */
    D_POSE_CHAMP,           /* SetFieldByName, ByNum, ByID */
    D_OUVRE,                /* NewXWindow */
    D_FERME,                /* CloseXWindow */
    D_DESSINE,              /* DrawXWindow */
    D_COPIE,                /* CopyXWindow */
    D_BOUTON,               /* Button */
    D_PEINTURE              /* GetCardPicture */
} Quoi;

typedef struct {
    Quoi        quoi;
    const char *texte;      /* la ligne, l'expression, le nom, le titre */
    const char *valeur;     /* ce qu'on pose */
    int         carte;      /* le champ est de la carte, sinon du fond */
    int         n, par_id;  /* son numéro, ou son id */

    /* Les fenêtres. Les tampons sont dans la mémoire du module, et WAMR en
     * a vérifié les bornes. */
    int                  fenetre;
    int                  rect[4];       /* gauche, haut, droite, bas */
    int                  visible, style, flottante;
    const unsigned char *pixels, *palette;
    unsigned char       *noir;
    int                  largeur, hauteur, echelle;
} Demande;

/* UN ÉVÉNEMENT d'une fenêtre, pour l'externe qui l'a ouverte : son numéro
 * chez HyperCard (hc_xcmd.h), la fenêtre, et ses paramètres. */
typedef struct {
    int         what;
    int         fenetre;
    const char *params[2];
    int         nparams;
} Evenement;

enum {
    X_OUVRE = 1000, X_FERME = 1001,
    X_ENVOIE = 1200, X_POSE = 1201, X_LIT = 1202
};

typedef struct {
    pthread_mutex_t verrou;
    pthread_cond_t  signal;
    int             vivant;     /* le fil existe */
    int             a_faire;    /* un appel l'attend */

    struct HcModule     *mo;
    wasm_function_inst_t boite, externe;
    const char          *nom_externe;   /* « XFCN maccam » */
    const char *const   *argv;
    int                  argc;
    const Evenement     *ev;            /* un événement, au lieu d'argv */
    uint32_t             taille;

    int             demande;    /* `d` attend le fil de HC */
    Demande         d;
    char           *rendu;      /* la chaîne qu'elle rend, ou NULL */
    int             reponse;    /* le nombre qu'elle rend */
    int             fini;       /* le fil du module a rendu */
    int             arrete;     /* 0, ARRET_DELAI ou ARRET_DEMANDE */
    /* Le module guette Button : Cmd-. lui laisse GRACE pour rendre la main
     * de lui-même. Lus et écrits par le fil de HC seulement. */
    int             ecoute;
    double          grace;

    int      ok;                /* les deux appels ont réussi */
    int      sans_boite;        /* hc_boite a rendu une adresse fausse */
    uint32_t bloc;              /* l'adresse du bloc, dans le module */
} Appel;

enum { ARRET_DELAI = 1, ARRET_DEMANDE = 2 };

/* L'ATTENTE ACTIVE, avant de s'endormir. Réveiller un fil endormi coûtait,
 * MESURÉ dans le conteneur Linux, une quinzaine de µs à chaque passage de
 * main : un XFCN vide passait de 5 µs, sur le fil de HC, à 37 µs, et un
 * SendCardMessage de 3 à 25 µs. Chacun des deux fils regarde donc d'abord
 * le drapeau qu'il attend, un dixième de milliseconde au plus, sans dormir
 * — un appel court est rendu bien avant. Le prix : ce dixième de
 * milliseconde de processeur, une fois par appel. Sur un Mac : NON MESURÉ.
 *
 * Ces trois drapeaux s'écrivent sous `verrou` ET atomiquement, puisqu'on
 * les lit aussi sans lui. */
#define GUET 100e-6

/* CMD-. ET UN MODULE QUI GUETTE Button. Button répond oui dès la touche
 * (hc_module_bouton) : une boucle qui le guette en sort d'elle-même, et le
 * module garde son instance — sa mémoire, ses fenêtres. HC lui en laisse le
 * temps avant de l'arrêter de force. Une seconde : un pas de MAC•CAM sur une
 * grande grille, interprété, en prend une fraction (NON MESURÉ sur un Mac). */
#define GRACE 1.0

static int travail_arrive(const Appel *a)
{
    return __atomic_load_n(&a->a_faire, __ATOMIC_ACQUIRE);
}

static int module_rend(const Appel *a)
{
    return __atomic_load_n(&a->fini, __ATOMIC_ACQUIRE)
        || __atomic_load_n(&a->demande, __ATOMIC_ACQUIRE);
}

static int demande_servie(const Appel *a)
{
    return !__atomic_load_n(&a->demande, __ATOMIC_ACQUIRE);
}

static double maintenant(void);

/* Appelée verrou pris, et le rend pris. */
static void guette(Appel *a, int (*arrive)(const Appel *))
{
    if (arrive(a)) return;
    pthread_mutex_unlock(&a->verrou);
    double fin = maintenant() + GUET;
    while (!arrive(a) && maintenant() < fin) {}
    pthread_mutex_lock(&a->verrou);
}

/* ---- les rappels ---- */

/* Tous tournent sur le fil du MODULE, et confient leur travail au fil de
 * HC, seul à toucher à la pile (servir, plus bas).
 *
 * Ce sont des rappels « bruts » : leurs arguments arrivent dans un tableau
 * de cases de 64 bits, un entier dans les 32 premiers, et ce qu'ils rendent
 * se pose dans la première. Une chaîne « $ » a déjà été vérifiée par WAMR,
 * qui pose ici son adresse native ; un tampon « *~ » aussi, avec sa taille.
 *
 * UNE CHAÎNE RENDUE (EvalExpr, GetGlobal, GetFieldBy…) reste du côté de HC :
 * le rappel n'en rend que la longueur, ou -1, et le module la fait copier
 * dans la place qu'il a faite, par hc_rendu (le kit, hc_xcmd.c). HC
 * n'écrit donc jamais dans un tas qu'il ne connaît pas. */

static int32_t entier(const uint64_t *args, int k)
{
    int32_t v;
    memcpy(&v, &args[k], sizeof v);
    return v;
}

static const char *chaine(const uint64_t *args, int k)
{
    return (const char *)(uintptr_t)args[k];
}

static void rend(uint64_t *args, int32_t v)
{
    memcpy(&args[0], &v, sizeof v);
}

/* Confie `d` au fil de HC, et attend qu'il l'ait servie. Une réponse
 * précédente que le module n'a pas fait copier est jetée. */
static Appel *confie(wasm_exec_env_t env, Demande d)
{
    Appel *a = wasm_runtime_get_user_data(env);
    if (!a) return NULL;
    pthread_mutex_lock(&a->verrou);
    free(a->rendu);
    a->rendu = NULL;
    a->reponse = 0;
    a->d = d;
    __atomic_store_n(&a->demande, 1, __ATOMIC_RELEASE);
    pthread_cond_broadcast(&a->signal);
    guette(a, demande_servie);
    while (a->demande) pthread_cond_wait(&a->signal, &a->verrou);
    pthread_mutex_unlock(&a->verrou);
    return a;
}

/* La longueur de la chaîne rendue, ou -1 s'il n'y en a pas. */
static int32_t longueur(const Appel *a)
{
    if (!a || !a->rendu) return -1;
    size_t n = strlen(a->rendu);
    return n > INT32_MAX - 1 ? INT32_MAX - 1 : (int32_t)n;
}

static void rappel_send_card(wasm_exec_env_t env, uint64_t *args)
{
    confie(env, (Demande){ .quoi = D_LIGNE, .texte = chaine(args, 0) });
}

static void rappel_eval_expr(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_EVALUE,
                                      .texte = chaine(args, 0) });
    rend(args, longueur(a));
}

static void rappel_get_global(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_GLOBALE,
                                      .texte = chaine(args, 0) });
    rend(args, longueur(a));
}

static void rappel_set_global(wasm_exec_env_t env, uint64_t *args)
{
    confie(env, (Demande){ .quoi = D_POSE_GLOBALE, .texte = chaine(args, 0),
                           .valeur = chaine(args, 1) });
}

static void rappel_get_field_name(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_CHAMP,
                                      .carte = entier(args, 0) != 0,
                                      .texte = chaine(args, 1) });
    rend(args, longueur(a));
}

static void rappel_get_field_num(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_CHAMP,
                                      .carte = entier(args, 0) != 0,
                                      .n = entier(args, 1) });
    rend(args, longueur(a));
}

static void rappel_get_field_id(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_CHAMP,
                                      .carte = entier(args, 0) != 0,
                                      .n = entier(args, 1), .par_id = 1 });
    rend(args, longueur(a));
}

static void rappel_set_field_name(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_POSE_CHAMP,
                                      .carte = entier(args, 0) != 0,
                                      .texte = chaine(args, 1),
                                      .valeur = chaine(args, 2) });
    rend(args, a ? a->reponse : 0);
}

static void rappel_set_field_num(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_POSE_CHAMP,
                                      .carte = entier(args, 0) != 0,
                                      .n = entier(args, 1),
                                      .valeur = chaine(args, 2) });
    rend(args, a ? a->reponse : 0);
}

static void rappel_set_field_id(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_POSE_CHAMP,
                                      .carte = entier(args, 0) != 0,
                                      .n = entier(args, 1), .par_id = 1,
                                      .valeur = chaine(args, 2) });
    rend(args, a ? a->reponse : 0);
}

/* ---- les fenêtres ---- */

static void rappel_new_xwindow(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_OUVRE,
                                      .rect = { entier(args, 0), entier(args, 1),
                                                entier(args, 2), entier(args, 3) },
                                      .texte = chaine(args, 4),
                                      .visible = entier(args, 5) != 0,
                                      .style = entier(args, 6),
                                      .flottante = entier(args, 7) != 0 });
    rend(args, a ? a->reponse : 0);
}

static void rappel_close_xwindow(wasm_exec_env_t env, uint64_t *args)
{
    confie(env, (Demande){ .quoi = D_FERME, .fenetre = entier(args, 0) });
}

/* Les tailles se vérifient ICI, sur le fil du module : WAMR a vérifié que
 * chaque tampon tient dans sa mémoire, pas qu'il tient l'image annoncée. */
static void rappel_draw_xwindow(wasm_exec_env_t env, uint64_t *args)
{
    int32_t npx = entier(args, 2), l = entier(args, 3), h = entier(args, 4);
    int32_t npal = entier(args, 6), e = entier(args, 7);
    if (l <= 0 || h <= 0 || l > INT32_MAX / h || npx < l * h || npal < 768)
        return;
    confie(env, (Demande){ .quoi = D_DESSINE, .fenetre = entier(args, 0),
                           .pixels = (const unsigned char *)(uintptr_t)args[1],
                           .largeur = l, .hauteur = h,
                           .palette = (const unsigned char *)(uintptr_t)args[5],
                           .echelle = e < 1 ? 1 : e });
}

static void rappel_copy_xwindow(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_COPIE,
                                      .fenetre = entier(args, 0) });
    rend(args, a ? a->reponse : 0);
}

static void rappel_button(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = confie(env, (Demande){ .quoi = D_BOUTON });
    rend(args, a ? a->reponse : 0);
}

static void rappel_get_card_picture(wasm_exec_env_t env, uint64_t *args)
{
    int32_t g = entier(args, 0), h = entier(args, 1);
    int32_t d = entier(args, 2), b = entier(args, 3), n = entier(args, 5);
    int64_t l = (int64_t)d - g, t = (int64_t)b - h;
    if (l <= 0 || t <= 0 || l * t > n) { rend(args, 0); return; }
    Appel *a = confie(env, (Demande){ .quoi = D_PEINTURE,
                                      .rect = { g, h, d, b },
                                      .noir = (unsigned char *)(uintptr_t)args[4] });
    rend(args, a ? a->reponse : 0);
}

/* hc_rendu(tampon, taille) : copie la dernière chaîne rendue, coupée à la
 * taille du tampon, zéro final compris. Elle ne sert qu'une fois. Pas de
 * passage au fil de HC : la réponse est déjà dans `a`, et le fil de HC
 * attend. */
static void rappel_rendu(wasm_exec_env_t env, uint64_t *args)
{
    Appel *a = wasm_runtime_get_user_data(env);
    char *dst = (char *)(uintptr_t)args[0];
    int32_t n = entier(args, 1);
    if (!a || n <= 0) return;
    size_t l = a->rendu ? strlen(a->rendu) : 0;
    if (l > (size_t)n - 1) l = (size_t)n - 1;
    if (l) memcpy(dst, a->rendu, l);
    dst[l] = '\0';
    free(a->rendu);
    a->rendu = NULL;
}

/* Les noms sont ceux de HyperXCmd.h ; les signatures, celles du kit. */
static NativeSymbol RAPPELS[] = {
    { "SendCardMessage", (void *)rappel_send_card,      "($)",    NULL },
    { "EvalExpr",        (void *)rappel_eval_expr,      "($)i",   NULL },
    { "GetGlobal",       (void *)rappel_get_global,     "($)i",   NULL },
    { "SetGlobal",       (void *)rappel_set_global,     "($$)",   NULL },
    { "GetFieldByName",  (void *)rappel_get_field_name, "(i$)i",  NULL },
    { "GetFieldByNum",   (void *)rappel_get_field_num,  "(ii)i",  NULL },
    { "GetFieldByID",    (void *)rappel_get_field_id,   "(ii)i",  NULL },
    { "SetFieldByName",  (void *)rappel_set_field_name, "(i$$)i", NULL },
    { "SetFieldByNum",   (void *)rappel_set_field_num,  "(ii$)i", NULL },
    { "SetFieldByID",    (void *)rappel_set_field_id,   "(ii$)i", NULL },
    { "hc_rendu",        (void *)rappel_rendu,          "(*~)",   NULL },
    { "NewXWindow",      (void *)rappel_new_xwindow,    "(iiii$iii)i", NULL },
    { "CloseXWindow",    (void *)rappel_close_xwindow,  "(i)",    NULL },
    { "DrawXWindow",     (void *)rappel_draw_xwindow,   "(i*~ii*~i)", NULL },
    { "CopyXWindow",     (void *)rappel_copy_xwindow,   "(i)i",   NULL },
    { "Button",          (void *)rappel_button,         "()i",    NULL },
    { "GetCardPicture",  (void *)rappel_get_card_picture, "(iiii*~)i", NULL },
};

/* ---- WAMR, démarré une fois ---- */

static int g_pret;      /* 0 : pas encore ; 1 : prêt ; -1 : impossible */

static int wamr_pret(void)
{
    if (g_pret) return g_pret > 0;
    RuntimeInitArgs init;
    memset(&init, 0, sizeof init);
    init.mem_alloc_type = Alloc_With_System_Allocator;
    if (!wasm_runtime_full_init(&init)) { g_pret = -1; return 0; }
    /* Ses messages iraient sur la sortie standard ; les nôtres disent la
     * même chose, en français, dans `faute`. */
    wasm_runtime_set_log_level(WASM_LOG_LEVEL_FATAL);
    if (!wasm_runtime_register_natives_raw("hc", RAPPELS,
            (uint32_t)(sizeof RAPPELS / sizeof *RAPPELS))) {
        wasm_runtime_destroy();
        g_pret = -1;
        return 0;
    }
    g_pret = 1;
    return 1;
}

/* ---- le temps, le délai, la profondeur ---- */

/* Tout ceci n'est lu et écrit que par le fil de HC : les fils de calcul n'y
 * touchent pas. */
static double g_debut;          /* l'appel le plus extérieur */
static double g_delai;          /* 0 : aucun */
static int    g_profondeur;     /* appels en cours, les uns dans les autres */

static double maintenant(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

void hc_module_delai(double secondes)
{
    g_delai = secondes > 0 ? secondes : 0;
}

int hc_module_actif(void)
{
    return g_profondeur > 0;
}

/* ---- charger et vérifier ---- */

static int est_externe(const char *nom)
{
    return (!strncmp(nom, "XCMD ", 5) || !strncmp(nom, "XFCN ", 5)) && nom[5];
}

/* Une fonction de type (i32) -> (), ou (i32) -> (i32) si `rend`. */
static int signature(wasm_func_type_t t, int rend)
{
    if (wasm_func_type_get_param_count(t) != 1) return 0;
    if (wasm_func_type_get_param_valkind(t, 0) != WASM_I32) return 0;
    return (int)wasm_func_type_get_result_count(t) == (rend ? 1 : 0);
}

/* Charge `octets` dans WAMR, et vérifie ce qu'un module de HC doit être.
 * Rend le module chargé, sa copie dans *lu ; ou NULL, et pourquoi. */
static wasm_module_t charge(const unsigned char *octets, size_t n,
                            unsigned char **lu, char *pourquoi, size_t np)
{
    *lu = NULL;
    if (!wamr_pret()) {
        snprintf(pourquoi, np, "l'interprète WebAssembly n'a pas démarré");
        return NULL;
    }
    if (n == 0) {
        snprintf(pourquoi, np, "le module est vide");
        return NULL;
    }
    if (n > HC_MODULE_OCTETS_MAX) {
        snprintf(pourquoi, np, "le module fait %zu octets, %d au plus",
                 n, HC_MODULE_OCTETS_MAX);
        return NULL;
    }
    unsigned char *copie = malloc(n);
    if (!copie) {
        snprintf(pourquoi, np, "mémoire insuffisante");
        return NULL;
    }
    memcpy(copie, octets, n);
    char err[128] = "";
    wasm_module_t m = wasm_runtime_load(copie, (uint32_t)n, err, sizeof err);
    if (!m) {
        snprintf(pourquoi, np, "ce n'est pas un module WebAssembly lisible "
                 "(%s)", err);
        free(copie);
        return NULL;
    }

    /* SES IMPORTS : des fonctions du module « hc », que HC fournit. Un
     * import inconnu se refuse ICI, plutôt qu'au premier appel. */
    int32_t ni = wasm_runtime_get_import_count(m);
    for (int32_t i = 0; i < ni; i++) {
        wasm_import_t im;
        wasm_runtime_get_import_type(m, i, &im);
        if (im.kind != WASM_IMPORT_EXPORT_KIND_FUNC
            || strcmp(im.module_name, "hc") != 0 || !im.linked) {
            snprintf(pourquoi, np, "le module demande « %s.%s », que HC "
                     "ne fournit pas", im.module_name, im.name);
            wasm_runtime_unload(m);
            free(copie);
            return NULL;
        }
    }

    /* SES EXPORTS : la mémoire, la boîte aux paramètres, et au moins un
     * externe, chacun avec la forme que HC appellera. */
    int externes = 0, boite = 0, memoire = 0;
    int32_t ne = wasm_runtime_get_export_count(m);
    for (int32_t i = 0; i < ne; i++) {
        wasm_export_t e;
        wasm_runtime_get_export_type(m, i, &e);
        if (e.kind == WASM_IMPORT_EXPORT_KIND_MEMORY
            && !strcmp(e.name, "memory")) {
            memoire = 1;
        } else if (e.kind == WASM_IMPORT_EXPORT_KIND_FUNC
                   && !strcmp(e.name, "hc_boite")) {
            boite = signature(e.u.func_type, 1);
        } else if (e.kind == WASM_IMPORT_EXPORT_KIND_FUNC
                   && est_externe(e.name)) {
            if (!signature(e.u.func_type, 0)) {
                snprintf(pourquoi, np, "« %s » n'a pas la forme d'un "
                         "externe de HC", e.name);
                wasm_runtime_unload(m);
                free(copie);
                return NULL;
            }
            externes++;
        }
    }
    const char *manque = !externes ? "le module n'apporte aucun XCMD ni XFCN"
                       : !memoire  ? "le module n'exporte pas sa mémoire"
                       : !boite    ? "le module n'a pas de boîte aux "
                                     "paramètres (hc_boite) : est-il compilé "
                                     "avec le kit de HC ?"
                       : NULL;
    if (manque) {
        snprintf(pourquoi, np, "%s", manque);
        wasm_runtime_unload(m);
        free(copie);
        return NULL;
    }
    *lu = copie;
    return m;
}

/* ---- les fenêtres des modules ----
 *
 * « NewXWindow » : une fenêtre de l'hôte, que le noyau numérote et nomme —
 * son titre est le nom que les scripts emploient. Elle appartient au module
 * qui l'a ouverte, et l'externe qui l'a ouverte en reçoit les événements.
 * Tout ceci n'est touché que par le fil de HC. */
typedef struct {
    int              id;            /* 0 : la case est libre */
    char             nom[256];
    struct HcModule *mo;
    char             externe[80];   /* « XFCN maccam » */
    int              a_ouvrir;      /* xOpenEvt n'est pas encore parti */
} Fenetre;

enum { FENETRES_MAX = 32 };
static Fenetre g_fenetres[FENETRES_MAX];
static int     g_dernier_id;

static Fenetre *fenetre_d_id(int id)
{
    if (id <= 0) return NULL;
    for (int i = 0; i < FENETRES_MAX; i++)
        if (g_fenetres[i].id == id) return &g_fenetres[i];
    return NULL;
}

/* Deux fenêtres du même nom : la plus récente. */
static Fenetre *fenetre_de_nom(const char *nom)
{
    Fenetre *f = NULL;
    for (int i = 0; i < FENETRES_MAX; i++)
        if (g_fenetres[i].id && !strcasecmp(g_fenetres[i].nom, nom)
            && (!f || g_fenetres[i].id > f->id))
            f = &g_fenetres[i];
    return f;
}

/* La case se libère AVANT que l'hôte ferme : s'il rappelait le noyau en
 * fermant, il ne trouverait plus la fenêtre. */
static void oublie(Fenetre *f)
{
    int id = f->id;
    memset(f, 0, sizeof *f);
    hc_xw_ferme(id);
}

/* L'instance d'un module s'en va — une faute, un arrêt, le module retiré ou
 * remplacé, la pile fermée : ses fenêtres partent avec elle, puisque tout
 * ce qu'elles montraient vivait dans sa mémoire. */
static void fenetres_fermees_avec(const struct HcModule *mo)
{
    for (int i = 0; i < FENETRES_MAX; i++)
        if (g_fenetres[i].id && g_fenetres[i].mo == mo) oublie(&g_fenetres[i]);
}

/* NewXWindow, sur le fil de HC, pendant l'appel `a` : la fenêtre est à son
 * module, et à l'externe en cours. Rend son numéro, ou 0. */
static int fenetre_ouvre(const Appel *a, const Demande *d)
{
    Fenetre *f = NULL;
    for (int i = 0; i < FENETRES_MAX && !f; i++)
        if (!g_fenetres[i].id) f = &g_fenetres[i];
    if (!f || !a->nom_externe) return 0;
    int id;
    do {
        g_dernier_id = g_dernier_id < INT32_MAX ? g_dernier_id + 1 : 1;
        id = g_dernier_id;
    } while (fenetre_d_id(id));
    const char *titre = d->texte ? d->texte : "";
    if (!hc_xw_ouvre(id, titre, d->rect, d->visible, d->style, d->flottante))
        return 0;
    f->id = id;
    snprintf(f->nom, sizeof f->nom, "%s", titre);
    f->mo = a->mo;
    snprintf(f->externe, sizeof f->externe, "%s", a->nom_externe);
    f->a_ouvrir = 1;
    return id;
}

static void desinstancie(struct HcModule *mo)
{
    fenetres_fermees_avec(mo);
    if (mo->env) wasm_runtime_destroy_exec_env(mo->env);
    if (mo->inst) wasm_runtime_deinstantiate(mo->inst);
    mo->env = NULL;
    mo->inst = NULL;
}

static void decharge(struct HcModule *mo)
{
    desinstancie(mo);
    if (mo->module) wasm_runtime_unload(mo->module);
    mo->module = NULL;
    free(mo->lu);
    mo->lu = NULL;
}

static int instancie(struct HcModule *mo, char *faute, size_t nf)
{
    if (mo->inst) return 0;
    if (!mo->module) {
        char pourquoi[200];
        mo->module = charge(mo->octets, mo->n, &mo->lu, pourquoi,
                            sizeof pourquoi);
        if (!mo->module) {
            snprintf(faute, nf, "le module « %s » ne se charge pas : %s",
                     mo->nom, pourquoi);
            return -1;
        }
    }
    InstantiationArgs a;
    memset(&a, 0, sizeof a);
    a.default_stack_size = PILE_MODULE;
    a.max_memory_pages = HC_MODULE_PAGES_MAX;
    char err[128] = "";
    mo->inst = wasm_runtime_instantiate_ex(mo->module, &a, err, sizeof err);
    if (!mo->inst) {
        snprintf(faute, nf, "le module « %s » ne démarre pas (%s)",
                 mo->nom, err);
        return -1;
    }
    mo->env = wasm_runtime_create_exec_env(mo->inst, PILE_MODULE);
    if (!mo->env) {
        desinstancie(mo);
        snprintf(faute, nf, "le module « %s » ne démarre pas (mémoire)",
                 mo->nom);
        return -1;
    }
    return 0;
}

/* ---- les modules d'une pile ---- */

static struct HcModule *cherche(const Object *pile, const char *nom, int *rang)
{
    if (!pile || !nom) return NULL;
    for (int i = 0; i < pile->nmodules; i++)
        if (!strcasecmp(pile->modules[i]->nom, nom)) {
            if (rang) *rang = i;
            return pile->modules[i];
        }
    return NULL;
}

/* Range un module neuf, ou remplace celui du même nom. `module` et `lu`,
 * s'ils sont donnés, sont ceux d'une vérification qu'on garde. */
static int range(Object *pile, const char *nom, const unsigned char *octets,
                 size_t n, wasm_module_t module, unsigned char *lu)
{
    struct HcModule *mo = cherche(pile, nom, NULL);
    unsigned char *o = malloc(n ? n : 1);
    char *nm = mo ? NULL : strdup(nom);
    if (!o || (!mo && !nm)) {
        free(o);
        free(nm);
        return -1;
    }
    memcpy(o, octets, n);
    if (!mo) {
        if (pile->nmodules == pile->capmodules) {
            int cap = pile->capmodules ? pile->capmodules * 2 : 4;
            struct HcModule **v = realloc(pile->modules,
                                          (size_t)cap * sizeof *v);
            if (!v) { free(o); free(nm); return -1; }
            pile->modules = v;
            pile->capmodules = cap;
        }
        mo = calloc(1, sizeof *mo);
        if (!mo) { free(o); free(nm); return -1; }
        mo->nom = nm;
        pile->modules[pile->nmodules++] = mo;
    } else {
        decharge(mo);
        free(mo->octets);
    }
    mo->octets = o;
    mo->n = n;
    mo->module = module;
    mo->lu = lu;
    return 0;
}

int hc_module_ajoute(Object *pile, const char *nom,
                     const unsigned char *octets, size_t n,
                     char *pourquoi, size_t npourquoi)
{
    if (!pile || !nom || !*nom || !octets) {
        snprintf(pourquoi, npourquoi, "il manque la pile, le nom ou le module");
        return -1;
    }
    struct HcModule *ancien = cherche(pile, nom, NULL);
    if (ancien && ancien->en_cours) {
        snprintf(pourquoi, npourquoi, "le module « %s » est en cours", nom);
        return -1;
    }
    unsigned char *lu = NULL;
    wasm_module_t m = charge(octets, n, &lu, pourquoi, npourquoi);
    if (!m) return -1;
    if (range(pile, nom, octets, n, m, lu) < 0) {
        wasm_runtime_unload(m);
        free(lu);
        snprintf(pourquoi, npourquoi, "mémoire insuffisante");
        return -1;
    }
    return 0;
}

int hc_module_pose(Object *pile, const char *nom,
                   const unsigned char *octets, size_t n)
{
    if (!pile || !nom || !*nom || !octets) return -1;
    struct HcModule *ancien = cherche(pile, nom, NULL);
    if (ancien && ancien->en_cours) return -1;
    return range(pile, nom, octets, n, NULL, NULL);
}

int hc_module_retire(Object *pile, const char *nom)
{
    int i = 0;
    struct HcModule *mo = cherche(pile, nom, &i);
    if (!mo || mo->en_cours) return 0;
    decharge(mo);
    free(mo->nom);
    free(mo->octets);
    free(mo);
    memmove(&pile->modules[i], &pile->modules[i + 1],
            (size_t)(pile->nmodules - i - 1) * sizeof *pile->modules);
    pile->nmodules--;
    return 1;
}

int hc_module_nombre(const Object *pile)
{
    return pile ? pile->nmodules : 0;
}

const char *hc_module_nom(const Object *pile, int i)
{
    if (!pile || i < 0 || i >= pile->nmodules) return NULL;
    return pile->modules[i]->nom;
}

const unsigned char *hc_module_octets(const Object *pile, int i, size_t *n)
{
    if (!pile || i < 0 || i >= pile->nmodules) { if (n) *n = 0; return NULL; }
    if (n) *n = pile->modules[i]->n;
    return pile->modules[i]->octets;
}

int hc_module_externes(const Object *pile, int i, char *liste, size_t taille)
{
    if (taille) liste[0] = '\0';
    if (!pile || i < 0 || i >= pile->nmodules) return -1;
    struct HcModule *mo = pile->modules[i];
    if (!mo->module) {
        char pourquoi[200];
        mo->module = charge(mo->octets, mo->n, &mo->lu, pourquoi,
                            sizeof pourquoi);
        if (!mo->module) return -1;
    }
    int k = 0;
    size_t pos = 0;
    int32_t ne = wasm_runtime_get_export_count(mo->module);
    for (int32_t j = 0; j < ne; j++) {
        wasm_export_t e;
        wasm_runtime_get_export_type(mo->module, j, &e);
        if (e.kind != WASM_IMPORT_EXPORT_KIND_FUNC || !est_externe(e.name))
            continue;
        int w = snprintf(liste + pos, pos < taille ? taille - pos : 0,
                         "%s%s", k ? "\n" : "", e.name);
        if (w > 0) pos += (size_t)w;
        k++;
    }
    return k;
}

void hc_modules_libere(Object *pile)
{
    if (!pile) return;
    for (int i = 0; i < pile->nmodules; i++) {
        struct HcModule *mo = pile->modules[i];
        decharge(mo);
        free(mo->nom);
        free(mo->octets);
        free(mo);
    }
    free(pile->modules);
    pile->modules = NULL;
    pile->nmodules = pile->capmodules = 0;
}

/* ---- l'appel ---- */

static void pose32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static uint32_t lis32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16
         | (uint32_t)p[3] << 24;
}

/* Le texte d'une exception de WAMR, sans son « Exception: ». */
static const char *exception(wasm_module_inst_t inst)
{
    const char *e = wasm_runtime_get_exception(inst);
    if (!e) return "faute inconnue";
    if (!strncmp(e, "Exception: ", 11)) e += 11;
    return e;
}

/* Le nom d'export exact de cet externe, s'il est dans ce module (HyperTalk
 * ne distingue pas les capitales ; un export, si). */
static int export_de(struct HcModule *mo, const char *genre, const char *nom,
                     char *exact, size_t nexact)
{
    if (!mo->module) return 0;      /* chargé par instancie, plus haut */
    int32_t ne = wasm_runtime_get_export_count(mo->module);
    for (int32_t j = 0; j < ne; j++) {
        wasm_export_t e;
        wasm_runtime_get_export_type(mo->module, j, &e);
        if (e.kind != WASM_IMPORT_EXPORT_KIND_FUNC) continue;
        if (strncmp(e.name, genre, 4) != 0 || e.name[4] != ' ') continue;
        if (strcasecmp(e.name + 5, nom) != 0) continue;
        snprintf(exact, nexact, "%s", e.name);
        return 1;
    }
    return 0;
}

/* Une chaîne dans la boîte, à l'adresse `*o` du module ; son adresse va à
 * la case `ou`. */
static void depose(unsigned char *q, uint32_t b, uint32_t *o,
                   const char *s, unsigned char *ou)
{
    size_t l = strlen(s) + 1;
    memcpy(q + (*o - b), s, l);
    pose32(ou, *o);
    *o += (uint32_t)l;
}

/* LE FIL DU MODULE : la boîte, les paramètres qu'on y range, l'externe.
 * Il ne touche qu'à la mémoire du module, et à `a`.
 *
 * UN ÉVÉNEMENT DE FENÊTRE se range comme chez HyperCard : paramCount à -1,
 * et dans params[0] l'adresse d'un XWEventInfo, posé juste après le bloc,
 * ses chaînes ensuite. */
static void calcule(Appel *a)
{
    struct HcModule *mo = a->mo;
    {
        uint32_t cases[1] = { a->taille };
        a->ok = wasm_runtime_call_wasm(mo->env, a->boite, 1, cases);
        uint32_t b = cases[0];
        if (a->ok && (!b || !wasm_runtime_validate_app_addr(mo->inst, b,
                                                            a->taille))) {
            a->ok = 0;
            a->sans_boite = 1;
        } else if (a->ok) {
            unsigned char *q = wasm_runtime_addr_app_to_native(mo->inst, b);
            memset(q, 0, BLOC_TAILLE);
            uint32_t o = b + BLOC_TAILLE;
            if (a->ev) {
                unsigned char *e = q + BLOC_TAILLE;
                q[0] = q[1] = 0xff;              /* paramCount = -1 */
                pose32(q + BLOC_PARAMS, o);
                memset(e, 0, EV_TAILLE);
                e[0] = (unsigned char)a->ev->what;
                e[1] = (unsigned char)(a->ev->what >> 8);
                pose32(e + EV_FENETRE, (uint32_t)a->ev->fenetre);
                o += EV_TAILLE;
                for (int k = 0; k < a->ev->nparams; k++)
                    depose(q, b, &o, a->ev->params[k], e + EV_PARAMS + 4 * k);
            } else {
                q[0] = (unsigned char)a->argc;   /* paramCount, un short */
                for (int k = 0; k < a->argc; k++)
                    depose(q, b, &o, a->argv[k], q + BLOC_PARAMS + 4 * k);
            }
            a->bloc = b;
            cases[0] = b;
            a->ok = wasm_runtime_call_wasm(mo->env, a->externe, 1, cases);
        }
    }
    pthread_mutex_lock(&a->verrou);
    __atomic_store_n(&a->fini, 1, __ATOMIC_RELEASE);
    pthread_cond_broadcast(&a->signal);
    pthread_mutex_unlock(&a->verrou);
}

/* Un fil gardé : il attend un appel, le calcule, et recommence. Il ne
 * finit qu'avec HC. */
static void *ouvrier(void *p)
{
    Appel *a = p;
    wasm_runtime_init_thread_env();
    pthread_mutex_lock(&a->verrou);
    for (;;) {
        guette(a, travail_arrive);
        while (!a->a_faire) pthread_cond_wait(&a->signal, &a->verrou);
        __atomic_store_n(&a->a_faire, 0, __ATOMIC_RELEASE);
        pthread_mutex_unlock(&a->verrou);
        calcule(a);
        pthread_mutex_lock(&a->verrou);
    }
    return NULL;
}

static Appel g_appels[NIDS_MAX];

/* Le fil de la profondeur `d`, créé au premier besoin. Rend NULL si le
 * système en refuse un. */
static Appel *ouvrier_de(int d)
{
    Appel *a = &g_appels[d];
    if (a->vivant) return a;
    if (pthread_mutex_init(&a->verrou, NULL) != 0) return NULL;
    if (pthread_cond_init(&a->signal, NULL) != 0) {
        pthread_mutex_destroy(&a->verrou);
        return NULL;
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, PILE_FIL);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t fil;
    int parti = pthread_create(&fil, &attr, ouvrier, a) == 0;
    pthread_attr_destroy(&attr);
    if (!parti) {
        pthread_cond_destroy(&a->signal);
        pthread_mutex_destroy(&a->verrou);
        return NULL;
    }
    a->vivant = 1;
    return a;
}

/* Attendre un signal du fil du module, `secondes` au plus. L'horloge des
 * conditions est l'heure murale : macOS n'en offre pas d'autre. */
static void attends_signal(Appel *a, double secondes)
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    long ns = t.tv_nsec + (long)(secondes * 1e9);
    t.tv_sec += ns / 1000000000L;
    t.tv_nsec = ns % 1000000000L;
    pthread_cond_timedwait(&a->signal, &a->verrou, &t);
}

/* Sur le fil de HC : ce que le rappel a demandé. Le fil du module attend,
 * sans toucher à `a`. */
static void sers(Appel *a)
{
    const Demande *d = &a->d;
    switch (d->quoi) {
    case D_LIGNE:
        hc_ligne_de_module(d->texte);
        break;
    case D_EVALUE:
        a->rendu = hc_module_evalue(d->texte);
        break;
    case D_GLOBALE:
        a->rendu = hc_module_globale(d->texte);
        break;
    case D_POSE_GLOBALE:
        hc_module_pose_globale(d->texte, d->valeur);
        break;
    case D_CHAMP:
        a->rendu = hc_module_champ(d->carte, d->texte, d->n, d->par_id);
        break;
    case D_POSE_CHAMP:
        a->reponse = hc_module_pose_champ(d->carte, d->texte, d->n,
                                          d->par_id, d->valeur);
        break;
    case D_OUVRE:
        a->reponse = fenetre_ouvre(a, d);
        break;
    case D_FERME:
    case D_DESSINE:
    case D_COPIE: {
        /* Un module ne touche qu'à SES fenêtres. */
        Fenetre *f = fenetre_d_id(d->fenetre);
        if (!f || f->mo != a->mo) break;
        if (d->quoi == D_FERME) oublie(f);
        else if (d->quoi == D_DESSINE)
            hc_xw_image(f->id, d->pixels, d->largeur, d->hauteur,
                        d->palette, d->echelle);
        else
            a->reponse = hc_xw_hote(f->id, "copie", NULL, NULL, NULL, 0) == 1;
        break;
    }
    case D_BOUTON:
        a->ecoute = 1;
        a->reponse = hc_module_bouton();
        break;
    case D_PEINTURE:
        a->reponse = hc_module_peinture(d->rect, d->noir);
        break;
    }
}

/* LE FIL DE HC, pendant que le module calcule : il sert les demandes de ses
 * rappels, il respire soixante fois par seconde — comme l'hôte, qui n'en
 * fait pas plus (cocoa_idle) —, et il arrête le module sur Cmd-. ou au
 * délai. Même un module qui ne fait qu'appeler ses rappels le laisse
 * respirer : la respiration est réglée sur l'horloge, pas sur l'attente. */
static void servir(Appel *a)
{
    double respire = maintenant() + 1.0 / 60.0;
    pthread_mutex_lock(&a->verrou);
    while (!a->fini) {
        if (a->demande) {
            /* Arrêté, le module n'a plus rien à demander : le rappel lui
             * rend la main sans avoir été servi, et il trouve l'arrêt en
             * revenant. */
            if (!a->arrete) {
                pthread_mutex_unlock(&a->verrou);
                sers(a);
                pthread_mutex_lock(&a->verrou);
            }
            __atomic_store_n(&a->demande, 0, __ATOMIC_RELEASE);
            pthread_cond_broadcast(&a->signal);
        } else {
            guette(a, module_rend);
            double d = respire - maintenant();
            if (!a->fini && !a->demande && d > 0) attends_signal(a, d);
        }
        if (a->fini || maintenant() < respire) continue;

        pthread_mutex_unlock(&a->verrou);
        int demande = hc_respire_module();
        pthread_mutex_lock(&a->verrou);
        respire = maintenant() + 1.0 / 60.0;
        if (a->fini || a->arrete) continue;
        a->arrete = demande ? ARRET_DEMANDE
                  : g_delai > 0 && maintenant() - g_debut > g_delai
                  ? ARRET_DELAI : 0;
        /* Un module qui guette Button l'a déjà vu répondre oui : il a GRACE
         * pour rendre la main, et garder ses fenêtres. */
        if (a->arrete == ARRET_DEMANDE && a->ecoute) {
            if (!a->grace) a->grace = maintenant() + GRACE;
            if (maintenant() < a->grace) a->arrete = 0;
        }
        /* Le fil du module pose `fini` sous ce verrou : l'arrêt vise donc
         * un calcul en cours, ou qui vient JUSTE de rendre. Dans ce second
         * cas, WAMR a marqué une instance qui n'en avait plus besoin ; elle
         * sera jetée comme les autres (appelle). */
        if (a->arrete) wasm_runtime_terminate(a->mo->inst);
    }
    pthread_mutex_unlock(&a->verrou);
}

/* APPELER L'EXTERNE `exact` DE `mo`, dans le fil de sa profondeur : un
 * appel ordinaire (argv, argc), ou un événement de ses fenêtres (ev). Rend
 * 1, ou -1 et pourquoi dans `faute`. Ce qu'il rend — returnValue, ou
 * l'eventResult d'un événement — va dans `res`. */
static int lance(struct HcModule *mo, const char *exact,
                 const char *const *argv, int argc, const Evenement *ev,
                 int *passe, char *res, size_t nres, char *faute, size_t nf)
{
    *passe = 0;
    if (res && nres) res[0] = '\0';
    if (mo->en_cours) {
        snprintf(faute, nf, "le module « %s » est déjà en cours : il ne "
                 "peut pas s'appeler lui-même", mo->nom);
        return -1;
    }
    if (instancie(mo, faute, nf) < 0) return -1;

    /* LA BOÎTE : le bloc, l'événement s'il y en a un, puis les chaînes. */
    size_t taille = BLOC_TAILLE;
    if (ev) {
        taille += EV_TAILLE;
        for (int k = 0; k < ev->nparams; k++)
            taille += strlen(ev->params[k]) + 1;
    }
    for (int k = 0; k < argc; k++) taille += strlen(argv[k]) + 1;
    if (taille > (size_t)HC_MODULE_PAGES_MAX * 65536) {
        snprintf(faute, nf, "%s : des paramètres trop longs", exact);
        return -1;
    }
    wasm_function_inst_t fb = wasm_runtime_lookup_function(mo->inst,
                                                           "hc_boite");
    wasm_function_inst_t fx = wasm_runtime_lookup_function(mo->inst, exact);
    if (!fb || !fx) {
        snprintf(faute, nf, "le module « %s » a perdu « %s »",
                 mo->nom, fb ? exact : "hc_boite");
        return -1;
    }
    if (g_profondeur >= NIDS_MAX) {
        snprintf(faute, nf, "trop de modules appelés les uns dans les "
                 "autres (%d au plus)", NIDS_MAX);
        return -1;
    }

    Appel *a = ouvrier_de(g_profondeur);
    if (!a) {
        snprintf(faute, nf, "le module « %s » n'a pas pu partir : le "
                 "système refuse un fil de plus", mo->nom);
        return -1;
    }
    if (g_profondeur++ == 0) g_debut = maintenant();
    mo->en_cours = 1;
    wasm_runtime_clear_exception(mo->inst);
    wasm_runtime_set_user_data(mo->env, a);

    pthread_mutex_lock(&a->verrou);
    a->mo = mo;
    a->boite = fb;
    a->externe = fx;
    a->nom_externe = exact;
    a->argv = argv;
    a->argc = argc;
    a->ev = ev;
    a->taille = (uint32_t)taille;
    __atomic_store_n(&a->demande, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&a->fini, 0, __ATOMIC_RELEASE);
    a->arrete = a->ok = a->sans_boite = 0;
    a->ecoute = 0;
    a->grace = 0;
    a->bloc = 0;
    __atomic_store_n(&a->a_faire, 1, __ATOMIC_RELEASE);
    pthread_cond_broadcast(&a->signal);
    pthread_mutex_unlock(&a->verrou);
    servir(a);

    wasm_runtime_set_user_data(mo->env, NULL);
    mo->en_cours = 0;
    g_profondeur--;
    free(a->rendu);         /* une réponse que le module n'a pas prise */
    a->rendu = NULL;

    if (a->arrete == ARRET_DELAI)
        snprintf(faute, nf, "le module « %s » a dépassé son délai, "
                 "et il a été arrêté", mo->nom);
    else if (a->arrete == ARRET_DEMANDE)
        snprintf(faute, nf, "le module « %s » a été arrêté", mo->nom);
    else if (a->sans_boite)
        snprintf(faute, nf, "le module « %s » n'a pas fait de place "
                 "pour ses paramètres", mo->nom);
    else if (!a->ok)
        snprintf(faute, nf, "le module « %s » s'est arrêté sur une "
                 "faute : %s", mo->nom, exception(mo->inst));
    if (a->arrete || !a->ok) {
        desinstancie(mo);
        return -1;
    }
    uint32_t b = a->bloc;

    /* Ce qu'il rend : une chaîne de SA mémoire, à vérifier comme tout ce
     * qui vient de lui. */
    const unsigned char *p = wasm_runtime_addr_app_to_native(mo->inst, b);
    uint32_t rv = ev ? lis32(p + BLOC_TAILLE + EV_RESULTAT)
                     : lis32(p + BLOC_RETOUR);
    *passe = p[BLOC_PASSE] != 0;
    if (rv && res && nres) {
        if (!wasm_runtime_validate_app_str_addr(mo->inst, rv)) {
            wasm_runtime_clear_exception(mo->inst);
            snprintf(faute, nf, "le module « %s » rend une valeur hors "
                     "de sa mémoire", mo->nom);
            return -1;
        }
        snprintf(res, nres, "%s",
                 (const char *)wasm_runtime_addr_app_to_native(mo->inst, rv));
    }
    return 1;
}

static int ouvertures(char *faute, size_t nf);

/* Un événement pour la fenêtre `id`, à l'externe qui l'a ouverte. La
 * fenêtre peut ne plus exister au retour — le module l'a fermée, ou s'est
 * arrêté sur une faute : ne la relire que par son numéro. */
static int evenement(int id, int what, const char *p0, const char *p1,
                     int *passe, char *res, size_t nres,
                     char *faute, size_t nf)
{
    *passe = 0;
    Fenetre *f = fenetre_d_id(id);
    if (!f) return 0;
    Evenement ev = { .what = what, .fenetre = id, .params = { p0, p1 },
                     .nparams = p1 ? 2 : p0 ? 1 : 0 };
    char externe[sizeof f->externe];
    snprintf(externe, sizeof externe, "%s", f->externe);
    if (lance(f->mo, externe, NULL, 0, &ev, passe, res, nres, faute, nf) < 0)
        return -1;
    return ouvertures(faute, nf) < 0 ? -1 : 1;
}

/* xOpenEvt, aux fenêtres qui viennent de s'ouvrir — « le premier événement
 * après votre création », chez HyperCard, que MACCAM attendait pour faire
 * sa grille. Il part quand l'appel ou l'événement qui les a ouvertes est
 * fini. Une seule boucle à la fois, et bornée : un module qui rouvrirait
 * une fenêtre à chaque xOpenEvt ne tiendrait pas HC indéfiniment. */
static int ouvertures(char *faute, size_t nf)
{
    static int dedans;
    if (dedans) return 0;           /* la boucle du dessus les prendra */
    dedans = 1;
    int r = 0;
    for (int n = 0; n < 4 * FENETRES_MAX && r == 0; n++) {
        Fenetre *f = NULL;
        /* Celle d'un module en cours attend la fin de SON appel. */
        for (int i = 0; i < FENETRES_MAX && !f; i++)
            if (g_fenetres[i].id && g_fenetres[i].a_ouvrir
                && !g_fenetres[i].mo->en_cours)
                f = &g_fenetres[i];
        if (!f) break;
        f->a_ouvrir = 0;
        int passe;
        char rien[8];
        if (evenement(f->id, X_OUVRE, NULL, NULL, &passe, rien, sizeof rien,
                      faute, nf) < 0)
            r = -1;
    }
    dedans = 0;
    return r;
}

static int appelle(Object *pile, const char *genre, const char *nom,
                   const char *const *argv, int argc, int *passe,
                   char *res, size_t nres, char *faute, size_t nf)
{
    *passe = 0;
    if (res && nres) res[0] = '\0';
    if (nf) faute[0] = '\0';
    if (!pile || !pile->nmodules || !nom || !*nom) return 0;

    for (int i = 0; i < pile->nmodules; i++) {
        struct HcModule *mo = pile->modules[i];
        /* Un module illisible ne répond à rien : il ne doit pas masquer
         * les suivants. On le saura en le listant, ou en l'appelant par
         * un nom qu'aucun autre ne porte. */
        if (!mo->module) {
            char pourquoi[200];
            mo->module = charge(mo->octets, mo->n, &mo->lu, pourquoi,
                                sizeof pourquoi);
            if (!mo->module) continue;
        }
        char exact[80];
        if (!export_de(mo, genre, nom, exact, sizeof exact)) continue;

        if (argc > HC_MODULE_PARAMS_MAX) {
            snprintf(faute, nf, "%s %s : %d paramètres, %d au plus",
                     genre, nom, argc, HC_MODULE_PARAMS_MAX);
            return -1;
        }
        if (lance(mo, exact, argv, argc, NULL, passe, res, nres,
                  faute, nf) < 0)
            return -1;
        return ouvertures(faute, nf) < 0 ? -1 : 1;
    }
    return 0;
}

int hc_module_xcmd(Object *pile, const char *nom,
                   const char *const *argv, int argc, int *passe,
                   char *res, size_t nres, char *faute, size_t nfaute)
{
    return appelle(pile, "XCMD", nom, argv, argc, passe, res, nres,
                   faute, nfaute);
}

int hc_module_xfcn(Object *pile, const char *nom,
                   const char *const *argv, int argc, int *passe,
                   char *res, size_t nres, char *faute, size_t nfaute)
{
    return appelle(pile, "XFCN", nom, argv, argc, passe, res, nres,
                   faute, nfaute);
}

/* ---- les fenêtres, vues des scripts ---- */

/* Ce que HC sert lui-même, pour toute fenêtre : quand le module passe, ou
 * qu'il est en cours — c'est lui qui pose alors, par SendCardMessage. Le
 * nom est celui des scripts : le noyau le tient, l'hôte en fait le titre. */
static int standard(int id, const char *quoi, const char *prop,
                    const char *valeur, char *out, int outlen)
{
    Fenetre *f = fenetre_d_id(id);
    if (!f) return 0;
    if (!strcmp(quoi, "lit") && prop && !strcasecmp(prop, "name")) {
        if (out && outlen > 0) snprintf(out, (size_t)outlen, "%s", f->nom);
        return 1;
    }
    int r = hc_xw_hote(id, quoi, prop, valeur, out, outlen);
    if (r == 1 && !strcmp(quoi, "pose") && prop && !strcasecmp(prop, "name"))
        snprintf(f->nom, sizeof f->nom, "%s", valeur ? valeur : "");
    return r;
}

int hc_module_fenetre(const char *nom, const char *quoi, const char *prop,
                      const char *valeur, char *out, int outlen,
                      char *faute, size_t nfaute)
{
    if (out && outlen > 0) out[0] = '\0';
    if (nfaute) faute[0] = '\0';
    Fenetre *f = nom && quoi ? fenetre_de_nom(nom) : NULL;
    if (!f || !strcmp(quoi, "palette")) return -2;
    if (!strcmp(quoi, "existe")) return 1;

    int id = f->id, passe = 0;
    struct HcModule *mo = f->mo;
    int libre = !mo->en_cours;      /* peut-il recevoir un événement ? */

    if (!strcmp(quoi, "ferme")) {
        /* Fermée de force : le module l'apprend, puis elle se ferme, quoi
         * qu'il réponde. Une faute l'a déjà fermée. */
        char rien[8];
        if (libre && evenement(id, X_FERME, NULL, NULL, &passe, rien,
                               sizeof rien, faute, nfaute) < 0)
            return -3;
        f = fenetre_d_id(id);
        if (f) oublie(f);
        return 1;
    }
    if (!strcmp(quoi, "envoie")) {
        if (!libre) {
            snprintf(faute, nfaute, "le module « %s » est déjà en cours : sa "
                     "fenêtre « %s » ne peut pas recevoir « %s »", mo->nom,
                     f->nom, valeur ? valeur : "");
            return -3;
        }
        char rien[8];
        /* Un message que le module PASSE n'a pas de suite : ce que faisait
         * alors HyperCard n'est PAS MESURÉ. */
        return evenement(id, X_ENVOIE, valeur ? valeur : "", NULL, &passe,
                         rien, sizeof rien, faute, nfaute) < 0 ? -3 : 1;
    }
    int lit = !strcmp(quoi, "lit");
    if (lit || !strcmp(quoi, "pose")) {
        if (!prop) return -1;
        if (libre) {
            int r = lit ? evenement(id, X_LIT, prop, NULL, &passe, out,
                                    out && outlen > 0 ? (size_t)outlen : 0,
                                    faute, nfaute)
                        : evenement(id, X_POSE, prop, valeur ? valeur : "",
                                    &passe, NULL, 0, faute, nfaute);
            if (r < 0) return -3;
            if (!passe) return 1;
        }
        return standard(id, quoi, prop, valeur, out, outlen);
    }
    return hc_xw_hote(id, quoi, prop, valeur, out, outlen);    /* montre, cache */
}

int hc_module_fenetre_fermee(int id, char *faute, size_t nfaute)
{
    if (nfaute) faute[0] = '\0';
    Fenetre *f = fenetre_d_id(id);
    if (!f) {
        hc_xw_ferme(id);
        return 0;
    }
    int passe, r = 0;
    char rien[8];
    if (!f->mo->en_cours
        && evenement(id, X_FERME, NULL, NULL, &passe, rien, sizeof rien,
                     faute, nfaute) < 0)
        r = -1;
    f = fenetre_d_id(id);
    if (f) oublie(f);
    return r;
}
