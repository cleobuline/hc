/* « delete me » : l'hôte doit apprendre que l'objet est mort, sans quoi son
 * pointeur survolé pointe sur de la mémoire rendue. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static Object *g_survole;      /* ce que l'interface croit avoir sous le curseur */
static int     g_avis;
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;
  if(strstr(t,"→ message")||strstr(t,"[message box]")) printf("   %s\n",t);}
static void mon_mort(Object *o){
  g_avis++;
  if (g_survole == o) { g_survole = NULL; printf("   l'hôte oublie l'objet survolé\n"); }
}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.object_gone=mon_mort;
  hc_set_host(&h);
  Object *st=hc_new_stack("T"); hc_register_stack(st);Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  Object *b=hc_new_button(c,"B");hc_set_current_card(c);
  hc_set_script(b,"on mouseLeave\n  put \"parti\"\nend mouseLeave\n"
                  "on mouseUp\n  put \"avant\"\n  delete me\n  put \"apres\"\nend mouseUp\n");
  g_survole = b;
  printf("── le clic\n");
  hc_send(b,"mouseUp");
  printf("── ce que l'interface ferait ensuite (survol)\n");
  if (g_survole) { printf("   POINTEUR PENDANT : elle parlerait à un objet mort\n");
                   hc_send(g_survole,"mouseLeave"); }
  else printf("   rien à qui parler : le pointeur a été oublié\n");
  printf("── objets sur la carte : %d, avis reçus : %d\n", c->nparts, g_avis);
  printf("── un pointeur LOCAL, que l'hôte ne peut pas oublier\n");
  {
    Object *b2 = hc_new_button(c,"B2");
    hc_set_script(b2,"on mouseDown\n  delete me\nend mouseDown\n"
                     "on mouseUp\n  put \"jamais\"\nend mouseUp\n");
    printf("   vivant avant : %d\n", hc_object_is_live(b2));
    hc_send(b2,"mouseDown");
    printf("   vivant après : %d  (le deuxième envoi est donc évité)\n",
           hc_object_is_live(b2));
    if (hc_object_is_live(b2)) hc_send(b2,"mouseUp");
  }
  printf("── un objet bien vivant\n");
  {
    Object *b3 = hc_new_button(c,"B3");
    printf("   vivant : %d\n", hc_object_is_live(b3));
    printf("   la carte : %d, le fond : %d, la pile : %d\n",
           hc_object_is_live(c), hc_object_is_live(bg), hc_object_is_live(st));
  }
  hc_unregister_stack(st);
  hc_free(st);
  printf("── avis après hc_free de la pile : %d\n", g_avis);
  return 0;}
