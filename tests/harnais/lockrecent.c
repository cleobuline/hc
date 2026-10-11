/* lockrecent — « lock recent » retire les cartes du menu Recent, et de lui seul.
 *
 * Relevé par un audit des sources le 11 octobre : « lock recent » posait
 * lockRecent, qui se relisait « true », et les cartes visitées continuaient
 * d'entrer dans le menu Recent. Personne ne consultait la propriété.
 *
 * Ce qu'elle doit faire, la carte lockRecent de HyperTalk Reference le dit :
 * « whether HyperCard displays miniature pictures for the last 42 cards
 * visited […] in the Recent card dialog box. (LockRecent does not affect the
 * trail of cards you can go back to.) » Le menu Recent — et « the recent
 * cards », qui en est la liste — perd donc les cartes visitées sous la
 * serrure ; « go back » les retrace toutes.
 *
 * NON MESURÉ dans HyperCard : que lockRecent retombe au repos. La référence le
 * dit de lockScreen et de lockErrorDialogs, pas de lui ; HC ne l'éteint donc
 * pas (section 4). */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      = %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static void fais(const char *l) { printf("   %s\n", l); hc_do(l); }

static void recentes(void)
{
    Object *vues[64];
    int n = hc_recent_distinct(vues, 64);
    printf("      menu Recent :");
    for (int i = 0; i < n; i++) printf(" %s", vues[i]->name);
    printf("\n");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("S"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c1 = hc_new_card(st, bg, "C1");
    for (int i = 2; i <= 6; i++) {
        char nom[8]; snprintf(nom, sizeof nom, "C%d", i);
        hc_new_card(st, bg, nom);
    }
    hc_set_current_card(c1);
    hc_go_card(c1);

    puts("== 1. sans serrure, chaque carte visitée entre au menu ==");
    fais("go card 2");
    recentes();

    puts("\n== 2. sous « lock recent », les cartes visitées n'y entrent pas ==");
    fais("lock recent");
    fais("put the lockRecent");
    fais("go card 3");
    fais("go card 4");
    recentes();
    fais("put the recent names");

    puts("\n== 3. « unlock recent » : les suivantes y entrent de nouveau ==");
    fais("unlock recent");
    fais("go card 5");
    recentes();

    puts("\n== 4. « go back » retrace TOUTE la piste, serrure ou non ==");
    for (int i = 0; i < 4; i++) {
        fais("go back");
        hc_do("put \"ici : \" & the short name of this card");
    }

    puts("\n== 5. par « set », dans un gestionnaire ==");
    Object *b = hc_new_button(hc_current_card(), "B");
    hc_set_script(b,
        "on mouseUp\n"
        "  set the lockRecent to true\n"
        "  go card 6\n"
        "  go card 1\n"
        "  set the lockRecent to false\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");
    hc_do("put \"ici : \" & the short name of this card");
    recentes();
    fais("put the lockRecent");

    puts("\n== 6. une serrure laissée en place ne retombe pas d'elle-même ==");
    hc_set_script(b, "on mouseUp\n  lock recent\nend mouseUp\n");
    hc_send(b, "mouseUp");
    fais("put the lockRecent");
    fais("go card 6");
    recentes();
    fais("unlock recent");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
