/* Les icones d'une pile survivent-elles a un enregistrement ?
 *
 * Elles ne survivaient plus, et c'est MOI qui l'ai casse. L'en-tete s'ecrit
 *
 *     iconres 20554 "Terminator"
 *
 * et se relisait par hc_entier(s + 8, ...), c'est-a-dire sur « 20554
 * "Terminator" » — un nombre SUIVI D'AUTRE CHOSE. Depuis que hc_entier refuse
 * ce qui traine derriere le nombre (« 42patate » ne vaut plus 42), il rendait
 * son defaut : zero. Toutes les icones d'une pile relue portaient donc le
 * numero 0, les boutons ne les retrouvaient plus, et l'editeur annoncait
 * « aucune icone ».
 *
 * C'est la MEME famille que les quatre lecteurs corriges avec coord_champ —
 * parse_ints, drag from, click at, la taille d'une plage de style. J'en avais
 * manque un, et il n'etait couvert par aucun harnais : il n'y en avait pas un
 * seul pour les icones. C'est la vraie lecon de ce defaut, et ce fichier la
 * tire.
 *
 * On verifie donc le CYCLE COMPLET, pas la seule lecture :
 *   - le numero, le nom et les 128 octets traversent l'aller-retour ;
 *   - un bouton retrouve SON icone par son numero ;
 *   - un numero negatif, un nom vide, un nom a guillemets passent aussi ;
 *   - deux icones ne se confondent pas.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

/* Un dessin reconnaissable, different pour chaque icone. */
static void remplis(struct StackIcon *ic, int graine)
{
    for (int i = 0; i < HC_ICON_BYTES; i++)
        ic->bits[i] = (unsigned char)((i * 7 + graine * 31) & 0xFF);
}

static int memes_bits(const struct StackIcon *ic, int graine)
{
    for (int i = 0; i < HC_ICON_BYTES; i++)
        if (ic->bits[i] != (unsigned char)((i * 7 + graine * 31) & 0xFF)) return 0;
    return 1;
}

static void montre(Object *st, int id, int graine)
{
    struct StackIcon *ic = hc_icon_get(st, id);
    if (!ic) { printf("   icone %-7d : INTROUVABLE\n", id); return; }
    printf("   icone %-7d : nom [%s]  octets %s\n",
           ic->id, ic->name ? ic->name : "(nul)",
           memes_bits(ic, graine) ? "intacts" : "ABIMES");
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    const char *fic = "/tmp/hc_icones.stack";
    remove(fic);

    Object *st = hc_new_stack("Pile"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    Object *b  = hc_new_button(c, "Bouton");
    hc_set_current_card(c);

    remplis(hc_icon_add(st, 20554, "Terminator"), 1);
    remplis(hc_icon_add(st, 3071,  "Close Box"),  2);
    remplis(hc_icon_add(st, -412,  ""),           3);   /* numero negatif, sans nom */
    remplis(hc_icon_add(st, 77,    "le \"vrai\" nom"), 4);  /* guillemets dans le nom */
    b->icon = 20554;

    puts("== avant enregistrement ==");
    printf("   nombre d'icones : %d\n", hc_icon_count(st));
    montre(st, 20554, 1); montre(st, 3071, 2);
    montre(st, -412, 3);  montre(st, 77, 4);
    printf("   icone du bouton : %d\n", b->icon);

    printf("   sauvegarde : %s\n", hc_save(st, fic) == 0 ? "faite" : "ECHEC");

    /* On relit dans une pile NEUVE : c'est le seul moyen de savoir ce que le
     * fichier contient vraiment, plutot que ce que la memoire a garde. */
    Object *st2 = hc_load(fic);
    puts("\n== apres relecture ==");
    if (!st2) { puts("   ECHEC DE RELECTURE"); return 1; }
    printf("   nombre d'icones : %d\n", hc_icon_count(st2));
    montre(st2, 20554, 1); montre(st2, 3071, 2);
    montre(st2, -412, 3);  montre(st2, 77, 4);

    /* Et le bouton doit retrouver la SIENNE. */
    Object *c2 = st2->nparts ? NULL : NULL;
    for (int i = 0; i < st2->nparts; i++)
        if (st2->parts[i]->type == OBJ_CARD) { c2 = st2->parts[i]; break; }
    Object *b2 = NULL;
    for (int i = 0; c2 && i < c2->nparts; i++)
        if (c2->parts[i]->type == OBJ_BUTTON) { b2 = c2->parts[i]; break; }
    printf("   icone du bouton : %d\n", b2 ? b2->icon : -1);
    printf("   et elle existe  : %s\n",
           (b2 && hc_icon_get(st2, b2->icon)) ? "oui" : "NON");

    /* L'ordre du registre, pour qu'une confusion de numeros se voie. */
    puts("\n== le registre, dans l'ordre ==");
    for (int i = 0; i < hc_icon_count(st2); i++) {
        struct StackIcon *ic = hc_icon_at(st2, i);
        printf("   [%d] id=%d nom=[%s]\n", i, ic->id, ic->name ? ic->name : "(nul)");
    }

    hc_free(st2); hc_free(st); remove(fic);
    return 0;
}
