#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void ligne(HcLineKind k,int d,const char *t){ (void)k;(void)d;(void)t; }

static double maintenant(void)
{
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void mesure(int nboutons)
{
    HcHost h; memset(&h,0,sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *stack = hc_new_stack("Bench");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card  = hc_new_card(stack, bg, "Une");
    hc_set_current_card(card);

    char nom[32];
    Object *cible = NULL;
    for (int i = 0; i < nboutons; i++) {
        snprintf(nom, sizeof nom, "Bouton %d", i);
        Object *b = hc_new_button(card, nom);
        b->textfont = strdup("Geneva");
        if (i == 0) cible = b;
    }
    hc_set_script(cible, "on mouseUp\n  put 1 into x\nend mouseUp\n");

    const int N = 200;
    double t0 = maintenant();
    for (int i = 0; i < N; i++) hc_send(cible, "mouseUp");
    double dt = maintenant() - t0;

    printf("  %4d boutons : %7.3f ms par clic\n", nboutons, dt * 1000.0 / N);
    hc_free(stack);
}

int main(void)
{
    printf("Coût NOYAU d'un clic, selon le nombre de boutons sur la carte :\n");
    mesure(1); mesure(50); mesure(200); mesure(500); mesure(1000);
    return 0;
}
