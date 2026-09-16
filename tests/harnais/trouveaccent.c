/* « find » sur du texte francais : le motif trouve doit etre DESIGNABLE.
 *
 * Le noyau retient le decalage du motif EN OCTETS — c'est « hit - tx ». Deux
 * lecteurs s'en servent, et ils n'attendent pas la meme chose :
 *
 *   the foundChunk   veut un rang de CARACTERE   -> le noyau convertit
 *   hc_found_range   rend les octets bruts       -> a l'hote de convertir
 *
 * Le dessin du surlignage, cote Cocoa, ne convertissait pas : la boite noire
 * glissait d'un cran par octet supplementaire, donc d'autant que le champ
 * contient d'accents, de guillemets « » ou de tirets longs AVANT le motif.
 * Elle coupait les mots. Mesure sur un texte ordinaire : le motif commencait
 * a l'octet 52 et au caractere 47 — cinq de decalage, cinq lettres avalees.
 *
 * Ce harnais tient ce que le NOYAU promet, la seule moitie qui se mesure ici :
 * le morceau rendu par « the foundChunk » doit relire exactement le motif.
 * Si un jour quelqu'un fait rendre les octets a foundChunk « pour aligner les
 * deux », ce test le dira tout de suite.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    Object *st = hc_new_stack("P"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "U");
    hc_set_current_card(c);
    Object *f  = hc_new_field(c, "texte");
    Object *b  = hc_new_button(c, "b");

    /* Du francais comme on en ecrit : accents, guillemets, tiret long. Chacun
     * pese deux ou trois octets pour un seul caractere. */
    const char *t =
        "— 1. « the » ne ment plus\n"
        "ok   the date répond\n"
        "un mot nu vaut son nom\n"
        "ok   le zorglub se plaint\n";
    hc_set_field_text(f, t);

    puts("== l'ecart entre octets et caracteres ==");
    {
        const char *p = strstr(t, "un mot");
        int octets = (int)(p - t), cars = 0;
        for (const char *q = t; q < p; q++) if ((*q & 0xC0) != 0x80) cars++;
        printf("   le motif commence a l'octet %d, au caractere %d (ecart %d)\n",
               octets, cars, octets - cars);
    }

    puts("\n== et « the foundChunk » designe bien le motif ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  find \"un mot nu vaut son nom\"\n"
        "  put \"foundText  : [\" & the foundText & \"]\"\n"
        "  put \"foundChunk : \" & the foundChunk\n"
        "  put \"foundLine  : \" & the foundLine\n"
        /* LE TEST QUI COMPTE : on relit par le morceau annonce. */
        "  put the foundChunk into ou\n"
        "  put \"relu       : [\" & value(ou) & \"]\"\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== un motif APRES encore plus d'accents ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  find \"zorglub\"\n"
        "  put \"foundChunk : \" & the foundChunk\n"
        "  put the foundChunk into ou\n"
        "  put \"relu       : [\" & value(ou) & \"]\"\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    hc_free(st);
    return 0;
}
