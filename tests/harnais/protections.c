/* Les deux verrous de l'Info carte : « Don't Search This Card » et
 * « Can't Delete This Card ».
 *
 * Le premier existait pour les CHAMPS seulement ; il vaut maintenant aussi
 * pour une carte — qu'on saute en entier — et pour un fond, dont on saute
 * toutes les cartes. Le second n'existait pas du tout.
 *
 * Les deux s'enregistrent avec la pile. C'est ce qui les rend utiles : un
 * verrou qui disparaît à la sauvegarde ne protège rien. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
static Object *b;
static void essai(const char *corps){
  char s[640]; snprintf(s,sizeof s,"on t\n  %s\nend t\n",corps);
  printf("── %s\n",corps); hc_set_script(b,s); hc_send(b,"t");
}
static Object *carte(Object *st,const char *nom){
  for(int i=0;i<st->nparts;i++)
    if(st->parts[i]->type==OBJ_CARD && st->parts[i]->name &&
       !strcmp(st->parts[i]->name,nom)) return st->parts[i];
  return NULL;
}
int main(void){
  /* Sans cela les traces internes, non tamponnées, remontent en tête de la
   * sortie et l'ordre dépend du tampon — une référence qui bouge toute seule. */
  setbuf(stdout, NULL);
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("V");hc_register_stack(st);
  Object *bg=hc_new_background(st,"F");
  Object *c1=hc_new_card(st,bg,"Une");
  Object *c2=hc_new_card(st,bg,"Deux");
  Object *c3=hc_new_card(st,bg,"Trois");
  /* L'aiguille n'est PAS sur la première carte : sans cela « find » la
   * trouverait sur place et ne dirait rien du saut. */
  hc_set_field_text(hc_new_field(c1,"x"),"rien");
  hc_set_field_text(hc_new_field(c2,"x"),"aiguille");
  hc_set_field_text(hc_new_field(c3,"x"),"aiguille");
  b=hc_new_button(c1,"B");hc_set_current_card(c1);

  printf("=== Don't Search ===\n");
  essai("find \"aiguille\"\n  put the short name of this card");
  essai("go to card \"Une\"\n"
        "  set the dontSearch of card \"Deux\" to true\n"
        "  find \"aiguille\"\n  put the short name of this card");
  essai("go to card \"Une\"\n"
        "  set the dontSearch of card \"Trois\" to true\n"
        "  find \"aiguille\"\n  put the short name of this card & \"/\" & the result");
  essai("go to card \"Une\"\n"
        "  set the dontSearch of card \"Deux\" to false\n"
        "  set the dontSearch of card \"Trois\" to false\n"
        "  set the dontSearch of bg \"F\" to true\n"
        "  find \"aiguille\"\n  put the short name of this card & \"/\" & the result");
  essai("set the dontSearch of bg \"F\" to false\n"
        "  put the dontSearch of bg \"F\"");

  printf("=== Can't Delete ===\n");
  essai("put the cantDelete of this card");
  essai("set the cantDelete of card \"Trois\" to true\n"
        "  delete card \"Trois\"\n"
        "  put the number of cards & \"/\" & the result");
  essai("set the cantDelete of card \"Trois\" to false\n"
        "  delete card \"Trois\"\n"
        "  put the number of cards & \"/\" & the result");

  /* Le verrou du FOND protège sa DERNIÈRE carte : c'est elle qui le ferait
   * disparaître. Il faut un second fond pour que le cas existe — sinon la
   * règle « jamais moins d'une carte » refuse déjà, et l'on ne saurait pas
   * lequel des deux verrous a parlé. */
  printf("=== le verrou du fond protège sa dernière carte ===\n");
  {
    Object *bg2 = hc_new_background(st, "G");
    hc_new_card(st, bg2, "Ailleurs");
    hc_new_card(st, bg2, "Encore");
  }
  essai("set the cantDelete of card \"Une\" to false\n"
        "  set the cantDelete of bg \"F\" to true\n"
        "  put the number of cards");
  essai("delete card \"Deux\"\n"
        "  put the number of cards & \"/\" & the result");
  /* « Une » est maintenant la dernière carte du fond F : la supprimer
   * emporterait le fond, donc c'est refusé. */
  essai("delete card \"Une\"\n"
        "  put the number of cards & \"/\" & the result");
  /* Une carte de l'AUTRE fond n'est pas concernée. */
  essai("delete card \"Encore\"\n"
        "  put the number of cards & \"/\" & the result");

  printf("=== la copie emporte les verrous ===\n");
  {
    Object *c = carte(st,"Une");
    if (c) { c->cant_delete = 1; c->dont_search = 1; }
    hc_copy_card(c);
    Object *colle = hc_paste_card(st);
    printf("   collée : dontSearch=%d cantDelete=%d\n",
           colle ? colle->dont_search : -1, colle ? colle->cant_delete : -1);
  }

  printf("=== enregistrement puis relecture ===\n");
  printf("   hc_save = %d (0 = réussi)\n", hc_save(st,"/tmp/hc_verrous.stack"));
  Object *relu = hc_load("/tmp/hc_verrous.stack");
  if (!relu) { printf("   relecture ÉCHOUÉE\n"); hc_free(st); return 1; }
  for (int i=0;i<relu->nparts;i++){
    Object *o=relu->parts[i];
    printf("   %s %-8s dontSearch=%d cantDelete=%d\n",
           o->type==OBJ_CARD?"carte":"fond ", o->name?o->name:"",
           o->dont_search, o->cant_delete);
  }
  hc_free(relu);
  hc_unregister_stack(st);hc_free(st);return 0;}
