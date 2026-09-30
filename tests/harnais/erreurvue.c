/* UNE ERREUR DE SCRIPT DOIT ARRIVER JUSQU'A L'UTILISATEUR.
 *
 * Trouve par l'utilisatrice, en testant dans l'application : « objet
 * introuvable, mais dans la console Xcode ». Le rappel de l'interface faisait
 *
 *     if (kind == HC_ERR || getenv("HC_TRACE")) NSLog("%s", text);
 *
 * NSLog, et rien d'autre. TOUTES les erreurs de script etaient invisibles pour
 * qui n'avait pas lance l'application depuis Xcode — pas seulement celles
 * qu'on venait d'ajouter : la syntaxe, l'objet introuvable, le verbe inconnu.
 * Depuis toujours. HyperCard, lui, ouvrait un dialogue.
 *
 * LE RAPPEL « line » NE POUVAIT PAS SERVIR A CA. Il part LIGNE PAR LIGNE,
 * pendant l'execution : une seule erreur en produit trois — le message,
 * l'extrait du script, le resume. Brancher un dialogue dessus en ouvrirait
 * trois. D'ou un second rappel, « erreur », qui part UNE FOIS, a la fin du
 * gestionnaire le plus exterieur, avec tout le texte accumule.
 *
 * Ce harnais tient les quatre proprietes qui rendent le dialogue possible :
 *
 *   1. une erreur -> UN SEUL appel, quel que soit le nombre de lignes ;
 *   2. des gestionnaires IMBRIQUES -> toujours un seul appel, a la fin du
 *      plus exterieur, et non un par niveau ;
 *   3. l'objet fautif est NOMME, pour que « Script » ouvre le bon ;
 *   4. un script SANS erreur n'appelle rien du tout ;
 *   5. la LIGNE fautive est donnee, pour que « Script » y envoie.
 *
 * Le point 4 est le garde-fou des trois autres : un rappel qui partirait a
 * chaque clic serait pire que le silence.
 *
 * LE POINT 5 EST VENU DE L'USAGE, comme le reste de ce harnais : « ya un
 * manque c'est le bouton debug et le bouton script n'envoie pas sur la ligne
 * fautive ». Le noyau situait l'erreur depuis toujours — le message dit
 * « ligne 3 de button "B".t » — mais dans son TEXTE, d'ou l'interface ne
 * pouvait pas la reprendre sans l'analyser. Elle arrive maintenant comme un
 * NOMBRE, a cote de l'objet.
 *
 * LA PREMIERE LIGNE NON NULLE GAGNE, et c'est une GARDE, pas une correction :
 * mesure faite, un seul niveau signale aujourd'hui, meme quand l'erreur
 * traverse DEUX objets — le cas 2 bis le montre, la carte appelle le bouton et
 * seul le bouton parle. Garder la derniere donnerait donc le meme resultat.
 *
 * La garde est la pour que les deux renseignements viennent toujours du MEME
 * niveau. Si un jour un niveau exterieur se met a signaler lui aussi, l'objet
 * retenu resterait l'INTERIEUR — il est pris au premier message — et la ligne
 * deviendrait celle de l'EXTERIEUR : l'editeur s'ouvrirait sur le bon script,
 * a une ligne qui appartient a un autre. C'est exactement le genre d'ecart qui
 * ne se voit pas, puisque les deux valeurs restent plausibles.
 *
 * CET ECART EXISTAIT, par un autre chemin que celui qu'on gardait : un
 * diagnostic d'ANALYSE (« script, ligne 2 colonne 16 ») sur le script d'un
 * autre objet, lu en chemin, entrait le premier et fixait l'objet. Mesure :
 * le bouton echouait ligne 3, « Script » ouvrait la CARTE a la ligne 3. Les
 * deux renseignements sont desormais poses ENSEMBLE par err_situe, et
 * seulement par l'erreur qui a arrete le script ; le diagnostic d'analyse va
 * au journal, plus au dialogue — d'ou « lignes du message » qui passe de 3 a 2
 * dans le cas de syntaxe ci-dessous. Les cinq cas mal situes sont tenus par
 * le harnais ouerreur.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static int   g_appels = 0;
static char  g_dernier[2048];
static char  g_coupable[128];

static void ligne(HcLineKind k, int d, const char *t)
{ (void)k; (void)d; (void)t; }   /* muet : on ne mesure que « erreur » */

static int   g_ligne = 0;

static void mon_erreur(const char *texte, Object *objet, int ligne)
{
    g_appels++;
    g_ligne = ligne;
    snprintf(g_dernier, sizeof g_dernier, "%s", texte ? texte : "");
    if (objet) hc_describe(objet, g_coupable, sizeof g_coupable);
    else       snprintf(g_coupable, sizeof g_coupable, "(aucun)");
}

static Object *b;

static void essai(const char *titre, const char *script, const char *msg)
{
    g_appels = 0; g_dernier[0] = '\0'; g_coupable[0] = '\0'; g_ligne = 0;
    hc_set_script(b, script);
    hc_send(b, msg);
    printf("   %s\n", titre);
    printf("      appels au dialogue : %d\n", g_appels);
    if (g_appels) {
        printf("      objet fautif       : %s\n", g_coupable);
        printf("      ligne fautive      : %d\n", g_ligne);
        /* Le texte complet, ligne par ligne : c'est lui qui remplirait le
         * dialogue, et c'est donc lui qu'il faut lire. */
        int n = 1;
        for (const char *p = g_dernier; *p; p++) if (*p == '\n') n++;
        printf("      lignes du message  : %d\n", n);
        char copie[2048]; snprintf(copie, sizeof copie, "%s", g_dernier);
        for (char *l = strtok(copie, "\n"); l; l = strtok(NULL, "\n"))
            printf("         %s\n", l);
    }
    puts("");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    h.erreur = mon_erreur;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);
    b = hc_new_button(c, "Bouton");

    puts("== 1. une erreur, plusieurs lignes, UN SEUL appel ==");
    essai("un objet qui n'existe pas",
          "on mouseUp\n  put the short name of card field \"AbsentTotal\"\nend mouseUp\n",
          "mouseUp");

    essai("une erreur de syntaxe",
          "on mouseUp\n  repeat with i = 1 up to 5\n  end repeat\nend mouseUp\n",
          "mouseUp");

    puts("== 2. des gestionnaires IMBRIQUES : toujours un seul ==");
    /* deux -> un -> l'erreur. L'accumulation ne doit pas se vider au retour
     * de « un », sinon le dialogue partirait au mauvais moment — ou trois
     * fois. */
    essai("deux niveaux d'imbrication",
          "on un\n  put the short name of card field \"AbsentTotal\"\nend un\n"
          "on deux\n  un\nend deux\n"
          "on mouseUp\n  deux\nend mouseUp\n",
          "mouseUp");

    /* 2 bis — L'IMBRICATION SUR DEUX OBJETS. La carte appelle le bouton, le
     * bouton tombe. Le dialogue doit nommer le BOUTON et SA ligne, pas la
     * carte et la ligne de son « send » : c'est le script qu'on veut ouvrir.
     * Les deux lignes different expres — 3 chez le bouton, 5 chez la carte —
     * pour qu'une confusion se voie. */
    {
        Object *cc = hc_current_card();
        hc_set_script(cc, "on porte\n  put 1\n  put 2\n  put 3\n"
                          "  send \"casse\" to card button \"Bouton\"\n"
                          "end porte\n");
        hc_set_script(b, "on casse\n  put 1\n"
                         "  put the short name of card field \"AbsentTotal\"\n"
                         "end casse\n");
        g_appels = 0; g_dernier[0] = '\0'; g_coupable[0] = '\0'; g_ligne = 0;
        hc_send(cc, "porte");
        printf("   deux OBJETS imbriques\n");
        printf("      appels au dialogue : %d\n", g_appels);
        printf("      objet fautif       : %s\n", g_coupable);
        printf("      ligne fautive      : %d   (3 chez le bouton, pas 5)\n",
               g_ligne);
        hc_set_script(cc, "");
        puts("");
    }

    puts("== 3. un script SANS erreur n'appelle rien ==");
    essai("tout se passe bien",
          "on mouseUp\n  put 2 + 2 into x\nend mouseUp\n",
          "mouseUp");

    puts("== 4. et l'appel suivant repart a zero ==");
    /* Le tampon est vide AVANT l'appel de l'hote, pour qu'un dialogue qui
     * relance un script ne se voie pas resservir l'erreur precedente. */
    essai("apres une erreur, un script sain",
          "on mouseUp\n  put 1 + 1\nend mouseUp\n",
          "mouseUp");

    hc_free(st);
    return 0;
}
