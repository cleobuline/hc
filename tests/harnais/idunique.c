/* DEUX OBJETS DE MEME IDENTIFIANT : hc_set_id verifiait les bornes, pas
 * l'unicite.
 *
 * Trouve par l'utilisatrice en relisant la fonction. Elle vérifiait que
 * l'identifiant tient entre 1 et HC_ID_MAX, et s'arretait la. Un .stack
 * portant deux fois « id 4 » chargeait donc deux objets homonymes.
 *
 * CE QUE FAIT UN DOUBLON, mesure en ecrivant o->id en direct :
 *
 *   card field id 4               ->  je suis le premier
 *   the name of card field id 4   ->  card field "premier"
 *   put "ecrit" into ... id 4     ->  premier=[ecrit] second=[inchange]
 *
 * LE PREMIER TROUVE GAGNE, en silence. Le second devient inatteignable par
 * identifiant — pour toujours, et sans message. Et hc_save reecrit « id 4 »
 * deux fois : le fichier transmet sa propre corruption au rechargement.
 *
 * (Ma premiere mesure annoncait que les DEUX champs devenaient muets. C'etait
 * faux : mon harnais n'imprimait pas HC_MSG. Le defaut est reel, mais c'est un
 * objet masque, pas deux objets perdus. Note ici parce qu'une mesure fausse
 * qui exagere un defaut fait corriger la mauvaise chose.)
 *
 * hc_set_id n'a qu'UN SEUL appelant, le lecteur de fichier. Un doublon ne peut
 * donc venir que d'un .stack — edite a la main, fusionne, ou abime. C'est
 * precisement le cas ou il faut le dire plutot que de le subir.
 *
 * ON REFUSE, et l'objet garde l'identifiant neuf que sa creation lui a donne :
 * il reste atteignable, sous un autre numero, et le message nomme l'objet et
 * les deux numeros pour que la pile soit reparable.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      %s\n", t ? t : ""); }

static const char *CHEMIN = "/tmp/hc_idunique.stack";

/* Remplace la PREMIERE occurrence de `avant` par `apres` dans le fichier.
 * C'est ainsi qu'on fabrique le doublon : en abimant un fichier sain, comme
 * le ferait une edition a la main. */
static int abime(const char *avant, const char *apres)
{
    FILE *f = fopen(CHEMIN, "r");
    if (!f) return 0;
    char *tout = malloc(1 << 20);
    if (!tout) { fclose(f); return 0; }
    size_t n = fread(tout, 1, (1 << 20) - 1, f);
    tout[n] = '\0';
    fclose(f);

    char *p = strstr(tout, avant);
    if (!p) { free(tout); return 0; }

    f = fopen(CHEMIN, "w");
    if (!f) { free(tout); return 0; }
    fwrite(tout, 1, (size_t)(p - tout), f);
    fputs(apres, f);
    fputs(p + strlen(avant), f);
    fclose(f);
    free(tout);
    return 1;
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    /* ── une pile saine, enregistree ── */
    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    Object *f1 = hc_new_field(c, "premier");
    Object *f2 = hc_new_field(c, "second");
    hc_set_current_card(c);
    hc_register_stack(st);
    hc_set_field_text(f1, "je suis le premier");
    hc_set_field_text(f2, "je suis le second");

    printf("== la pile saine ==\n   premier id %d, second id %d\n",
           f1->id, f2->id);
    char cible[32], remplacant[32];
    snprintf(cible,      sizeof cible,      "id %d", f2->id);
    snprintf(remplacant, sizeof remplacant, "id %d", f1->id);

    remove(CHEMIN);
    if (hc_save(st, CHEMIN) != 0) { puts("enregistrement impossible"); return 1; }

    /* ── on abime le fichier : le second porte l'identifiant du premier ── */
    puts("\n== on abime le fichier a la main ==");
    printf("   « %s » du second devient « %s »\n", cible, remplacant);
    /* La premiere occurrence de « id <f2> » est bien celle du second : les
     * blocs sont ecrits dans l'ordre, et f1 porte un numero plus petit. */
    if (!abime(cible, remplacant)) { puts("   introuvable dans le fichier"); return 1; }

    puts("\n== rechargement ==");
    Object *r = hc_load(CHEMIN);
    if (!r) { puts("   chargement impossible"); return 1; }

    Object *rc = NULL;
    for (int i = 0; i < r->nparts; i++)
        if (r->parts[i]->type == OBJ_CARD) { rc = r->parts[i]; break; }

    puts("\n== les identifiants apres relecture ==");
    int doublons = 0;
    for (int i = 0; rc && i < rc->nparts; i++) {
        printf("   %-10s id %d\n", rc->parts[i]->name, rc->parts[i]->id);
        for (int j = i + 1; j < rc->nparts; j++)
            if (rc->parts[i]->id == rc->parts[j]->id) doublons++;
    }
    printf("   doublons : %d\n", doublons);

    /* ── chaque objet reste atteignable, sous SON numero ── */
    puts("\n== chacun reste atteignable ==");
    Object *b = rc ? hc_new_button(rc, "Pilote") : NULL;
    if (b) {
        hc_set_current_card(rc);
        hc_register_stack(r);
        for (int i = 0; i < rc->nparts; i++) {
            if (rc->parts[i]->type != OBJ_FIELD) continue;
            char s[200];
            snprintf(s, sizeof s,
                     "on t\n  put card field id %d\nend t\n", rc->parts[i]->id);
            printf("   card field id %d\n", rc->parts[i]->id);
            hc_set_script(b, s);
            hc_send(b, "t");
        }
    }

    /* ── reposer sur un objet l'identifiant qu'il porte DEJA n'est pas un
     *    conflit : sans l'exclusion de soi, il se declarerait en doublon
     *    avec lui-meme. ── */
    puts("\n== reposer son propre identifiant n'est pas un conflit ==");
    if (rc && rc->nparts > 0) {
        Object *o = rc->parts[0];
        int avant = o->id;
        hc_set_id(o, avant);
        printf("   %s : %d -> %d  %s\n", o->name, avant, o->id,
               o->id == avant ? "inchange, aucun message" : "*** PERDU ***");
    }

    hc_free(r);
    hc_free(st);
    remove(CHEMIN);
    return 0;
}
