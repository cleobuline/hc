/* Les références d'objet, et « the partNumber ».
 *
 * CE HARNAIS EST NÉ D'UNE ERREUR DE MA PART. Après la suppression de l'ancien
 * exécuteur de lignes, j'avais rangé « card field 1 », « titleWidth of me » et
 * « partNumber of me » dans la même case : « des références que la v3 ne sait
 * pas résoudre ». C'était faux pour deux d'entre elles.
 *
 *   card field 1        se résout très bien, et field 1 et bg field 1 aussi.
 *                       Elles apparaissaient dans le relevé des retours vers
 *                       l'ancien interprète pour une autre raison, pas parce
 *                       qu'elles échouaient. Ce harnais les fixe, pour que
 *                       personne — moi compris — ne les redéclare cassées.
 *
 *   partNumber          manquait vraiment. hc_part_number existait déjà et
 *                       servait l'interface ; le langage ne savait pas le
 *                       demander.
 *
 *   titleWidth          n'est pas un accesseur manquant : c'est la largeur de
 *                       la zone de titre d'un bouton popup, et le dessin n'a
 *                       aucune zone de titre. L'ajouter serait écrire cette
 *                       fonctionnalité, pas combler un trou. Laissé de côté,
 *                       sciemment. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *b;

static void essai(const char *corps)
{
    char s[400];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", corps);
    printf("── %-44s", corps);
    hc_set_script(b, s);
    hc_send(b, "t");
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("T"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "U");
    Object *f1 = hc_new_field(c, "un");   hc_set_field_text(f1, "texte du premier");
    Object *f2 = hc_new_field(c, "deux"); hc_set_field_text(f2, "texte du second");
    hc_new_field(bg, "fond1");
    b = hc_new_button(c, "B");
    hc_new_button(c, "C");
    hc_set_current_card(c);
    (void)f2;

    puts("=== le champ par son rang ===");
    essai("put card field 1");
    essai("put card field 2");
    essai("put field 1");
    essai("put the text of card field 1");
    essai("put card field \"un\"");
    essai("put the name of card field 2");
    essai("put the name of bg field 1");
    /* Un rang qui dépasse doit se voir, pas rendre du vide au hasard. */
    essai("put the name of card field 9");

    puts("\n=== the partNumber : le rang parmi TOUTES les parts ===");
    /* L'ordre de création est : champ un, champ deux, bouton B, bouton C.
     * partNumber les compte mêlés — c'est là toute la différence avec
     * « the number of card buttons », qui ne compterait que les boutons. */
    essai("put the partNumber of card field \"un\"");
    essai("put the partNumber of card field \"deux\"");
    essai("put the partNumber of card button \"B\"");
    essai("put the partNumber of card button \"C\"");
    essai("put the partNumber of me");
    essai("put partNumber of me");
    /* Un fond a sa propre liste de parts : le champ de fond y est le premier,
     * et non le cinquième de la carte. */
    essai("put the partNumber of bg field \"fond1\"");
    /* Une carte n'est pas une part : elle n'a pas de rang de part. */
    essai("put the partNumber of this card");

    puts("\n=== titleWidth reste inconnue, et le dit à sa façon ===");
    /* Un nom de propriété inconnu rend son propre texte : c'est la règle
     * d'HyperTalk pour tout nom non affecté. Le jour où titleWidth sera
     * écrite, cette ligne changera — et c'est précisément ce qu'on veut voir. */
    essai("put the titleWidth of me");

    hc_free(st);
    return 0;
}
