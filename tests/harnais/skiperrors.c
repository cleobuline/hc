/* « the skipErrors » : CONTINUER APRÈS UNE ERREUR, OU TOUT ARRÊTER.
 *
 * Une propriété de HC, pas d'HyperCard, voulue par l'utilisatrice :
 *
 *   true   (le défaut) une erreur n'arrête que le gestionnaire où elle tombe,
 *          l'appelant continue — ce que HC a toujours fait ;
 *   false  une erreur arrête TOUT le script, appelants compris — ce que fait
 *          HyperCard.
 *
 * MESURÉ DANS HYPERCARD (Basilisk II) le 1er octobre, pile Torture3 : une
 * erreur levée dans un « do », dans un gestionnaire appelé par la boucle d'un
 * mouseUp, a emporté la boucle aussi. Le cas de la commande et de la fonction
 * n'y est PAS mesuré (docs/mesures/erreur_abandon.txt) : « false » les traite
 * comme le « do ».
 *
 * Ce que tient ce harnais :
 *
 *   1. par défaut, rien n'a changé : les trois bancs de erreur_abandon.txt
 *      rendent ce qu'ils rendaient ;
 *   2. sous « false », les trois s'arrêtent net, l'appelant compris, et
 *      l'erreur n'est dite qu'UNE fois — pas de cascade de messages ;
 *   3. une chaîne de trois niveaux s'arrête entière ;
 *   4. la propriété se relit, et revient à true quand le script est fini ;
 *   5. sous « false », « errorDialog » reçoit encore l'erreur. Ce n'était pas
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
    hc_set_host(&h);
    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    b = hc_new_button(c, "B");

    puts("== 1. par défaut (true) : l'appelant continue, comme toujours dans HC ==");
    clic("  commande", NULL);
    clic("  fonction", NULL);
    clic("  parDo", NULL);

    puts("\n== 2. sous « false » : tout s'arrête, comme HyperCard ==");
    clic("  commande", "  set the skipErrors to false\n");
    clic("  fonction", "  set the skipErrors to false\n");
    clic("  parDo", "  set the skipErrors to false\n");

    puts("\n== 3. une chaîne de trois niveaux ==");
    clic("  trois", NULL);
    clic("  trois", "  set the skipErrors to false\n");

    puts("\n== 4. la propriété se relit, et revient à true après le script ==");
    clic("  put \"pendant : \" & the skipErrors", "  set the skipErrors to false\n");
    clic("  put \"après : \" & the skipErrors", NULL);
    /* PAS UN REFUS : pour tous les réglages booléens de HC, ce qui n'est
     * pas « true » vaut false — lockErrorDialogs suit la même règle. Ce que
     * fait HyperCard d'une telle valeur n'est pas mesuré. */
    clic("  set the skipErrors to \"peut-être\"\n  put \"autre que true : \" & the skipErrors",
         NULL);

    puts("\n== 5. sous « false », errorDialog reçoit encore l'erreur ==");
    hc_set_script(st,
        "on errorDialog quoi\n"
        "  put \"errorDialog a reçu : \" & quoi\n"
        "end errorDialog\n");
    clic("  commande",
         "  set the lockErrorDialogs to true\n  set the skipErrors to false\n");

    hc_free(st);
    return 0;
}
