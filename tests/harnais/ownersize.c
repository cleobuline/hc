/* « the owner of » et « the size of » : deux proprietes qui rendaient leur
 * propre texte.
 *
 * Trouvees par le releve du corpus, dans la colonne des recours : « recours
 * of owner », « recours of size ». Le recours reconstituait le texte source,
 * l'ancien moteur ne connaissait pas ces noms non plus, et la vieille regle
 * « mot inconnu = son propre nom » rendait la chaine « owner of me ».
 *
 * SANS AUCUNE ERREUR. C'est ce qui rend ce defaut interessant : un script qui
 * ecrit « if the owner of me is the owner of target » comparait deux textes
 * identiques et repondait vrai quoi qu'il arrive. Le meme mecanisme faisait
 * autrefois passer « short name of o » pour un resultat.
 *
 * Ce que ce harnais tient :
 *
 *   1. le proprietaire suit la hierarchie de HyperCard, PAS notre chainage
 *      interne : le proprietaire d'une carte est son FOND, alors que chez
 *      nous card->owner est la pile ;
 *   2. les adjectifs short / long fonctionnent, et le defaut est le nom LONG ;
 *   3. une pile n'a pas de proprietaire, et le dit en rendant vide plutot
 *      qu'en se plaignant ;
 *   4. « the size of <pile> » rend la meme chose que la fonction du monde
 *      « the size » — deux ecritures d'une question, une seule reponse ;
 *   5. la taille suit le fichier : zero avant enregistrement, non nulle
 *      apres.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("%s\n", t ? t : "");
  else if (k == HC_ERR) printf("[ERR] %s\n", t ? t : ""); }

static Object *b;

static void e(const char *x)
{
    char s[400];
    snprintf(s, sizeof s, "on t\n  put %s\nend t\n", x);
    printf("   %-40s -> ", x);
    hc_set_script(b, s);
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_new_field(c, "champ");
    b = hc_new_button(c, "Bouton");
    hc_set_current_card(c);

    puts("== 1. la hierarchie de HyperCard ==");
    e("the owner of me");
    e("the owner of card field \"champ\"");
    /* Le point qui distingue les deux hierarchies : chez nous card->owner est
     * la PILE. Si cette ligne rend « stack "Pile" », le modele a pris le pas
     * sur HyperCard. */
    e("the owner of this card");

    puts("\n== 2. les adjectifs ==");
    e("the short owner of me");
    e("the long owner of me");
    e("the short owner of this card");

    puts("\n== 3. le haut de la hierarchie ==");
    e("the owner of this stack");

    puts("\n== 4. les deux ecritures de « the size » ==");
    e("the size of this stack");
    e("the size");

    puts("\n== 5. ce qui ne doit pas avoir bouge ==");
    e("the name of me");
    e("the short name of me");
    e("the long name of me");

    /* L'ENREGISTREMENT EN DERNIER, ET SOUS UN NOM FIXE.
     *
     * hc_save ADOPTE le chemin : la pile s'appelle ensuite comme son fichier.
     * Un nom tire par mkstemp rendait donc « the long name of me » different
     * a chaque execution, et la reference inverifiable. Tout ce qui lit un
     * nom passe donc AVANT, et le chemin est fixe.
     *
     * On n'imprime pas la taille elle-meme : elle depend de la mise en forme
     * du fichier et changerait la reference a chaque evolution du format. Ce
     * qui compte est qu'elle cesse d'etre nulle, et que les deux ecritures
     * s'accordent. */
    puts("\n== 6. la taille suit le fichier ==");
    const char *chemin = "/tmp/hc_ownersize.stack";
    remove(chemin);
    if (hc_save(st, chemin) == 0) {   /* 0 = succes */
        printf("   le fichier fait des octets                -> %s\n",
               hc_taille_fichier(chemin) > 0 ? "oui" : "NON");
        hc_set_script(b,
            "on t\n"
            "  if the size of this stack = the size then\n"
            "    put \"les deux ecritures s'accordent\"\n"
            "  else\n"
            "    put \"ELLES DIVERGENT\"\n"
            "  end if\n"
            "  if the size of this stack > 0 then\n"
            "    put \"taille non nulle\"\n"
            "  else\n"
            "    put \"TAILLE NULLE\"\n"
            "  end if\n"
            "end t\n");
        printf("   ");
        hc_send(b, "t");
        remove(chemin);
    } else {
        puts("   enregistrement impossible : rien mesure");
    }

    hc_free(st);
    return 0;
}
