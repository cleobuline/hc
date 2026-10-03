/* L'ÉDITEUR DE POLYGONE, côté noyau (docs/mesures/polygone.txt).
 *
 * Le scénario de l'utilisatrice : un polygone naît triangle ; ses sommets
 * se déplacent ; un clic sur un côté en ajoute un ; Delete en ôte un, jamais
 * sous trois. L'application tient la souris ; ce harnais tient la géométrie
 * qu'elle appelle. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("      %s\n", t);
    else if (k == HC_ERR) printf("      [ERR] %s\n", t);
}

static Object *B, *P;

static void joue(const char *s)
{
    char b[1024];
    printf("   %s\n", s);
    snprintf(b, sizeof b, "on t\n%s\nend t\n", s);
    hc_set_script(B, b);
    hc_send(B, "t");
}

static void montre(const char *quoi)
{
    int xy[2 * HC_SOMMETS_MAX];
    int n = hc_bouton_sommets(P, xy, HC_SOMMETS_MAX);
    printf("   %-34s %d sommets :", quoi, P->npoints);
    for (int i = 0; i < n && P->npoints; i++) printf(" %d,%d", xy[2 * i], xy[2 * i + 1]);
    printf("   rect %d,%d,%d,%d\n", P->x, P->y, P->x + P->w, P->y + P->h);
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "C");
    P = hc_new_button(c, "p");
    P->x = 100; P->y = 100; P->w = 60; P->h = 40;
    B = hc_new_button(c, "s");
    hc_set_current_card(c);

    puts("== 1. un polygone naît triangle, inscrit dans son rectangle ==");
    montre("rectangle ordinaire");
    joue("set the style of button \"p\" to polygon");
    montre("devenu polygone");
    joue("put the points of button \"p\"");
    joue("set the style of button \"p\" to polygon");
    montre("polygone redemandé : inchangé");
    printf("   hc_polygone_par_defaut sur un polygone qui a ses sommets : %d\n",
           hc_polygone_par_defaut(P));

    puts("\n== 2. tirer un sommet ==");
    printf("   sommet proche de 131,101 (tol 4) : %d\n", hc_sommet_proche(P, 131, 101, 4));
    printf("   sommet proche de 140,120 (tol 4) : %d\n", hc_sommet_proche(P, 140, 120, 4));
    hc_sommet_deplace(P, 0, 130, 80);
    montre("sommet 0 tiré en 130,80");
    printf("   sommet 7 (n'existe pas) : %d\n", hc_sommet_deplace(P, 7, 1, 1));

    puts("\n== 3. un clic sur un côté ajoute un sommet ==");
    printf("   côté proche de 130,141 (tol 4) : %d   (la base : du sommet 1 au sommet 2)\n",
           hc_cote_proche(P, 130, 141, 4));
    printf("   côté proche de 130,120 (tol 4) : %d   (au milieu : aucun)\n",
           hc_cote_proche(P, 130, 120, 4));
    int k = hc_sommet_insere(P, hc_cote_proche(P, 130, 141, 4), 130, 140);
    printf("   inséré à l'indice %d\n", k);
    montre("après insertion");
    hc_sommet_deplace(P, k, 130, 170);
    montre("le nouveau tiré en 130,170");

    puts("\n== 4. Delete ôte un sommet, jamais sous trois ==");
    printf("   ôter le sommet 3 : %d\n", hc_sommet_ote(P, 3));
    montre("après");
    printf("   ôter encore (il en reste trois) : %d   (refusé)\n", hc_sommet_ote(P, 0));
    montre("inchangé");
    printf("   ôter un sommet qui n'existe pas : %d\n", hc_sommet_ote(P, 9));

    puts("\n== 5. un polygone étiré, puis retouché : la forme ne saute pas ==");
    joue("set the width of button \"p\" to 120");
    montre("élargi de 30 à 120 points");
    hc_sommet_deplace(P, 1, P->x + P->w, P->y + P->h + 10);
    montre("sommet 1 tiré 10 plus bas");

    puts("\n== 6. un bouton qui n'est pas un polygone ==");
    Object *r = hc_new_button(c, "r");
    r->x = 0; r->y = 0; r->w = 10; r->h = 10;
    printf("   par défaut : %d   déplacer : %d   insérer : %d   ôter : %d\n",
           hc_polygone_par_defaut(r), hc_sommet_deplace(r, 0, 1, 1),
           hc_sommet_insere(r, 0, 1, 1), hc_sommet_ote(r, 0));

    hc_free(st);
    return 0;
}
