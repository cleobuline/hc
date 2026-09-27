/* LE NOM D'UN MENU, ET CE QU'IL ARRIVE QUAND IL EST LONG.
 *
 * DEUX AFFAIRES DISTINCTES, ET J'AVAIS CONFONDU LA BONNE AVEC LA MAUVAISE.
 *
 * 1. LA CRÉATION REFUSE, et elle a toujours refusé. « create menu » vérifie
 *    la longueur et rend « nom de menu trop long » ; rien n'est amputé. J'ai
 *    pourtant annoncé trois fois dans la journée « le nom d'un menu amputé à
 *    256 octets en silence », parce qu'un avertissement du compilateur le
 *    disait — sans jamais lire les huit lignes au-dessus, où le refus est
 *    écrit. Un avertissement n'est pas une mesure : c'est une QUESTION posée
 *    au code, et il faut aller lire la réponse.
 *
 * 2. LA SUPPRESSION, ELLE, ÉCHOUAIT VRAIMENT — et en annonçant la réussite.
 *    « delete menu "X" » tenait déjà le RANG du menu, et repassait pourtant
 *    par le NOM : il le recopiait dans soixante-quatre octets pour le
 *    redonner à une recherche. Au-delà de 63 caractères, le nom arrivait
 *    tronqué, ne correspondait plus à rien, et le menu survivait. « the
 *    result » restait vide, c'est-à-dire annonçait que tout s'était bien
 *    passé.
 *
 *        63 caractères -> supprimé
 *        64 caractères -> PAS supprimé, et rien ne le dit
 *
 * ET GCC LE DISAIT, À -O0 SEULEMENT. « output between 1 and 256 bytes into a
 * destination of size 64 ». La cible « make avertissements » compilait en
 * -O2, où cet avertissement n'apparaît pas : le défaut était couvert par le
 * niveau d'optimisation de sa propre porte. Elle compile désormais aux trois
 * niveaux.
 *
 * La recherche par nom n'avait pas d'autre appelant que ce détour ; en
 * supprimant le détour, elle est partie avec. Un chemin qui ne sert qu'à
 * revenir où l'on était est rarement innocent.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if      (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *b;

/* Un nom de `n` Z, créé puis supprimé. On ne montre que les COMPTES : le nom
 * lui-même ferait trois cents colonnes pour ne rien apprendre. */
static void cycle(int n)
{
    char nom[400];
    if (n > (int)sizeof nom - 1) n = (int)sizeof nom - 1;
    memset(nom, 'Z', (size_t)n);
    nom[n] = '\0';

    char s[1100];
    snprintf(s, sizeof s,
             "on t\n"
             "  create menu \"%s\"\n"
             "  put \"  cree   : \" & the number of menus & \" menu(s)\"\n"
             "  delete menu \"%s\"\n"
             "  put \"  detruit: \" & the number of menus & \" menu(s)"
             "   result=[\" & the result & \"]\"\n"
             "end t\n", nom, nom);
    printf("── un nom de %d caractères\n", n);
    hc_set_script(b, s);
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    b = hc_new_button(c, "B");

    puts("== 1. LE CYCLE COMPLET, de part et d'autre des 64 caractères ==");
    /* 63 et 64 encadrent l'ancien tampon : c'est là que ça basculait. Les
     * deux doivent maintenant finir à zéro menu. */
    cycle(10);
    cycle(63);
    cycle(64);
    cycle(200);

    puts("\n== 2. LA CRÉATION REFUSE AU-DELÀ DE 255, et le dit ==");
    cycle(255);
    cycle(256);
    cycle(300);

    puts("\n== 3. ET LE COMPTE RETOMBE BIEN À ZÉRO ==");
    /* Le garde-fou de tout le harnais : un menu qui aurait survécu à l'un des
     * cycles ci-dessus se verrait ici, et nulle part ailleurs. */
    hc_set_script(b, "on t\n  put \"menus restants : \" & the number of menus\nend t\n");
    hc_send(b, "t");

    hc_free(st);
    return 0;
}
