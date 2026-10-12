/* flash — L'ÉCRAN CLIGNOTE : la carte s'inverse, puis revient, n fois.
 *
 * HC répondait « commande inconnue » : flash était dans la liste « Not
 * supported yet » des notes de version.
 *
 * DIT PAR L'UTILISATRICE le 12 octobre, NON VU DANS HYPERCARD : « flash »
 * clignote une fois, « flash 2 » deux fois, « flash 3 » trois fois.
 *
 * NON MESURÉ, et choisi par HC : la vitesse (FLASH_TICKS, hc_core.c), un
 * nombre nul ou négatif (rien), un nombre qui n'en est pas un (refusé), un
 * nombre décimal (sa partie entière). Le harnais tient la place de l'hôte et
 * compte les inversions ; la carte doit TOUJOURS finir à l'endroit. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static char g_trace[256];
static int  g_etat;
static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if      (k == HC_ERR) printf("      [ERR] %s\n", t);
    else if (k == HC_MSG) printf("      = %s\n", t);
}
static void inverse(int i)
{
    size_t l = strlen(g_trace);
    if (l + 2 < sizeof g_trace) { g_trace[l] = i ? 'X' : '.'; g_trace[l + 1] = '\0'; }
    g_etat = i;
}
static void fais(const char *l)
{
    g_trace[0] = '\0';
    g_etat = 0;
    printf("   %s\n", l);
    hc_do(l);
    printf("      hôte : %s  (fin %s)\n", g_trace[0] ? g_trace : "rien",
           g_etat ? "INVERSÉE" : "à l'endroit");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne; h.flash_screen = inverse;
    hc_set_host(&h);
    Object *st = hc_new_stack("S"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "C");
    hc_set_current_card(c);

    puts("== 1. une fois, deux fois, trois fois (dit par l'utilisatrice) ==");
    fais("flash");
    fais("flash 2");
    fais("flash 3");

    puts("\n== 2. une expression, une variable ==");
    fais("flash 1 + 1");
    fais("put 3 into n");
    fais("flash n");

    puts("\n== 3. les bornes (NON MESURÉ) ==");
    fais("flash 0");
    fais("flash -2");
    fais("flash 2.7");
    fais("flash \"zut\"");
    fais("put the result");

    puts("\n== 4. sans hôte qui clignote : pas d'erreur ==");
    h.flash_screen = NULL;
    fais("flash 2");
    fais("put the result");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
