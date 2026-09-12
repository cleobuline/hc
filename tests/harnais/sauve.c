/* Disque plein au milieu de l'écriture : l'ancienne pile survit-elle ? */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <dirent.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}
static long taille(const char *p){struct stat st; return stat(p,&st)==0?(long)st.st_size:-1;}

/* Compte les fichiers temporaires laissés derrière, quel que soit leur nom.
 *
 * Ce test cherchait « hc_sv.stack.tmp » — un nom fixe, que hc_save n'écrit
 * plus depuis qu'elle passe par mkstemp. Il répondait donc « oui » quoi qu'il
 * arrive : une vérification vide, qui aurait continué de passer même si la
 * sauvegarde laissait un fichier à chaque appel. On balaie le répertoire. */
static int restes(void){
  DIR *d = opendir("/tmp"); if (!d) return -1;
  int n = 0; struct dirent *e;
  while ((e = readdir(d)))
    if (!strncmp(e->d_name, "hc_sv.stack.", 12)) n++;
  closedir(d);
  return n;
}
int main(void){
  signal(SIGXFSZ, SIG_IGN);          /* le dépassement doit rendre une erreur, pas tuer */
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("P");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");Object *f=hc_new_field(c,"f");hc_set_current_card(c);
  static char gros[40000]; memset(gros,'Z',39999); gros[39999]=0;
  hc_set_field_text(f,gros);

  remove("/tmp/hc_sv.stack");
  { DIR *d = opendir("/tmp"); struct dirent *e; char p[256];
    while (d && (e = readdir(d)))
      if (!strncmp(e->d_name, "hc_sv.stack.", 12)) {
        snprintf(p, sizeof p, "/tmp/%s", e->d_name); remove(p);
      }
    if (d) closedir(d); }
  int r1 = hc_save(st,"/tmp/hc_sv.stack");
  long ref = taille("/tmp/hc_sv.stack");
  printf("1. sauvegarde normale        : code %d, %ld octets\n", r1, ref);
  printf("2. aucun temporaire resté    : %s\n", restes()==0?"oui":"NON");

  /* On borne la taille des fichiers à 8 Ko : l'écriture échouera en cours. */
  struct rlimit rl; rl.rlim_cur = 8192; rl.rlim_max = 8192;
  setrlimit(RLIMIT_FSIZE, &rl);
  int r2 = hc_save(st,"/tmp/hc_sv.stack");
  rl.rlim_cur = rl.rlim_max = RLIM_INFINITY; setrlimit(RLIMIT_FSIZE, &rl);

  long apres = taille("/tmp/hc_sv.stack");
  printf("3. disque plein en cours      : code %d (doit être -1)\n", r2);
  printf("3b. ni après l'échec         : %s\n", restes()==0?"oui":"NON");
  printf("4. l'ancienne pile            : %ld octets (était %ld) -> %s\n",
         apres, ref, apres==ref ? "INTACTE" : "*** PERDUE ***");
  Object *rl2 = hc_load("/tmp/hc_sv.stack");
  printf("5. et elle se relit           : %s\n", rl2?"oui":"*** NON ***");
  if(rl2) hc_free(rl2);
  hc_free(st); return 0;}
