/* corpschamp — le corps d'un champ qui n'en a pas de posé se lit 12.
 *
 * MESURÉ le 11 octobre, les deux côtés, sur un champ neuf :
 *   DANS HYPERCARD 2.4.1 (Basilisk II) : the textSize → 12, the textFont →
 *   Geneva ;
 *   DANS HC (l'application) : the textSize → 0.
 *
 * HC dessine pourtant ce champ en 12, et « the textHeight » répondait déjà
 * 16, calculé sur 12. Un morceau mis en 12 se lisait en plus « mixed » à côté
 * de ses voisins, dessinés pareil.
 *
 * Le champ garde son 0 — rien de posé —, et le fichier ne l'écrit toujours pas
 * (section 4) : seules les lectures rendent le corps effectif.
 *
 * Un BOUTON neuf, mesuré DANS HYPERCARD le même jour : « 12 Chicago ». HC
 * répondait 0 et rien ; il répond maintenant pareil (section 5). Chicago ne
 * ment pas : HC n'a pas cette fonte, et la dessine déjà en police système.
 *
 * NON CORRIGÉ ICI : « the textFont » d'un champ neuf, vide là où HyperCard
 * dit Geneva — HC le dessine en police système, et répondre Geneva serait
 * faux tant que le dessin ne suit pas. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      = %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static void fais(const char *l) { printf("   %s\n", l); hc_do(l); }

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("S"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "C");
    hc_set_current_card(c);
    Object *f = hc_new_field(c, "Neuf");
    hc_set_field_text(f, "un deux trois");
    hc_new_button(c, "Bouton");

    puts("== 1. un champ neuf : 12, comme HyperCard, et comme il est dessiné ==");
    fais("put the textSize of card field 1");
    fais("put the textHeight of card field 1");
    fais("put the textSize of word 1 of card field 1");

    puts("\n== 2. un mot mis en 12 n'est pas « mixed » avec ses voisins ==");
    fais("set the textSize of word 3 of card field 1 to 12");
    fais("put the textSize of word 2 to 3 of card field 1");
    fais("set the textSize of word 3 of card field 1 to 18");
    fais("put the textSize of word 2 to 3 of card field 1");

    puts("\n== 3. posé, il se relit ==");
    fais("set the textSize of card field 1 to 14");
    fais("put the textSize of card field 1 && the textSize of word 1 of card field 1");

    puts("\n== 4. un champ sans corps posé ne l'écrit pas dans le fichier ==");
    Object *g = hc_new_field(c, "Autre");
    hc_set_field_text(g, "x");
    char chemin[] = "/tmp/corpschamp_XXXXXX";
    int fd = mkstemp(chemin);
    if (fd < 0) { perror("mkstemp"); return 1; }
    close(fd);
    int ok = hc_save(st, chemin) == 0;
    Object *relue = ok ? hc_load(chemin) : NULL;
    unlink(chemin);
    Object *g2 = NULL;
    if (relue)
        for (int i = 0; i < relue->nparts; i++) {
            Object *cd = relue->parts[i];
            if (cd->type != OBJ_CARD) continue;
            for (int k = 0; k < cd->nparts; k++)
                if (cd->parts[k]->name && !strcmp(cd->parts[k]->name, "Autre")) g2 = cd->parts[k];
        }
    printf("      relue : %s ; corps rangé du champ « Autre » : %d\n",
           relue ? "oui" : "NON", g2 ? g2->textsize : -1);
    if (relue) hc_free(relue);

    puts("\n== 5. un bouton neuf : « 12 Chicago », comme HyperCard ; posés, ils se relisent ==");
    fais("put the textSize of card button 1 && the textFont of card button 1");
    fais("set the textSize of card button 1 to 9");
    fais("set the textFont of card button 1 to \"Geneva\"");
    fais("put the textSize of card button 1 && the textFont of card button 1");
    fais("put the textFont of card field 1");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
