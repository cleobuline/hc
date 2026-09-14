/* Graph Maker 2.2 : la pile ENTIÈRE, ses trois scripts d'époque.
 *
 * Les trois couches sont les vraies, données par l'auteur et rangées telles
 * quelles dans donnees/ : le script de PILE, celui du FOND, celui de la CARTE.
 * Rien n'est réécrit — les coquilles de l'original en font partie, dont
 * « set the foreclor to green » et un getMaxValue défini DEUX fois.
 *
 * C'est ce qui fait la valeur de ce harnais : la chaîne de messages complète,
 * carte -> fond -> pile, avec « pass », des globales, et neuf gestionnaires
 * auxiliaires que la carte appelle et qui vivent dans le fond. Jusqu'ici je
 * les avais écrits moi-même ; ce sont maintenant ceux de la pile.
 *
 * L'hôte ne dessine rien : il COMPTE. Un camembert, c'est un nombre de traits,
 * de remplissages et de titres, et ces nombres se vérifient à la main.
 *
 * Ce que ce harnais exerce et qu'aucun autre ne réunissait :
 *
 *   wrongStack()   « the value of word 2 of the long name of me », l'idiome
 *                  canonique pour retrouver le nom de sa propre pile. Il ne
 *                  marche que si « word » traite un texte entre guillemets
 *                  comme UN mot.
 *   openStack      « set hilite of bg btn id 68 of cd 1 of bg "Graphs" » :
 *                  trois niveaux de portée dans une seule référence.
 *   openCard       lit un rect dans un champ caché, le pose sur deux boutons,
 *                  puis « send "drawChart" to this card ».
 *   les globales   déclarées dans trois gestionnaires différents.
 *   deux « if »    dont le « then » est sur la ligne suivante.
 *   « pass »       depuis la carte vers le fond vers la pile.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

/* ---- ce que l'hôte compte ---- */
static long nDrag, nClick, nType, nMenu;
static char dernierOutil[32] = "browse";
static char dernierMotif[16] = "";
static char dernierCurseur[16] = "";
static int  nReponses;
static char premiereReponse[128] = "";

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : "");
  else if (k == HC_MSG) printf("   [msg] %s\n", t ? t : ""); }

static void mon_outil(const char *n)
{ snprintf(dernierOutil, sizeof dernierOutil, "%s", n ? n : ""); }
static void mon_drag(int a,int b,int c,int d,const char *m)
{ (void)a;(void)b;(void)c;(void)d;(void)m; nDrag++; }
static void mon_click(int x,int y,const char *m)
{ (void)x;(void)y;(void)m; nClick++; }
static void mon_type(const char *t,const char *m)
{ (void)t;(void)m; nType++; }
static void mon_menu(const char *i) { (void)i; nMenu++; }

static char gTextFont[32] = "geneva", gTextAlign[16] = "left",
            gTextStyle[32] = "plain";
static int  gTextSize = 12, gTextHeight = 16, gFilled = 0;

static void mon_set(const char *n, const char *v)
{
    if (!n || !v) return;
    if (!strcasecmp(n, "pattern"))         snprintf(dernierMotif, sizeof dernierMotif, "%s", v);
    else if (!strcasecmp(n, "cursor"))     snprintf(dernierCurseur, sizeof dernierCurseur, "%s", v);
    else if (!strcasecmp(n, "textFont"))   snprintf(gTextFont, sizeof gTextFont, "%s", v);
    else if (!strcasecmp(n, "textAlign"))  snprintf(gTextAlign, sizeof gTextAlign, "%s", v);
    else if (!strcasecmp(n, "textStyle"))  snprintf(gTextStyle, sizeof gTextStyle, "%s", v);
    else if (!strcasecmp(n, "textSize"))   gTextSize = atoi(v);
    else if (!strcasecmp(n, "textHeight")) gTextHeight = atoi(v);
    else if (!strcasecmp(n, "filled"))     gFilled = !strcasecmp(v, "true");
}

static const char *mon_get(const char *n)
{
    static char b[32];
    if (!n) return NULL;
    if (!strcasecmp(n, "tool"))       { snprintf(b, sizeof b, "%s tool", dernierOutil); return b; }
    if (!strcasecmp(n, "textFont"))   return gTextFont;
    if (!strcasecmp(n, "textAlign"))  return gTextAlign;
    if (!strcasecmp(n, "textStyle"))  return gTextStyle;
    if (!strcasecmp(n, "textSize"))   { snprintf(b, sizeof b, "%d", gTextSize); return b; }
    if (!strcasecmp(n, "textHeight")) { snprintf(b, sizeof b, "%d", gTextHeight); return b; }
    if (!strcasecmp(n, "filled"))     return gFilled ? "true" : "false";
    if (!strcasecmp(n, "pattern"))    return dernierMotif;
    /* checkUserCancel interroge la souris : sans écran, elle n'est jamais
     * cliquée — sinon tout le tracé s'arrêterait par « exit to hyperCard ». */
    if (!strcasecmp(n, "mouseClick")) return "false";
    if (!strcasecmp(n, "mouse"))      return "up";
    return NULL;
}

static const char *ma_reponse(const char *p, const char *b1, const char *b2, const char *b3)
{
    (void)b2; (void)b3;
    nReponses++;
    if (!premiereReponse[0] && p) snprintf(premiereReponse, sizeof premiereReponse, "%s", p);
    return b1 ? b1 : "OK";
}

/* ---- accès aux objets, par parcours : le noyau n'expose pas de recherche ---- */
static Object *bgFond;
static Object *trouve(Object *couche, ObjType type, const char *nom)
{
    for (int i = 0; couche && i < couche->nparts; i++) {
        Object *o = couche->parts[i];
        if (o->type == type && o->name && strcasecmp(o->name, nom) == 0) return o;
    }
    return NULL;
}
static const char *texte_de(const char *nom)
{
    Object *f = trouve(bgFond, OBJ_FIELD, nom);
    return f ? hc_field_text(f) : "(champ introuvable)";
}

static char *lire(const char *chemin)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) { printf("   *** %s introuvable ***\n", chemin); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    if (!s) { fclose(f); return NULL; }
    size_t lu = fread(s, 1, (size_t)n, f); s[lu] = '\0'; fclose(f);
    return s;
}

static Object *bouton(Object *ou, const char *nom, int x, int y, int w, int hh)
{ Object *b = hc_new_button(ou, nom); b->x = x; b->y = y; b->w = w; b->h = hh; return b; }

static void compte_rendu(const char *quand)
{
    printf("   %-24s traces=%-4ld clics=%-4ld frappes=%-3ld menus=%-2ld "
           "outil=%-10s motif=%-3s curseur=%s\n",
           quand, nDrag, nClick, nType, nMenu, dernierOutil,
           dernierMotif[0] ? dernierMotif : "-",
           dernierCurseur[0] ? dernierCurseur : "-");
}

static void remet_a_zero(void)
{ nDrag = nClick = nType = nMenu = 0; }

int main(int argc, char **argv)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne; h.answer = ma_reponse;
    h.global_get = mon_get; h.global_set = mon_set;
    h.choose_tool = mon_outil; h.drag = mon_drag; h.click_at = mon_click;
    h.type_text = mon_type; h.do_menu = mon_menu;
    hc_set_host(&h);

    const char *dossier = argc > 1 ? argv[1] : "donnees";
    char chemin[512];
    snprintf(chemin, sizeof chemin, "%s/graphmaker_pile.txt", dossier);
    char *sPile = lire(chemin);
    snprintf(chemin, sizeof chemin, "%s/graphmaker_fond.txt", dossier);
    char *sFond = lire(chemin);
    snprintf(chemin, sizeof chemin, "%s/graphmaker.txt", dossier);
    char *sCarte = lire(chemin);
    if (!sPile || !sFond || !sCarte) return 1;

    /* Le fond s'appelle « Graphs » : openStack le cherche par ce nom. */
    Object *st = hc_new_stack("Graph Maker"); hc_register_stack(st);
    st->w = 512; st->h = 342;
    hc_set_script(st, sPile);

    Object *bg = hc_new_background(st, "Graphs");
    bgFond = bg;
    hc_set_script(bg, sFond);

    const char *champs[] = { "Data", "Labels", "Labels Title", "Graph Title",
                             "Units", "Total", "Percents", "Percent Total",
                             "Drew Data", "Graph Rect", "Graph Info", NULL };
    for (int i = 0; champs[i]; i++) hc_new_field(bg, champs[i]);

    bouton(bg, "Frame",        20,  30, 240, 240);
    bouton(bg, "Grow",        252, 262,  12,  12);
    bouton(bg, "Legend",      280,  30, 120,  60);
    bouton(bg, "GrowLegend",  388,  78,  12,  12);
    bouton(bg, "Total:",       20, 290,  50,  16);
    bouton(bg, "Total Frame",  70, 290,  80,  16);
    bouton(bg, "About Graphs",420, 290,  80,  16);
    /* openStack éteint « bg btn id 68 of cd 1 of bg "Graphs" » : le bouton de
     * partage des données. Trois niveaux de portée dans une seule référence. */
    Object *b68 = bouton(bg, "Shared Data", 420, 10, 80, 16);
    hc_set_id(b68, 68);
    b68->hilite = 1;

    Object *c = hc_new_card(st, bg, "Pie");
    hc_set_script(c, sCarte);
    free(sPile); free(sFond); free(sCarte);
    hc_set_current_card(c);

    /* LE TEXTE APRÈS LA CARTE. Un champ de fond non partagé porte un texte PAR
     * CARTE — chaque carte de Graph Maker est un graphique différent. */
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Data"),
                      "25\n40\n10\n5\n20");
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Labels"),
                      "Pommes\nPoires\nPrunes\nCerises\nAbricots");
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Labels Title"), "Recolte 1987");
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Graph Title"),  "Legende");
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Graph Rect"),
                      "20,30,260,270\n252,262,264,274");

    hc_do("get 1");   /* la banniere v3 sort ici, pas au milieu d'un releve */

    puts("=== le script de PILE : wrongStack, et la portee a trois niveaux ===");
    printf("   hilite de bg btn id 68 avant : %s\n",
           hc_hilite_of(b68, c) ? "true" : "false");
    hc_env_message("startUp");          /* ne fait rien, mais ne doit pas planter */
    hc_send(st, "openStack");
    printf("   hilite de bg btn id 68 apres : %s   (openStack l'eteint)\n",
           hc_hilite_of(b68, c) ? "true" : "false");
    printf("   the userLevel apres openStack :\n");
    hc_do("put the userLevel");

    puts("\n=== updateTotals : les pourcentages, par les VRAIES fonctions du fond ===");
    hc_send(c, "updateTotals");
    printf("   Total         = [%s]\n", texte_de("Total"));
    printf("   Percents      = [%s]\n", texte_de("Percents"));
    printf("   Percent Total = [%s]\n", texte_de("Percent Total"));

    puts("\n=== drawChart : le camembert et sa legende ===");
    hc_v3_bilan_remise_a_zero();
    remet_a_zero();
    compte_rendu("avant");
    hc_send(c, "drawChart");
    compte_rendu("apres");
    printf("   Drew Data     = [%s]\n", texte_de("Drew Data"));
    {
        Object *lg = trouve(bg, OBJ_BUTTON, "Legend");
        Object *gl = trouve(bg, OBJ_BUTTON, "GrowLegend");
        printf("   rect Legend   = %d,%d,%d,%d\n", lg->x, lg->y,
               lg->x + lg->w, lg->y + lg->h);
        printf("   coin GrowLegend suit : %s\n",
               (gl->x + gl->w == lg->x + lg->w && gl->y + gl->h == lg->y + lg->h)
               ? "oui" : "NON");
    }

    puts("\n=== closeCard : saveRect ecrit le rect dans le champ cache ===");
    hc_send(c, "closeCard");
    printf("   Graph Rect    = [%s]\n", texte_de("Graph Rect"));
    printf("   Graph Info visible : %s\n",
           trouve(bg, OBJ_FIELD, "Graph Info")->visible ? "oui" : "non");

    puts("\n=== openCard : relit le rect, et redessine si besoin ===");
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Drew Data"), "");
    remet_a_zero();
    hc_send(c, "openCard");
    compte_rendu("apres openCard");
    printf("   Drew Data     = [%s]   (openCard a relance drawChart)\n",
           texte_de("Drew Data"));

    puts("\n=== sans donnees, il le dit au lieu de dessiner ===");
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Data"), "");
    nReponses = 0; premiereReponse[0] = '\0';
    remet_a_zero();
    hc_send(c, "drawChart");
    printf("   reponses demandees : %d\n", nReponses);
    printf("   message            : [%s]\n", premiereReponse);
    compte_rendu("rien dessine");

    puts("\n=== closeStack ===");
    hc_send(st, "closeStack");
    printf("   the userLevel, rendu par closeStack :\n");
    hc_do("put the userLevel");

    puts("\n=== ce que la v3 renvoie encore a l'ancien moteur ===");
    hc_v3_bilan();

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
