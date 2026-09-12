/* Les six messages du cycle de vie. hc_env_message se teste ici ; les deux de
 * pile vivent dans Hcdocument.m et ne se compilent pas hors AppKit, on
 * vérifie donc qu'ils ARRIVENT en les envoyant comme la vue le ferait. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("Demo");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");
  hc_set_current_card(c);
  hc_set_script(st,
    "on startUp\n     put \"startUp     — la pile s'installe\"\n   end startUp\n"
    "on quit\n        put \"quit        — la pile se range\"\n   end quit\n"
    "on suspend\n     put \"suspend     — on passe a une autre appli\"\n end suspend\n"
    "on resume\n      put \"resume      — on revient\"\n           end resume\n"
    "on suspendStack\n put \"suspendStack — on quitte cette pile\"\n end suspendStack\n"
    "on resumeStack\n  put \"resumeStack  — on y revient\"\n        end resumeStack\n");

  puts("── les quatre de l'environnement");
  hc_env_message("startUp");
  hc_env_message("suspend");
  hc_env_message("resume");
  hc_env_message("quit");

  puts("\n── les deux de pile (envoyes comme Hcdocument.m le fait)");
  hc_send(c,"suspendStack");
  hc_send(c,"resumeStack");

  puts("\n── sans carte courante : rien, et pas de plantage");
  hc_set_current_card(NULL);
  hc_env_message("startUp");
  puts("   (silence attendu)");

  puts("\n── « lock messages » ne les retient PAS");
  hc_set_current_card(c);
  hc_do("lock messages");
  hc_env_message("resume");
  hc_do("unlock messages");

  hc_free(st);return 0;}
