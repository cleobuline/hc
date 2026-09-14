/* Un texte entre GUILLEMETS est UN mot.
 *
 * En HyperTalk, « un mot est une suite de caracteres sans espace, OU un texte
 * entre guillemets ». Ce n'est pas un detail de confort : l'idiome canonique
 * pour qu'une pile retrouve son propre nom en depend, et c'est exactement ce
 * qu'ecrit le script de pile de Graph Maker 2.2 :
 *
 *     function wrongStack
 *       get the value of word 2 of the long name of me
 *       return (it is not line 1 of the stacks)
 *     end wrongStack
 *
 * « the long name of me » rend « stack "Graph Maker" ». Sans la regle, word 2
 * valait « "Graph » — un guillemet ouvert — et « the value of » refusait
 * ensuite avec « guillemet fermant manquant ». wrongStack() rendait donc VRAI
 * dans sa propre pile, et openStack passait la main sans rien faire.
 *
 * Mesure avant correction :
 *
 *     the number of words of 'stack "Graph Maker"'   3   au lieu de 2
 *     word 2                                         "Graph
 *     wrongStack()                                   true  au lieu de false
 *
 * Le guillemet n'ouvre un mot que s'il le COMMENCE, et un guillemet non
 * referme emporte le reste de la chaine — la seule reponse qui ne coupe pas le
 * texte au milieu d'une citation. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    Object *st = hc_new_stack("Graph Maker"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    Object *d  = hc_new_button(c, "decl");

    hc_set_script(st,
        "function wrongStack\n"
        "  get the value of word 2 of the long name of me\n"
        "  return (it is not line 1 of the stacks)\n"
        "end wrongStack\n");

    hc_set_script(d,
        "on mouseUp\n"
        "  put \"== compter les mots ==\"\n"
        "  put \"stack \" & quote & \"Graph Maker\" & quote into ln\n"
        "  put \"[\" & ln & \"]  -> \" & the number of words of ln & \" mots\"\n"
        "  put \"  word 1 = [\" & word 1 of ln & \"]\"\n"
        "  put \"  word 2 = [\" & word 2 of ln & \"]\"\n"
        "  put \"a b c\" into s1\n"
        "  put \"[\" & s1 & \"]  -> \" & the number of words of s1 & \" mots\"\n"
        "  put \"a \" & quote & \"b c\" & quote & \" d\" into s2\n"
        "  put \"[\" & s2 & \"]  -> \" & the number of words of s2 & \" mots\"\n"
        "  put \"  word 2 = [\" & word 2 of s2 & \"]\"\n"
        "  put \"  word 3 = [\" & word 3 of s2 & \"]\"\n"

        "  put \"== le guillemet n'ouvre un mot que s'il le COMMENCE ==\"\n"
        "  put \"abc\" & quote & \"def ghi\" & quote into s3\n"
        "  put \"[\" & s3 & \"]  -> \" & the number of words of s3 & \" mots\"\n"
        "  put \"  word 1 = [\" & word 1 of s3 & \"]\"\n"
        "  put \"  word 2 = [\" & word 2 of s3 & \"]\"\n"

        "  put \"== un guillemet NON REFERME emporte le reste ==\"\n"
        "  put \"a \" & quote & \"b c d\" into s4\n"
        "  put \"[\" & s4 & \"]  -> \" & the number of words of s4 & \" mots\"\n"
        "  put \"  word 2 = [\" & word 2 of s4 & \"]\"\n"

        "  put \"== une citation vide, et une citation seule ==\"\n"
        "  put \"a \" & quote & quote & \" b\" into s5\n"
        "  put \"[\" & s5 & \"]  -> \" & the number of words of s5 & \" mots\"\n"
        "  put \"  word 2 = [\" & word 2 of s5 & \"]\"\n"
        "  put quote & \"tout seul\" & quote into s6\n"
        "  put \"[\" & s6 & \"]  -> \" & the number of words of s6 & \" mots\"\n"

        "  put \"== ce qui doit suivre : supprimer et ecrire un mot ==\"\n"
        "  put ln into v\n"
        "  delete word 2 of v\n"
        "  put \"  apres delete word 2 : [\" & v & \"]\"\n"
        "  put ln into w\n"
        "  put \"X\" into word 2 of w\n"
        "  put \"  apres put X into word 2 : [\" & w & \"]\"\n"

        "  put \"== et l'idiome canonique, tel que Graph Maker l'ecrit ==\"\n"
        "  put \"  the long name of this stack = [\" & the long name of this stack & \"]\"\n"
        "  put \"  word 2                      = [\" & word 2 of the long name of this stack & \"]\"\n"
        "  put \"  the value of word 2         = [\" & the value of word 2 of the long name of this stack & \"]\"\n"
        "  put \"  line 1 of the stacks        = [\" & line 1 of the stacks & \"]\"\n"
        "  put \"  wrongStack()                = \" & wrongStack()\n"
        "end mouseUp\n");
    hc_send(d, "mouseUp");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
