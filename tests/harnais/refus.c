/* Ce que le lecteur de piles doit REFUSER, et POURQUOI il refuse.
 *
 * Trois points, de la meme famille : un fichier n'est pas un peu lisible.
 *
 * 1. LES PERMISSIONS DU DOCUMENT survivent a l'enregistrement.
 *    mkstemp cree le temporaire en 0600 — c'est bien ce qu'on veut d'un
 *    temporaire —, mais rename lui donne ensuite le nom du document. Une pile
 *    en 0644, partagee par un groupe, repassait en 0600 a chaque
 *    enregistrement. Mesure : -rw-r--r-- devenait -rw-------.
 *
 * 2. UN FORMAT PLUS RECENT SE REFUSE. Tout numero positif etait accepte, et
 *    tout ce qui valait deux ou plus empruntait les regles de la version 2.
 *    Le jour ou une version 3 existe, un binaire d'aujourd'hui l'ouvrirait
 *    comme s'il la connaissait — et l'enregistrement suivant ecraserait
 *    l'original avec ce qu'il en aura compris. C'est la seule incompatibilite
 *    vraiment couteuse : celle qui ne se voit pas.
 *
 * 3. UNE ICONE ABIMEE REFUSE LE FICHIER. Les octets manquants restaient a
 *    zero et l'icone revenait a moitie, en silence. Le lecteur refuse une pile
 *    dont un TEXTE a ete ampute ; il n'y a aucune raison d'accepter une image
 *    qui l'est.
 *
 * Et dans les trois cas de refus, hc_load_erreur dit lequel : « format trop
 * recent » et « fichier abime » appellent des gestes tres differents de la
 * part de l'utilisateur, et l'interface n'avait que « Pile illisible » a en
 * dire. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define FIC "/tmp/hc_refus.stack"

static void ligne(HcLineKind k, int d, const char *t) { (void)k; (void)d; (void)t; }

static void mode_de(const char *quand)
{
    struct stat sb;
    if (stat(FIC, &sb) != 0) { printf("   %-26s (absent)\n", quand); return; }
    printf("   %-26s %04o\n", quand, (unsigned)(sb.st_mode & 07777));
}

/* Un fichier ecrit a la main, puis relu : le verdict et sa raison. */
static void essaie(const char *quoi, const char *contenu)
{
    FILE *g = fopen(FIC, "w");
    if (g) { fputs(contenu, g); fclose(g); }
    Object *st = hc_load(FIC);
    const char *pourquoi = hc_load_erreur();
    printf("   %-24s %s", quoi, st ? "acceptee" : "REFUSEE");
    if (pourquoi) printf("  — %s", pourquoi);
    printf("\n");
    if (st) hc_free(st);
}

/* Une pile minimale, avec un bloc d'icone de `octets` octets sur 128. */
static void icone(const char *quoi, int octets, const char *queue)
{
    FILE *g = fopen(FIC, "w");
    if (g) {
        fputs("-- pile HyperCard (format maison)\nformat 2\n\n"
              "stack \"P\"\nsize 512,342\n", g);
        fputs("iconres 7 \"i\"\n| ", g);
        for (int i = 0; i < octets; i++) fputs("A5", g);
        fputs(queue, g);
        fputs("\nend iconres\nend stack\n\n"
              "background \"F\"\nid 2\nend background\n\n"
              "card \"A\" background \"F\"\nid 3\nend card\n\nend hc-file\n", g);
        fclose(g);
    }
    Object *st = hc_load(FIC);
    const char *pourquoi = hc_load_erreur();
    printf("   %-24s %s", quoi, st ? "acceptee" : "REFUSEE");
    if (pourquoi) printf("  — %s", pourquoi);
    printf("\n");
    if (st) hc_free(st);
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);
    /* Le umask est FIXE ici : la premiere sauvegarde prend 0666 filtre par
     * lui, et un releve qui en dependrait changerait d'une machine a l'autre. */
    umask(022);

    puts("=== les permissions du document survivent a l'enregistrement ===");
    {
        Object *st = hc_new_stack("P");
        Object *bg = hc_new_background(st, "F");
        hc_set_current_card(hc_new_card(st, bg, "A"));
        remove(FIC);
        hc_save(st, FIC);
        mode_de("premiere sauvegarde");   /* 0666 & ~umask, comme tout programme */
        const int modes[] = { 0644, 0664, 0600, 0640 };
        for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++) {
            char q[48];
            chmod(FIC, (mode_t)modes[i]);
            hc_save(st, FIC);
            snprintf(q, sizeof q, "apres chmod %04o", (unsigned)modes[i]);
            mode_de(q);
        }
        hc_free(st);
    }

    puts("\n=== un format plus recent se refuse, il ne s'improvise pas ===");
    {
        const char *modele =
            "-- pile HyperCard (format maison)\nformat %d\n\n"
            "stack \"P\"\nsize 512,342\nend stack\n\n"
            "background \"F\"\nid 2\nend background\n\n"
            "card \"A\" background \"F\"\nid 3\nend card\n\nend hc-file\n";
        for (int v = 1; v <= 4; v++) {
            char contenu[512], quoi[32];
            snprintf(contenu, sizeof contenu, modele, v);
            snprintf(quoi, sizeof quoi, "format %d", v);
            /* Le format 1 n'a pas de signature de fin : on la retire. */
            if (v == 1) {
                char *p = strstr(contenu, "\nend hc-file\n");
                if (p) *p = '\0';
            }
            essaie(quoi, contenu);
        }
    }

    puts("\n=== une icone incomplete ou abimee refuse le fichier ===");
    icone("128 octets, entiere",   128, "");
    icone("127 octets",            127, "");
    icone("0 octet",                 0, "");
    icone("129 octets",            129, "");
    icone("127 + un chiffre seul", 127, "A");
    icone("127 + du non-hexa",     127, "ZZ");

    puts("\n=== une pile saine reste acceptee, et sans raison d'erreur ===");
    essaie("pile minimale",
           "-- pile HyperCard (format maison)\nformat 2\n\n"
           "stack \"P\"\nsize 512,342\nend stack\n\n"
           "background \"F\"\nid 2\nend background\n\n"
           "card \"A\" background \"F\"\nid 3\nend card\n\nend hc-file\n");
    essaie("fichier sans pile", "-- rien du tout\n");
    essaie("fichier tronque",
           "-- pile HyperCard (format maison)\nformat 2\n\n"
           "stack \"P\"\nsize 512,342\nend stack\n\n"
           "background \"F\"\nid 2\nend background\n\n"
           "card \"A\" background \"F\"\nid 3\n");

    remove(FIC);
    return 0;
}
