/* Deux troncatures SILENCIEUSES, que -Wno-format-truncation masquait.
 *
 * Le drapeau etait la avec ce commentaire : « deux cas connus et benins ».
 * L'un des deux ne l'etait pas.
 *
 * 1. LE NOM D'UN MENU etait ampute a 64 octets sans un mot. Apres quoi
 *    « menu "NomComplet" » ne le retrouvait plus jamais : menu_index compare
 *    le nom ENTIER. Un script pouvait creer un menu et ne plus pouvoir le
 *    designer — ni le remplir, ni le supprimer.
 *
 *    Un menu n'a pas de seconde forme — pas de « menu id N » —, donc
 *    contrairement a un descripteur d'objet il n'y a rien sur quoi se
 *    rabattre : on refuse, et on le dit.
 *
 * 2. L'INVITE de « open file » tenait dans 256 octets. Un chemin macOS va
 *    jusqu'a 1024, et un seul dossier peut en prendre 255 : la question
 *    « Ou est le fichier "/Users/.../Docum" ? » perdait precisement le
 *    renseignement qu'elle apportait.
 *
 * La lecon porte sur le drapeau autant que sur les defauts : on avait juge la
 * famille benigne sans l'examiner cas par cas. Un avertissement qu'on eteint
 * est un avertissement qu'on ne relit plus.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static char g_invite[4096];
static char g_rendu[64];

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

static const char *mon_answer_file(const char *prompt)
{
    snprintf(g_invite, sizeof g_invite, "%s", prompt ? prompt : "(nul)");
    g_rendu[0] = '\0';                 /* on annule : rien a ouvrir ici */
    return NULL;
}

static void nom_de(char *out, int n, char c)
{ memset(out, c, (size_t)n); out[n] = '\0'; }

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne; h.answer_file = mon_answer_file;
    hc_set_host(&h);

    Object *st = hc_new_stack("P"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "U");
    hc_set_current_card(c);
    Object *b = hc_new_button(c, "b");

    puts("== un nom de menu qui TIENT se retrouve par son nom entier ==");
    {
        char nom[300]; nom_de(nom, 200, 'M');
        char s[700];
        snprintf(s, sizeof s,
            "on mouseUp\n"
            "  create menu \"%s\"\n"
            "  put \"menus : \" & the number of menus\n"
            "  put \"retrouve : \" & there is a menu \"%s\"\n"
            "end mouseUp\n", nom, nom);
        hc_set_script(b, s); hc_send(b, "mouseUp");
        printf("   (nom de %d caracteres)\n", 200);
    }

    puts("\n== au dela, on REFUSE au lieu d'amputer ==");
    {
        char nom[400]; nom_de(nom, 300, 'X');
        char s[900];
        snprintf(s, sizeof s,
            "on mouseUp\n"
            "  create menu \"%s\"\n"
            "  put \"menus : \" & the number of menus\n"
            "end mouseUp\n", nom);
        hc_set_script(b, s); hc_send(b, "mouseUp");
        printf("   (nom de %d caracteres : le compte ne doit pas bouger)\n", 300);
    }

    puts("\n== renommer trop long se refuse aussi ==");
    {
        char nom[400]; nom_de(nom, 300, 'Y');
        char s[900];
        snprintf(s, sizeof s,
            "on mouseUp\n"
            "  set the name of menu 1 to \"%s\"\n"
            "  put \"nom du menu 1 inchange : \" & "
            "(the number of chars of the name of menu 1)\n"
            "end mouseUp\n", nom);
        hc_set_script(b, s); hc_send(b, "mouseUp");
    }

    puts("\n== l'invite d'« open file » porte le nom ENTIER ==");
    {
        char nom[700]; nom_de(nom, 600, 'C');
        char s[1200];
        snprintf(s, sizeof s,
            "on mouseUp\n"
            "  open file \"/%s/notes.txt\"\n"
            "end mouseUp\n", nom);
        hc_set_script(b, s); hc_send(b, "mouseUp");
        /* Le chemin porte une barre oblique : file_open cree le fichier sans
         * rien demander. On verifie donc le cas du nom SEUL, juste apres. */
        snprintf(s, sizeof s,
            "on mouseUp\n"
            "  open file \"%s\"\n"
            "end mouseUp\n", nom);
        g_invite[0] = '\0';
        hc_set_script(b, s); hc_send(b, "mouseUp");
        printf("   invite recue : %d caracteres\n", (int)strlen(g_invite));
        printf("   elle contient le nom en entier : %s\n",
               strstr(g_invite, nom) ? "oui" : "NON, TRONQUEE");
    }

    hc_free(st);
    return 0;
}
