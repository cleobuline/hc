/* Un fichier qui contient DEUX piles est refusé, avec son motif.
 *
 * hc_save n'en écrit jamais qu'une. Une seconde ligne « stack » écrasait
 * pourtant le pointeur de la pile en cours : la première, avec ses fonds et
 * ses cartes, partait dans la nature — LeakSanitizer le voyait à chaque fois —
 * et un « button » qui suivait s'attachait encore à une carte de l'ANCIENNE
 * pile, que rien ne rendrait plus. Trouvé par le fuzzing du lecteur : les dix
 * premiers fichiers abîmés qu'il a signalés étaient tous celui-là.
 *
 * Le témoin compte autant que le cas : un script et un champ dont une ligne
 * COMMENCE par « stack » doivent toujours s'ouvrir. Ils sont écrits dans des
 * blocs, préfixés de « | », et ne passent donc jamais par l'en-tête — c'est ce
 * que le témoin vérifie au lieu de le supposer. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

#define F1 "/tmp/hc_deuxpiles_a.stack"
#define F2 "/tmp/hc_deuxpiles_b.stack"

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)k; (void)d; (void)t; }

/* Recopie F1 dans F2 en doublant la ligne « stack … » : deux en-têtes. */
static int double_l_entete(void)
{
    FILE *a = fopen(F1, "r"), *b = fopen(F2, "w");
    if (!a || !b) { if (a) fclose(a); if (b) fclose(b); return 0; }
    char l[4096];
    int vu = 0;
    while (fgets(l, sizeof l, a)) {
        fputs(l, b);
        if (!vu && strncmp(l, "stack ", 6) == 0) { fputs("stack \"Seconde\"\n", b); vu = 1; }
    }
    fclose(a); fclose(b);
    return vu;
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Premiere");
    Object *bg = hc_new_background(st, "F");
    Object *c = hc_new_card(st, bg, "U");
    Object *btn = hc_new_button(c, "B");
    hc_set_script(btn, "on mouseUp\nstack \"piège\"\nend mouseUp\n");
    Object *f = hc_new_field(c, "C");
    hc_set_field_text(f, "stack \"piège aussi\"\nfin");
    if (hc_save(st, F1) != 0) { puts("écriture impossible"); return 1; }
    hc_free(st);

    puts("== le témoin : « stack » en début de ligne dans un script et un champ ==");
    Object *t = hc_load(F1);
    printf("   %s\n", t ? "ouverte" : hc_load_erreur());
    if (t) {
        Object *b2 = NULL, *f2 = NULL;
        for (int i = 0; i < t->nparts; i++)
            if (t->parts[i]->type == OBJ_CARD && t->parts[i]->nparts >= 2) {
                b2 = t->parts[i]->parts[0];
                f2 = t->parts[i]->parts[1];
            }
        printf("   script intact : %s\n",
               b2 && b2->script && strstr(b2->script, "stack \"piège\"") ? "oui" : "NON");
        printf("   champ intact  : %s\n",
               f2 && strstr(hc_field_text(f2), "stack \"piège aussi\"") ? "oui" : "NON");
        hc_free(t);
    }

    puts("\n== deux en-têtes « stack » ==");
    if (!double_l_entete()) { puts("   la ligne « stack » est introuvable"); return 1; }
    Object *d = hc_load(F2);
    printf("   %s\n", d ? "OUVERTE, à tort" : hc_load_erreur());
    if (d) hc_free(d);

    remove(F1);
    remove(F2);
    return 0;
}
