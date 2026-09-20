/* « FIRST FIELD » DESIGNAIT LA PREMIERE CARTE.
 *
 * SIGNALE A L'USAGE :
 *
 *     set the name of first field to "toto"   ->  renommait la CARTE
 *
 * Une ecriture qui se pose ailleurs que la ou on l'envoie, en SILENCE. En
 * lecture, « the name of first field » rendait « card "Une" » — une mauvaise
 * reponse, pas une erreur. Et « second field » disait « objet introuvable »,
 * ce qui rendait le defaut plus trompeur encore : la forme la plus courante,
 * first, etait justement celle qui mentait sans rien dire.
 *
 * DEUX CAUSES, UNE PAR MOTEUR, et c'est tout l'interet du cas :
 *
 *   - le resolveur v3 traitait HCT_DES_ORDINAL pour les fonds et pour les
 *     cartes, et PAS pour les boutons et les champs : le designateur tombait
 *     sur le `default`, hct_resout rendait NULL, et le pont repassait la
 *     phrase a l'ancien moteur ;
 *
 *   - l'ancien resolveur disait « first » -> premiere CARTE sans regarder le
 *     mot suivant. Or le bloc juste au-dessus faisait DEJA ce test pour les
 *     fonds : « first background » lit ce qui le suit depuis longtemps. La
 *     regle etait connue trois lignes plus haut.
 *
 * Corriger un seul des deux n'aurait rien donne : la LECTURE passe par v3, et
 * « set » reconstruit sa cible en texte pour l'ancien. Le meme mot aurait
 * designe deux objets differents selon le chemin.
 *
 * CE QUE CE HARNAIS TIENT : les deux chemins, la portee par couche — « last
 * field » n'a pas le meme sens sur la carte et sur le fond — et les treize
 * ordinaux, pas seulement first et last.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *pilote, *carte1, *fond;

static void execute(const char *corps)
{
    char s[2048];
    printf("   %s\n", corps);
    snprintf(s, sizeof s, "on essaie\n%s\nend essaie\n", corps);
    hc_set_script(pilote, s);
    hc_send(pilote, "essaie");
}

static void noms(const char *quoi)
{
    printf("   %-38s carte=\"%s\"", quoi, carte1->name ? carte1->name : "(nul)");
    for (int i = 0; i < carte1->nparts; i++)
        if (carte1->parts[i]->type == OBJ_FIELD)
            printf("  champ=\"%s\"", carte1->parts[i]->name ? carte1->parts[i]->name : "(nul)");
    printf("\n");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    fond   = hc_new_background(st, "Fond");
    carte1 = hc_new_card(st, fond, "Une");
    hc_new_card(st, fond, "Deux");
    hc_new_card(st, fond, "Trois");
    hc_set_current_card(carte1);
    hc_register_stack(st);

    pilote = hc_new_button(carte1, "pilote");
    hc_new_field(carte1, "alpha");
    hc_new_field(carte1, "beta");
    hc_new_field(carte1, "gamma");
    /* Le fond a SES champs, en nombre different : c'est ce qui fait que
     * « last field » ne peut pas se calculer une fois pour les deux. */
    hc_new_field(fond, "pied");
    hc_new_button(fond, "suivant");

    puts("== 1. LE CAS SIGNALE : l'ECRITURE ==");
    noms("au depart");
    execute("  set the name of first field to \"toto\"");
    noms("apres");
    /* La carte ne doit PAS avoir bouge : c'est la moitie qui compte. */

    puts("\n== 2. et la LECTURE, qui passe par l'autre moteur ==");
    execute("  put the name of first field");
    execute("  put the name of second field");
    execute("  put the name of third field");
    execute("  put the name of last field");
    execute("  put the name of middle field");

    puts("\n== 3. LES CARTES N'ONT RIEN PERDU ==");
    /* « first » tout seul designe toujours une carte — la forme des scripts
     * d'epoque — et les autres ordinaux marchent maintenant aussi. */
    execute("  put the name of first card");
    execute("  put the name of second card");
    execute("  put the name of last card");
    execute("  put the name of middle card");

    puts("\n== 4. LA PORTEE : le fond a sa propre numerotation ==");
    /* « last field » compte les champs de la COUCHE interrogee. Trois sur la
     * carte, un seul sur le fond : un total calcule une fois pour les deux
     * rendrait le mauvais objet, ou rien. */
    execute("  put the name of last card field");
    execute("  put the name of last bg field");
    execute("  put the name of first bg field");
    execute("  put the name of last bg button");

    puts("\n== 5. LE REPLI SUR LE FOND, SANS PORTEE ECRITE ==");
    /* Sans portee, on cherche sur la carte puis sur le fond. « first button »
     * n'existe que sur la carte ici (pilote) ; on verifie que le repli sert
     * quand la carte n'a pas de part de ce type. */
    execute("  put the name of first button");
    execute("  put the name of second button");

    puts("\n== 6. CE QUI N'EXISTE PAS SE DIT ==");
    execute("  put the name of tenth field");
    execute("  set the name of tenth field to \"raté\"");
    noms("rien n'a bouge");

    puts("\n== 7. L'ECRITURE SUR LE FOND ==");
    execute("  set the name of last bg field to \"bas de page\"");
    execute("  put the name of last bg field");
    noms("la carte est intacte");

    hc_free(st);
    return 0;
}
