/* « set the » d'une propriete inconnue : se plaint-il, ou se tait-il ?
 *
 * Il se taisait. L'ecriture d'une propriete GLOBALE partait droit chez l'hote,
 * quel que soit le nom, et une coquille ne produisait donc RIEN : ni effet,
 * ni message.
 *
 * Trouve dans Graph Maker 2.2, ou l'auteur avait ecrit
 *
 *     set the foreclor to green
 *
 * et ou le trait est reste noir pendant trente-huit ans.
 *
 * L'incoherence etait complete, et c'est elle qui tranche : « put the
 * foreclor » rendait deja « propriete ou fonction inconnue », et « set the
 * widht of card button 1 » aussi. SEULE l'ecriture d'une globale passait. Un
 * langage qui refuse de lire ce qu'il accepte d'ecrire ment sur l'un des deux.
 *
 * TROIS REPONSES, et pas deux, parce qu'elles appellent trois gestes
 * differents :
 *
 *   la propriete se pose            on la pose
 *   elle existe mais se LIT seule   « propriete en lecture seule » — la
 *       souris, les touches, le rectangle d'ecran, l'espace disque. Ne
 *       refuser que les noms INCONNUS aurait laisse « set the screenRect to
 *       … » passer en silence : le meme defaut, deplace d'un nom a l'autre.
 *   elle n'existe pas               « propriete inconnue »
 *
 * La liste de reference est celle que la LECTURE emploie deja, pour que les
 * deux sens du meme nom ne puissent plus diverger. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static void mon_set(const char *n,const char *v){
  /* l'hote imite cocoa_global_set : il ignore ce qu'il ne connait pas */
  if(!strcasecmp(n,"brush")||!strcasecmp(n,"lineSize"))
    printf("   [HOTE] %s <- %s\n",n,v);
}
static const char *mon_get(const char *n){
  if(!strcasecmp(n,"brush")) return "5";
  if(!strcasecmp(n,"lineSize")) return "1";
  if(!strcasecmp(n,"screenRect")) return "0,0,512,342";
  return NULL;}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;h.global_set=mon_set;h.global_get=mon_get;
  hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");
  hc_set_current_card(c);
  hc_set_script(b,
   "on mouseUp\n"
   "  put \"1. une propriete qui se pose :\"\n"
   "  set the brush to 4\n"
   "  put \"   the result = [\" & the result & \"]\"\n"
   "  put \"2. une qui existe mais se LIT seule :\"\n"
   "  set the screenRect to \"0,0,1,1\"\n"
   "  put \"   the result = [\" & the result & \"]\"\n"
   "  put \"   et elle se lit toujours : \" & the screenRect\n"
   "  put \"3. une coquille — le cas de Graph Maker :\"\n"
   "  set the foreclor to green\n"
   "  put \"   the result = [\" & the result & \"]\"\n"
   "  put \"4. une qui n'existe pas :\"\n"
   "  set the brushSize to 4\n"
   "  put \"5. une completement inventee :\"\n"
   "  set the zorglub to 4\n"
   "  put \"6. et la LECTURE, qui refusait deja :\"\n"
   "  put the brushSize\n"
   "  put \"7. (cette ligne ne doit pas paraitre)\"\n"
   "end mouseUp\n");
  hc_send(b,"mouseUp");
  hc_unregister_stack(st);hc_free(st);return 0;}
