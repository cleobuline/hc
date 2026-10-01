/* « the skipErrors » : CE QUE FAIT UNE ERREUR. Propriété PROPRE À HC, à trois
 * états, voulue par l'utilisatrice — voir g_skip_errors dans hc_core.c :
 *
 *   non posée   le gestionnaire fautif s'arrête, l'appelant continue — ce que
 *               HC a toujours fait ;
 *   true        la LIGNE fautive est sautée, et le gestionnaire continue ;
 *   false       tout le script s'arrête, appelants compris — HyperCard.
 *
 * LE CAS QUI A TOUT DÉCIDÉ, rapporté à l'usage avec la première version :
 *
 *     on mouseUp
 *       set skiperrors to true
 *       put the zorglup of 3
 *       put "torture en cours..."
 *     end mouseUp
 *
 * « ça skippe rien du tout » : cette version-là arrêtait le gestionnaire
 * fautif, ce que HC faisait déjà sans elle. Section 2.
 *
 * MESURÉ DANS HYPERCARD (Basilisk II) le 1er octobre, pile Torture3, pour
 * « false » : une erreur levée dans un « do » emporte aussi la boucle qui
 * l'appelait. Commande et fonction : NON mesurées.
 *
 * Ce que tient ce harnais :
 *
 *   1. non posée, rien n'a changé : les trois bancs de erreur_abandon.txt ;
 *   2. true : la ligne est sautée — le cas rapporté, puis les trois bancs ;
 *      l'erreur va au JOURNAL, une fois, et AUCUN dialogue ne s'ouvre :
 *      celui qui la récapitulait à la fin faisait croire, à l'usage, que rien
 *      n'avait été sauté. Les autres états, eux, ouvrent toujours le leur ;
 *   3. false : les trois bancs s'arrêtent net, l'appelant compris ;
 *   4. une chaîne de trois niveaux, dans les trois états ;
 *   5. la propriété se relit — vide quand elle n'est pas posée —, redevient
 *      non posée après le script, et refuse ce qui n'est ni true ni false ;
 *   6. sous false, « errorDialog » reçoit encore l'erreur. Ce n'était pas
 *      gratuit : le drapeau d'arrêt, encore levé quand HC livre ce message,
 *      arrêtait son gestionnaire à la première ligne, et l'erreur se perdait
 *      — trouvé par torture2, torture3 et lockerreur. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

/* L'hôte ouvrirait ici le dialogue d'erreur. Sous « true », il ne doit RIEN
 * recevoir : la ligne sautée va au journal seulement. */
static void dialogue(const char *t, Object *o, int l)
{ (void)o; (void)l; printf("      [DIALOGUE] %s\n", t); }

static Object *b;

static const char *SCRIPT =
    "on commande\n"
    "  global gJ\n"
    "  put \"1 avant\" into gJ\n"
    "  plante\n"
    "  put \" / 3 après plante\" after gJ\n"
    "end commande\n"
    "on plante\n"
    "  global gJ\n"
    "  put \" / 2 dans plante\" after gJ\n"
    "  get \"abc\" + 1\n"
    "  put \" / 2b après l'erreur\" after gJ\n"
    "end plante\n"
    "on fonction\n"
    "  global gJ\n"
    "  put \"1 avant\" into gJ\n"
    "  get f()\n"
    "  put \" / 3 après f, it=[\" & it & \"]\" after gJ\n"
    "end fonction\n"
    "function f\n"
    "  global gJ\n"
    "  put \" / 2 dans f\" after gJ\n"
    "  get \"abc\" + 1\n"
    "  put \" / 2b après l'erreur\" after gJ\n"
    "  return \"rendu\"\n"
    "end f\n"
    "on parDo\n"
    "  global gJ\n"
    "  put \"1 avant\" into gJ\n"
    "  faitDo\n"
    "  put \" / 3 après faitDo\" after gJ\n"
    "end parDo\n"
    "on faitDo\n"
    "  global gJ\n"
    "  do \"get \" & quote & \"abc\" & quote & \" + 1\"\n"
    "  put \" / 2b après le do\" after gJ\n"
    "end faitDo\n"
    "on trois\n"
    "  global gJ\n"
    "  put \"a\" into gJ\n"
    "  niveauB\n"
    "  put \" / a après b\" after gJ\n"
    "end trois\n"
    "on niveauB\n"
    "  global gJ\n"
    "  put \" / b\" after gJ\n"
    "  niveauC\n"
    "  put \" / b après c\" after gJ\n"
    "end niveauB\n"
    "on niveauC\n"
    "  global gJ\n"
    "  put \" / c\" after gJ\n"
    "  get \"abc\" + 1\n"
    "  put \" / c après l'erreur\" after gJ\n"
    "end niveauC\n"
    "on journal\n"
    "  global gJ\n"
    "  put \"journal : \" & gJ\n"
    "end journal\n";

/* Un clic : le gestionnaire « quoi », précédé ou non de la propriété, puis le
 * journal dans un script à part — comme le bouton « Journal » du banc. */
static void clic(const char *quoi, const char *avant)
{
    char s[8192];
    printf("   %s%s\n", avant ? avant : "", quoi);
    snprintf(s, sizeof s,
             "on clic\n%s%s\nend clic\n%s",
             avant ? avant : "", quoi, SCRIPT);
    hc_set_script(b, s);
    hc_send(b, "clic");
    hc_send(b, "journal");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    h.erreur = dialogue;
    hc_set_host(&h);
    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    b = hc_new_button(c, "B");

    puts("== 1. non posée : rien ne change ==");
    clic("  commande", NULL);
    clic("  fonction", NULL);
    clic("  parDo", NULL);

    puts("\n== 2. true : la ligne fautive est sautée ==");
    clic("  global gJ\n  put \"1\" into gJ\n  put the zorglup of 3\n"
         "  put \" / 2 après la ligne sautée\" after gJ",
         "  set skiperrors to true\n");
    clic("  commande", "  set the skipErrors to true\n");
    clic("  fonction", "  set the skipErrors to true\n");
    clic("  parDo", "  set the skipErrors to true\n");

    puts("\n== 3. false : tout s'arrête, comme HyperCard ==");
    clic("  commande", "  set the skipErrors to false\n");
    clic("  fonction", "  set the skipErrors to false\n");
    clic("  parDo", "  set the skipErrors to false\n");

    puts("\n== 4. une chaîne de trois niveaux ==");
    clic("  trois", NULL);
    clic("  trois", "  set the skipErrors to true\n");
    clic("  trois", "  set the skipErrors to false\n");

    puts("\n== 5. la propriété se relit, et redevient non posée après le script ==");
    clic("  put \"au départ : [\" & the skipErrors & \"]\"", NULL);
    clic("  put \"pendant : [\" & the skipErrors & \"]\"", "  set the skipErrors to true\n");
    clic("  put \"après : [\" & the skipErrors & \"]\"", NULL);
    /* Une valeur qui n'est ni true ni false SE REFUSE, à la différence des
     * autres réglages booléens : chaque valeur choisit un mode, et une faute
     * de frappe ne doit pas en choisir un en silence. */
    clic("  set the skipErrors to \"peut-être\"\n  put \"refusé, reste : [\" & the skipErrors & \"]\"",
         "  set the skipErrors to false\n");

    puts("\n== 6. sous false, errorDialog reçoit encore l'erreur ==");
    hc_set_script(st,
        "on errorDialog quoi\n"
        "  put \"errorDialog a reçu : \" & quoi\n"
        "end errorDialog\n");
    clic("  commande",
         "  set the lockErrorDialogs to true\n  set the skipErrors to false\n");

    hc_free(st);
    return 0;
}
