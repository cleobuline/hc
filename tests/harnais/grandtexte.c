/* Un texte de plus de 64 Ko : un champ le garde, un paramètre le REFUSE.
 *
 * 1. « put "FIN" after card field 1 » fusionnait l'ancien texte et le
 *    nouveau dans un tampon de 65 536 octets : sur un champ de 81 920
 *    caractères, le champ retombait à 65 535 et le « FIN » lui-même était
 *    perdu, sans un mot. De même « before ». « put … into » n'avait pas
 *    cette borne.
 *
 * 2. Les arguments d'un message et la valeur d'un « return » voyagent dans
 *    des tampons de taille fixe : un texte plus long y était COUPÉ en
 *    silence, et le script continuait sur un texte amputé. Décidé par
 *    l'utilisatrice le 10 novembre : la borne reste, et elle se dit — une
 *    faute « paramètre trop long ». À 65 535 octets tout juste, ça passe. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static const char *OUTILS =
    "on h1 p\n  put \"paramètre reçu :\" && the length of p\nend h1\n"
    "function f1 p\n  return the length of p\nend f1\n"
    "function f2 n\n  put \"abcdefghij\" into s\n  repeat n times\n    put s & s into s\n  end repeat\n  return s\nend f2\n"
    "function gros\n  put \"abcdefghij\" into s\n  repeat 14 times\n    put s & s into s\n  end repeat\n  return s\nend gros\n";
static void joue(const char *titre,const char *corps){
  static char s[4000];
  snprintf(s,sizeof s,"on t\n  put \"abcdefghij\" into s\n  repeat 13 times\n    put s & s into s\n  end repeat\n%send t\n%s",corps,OUTILS);
  printf("── %s\n",titre);hc_set_script(b,s);hc_send(b,"t");}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");hc_register_stack(st);Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"U");b=hc_new_button(c,"B");hc_new_field(c,"F1");
  hc_set_current_card(c);
  joue("after et before sur un champ de 81 920 caractères",
    "  put s into cd fld \"F1\"\n"
    "  put \"FIN\" after cd fld \"F1\"\n"
    "  put the length of cd fld \"F1\" && \"[\" & char 81921 to 81923 of cd fld \"F1\" & \"]\"\n"
    "  put \"DEBUT\" before cd fld \"F1\"\n"
    "  put the length of cd fld \"F1\" && \"[\" & char 1 to 5 of cd fld \"F1\" & \"]\"\n");
  joue("un paramètre de 65 535 octets passe",
    "  put char 1 to 65535 of s into p\n"
    "  h1 p\n"
    "  put \"par fonction :\" && f1(p)\n"
    "  send \"h1 p\" to me\n");
  joue("un paramètre de 81 920 octets, à un gestionnaire",
    "  h1 s\n  put \"après\"\n");
  joue("à une fonction",
    "  put f1(s)\n  put \"après\"\n");
  joue("par send",
    "  send \"h1 s\" to me\n  put \"après\"\n");
  joue("une fonction qui rend 40 960 octets : ça passe",
    "  put the length of f2(12)\n");
  joue("une fonction qui rend 163 840 octets",
    "  put the length of gros()\n  put \"après\"\n");
  joue("les fonctions intégrées ne sont pas bornées",
    "  put the length of s && length(s) && offset(\"j\", s)\n");
  hc_free(st);return 0;}
