/* nomvide.c — une carte sans nom, et « go card "" ».
 *
 * Les cartes et les fonds neufs du menu naissent avec le nom "" (HC ne les
 * nomme plus « carte 2 » depuis le 17 octobre). Deux défauts en venaient,
 * relevés par la revue du 24 octobre (constat 4) :
 *
 *   · hc_describe ne testait que NULL : une carte au nom vide se disait
 *     card "", dans « the name », dans le menu Recent et dans « the recent
 *     cards » ;
 *   · « go card "" » trouvait la PREMIÈRE carte au nom vide, si bien que
 *     « go to line 2 of the recent cards » menait à la mauvaise carte.
 *
 * MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II) par l'utilisatrice, le 24
 * octobre, par un bouton qui range ses réponses dans un champ :
 *
 *   · « go card "" » ne trouve aucune carte, même quand la pile a des
 *     cartes sans nom ; the result vaut « No such card », SANS point, et
 *     le script CONTINUE sur la carte où il était (« H: No such card 3 3 ») ;
 *   · une carte sans nom — faite au menu, ou dont on a vidé le nom — se
 *     dit « card id 9789 » dans the name ET dans the short name.
 *
 * HC arrêtait le script sur « objet introuvable », et rendait vide le nom
 * court. NON MESURÉ : le nom court d'un fond, d'un bouton, d'un champ sans
 * nom ; « go bkgnd "" », qui dans HyperCard a CHANGÉ de carte sans rien
 * dire (« K:  4 »), pour une raison qu'on ne sait pas encore. */
#include "hc_core.h"
#include <stdio.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG || k == HC_ERR) printf("   %s\n", t); }

static void fais(const char *l)
{
    printf("%s\n", l);
    hc_do(l);
}

int main(void)
{
    static HcHost h;
    h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("R");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *a = hc_new_card(st, bg, "A");
    hc_new_card(st, bg, "B");
    hc_new_card(st, bg, "C");
    hc_new_card(st, bg, "D");
    hc_set_current_card(a);

    puts("== 1. deux cartes vidées de leur nom ==");
    fais("go to card \"B\"");
    fais("set the name of this card to empty");
    fais("put the name of this card");
    fais("put the short name of this card");
    fais("go to card \"C\"");
    fais("set the name of this card to empty");
    fais("go to card \"D\"");

    puts("\n== 2. the recent cards : chaque carte se désigne ==");
    fais("put the recent cards");
    fais("go to line 2 of the recent cards");      /* C, la carte 3 */
    fais("put the result & \" / \" & the number of this card");
    fais("go to card \"D\"");
    fais("go to line 3 of the recent cards");      /* B, la carte 2 */
    fais("put the result & \" / \" & the number of this card");

    puts("\n== 3. go card \"\" : aucune carte, et le texte d'HyperCard ==");
    fais("go to card \"D\"");
    fais("go card \"\"");
    fais("put the result & \" / \" & the number of this card");
    fais("go card \"Zut\"");
    fais("put the result & \" / \" & the number of this card");

    puts("\n== 4. dans un gestionnaire, le script continue ==");
    /* Le banc d'HyperCard était un mouseUp : la ligne d'après s'y exécute,
     * sur la carte de départ. */
    Object *b = hc_new_button(a, "aller");
    hc_set_script(b, "on mouseUp\n  go card \"Zut\"\n"
                     "  put \"suite : \" & the result && the short name of this card\n"
                     "end mouseUp");
    fais("go to card \"A\"");
    fais("send mouseUp to button \"aller\"");

    puts("\n== 5. la carte neuve des menus : juste après la courante ==");
    /* hc_new_card_after, que New Card et New Background appellent. MESURÉ
     * DANS HYPERCARD 2.4.1 : depuis la carte 2, la neuve est la 3. Un
     * `apres` absent de la pile la laisse à la fin. */
    fais("go to card 2");
    Object *neuve = hc_new_card_after(st, bg, "", hc_current_card());
    hc_set_current_card(neuve);
    fais("put the number of this card && the number of cards && the name of this card");
    Object *fond2 = hc_new_background(st, "");
    neuve = hc_new_card_after(st, fond2, "", hc_current_card());
    hc_set_current_card(neuve);
    fais("put the number of this card && the number of cards && the name of this bkgnd");
    neuve = hc_new_card_after(st, bg, "Fin", NULL);
    hc_set_current_card(neuve);
    fais("put the number of this card && the number of cards");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
