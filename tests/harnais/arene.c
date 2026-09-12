/* L'arène de tampons ne doit pas saturer sur un script LÉGAL.
 *
 * À saturation, arena_buf rend g_apanic — un tampon statique PARTAGÉ — et
 * toutes les valeurs suivantes s'écrasent mutuellement. Le script ne plante
 * pas : il rend un nombre faux et continue. C'est le pire mode d'échec du
 * noyau, et il s'est déjà produit (voir la note de v3_execute : « des zéros là
 * où l'on attendait des pourcentages »).
 *
 * Avec HC_VAL à 1 Mio, l'arène — plafonnée à 1 Go — tenait 1 024 tampons, et
 * une récursion par message en consomme une trentaine par niveau : elle
 * saturait vers le 32e niveau, pour un plafond de profondeur de 64. Ce harnais
 * pousse jusqu'au plafond et VÉRIFIE LE RÉSULTAT, parce qu'un total faux est
 * la seule chose que la saturation laisse voir.
 *
 * Le calcul est déterministe : « x » & « U » (nom court de la carte) & i fait
 * trois caractères pour i de 1 à 9 et quatre de 10 à 20, soit 9*3 + 11*4 = 71
 * par niveau. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static int g_sature = 0;
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  /* Les messages de saturation sont comptés, pas affichés : leur nombre
   * dépend du chemin, pas leur présence. */
  if(k==HC_ERR && strstr(t,"arène")) { g_sature++; return; }
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void creuse(int prof, int par_niveau){
  char s[1600];
  snprintf(s,sizeof s,
    "on creuse n\n"
    "  global total\n"
    "  if n <= 0 then exit creuse\n"
    "  put 0 into s\n"
    "  repeat with i = 1 to %d\n"
    "    put s + the number of chars of (\"x\" & the short name of this card & i) into s\n"
    "  end repeat\n"
    "  put total + s into total\n"
    "  creuse n - 1\n"
    "end creuse\n"
    "on t\n  global total\n  put 0 into total\n  creuse %d\n"
    "  put total\nend t\n", par_niveau, prof);
  g_sature = 0;
  hc_set_script(b,s);
  printf("── profondeur %d, %d évaluations par niveau (attendu %d)\n",
         prof, par_niveau, prof * 71);
  hc_send(b,"t");
  printf("   saturations : %d\n", g_sature);
}
int main(void){
  setbuf(stdout,NULL);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");
  b=hc_new_button(c,"B");hc_set_current_card(c);

  /* En deçà du seuil d'autrefois, puis au-delà, puis au plafond de
   * profondeur : les trois doivent être justes et muets. */
  creuse(5,  20);
  creuse(30, 20);
  creuse(55, 20);
  creuse(62, 20);   /* juste sous HC_MAX_DEPTH, qui vaut 64 avec « t » */
  hc_unregister_stack(st);hc_free(st);return 0;}
