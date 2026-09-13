/* Les entrées-sorties fichier, forme par forme, avec le relevé. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <stdio.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);
  else if(strstr(t,"v1 ")||strstr(t,"relit")||strstr(t,"recours")) printf("   %s\n",t);}
static Object *b;
static void essai(const char *titre,const char *corps){
  char s[2048]; snprintf(s,sizeof s,"on t\n  debug raz\n  %s\n  debug bilan\nend t\n",corps);
  printf("── %s\n",titre);
  hc_set_script(b,s); hc_send(b,"t"); printf("\n");
}
int main(void){
  /* Repartir de fichiers neufs : « write … at end » ajoute, et deux
   * exécutions de suite ne donneraient pas la même chose. */
  remove("/tmp/hc_f1.txt"); remove("/tmp/hc_f2.txt");
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");b=hc_new_button(c,"B");hc_set_current_card(c);
  essai("écrire puis relire en entier",
    "open file \"/tmp/hc_f1.txt\"\n"
    "  write \"abcdefghij\" to file \"/tmp/hc_f1.txt\"\n"
    "  close file \"/tmp/hc_f1.txt\"\n"
    "  open file \"/tmp/hc_f1.txt\"\n"
    "  read from file \"/tmp/hc_f1.txt\"\n"
    "  put \"[\" & it & \"]\"\n"
    "  close file \"/tmp/hc_f1.txt\"");
  essai("read ... for N",
    "open file \"/tmp/hc_f1.txt\"\n"
    "  read from file \"/tmp/hc_f1.txt\" for 4\n"
    "  put \"[\" & it & \"]\"\n"
    "  close file \"/tmp/hc_f1.txt\"");
  essai("read ... at P for N  (la forme qui manquait)",
    "open file \"/tmp/hc_f1.txt\"\n"
    "  read from file \"/tmp/hc_f1.txt\" at 4 for 3\n"
    "  put \"[\" & it & \"]\"\n"
    "  close file \"/tmp/hc_f1.txt\"");
  essai("write ... at end  (l'autre forme qui manquait)",
    "open file \"/tmp/hc_f1.txt\"\n"
    "  write \"ZZ\" to file \"/tmp/hc_f1.txt\" at end\n"
    "  close file \"/tmp/hc_f1.txt\"\n"
    "  open file \"/tmp/hc_f1.txt\"\n"
    "  read from file \"/tmp/hc_f1.txt\"\n"
    "  put \"[\" & it & \"]\"\n"
    "  close file \"/tmp/hc_f1.txt\"");
  essai("read ... until return",
    "open file \"/tmp/hc_f2.txt\"\n"
    "  write \"une\" & return & \"deux\" & return to file \"/tmp/hc_f2.txt\"\n"
    "  close file \"/tmp/hc_f2.txt\"\n"
    "  open file \"/tmp/hc_f2.txt\"\n"
    "  read from file \"/tmp/hc_f2.txt\" until return\n"
    "  put \"[\" & it & \"]\"\n"
    "  read from file \"/tmp/hc_f2.txt\" until return\n"
    "  put \"[\" & it & \"]\"\n"
    "  close file \"/tmp/hc_f2.txt\"");
  essai("le nom du fichier dans une variable",
    "put \"/tmp/hc_f1.txt\" into nomf\n"
    "  open file nomf\n"
    "  read from file nomf for 3\n"
    "  put \"[\" & it & \"]\"\n"
    "  close file nomf");
  essai("fichier non ouvert","read from file \"/tmp/hc_absent.txt\" for 3");

  /* UNE ÉCRITURE QUI ÉCHOUE DOIT LE DIRE.
   *
   * fwrite rend le nombre d'éléments écrits et fflush rend EOF sur erreur —
   * ni l'un ni l'autre n'était regardé. Disque plein, quota, erreur d'E/S :
   * le fichier était tronqué et « the result » restait vide, donc le script
   * croyait son journal en sécurité.
   *
   * La limite est posée JUSTE AUTOUR de l'écriture puis relevée : elle vaut
   * pour tous les fichiers du processus, y compris la sortie de ce harnais,
   * que le lanceur redirige. La laisser en place ferait disparaître tout ce
   * qui suit — ce qui m'est arrivé au premier essai. */
  /* UNE ÉCRITURE QUI ÉCHOUE DOIT LE DIRE.
   *
   * fwrite rend le nombre d'éléments écrits et fflush rend EOF sur erreur —
   * ni l'un ni l'autre n'était regardé. Disque plein, quota, erreur d'E/S :
   * le fichier était tronqué et « the result » restait vide, donc le script
   * croyait son journal en sécurité.
   *
   * On écrit dans /dev/full, qui refuse toute écriture avec ENOSPC. C'est
   * exactement le disque plein, sans toucher au processus.
   *
   * J'ai d'abord essayé RLIMIT_FSIZE, comme le harnais « sauve ». Mauvaise
   * idée ici : la limite vaut pour TOUS les fichiers du processus, donc aussi
   * pour la sortie de ce harnais, que le lanceur redirige. Elle vidait le
   * tampon de stdout en pleine limite, perdait vingt-quatre lignes écrites
   * bien avant, et posait sur le flux un drapeau d'erreur qui survivait au
   * relèvement. /dev/full ne touche à rien d'autre qu'au fichier visé. */
  essai("disque plein pendant « write »",
    "open file \"/dev/full\"\n"
    "  write \"quelques octets\" to file \"/dev/full\"\n"
    "  put \"the result = [\" & the result & \"]\"\n"
    "  close file \"/dev/full\"");

  hc_free(st);return 0;}
