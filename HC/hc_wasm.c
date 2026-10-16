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
 *   · L'ARRÊT VIENT D'UN AUTRE FIL : wasm_runtime_terminate, appelé par une
 *     sentinelle, arrête en 200 ms un module qui boucle — à condition que
 *     WAMR soit construit avec son gestionnaire de fils (wamr_hc.h). */

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
    PILE_MODULE  = 64 * 1024,
    NIDS_MAX     = 8           /* modules en cours les uns dans les autres */
};

/* ---- les rappels ---- */

static void rappel_send_card(wasm_exec_env_t env, uint64_t *args)
{
    (void)env;
    /* « $ » : WAMR a vérifié la chaîne et posé ici son adresse native. */
    const char *ligne = (const char *)(uintptr_t)args[0];
    hc_ligne_de_module(ligne);
}

static NativeSymbol RAPPELS[] = {
    { "SendCardMessage", (void *)rappel_send_card, "($)", NULL },
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

/* ---- la sentinelle ---- */

static pthread_mutex_t    g_garde_verrou = PTHREAD_MUTEX_INITIALIZER;
static wasm_module_inst_t g_actifs[NIDS_MAX];
static int                g_nactifs;
static double             g_debut;          /* l'appel le plus extérieur */
static double             g_delai;          /* 0 : aucun */
static int                g_arret_demande;
static int                g_arret_cause;    /* 1 : délai ; 2 : demandé */
static int                g_garde_lancee;

static double maintenant(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static void *garde(void *rien)
{
    (void)rien;
    for (;;) {
        struct timespec pause = { 0, 20 * 1000 * 1000 };
        nanosleep(&pause, NULL);
        pthread_mutex_lock(&g_garde_verrou);
        if (g_nactifs && !g_arret_cause) {
            if (g_arret_demande) g_arret_cause = 2;
            else if (g_delai > 0 && maintenant() - g_debut > g_delai)
                g_arret_cause = 1;
            if (g_arret_cause)
                for (int i = 0; i < g_nactifs; i++)
                    wasm_runtime_terminate(g_actifs[i]);
        }
        pthread_mutex_unlock(&g_garde_verrou);
    }
    return NULL;
}

void hc_module_delai(double secondes)
{
    pthread_mutex_lock(&g_garde_verrou);
    g_delai = secondes > 0 ? secondes : 0;
    pthread_mutex_unlock(&g_garde_verrou);
}

void hc_module_interromps(void)
{
    pthread_mutex_lock(&g_garde_verrou);
    if (g_nactifs) g_arret_demande = 1;
    pthread_mutex_unlock(&g_garde_verrou);
}

/* Rend 0, ou -1 s'il n'y a plus de place pour un appel de plus. */
static int garde_entre(wasm_module_inst_t inst)
{
    pthread_mutex_lock(&g_garde_verrou);
    if (g_nactifs >= NIDS_MAX) {
        pthread_mutex_unlock(&g_garde_verrou);
        return -1;
    }
    if (g_nactifs == 0) {
        g_debut = maintenant();
        g_arret_demande = 0;
        g_arret_cause = 0;
    }
    g_actifs[g_nactifs++] = inst;
    if (!g_garde_lancee) {
        pthread_t t;
        if (pthread_create(&t, NULL, garde, NULL) == 0) {
            pthread_detach(t);
            g_garde_lancee = 1;
        }
    }
    pthread_mutex_unlock(&g_garde_verrou);
    return 0;
}

/* Rend la cause d'un arrêt survenu pendant l'appel : 0, 1 ou 2. */
static int garde_sort(void)
{
    pthread_mutex_lock(&g_garde_verrou);
    int cause = g_arret_cause;
    if (g_nactifs > 0) g_nactifs--;
    if (g_nactifs == 0) { g_arret_demande = 0; g_arret_cause = 0; }
    pthread_mutex_unlock(&g_garde_verrou);
    return cause;
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

static void desinstancie(struct HcModule *mo)
{
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
        if (mo->en_cours) {
            snprintf(faute, nf, "le module « %s » est déjà en cours : il ne "
                     "peut pas s'appeler lui-même", mo->nom);
            return -1;
        }
        if (instancie(mo, faute, nf) < 0) return -1;

        /* LA BOÎTE : le bloc, puis les chaînes. */
        size_t taille = BLOC_TAILLE;
        for (int k = 0; k < argc; k++) taille += strlen(argv[k]) + 1;
        if (taille > (size_t)HC_MODULE_PAGES_MAX * 65536) {
            snprintf(faute, nf, "%s %s : des paramètres trop longs",
                     genre, nom);
            return -1;
        }
        wasm_function_inst_t fb = wasm_runtime_lookup_function(mo->inst,
                                                               "hc_boite");
        wasm_function_inst_t fx = wasm_runtime_lookup_function(mo->inst,
                                                               exact);
        if (!fb || !fx) {
            snprintf(faute, nf, "le module « %s » a perdu « %s »",
                     mo->nom, fb ? exact : "hc_boite");
            return -1;
        }
        if (garde_entre(mo->inst) < 0) {
            snprintf(faute, nf, "trop de modules appelés les uns dans les "
                     "autres (%d au plus)", NIDS_MAX);
            return -1;
        }
        mo->en_cours = 1;
        wasm_runtime_clear_exception(mo->inst);

        uint32_t cases[1] = { (uint32_t)taille };
        int ok = wasm_runtime_call_wasm(mo->env, fb, 1, cases);
        uint32_t b = cases[0];
        if (ok && (!b || !wasm_runtime_validate_app_addr(mo->inst, b,
                                                         taille))) {
            mo->en_cours = 0;
            garde_sort();
            snprintf(faute, nf, "le module « %s » n'a pas fait de place "
                     "pour ses paramètres", mo->nom);
            desinstancie(mo);
            return -1;
        }
        if (ok) {
            unsigned char *p = wasm_runtime_addr_app_to_native(mo->inst, b);
            memset(p, 0, BLOC_TAILLE);
            p[0] = (unsigned char)argc;          /* paramCount, un short */
            uint32_t o = b + BLOC_TAILLE;
            for (int k = 0; k < argc; k++) {
                size_t l = strlen(argv[k]) + 1;
                memcpy(p + (o - b), argv[k], l);
                pose32(p + BLOC_PARAMS + 4 * k, o);
                o += (uint32_t)l;
            }
            cases[0] = b;
            ok = wasm_runtime_call_wasm(mo->env, fx, 1, cases);
        }
        mo->en_cours = 0;
        int cause = garde_sort();

        if (!ok) {
            if (cause == 1)
                snprintf(faute, nf, "le module « %s » a dépassé son délai, "
                         "et il a été arrêté", mo->nom);
            else if (cause == 2)
                snprintf(faute, nf, "le module « %s » a été arrêté",
                         mo->nom);
            else
                snprintf(faute, nf, "le module « %s » s'est arrêté sur une "
                         "faute : %s", mo->nom, exception(mo->inst));
            desinstancie(mo);
            return -1;
        }

        /* Ce qu'il rend : returnValue, une chaîne de SA mémoire, à
         * vérifier comme tout ce qui vient de lui. */
        const unsigned char *p = wasm_runtime_addr_app_to_native(mo->inst, b);
        uint32_t rv = lis32(p + BLOC_RETOUR);
        *passe = p[BLOC_PASSE] != 0;
        if (rv && res && nres) {
            if (!wasm_runtime_validate_app_str_addr(mo->inst, rv)) {
                wasm_runtime_clear_exception(mo->inst);
                snprintf(faute, nf, "le module « %s » rend une valeur hors "
                         "de sa mémoire", mo->nom);
                return -1;
            }
            snprintf(res, nres, "%s",
                     (const char *)wasm_runtime_addr_app_to_native(mo->inst,
                                                                   rv));
        }
        return 1;
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
