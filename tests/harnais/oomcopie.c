/* oomcopie — COPIER, COLLER, DUPLIQUER QUAND LA MÉMOIRE MANQUE.
 *
 * Le presse-papiers s'est donné une règle, écrite en tête de clone_part :
 * « une copie qui échoue est un désagrément ; un arrêt en perd la pile ».
 * Ce harnais vérifie qu'elle est TENUE, et pas seulement écrite.
 *
 * Chaque opération est rejouée autant de fois qu'elle fait d'allocations :
 * la première fois on refuse la 1re, la fois suivante la 2e, et ainsi de
 * suite. Une opération transactionnelle n'a que deux issues :
 *
 *     ÉCHEC NET      elle le dit, et la pile comme le presse-papiers sont
 *                    EXACTEMENT ce qu'ils étaient ;
 *     RÉUSSITE NETTE elle le dit, et le résultat est EXACTEMENT celui d'une
 *                    exécution sans pénurie.
 *
 * Tout le reste est un défaut, nommé :
 *
 *     ARRÊT     le programme s'est arrêté (hc_memoire_epuisee)
 *     PARTIEL   l'opération dit avoir échoué, mais quelque chose a changé
 *     DÉGRADÉ   l'opération dit avoir réussi, mais le résultat diffère
 *     PLANTAGE  un signal
 *     SANITIZER une fuite ou une faute mémoire (seulement sous lance.sh
 *               --asan)
 *
 * « Exactement » se mesure par hc_save : les deux piles sont écrites dans
 * notre format, et le presse-papiers aussi — collé, sans pénurie, dans une
 * pile témoin écrite à son tour. Ce qui ne s'enregistre pas n'est pas
 * comparé.
 *
 * LA SONDE n'est pas dans le noyau : malloc, calloc et realloc sont
 * interceptés à l'édition de liens (--wrap, ligne « lance-lien » ci-dessous).
 * Chaque essai tourne dans un FILS, pour qu'un arrêt ou un plantage soit
 * relevé au lieu d'emporter le harnais. */
/* lance-lien : -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* SOUS ASan, LeakSanitizer sort avec le code d'ASan, 1 par défaut — le même
 * que hc_memoire_epuisee. Une fuite passait donc pour un ARRÊT, et le piège à
 * fuite semblait manqué. lance.sh ne fixe pas ce code : on le fixe ici, pour
 * ce harnais seul. Une faute mémoire sort par le même code. */
const char *__asan_default_options(void);
const char *__asan_default_options(void) { return "exitcode=23"; }

/* ---------------------------------------------------------- la sonde --- */

void *__real_malloc(size_t n);
void *__real_calloc(size_t n, size_t t);
void *__real_realloc(void *p, size_t n);

static int  g_arme  = 0;     /* compte-t-on ? */
static long g_vues  = 0;     /* allocations vues depuis l'armement */
static long g_refus = 0;     /* refuser celle-ci (1 = la première), 0 : aucune */

static int refuse(void)
{
    if (!g_arme) return 0;
    g_vues++;
    return g_vues == g_refus;
}
void *__wrap_malloc(size_t n)            { return refuse() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t t)  { return refuse() ? NULL : __real_calloc(n, t); }
void *__wrap_realloc(void *p, size_t n)  { return refuse() ? NULL : __real_realloc(p, n); }

/* ---------------------------------------------------------- les piles --- */

static void muet(HcLineKind k, int d, const char *t) { (void)k; (void)d; (void)t; }

static Object *A, *B, *une, *deux, *poly, *texte, *titre, *bgA, *accB;

static void construit(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = muet; hc_set_host(&h);

    A   = hc_new_stack("Source");
    bgA = hc_new_background(A, "Fond");
    une = hc_new_card(A, bgA, "Une");
    deux = hc_new_card(A, bgA, "Deux");
    hc_register_stack(A);

    titre = hc_new_field(bgA, "Titre");               /* non partagé */
    Object *cs = hc_new_button(bgA, "Case");
    poly  = hc_new_button(une, "Poly");
    texte = hc_new_field(une, "Texte");
    hc_set_current_card(une);

    /* Une icône DE LA PILE, en couleur : elle voyage avec le bouton. */
    struct StackIcon *ic = hc_icon_add(A, 2000, "Etoile");
    if (ic) { ic->bits[0] = 0xF0; hc_icon_couleur_cree(ic); }

    hc_do("set the style of button \"Poly\" to \"polygon\"");
    hc_do("set the points of button \"Poly\" to \"10,10,60,15,40,50\"");
    hc_do("set the backColor of button \"Poly\" to \"255,0,0\"");
    hc_do("set the icon of button \"Poly\" to 2000");
    hc_do("set the script of button \"Poly\" to \"on mouseUp\" & return & \"beep\" & return & \"end mouseUp\"");
    hc_do("set the style of bkgnd button \"Case\" to \"checkBox\"");
    hc_do("set the sharedHilite of bkgnd button \"Case\" to false");

    hc_set_field_text(texte, "un deux trois");
    hc_run_add_full(texte, 0, 2, HC_BOLD, 14, "Monaco");
    hc_run_add_full(texte, 3, 4, HC_ITALIC, 9, "Geneva");

    hc_set_field_text(titre, "titre de la une");      /* bgtext de la carte */
    hc_run_add_full(titre, 0, 5, HC_BOLD, 12, "Chicago");
    hc_set_hilite(cs, une, 1);                         /* bghilite de la carte */
    hc_set_paint(une, "SENQMQ==");

    B   = hc_new_stack("Accueil");
    Object *bgB = hc_new_background(B, "Autre");
    accB = hc_new_card(B, bgB, "Ici");
    hc_register_stack(B);
}

/* ------------------------------------------------------ les relevés --- */

static char *lit_fichier(const char *chemin)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    char *s = malloc((size_t)n + 1);
    if (s && fread(s, 1, (size_t)n, f) != (size_t)n) { free(s); s = NULL; }
    if (s) s[n] = '\0';
    fclose(f);
    return s;
}

static char g_tmp[64];

static char *photo_pile(Object *st)
{
    if (hc_save(st, g_tmp) != 0) return NULL;
    char *s = lit_fichier(g_tmp);
    unlink(g_tmp);
    return s;
}

/* Le presse-papiers, collé dans une pile témoin puis écrit. */
static char *photo_presse(void)
{
    Object *t = hc_new_stack("Temoin");
    Object *tb = hc_new_background(t, "TF");
    Object *tc = hc_new_card(t, tb, "TC");
    hc_register_stack(t);
    Object *cur = hc_current_card();
    hc_set_current_card(tc);
    if (hc_clipboard_has_card())      hc_paste_card(t);
    else if (hc_clipboard_has_part()) hc_paste_part(tc);
    char *s = photo_pile(t);
    hc_set_current_card(cur);
    hc_clipboard_stack_closing(t);
    hc_unregister_stack(t);
    hc_free(t);
    return s ? s : strdup("(rien)");
}

/* LES IDENTIFIANTS DE LA PILE TÉMOIN, RENUMÉROTÉS DANS L'ORDRE D'APPARITION.
 *
 * Ils viennent d'un compteur global : d'une photo à l'autre, ils avancent
 * même quand rien n'a changé. La première version de ce harnais l'ignorait,
 * et annonçait « PARTIEL » au premier refus de CHAQUE opération — un défaut
 * de l'instrument, pas du noyau, trouvé en comparant les deux photos ligne
 * à ligne. Seules les lignes « id N » et « backgroundid N » sont touchées,
 * et seulement dans cette section : les piles A et B gardent leurs numéros. */
static void renumerote(char *s)
{
    long vus[256]; int nvus = 0;
    for (char *l = s; l && *l; ) {
        char *fin = strchr(l, '\n');
        char *q = NULL;
        if (strncmp(l, "id ", 3) == 0) q = l + 3;
        else { char *b = strstr(l, "backgroundid "); if (b && (!fin || b < fin)) q = b + 13; }
        if (q) {
            long v = strtol(q, NULL, 10);
            int k = 0;
            while (k < nvus && vus[k] != v) k++;
            if (k == nvus && nvus < 256) vus[nvus++] = v;
            /* même largeur que possible : on écrit un numéro court, complété
             * d'espaces, pour ne pas déplacer le reste du texte */
            char *e = q; while (*e >= '0' && *e <= '9') e++;
            char nb[24]; int w = snprintf(nb, sizeof nb, "%d", k + 1);
            int place = (int)(e - q);
            if (w <= place) { memset(q, ' ', (size_t)place); memcpy(q, nb, (size_t)w); }
        }
        l = fin ? fin + 1 : NULL;
    }
}

/* Tout ce qui doit être identique, en un seul texte. */
static char *photo(void)
{
    char *a = photo_pile(A), *b = photo_pile(B), *c = photo_presse();
    renumerote(c);
    size_t n = strlen(a) + strlen(b) + strlen(c) + 16;
    char *s = malloc(n);
    snprintf(s, n, "%s\n==B==\n%s\n==P==\n%s", a, b, c);
    free(a); free(b); free(c);
    return s;
}

/* ------------------------------------------------------ les opérations --- */

typedef struct {
    const char *nom;
    void (*avant)(void);          /* sans pénurie */
    int  (*op)(void);             /* 1 réussite, 0 échec */
} Operation;

static void rien(void) {}
static void copie_poly(void) { hc_copy_part(poly); }
static void copie_une(void)  { hc_copy_card(une); }
static void copie_une_colle_b(void) { hc_copy_card(une); hc_paste_card(B); }

/* LES TÉMOINS DE L'INSTRUMENT. Une opération propre, qui doit sortir nette ;
 * et deux pièges, qu'il doit attraper. S'il manque l'un des trois, aucun
 * verdict sur le noyau n'a de sens. */
static int op_temoin_propre(void)
{
    char *p = malloc(64);
    if (!p) return 0;
    free(p);
    return 1;
}
static int op_piege_partiel(void)          /* échoue en laissant une trace */
{
    hc_set_field_text(texte, "abîmé");
    char *p = malloc(64);
    if (!p) return 0;
    free(p);
    hc_set_field_text(texte, "un deux trois");
    return 1;
}
static int op_piege_degrade(void)          /* réussit à moitié */
{
    char *p = malloc(64);
    if (p) { free(p); hc_set_field_text(texte, "un deux trois"); }
    else   hc_set_field_text(texte, "à moitié");
    return 1;
}

/* Le piège à FUITE : refuser la seconde allocation perd la première. Sous
 * AddressSanitizer, LeakSanitizer doit le dire (code 23) ; sans lui, rien ne
 * peut le voir. Vérifié en silence, pour que la sortie reste la même dans
 * les deux modes de lance.sh. */
/* LeakSanitizer balaie la pile de façon prudente : un pointeur resté dans un
 * emplacement mort y passe pour atteignable, et la fuite ne se voit pas. Le
 * premier piège tombait dans ce trou. Le bloc perdu n'est donc gardé que
 * MASQUÉ, dans une globale, par une fonction qui ne laisse rien derrière elle. */
static volatile unsigned long g_masque;
/* Par un pointeur VOLATILE : gcc supprimait le premier malloc à
 * l'optimisation — le désassemblage n'en montrait qu'un — et le piège ne
 * posait aucune fuite. */
static void *(*volatile g_alloue)(size_t) = malloc;
static __attribute__((noinline)) int deux_blocs(void)
{
    char *p = g_alloue(64);
    g_masque = (unsigned long)p ^ 0x5a5a5a5aUL;
    p = NULL;
    char *q = g_alloue(64);
    if (!q) return 0;                     /* le premier est perdu */
    free((char *)(g_masque ^ 0x5a5a5a5aUL));
    free(q);
    return 1;
}
static int op_piege_fuite(void) { return deux_blocs(); }

static int op_copie_bouton(void)  { return hc_copy_part(poly); }
static int op_copie_champ(void)   { return hc_copy_part(texte); }
static int op_copie_fond(void)    { return hc_copy_part(titre); }
static int op_colle_bouton(void)  { return hc_paste_part(deux) != NULL; }
static int op_copie_carte(void)   { return hc_copy_card(une); }
static int op_colle_ici(void)     { return hc_paste_card(A) != NULL; }
static int op_colle_ailleurs(void){ return hc_paste_card(B) != NULL; }
static int op_duplique(void)      { return hc_duplicate_card(une) != NULL; }

static const Operation OPS[] = {
    { "TÉMOIN : opération propre",              rien,              op_temoin_propre },
    { "PIÈGE : échec qui laisse une trace",     rien,              op_piege_partiel },
    { "PIÈGE : réussite à moitié",              rien,              op_piege_degrade },
#define OP_FUITE 3
    { "PIÈGE : fuite",                          rien,              op_piege_fuite },
    { "copier un bouton (polygone, icône)",     rien,              op_copie_bouton },
    { "copier un champ (plages de style)",      rien,              op_copie_champ },
    { "copier un champ de fond non partagé",    rien,              op_copie_fond },
    { "coller un bouton",                       copie_poly,        op_colle_bouton },
    { "copier une carte",                       rien,              op_copie_carte },
    { "coller la carte dans sa pile",           copie_une,         op_colle_ici },
    { "coller la carte ailleurs (fond recréé)", copie_une,         op_colle_ailleurs },
    { "coller ailleurs une 2e fois (fond porté)", copie_une_colle_b, op_colle_ailleurs },
    { "dupliquer une carte",                    rien,              op_duplique },
};
#define NOPS ((int)(sizeof OPS / sizeof *OPS))

/* ---------------------------------------------------------- un essai --- */

enum { NET_ECHEC = 10, NET_REUSSITE = 11, PARTIEL = 12, DEGRADE = 13, REF = 14 };

/* Le fils : construit, prépare, photographie, arme, opère, compare.
 * refus = 0 : l'essai de référence, qui écrit sa photo et son compte. */
static void essai(int o, long refus, const char *ref_photo, const char *ref_compte)
{
    construit();
    OPS[o].avant();
    char *avant = photo();

    g_vues = 0; g_refus = refus; g_arme = 1;
    int r = OPS[o].op();
    g_arme = 0;
    long vues = g_vues;

    char *apres = photo();

    if (refus == 0) {
        FILE *f = fopen(ref_photo, "w");  fputs(apres, f); fclose(f);
        f = fopen(ref_compte, "w");       fprintf(f, "%ld %d\n", vues, r); fclose(f);
        free(avant); free(apres);
        exit(REF);
    }

    int code;
    if (!r) code = strcmp(avant, apres) == 0 ? NET_ECHEC : PARTIEL;
    else {
        char *attendu = lit_fichier(ref_photo);
        code = attendu && strcmp(attendu, apres) == 0 ? NET_REUSSITE : DEGRADE;
        free(attendu);
    }
    free(avant); free(apres);
    exit(code);           /* exit et non _exit : LeakSanitizer passe ici */
}

static int lance(int o, long refus, const char *rp, const char *rc)
{
    fflush(stdout);
    pid_t p = fork();
    if (p == 0) {
        /* hc_memoire_epuisee écrit sur stderr avant de s'arrêter : l'arrêt
         * se lit au code de sortie, le texte ne ferait que du bruit. */
        if (!freopen("/dev/null", "w", stderr)) _exit(99);
        essai(o, refus, rp, rc);
    }
    int st = 0;
    waitpid(p, &st, 0);
    if (WIFSIGNALED(st)) return -WTERMSIG(st);
    return WEXITSTATUS(st);
}

int main(void)
{
    char rp[64], rc[64];
    snprintf(g_tmp, sizeof g_tmp, "/tmp/hc_oom_%d.stack", (int)getpid());
    snprintf(rp, sizeof rp, "/tmp/hc_oom_%d.ref", (int)getpid());
    snprintf(rc, sizeof rc, "/tmp/hc_oom_%d.cpt", (int)getpid());

    /* La bannière de l'exécuteur s'imprime au premier script : ici, une fois,
     * plutôt que dans chaque fils. */
    construit();
    hc_do("get 1");

    int defauts = 0, sourd = 0;
    for (int o = 0; o < NOPS; o++) {
        if (o == OP_FUITE) {
#if defined(__SANITIZE_ADDRESS__)
            /* Comme les autres : la référence d'abord, puis chaque refus. Au
             * moins l'un d'eux doit faire parler LeakSanitizer. */
            int vue = 0;
            long nf = 0; int rf = 0;
            if (lance(o, 0, rp, rc) == REF) {
                FILE *g = fopen(rc, "r");
                if (g && fscanf(g, "%ld %d", &nf, &rf) == 2) {
                    for (long k = 1; k <= nf; k++) if (lance(o, k, rp, rc) == 23) vue = 1;
                }
                if (g) fclose(g);
            }
            if (!vue) sourd = 1;
#endif
            continue;
        }
        int canari = o < 3;      /* le témoin et les deux pièges */
        if (lance(o, 0, rp, rc) != REF) { printf("%s : la référence a échoué\n", OPS[o].nom); defauts++; continue; }
        long n = 0; int r = 0;
        FILE *f = fopen(rc, "r");
        if (!f || fscanf(f, "%ld %d", &n, &r) != 2) { puts("compte illisible"); return 1; }
        fclose(f);

        int net_e = 0, net_r = 0;
        char detail[2048] = "";
        size_t dl = 0;
        for (long k = 1; k <= n; k++) {
            int c = lance(o, k, rp, rc);
            const char *mot = NULL;
            switch (c) {
            case NET_ECHEC:    net_e++; break;
            case NET_REUSSITE: net_r++; break;
            case PARTIEL:      mot = "PARTIEL";  break;
            case DEGRADE:      mot = "DÉGRADÉ";  break;
            case 1:            mot = "ARRÊT";    break;
            case 23:           mot = "SANITIZER (fuite ou faute mémoire)"; break;
            default:           mot = c < 0 ? "PLANTAGE" : "?"; break;
            }
            if (mot && dl < sizeof detail - 40)
                dl += (size_t)snprintf(detail + dl, sizeof detail - dl, "\n      refus n° %ld : %s", k, mot);
            if (mot && !canari) defauts++;
            if (mot && o == 0) sourd = 1;            /* le témoin doit être net */
        }
        if (o >= 1 && o < 3 && net_e + net_r == n) sourd = 1;   /* piège manqué */
        printf("%-42s %s, %2ld allocation(s) : %ld/%ld nettes (%d échecs, %d réussites)%s\n",
               OPS[o].nom, r ? "réussit" : "ÉCHOUE", n,
               (long)(net_e + net_r), n, net_e, net_r, detail);
    }
    unlink(rp); unlink(rc);
    if (sourd) { puts("\nL'INSTRUMENT EST SOURD : aucun verdict"); return 1; }
    printf("\ninstrument : témoin net, pièges attrapés\n%s\n",
           defauts ? "DES OPÉRATIONS NE SONT PAS TRANSACTIONNELLES"
                   : "toutes les opérations sont transactionnelles");
    return 0;
}
