/* UN HÔTE MINIMAL : tous les rappels à NULL sauf « line ».
 *
 * Le noyau ne doit rien supposer de son hôte. Chaque rappel de HcHost est
 * facultatif, et un hôte peut légitimement n'en servir aucun — c'est le cas
 * au démarrage de l'application, avant que la vue existe, et le cas de tout
 * outil en ligne de commande qui se contenterait du noyau.
 *
 * Ce harnais passe en revue les commandes qui PARLENT à l'hôte, avec un hôte
 * qui ne répond à rien. Aucune ne doit planter, et chacune doit dire quelque
 * chose de sensé — ni un silence qui laisse croire que ça a marché, ni une
 * adresse déréférencée.
 *
 * Il y a une deuxième partie, plus vicieuse : un hôte qui répond, mais mal.
 * Un load_stack qui rend NULL, un ask qui rend NULL, un global_get qui rend
 * une chaîne vide. Ce sont des réponses légales, et le noyau doit les
 * traiter — c'est exactement ce que fait Cocoa quand l'utilisateur annule. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t);
  else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *corps){
  char s[700]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",corps);
  hc_set_script(b,s); hc_send(b,"t");
}
/* Deuxième hôte : il répond, mais par l'annulation ou le vide. */
static const char *annule_ask(const char *p,const char *d){(void)p;(void)d;return NULL;}
static const char *annule_answer(const char *p,const char *a,const char *bb,const char *cc)
{(void)p;(void)a;(void)bb;(void)cc;return NULL;}
static Object *rien_load(const char *n){(void)n;return NULL;}
static Object *rien_open(const char *n){(void)n;return NULL;}
static const char *vide_global(const char *n){(void)n;return "";}
static int rate_save(Object *s,const char *p){(void)s;(void)p;return 0;}

static void batterie(const char *titre){
  printf("\n════ %s ════\n", titre);
  essai("answer \"question\"\n  put the result & \"|\" & it & \"|\"");
  essai("ask \"question\" with \"defaut\"\n  put the result & \"|\" & it & \"|\"");
  essai("play \"boing\"");
  essai("beep");
  essai("choose browse tool");
  essai("drag from 1,2 to 3,4");
  essai("click at 5,6");
  essai("type \"abc\"");
  essai("visual effect dissolve");
  essai("doMenu \"Nouvelle carte\"\n  put the result & \"|\"");
  essai("go to stack \"AbsolumentAbsente\"\n  put the result & \"|\"");
  essai("start using stack \"AbsolumentAbsente\"\n  put the result & \"|\"");
  essai("save this stack as \"/tmp/hc_muet.stack\"\n  put the result & \"|\"");
  essai("print this card\n  put the result & \"|\"");
  essai("put the diskSpace & \"|\" & the systemVersion & \"|\"");
  essai("set the cursor to watch");
  essai("select text of card field \"x\"");
  essai("put the tool & \"|\"");
  essai("open printing\n  print this card\n  close printing");
}
int main(void){
  setbuf(stdout,NULL);
  static HcHost muet; memset(&muet,0,sizeof muet); muet.line = ma_ligne;
  hc_set_host(&muet);
  Object *st=hc_new_stack("T"); hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");
  hc_new_field(c,"x");
  b=hc_new_button(c,"B"); hc_set_current_card(c);

  batterie("hôte muet : tous les rappels à NULL");

  static HcHost refus; memset(&refus,0,sizeof refus);
  refus.line = ma_ligne;
  refus.ask = annule_ask; refus.answer = annule_answer;
  refus.load_stack = rien_load; refus.open_stack = rien_open;
  refus.global_get = vide_global; refus.save_stack = rate_save;
  hc_set_host(&refus);
  batterie("hôte qui répond par l'annulation ou le vide");

  hc_set_host(&muet);
  hc_unregister_stack(st); hc_free(st); return 0;}
