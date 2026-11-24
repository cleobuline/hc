/* « click twice at » — le double-clic de LiveCode, extension de HC décidée
 * par l'utilisatrice le 24 novembre, pour fermer un polygone par script.
 *
 * Ce que le noyau doit faire, et que ce harnais tient :
 *   1. « click at » reste un clic simple, avec ou sans touches ;
 *   2. « click twice at » appelle click_twice_at de l'hôte, une seule fois,
 *      avec le point et les touches ;
 *   3. un hôte qui n'a pas click_twice_at reçoit DEUX click_at ;
 *   4. « twice » ne devient pas une partie du point : un point calculé passe.
 *
 * Ce que fait l'hôte Cocoa du double-clic — poser le dernier sommet du
 * polygone puis le terminer — ne se vérifie pas ici : c'est du code AppKit,
 * compilé par le job « interface Cocoa » de l'intégration continue, et
 * mesuré DANS HC (l'application) par l'utilisatrice. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_ERR) printf("      [ERR] %s\n", t);
}
static void mon_clic(int x, int y, const char *m)
{
    printf("      [HÔTE] click_at %d,%d [%s]\n", x, y, m ? m : "");
}
static void mon_double(int x, int y, const char *m)
{
    printf("      [HÔTE] click_twice_at %d,%d [%s]\n", x, y, m ? m : "");
}

static Object *B;
static void joue(const char *l)
{
    char s[512];
    printf("   %s\n", l);
    snprintf(s, sizeof s, "on t\n%s\nend t\n", l);
    hc_set_script(B, s);
    hc_send(B, "t");
}

int main(void)
{
    static HcHost h;
    h.line = ligne;
    h.click_at = mon_clic;
    h.click_twice_at = mon_double;
    hc_set_host(&h);

    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "C");
    B = hc_new_button(c, "script");
    hc_set_current_card(c);

    puts("== 1. le clic simple, inchangé ==");
    joue("click at 100,120");
    joue("click at 100,120 with shiftKey");

    puts("\n== 2. le double-clic ==");
    joue("click twice at 100,120");
    joue("click twice at 5,6 with optionKey");
    joue("put 30 into x\nclick twice at x*2, x+1");

    puts("\n== 3. un hôte sans double-clic reçoit deux clics ==");
    h.click_twice_at = NULL;
    joue("click twice at 7,8");

    hc_free(st);
    return 0;
}
