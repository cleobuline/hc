/* torture3 — LES EXPRESSIONS TORDUES, MONTÉES PAR LE CODE.
 *
 * Une pile d'une carte « Atelier » (et trois autres pour les comptages),
 * un champ « Essais » qui porte une expression par ligne —
 *
 *     expression ==> attendu          (attendu « ? » : non mesuré ;
 *                                      « ERREUR » : doit lever)
 *
 * — et un bouton TORTURE qui donne le départ : la pile les passe toutes
 * par « do », dans « on idle », et écrit ok / ECHEC / ? dans le champ « R ».
 * Dans « idle » parce que dans HyperCard une erreur arrête TOUT le script,
 * boucle comprise — mesuré le 1er octobre ; chaque « idle » est un script
 * neuf. Les essais vivent
 * dans le CHAMP et non dans un script pour que l'utilisatrice puisse en
 * ajouter à la main, sans toucher à rien d'autre.
 *
 * TROIS USAGES POUR UN SEUL PROGRAMME, comme torture2 :
 *
 *   torture3 bouton.txt pile.txt essais.txt              le test
 *   torture3 bouton.txt pile.txt essais.txt sortie.stack écrit la pile
 *
 * LE TEST JOUE LE BOUTON DEUX FOIS, dans deux processus : avec l'ancien
 * moteur d'expressions, puis sans (HC_SANS_V1=1, lu une fois par processus).
 * Il imprime le rapport du premier, puis les lignes du second qui n'y
 * figurent pas. Une ligne « SANS V1 » est une réponse que la v3 ne donne pas
 * seule — voir docs/mesures/sansv1.txt. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("%s\n", t);
    else if (k == HC_ERR) printf("[ERR] %s\n", t);
}

static char *lire(const char *chemin)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) { perror(chemin); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *t = malloc((size_t)n + 1);
    if (!t) { fclose(f); return NULL; }
    if (fread(t, 1, (size_t)n, f) != (size_t)n) { free(t); fclose(f); return NULL; }
    t[n] = 0; fclose(f);
    return t;
}

static Object *pose_champ(Object *ou, const char *nom,
                          int x, int y, int w, int h, const char *style)
{
    Object *f = hc_new_field(ou, nom);
    if (!f) return NULL;
    f->x = x; f->y = y; f->w = w; f->h = h;
    if (style) { free(f->style); f->style = strdup(style); }
    return f;
}

static Object *pose_bouton(Object *ou, const char *nom,
                           int x, int y, int w, int h, const char *script)
{
    Object *b = hc_new_button(ou, nom);
    if (!b) return NULL;
    b->x = x; b->y = y; b->w = w; b->h = h;
    free(b->style); b->style = strdup("roundRect");
    hc_set_script(b, script);
    return b;
}

static char *src_bouton, *src_pile, *src_essais;
static Object *g_bouton, *g_rapport, *g_carte;

/* La pile, montée à l'identique pour le test et pour le fichier livré. */
static Object *monte(void)
{
    Object *st = hc_new_stack("Torture3");
    hc_register_stack(st);
    Object *un   = hc_new_background(st, "Un");
    Object *deux = hc_new_background(st, "Deux");

    /* LES FONDS SONT ENTRELACÉS — Un, Deux, Un, Deux —, pour que le rang
     * dans le fond et le rang dans la pile ne coïncident pour aucune carte
     * au-delà de la première. Voir cartedufond. */
    Object *c1 = hc_new_card(st, un, "Atelier");
    hc_new_card(st, deux, "Deux");
    hc_new_card(st, un, "Trois");
    hc_new_card(st, deux, "Quatre");

    /* L'ORDRE DE CRÉATION EST LE NUMÉRO : A est « card field 1 », et des
     * essais s'en servent. La carte fait 512 × 342 : deux colonnes de 230
     * (20 → 250, 262 → 492), les champs jusqu'à 290, les boutons de 300 à
     * 322. */
    Object *a = pose_champ(c1, "A", 20, 20, 230, 24, NULL);
    if (a) hc_set_field_text(a, "alpha beta gamma");
    Object *e = pose_champ(c1, "Essais", 20, 50, 230, 240, "scrolling");
    if (e) hc_set_field_text(e, src_essais);
    g_rapport = pose_champ(c1, "R", 262, 20, 230, 270, "scrolling");

    g_bouton = pose_bouton(c1, "TORTURE", 20, 300, 110, 22, src_bouton);
    pose_bouton(c1, "Effacer", 140, 300, 110, 22,
                "on mouseUp\n  razCompteurs\nend mouseUp\n");

    hc_set_script(st, src_pile);
    hc_set_current_card(c1);
    g_carte = c1;
    return st;
}

/* Joue le bouton dans ce processus et écrit le champ « R » sur `fd`. */
static void joue(int fd, int sans_v1)
{
    if (sans_v1) setenv("HC_SANS_V1", "1", 1);
    else         unsetenv("HC_SANS_V1");

    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    /* La boîte de message part sur la sortie d'erreur : seul le RAPPORT se
     * compare, et le bilan qu'on y « put » est déjà dans le champ. */
    if (dup2(STDERR_FILENO, STDOUT_FILENO) < 0) _exit(2);
    Object *st = monte();
    hc_send(g_bouton, "mouseUp");
    /* LES ESSAIS SE JOUENT DANS « on idle » (script de la pile) : le bouton
     * ne fait que donner le départ. On envoie donc « idle » comme le fait
     * l'application, jusqu'au bilan — et pas indéfiniment : une pile qui ne
     * conclurait jamais doit se voir, pas bloquer la suite. */
    for (int i = 0; i < 1000; i++) {
        const char *r = g_rapport && g_rapport->contents ? g_rapport->contents : "";
        if (strstr(r, "==============================")) break;
        hc_send(g_carte, "idle");
    }
    const char *r = (g_rapport && g_rapport->contents) ? g_rapport->contents : "";
    size_t n = strlen(r);
    if (write(fd, r, n) != (ssize_t)n) _exit(3);
    hc_free(st);
}

static char *capture(int sans_v1)
{
    int p[2];
    if (pipe(p) < 0) return NULL;
    fflush(stdout);
    pid_t f = fork();
    if (f < 0) return NULL;
    if (f == 0) { close(p[0]); joue(p[1], sans_v1); _exit(0); }
    close(p[1]);
    size_t n = 0, cap = 1 << 16;
    char *t = malloc(cap);
    if (!t) return NULL;
    for (;;) {
        if (n + 4096 > cap) {
            char *u = realloc(t, cap *= 2);
            if (!u) { free(t); return NULL; }
            t = u;
        }
        ssize_t r = read(p[0], t + n, cap - n - 1);
        if (r <= 0) break;
        n += (size_t)r;
    }
    t[n] = '\0';
    close(p[0]);
    int etat;
    waitpid(f, &etat, 0);
    return t;
}

/* Les lignes de `b` absentes de `a`, chacune précédée de `marque`. */
static int absentes(const char *a, const char *b, const char *marque)
{
    int n = 0;
    const char *l = b;
    while (*l) {
        const char *fin = strchr(l, '\n');
        size_t len = fin ? (size_t)(fin - l) : strlen(l);
        int vue = 0;
        for (const char *m = a; *m && !vue; ) {
            const char *fm = strchr(m, '\n');
            size_t lm = fm ? (size_t)(fm - m) : strlen(m);
            if (lm == len && memcmp(m, l, len) == 0) vue = 1;
            m += lm + (fm ? 1 : 0);
        }
        if (!vue && len) { printf("%s%.*s\n", marque, (int)len, l); n++; }
        l += len + (fin ? 1 : 0);
    }
    return n;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s bouton.txt pile.txt essais.txt [sortie.stack]\n",
                argv[0]);
        return 2;
    }
    src_bouton = lire(argv[1]);
    src_pile   = lire(argv[2]);
    src_essais = lire(argv[3]);
    if (!src_bouton || !src_pile || !src_essais) return 2;

    if (argc > 4) {
        /* Écrite AVANT tout clic : le fichier livré part d'un rapport vide. */
        static HcHost h;
        memset(&h, 0, sizeof h);
        h.line = ma_ligne;
        hc_set_host(&h);
        Object *st = monte();
        if (hc_save(st, argv[4]) != 0) {
            fprintf(stderr, "impossible d'écrire %s\n", argv[4]);
            return 1;
        }
        printf("pile écrite : %s\n", argv[4]);
        hc_free(st);
        return 0;
    }

    char *avec = capture(0);
    char *sans = capture(1);
    if (!avec || !sans) { puts("capture impossible"); return 1; }

    printf("───── champ « R », avec l'ancien moteur ─────\n%s\n", avec);
    printf("───── ce qui change sans lui ─────\n");
    int n = absentes(avec, sans, "  SANS V1 + ");
    n += absentes(sans, avec, "  SANS V1 - ");
    if (!n) printf("  rien : la v3 seule rend le même rapport, ligne pour ligne\n");

    free(avec); free(sans);
    free(src_bouton); free(src_pile); free(src_essais);
    return 0;
}
