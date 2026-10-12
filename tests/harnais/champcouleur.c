/* champcouleur — le fond et le cadre d'un champ, avec leur opacité.
 *
 * L'extension accordée par l'utilisatrice le 11 octobre au soir, « pour être
 * cohérent avec notre HC couleur » : backColor pour le fond, foreColor pour le
 * cadre, l'opacité en quatrième nombre — EXACTEMENT comme les boutons, même
 * stockage, mêmes noms, même règle « rien de posé ». Rien pour le texte :
 * textColor sur les morceaux le fait déjà. Pas de hiliteColor : un champ ne
 * s'allume pas.
 *
 * Ce harnais tient le noyau et le fichier. Le DESSIN — le fond mélangé à ce
 * qui est derrière, le cadre, la transparence pendant l'édition — est dans
 * HCview.m, et ne se vérifie qu'à l'écran. */
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

static void couleurs(void)
{
    hc_do("put \"      fond [\" & the backColor of card field 1 & \"]  cadre [\" & "
          "the foreColor of card field 1 & \"]\"");
}

static Object *trouve(Object *pile, const char *nom)
{
    for (int i = 0; i < pile->nparts; i++) {
        Object *cd = pile->parts[i];
        if (cd->type != OBJ_CARD) continue;
        for (int k = 0; k < cd->nparts; k++)
            if (cd->parts[k]->name && !strcmp(cd->parts[k]->name, nom)) return cd->parts[k];
    }
    return NULL;
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("S"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "C");
    hc_set_current_card(c);
    Object *f = hc_new_field(c, "Note");
    hc_set_field_text(f, "semi-transparent");
    Object *b = hc_new_button(c, "Témoin");

    puts("== 1. un champ neuf : rien de posé, il se dessine comme dans HyperCard ==");
    couleurs();
    printf("      rangés : fond %d, cadre %d, opacités %d %d\n",
           f->backcolor, f->forecolor, f->backalpha, f->forealpha);

    puts("\n== 2. un fond semi-transparent, un cadre bleu marine ==");
    fais("set the backColor of card field 1 to \"255,255,200,128\"");
    fais("set the foreColor of card field 1 to navy");
    couleurs();

    puts("\n== 3. les noms, et 255 en quatrième nombre : opaque, lu en trois ==");
    fais("set the backColor of card field 1 to gold");
    couleurs();
    fais("set the foreColor of card field 1 to \"200,0,0,255\"");
    couleurs();
    fais("set the backgroundColor of card field 1 to \"10,20,30,0\"");
    couleurs();

    puts("\n== 4. une couleur inconnue se refuse, la précédente reste ==");
    fais("set the backColor of card field 1 to \"zinzolin\"");
    couleurs();

    puts("\n== 5. vide efface : le champ reprend le dessin d'HyperCard ==");
    fais("set the backColor of card field 1 to empty");
    fais("set the foreColor of card field 1 to \"\"");
    couleurs();
    printf("      rangés : fond %d, cadre %d, opacités %d %d\n",
           f->backcolor, f->forecolor, f->backalpha, f->forealpha);

    puts("\n== 6. pas de hiliteColor pour un champ ; le texte garde textColor ==");
    fais("set the hiliteColor of card field 1 to red");
    fais("put the hiliteColor of card field 1");
    fais("set the textColor of word 1 of card field 1 to blue");
    fais("put the textColor of word 1 of card field 1");

    puts("\n== 7. le bouton, témoin, n'a pas bougé ==");
    fais("set the backColor of card button 1 to \"0,128,0,200\"");
    fais("set the hiliteColor of card button 1 to red");
    fais("put the backColor of card button 1 && the hiliteColor of card button 1");

    puts("\n== 8. le fichier : écrit, relu, les couleurs et l'opacité tiennent ==");
    fais("set the backColor of card field 1 to \"255,255,200,128\"");
    fais("set the foreColor of card field 1 to \"0,0,128,64\"");
    char chemin[] = "/tmp/champcouleur_XXXXXX";
    int fd = mkstemp(chemin);
    if (fd < 0) { perror("mkstemp"); return 1; }
    close(fd);
    int ok = hc_save(st, chemin) == 0;
    Object *relue = ok ? hc_load(chemin) : NULL;
    unlink(chemin);
    Object *f2 = relue ? trouve(relue, "Note") : NULL;
    Object *b2 = relue ? trouve(relue, "Témoin") : NULL;
    printf("      relue : %s\n", relue ? "oui" : "NON");
    if (f2) printf("      champ  : fond %06X/%d, cadre %06X/%d\n",
                   HC_COUL_RVB(f2->backcolor), HC_ALPHA(f2->backalpha),
                   HC_COUL_RVB(f2->forecolor), HC_ALPHA(f2->forealpha));
    if (b2) printf("      bouton : fond %06X/%d\n",
                   HC_COUL_RVB(b2->backcolor), HC_ALPHA(b2->backalpha));
    if (relue) hc_free(relue);
    (void)b;

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
