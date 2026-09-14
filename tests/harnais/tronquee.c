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
 * TROIS FAMILLES DE COUPURES, et il a fallu trois contrôles distincts :
 *
 *   dans un BLOC de texte      un « end script » manquant : le texte accumulé
 *                              n'est jamais posé sur l'objet ;
 *   entre deux OBJETS          « end button » ou « end card » manquant : part
 *                              ou owner reste ouvert à la fin du fichier ;
 *   sur une frontière PROPRE   rien ne manque syntaxiquement, il manque
 *                              seulement la suite. Seule la signature finale
 *                              « end hc-file » peut le dire.
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
    /* Une SECONDE carte : sans elle, une coupure « juste après end card » ne
     * perdrait rien, et le cas le plus sournois du lot ne se verrait pas. */
    hc_new_card(st, bg, "Deux");
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

    /* ENTRE DEUX OBJETS. Aucun bloc de texte n'est ouvert ici : le fichier
     * s'arrête après « rect 10,10,100,40 », et il manque le « end » de la
     * part, puis celui de la carte. C'est part et owner, restés ouverts à la
     * fin du fichier, qui le disent. */
    if (coupe_avant("end button"))     verdict("avant « end button »", 0);
    else                               printf("   pas de « end button » !\n");
    if (coupe_avant("end card"))       verdict("avant « end card »", 0);
    else                               printf("   pas de « end card » !\n");
    if (coupe_avant("end background")) verdict("avant « end background »", 0);
    else                               printf("   pas de « end background » !\n");
    if (coupe_avant("end stack"))      verdict("avant « end stack »", 0);
    else                               printf("   pas de « end stack » !\n");

    /* UNE COUPURE SUR UNE FRONTIÈRE PROPRE EST MAINTENANT ATTRAPÉE, ELLE AUSSI.
     *
     * Ce cas était le trou du contrôle : un fichier coupé juste après un
     * « end stack » — ou après n'importe quel « end card » — est
     * syntaxiquement irréprochable. Aucun bloc ouvert, aucun objet ouvert. Il
     * manque seulement tout ce qui suivait, et rien dans le texte ne permet de
     * le savoir.
     *
     * C'est la signature finale « end hc-file » qui le dit, et elle seule.
     * L'écrivain la pose toujours ; son absence dans un fichier qui annonce le
     * format 2 signifie que le fichier s'arrête avant sa fin.
     *
     * Sur un fichier v1, qui n'a pas de signature, ce cas reste indétectable —
     * et c'est une raison de plus pour que les piles repassent par une
     * sauvegarde. Le harnais « allerretour » vérifie qu'un v1 se relit bien. */
    coupe_a(5);
    verdict("juste après « end stack » (sans signature)", 0);

    remove(SRC); remove(CUT);
    hc_free(st);
    return 0;
}
