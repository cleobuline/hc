/* Une même erreur répétée dans une boucle se COMPTE, elle ne s'empile pas.
 *
 * SIGNALÉ À L'ÉCRAN : un traceur de courbes en mode point appelle « click at »
 * une fois par abscisse. Sur y=ln(x), les abscisses négatives donnent un NaN,
 * la coordonnée est refusée, et le dialogue recevait SEPT lignes identiques —
 * puis mille, sur une courbe plus fine.
 *
 * Sept copies n'apprennent rien de plus qu'une. Le NOMBRE, lui, apprend
 * quelque chose : il dit si la faute est un accident ou toute la boucle.
 *
 * CE QUI NE DOIT PAS SE PERDRE, et c'est tout l'objet des sections 3 et 4 :
 * deux fautes DIFFÉRENTES doivent rester visibles toutes les deux. Ne garder
 * que la première, ou n'en compter qu'une seule sorte, transformerait un
 * rapport en devinette. Seules les lignes CONSÉCUTIVES et IDENTIQUES
 * fusionnent.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)d; (void)k; (void)t; }      /* muet : seul le dialogue nous intéresse */

static void mon_erreur(const char *t, Object *o)
{
    (void)o;
    printf("   --- ce que le dialogue reçoit ---\n");
    for (const char *p = t; *p; ) {
        const char *fin = strchr(p, '\n');
        int n = fin ? (int)(fin - p) : (int)strlen(p);
        printf("   | %.*s\n", n, p);
        if (!fin) break;
        p = fin + 1;
    }
}

static Object *g_pile, *g_carte;
static void fais(const char *ligne)
{
    char script[1024];
    snprintf(script, sizeof script, "on essai\n  %s\nend essai\n", ligne);
    hc_set_script(g_pile, script);
    hc_send(g_carte, "essai");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.erreur = mon_erreur;
    hc_set_host(&h);

    g_pile  = hc_new_stack("Repetition");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);

    printf("=== 1. une seule faute : rien ne change ===\n");
    printf("   (le compte ne s'affiche qu'à partir de deux)\n");
    fais("put 0/0 into z\n  click at 10,z");

    printf("=== 2. la même faute sept fois : une ligne et son compte ===\n");
    printf("   (c'est le cas signalé : « click at » dans une boucle de tracé)\n");
    fais("put 0/0 into z\n"
         "  repeat with i = 1 to 7\n"
         "    click at 10,z\n"
         "  end repeat");

    printf("=== 3. deux fautes DIFFÉRENTES restent toutes les deux ===\n");
    printf("   (ne garder que la première ferait d'un rapport une devinette)\n");
    fais("put 0/0 into z\n"
         "  click at 10,z\n"
         "  drag from 1,2 to 3,z\n"
         "  put the zorglub of this card");

    printf("=== 4. et elles gardent chacune son compte ===\n");
    printf("   (deux boucles à la suite, deux fautes distinctes)\n");
    fais("put 0/0 into z\n"
         "  repeat with i = 1 to 3\n"
         "    click at 10,z\n"
         "  end repeat\n"
         "  repeat with i = 1 to 5\n"
         "    drag from 1,2 to 3,z\n"
         "  end repeat");

    printf("=== 5. seules les lignes CONSÉCUTIVES fusionnent ===\n");
    printf("   (deux fautes qui alternent restent lisibles : on veut voir\n");
    printf("    qu'elles alternent, c'est une information sur la boucle)\n");
    fais("put 0/0 into z\n"
         "  repeat with i = 1 to 2\n"
         "    click at 10,z\n"
         "    drag from 1,2 to 3,z\n"
         "  end repeat");

    printf("=== 6. le témoin : un script sans faute n'ouvre rien ===\n");
    fais("put 2 + 2 into z");
    printf("   (aucun dialogue ci-dessus : c'est le résultat attendu)\n");
    return 0;
}
