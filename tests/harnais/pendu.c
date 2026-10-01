/* pendu — LE JEU DU PENDU, MONTÉ PAR LE CODE, pour le DMG.
 *
 * Une pile d'exemple, livrée avec l'application : une carte, la potence
 * dessinée en caractères, le mot à trouver, vingt-six boutons de lettres et
 * un bouton « Nouvelle partie ». On joue en cliquant ou au clavier ; sept
 * erreurs et le bonhomme est pendu. Tout le jeu vit dans le script de la
 * pile, tests/donnees/pendu_pile.txt.
 *
 * DEUX USAGES POUR UN SEUL PROGRAMME, comme torture2 et torture3 :
 *
 *   pendu pile.txt               le test : quelques parties jouées d'avance
 *   pendu pile.txt Pendu.stack   écrit la pile, pour le DMG
 *
 * La pile écrite n'est pas versionnée — ses sources le sont —, et c'est elle
 * qu'on copie à côté de HC.app au moment de construire le DMG
 * (docs/livraison.md).
 *
 * Le tirage au sort ne se teste pas : les parties jouées ici imposent leur
 * mot, par l'argument de « nouvellePartie ». */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("   [msg] %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
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

static Object *pose_champ(Object *ou, const char *nom, int x, int y, int w, int h,
                          const char *police, int taille, int style, int centre)
{
    Object *f = hc_new_field(ou, nom);
    if (!f) return NULL;
    f->x = x; f->y = y; f->w = w; f->h = h;
    f->locktext = 1;            /* on ne tape pas dedans : on joue */
    f->dont_wrap = 0;
    if (police) { free(f->textfont); f->textfont = strdup(police); }
    if (taille) f->textsize = taille;
    f->textstyle = style;
    f->text_align = centre;
    return f;
}

static Object *pose_bouton(Object *ou, const char *nom, int x, int y, int w, int h,
                           const char *script)
{
    Object *b = hc_new_button(ou, nom);
    if (!b) return NULL;
    b->x = x; b->y = y; b->w = w; b->h = h;
    free(b->style); b->style = strdup("roundRect");
    hc_set_script(b, script);
    return b;
}

static Object *g_carte;

/* La carte fait 512 × 342. Une colonne à gauche pour la potence (20 → 170),
 * une à droite pour le reste (185 → 492) ; les lettres en deux rangées de
 * treize, 13 × 33 + 12 × 3 = 465, posées de 23 à 488 ; le bouton du bas finit
 * à 308. Écrit ici parce que la géométrie est la seule chose que ce harnais
 * ne vérifie pas. */
static Object *monte(const char *script_pile)
{
    Object *st = hc_new_stack("Pendu");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Pendu");
    g_carte = c;

    Object *t = pose_champ(c, "Titre", 20, 8, 472, 32, NULL, 24, HC_BOLD, 1);
    if (t) hc_set_field_text(t, "LE PENDU");
    pose_champ(c, "Potence", 20, 46, 150, 150, "Monaco", 14, 0, 0);
    pose_champ(c, "Mot",     185, 56, 307, 44, "Monaco", 24, HC_BOLD, 1);
    pose_champ(c, "Compte",  185, 106, 307, 22, NULL, 12, 0, 1);
    pose_champ(c, "Lettres", 185, 132, 307, 22, "Monaco", 12, 0, 1);
    pose_champ(c, "Message", 185, 158, 307, 44, NULL, 12, HC_ITALIC, 1);

    for (int i = 0; i < 26; i++) {
        char nom[2] = { (char)('A' + i), 0 };
        int rangee = i / 13, col = i % 13;
        pose_bouton(c, nom, 23 + col * 36, 212 + rangee * 32, 33, 26,
                    "on mouseUp\n  joue the short name of me\nend mouseUp\n");
    }
    pose_bouton(c, "Nouvelle partie", 181, 282, 150, 26,
                "on mouseUp\n  nouvellePartie\nend mouseUp\n");

    hc_set_script(st, script_pile);
    hc_set_current_card(c);
    return st;
}

static const char *texte_de(const char *nom)
{
    for (int i = 0; i < g_carte->nparts; i++) {
        Object *f = g_carte->parts[i];
        if (f->type == OBJ_FIELD && f->name && !strcmp(f->name, nom))
            return f->contents ? f->contents : "";
    }
    return "(absent)";
}

static void etat(void)
{
    printf("%s\n", texte_de("Potence"));
    printf("   mot     : %s\n", texte_de("Mot"));
    printf("   %s\n", texte_de("Compte"));
    printf("   essayées: %s\n", texte_de("Lettres"));
    printf("   message : %s\n", texte_de("Message"));
    printf("   boutons cachés :");
    for (int i = 0; i < g_carte->nparts; i++) {
        Object *b = g_carte->parts[i];
        if (b->type == OBJ_BUTTON && !b->visible) printf(" %s", b->name);
    }
    printf("\n\n");
}

static Object *bouton(const char *nom)
{
    for (int i = 0; i < g_carte->nparts; i++) {
        Object *b = g_carte->parts[i];
        if (b->type == OBJ_BUTTON && b->name && !strcmp(b->name, nom)) return b;
    }
    return NULL;
}

static void clique(const char *lettres)
{
    for (const char *p = lettres; *p; p++) {
        char nom[2] = { *p, 0 };
        Object *b = bouton(nom);
        if (b) hc_send(b, "mouseUp");
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s pile.txt [Pendu.stack]\n", argv[0]);
        return 2;
    }
    char *script = lire(argv[1]);
    if (!script) return 2;

    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    Object *st = monte(script);

    if (argc > 2) {
        if (hc_save(st, argv[2]) != 0) {
            fprintf(stderr, "impossible d'écrire %s\n", argv[2]);
            return 1;
        }
        printf("pile écrite : %s\n", argv[2]);
        hc_free(st); free(script);
        return 0;
    }

    puts("== 1. à l'ouverture, une partie se tire au sort ==");
    hc_send(g_carte, "openCard");
    printf("   un mot est tiré : %s\n\n",
           strchr(texte_de("Mot"), '_') ? "oui" : "NON");

    puts("== 2. une partie gagnée : HIBOU, avec une erreur (Z) ==");
    hc_send_arg(g_carte, "nouvellePartie", "HIBOU");
    etat();
    clique("HZIB");
    etat();
    clique("OU");
    etat();
    puts("   -- une fois gagnée, plus rien ne joue :");
    clique("A");
    etat();

    puts("== 3. une lettre déjà essayée ne compte pas deux fois ==");
    hc_send_arg(g_carte, "nouvellePartie", "FUSEE");
    clique("A");
    hc_send_arg(g_carte, "joue", "A");
    etat();

    puts("== 4. au clavier, en minuscule ==");
    hc_send_arg(g_carte, "keyDown", "e");
    etat();

    puts("== 5. une partie perdue : FUSEE, sept erreurs, le mot dévoilé ==");
    clique("BCDGHIJ");
    etat();

    puts("== 6. « Nouvelle partie » remet tout à zéro ==");
    Object *np = bouton("Nouvelle partie");
    if (np) hc_send(np, "mouseUp");
    printf("   erreurs remises à zéro : %s\n",
           strstr(texte_de("Compte"), "0 sur 7") ? "oui" : "NON");
    printf("   lettres effacées       : %s\n",
           texte_de("Lettres")[0] ? "NON" : "oui");
    int caches = 0;
    for (int i = 0; i < g_carte->nparts; i++) {
        Object *b = g_carte->parts[i];
        if (b->type == OBJ_BUTTON && !b->visible) caches++;
    }
    printf("   boutons tous revenus   : %s\n", caches ? "NON" : "oui");

    hc_free(st); free(script);
    return 0;
}
