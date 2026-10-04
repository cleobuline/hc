/* verrou — « THE CANTMODIFY OF THIS STACK », LA PILE VERROUILLÉE.
 *
 * Venu de la pile d'aide d'Apple, « HyperCard Help », qui lit et pose cette
 * propriété (checkCantModify). HC répondait « propriété inconnue ».
 *
 * LE BANC, JOUÉ DANS HYPERCARD (Basilisk II) le 4 octobre, pile test6 :
 *
 *     set the cantModify of this stack to true
 *     put the cantModify of this stack into v       v = true
 *     put "essai" into card field 1                  RIEN — le champ ne change pas
 *     set the cantModify of this stack to false
 *     put return & "cantModify : " & v after card field 1     écrit
 *
 * Une pile verrouillée refuse EN SILENCE qu'un script écrive dans un champ :
 * pas de message, et le script continue (docs/mesures/long_id.txt).
 *
 * Ce harnais rejoue le banc, puis les écritures PAR MORCEAU — qui passent par
 * un autre chemin du noyau —, puis l'aller-retour par le fichier. Ce que la
 * pile verrouillée fait des autres changements n'est pas mesuré, et n'est
 * donc pas verrouillé. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FIC "/tmp/hc_verrou_cantmodify.stack"

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    Object *st = hc_new_stack("test6");
    Object *bg = hc_new_background(st, "Fond");
    Object *c1 = hc_new_card(st, bg, "Une");
    Object *f  = hc_new_field(c1, "F");
    Object *b  = hc_new_button(c1, "B");
    hc_set_current_card(c1);
    hc_set_field_text(f, "avant");

    puts("== 1. le banc d'HyperCard ==");
    hc_set_script(b,
        "on mouseUp\n"
        "  put \"au départ : \" & the cantModify of this stack\n"
        "  set the cantModify of this stack to true\n"
        "  put the cantModify of this stack into v\n"
        "  put \"essai\" into card field 1\n"
        "  put \"champ verrouillé : \" & card field 1\n"
        "  set the cantModify of this stack to false\n"
        "  put return & \"cantModify : \" & v after card field 1\n"
        "  put \"champ libre : \" & card field 1\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== 2. les écritures par morceau, verrouillé ==");
    hc_set_field_text(f, "un deux trois");
    hc_set_script(b,
        "on mouseUp\n"
        "  set the cantModify of this stack to true\n"
        "  put \"DEUX\" into word 2 of card field 1\n"
        "  put \"!\" after card field 1\n"
        "  put \">\" before card field 1\n"
        "  delete word 3 of card field 1\n"
        "  put \"champ : \" & card field 1\n"
        "  put \"variable : \" & (\"x\" & \"y\")\n"
        "end mouseUp\n");
    hc_send(b, "mouseUp");

    puts("\n== 3. l'aller-retour par le fichier ==");
    if (hc_save(st, FIC) != 0) { puts("   enregistrement impossible"); return 1; }
    Object *re = hc_load(FIC);
    printf("   relu verrouillé : %s\n", re && re->cant_modify ? "oui" : "non");
    if (re) hc_free(re);
    st->cant_modify = 0;
    if (hc_save(st, FIC) != 0) { puts("   enregistrement impossible"); return 1; }
    re = hc_load(FIC);
    printf("   relu libre      : %s\n", re && !re->cant_modify ? "oui" : "non");
    FILE *t = fopen(FIC, "r");
    int ligne_vue = 0;
    char l[256];
    while (t && fgets(l, sizeof l, t)) if (strncmp(l, "cantmodify", 10) == 0) ligne_vue = 1;
    if (t) fclose(t);
    printf("   ligne « cantmodify » dans le fichier libre : %s\n", ligne_vue ? "OUI" : "aucune");
    if (re) hc_free(re);
    unlink(FIC);

    hc_free(st);
    return 0;
}
