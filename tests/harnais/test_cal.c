#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(n+1);
    fread(buf, 1, n, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}

int main(void)
{
    Object *stack = hc_new_stack("Test");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card1 = hc_new_card(stack, bg, "Une");
    Object *fld = hc_new_field(card1, "CalField");
    fld->x = 10; fld->y = 10; fld->w = 300; fld->h = 200;
    hc_set_current_card(card1);

    char *script = slurp("donnees/calendrier_court.txt");
    hc_set_script(fld, script);
    free(script);              /* hc_set_script a copié : le tampon peut partir */

    hc_send(fld, "drawCalendar 2026,12,1,0,0,0,3");

    printf("=== texte du champ apres drawCalendar ===\n%s\n", hc_field_text(fld));

    hc_v3_bilan();
    hc_free(stack);
    return 0;
}
