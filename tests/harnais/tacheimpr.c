/* tacheimpr — « open printing » … « close printing » : UN travail d'impression.
 *
 * Les deux répondaient « ne sait pas faire » ; relevé le 11 octobre sur
 * HyperTalk Reference. Entre les deux, chaque « print card » ajoute ses
 * cartes au travail, et « close printing » les remet à l'hôte d'un coup :
 * un dialogue, un document. La section 3 est l'exemple de la carte « open
 * printing », recopié dans sa forme.
 *
 * « print 2 cards », dont l'exemple a besoin, répondait « rien à
 * imprimer » : section 1 bis.
 *
 * NON MESURÉ dans HyperCard : le moment du dialogue avec « with dialog »
 * (ici, à la fermeture), une carte découpée dans un travail (ici, elle
 * part seule, tout de suite), et « print N cards » au-delà de la dernière
 * carte (ici, on s'y arrête). « open report printing » reste refusé : un
 * rapport n'est pas une image de carte. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      = %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

/* L'hôte dit ce qu'il reçoit : c'est tout ce qu'un harnais peut voir d'une
 * imprimante. */
static void imprime(Object **cartes, int n, const int *decoupe)
{
    printf("      -> hôte : imprime %d carte%s :", n, n > 1 ? "s" : "");
    for (int i = 0; i < n; i++) printf(" %s", cartes[i]->name);
    if (decoupe) printf("  (découpe %d,%d,%d,%d)", decoupe[0], decoupe[1],
                        decoupe[2], decoupe[3]);
    printf("\n");
}

static void fais(const char *l) { printf("   %s\n", l); hc_do(l); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne; h.print_cards = imprime;
    hc_set_host(&h);
    Object *st = hc_new_stack("Fiches"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c1 = hc_new_card(st, bg, "C1");
    for (int i = 2; i <= 6; i++) {
        char nom[8]; snprintf(nom, sizeof nom, "C%d", i);
        hc_new_card(st, bg, nom);
    }
    hc_set_current_card(c1);
    hc_set_script(st,
        "on printRange start,total\n"
        "  push card\n"
        "  open printing\n"
        "  go to card start\n"
        "  print (total-start) + 1 cards\n"
        "  close printing\n"
        "  pop card\n"
        "end printRange\n");

    puts("== 1. témoin : sans travail, chaque print part tout de suite ==");
    fais("print card");

    puts("\n== 1 bis. « print N cards » : N cartes depuis la carte courante ==");
    fais("print 2 cards");
    fais("go card 5");
    fais("print 9 cards");
    fais("print \"deux\" cards");
    fais("go card 1");

    puts("\n== 2. un travail : rien ne part avant close printing ==");
    fais("open printing");
    fais("print this card");
    fais("go next card");
    fais("print this card");
    fais("print card 5");
    fais("put \"[\" & the result & \"]\"");
    fais("close printing");
    fais("put \"[\" & the result & \"]\"");

    puts("\n== 3. l'exemple de la référence : printRange 2,4 ==");
    fais("go card 1");
    fais("printRange 2,4");
    fais("put the short name of this card");

    puts("\n== 4. with dialog, et un close sans open ==");
    fais("open printing with dialog");
    fais("print card 6");
    fais("close printing");
    fais("close printing");
    fais("put \"[\" & the result & \"]\"");

    puts("\n== 5. une carte supprimée sort du travail ==");
    fais("open printing");
    fais("print card 3");
    fais("print card 4");
    fais("go card 3");
    fais("delete this card");
    fais("close printing");

    puts("\n== 6. une carte découpée part seule, tout de suite ==");
    fais("open printing");
    fais("print card 1");
    fais("print card from 0,0 to 100,50");
    fais("close printing");

    puts("\n== 7. ce qui reste refusé ==");
    fais("open report printing");
    fais("open printing now");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
