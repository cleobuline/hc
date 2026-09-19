/* UN SCRIPT POUVAIT ECRASER LE TAS EN ECRIVANT DANS UN ITEM LOINTAIN.
 *
 * SIGNALE PAR UN AUDIT EXTERIEUR, reproduit sous ASan et UBSan. Quand on
 * ecrit dans un morceau au-dela de la fin, hct_chunk_ecrit ajoute les
 * separateurs manquants. Elle calculait la taille a reserver en `int` :
 *
 *     int taille = len + (besoin_sep + manquants) * lsep + lv;
 *
 * `manquants` monte jusqu'a HCT_RANG_MAX, soit 16 777 216, et `lsep` est une
 * CHAINE de longueur libre — « set the itemDelimiter to … » ne borne rien.
 * Le produit depasse donc INT_MAX, et un debordement signe n'est pas un
 * nombre faux : c'est un comportement indefini.
 *
 * LA MESURE, avec un delimiteur de 512 octets et « item 8388609 ». Les
 * chiffres ne sont pas pris au hasard : ils sont choisis pour que
 * (1 + manquants) x 512 vaille EXACTEMENT 2^32, donc zero une fois tronque.
 *
 *     hct_chunk.c:361 runtime error: signed integer overflow
 *     AddressSanitizer: heap-buffer-overflow
 *     WRITE of size 512 ... 0 bytes after 3-byte region
 *
 * `taille` retombait a trois octets, malloc reussissait, et la boucle
 * ecrivait quand meme ses huit millions de separateurs. Un script HyperTalk
 * pouvait ecraser le tas : la pire sorte de defaut, celui qui sort du
 * langage.
 *
 * QUE LES 208 HARNAIS PASSENT SOUS ASan NE PROUVAIT RIEN CONTRE LUI. Aucun ne
 * demandait ce cas. Un instrument ne voit que ce qu'on lui montre, et c'est
 * la troisieme fois ce mois-ci qu'il faut se le redire.
 *
 * Ce harnais tient :
 *
 *   1. le cas qui debordait : il doit rendre « memoire insuffisante » et le
 *      programme doit SURVIVRE. Sous --asan, un seul octet en trop rougirait
 *      la suite — c'est la que ce harnais gagne sa place ;
 *   2. deux autres combinaisons qui debordaient aussi, pour que la borne ne
 *      soit pas ajustee a un seul jeu de chiffres ;
 *   3. et surtout : les extensions RAISONNABLES marchent toujours. Un refus
 *      general passerait les points 1 et 2 sans rien valoir.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

/* `delim` peut etre long : on le fabrique ici plutot que dans le script,
 * pour que le harnais reste lisible.
 *
 * ET ON LE REPOSE A CHAQUE CAS, ldelim == 0 valant la virgule. Premiere
 * version : le « set » n'etait emis que pour ldelim > 0, et l'itemDelimiter
 * etant GLOBAL, les cas raisonnables de la section 3 heritaient des 256 Z du
 * cas precedent — « item 5 of "a,b" » rendait « a,b » suivi de mille Z, et
 * « a,b,c » comptait pour UN item. Le harnais mesurait l'etat laisse par son
 * voisin. */
static void execute(const char *titre, int ldelim, const char *corps)
{
    char delim[1024];
    if (ldelim > (int)sizeof delim - 1) ldelim = (int)sizeof delim - 1;
    if (ldelim > 0) { memset(delim, 'Z', (size_t)ldelim); delim[ldelim] = '\0'; }
    else            { delim[0] = ','; delim[1] = '\0'; }

    char s[4096];
    printf("   %s\n", titre);
    snprintf(s, sizeof s,
             "on essaie\n  set the itemDelimiter to \"%s\"\n%s\nend essaie\n",
             delim, corps);
    hc_set_script(b, s);
    hc_send(b, "essaie");
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
    b = hc_new_button(c, "B");

    puts("== 1. LE CAS QUI DEBORDAIT : refus net, et on revient vivant ==");
    /* (1 + 8388607) * 512 == 2^32 : la taille retombait a trois octets. */
    execute("delimiteur de 512 octets, item 8388609", 512,
            "  put \"a\" into v\n"
            "  put \"x\" into item 8388609 of v\n"
            "  put \"si on lit ceci, le tas est intact\"");

    puts("\n== 2. deux autres combinaisons du meme genre ==");
    /* (1 + 16777214) * 256 : depassement aussi, avec un reste different. */
    execute("delimiteur de 256 octets, item 16777216", 256,
            "  put \"a\" into v\n"
            "  put \"x\" into item 16777216 of v\n"
            "  put \"vivant\"");
    /* Sans delimiteur pose : les LIGNES, dont le separateur fait un octet.
     * 16 millions de lignes, c'est 16 Mo — legitime en taille, on verifie
     * seulement que la borne ne le refuse pas par exces de prudence. */
    execute("lignes, rang maximal, separateur d'un octet", 0,
            "  put \"a\" into v\n"
            "  put \"x\" into line 1000000 of v\n"
            "  put the number of lines of v");

    puts("\n== 3. LES EXTENSIONS RAISONNABLES MARCHENT TOUJOURS ==");
    execute("item 5 d'une valeur a deux items", 0,
            "  put \"a,b\" into v\n"
            "  put \"x\" into item 5 of v\n"
            "  put v");
    execute("avec un delimiteur multi-octets", 0,
            "  set the itemDelimiter to \"--\"\n"
            "  put \"a--b\" into v\n"
            "  put \"x\" into item 4 of v\n"
            "  put v");
    execute("ligne 3 d'une valeur a une ligne", 0,
            "  put \"a\" into v\n"
            "  put \"c\" into line 3 of v\n"
            "  put the number of lines of v & \" lignes\"");
    execute("remplacer un item qui EXISTE", 0,
            "  put \"a,b,c\" into v\n"
            "  put \"B\" into item 2 of v\n"
            "  put v");
    execute("un delimiteur long mais un rang petit", 64,
            "  put \"a\" into v\n"
            "  put \"x\" into item 3 of v\n"
            "  put the number of items of v & \" items, \" "
            "& the length of v & \" octets\"");

    hc_free(st);
    return 0;
}
