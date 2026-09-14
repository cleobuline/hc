/* « x is a date » disait oui a peu pres a tout.
 *
 * Le test tenait dans une ligne :
 *
 *     sscanf(v, "%d/%d/%d", &m, &j, &an) == 3
 *
 * qui ne regarde NI la queue de la chaine — « 12/25/96patate » passait — NI le
 * calendrier — « 99/99/99 » passait aussi, alors qu'il n'y a pas de 99e mois.
 *
 * Et il se contredisait avec `convert`, qui, lui, connait les noms de mois et
 * les dateItems. La garde la plus banale du langage
 *
 *     if d is a date then convert d to seconds
 *
 * refusait donc des dates que la ligne suivante aurait converties sans
 * broncher. Deux definitions de ce qu'est une date, et elles divergeaient.
 *
 * Il n'y en a plus qu'une : celle de parse_datetime, prise dans son acception
 * STRICTE — rien d'incompris dans la chaine, et des composantes qui existent
 * au calendrier. La distinction compte : `convert` DOIT continuer d'accepter
 * un 31 fevrier et de le normaliser en 3 mars, c'est tout l'interet de
 * « add 1 to item 3 of d ». La souplesse reste donc a convert ; la severite
 * va a `is a date`. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   %s\n", t); }

static void essaie(const char *v)
{
    char cmd[256];
    printf("   %-28s -> ", v);
    snprintf(cmd, sizeof cmd, "put (\"%s\" is a date)", v);
    hc_do(cmd);
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    hc_set_current_card(hc_new_card(st, bg, "A"));

    /* La banniere de selection de l'executeur sort au PREMIER hc_do : on la
     * provoque ici, sinon elle se colle au premier releve et le rend illisible. */
    hc_do("get 1");

    puts("=== ce qui EST une date ===");
    essaie("12/25/96");
    essaie("8/7/2026");
    essaie("2/29/2024");                 /* 2024 est bissextile */
    essaie("August 7, 2026");            /* convert l'accepte : is a date aussi */
    essaie("Friday, August 7, 2026");
    essaie("2026,8,7");                  /* dateItems */
    essaie("2026,8,7,14,30,0,6");
    essaie("3868905600");                /* les secondes du Macintosh, que convert rend */

    puts("\n=== ce qui n'en est PAS ===");
    essaie("99/99/99");                  /* pas de 99e mois */
    essaie("13/1/96");                   /* pas de 13e mois */
    essaie("2/30/1996");                 /* fevrier n'a pas 30 jours */
    essaie("2/29/1900");                 /* 1900 n'est PAS bissextile */
    essaie("12/25/96patate");            /* la queue compte */
    essaie("12/0/96");                   /* il n'y a pas de jour zero */
    essaie("patate");
    essaie("");
    essaie("3:30 PM");                   /* une heure seule n'est pas une date */
    essaie("42");

    puts("\n=== 2/29 : une annee bissextile sur deux siecles ===");
    essaie("2/29/2000");                 /* divisible par 400 : bissextile */
    essaie("2/29/2100");                 /* divisible par 100 seulement : non */

    puts("\n=== la garde classique ne se contredit plus ===");
    hc_do("put \"August 7, 2026\" into d");
    hc_do("if d is a date then convert d to seconds");
    hc_do("put d");

    puts("\n=== convert reste SOUPLE : un 31 fevrier se normalise ===");
    hc_do("put \"2/31/1996\" into e");
    hc_do("put (e is a date)");
    hc_do("convert e to short date");
    hc_do("put e");

    puts("\n=== is not a date ===");
    hc_do("put (\"99/99/99\" is not a date)");
    hc_do("put (\"12/25/96\" is not a date)");

    hc_free(st);
    return 0;
}
