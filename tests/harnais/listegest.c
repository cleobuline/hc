/* listegest — LES MENUS « HANDLERS » ET « FUNCTIONS » DE L'ÉDITEUR.
 *
 * La fenêtre de script d'HyperCard 2.4 porte deux menus locaux qui listent
 * les gestionnaires du script, « on » d'un côté, « function » de l'autre ;
 * l'utilisatrice l'a montré le 9 octobre, dans Basilisk II, sur le script de
 * pile de « HyperTalk Reference ». L'éditeur de HC les reprend, et c'est
 * hct_gestionnaires qui les remplit : ce harnais tient ce qu'elle rend.
 *
 * Les pièges, chacun pris dans une pile réelle :
 *   · un bandeau SANS « -- » avant le premier gestionnaire, comme celui
 *     d'Apple — une de ses lignes commence par « on » ;
 *   · un « on » en commentaire, et un dans une chaîne ;
 *   · une ligne continuée par « ¬ » : les numéros de ligne qui suivent
 *     doivent rester ceux de l'éditeur, où le curseur sera placé ;
 *   · un gestionnaire dont le corps porte une faute de syntaxe : il reste
 *     dans la liste, c'est justement là qu'on veut aller.
 */
#include "hct_verif.h"
#include <stdio.h>
#include <string.h>

static void liste(const char *titre, const char *src)
{
    HctGestionnaire g[16];
    int n = hct_gestionnaires(src, g, 16);
    printf("== %s : %d ==\n", titre, n);
    for (int i = 0; i < n; i++)
        printf("   ligne %3d  %-9s %s\n", g[i].ligne,
               g[i].fonction ? "function" : "on", g[i].nom);
}

int main(void)
{
    liste("vide", "");
    liste("sans gestionnaire", "-- rien que des commentaires\n-- on mouseUp\n");

    liste("le script de pile d'Apple, dans sa forme",
        "∞∞∞∞∞∞∞∞∞∞∞∞∞∞∞∞\n"                                   /*  1 */
        "HyperTalk Reference Stack\n"                           /*  2 */
        "on the other hand, this banner has no dashes\n"         /*  3 */
        "∞∞∞∞∞∞∞∞∞∞∞∞∞∞∞∞\n"                                   /*  4 */
        "\n"                                                    /*  5 */
        "on openStack\n"                                        /*  6 */
        "  if wrongStack() then pass openStack\n"               /*  7 */
        "  useHyperCardHelp\n"                                  /*  8 */
        "  pass openStack\n"                                    /*  9 */
        "end openStack\n"                                       /* 10 */
        "\n"                                                    /* 11 */
        "on resumeStack\n"                                      /* 12 */
        "  -- on fauxGestionnaire, en commentaire\n"            /* 13 */
        "  put \"on dansUneChaine\" into x\n"                   /* 14 */
        "end resumeStack\n"                                     /* 15 */
        "\n"                                                    /* 16 */
        "function wrongStack\n"                                 /* 17 */
        "  get the value of word 2 of ¬\n"                      /* 18 */
        "  the long name of me\n"                               /* 19 */
        "  return (it is not line 1 of the stacks)\n"           /* 20 */
        "end wrongStack\n"                                      /* 21 */
        "\n"                                                    /* 22 */
        "on setCheckMark TRUEorFALSE\n"                         /* 23 */
        "  global gHMnu\n"                                      /* 24 */
        "  set the checkMark of menuItem 4 of menu gHMnu to TRUEorFALSE\n"
        "end setCheckMark\n");                                  /* 26 */

    liste("une faute dans un corps",
        "on mouseUp\n"
        "  repeat with i = 1\n"           /* « to » manque */
        "  end repeat\n"
        "end mouseUp\n"
        "function double x\n"
        "  return x * 2\n"
        "end double\n");

    liste("fins de ligne Mac d'époque (CR)",
        "on mouseUp\r  beep\rend mouseUp\r\rfunction f\r  return 1\rend f\r");

    /* Le plafond : on n'écrit jamais au-delà de `max`. */
    {
        HctGestionnaire g[2];
        int n = hct_gestionnaires("on a\nend a\non b\nend b\non c\nend c\n", g, 2);
        printf("== plafond à 2 : %d (%s, %s) ==\n", n, g[0].nom, g[1].nom);
    }
    return 0;
}
