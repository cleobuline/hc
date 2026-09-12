/* LE CONTRAT DE MORT : tout objet libéré doit être annoncé à l'hôte, une fois.
 *
 * C'est le contrat le plus important entre le noyau et Cocoa, et le seul dont
 * la violation fait PLANTER l'application au lieu de rendre un résultat faux.
 * L'interface garde des pointeurs — l'objet survolé, le bouton pressé, la
 * sélection, la cible du panneau de police : neuf emplacements dans HCview.m.
 * Si le noyau libère un objet sans le dire, le prochain survol de souris lit
 * de la mémoire rendue.
 *
 * Ce défaut a coûté trois plantages successifs sur « delete me », et le
 * harnais mortobjet ne couvrait que ce cas-là. Celui-ci vérifie TOUS les
 * chemins de libération, et il vérifie l'exhaustivité : chaque objet créé est
 * inscrit, chaque avis le raye, et il ne doit rien rester à la fin.
 *
 * Le compte se fait sur des POINTEURS COMPARÉS, jamais déréférencés après
 * l'avis : c'est justement ce qu'un hôte correct doit savoir faire. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

#define MAX 256
static Object *g_suivis[MAX];      /* créés, en attente d'un avis */
static char    g_nom[MAX][40];     /* leur nom, retenu AVANT la mort */
static int     g_n = 0;
static int     g_avis_total = 0;
static int     g_avis_inconnu = 0; /* avis pour un objet qu'on ne suivait pas */
static int     g_avis_double = 0;  /* deux avis pour le même objet */
static Object *g_deja[MAX]; static int g_ndeja = 0;

static void suis(Object *o, const char *nom){
  if (g_n >= MAX) { printf("   TROP D'OBJETS SUIVIS\n"); return; }
  g_suivis[g_n] = o;
  snprintf(g_nom[g_n], sizeof g_nom[0], "%s", nom);
  g_n++;
}
static void mon_mort(Object *o){
  g_avis_total++;
  for (int i = 0; i < g_ndeja; i++)
    if (g_deja[i] == o) { g_avis_double++; return; }
  if (g_ndeja < MAX) g_deja[g_ndeja++] = o;
  int connu = 0;
  for (int i = 0; i < g_n; i++)
    if (g_suivis[i] == o) { g_suivis[i] = NULL; connu = 1; break; }
  if (!connu) g_avis_inconnu++;
}
static int restants(void){
  int r = 0;
  for (int i = 0; i < g_n; i++) if (g_suivis[i]) r++;
  return r;
}
static void bilan(const char *titre){
  printf("   %-34s avis %3d, non annoncés %d, doubles %d, inconnus %d\n",
         titre, g_avis_total, restants(), g_avis_double, g_avis_inconnu);
  for (int i = 0; i < g_n; i++)
    if (g_suivis[i]) printf("      PAS ANNONCÉ : %s\n", g_nom[i]);
}
static void raz(void){
  g_n = 0; g_avis_total = 0; g_avis_inconnu = 0; g_avis_double = 0; g_ndeja = 0;
  memset(g_suivis, 0, sizeof g_suivis);
}
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;(void)k;(void)t;}

/* Une pile peuplée, dont chaque objet est suivi. */
static Object *peuple(const char *nom, int ncartes, int nparts){
  Object *st = hc_new_stack(nom); hc_register_stack(st);
  suis(st, "la pile");
  Object *bg = hc_new_background(st, "F");
  suis(bg, "le fond");
  for (int i = 0; i < nparts; i++) {
    char n[32]; snprintf(n,sizeof n,"bgbouton%d",i+1);
    Object *o = hc_new_button(bg, n);
    char e[40]; snprintf(e,sizeof e,"%s (du fond)",n); suis(o, e);
  }
  for (int k = 0; k < ncartes; k++) {
    char cn[32]; snprintf(cn,sizeof cn,"C%d",k+1);
    Object *c = hc_new_card(st, bg, cn);
    char e[40]; snprintf(e,sizeof e,"carte %s",cn); suis(c, e);
    for (int i = 0; i < nparts; i++) {
      char n[32]; snprintf(n,sizeof n,"%s.champ%d",cn,i+1);
      Object *f = hc_new_field(c, n);
      suis(f, n);
    }
  }
  return st;
}

int main(void){
  setbuf(stdout,NULL);
  static HcHost h; memset(&h,0,sizeof h);
  h.line = ma_ligne; h.object_gone = mon_mort;
  hc_set_host(&h);

  printf("── hc_free de la pile : tout doit être annoncé\n");
  {
    raz();
    Object *st = peuple("A", 3, 2);
    for (int i = 0; i < st->nparts; i++)
      if (st->parts[i]->type == OBJ_CARD) { hc_set_current_card(st->parts[i]); break; }
    hc_unregister_stack(st); hc_free(st);
    bilan("pile de 3 cartes, 2 parties");
  }

  printf("\n── delete card : la carte ET ses parties\n");
  {
    raz();
    Object *st = peuple("B", 3, 2);
    Object *c1 = NULL;
    for (int i = 0; i < st->nparts; i++)
      if (st->parts[i]->type == OBJ_CARD) { c1 = st->parts[i]; break; }
    hc_set_current_card(c1);
    int avant = g_avis_total;
    hc_delete_card(c1);
    printf("   delete card a produit %d avis (attendu 3 : la carte et ses 2 champs)\n",
           g_avis_total - avant);
    hc_unregister_stack(st); hc_free(st);
    bilan("après hc_free du reste");
  }

  printf("\n── delete part : un champ seul\n");
  {
    raz();
    Object *st = peuple("C", 1, 2);
    Object *c = NULL;
    for (int i = 0; i < st->nparts; i++)
      if (st->parts[i]->type == OBJ_CARD) { c = st->parts[i]; break; }
    hc_set_current_card(c);
    int avant = g_avis_total;
    hc_delete_part(c->parts[0]);
    printf("   delete part a produit %d avis (attendu 1)\n", g_avis_total - avant);
    hc_unregister_stack(st); hc_free(st);
    bilan("après hc_free du reste");
  }

  printf("\n── « delete me » depuis un script\n");
  {
    raz();
    Object *st = peuple("D", 1, 1);
    Object *c = NULL;
    for (int i = 0; i < st->nparts; i++)
      if (st->parts[i]->type == OBJ_CARD) { c = st->parts[i]; break; }
    hc_set_current_card(c);
    Object *b = hc_new_button(c,"suicide"); suis(b,"le bouton suicide");
    hc_set_script(b,"on mouseUp\n  delete me\nend mouseUp\n");
    int avant = g_avis_total;
    hc_send(b,"mouseUp");
    printf("   « delete me » a produit %d avis (attendu 1)\n", g_avis_total - avant);
    printf("   hc_object_is_live après : %d (attendu 0)\n", hc_object_is_live(b));
    hc_unregister_stack(st); hc_free(st);
    bilan("après hc_free du reste");
  }

  printf("\n── la dernière carte d'un fond emporte le fond\n");
  {
    raz();
    Object *st = hc_new_stack("E"); hc_register_stack(st); suis(st,"la pile");
    Object *bg1 = hc_new_background(st,"F1"); suis(bg1,"fond F1");
    Object *bg2 = hc_new_background(st,"F2"); suis(bg2,"fond F2");
    Object *c1 = hc_new_card(st,bg1,"seule");  suis(c1,"carte de F1");
    Object *c2 = hc_new_card(st,bg2,"autre");  suis(c2,"carte de F2");
    Object *c3 = hc_new_card(st,bg2,"encore"); suis(c3,"2e carte de F2");
    hc_set_current_card(c2);
    int avant = g_avis_total;
    hc_delete_card(c1);
    printf("   supprimer la seule carte de F1 : %d avis (la carte + son fond)\n",
           g_avis_total - avant);
    hc_unregister_stack(st); hc_free(st);
    bilan("après hc_free du reste");
  }
  return 0;}
