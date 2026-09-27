/* torture2 — LA PILE DE TORTURE, MONTÉE PAR LE CODE.
 *
 * Elle éprouve ce que les bancs de « docs/mesures » ont mesuré dans
 * HyperCard : la recherche, les morceaux qui débordent, ce que désignent les
 * « selected… », les couches, et les dix messages du clavier. Chaque essai
 * annonce ce qu'il attend et ce qu'il obtient ; ce qui n'est pas mesuré
 * s'affiche SANS verdict, marqué « ? ».
 *
 * UN SEUL PROGRAMME POUR DEUX USAGES, et c'est tout l'intérêt :
 *
 *   torture2 bouton.txt pile.txt              le test de non-régression
 *   torture2 bouton.txt pile.txt sortie.stack écrit la pile sur le disque
 *
 * Le second produit le fichier qu'on ouvre dans l'application pour cliquer
 * soi-même. Monter la pile deux fois — une fois ici, une fois dans un
 * générateur à part — aurait donné deux piles qui divergent au premier
 * changement, et le test aurait cessé de parler de la pile livrée.
 *
 * POURQUOI LES SCRIPTS SONT DANS « donnees/ » et non dans ce fichier : ils
 * font quatre cents lignes de HyperTalk, ils sont faits pour être relus et
 * modifiés par quelqu'un qui n'écrit pas de C, et surtout ils doivent pouvoir
 * être recopiés tels quels dans HyperCard pour comparer. Un script enfermé
 * dans des guillemets C ne se recopie pas.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s bouton.txt pile.txt [sortie.stack]\n", argv[0]);
        return 2;
    }
    char *src_bouton = lire(argv[1]);
    char *src_pile   = lire(argv[2]);
    if (!src_bouton || !src_pile) { free(src_bouton); free(src_pile); return 2; }

    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Torture");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");

    /* LE CHAMP DE FOND EST CRÉÉ AVANT LES CARTES et il n'est PAS partagé :
     * chaque carte y garde son propre texte, ce qui est indispensable à la
     * section 1 — la recherche doit trouver « alpha » sur deux cartes et
     * « élève » sur une troisième, dans le MÊME champ de fond. */
    pose_champ(bg, "T", 20, 20, 470, 28, NULL);

    Object *c1 = hc_new_card(st, bg, "Atelier");
    hc_new_card(st, bg, "Deux");
    hc_new_card(st, bg, "Trois");
    hc_new_card(st, bg, "Quatre");

    /* L'ORDRE DE CRÉATION EST LE NUMÉRO, et la section 3 le vérifie :
     * A est « card field 1 », B « card field 2 », R « card field 3 ». Les
     * créer dans un autre ordre ferait échouer des essais qui ont raison. */
    pose_champ(c1, "A", 20,  54, 230, 26, NULL);
    pose_champ(c1, "B", 260, 54, 230, 26, NULL);
    /* LA CARTE FAIT 512 × 342, et tout doit y tenir : le champ du rapport
     * s'arrête à 288, les boutons occupent la bande du dessous. Posé d'abord
     * à y=450, il sortait de la carte — invisible dans l'application, alors
     * que le harnais, qui lit le contenu et non l'écran, ne s'en plaignait
     * pas. La géométrie est le seul endroit où ce harnais ne mesure rien, et
     * elle s'est trompée DEUX FOIS : le cinquième bouton, ajouté ensuite, a
     * d'abord été posé sur une deuxième rangée à y=328, qui dépasse elle aussi.
     * D'où une seule rangée de cinq, calculée : 5 × 90 + 4 × 4 = 466, posée de
     * 20 à 486, et 302 + 22 = 324, sous les 342. Un commentaire qui décrit le
     * piège ne l'évite pas ; un calcul écrit, oui. */
    Object *r = pose_champ(c1, "R", 20, 88, 470, 200, "scrolling");
    /* Le champ du rapport porte tous les motifs cherchés, puisqu'il nomme
     * chaque essai : sans dontSearch, la section 1 se trouverait elle-même.
     * Le script le repose à chaque clic — ici, c'est pour que le FICHIER
     * livré l'ait déjà. */
    if (r) r->dont_search = 1;

    /* LE BOUTON PORTE LES SIX SECTIONS, la pile porte l'échafaudage. C'est le
     * partage naturel : « journal », « egal » et « note » servent aux quatre
     * boutons et doivent donc être au-dessus d'eux, tandis que les sections ne
     * servent qu'à celui-ci. Tout mettre dans le script de la pile aurait
     * donné un « on mouseUp » au niveau de la pile, qui aurait intercepté les
     * clics de tous les autres objets. */
    Object *bt = pose_bouton(c1, "TORTURE", 20, 302, 90, 22, src_bouton);
    pose_bouton(c1, "clavier", 114, 302, 90, 22,
                "-- Relit les touches tapées POUR DE VRAI, depuis le journal que\n"
                "-- tiennent les dix gestionnaires du script de la pile.\n"
                "on mouseUp\n  relitClavier\nend mouseUp\n");
    pose_bouton(c1, "selection", 208, 302, 90, 22,
                "-- Cliquez d'abord DANS le champ A, puis ici : c'est la seule\n"
                "-- façon de mesurer une sélection posée à la main.\n"
                "on mouseUp\n  relitSelection\nend mouseUp\n");
    pose_bouton(c1, "Effacer", 302, 302, 90, 22,
                "on mouseUp\n  razCompteurs\nend mouseUp\n");
    /* LE BOUTON DES QUESTIONNES OUVERTES EST À PART, et ce n'est pas du rangement :
     * ses essais LÈVENT — « hide ch » le premier — et dans l'application une
     * erreur arrête le script sur un dialogue. Ils étaient d'abord à la suite
     * des autres, et le rapport s'arrêtait là : on perdait la fin et le bilan.
     * Le banc s'arrêtait là où il ne voulait qu'OBSERVER. */
    Object *bo = pose_bouton(c1, "ouvert", 396, 302, 90, 22,
                "-- Ce qui n'est pas encore mesuré. Chaque essai lève peut-être :\n"
                "-- cliquez ce qu'il faut pour continuer, le suivant se joue\n"
                "-- quand même.\n"
                "on mouseUp\n  sectionOuvert\nend mouseUp\n");

    hc_set_script(st, src_pile);
    hc_set_current_card(c1);

    if (argc > 3) {
        /* On écrit la pile AVANT de la faire tourner : le fichier livré doit
         * partir d'un rapport vide, pas du rapport d'un essai déjà joué. */
        if (hc_save(st, argv[3]) != 0) {
            fprintf(stderr, "impossible d'écrire %s\n", argv[3]);
            hc_free(st); free(src_bouton); free(src_pile); return 1;
        }
        printf("pile écrite : %s\n", argv[3]);
    }

    hc_send(bt, "mouseUp");
    /* Le second bouton tout de suite après : le harnais, lui, n'a pas de
     * dialogue à cliquer, et les lignes « ? » doivent rester sous la suite —
     * une question qu'on cesse de poser est une question qu'on oublie. */
    hc_send(bo, "mouseUp");

    {   /* Le rapport vit dans le champ, ligne à ligne, et non dans la boîte
         * de message : « put » vers elle passe par un tampon de 1024 octets
         * et couperait un long rapport en silence. */
        for (int i = 0; i < c1->nparts; i++) {
            Object *f = c1->parts[i];
            if (f->type == OBJ_FIELD && f->name && !strcmp(f->name, "R")) {
                printf("\n───── champ « R » ─────\n%s\n",
                       f->contents ? f->contents : "(vide)");
                break;
            }
        }
    }

    hc_free(st); free(src_bouton); free(src_pile);
    return 0;
}
