#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    /* Sans ce garde, un fichier de données absent donnait un fseek sur NULL
     * — donc un plantage, sans dire lequel manquait. C'est exactement ce que
     * le CI a montré : cinq harnais lisaient des chemins qui n'existaient que
     * sur la machine où ils ont été écrits. */
    if (!f) { fprintf(stderr, "données introuvables : %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(n+1); fread(buf, 1, n, f); buf[n] = 0; fclose(f);
    return buf;
}
int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *fld = hc_new_field(card1, "calendar");
    fld->x = 10; fld->y = 10; fld->w = 300; fld->h = 200;
    hc_set_current_card(card1);

    hc_set_script(fld, slurp("donnees/calendrier_complet.txt"));
    hc_set_script(card1,
        "on openCard\n"
        "  send \"updateCalendar\" to card field \"calendar\"\n"
        "  pass openCard\n"
        "end openCard\n");

    hc_send(card1, "openCard");

    printf("=== texte du champ apres openCard ===\n%s\n", hc_field_text(fld));
    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
