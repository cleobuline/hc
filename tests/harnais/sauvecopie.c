/* « save stack "X" as "Y" » ne doit pas DEMENAGER la pile.
 *
 * Le contrat de HcHost.save_stack, dans hc_core.h, est explicite :
 *
 *     « Enregistre une COPIE de la pile sous un autre nom. C'est ce que veut
 *       dire "save stack X as Y" : dupliquer, et non enregistrer les
 *       modifications en cours. »
 *
 * hc_save adoptait pourtant le chemin dans tous les cas. Mesure avant
 * correction : apres « save this stack as "copie.stack" », la pile en memoire
 * habitait la copie, et « the long name of this stack » changeait de reponse
 * APRES une commande censee ne rien changer. Un script qui fait une sauvegarde
 * de securite demenageait son propre document.
 *
 * Deux fonctions maintenant, et le nom dit laquelle :
 *   hc_save        adopte le chemin — c'est ainsi qu'une pile neuve en recoit un
 *   hc_save_copie  ecrit le fichier et ne touche a rien
 *
 * Le fichier copie doit etre COMPLET : une copie qui n'en serait pas une
 * serait une facon plus discrete de perdre le travail.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

static void ou(Object *st, const char *quand)
{
    const char *p = hc_stack_path(st);
    printf("   %-22s adresse = %s\n", quand, (p && *p) ? p : "(aucune)");
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    const char *A = "/tmp/hc_origine.stack", *C = "/tmp/hc_copie.stack";
    remove(A); remove(C);

    Object *st = hc_new_stack("Pile"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    Object *b  = hc_new_button(c, "Bouton"); hc_set_id(b, 2915);
    hc_set_current_card(c);

    puts("== une pile neuve n'a pas d'adresse ==");
    ou(st, "au depart");

    puts("\n== hc_save la lui donne ==");
    printf("   enregistrement : %s\n", hc_save(st, A) == 0 ? "fait" : "ECHEC");
    ou(st, "apres hc_save");

    puts("\n== hc_save_copie ecrit ailleurs SANS demenager ==");
    printf("   copie : %s\n", hc_save_copie(st, C) == 0 ? "faite" : "ECHEC");
    ou(st, "apres hc_save_copie");

    puts("\n== et « the long name of this stack » ne bouge pas ==");
    Object *d = hc_new_button(c, "d");
    hc_set_script(d,
        "on mouseUp\n"
        "  put the long name of this stack\n"
        "end mouseUp\n");
    hc_send(d, "mouseUp");
    printf("   copie a nouveau : %s\n",
           hc_save_copie(st, C) == 0 ? "faite" : "ECHEC");
    hc_send(d, "mouseUp");

    puts("\n== la copie est une VRAIE copie ==");
    {
        Object *relue = hc_load(C);
        if (!relue) { puts("   ECHEC DE RELECTURE"); return 1; }
        int nb = 0, nc = 0;
        for (int i = 0; i < relue->nparts; i++) {
            if (relue->parts[i]->type == OBJ_CARD) {
                nc++;
                for (int j = 0; j < relue->parts[i]->nparts; j++)
                    if (relue->parts[i]->parts[j]->type == OBJ_BUTTON) nb++;
            }
        }
        printf("   relue : %d carte(s), %d bouton(s), nom [%s]\n",
               nc, nb, relue->name ? relue->name : "");
        hc_free(relue);
    }

    hc_free(st);
    remove(A); remove(C);
    return 0;
}
