/* LE CONTRAT D'AFFICHAGE : ce que l'hôte doit repeindre, et quand.
 *
 * « lock screen » est la raison d'être des boucles qui écrivent dix mille
 * fois dans un champ : sans lui, chaque écriture provoquait un redessin et
 * l'affichage coûtait plus que l'interprétation. Le noyau retient donc les
 * champs touchés et ne réveille que ceux-là au déverrouillage.
 *
 * Trois choses à vérifier, et la troisième est la moins évidente :
 *
 *   — verrouillé, aucun field_changed ne sort ;
 *   — au déverrouillage, un avis par champ TOUCHÉ, pas un par écriture, et
 *     surtout pas un rafraîchissement global — celui-ci repeint la carte
 *     entière, ce qui coûterait plus cher que les redessins évités ;
 *   — au-delà de HC_VERROU_MAX champs distincts (64), le noyau ne peut plus
 *     les retenir et RETOMBE sur le rafraîchissement global. C'est correct —
 *     mieux vaut trop repeindre que pas assez — mais il faut que ce soit
 *     vraiment ce qui se passe, et non un silence. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

#define MAXF 200
static Object *g_vus[MAXF]; static int g_nvus = 0;   /* champs distincts avisés */
static int g_avis = 0;                               /* avis au total */
static void mon_champ(Object *f){
  g_avis++;
  for (int i = 0; i < g_nvus; i++) if (g_vus[i] == f) return;
  if (g_nvus < MAXF) g_vus[g_nvus++] = f;
}
static void raz(void){ g_avis = 0; g_nvus = 0; (void)hc_take_visual_dirty(); }
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
static Object *b;
static void joue(const char *corps){
  char s[900]; snprintf(s,sizeof s,"on t\n%s\nend t\n",corps);
  hc_set_script(b,s); hc_send(b,"t");
}
int main(void){
  setbuf(stdout,NULL);
  static HcHost h; memset(&h,0,sizeof h);
  h.line = ma_ligne; h.field_changed = mon_champ;
  hc_set_host(&h);
  Object *st=hc_new_stack("T"); hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");
  /* Quatre-vingts champs : de quoi passer le plafond de soixante-quatre. */
  Object *f[80];
  for (int i = 0; i < 80; i++) {
    char n[16]; snprintf(n,sizeof n,"f%d",i+1);
    f[i] = hc_new_field(c, n);
  }
  b = hc_new_button(c,"B"); hc_set_current_card(c);

  printf("── sans verrou : un avis par écriture\n");
  raz();
  joue("  repeat with i = 1 to 5\n    put i into card field \"f1\"\n  end repeat");
  printf("   avis %d, champs distincts %d, global %d\n",
         g_avis, g_nvus, hc_take_visual_dirty());

  printf("\n── verrouillé : un avis par champ TOUCHÉ, pas par écriture\n");
  raz();
  joue("  lock screen\n"
       "  repeat with i = 1 to 50\n    put i into card field \"f1\"\n  end repeat\n"
       "  put 0 into card field \"f2\"\n"
       "  unlock screen");
  printf("   avis %d, champs distincts %d, global %d\n",
         g_avis, g_nvus, hc_take_visual_dirty());
  printf("   (attendu : 2 avis pour 2 champs, 51 écritures, aucun global)\n");

  /* Le verrou NE SURVIT PAS au gestionnaire. HyperCard déverrouille de
   * lui-même dès qu'il a fini de traiter un message, et le noyau fait pareil :
   * sans cela, un gestionnaire qui sort avant son « unlock screen » — un exit,
   * une erreur, une branche oubliée — laisserait l'écran verrouillé POUR
   * TOUJOURS, sans que rien ne dise pourquoi. */
  printf("\n── un gestionnaire qui verrouille et sort sans déverrouiller\n");
  raz();
  joue("  lock screen\n"
       "  put \"a\" into card field \"f1\"\n"
       "  exit t\n"
       "  put \"jamais\" into card field \"f2\"");
  printf("   à la sortie du gestionnaire : avis %d, champs %d, global %d\n",
         g_avis, g_nvus, hc_take_visual_dirty());
  printf("   (attendu : 1 avis — le verrou est relâché tout seul)\n");
  raz();
  joue("  put \"b\" into card field \"f3\"");
  printf("   et l'écriture suivante passe : avis %d (attendu 1, pas 0)\n", g_avis);

  printf("\n── verrouillé, dix champs, plusieurs écritures chacun\n");
  raz();
  joue("  lock screen\n"
       "  repeat with tour = 1 to 3\n"
       "    repeat with i = 1 to 10\n"
       "      put tour into card field (\"f\" & i)\n"
       "    end repeat\n"
       "  end repeat\n"
       "  unlock screen");
  printf("   avis %d, champs distincts %d, global %d\n",
         g_avis, g_nvus, hc_take_visual_dirty());
  printf("   (attendu : 10 avis pour 10 champs, 30 écritures, aucun global)\n");

  printf("\n── au-delà du plafond : retombée sur le rafraîchissement global\n");
  raz();
  joue("  lock screen\n"
       "  repeat with i = 1 to 80\n"
       "    put i into card field (\"f\" & i)\n"
       "  end repeat\n"
       "  unlock screen");
  printf("   avis %d, champs distincts %d, global %d\n",
         g_avis, g_nvus, hc_take_visual_dirty());
  printf("   (attendu : 0 avis, et global = 1 — le noyau renonce à la liste)\n");

  printf("\n── le verrou se referme proprement : le tour suivant est normal\n");
  raz();
  joue("  put \"x\" into card field \"f1\"");
  printf("   avis %d, champs distincts %d, global %d\n",
         g_avis, g_nvus, hc_take_visual_dirty());

  hc_unregister_stack(st); hc_free(st); return 0;}
