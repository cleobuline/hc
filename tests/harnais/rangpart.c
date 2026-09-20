/* LE RANG D'UNE PART NE SE POSAIT PAS.
 *
 * SIGNALE A L'USAGE :
 *
 *     set the partNumber of button "pin pon" to 1
 *     -> « propriete inconnue : partNumber »
 *
 * Le message etait faux : partNumber EXISTE, et se LIT depuis toujours. Il
 * envoyait donc chercher du cote du nom de la propriete, le seul endroit ou
 * il n'y avait rien. C'est la quatrieme fois qu'on corrige cette forme-la —
 * apres « Can't understand » pose sur un objet manquant, « propriete
 * inconnue » pose sur une cible absente, et le triple diagnostic des menus.
 *
 * ET LE REFUS SE JUSTIFIAIT PAR UNE COMMANDE ABSENTE. Le commentaire de la
 * lecture disait : « le rang ne se pose pas, il se constate ; le changer
 * serait le travail de send farther et de ses voisins ». Or ni « Send
 * Farther » ni « Bring Closer » n'existent dans ce projet — mesure faite. Il
 * n'y avait donc AUCUN moyen de changer l'ordre de superposition : un bouton
 * pose sous un champ y restait pour toujours.
 *
 * CE QUE CE HARNAIS TIENT, ce n'est pas que la commande « ne dit plus
 * d'erreur » — un « ne rien faire en silence » passerait ce test-la — mais
 * que l'ORDRE CHANGE, dans les deux sens, et que les autres parts gardent
 * leur ordre relatif. Plus les cas ou il faut refuser, et la survie de
 * l'ordre a un aller-retour par le fichier : un plan qui se remettrait a
 * plat au rechargement ne servirait a rien.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

#define FIC "/tmp/hc_rangpart.stack"

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *pilote;

static void execute(const char *corps)
{
    char s[2048];
    printf("   %s\n", corps);
    snprintf(s, sizeof s, "on essaie\n%s\nend essaie\n", corps);
    hc_set_script(pilote, s);
    hc_send(pilote, "essaie");
}

/* L'ordre des parts, en clair. C'est la seule chose qui compte ici. */
static void ordre(const char *quoi, Object *couche)
{
    printf("   %-34s", quoi);
    for (int i = 0; i < couche->nparts; i++) {
        Object *p = couche->parts[i];
        printf(" %d:%s", hc_part_number(p), p->name ? p->name : "(sans nom)");
    }
    printf("\n");
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

    pilote = hc_new_button(c, "pilote");
    hc_new_field(c, "champ");
    hc_new_button(c, "pin pon");
    hc_new_field(c, "note");

    puts("== 1. LE CAS SIGNALE ==");
    ordre("au depart", c);
    execute("  put the partNumber of button \"pin pon\"");
    execute("  set the partNumber of button \"pin pon\" to 1");
    ordre("apres le deplacement", c);

    puts("\n== 2. LES DEUX SENS, ET L'ORDRE RELATIF DES AUTRES ==");
    /* Une rotation, pas un echange : les parts qu'on n'a pas nommees gardent
     * leur ordre entre elles. Un echange avec l'occupant du rang vise
     * bousculerait un troisieme objet qui n'a rien demande. */
    execute("  set the partNumber of button \"pin pon\" to 4");
    ordre("« pin pon » renvoye au rang 4", c);
    execute("  set the partNumber of field \"note\" to 2");
    ordre("« note » amene au rang 2", c);

    puts("\n== 3. LE RANG EST ECRETE, PAS REFUSE ==");
    /* « set the partNumber to 999 » pour passer au-dessus de tout est
     * l'idiome courant. Le refuser casserait ce qu'on vient d'ajouter. */
    execute("  set the partNumber of button \"pilote\" to 999\n"
            "  put the partNumber of button \"pilote\"");
    ordre("« pilote » envoye a 999", c);
    execute("  set the partNumber of button \"pilote\" to 0\n"
            "  put the partNumber of button \"pilote\"");
    ordre("« pilote » envoye a 0", c);
    execute("  set the partNumber of button \"pilote\" to -5\n"
            "  put the partNumber of button \"pilote\"");
    ordre("« pilote » envoye a -5", c);

    puts("\n== 4. CE QU'IL FAUT REFUSER ==");
    /* Un nombre, ou rien. Ecreter « patate » a 1 rangerait la part quelque
     * part sans que personne l'ait demande. */
    execute("  set the partNumber of button \"pin pon\" to \"patate\"\n"
            "  put the result");
    ordre("l'ordre n'a pas bouge", c);
    execute("  set the partNumber of this card to 1\n"
            "  put the result");
    execute("  set the partNumber of this background to 1");
    execute("  set the partNumber of this stack to 1");

    puts("\n== 5. LE FOND A SA PROPRE NUMEROTATION ==");
    /* « bg field 1 » et « card field 1 » sont deux objets : le rang est
     * PROPRE AU PROPRIETAIRE. Deplacer une part de fond ne touche pas la
     * carte, et reciproquement. */
    hc_new_field(bg, "entete");
    hc_new_button(bg, "suivant");
    hc_new_field(bg, "pied");
    ordre("le fond au depart", bg);
    execute("  set the partNumber of bg button \"suivant\" to 1");
    ordre("« suivant » amene au rang 1", bg);
    ordre("la carte n'a pas bouge", c);

    puts("\n== 6. L'AUTRE COMPTEUR SUIT ==");
    /* hc_object_number compte parmi les objets DE MEME TYPE, hc_part_number
     * parmi toutes les parts. Deplacer une part change les deux, et les
     * confondre fait ecrire des scripts qui ne designent pas ce qu'on croit. */
    execute("  put \"note est le field\" && the number of card field \"note\"");
    execute("  put \"champ est le field\" && the number of card field \"champ\"");
    /* On fait passer « champ » DEVANT « note » : c'est le seul deplacement
     * qui echange leurs numeros de champ. Ma premiere version bougeait une
     * part qui etait deja du bon cote, et affichait donc deux fois la meme
     * chose en ayant l'air de prouver quelque chose. */
    execute("  set the partNumber of field \"champ\" to 1");
    ordre("« champ » amene au rang 1", c);
    execute("  put \"note est le field\" && the number of card field \"note\"");
    execute("  put \"champ est le field\" && the number of card field \"champ\"");
    puts("\n== 7. « PART N » NE TRIAIT PAS, IL PRENAIT LES CHAMPS ==");
    /* Defaut trouve en ecrivant ce harnais, et bien pire que celui qu'on
     * corrigeait : « part 1 » rendait le premier CHAMP. Une mauvaise reponse,
     * pas une erreur — le script recevait un objet, et le mauvais. Au-dela du
     * nombre de champs, il disait « objet introuvable » alors que la part
     * existait.
     *
     * Le COMPTAGE, lui, savait deja compter les parts melees. Une regle
     * connue d'un seul site, que les autres n'appliquaient pas. Le
     * deplacement d'une part ne servirait pas a grand-chose si le designateur
     * qui les nomme rendait autre chose. */
    ordre("l'ordre courant", c);
    /* « the number of parts » sans portee compte la carte ET le fond — c'est
     * le comportement de longue date, et non un effet de ce qu'on corrige :
     * quatre parts de carte plus trois de fond. */
    execute("  put the number of parts");
    execute("  put the name of part 1");
    execute("  put the name of part 2");
    execute("  put the name of part 3");
    execute("  put the name of part 4");
    /* Le nom et l'identifiant prenaient le meme seau que le rang : les trois
     * recherches sont corrigees ensemble, sinon la divergence renaitrait un
     * cran plus bas. */
    execute("  put the name of part \"pin pon\"");
    execute("  put the partNumber of part \"pin pon\"");

    puts("\n== 8. CE DONT « BRING CLOSER » ET « SEND FARTHER » DEPENDENT ==");
    /* Les deux articles du menu Objets ne font qu'un pas de +1 ou -1 sur le
     * rang. Leur seule subtilite est aux EXTREMITES : la part du dessus ne
     * peut pas monter, celle du dessous ne peut pas descendre. L'article se
     * grise alors, mais « doMenu "Bring Closer" » ne passe PAS par la
     * validation — il doit donc rester sans effet plutot que de deborder, et
     * c'est l'ecretage du noyau qui le garantit.
     *
     * Le menu vit dans du Objective-C que cette suite ne compile pas. Le
     * CONTRAT sur lequel il s'appuie, lui, se tient ici. */
    {
        Object *dessus = c->parts[c->nparts - 1];
        Object *dessous = c->parts[0];
        printf("   %-34s %s au rang %d\n", "la part du dessus",
               dessus->name, hc_part_number(dessus));
        hc_set_part_number(dessus, hc_part_number(dessus) + 1);
        printf("   %-34s rang %d\n", "un pas de plus vers le haut",
               hc_part_number(dessus));
        printf("   %-34s %s au rang %d\n", "la part du dessous",
               dessous->name, hc_part_number(dessous));
        hc_set_part_number(dessous, hc_part_number(dessous) - 1);
        printf("   %-34s rang %d\n", "un pas de plus vers le bas",
               hc_part_number(dessous));
        ordre("rien n'a bouge", c);
    }

    puts("\n== 9. ET L'ORDRE SURVIT AU FICHIER ==");
    /* Un plan qui se remettrait a plat au rechargement ne servirait a rien.
     * L'ordre n'est ecrit nulle part en toutes lettres : c'est l'ordre des
     * lignes du .stack qui le porte. */
    {
        if (hc_save(st, FIC) != 0) { puts("   !! echec de l'enregistrement"); return 1; }
        hc_free(st);
        Object *rl = hc_load(FIC);
        if (!rl) { puts("   !! echec du chargement"); return 1; }
        hc_register_stack(rl);
        for (int i = 0; i < rl->nparts; i++)
            if (rl->parts[i]->type == OBJ_CARD) { ordre("apres aller-retour", rl->parts[i]); break; }
        for (int i = 0; i < rl->nparts; i++)
            if (rl->parts[i]->type == OBJ_BACKGROUND) { ordre("le fond aussi", rl->parts[i]); break; }
        hc_free(rl);
        remove(FIC);
    }
    return 0;
}
