/* « THE CMDKEY » : UN SYNONYME ANNONCE PAR LE NOYAU, SERVI PAR PERSONNE.
 *
 * HyperTalk ecrit la touche pomme de deux facons — « the commandKey » et
 * « the cmdKey » — et les deux designent la meme touche. La table
 * V3_GLOBALES_HOTE du noyau les annonce toutes les deux : elle dit « ces
 * noms-la, c'est l'hote qui les sert ».
 *
 * Seulement un hote implemente ce qu'il a lu dans une documentation, pas ce
 * qu'une table du noyau promet. Ni l'hote Cocoa (cocoa_global_get) ni l'hote
 * console (console_global) n'avaient de branche « cmdKey » : verifie par
 * grep, le mot n'apparaissait qu'UNE fois dans tout l'arbre, dans la table
 * qui le promettait. Le noyau transmettait donc fidelement un nom que
 * personne ne servait, l'hote rendait NULL, et :
 *
 *   if the cmdKey is down then ...
 *
 * — une ligne ordinaire dans une pile de 1990 — repondait « propriete ou
 * fonction inconnue : cmdKey » au lieu de « up ».
 *
 * LA CORRECTION EST DANS host_global, PAS DANS LES HOTES. Une orthographe
 * n'est pas une connaissance de l'hote : reparee chez l'un, elle serait
 * restee cassee chez l'autre. Ce harnais le tient en n'offrant QUE
 * « commandKey » — exactement comme l'hote Cocoa —, si bien qu'il echoue si
 * quelqu'un deplace un jour la traduction dans les hotes.
 *
 * Ce qu'il tient :
 *
 *   1. cmdKey rend ce que l'hote repond pour commandKey, enfonce ou non ;
 *   2. les deux orthographes rendent LA MEME chose au meme instant — c'est
 *      la propriete qui compte, pas une valeur en dur ;
 *   3. « is down » decide juste sur les deux, puisque c'est l'ecriture reelle
 *      des scripts et qu'une chaine vide y serait fausse sans le dire ;
 *   4. les trois autres touches ne sont pas touchees ;
 *   5. un nom que l'hote ignore VRAIMENT reste un refus franc : la traduction
 *      ne doit pas transformer l'inconnu en silence.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

/* L'etat de la touche, que le harnais pilote. */
static int g_pomme = 0;

/* HOTE MINIMAL, VOLONTAIREMENT SOURD A « cmdKey ».
 *
 * Il connait commandKey, shiftKey, optionKey, mouse — et rien d'autre. C'est
 * la copie fidele de ce que savent les hotes reels. */
static const char *globale(const char *nom)
{
    if (!strcasecmp(nom, "commandKey")) return g_pomme ? "down" : "up";
    if (!strcasecmp(nom, "shiftKey"))   return "down";
    if (!strcasecmp(nom, "optionKey"))  return "up";
    if (!strcasecmp(nom, "mouse"))      return "up";
    return NULL;
}

static Object *b;

static void execute(const char *corps)
{
    char s[1024];
    snprintf(s, sizeof s, "on mouseUp\n%s\nend mouseUp\n", corps);
    hc_set_script(b, s);
    hc_send(b, "mouseUp");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne; h.global_get = globale;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);
    b = hc_new_button(c, "B");

    printf("1. touche relachee\n");
    g_pomme = 0;
    execute("  put the cmdKey");
    execute("  put the commandKey");

    printf("2. touche enfoncee\n");
    g_pomme = 1;
    execute("  put the cmdKey");
    execute("  put the commandKey");

    printf("3. les deux orthographes s'accordent, quel que soit l'etat\n");
    g_pomme = 0;
    execute("  put (the cmdKey = the commandKey)");
    g_pomme = 1;
    execute("  put (the cmdKey = the commandKey)");

    printf("4. « is down » decide, c'est l'ecriture des vrais scripts\n");
    g_pomme = 1;
    execute("  if the cmdKey is down then\n"
            "    put \"pomme enfoncee\"\n"
            "  else\n"
            "    put \"pomme relachee\"\n"
            "  end if");
    g_pomme = 0;
    execute("  if the cmdKey is down then\n"
            "    put \"pomme enfoncee\"\n"
            "  else\n"
            "    put \"pomme relachee\"\n"
            "  end if");

    printf("5. les autres touches ne bougent pas\n");
    execute("  put the shiftKey");
    execute("  put the optionKey");
    execute("  put the mouse");

    printf("6. un nom que l'hote ignore reste un refus franc\n");
    execute("  put the clickText");

    printf("7. la casse ne compte pas, comme pour tout le reste\n");
    g_pomme = 1;
    execute("  put the CMDKEY");
    execute("  put the CmdKey");

    return 0;
}
