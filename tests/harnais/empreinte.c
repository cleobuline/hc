/* L'empreinte d'une pile (hc_empreinte), pour la question « Save changes? »
 * à la fermeture — demandée par l'utilisatrice le 27 novembre, après une
 * carte perdue en fermant une pile modifiée.
 *
 * Ce qu'il tient :
 *   1. la même pile, deux fois de suite : la même empreinte ;
 *   2. un champ modifié, un bouton ajouté, un script changé : une autre ;
 *   3. le champ remis comme avant : l'empreinte d'avant revient ;
 *   4. le DESSIN n'y entre pas — il est suivi par l'interface ;
 *   5. enregistrer puis relire : la pile relue a l'empreinte de celle qui
 *      vient d'être écrite, chemin compris (le chemin n'y entre pas) ;
 *   6. pas de pile : 0. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_ERR) printf("      [ERR] %s\n", t);
}

static const char *pareil(unsigned long long a, unsigned long long b)
{
    return a == b ? "la même" : "une autre";
}

int main(void)
{
    static HcHost h;
    h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Rosace");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "C");
    Object *f  = hc_new_field(c, "f");
    hc_set_current_card(c);
    hc_set_field_text(f, "vitrail");

    unsigned long long e0 = hc_empreinte(st);
    printf("1. deux fois de suite : %s\n", pareil(e0, hc_empreinte(st)));
    printf("   non nulle : %s\n", e0 ? "oui" : "NON");

    hc_set_field_text(f, "vitrail bleu");
    printf("2. champ modifié : %s\n", pareil(e0, hc_empreinte(st)));
    hc_set_field_text(f, "vitrail");
    printf("3. champ remis : %s\n", pareil(e0, hc_empreinte(st)));

    Object *b = hc_new_button(c, "B");
    printf("2. bouton ajouté : %s\n", pareil(e0, hc_empreinte(st)));
    unsigned long long e1 = hc_empreinte(st);
    hc_set_script(b, "on mouseUp\n  beep\nend mouseUp\n");
    printf("2. script changé : %s\n", pareil(e1, hc_empreinte(st)));

    unsigned long long e2 = hc_empreinte(st);
    hc_set_paint(c, "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");
    printf("4. dessin posé : %s\n", pareil(e2, hc_empreinte(st)));

    char chemin[] = "/tmp/hc_empreinte_XXXXXX";
    int fd = mkstemp(chemin);
    if (fd >= 0) close(fd);
    if (hc_save(st, chemin) != 0) { puts("5. enregistrement : ÉCHEC"); return 1; }
    unsigned long long es = hc_empreinte(st);
    printf("5. après l'enregistrement : %s\n", pareil(e2, es));
    Object *relue = hc_load(chemin);
    printf("5. la pile relue : %s\n", relue ? pareil(es, hc_empreinte(relue)) : "ILLISIBLE");
    remove(chemin);

    printf("6. pas de pile : %llu\n", hc_empreinte(NULL));

    if (relue) hc_free(relue);
    hc_free(st);
    return 0;
}
