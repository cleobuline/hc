/* Une pile coupée au milieu d'un bloc ne doit PAS s'ouvrir.
 *
 * « script » ouvre un bloc que « end script » ferme ; idem pour contents,
 * paint, bgtextdata et iconres. Un fichier tranché à l'intérieur de l'un
 * d'eux se lisait sans la moindre erreur : la boucle de lecture s'arrêtait
 * sur une fin de fichier ORDINAIRE — ligne_lit ne distingue une coupure
 * propre de rien du tout — et le texte accumulé n'était même jamais posé sur
 * l'objet, faute du « end » qui l'y pose.
 *
 * La pile s'ouvrait donc, l'air complète, avec un script VIDE là où il y en
 * avait un. Et la première sauvegarde écrivait cette version-là par-dessus
 * l'original : le script était perdu pour de bon.
 *
 * On fabrique une vraie pile, on la sauvegarde, puis on la recoupe à
 * plusieurs endroits et on regarde ce que hc_load en dit. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "/tmp/hc_tronq.stack"
#define CUT "/tmp/hc_tronq_coupe.stack"

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)k; (void)d; (void)t; }

/* Recopie SRC dans CUT, en s'arrêtant JUSTE AVANT la première ligne égale à
 * `borne`. Rend 0 si la borne n'a pas été trouvée — un test qui ne coupe
 * rien doit se voir, pas passer. */
static int coupe_avant(const char *borne)
{
    FILE *a = fopen(SRC, "r");
    FILE *b = fopen(CUT, "w");
    if (!a || !b) { if (a) fclose(a); if (b) fclose(b); return 0; }
    char l[4096];
    int trouve = 0;
    while (fgets(l, sizeof l, a)) {
        char net[4096];
        snprintf(net, sizeof net, "%s", l);
        char *p = net + strlen(net);
        while (p > net && (p[-1] == '\n' || p[-1] == '\r')) *--p = '\0';
        if (strcmp(net, borne) == 0) { trouve = 1; break; }
        fputs(l, b);
    }
    fclose(a); fclose(b);
    return trouve;
}

/* Recopie les n premières lignes seulement. */
static void coupe_a(int n)
{
    FILE *a = fopen(SRC, "r");
    FILE *b = fopen(CUT, "w");
    if (!a || !b) { if (a) fclose(a); if (b) fclose(b); return; }
    char l[4096];
    for (int i = 0; i < n && fgets(l, sizeof l, a); i++) fputs(l, b);
    fclose(a); fclose(b);
}

/* `ouvrable` dit ce qu'on attend : sans cela, un harnais où hc_load
 * refuserait TOUT passerait pour un succès complet. */
static void verdict(const char *quoi, int ouvrable)
{
    Object *p = hc_load(CUT);
    int ouverte = p != NULL;
    printf("   %-38s %-8s %s\n", quoi,
           ouverte ? "ouverte" : "refusée",
           ouverte == ouvrable ? "(attendu)" : "*** INATTENDU ***");
    if (p) hc_free(p);
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "U");
    Object *f  = hc_new_field(c, "f");
    Object *b  = hc_new_button(c, "b");
    hc_set_current_card(c);
    hc_set_field_text(f, "premiere ligne\ndeuxieme ligne\ntroisieme ligne");
    hc_set_script(b,
        "on mouseUp\n"
        "  put \"un\"\n"
        "  put \"deux\"\n"
        "  put \"trois\"\n"
        "end mouseUp\n");

    remove(SRC); remove(CUT);
    printf("── la pile de référence\n");
    printf("   sauvegarde : %s\n", hc_save(st, SRC) == 0 ? "faite" : "ÉCHEC");
    Object *ref = hc_load(SRC);
    printf("   relecture  : %s\n", ref ? "faite" : "ÉCHEC");
    if (ref) {
        /* Le script est bien là dans la version entière : sans cette
         * vérification, « refusée » plus bas ne prouverait rien. */
        const char *sc = NULL;
        for (int i = 0; i < ref->nparts && !sc; i++) {
            Object *ca = ref->parts[i];
            if (ca->type != OBJ_CARD) continue;
            for (int j = 0; j < ca->nparts; j++)
                if (ca->parts[j]->type == OBJ_BUTTON) { sc = hc_script_of(ca->parts[j]); break; }
        }
        printf("   le script du bouton : %d octets\n", sc ? (int)strlen(sc) : -1);
        hc_free(ref);
    }
    printf("\n── la même, coupée\n");

    if (coupe_avant("end script")) verdict("au milieu d'un bloc « script »", 0);
    else                          printf("   pas de « end script » dans le fichier !\n");

    if (coupe_avant("end contents")) verdict("au milieu d'un bloc « contents »", 0);
    else                             printf("   pas de « end contents » !\n");

    /* Une coupure sur une frontière PROPRE — juste après « end stack », soit
     * cinq lignes : l'en-tête, une ligne vide, « stack », « size », la
     * fermeture — n'ouvre aucun bloc. Elle reste ACCEPTÉE : la pile est
     * pauvre, mais elle n'a rien perdu en silence. C'est la limite du
     * verdict, et il faut qu'elle se voie, sinon le test ne dirait plus que
     * « hc_load refuse tout ». */
    coupe_a(5);
    verdict("juste après « end stack »", 1);

    remove(SRC); remove(CUT);
    hc_free(st);
    return 0;
}
