/* Disque plein au milieu de l'écriture : l'ancienne pile survit-elle ? */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/resource.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
static long taille(const char *p){struct stat st; return stat(p,&st)==0?(long)st.st_size:-1;}
int main(void){
  signal(SIGXFSZ, SIG_IGN);          /* le dépassement doit rendre une erreur, pas tuer */
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("P");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");Object *f=hc_new_field(c,"f");hc_set_current_card(c);
  static char gros[40000]; memset(gros,'Z',39999); gros[39999]=0;
  hc_set_field_text(f,gros);

  remove("/tmp/hc_sv.stack"); remove("/tmp/hc_sv.stack.tmp");
  int r1 = hc_save(st,"/tmp/hc_sv.stack");
  long ref = taille("/tmp/hc_sv.stack");
  printf("1. sauvegarde normale        : code %d, %ld octets\n", r1, ref);
  printf("2. le .tmp a disparu         : %s\n", taille("/tmp/hc_sv.stack.tmp")<0?"oui":"NON");

  /* On borne la taille des fichiers à 8 Ko : l'écriture échouera en cours. */
  struct rlimit rl; rl.rlim_cur = 8192; rl.rlim_max = 8192;
  setrlimit(RLIMIT_FSIZE, &rl);
  int r2 = hc_save(st,"/tmp/hc_sv.stack");
  rl.rlim_cur = rl.rlim_max = RLIM_INFINITY; setrlimit(RLIMIT_FSIZE, &rl);

  long apres = taille("/tmp/hc_sv.stack");
  printf("3. disque plein en cours      : code %d (doit être -1)\n", r2);
  printf("4. l'ancienne pile            : %ld octets (était %ld) -> %s\n",
         apres, ref, apres==ref ? "INTACTE" : "*** PERDUE ***");
  Object *rl2 = hc_load("/tmp/hc_sv.stack");
  printf("5. et elle se relit           : %s\n", rl2?"oui":"*** NON ***");
  if(rl2) hc_free(rl2);
  remove("/tmp/hc_sv.stack.tmp");
  hc_free(st); return 0;}
