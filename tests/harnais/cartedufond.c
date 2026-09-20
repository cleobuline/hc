/* « CARD 1 OF BG 2 » RENDAIT LA PREMIÈRE CARTE DE LA PILE.
 *
 * SIGNALE A L'USAGE — « go card 1 of bg 2 ne fonctionne pas ». Il faisait
 * pire que ne pas fonctionner : il rendait une carte, la MAUVAISE, sans le
 * moindre message.
 *
 * « card 1 of bg 2 » designe la premiere carte QUI UTILISE LE FOND 2. Les
 * cinq recherches de carte ignoraient ce « of » : elles indexaient dans la
 * pile entiere. Mesure, sur une pile A B C D dont A et B sont sur le fond 1
 * et C et D sur le fond 2 (le harnais, lui, les entrelace — voir plus bas) :
 *
 *     card 1 of bg 2      -> A     (c'est C)
 *     card 2 of bg 2      -> B     (c'est D)
 *     go card 1 of bg 2   -> reste sur A
 *
 * LE COMPTAGE, LUI, ETAIT JUSTE. « the number of cards of bg 2 » rendait
 * bien 2 : une boucle ecrite a la main, a un seul endroit, qui savait ce que
 * les cinq resolveurs ignoraient. C'est le signe habituel — une regle connue
 * d'un seul site est une regle que les autres n'appliquent pas. Les cinq
 * passent desormais par les memes aides que ce comptage.
 *
 * LE FOND NE RESTREINT QUE S'IL EST NOMME, et c'est le point delicat. La
 * variable `bg` du resolveur vaut par defaut le fond de la carte courante ;
 * s'en servir aurait fait de « card 1 » la premiere carte du fond courant,
 * ce qui n'est pas ce que dit HyperTalk :
 *
 *     card 1            la premiere carte de la PILE
 *     card 1 of bg 2    la premiere carte QUI UTILISE le fond 2
 *
 * LE JUMEAU, trouve avant de commiter cette fois. La v3 corrigee,
 * « card "A" of bg 2 » — A etant sur le fond 1 — etait refuse par elle et
 * TROUVE par l'ancien moteur, qui se contentait de se POSER sur une carte du
 * fond avant de chercher dans toute la pile. Se poser suffit pour un champ
 * de fond, qui est le meme partout ; pas pour designer une carte. L'ancien
 * moteur retient donc la portee, comme la v3.
 *
 * Ce harnais tient les cinq designateurs, les deux ecritures du fond, la
 * portee imbriquee, le refus d'une carte d'un autre fond — et surtout que
 * SANS fond nomme rien n'a change.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

static void execute(const char *corps)
{
    char s[2048];
    printf("   %s\n", corps);
    snprintf(s, sizeof s, "on essaie\n%s\nend essaie\n", corps);
    hc_set_script(b, s);
    hc_send(b, "essaie");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    /* LES DEUX FONDS SONT ENTRELACES, et ce n'est pas un detail. L'ordre
     * dans la pile est A C B D, soit :
     *
     *     place dans la pile   1   2   3   4
     *     carte                A   C   B   D
     *     fond                 Un  Deux Un Deux
     *
     * si bien que le rang DANS LE FOND et le rang DANS LA PILE ne coincident
     * pour aucune carte au-dela de la premiere. Avec les deux fonds en blocs
     * contigus — A B puis C D — « card 1 of bg 1 » tomberait juste par
     * hasard, et le harnais aurait passe avant comme apres la correction sur
     * la moitie de ses cas. */
    Object *st = hc_new_stack("Pile");
    Object *f1 = hc_new_background(st, "Un");
    Object *f2 = hc_new_background(st, "Deux");
    Object *cA = hc_new_card(st, f1, "A");
    Object *cC = hc_new_card(st, f2, "C");
    Object *cB = hc_new_card(st, f1, "B");
    Object *cD = hc_new_card(st, f2, "D");
    hc_set_current_card(cA);
    hc_register_stack(st);

    /* Un champ de CARTE homonyme sur chacune : il dit sur laquelle on est
     * arrive, ce qu'un nom de carte seul ne prouverait pas pour les portees
     * imbriquees. */
    struct { Object *c; const char *t; } champs[] = {
        { cA, "sur A" }, { cC, "sur C" }, { cB, "sur B" }, { cD, "sur D" } };
    for (int i = 0; i < 4; i++)
        hc_set_field_text(hc_new_field(champs[i].c, "X"), champs[i].t);

    b = hc_new_button(cA, "B");

    puts("== 1. le rang, dans le fond nomme ==");
    execute("  put the short name of card 1 of bg 2");
    execute("  put the short name of card 2 of bg 2");
    execute("  put the short name of card 1 of bg 1");
    execute("  put the short name of card 2 of bg 1");

    puts("\n== 2. LE CAS SIGNALE : go ==");
    execute("  go card 1 of bg 2\n"
            "  put \"arrive sur : \" & the short name of this card");
    execute("  go to card 2 of background \"Un\"\n"
            "  put \"arrive sur : \" & the short name of this card");
    execute("  go card 1\n  put \"retour sur : \" & the short name of this card");

    puts("\n== 3. les ordinaux ==");
    execute("  put the short name of first card of bg 2");
    execute("  put the short name of second card of bg 2");
    execute("  put the short name of last card of bg 2");
    execute("  put the short name of last card of bg 1");

    puts("\n== 4. par nom, par identifiant ==");
    execute("  put the short name of card \"C\" of bg 2");
    execute("  put the id of card 1 of bg 2 = the id of card \"C\"");
    /* A est sur le fond 1 : la demander dans le fond 2 doit ECHOUER, et
     * c'est le cas qui distinguait les deux moteurs. */
    execute("  put the short name of card \"A\" of bg 2");

    puts("\n== 5. la portee imbriquee : un champ d'une carte d'un fond ==");
    execute("  put field \"X\" of card 1 of bg 2");
    execute("  put field \"X\" of card 2 of bg 1");
    execute("  put field \"X\" of last card of bg 2");

    puts("\n== 6. le comptage, qui lui etait deja juste ==");
    execute("  put the number of cards of bg 1 & \" et \" "
            "& the number of cards of bg 2");
    execute("  put the number of cards");

    puts("\n== 7. SANS FOND NOMME, RIEN N'A CHANGE ==");
    /* La moitie qui coute : restreindre par le fond COURANT ferait de
     * « card 1 » la premiere carte du fond de la carte ou l'on est. */
    execute("  go card 3\n  put \"on est sur : \" & the short name of this card");
    execute("  put the short name of card 1");
    execute("  put the short name of card 4");
    execute("  put the short name of first card");
    execute("  put the short name of last card");
    execute("  put field \"X\" of card 1");

    hc_free(st);
    return 0;
}
