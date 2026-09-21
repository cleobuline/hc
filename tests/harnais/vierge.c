/* « Cette pile a-t-elle servi ? » — le test qui autorise l'interface à
 * remplacer la pile « Sans titre » du démarrage.
 *
 * SIGNALÉ À L'USAGE : « quand on ouvre HC on a une pile Sans titre, c'est
 * très bien, sauf que si j'ouvre une autre pile la pile Sans titre
 * disparaît ». Elle disparaissait même pleine : le test ne comptait que les
 * CARTES, si bien qu'une pile d'UNE carte où l'on avait tout dessiné passait
 * pour vide et partait à hc_free — sans confirmation ni message.
 *
 * Le test vivait dans AppDelegate.m, en DEUX exemplaires. Ici il se mesure,
 * et il n'y en a plus qu'un. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_ERR) printf("   [ERR] %s\n", t); }

static void dis(const char *quoi, Object *st)
{ printf("   %-42s %s\n", quoi, hc_stack_vierge(st) ? "vierge" : "A SERVI"); }

/* Une pile neuve, comme celle du démarrage : un fond, une carte, rien. */
static Object *neuve(const char *nom, Object **fond, Object **carte)
{
    Object *st = hc_new_stack(nom);
    *fond  = hc_new_background(st, "Fond");
    *carte = hc_new_card(st, *fond, "Une");
    return st;
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne;
    hc_set_host(&h);

    Object *f, *c, *st;

    printf("=== 1. la pile du démarrage : un fond, une carte, rien dessus ===\n");
    st = neuve("Sans titre", &f, &c);
    dis("neuve", st);

    printf("=== 2. ce qui compte pour une trace ===\n");
    st = neuve("A", &f, &c); hc_new_button(c, "b");
    dis("un bouton sur la carte", st);

    st = neuve("B", &f, &c); hc_new_field(c, "ch");
    dis("un champ sur la carte", st);

    st = neuve("C", &f, &c); hc_new_button(f, "b");
    dis("un bouton sur le FOND", st);

    st = neuve("D", &f, &c); hc_set_script(st, "on openStack\nend openStack\n");
    dis("un script de pile", st);

    st = neuve("E", &f, &c); hc_set_script(c, "on mouseUp\nend mouseUp\n");
    dis("un script de carte", st);

    st = neuve("F", &f, &c); hc_set_script(f, "on mouseUp\nend mouseUp\n");
    dis("un script de FOND", st);

    st = neuve("G", &f, &c); hc_set_paint(c, "iVBORw0KGgo=");
    dis("de la peinture sur la carte", st);

    st = neuve("H", &f, &c); hc_set_paint(f, "iVBORw0KGgo=");
    dis("de la peinture sur le FOND", st);
    printf("   (le fond compte autant : on peut avoir tout dessiné\n");
    printf("    dessus sans jamais toucher la carte)\n");

    st = neuve("I", &f, &c); hc_new_card(st, f, "Deux");
    dis("une seconde carte", st);

    printf("=== 3. ce qui NE compte pas — et c'est voulu ===\n");
    st = neuve("J", &f, &c);
    /* Renommer PAR SCRIPT, puis retirer le script : sans quoi c'est le
     * script lui-même qui ferait la trace, et la mesure ne dirait rien. */
    hc_set_current_card(c);
    hc_set_script(st, "on ren\n  set the name of this stack to \"Mes essais\"\nend ren\n");
    hc_send(c, "ren");
    hc_set_script(st, "");
    printf("   (nom de la pile : « %s »)\n", st->name ? st->name : "");
    dis("la pile renommée, rien d'autre", st);
    printf("   (on ne sait pas dire quel nom est « celui par défaut » sans\n");
    printf("    le supposer ; dans le doute on ne retient pas ce signal)\n");

    printf("=== 4. un script VIDE n'est pas un script ===\n");
    st = neuve("K", &f, &c); hc_set_script(st, "");
    dis("script de pile vide", st);
    printf("   (sans quoi ouvrir puis refermer l'éditeur de script suffirait\n");
    printf("    à rendre une pile ineffaçable)\n");

    printf("=== 5. le cas dégénéré ===\n");
    printf("   %-42s %s\n", "NULL", hc_stack_vierge(NULL) ? "vierge" : "A SERVI");
    printf("   (NULL n'est pas une pile vierge : rendre « vierge » ici\n");
    printf("    autoriserait l'appelant à remplacer ce qu'il n'a pas)\n");
    return 0;
}
