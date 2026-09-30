/* Une pénurie de mémoire au milieu d'une opération laisse l'objet TEL QU'IL
 * ÉTAIT — ou le dit.
 *
 * Signalé par un audit extérieur, confirmé en lisant, et mesuré ici :
 *
 *   hc_icon_colle_rvba    créait la couleur SUR l'icône — ce qui vide sa
 *                         silhouette — puis allouait son tableau de travail.
 *                         Qu'il manque, et la fonction rendait 0 sur une icône
 *                         noir et blanc dont le dessin venait d'être effacé.
 *   hc_icon_copie_dessin  recopiait les bits de la destination AVANT de savoir
 *                         si la couleur tiendrait : « échec » rendu sur une
 *                         icône qui avait déjà changé.
 *   hc_importe_pile       perdait un dessin lu en silence : pose_le_dessin
 *                         ne rendait rien, et la pile s'importait sans lui.
 *   put … after v         (hct_exec.c) un malloc manqué ne faisait RIEN, sans
 *                         un mot : v restait tel quel, le script continuait.
 *                         Trouvé en corrigeant l'arithmétique de la même
 *                         fonction, signalée par le même audit.
 *   feuille (hct_eval.c)  la copie manquée d'un NOM de variable rendait le
 *                         vide : « put v into r » rangeait "" dans r, sans un
 *                         mot. Trouvé PAR CE HARNAIS, une fois le cas
 *                         précédent corrigé : il restait un essai abîmé.
 *
 * COMMENT ON LE MESURE, comme penurie.c : ce harnais définit malloc, calloc
 * et realloc, qui l'emportent sur ceux de la bibliothèque C et sur les
 * intercepteurs d'ASan. Le n-ième appel, compté depuis l'armement, échoue ; on
 * balaie n jusqu'à ce que l'opération passe sans qu'aucun échec n'ait été
 * provoqué. Chaque essai se fait dans un FILS, qui ne parle que par son code de
 * sortie.
 *
 * Ce qui s'affiche est la seule chose stable d'une bibliothèque C à l'autre :
 * le nombre d'essais où l'objet a été abîmé par un échec. Il doit valoir zéro.
 * Vérifié en retirant les corrections : il ne vaut pas zéro. */
#define _GNU_SOURCE
#include "hc_core.h"
#include "hc_origine.h"
#include "hc_importe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>

/* ------------------------------------------------ l'allocateur piégé */

static int seuil = -1, compte, provoque;
static void *(*vrai_malloc)(size_t);
static void *(*vrai_calloc)(size_t, size_t);
static void *(*vrai_realloc)(void *, size_t);

/* LES VRAIS SYMBOLES SE CHERCHENT AU PREMIER APPEL, pas dans main : sous
 * ASan, des centaines d'allocations ont lieu avant main, et elles doivent
 * aller au vrai allocateur. Pendant la recherche elle-même, dlsym peut
 * allouer — ces appels-là, et eux seuls, sont servis par un petit tampon
 * statique, qu'on ne rend jamais. */
static unsigned char amorce[65536];
static size_t amorce_pos;
static int cherche;

static void *depuis_amorce(size_t n)
{
    n = (n + 15) & ~(size_t)15;
    if (amorce_pos + n > sizeof amorce) return NULL;
    void *p = amorce + amorce_pos;
    amorce_pos += n;
    memset(p, 0, n);
    return p;
}

static void resous(void)
{
    if (vrai_malloc || cherche) return;
    cherche = 1;
    vrai_malloc  = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
    vrai_calloc  = (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
    vrai_realloc = (void *(*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");
    cherche = 0;
}

static int panne(void)
{
    if (seuil < 0) return 0;
    if (compte++ == seuil) { provoque = 1; return 1; }
    return 0;
}

void *malloc(size_t n)
{
    resous();
    if (!vrai_malloc) return depuis_amorce(n);
    if (panne()) return NULL;
    return vrai_malloc(n);
}

void *calloc(size_t a, size_t b)
{
    resous();
    if (!vrai_calloc) return depuis_amorce(a * b);
    if (panne()) return NULL;
    return vrai_calloc(a, b);
}

void *realloc(void *p, size_t n)
{
    resous();
    if (!vrai_realloc) return NULL;
    if (panne()) return NULL;
    return vrai_realloc(p, n);
}

static void arme(int n) { seuil = n; compte = 0; provoque = 0; }
static void desarme(void) { seuil = -1; }

/* ------------------------------------------------------- les objets */

/* Ce que le script dit : une erreur annoncée, et ce qu'il écrit dans la
 * boîte de message. */
static int  g_erreur;
static char g_msg[256];

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_ERR) g_erreur = 1;
    else if (k == HC_MSG) snprintf(g_msg, sizeof g_msg, "%s", t);
}

static Object *pile_neuve(void)
{
    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    hc_new_card(st, bg, "U");
    return st;
}

/* Une icône noir et blanc avec un motif reconnaissable. */
static struct StackIcon *icone_nb(Object *st, int id)
{
    struct StackIcon *ic = hc_icon_add(st, id, "nb");
    for (int i = 0; i < HC_ICON_BYTES; i++) ic->bits[i] = (unsigned char)(i * 37 + 5);
    return ic;
}

/* Une image de plus de 255 couleurs : la découpe médiane est obligée de
 * tourner, et avec elle toutes les allocations de la fonction. */
static void image_riche(unsigned char *rvba)
{
    for (int i = 0; i < HC_ICON_PIXELS; i++) {
        rvba[i * 4]     = (unsigned char)(i * 7);
        rvba[i * 4 + 1] = (unsigned char)(i * 13);
        rvba[i * 4 + 2] = (unsigned char)(i / 4);
        rvba[i * 4 + 3] = (unsigned char)((i % 5) ? 255 : 0);
    }
}

/* Codes de sortie d'un fils. LOIN DE 1, et ce n'est pas un détail :
 * hc_memoire_epuisee sort par exit(1), et la première version de ce harnais
 * avait donné le code 1 à « abîmé ». Un arrêt ANNONCÉ passait donc pour un
 * objet abîmé en silence — l'instrument inventait un défaut dans l'import. */
enum { INTACT = 40, ABIME = 41, REUSSI = 42, FINI = 43 };

/* REUSSI et FINI NE SONT PAS LA MÊME CHOSE, et les confondre a été la seconde
 * faute de cet instrument. REUSSI : une panne a été provoquée, et l'opération
 * a abouti quand même — l'allocation manquée ne comptait pas, on continue.
 * FINI : aucune panne n'a été provoquée, l'opération tient désormais dans le
 * seuil, le balayage est terminé. S'arrêter au premier REUSSI faisait finir le
 * balayage de l'import AU PREMIER ESSAI, sans avoir touché au dessin. */

/* --------------------------------------------------------- les cas */

static int cas_colle_nb(int n)
{
    Object *st = pile_neuve();
    struct StackIcon *ic = icone_nb(st, 300);
    unsigned char avant[HC_ICON_BYTES];
    memcpy(avant, ic->bits, sizeof avant);
    static unsigned char rvba[HC_ICON_PIXELS * 4];
    image_riche(rvba);

    arme(n);
    int r = hc_icon_colle_rvba(ic, rvba);
    desarme();
    if (!provoque) return FINI;
    if (r) return REUSSI;           /* l'échec n'a pas touché ce qui compte */
    return (!ic->couleur && memcmp(avant, ic->bits, sizeof avant) == 0) ? INTACT : ABIME;
}

static int cas_colle_couleur(int n)
{
    Object *st = pile_neuve();
    struct StackIcon *ic = icone_nb(st, 301);
    hc_icon_couleur_depuis_bits(ic, 200, 10, 10);
    struct HcIconCouleur avant = *ic->couleur;
    unsigned char bits[HC_ICON_BYTES];
    memcpy(bits, ic->bits, sizeof bits);
    static unsigned char rvba[HC_ICON_PIXELS * 4];
    image_riche(rvba);

    arme(n);
    int r = hc_icon_colle_rvba(ic, rvba);
    desarme();
    if (!provoque) return FINI;
    if (r) return REUSSI;
    return (ic->couleur && memcmp(&avant, ic->couleur, sizeof avant) == 0 &&
            memcmp(bits, ic->bits, sizeof bits) == 0) ? INTACT : ABIME;
}

static int cas_copie(int n)
{
    Object *st = pile_neuve();
    struct StackIcon *dst = icone_nb(st, 302);
    struct StackIcon *src = hc_icon_add(st, 303, "src");
    memset(src->bits, 0xAA, HC_ICON_BYTES);
    hc_icon_couleur_depuis_bits(src, 1, 2, 3);
    unsigned char avant[HC_ICON_BYTES];
    memcpy(avant, dst->bits, sizeof avant);

    arme(n);
    int r = hc_icon_copie_dessin(dst, src);
    desarme();
    if (!provoque) return FINI;
    if (r) return REUSSI;
    return (!dst->couleur && memcmp(avant, dst->bits, sizeof avant) == 0) ? INTACT : ABIME;
}

/* Une pile d'origine fabriquée en mémoire : un fond, une carte, un dessin
 * chacun. hc_origine n'intervient pas — c'est le BÂTISSEUR qu'on éprouve. */
static unsigned char plan_image[8 * 16], plan_masque[8 * 16];

static void couche(HcOrigCouche *k, int id, int fond)
{
    memset(k, 0, sizeof *k);
    k->id = id;
    k->fond = fond;
    k->nom = (char *)(fond ? "carte" : "fond");
    k->montre_le_dessin = 1;
    k->dessin.present = 1;
    k->dessin.largeur = 64;
    k->dessin.hauteur = 16;
    k->dessin.octets_par_ligne = 8;
    k->dessin.image  = plan_image;
    k->dessin.masque = plan_masque;
}

static int cas_import(int n)
{
    for (int i = 0; i < (int)sizeof plan_image; i++) {
        plan_image[i]  = (unsigned char)(i * 3);
        plan_masque[i] = 0xFF;
    }
    static HcOrigCouche fond, carte;
    couche(&fond, 2, 0);
    couche(&carte, 3, 2);
    static HcOrigPile p;
    memset(&p, 0, sizeof p);
    p.largeur = 64; p.hauteur = 16;
    p.fonds  = &fond;  p.nfonds_lus   = 1;
    p.cartes = &carte; p.ncartes_lues = 1;

    arme(n);
    Object *st = hc_importe_pile(&p, "imp");
    desarme();
    if (!st) return provoque ? INTACT : ABIME;   /* NULL sans pénurie : faux */

    /* Une pile rendue doit porter ses DEUX dessins. */
    int dessins = 0;
    for (int i = 0; i < st->nparts; i++)
        if (hc_paint_of(st->parts[i])) dessins++;
    if (dessins != 2) return ABIME;
    return provoque ? REUSSI : FINI;
}

/* « put … after » sur une variable. Le script est analysé une première fois
 * HORS de la panne : c'est l'ajout qu'on éprouve, pas l'analyseur. */
static int cas_put_after(int n)
{
    Object *st = pile_neuve();
    Object *c = NULL;
    for (int i = 0; i < st->nparts; i++)
        if (st->parts[i]->type == OBJ_CARD) c = st->parts[i];
    Object *b = hc_new_button(c, "B");
    hc_set_current_card(c);
    hc_set_script(b,
        "on t\n  global r\n  put \"abc\" into v\n  put \"xyz\" after v\n"
        "  put v into r\nend t\n"
        "on lis\n  global r\n  put \"[\" & r & \"]\"\nend lis\n");
    hc_send(b, "t");                       /* à blanc : l'arbre se construit */
    hc_send(b, "lis");
    if (strcmp(g_msg, "[abcxyz]") != 0) return ABIME;

    hc_set_script(b,
        "on t\n  global r\n  put \"abc\" into v\n  put \"xyz\" after v\n"
        "  put v into r\nend t\n"
        "on lis\n  global r\n  put \"[\" & r & \"]\"\nend lis\n");
    hc_send(b, "lis");                     /* l'arbre, encore, hors panne */
    hc_send(b, "t");
    hc_send(b, "lis");
    g_erreur = 0;

    arme(n);
    hc_send(b, "t");
    desarme();
    int dit = g_erreur;
    hc_send(b, "lis");
    if (!provoque) return FINI;
    if (dit) return INTACT;                /* l'échec s'est dit */
    return strcmp(g_msg, "[abcxyz]") == 0 ? REUSSI : ABIME;
}

/* ---------------------------------- les données de l'utilisatrice
 *
 * Le second audit extérieur (30 septembre) : l'écriture d'un texte de champ,
 * le passage en texte partagé, le tri des cartes, l'ouverture d'un fichier et
 * le nom d'un article de menu détruisaient l'ancien état AVANT de savoir si
 * le nouveau tiendrait — ou annonçaient une réussite qui n'avait pas eu lieu.
 * Chaque cas dit : le nouvel état complet (REUSSI), l'ancien intact ou un
 * échec DIT (INTACT) ; tout le reste est ABIME. */

static Object *carte_de(Object *st)
{
    for (int i = 0; i < st->nparts; i++)
        if (st->parts[i]->type == OBJ_CARD) return st->parts[i];
    return NULL;
}

/* Un champ qui porte « ancien texte », son premier mot en gras. */
static Object *champ_style(Object *couche, Object *c, Object *b)
{
    Object *f = hc_new_field(couche, "F");
    hc_set_current_card(c);
    hc_set_script(b,
        "on s\n  put \"ancien texte\" into field \"F\"\n"
        "  set the textStyle of word 1 of field \"F\" to bold\nend s\n");
    hc_send(b, "s");
    return f;
}

static struct RunList *plages_de(Object *f, Object *c)
{
    if (f->owner && f->owner->type == OBJ_BACKGROUND && !f->shared_text)
        for (int i = 0; i < c->nbgtexts; i++)
            if (c->bgtexts[i].field_id == f->id) return &c->bgtexts[i].runs;
    return &f->runs;
}

static int verdict_texte(Object *f, Object *c)
{
    const char *t = hc_field_text(f);
    if (!t) return ABIME;
    if (strcmp(t, "nouveau") == 0) return REUSSI;
    if (strcmp(t, "ancien texte") == 0)
        return plages_de(f, c)->n > 0 ? INTACT : ABIME;   /* style perdu */
    return ABIME;
}

static int cas_texte_carte(int n)
{
    Object *st = pile_neuve(); Object *c = carte_de(st);
    Object *b = hc_new_button(c, "B");
    Object *f = champ_style(c, c, b);
    if (strcmp(hc_field_text(f), "ancien texte") != 0 || f->runs.n == 0) return ABIME;
    arme(n);
    hc_set_field_text(f, "nouveau");
    desarme();
    if (!provoque) return FINI;
    return verdict_texte(f, c);
}

static int cas_texte_fond(int n)
{
    Object *st = pile_neuve(); Object *c = carte_de(st);
    Object *b = hc_new_button(c, "B");
    Object *f = champ_style(c->bg, c, b);
    if (strcmp(hc_field_text(f), "ancien texte") != 0) return ABIME;
    arme(n);
    hc_set_field_text(f, "nouveau");
    desarme();
    if (!provoque) return FINI;
    return verdict_texte(f, c);
}

/* Passer en texte partagé : le texte ET le style de la carte deviennent
 * ceux du champ, ou rien ne change. */
static int cas_partage(int n)
{
    Object *st = pile_neuve(); Object *c = carte_de(st);
    Object *b = hc_new_button(c, "B");
    Object *f = champ_style(c->bg, c, b);
    arme(n);
    hc_set_shared_text(f, 1);
    desarme();
    if (!provoque) return FINI;
    const char *t = hc_field_text(f);
    struct RunList *r = plages_de(f, c);
    if (!t || strcmp(t, "ancien texte") != 0 || r->n == 0) return ABIME;
    return f->shared_text ? REUSSI : INTACT;
}

/* « sort cards » : l'ordre neuf, ou l'ancien et une erreur DITE. */
static int cas_tri(int n)
{
    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "c");
    hc_new_card(st, bg, "a");
    hc_new_card(st, bg, "b");
    hc_set_current_card(c);
    Object *btn = hc_new_button(c, "B");
    hc_set_script(btn, "on t\n  sort cards by the short name of this card\nend t\n");
    hc_send(btn, "t");                     /* à blanc : l'arbre se construit */
    hc_set_script(btn, "on t\n  sort cards descending by the short name of this card\nend t\n"
                       "on u\n  sort cards by the short name of this card\nend u\n");
    hc_send(btn, "t");                     /* c, b, a */
    hc_send(btn, "u");                     /* a, b, c : l'arbre de u existe */
    hc_send(btn, "t");                     /* c, b, a : on part de là */
    g_erreur = 0;
    arme(n);
    hc_send(btn, "u");
    desarme();
    if (!provoque) return FINI;
    char ordre[16] = ""; int k = 0;
    for (int i = 0; i < st->nparts && k < 8; i++)
        if (st->parts[i]->type == OBJ_CARD) ordre[k++] = st->parts[i]->name[0];
    ordre[k] = 0;
    if (strcmp(ordre, "abc") == 0) return REUSSI;
    if (strcmp(ordre, "cba") == 0 && g_erreur) return INTACT;
    return ABIME;
}

/* « open file » : ouvert pour de bon, ou refusé en le disant. */
#define FICHIER_ESSAI "/tmp/hc_penurie_fichier.txt"
static int cas_fichier(int n)
{
    remove(FICHIER_ESSAI);
    Object *st = pile_neuve(); Object *c = carte_de(st);
    Object *b = hc_new_button(c, "B");
    hc_set_current_card(c);
    hc_set_script(b,
        "on o\n  open file \"" FICHIER_ESSAI "\"\n  put the result into r\n"
        "  global g\n  put r into g\nend o\n"
        "on w\n  write \"ok\" to file \"" FICHIER_ESSAI "\"\n"
        "  close file \"" FICHIER_ESSAI "\"\nend w\n"
        "on g\n  global g\n  put \"[\" & g & \"]\"\nend g\n");
    hc_send(b, "g");  hc_send(b, "w");     /* à blanc */
    remove(FICHIER_ESSAI);
    g_erreur = 0;
    arme(n);
    hc_send(b, "o");
    desarme();
    int dit = g_erreur;
    hc_send(b, "g");
    int refuse = dit || strcmp(g_msg, "[]") != 0;
    hc_send(b, "w");
    if (!provoque) return FINI;
    FILE *fp = fopen(FICHIER_ESSAI, "r");
    char lu[8] = "";
    if (fp) { if (!fgets(lu, sizeof lu, fp)) lu[0] = 0; fclose(fp); }
    remove(FICHIER_ESSAI);
    if (strncmp(lu, "ok", 2) == 0) return REUSSI;
    return refuse ? INTACT : ABIME;
}

/* Le nom d'un article de menu : changé, ou l'échec dit. */
static int cas_menu(int n)
{
    Object *st = pile_neuve(); Object *c = carte_de(st);
    Object *b = hc_new_button(c, "B");
    hc_set_current_card(c);
    hc_set_script(b,
        "on m\n  if there is not a menu \"Essai\" then create menu \"Essai\"\n"
        "  put \"premier,second\" into menu \"Essai\"\nend m\n"
        "on s\n  set the name of menuItem 1 of menu \"Essai\" to \"nouveau\"\nend s\n"
        "on l\n  put the name of menuItem 1 of menu \"Essai\"\nend l\n");
    hc_send(b, "m");  hc_send(b, "l");  hc_send(b, "s");   /* à blanc */
    hc_send(b, "m");
    g_erreur = 0;
    arme(n);
    hc_send(b, "s");
    desarme();
    int dit = g_erreur;
    hc_send(b, "l");
    if (!provoque) return FINI;
    if (strcmp(g_msg, "nouveau") == 0) return REUSSI;
    if (strcmp(g_msg, "premier") == 0 && dit) return INTACT;
    return ABIME;
}

/* --------------------------------------------------------- le balayage */

static void balaie(const char *titre, int (*cas)(int))
{
    int essais = 0, abimes = 0;
    for (int n = 0; n < 5000; n++) {
        fflush(stdout);
        pid_t pid = fork();
        if (pid == 0) {
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
            _exit(cas(n));
        }
        int st = 0;
        waitpid(pid, &st, 0);
        /* Un exit(1) de hc_memoire_epuisee est un arrêt DIT, pas un objet
         * abîmé en silence : il ne compte pas. Un signal, si. */
        if (WIFSIGNALED(st)) { abimes++; essais++; continue; }
        int code = WEXITSTATUS(st);
        if (code == FINI) break;
        essais++;
        if (code == ABIME) abimes++;
        else if (code != INTACT && code != REUSSI && code != 1) abimes++;
    }
    printf("%-44s %s, %d abîmé(s)\n", titre,
           essais > 0 ? "des pénuries provoquées" : "AUCUNE PÉNURIE PROVOQUÉE",
           abimes);
}

int main(void)
{
    resous();
    if (!vrai_malloc || !vrai_calloc || !vrai_realloc) {
        puts("allocateur introuvable");
        return 1;
    }
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    balaie("coller une image sur une icône noir et blanc", cas_colle_nb);
    balaie("coller une image sur une icône en couleur", cas_colle_couleur);
    balaie("copier une icône en couleur sur une autre", cas_copie);
    balaie("importer une pile qui porte des dessins", cas_import);
    balaie("put … after une variable", cas_put_after);
    balaie("le texte d'un champ de carte", cas_texte_carte);
    balaie("le texte d'un champ de fond, par carte", cas_texte_fond);
    balaie("passer un champ en texte partagé", cas_partage);
    balaie("sort cards", cas_tri);
    balaie("open file", cas_fichier);
    balaie("le nom d'un article de menu", cas_menu);
    return 0;
}
