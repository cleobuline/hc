#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void dump(const char *label, const char *s)
{
    printf("%-22s [%s] octets:", label, s ? s : "(null)");
    for (const unsigned char *p=(const unsigned char*)s; p && *p; p++) printf(" %02X", *p);
    printf("\n");
}
int main(void)
{
    const char *nom = "\xe2\x80\x9cDepth\xe2\x80\x9d";
    dump("litteral C", nom);
    Object *stack = hc_new_stack("T");
    Object *bg    = hc_new_background(stack, "F");
    Object *card  = hc_new_card(stack, bg, "U");
    hc_set_current_card(card);
    Object *b = hc_new_button(bg, nom);
    dump("apres hc_new_button", b->name);
    char d[128]; hc_describe(b, d, sizeof d);
    dump("hc_describe", d);
    Object *r = hc_resolve("bg btn \"\xe2\x80\x9cDepth\xe2\x80\x9d\"");
    printf("resolution par nom : %s\n", r ? "TROUVE" : "echec");
    Object *r2 = hc_resolve("bg btn \"Depth\"");
    printf("sans les guillemets courbes : %s\n", r2 ? "TROUVE" : "echec");
    hc_free(stack); return 0;
}
