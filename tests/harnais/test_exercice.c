#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *lire(const char *chemin)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) { perror(chemin); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1);
    fread(b, 1, (size_t)n, f); b[n] = 0; fclose(f);
    return b;
}
int main(int argc, char **argv)
{
    const char *chemin = argc > 1 ? argv[1] : "exercice.txt";
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *btn   = hc_new_button(card1, "Exercice");
    hc_set_current_card(card1);
    char *s = lire(chemin);
    hc_set_script(btn, s);
    hc_send(btn, "mouseUp");
    free(s);
    hc_free(stack);
    return 0;
}
