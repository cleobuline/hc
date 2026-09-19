/* « REPEAT FOR 3 TIMES » NE TOURNAIT PAS, ET L'EN-TETE NE FINISSAIT PAS AVEC
 * SA LIGNE.
 *
 * DEUX DEFAUTS DANS LA MEME FONCTION, et le second explique le premier.
 *
 * 1. LE « FOR » EST FACULTATIF CHEZ HYPERCARD. La grammaire d'origine est
 *    « repeat [for] <nombre> [times] » : les deux mots encadrants s'omettent
 *    separement, si bien que « repeat 3 », « repeat 3 times »,
 *    « repeat for 3 » et « repeat for 3 times » sont la MEME boucle. Nous
 *    n'acceptions que les deux premieres. L'analyseur lisait « for » comme le
 *    debut d'une expression, se plaignait deux fois, et la boucle faisait
 *    ZERO tour. Un script d'epoque parfaitement valide ne tournait pas.
 *
 *    (« repeat for each line L in … » etait deja traite : cette branche-la
 *    se reconnait au DEUXIEME mot, et passe avant.)
 *
 * 2. L'EN-TETE NE VERIFIAIT PAS QU'IL FINISSAIT AVEC SA LIGNE. corps() pose
 *    ce controle apres chaque instruction — mais pour un repeat,
 *    « l'instruction » englobe le corps ET le « end repeat », si bien que le
 *    controle tombe APRES le end, et que ce qui trainait sur la ligne
 *    d'en-tete a deja ete avale par le corps. Le mot en trop devenait la
 *    premiere instruction de la boucle, reexecutee a chaque tour :
 *
 *      repeat with i = 1 to 10 step 3      -- « step » n'est pas d'HyperTalk
 *      -> « personne ne repond a "step 3" », DIX fois, et dix tours
 *
 *    Dix messages qui parlent d'un envoi imaginaire, la ou un seul devait
 *    dire que l'en-tete n'est pas correct. La boucle tourne toujours — sur
 *    l'en-tete tel qu'il a pu etre lu —, ce qui change est le nombre de
 *    plaintes : une, a l'analyse, au lieu d'une par tour.
 *
 * ET PAS DEUX PLAINTES POUR UNE SEULE FAUTE. Quand l'en-tete a deja echoue,
 * ce qui reste sur la ligne est le RESTE de cette faute-la :
 *
 *   repeat with i = 1 up to 5
 *   -> « to » attendu                    (la vraie cause)
 *   -> texte inattendu en fin de ligne   (la meme, redite)
 *
 * Le controle ne parle donc que si l'en-tete s'est lu sans faute. Il saute
 * quand meme jusqu'au bout de la ligne — sinon le mot en trop redeviendrait
 * la premiere instruction du corps, ce qu'on vient d'empecher.
 *
 * Ce harnais tient les SIX formes de repeat, les quatre ecritures du
 * comptage, les deux cas de texte en trop, et le fait qu'une seule faute ne
 * produit qu'un seul message.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

static void execute(const char *titre, const char *corps)
{
    char s[2048];
    printf("   %s\n", titre);
    snprintf(s, sizeof s, "on essaie\n%s\nend essaie\n", corps);
    hc_set_script(b, s);
    hc_send(b, "essaie");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);
    Object *f = hc_new_field(c, "F");
    hc_set_field_text(f, "alpha\nbeta\ngamma");
    b = hc_new_button(c, "B");

    puts("== 1. les quatre ecritures du comptage donnent la MEME boucle ==");
    execute("repeat 3",
            "  put 0 into n\n  repeat 3\n    add 1 to n\n  end repeat\n  put n");
    execute("repeat 3 times",
            "  put 0 into n\n  repeat 3 times\n    add 1 to n\n  end repeat\n  put n");
    execute("repeat for 3",
            "  put 0 into n\n  repeat for 3\n    add 1 to n\n  end repeat\n  put n");
    execute("repeat for 3 times",
            "  put 0 into n\n  repeat for 3 times\n    add 1 to n\n  end repeat\n  put n");
    /* Le compte peut etre une EXPRESSION : « for » ne doit pas avoir mange
     * le premier terme. */
    execute("repeat for 1+2 times",
            "  put 0 into n\n  repeat for 1+2 times\n    add 1 to n\n  end repeat\n  put n");

    puts("\n== 2. les autres formes ne bougent pas ==");
    execute("repeat nu + exit repeat",
            "  put 0 into n\n  repeat\n    add 1 to n\n"
            "    if n = 3 then exit repeat\n  end repeat\n  put n");
    execute("repeat forever",
            "  put 0 into n\n  repeat forever\n    add 1 to n\n"
            "    if n = 3 then exit repeat\n  end repeat\n  put n");
    execute("repeat while",
            "  put 0 into n\n  repeat while n < 3\n    add 1 to n\n  end repeat\n  put n");
    execute("repeat until",
            "  put 0 into n\n  repeat until n = 3\n    add 1 to n\n  end repeat\n  put n");
    execute("repeat with ... to",
            "  put 0 into n\n  repeat with i = 1 to 3\n    add i to n\n  end repeat\n  put n");
    execute("repeat with ... down to",
            "  put 0 into n\n  repeat with i = 3 down to 1\n    add i to n\n  end repeat\n  put n");
    execute("repeat with ... by",
            "  put 0 into n\n  repeat with i = 1 to 10 by 3\n    add 1 to n\n  end repeat\n  put n");

    puts("\n== 3. « for each » se reconnait toujours au deuxieme mot ==");
    execute("repeat for each line",
            "  put empty into r\n  repeat for each line L in card field \"F\"\n"
            "    put char 1 of L after r\n  end repeat\n  put r");
    execute("repeat for each item",
            "  put empty into r\n  repeat for each item I in \"a,b,c\"\n"
            "    put I after r\n  end repeat\n  put r");

    /* La boucle TOURNE quand meme, sur l'en-tete tel qu'il a pu etre lu : ce
     * qui change est le nombre de plaintes. Avant, le mot en trop etait
     * execute a chaque tour et se plaignait a chaque tour — dix messages pour
     * « step 3 », deux pour « blabla ». Maintenant, un seul, a l'analyse. */
    puts("\n== 4. du texte en trop sur l'en-tete : UN message, pas un par tour ==");
    execute("step, qui n'est pas d'HyperTalk",
            "  put 0 into n\n  repeat with i = 1 to 10 step 3\n"
            "    add 1 to n\n  end repeat\n  put n");
    execute("un mot quelconque",
            "  repeat 2 blabla\n    put \"tour\"\n  end repeat\n  put \"fini\"");

    puts("\n== 5. une seule faute, un seul message ==");
    execute("up to : « to » attendu, et rien de plus",
            "  repeat with i = 1 up to 5\n    put \"tour\"\n  end repeat");

    hc_free(st);
    return 0;
}
