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

    /* ─── LE TRANSPORT D'ICONES, DEUX CHEMINS ET UN SEUL ETAIT COUVERT ──
     *
     * Copier une CARTE emportait ses icones : quand le numero est deja pris
     * dans la pile d'arrivee par un AUTRE dessin, on en prend un libre et l'on
     * reetiquette les boutons. Copier un BOUTON n'emportait rien du tout : ni
     * hc_copy_part ni hc_paste_part ne regardaient les icones.
     *
     * Le bouton colle gardait donc son numero, et a l'arrivee ce numero
     * designait un autre dessin. Mesure : le bouton affichait l'icone de la
     * pile de destination, et la sienne etait perdue.
     *
     * Un chemin sur deux, c'est le genre de moitie qui ne se voit pas —
     * jusqu'a ce qu'on copie un bouton. Les deux sont tenus ici. */
    puts("\n== transport d'icones vers une pile ou le numero est PRIS ==");
    {
        Object *B  = hc_new_stack("Arrivee"); hc_register_stack(B);
        Object *bB = hc_new_background(B, "fb");
        Object *cB = hc_new_card(B, bB, "cb");
        Object *deja = hc_new_button(cB, "DejaLa");
        remplis(hc_icon_add(B, 20554, "autre dessin"), 9);   /* MEME numero */
        deja->icon = 20554;

        /* la source : le bouton de la pile d'origine porte l'icone 20554 */
        b->icon = 20554;

        puts("   --- en copiant la CARTE ---");
        hc_set_current_card(c);
        hc_copy_card(c);
        hc_set_current_card(cB);
        Object *nc = hc_paste_card(B);
        for (int i = 0; nc && i < nc->nparts; i++) {
            Object *p = nc->parts[i];
            if (p->type != OBJ_BUTTON) continue;
            struct StackIcon *ic = hc_icon_get(B, p->icon);
            printf("      bouton [%s] -> icone %d : %s\n",
                   p->name ? p->name : "", p->icon,
                   ic ? (memes_bits(ic, 1) ? "SON dessin" : "le MAUVAIS dessin")
                      : "INTROUVABLE");
        }

        puts("   --- en copiant le BOUTON seul ---");
        hc_set_current_card(c);
        hc_copy_part(b);
        hc_set_current_card(cB);
        Object *nb = hc_paste_part(cB);
        if (nb) {
            struct StackIcon *ic = hc_icon_get(B, nb->icon);
            printf("      bouton [%s] -> icone %d : %s\n",
                   nb->name ? nb->name : "", nb->icon,
                   ic ? (memes_bits(ic, 1) ? "SON dessin" : "le MAUVAIS dessin")
                      : "INTROUVABLE");
        }

        /* Et l'icone qui etait deja la n'a pas bouge : transplanter ne doit
         * jamais abimer la pile d'arrivee. */
        {
            struct StackIcon *ic = hc_icon_get(B, deja->icon);
            printf("   l'icone d'origine de l'arrivee : %s\n",
                   (ic && memes_bits(ic, 9)) ? "intacte" : "ABIMEE");
        }
        hc_free(B);
    }

    hc_free(st2); hc_free(st); remove(fic);
    return 0;
}
