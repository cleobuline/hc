/* LE DÉSIGNATEUR CALCULÉ — cinq commandes, et un témoin négatif.
 *
 * L'idiome du manuel, mot pour mot :
 *
 *     find "bet"
 *     if the result is empty then select the foundChunk
 *
 * Il rendait « ne sait pas faire ». Le désignateur n'existe que pour être
 * relu — c'est ce qu'on a écrit en corrigeant foundChunk, qui rend la couche
 * et le numéro justement pour que « select » retrouve le bon champ — et il
 * n'était relisible par personne.
 *
 * LA CAUSE : v3_cmd_select ne regardait que la FORME du nœud. Un morceau
 * écrit en clair — « select char 7 to 10 of card field 1 » — passait ; une
 * LECTURE dont la valeur est ce même texte rendait la main à l'ancien
 * interprète, qui n'a pas de « select » du tout. D'où un refus, et non un
 * silence : c'est ce qui a permis de le voir du premier coup.
 *
 * Trouvé par la pile de torture (harnais torture2, section 7), qui ne compare
 * pas HC à HyperCard mais HC à LUI-MÊME : deux tournures qui posent la même
 * question doivent rendre la même chose.
 *
 * §4 est le TÉMOIN NÉGATIF : un mot nu qui ne désigne rien doit continuer de
 * se faire refuser. Une correction qui accepte tout n'aurait rien corrigé.
 *
 * §6 À §8 SONT VENUS APRÈS, ET DE LA MESURE. Le banc joué dans Basilisk II le
 * 27/09 a répondu pour les quatre AUTRES commandes qui prennent un
 * désignateur, et il a démenti l'hypothèse prudente qu'on gardait :
 *
 *     put "card field 1" into ch
 *     hide ch                          -> HyperCard CACHE le champ
 *     find "bet" in ch                 -> HyperCard TROUVE
 *     set the textStyle of ch to bold   -> HyperCard POSE le style
 *
 * HC rendait « ne sait pas faire », « Not found » et « objet introuvable ».
 * Le deuxième était le plus vicieux : une recherche restreinte à un champ
 * introuvable ne fouille rien et répond comme si le mot n'y était pas.
 *
 * « go to ch » et « the name of ch » marchaient déjà : ils sont ici comme
 * TÉMOINS POSITIFS, pour qu'une régression sur eux se voie aussi.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("%s\n", t);
    else if (k == HC_ERR) printf("[ERR] %s\n", t);
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("S");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Un");
    hc_new_field(c, "A");          /* card field 1 */
    hc_new_field(bg, "T");         /* bkgnd field 1 */
    Object *b  = hc_new_button(c, "B");
    hc_set_current_card(c);

    hc_set_script(b,
        "on mouseUp\n"
        "  put \"alpha beta gamma\" into card field \"A\"\n"
        "  put \"sigma tau\" into bkgnd field \"T\"\n"
        "\n"
        "  put \"-- 1. l'idiome du manuel --\"\n"
        "  find \"bet\"\n"
        "  if the result is empty then select the foundChunk\n"
        "  put \"foundChunk    [\" & the foundChunk & \"]\"\n"
        "  put \"selectedChunk [\" & the selectedChunk & \"]\"\n"
        "  put \"selectedText  [\" & the selectedText & \"]\"\n"
        "\n"
        "  put \"-- 2. par une variable --\"\n"
        "  select empty\n"
        "  put the foundChunk into ou\n"
        "  select ou\n"
        "  put \"selectedChunk [\" & the selectedChunk & \"]\"\n"
        "\n"
        "  put \"-- 3. la couche du fond survit au detour --\"\n"
        "  select word 2 of bkgnd field \"T\"\n"
        "  put the selectedChunk into ou\n"
        "  select empty\n"
        "  select ou\n"
        "  put \"selectedChunk [\" & the selectedChunk & \"]\"\n"
        "  put \"selectedText  [\" & the selectedText & \"]\"\n"
        "\n"
        "  put \"-- 3 bis. before et after, sur un designateur calcule --\"\n"
        "  put \"char 2 to 4 of card field 1\" into ou\n"
        "  select before ou\n"
        "  put \"before [\" & the selectedChunk & \"]\"\n"
        "  select after ou\n"
        "  put \"after  [\" & the selectedChunk & \"]\"\n"
        "\n"
        "  put \"-- 4. temoin negatif : un mot nu ne designe rien --\"\n"
        "  select empty\n"
        "  select zorglub\n"
        "  put \"selectedChunk [\" & the selectedChunk & \"]\"\n"
        "  put \"-- 5. temoin negatif : un texte qui n'est pas un morceau --\"\n"
        "  put \"bonjour les amis\" into ou\n"
        "  select ou\n"
        "  put \"selectedChunk [\" & the selectedChunk & \"]\"\n"
        "\n"
        "  put \"-- 6. hide et show --\"\n"
        "  put \"card field 1\" into ch\n"
        "  hide ch\n"
        "  put \"apres hide ch  : visible \" & the visible of card field 1\n"
        "  show ch\n"
        "  put \"apres show ch  : visible \" & the visible of card field 1\n"
        "\n"
        "  put \"-- 7. set la propriete d'un designateur calcule --\"\n"
        "  set the textStyle of ch to bold\n"
        "  put \"textStyle       : \" & the textStyle of card field 1\n"
        "  set the textStyle of ch to plain\n"
        "  put \"remis a plain   : \" & the textStyle of card field 1\n"
        "\n"
        "  put \"-- 8. find restreint a un designateur calcule --\"\n"
        "  find \"sigm\" in ch\n"
        "  put \"dans le champ de carte : [\" & the result & \"]\"\n"
        "  put \"bkgnd field 1\" into cb\n"
        "  find \"sigm\" in cb\n"
        "  put \"dans le champ de fond  : [\" & the result & \"] \" & the foundText\n"
        "\n"
        "  put \"-- 9. temoins POSITIFS : ils marchaient deja --\"\n"
        "  put \"the name of ch  : \" & the name of ch\n"
        "\n"
        "  put \"-- 10. temoin negatif sur les trois nouvelles --\"\n"
        "  put \"zorglub\" into rien\n"
        "  hide rien\n"
        "  set the textStyle of rien to bold\n"
        "  find \"sigm\" in rien\n"
        "  put \"apres les trois refus : [\" & the result & \"]\"\n"
        "end mouseUp\n");

    hc_send(b, "mouseUp");
    hc_free(st);
    return 0;
}
