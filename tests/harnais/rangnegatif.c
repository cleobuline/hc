/* UN RANG INFERIEUR A 1 : TROIS REPONSES A LA MEME QUESTION.
 *
 * SIGNALE PAR UN AUDIT EXTERIEUR. borne_simple refuse « n < 1 » : a la
 * lecture comme a la suppression, « item -1 » et « item 0 » sont des morceaux
 * ABSENTS. Mais l'ECRITURE ne passait pas par ce refus. Ne trouvant pas le
 * morceau, elle tombait dans la branche d'EXTENSION, ou
 *
 *     manquants = n - existants - 1;
 *     if (manquants < 0) manquants = 0;
 *
 * ramenait le compte a zero et ajoutait tranquillement un separateur et la
 * valeur. Mesure, sur « a,b » :
 *
 *     put item -1 of v          -> (vide)   absent
 *     delete item -1 of v       -> a,b      rien
 *     put "X" into item -1 of v -> a,b,X    AJOUTE
 *
 * Le meme morceau etait a la fois inexistant, insupprimable, et synonyme de
 * « a la fin ». Ce n'est pas une question de fidelite a HyperCard : c'est une
 * incoherence interne, et c'est ce qui la rend facile a trancher.
 *
 * LE CAS QUI FAIT MAL N'EST PAS « -1 » ECRIT A LA MAIN — personne ne
 * l'ecrit. C'est un rang CALCULE qui tombe a zero :
 *
 *     put "X" into item (i - 1) of v      avec i valant 1
 *
 * qui ajoutait un item au lieu de ne rien faire, sans un mot. Une boucle qui
 * recule d'un cran de trop allongeait la liste au lieu de s'arreter.
 *
 * LA REPONSE RETENUE est la chaine INCHANGEE, celle que la suppression donne
 * deja. Le point n'est pas laquelle des trois est la meilleure : c'est qu'il
 * n'y en ait qu'une.
 *
 * Ce harnais tient les quatre sortes de morceaux, les deux valeurs fautives
 * (0 et negatif), les trois operations, les plages — et surtout que les rangs
 * LEGITIMES n'ont pas bouge, extension comprise : un refus trop large
 * passerait tout le reste sans rien valoir.
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
    snprintf(s, sizeof s, "on essaie\n  set the itemDelimiter to \",\"\n%s\n"
                          "end essaie\n", corps);
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
    b = hc_new_button(c, "B");

    puts("== 1. les trois operations s'accordent enfin, sur item ==");
    execute("lecture, rang -1",
            "  put \"[\" & item -1 of \"a,b\" & \"]\"");
    execute("suppression, rang -1",
            "  put \"a,b\" into v\n  delete item -1 of v\n  put v");
    execute("ECRITURE, rang -1",
            "  put \"a,b\" into v\n  put \"X\" into item -1 of v\n  put v");
    execute("ECRITURE, rang 0",
            "  put \"a,b\" into v\n  put \"X\" into item 0 of v\n  put v");

    puts("\n== 2. les autres sortes de morceaux suivent la meme regle ==");
    execute("char 0",  "  put \"abc\" into v\n  put \"Z\" into char 0 of v\n  put v");
    execute("char -1", "  put \"abc\" into v\n  put \"Z\" into char -1 of v\n  put v");
    execute("word 0",  "  put \"un deux\" into v\n  put \"Z\" into word 0 of v\n  put v");
    execute("line -2", "  put \"a\" into v\n  put \"Z\" into line -2 of v\n  put v");

    puts("\n== 3. LE CAS QUI FAIT MAL : un rang calcule qui tombe a zero ==");
    execute("item (i-1) avec i valant 1",
            "  put \"a,b\" into v\n  put 1 into i\n"
            "  put \"X\" into item (i - 1) of v\n"
            "  put v & \"   (\" & the number of items of v & \" items)\"");

    puts("\n== 4. les plages aussi ==");
    execute("item 0 to 2",  "  put \"a,b,c\" into v\n"
                            "  put \"X\" into item 0 to 2 of v\n  put v");
    execute("item -1 to 1", "  put \"a,b,c\" into v\n"
                            "  put \"X\" into item -1 to 1 of v\n  put v");

    puts("\n== 5. LES RANGS LEGITIMES N'ONT PAS BOUGE ==");
    execute("remplacer un item existant",
            "  put \"a,b,c\" into v\n  put \"B\" into item 2 of v\n  put v");
    execute("etendre au-dela de la fin",
            "  put \"a,b\" into v\n  put \"X\" into item 5 of v\n  put v");
    execute("le tout premier",
            "  put \"a,b\" into v\n  put \"A\" into item 1 of v\n  put v");
    execute("une plage legitime",
            "  put \"a,b,c\" into v\n  put \"X\" into item 2 to 3 of v\n  put v");
    execute("char 1, word 1, line 1",
            "  put \"abc\" into v\n  put \"Z\" into char 1 of v\n  put v\n"
            "  put \"un deux\" into w\n  put \"UN\" into word 1 of w\n  put w\n"
            "  put \"l1\" into x\n  put \"L1\" into line 1 of x\n  put x");
    execute("etendre des lignes",
            "  put \"a\" into v\n  put \"c\" into line 3 of v\n"
            "  put the number of lines of v & \" lignes\"");

    hc_free(st);
    return 0;
}
