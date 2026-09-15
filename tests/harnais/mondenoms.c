/* LES NOMS DU MONDE : la v3 les sert-elle SANS sonder l'ancien moteur ?
 *
 * v3_fonction sondait l'ancien moteur pour tout nom qu'elle ne servait pas
 * elle-meme : « est-ce que tu connais ca ? ». Mesure sur les 192 harnais,
 * 348 sondes — 63 % de toutes les entrees dans term_value / call_function.
 * Mesure nom par nom sur 118 candidats, l'ancien moteur en servait UN :
 * « the tool », et seulement parce qu'il avait un DEFAUT (« browse tool »)
 * que la v3 n'avait pas.
 *
 * 348 questions pour une reponse. Le reste rendait le mot qu'on lui avait
 * donne, et v1_note_muet mettait cette non-reponse en cache — tout un
 * mecanisme pour amortir une question qu'il ne fallait pas poser.
 *
 * Ce harnais tient l'invariant qui remplace la sonde :
 *
 *   1. chaque nom de V3_V1_FONCTIONS_0 est servi par la v3, donc « retours
 *      vers l'ancien interpreteur » reste (aucun) et « ce que l'ancien
 *      interprete execute » reste (rien) ;
 *   2. les six noms ajoutes — version, destination, language, sound, size,
 *      freeSize — repondent, et repondent juste ;
 *   3. un nom inconnu se dit inconnu, au lieu de rendre son propre texte.
 *
 * Le point 3 est le garde-fou du point 2 : une table qui repond a tout ne
 * vaut pas mieux qu'un echo.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *g_bouton;

static void demande(const char *nom)
{
    char s[512];
    printf("   the %s\n", nom);
    snprintf(s, sizeof s, "on mouseUp\n  put the %s\nend mouseUp\n", nom);
    hc_set_script(g_bouton, s);
    hc_send(g_bouton, "mouseUp");
}

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    Object *st = hc_new_stack("Pile"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    g_bouton = hc_new_button(c, "B");

    hc_v3_bilan_remise_a_zero();

    puts("== les six noms du monde qui manquaient ==");
    demande("version");
    demande("destination");
    demande("language");
    demande("sound");
    demande("size");
    demande("freeSize");

    puts("\n== « the tool », le seul que l'ancien moteur servait vraiment ==");
    /* Son defaut vit maintenant des deux cotes ; sans cela la boucle sur
     * V3_GLOBALES_HOTE renoncait des que l'hote se taisait, et sondait. */
    demande("tool");

    puts("\n== ce que la v3 servait deja, et doit continuer a servir seule ==");
    demande("result");
    demande("date");
    demande("time");
    demande("ticks");
    demande("paramCount");
    demande("itemDelimiter");
    demande("numberFormat");
    demande("lockScreen");
    demande("lockMessages");
    demande("userLevel");
    demande("dragSpeed");
    demande("blindTyping");
    demande("stacksInUse");
    demande("selectedText");
    demande("foundText");

    puts("\n== un nom inconnu se DIT inconnu ==");
    /* Le garde-fou : sans lui, une table qui repond a tout ne vaudrait pas
     * mieux que l'echo qu'on vient de supprimer. */
    demande("zorglub");
    demande("maTemperature");

    puts("\n== et le bilan doit rester vide ==");
    /* C'est la mesure qui compte : aucune de ces demandes ne doit avoir
     * emprunte l'ancien moteur. */
    hc_v3_bilan();

    hc_free(st);
    return 0;
}
