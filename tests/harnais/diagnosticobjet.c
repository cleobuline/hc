/* UN DIAGNOSTIC QUI SURVIT A SA LIGNE ACCUSE LA SUIVANTE.
 *
 * SIGNALE PAR UN AUDIT EXTERIEUR, confirme par la mesure. Quand la v3 ne sait
 * pas executer une ligne, elle distingue deux fautes :
 *
 *   « ne sait pas faire : X »   le verbe n'est pas compris  -> Can't understand
 *   « objet introuvable : X »   le verbe l'est, l'objet non -> No such object
 *
 * La distinction est bonne — elle a ete faite pour ca — mais elle reposait
 * sur un BOOLEEN global, pose par hct_resout des qu'un noeud d'objet ne se
 * resolvait pas, et remis a zero dans le seul repartiteur.
 *
 * DEUX FAIBLESSES QUI SE COMPOSENT :
 *
 *   1. une ligne servie entierement par l'executeur v3 ne passe JAMAIS par le
 *      repartiteur. Le drapeau survit donc a la ligne qui l'a leve — alors
 *      que le commentaire du code affirmait « remis a zero au debut de chaque
 *      ligne executee ». Il ne l'etait pas ;
 *
 *   2. le drapeau se leve pour des absences PARFAITEMENT NORMALES.
 *      « there is a field "Absent" » est une QUESTION, pas une faute :
 *      l'evaluateur resout, ne trouve pas, repond false — et la resolution
 *      ratee leve le drapeau au passage.
 *
 * Mesure, avant correction :
 *
 *     put there is a field "Absent" into x
 *     help
 *     -> « objet introuvable : help »   et   the result = "No such object"
 *
 * alors que « help » ne contient pas le moindre objet. Idem pour
 * « dial "555" », « open printing », « palette "x" ». Un diagnostic perime
 * est pire qu'un diagnostic vague : il est faux avec assurance, et il envoie
 * chercher la ou il n'y a rien.
 *
 * ON RETIENT DONC LE NOEUD, PAS UN OUI/NON, et le diagnostic ne s'en sert
 * que si l'echec APPARTIENT a la commande qu'il explique.
 *
 * CE QUE CE HARNAIS TIENT SURTOUT, c'est le cas qui a failli etre perdu en
 * chemin : « put x into card field "Absent" » resout sa cible dans
 * l'EXECUTEUR, donc avant le repartiteur. Une correction naive — remettre a
 * zero a l'entree du repartiteur — effacerait justement ce qu'il faut lire,
 * et cette commande redirait « Can't understand ». Le noeud, lui, est bien
 * dans l'arbre de la commande : il est reconnu.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

static void execute(const char *titre, const char *corps)
{
    char s[2048];
    printf("   %s\n", titre);
    snprintf(s, sizeof s, "on essaie\n%s\n  put \"  the result = \" & the result\n"
                          "end essaie\n", corps);
    hc_set_script(b, s);
    hc_send(b, "essaie");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);
    hc_new_field(c, "Present");
    b = hc_new_button(c, "B");

    puts("== 1. une commande non portee, SEULE : le verbe est en cause ==");
    execute("help",          "  help");
    execute("dial",          "  dial \"555\"");
    execute("open printing", "  open printing");
    execute("palette",       "  palette \"x\"");

    puts("\n== 2. LA MEME, apres une question dont la reponse est false ==");
    /* « there is » resout et ne trouve pas : c'est normal, ca ne doit rien
     * changer au diagnostic de la ligne SUIVANTE. */
    execute("there is, puis help",
            "  put there is a field \"Absent\" into x\n  help");
    execute("there is, puis dial",
            "  put there is a field \"Absent\" into x\n  dial \"555\"");
    execute("there is, puis open printing",
            "  put there is a field \"Absent\" into x\n  open printing");

    puts("\n== 3. et « there is » repond toujours juste ==");
    execute("sur un champ absent",  "  put there is a field \"Absent\"");
    execute("sur un champ present", "  put there is a field \"Present\"");

    puts("\n== 4. un VRAI objet manquant reste nomme comme tel ==");
    execute("show sur un champ absent", "  show field \"Absent\"");
    /* LE CAS QUI COUTE : la cible est resolue dans l'EXECUTEUR, avant le
     * repartiteur. Une remise a zero a l'entree de celui-ci l'effacerait. */
    execute("put into un champ absent",
            "  put \"x\" into card field \"Absent\"");
    execute("show sur un champ present", "  show field \"Present\"");

    puts("\n== 5. deux fautes de suite : chacune garde la sienne ==");
    execute("objet absent, puis verbe inconnu",
            "  show field \"Absent\"\n  help");
    execute("verbe inconnu, puis objet absent",
            "  help\n  show field \"Absent\"");

    hc_free(st);
    return 0;
}
