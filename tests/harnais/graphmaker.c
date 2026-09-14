/* Graph Maker 2.2 : un VRAI script d'epoque, execute pour de bon.
 *
 * Le script de carte est dans donnees/graphmaker.txt, donne tel quel — les
 * coquilles de l'original comprises, et c'est precisement ce qu'un interprete
 * doit savoir traverser.
 *
 * Il n'est pas autonome : il appelle neuf gestionnaires qui vivent dans le
 * script de PILE. On les ecrit ici EN HYPERTALK plutot que de les simuler en
 * C, pour que l'appel script -> script soit lui aussi exerce — c'est la moitie
 * de ce qu'on veut mesurer.
 *
 * L'hote ne dessine rien : il COMPTE. Un camembert, c'est un nombre de traits,
 * de remplissages et de titres, et ces nombres se verifient.
 *
 * Ce que ce harnais exerce, et qu'aucun autre ne reunissait :
 *
 *   des gestionnaires a PARAMETRES multiples, appeles sans parentheses
 *     — « doPieChart chartRect,data,graphTitle »
 *   des fonctions utilisateur dans une expression — « getPattern(slice) »
 *   « rect of bg btn "Frame" » : une propriete SANS « the », sur un objet de
 *     fond, ecrite ET lue
 *   l'ecriture dans un ITEM d'une variable — « put minRight into item 3 of
 *     legendRect » — puis « set rect of ... to legendRect »
 *   « set bottomRight of A to bottomRight of B »
 *   « the value of theLine », « next repeat », « pass », « exit <nom> »
 *   les outils de peinture : text, oval, line, bucket, rectangle, select
 *   « the textHeight », « set pattern to », « set filled to true »
 *   « bg field data » sans guillemets, et « bg fld » abrege
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>

/* ---- ce que l'hote compte ---- */
static long nDrag, nClick, nType, nMenu, nSon;
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
static void mon_son(const char *n)  { (void)n; nSon++; }

/* Les globales que le script pose et relit. On les retient, c'est tout ce que
 * le noyau demande d'un hote sans ecran. */
static char gTextFont[32] = "geneva", gTextAlign[16] = "left",
            gTextStyle[32] = "plain";
static int  gTextSize = 12, gFilled = 0;

static void mon_set(const char *n, const char *v)
{
    if (!n || !v) return;
    if (!strcasecmp(n, "pattern"))        snprintf(dernierMotif, sizeof dernierMotif, "%s", v);
    else if (!strcasecmp(n, "cursor"))    snprintf(dernierCurseur, sizeof dernierCurseur, "%s", v);
    else if (!strcasecmp(n, "textFont"))  snprintf(gTextFont, sizeof gTextFont, "%s", v);
    else if (!strcasecmp(n, "textAlign")) snprintf(gTextAlign, sizeof gTextAlign, "%s", v);
    else if (!strcasecmp(n, "textStyle")) snprintf(gTextStyle, sizeof gTextStyle, "%s", v);
    else if (!strcasecmp(n, "textSize"))  gTextSize = atoi(v);
    else if (!strcasecmp(n, "filled"))    gFilled = !strcasecmp(v, "true");
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
    if (!strcasecmp(n, "textHeight")) { snprintf(b, sizeof b, "%d", gTextSize * 4 / 3); return b; }
    if (!strcasecmp(n, "filled"))     return gFilled ? "true" : "false";
    if (!strcasecmp(n, "pattern"))    return dernierMotif;
    return NULL;
}

static const char *ma_reponse(const char *p, const char *b1, const char *b2, const char *b3)
{
    (void)b2; (void)b3;
    nReponses++;
    if (!premiereReponse[0] && p) snprintf(premiereReponse, sizeof premiereReponse, "%s", p);
    return b1 ? b1 : "OK";
}

/* Les neuf gestionnaires que le script de carte appelle et qui vivent
 * ailleurs. Ecrits en HyperTalk exprès : l'appel script -> script fait partie
 * de ce qu'on mesure. */
static const char *SCRIPT_PILE =
"on showInfo flag\n"
"  -- de l'affichage : rien a faire sans ecran\n"
"end showInfo\n"
"\n"
"on checkUserCancel\n"
"  -- commande-point : rien a faire sans clavier\n"
"end checkUserCancel\n"
"\n"
"on clearScreen\n"
"  choose select tool\n"
"  doMenu \"Select All\"\n"
"  doMenu \"Clear Picture\"\n"
"  choose browse tool\n"
"end clearScreen\n"
"\n"
"on setFont theFont,theSize,theAlign,theStyle\n"
"  set the textFont to theFont\n"
"  set the textSize to theSize\n"
"  set the textAlign to theAlign\n"
"  set the textStyle to theStyle\n"
"end setFont\n"
"\n"
"function stripReturns txt\n"
"  repeat while the last char of txt is return\n"
"    delete the last char of txt\n"
"  end repeat\n"
"  return txt\n"
"end stripReturns\n"
"\n"
"function validatedData raw\n"
"  put empty into sortie\n"
"  repeat with i = 1 to the number of lines of raw\n"
"    put line i of raw into l\n"
"    if l is empty then next repeat\n"
"    put l & return after sortie\n"
"  end repeat\n"
"  if the last char of sortie is return then delete the last char of sortie\n"
"  return sortie\n"
"end validatedData\n"
"\n"
"function maxChars txt\n"
"  put 0 into m\n"
"  repeat with i = 1 to the number of lines of txt\n"
"    put the number of chars of line i of txt into n\n"
"    if n > m then put n into m\n"
"  end repeat\n"
"  return m\n"
"end maxChars\n"
"\n"
"function rectHeight r\n"
"  return item 4 of r - item 2 of r\n"
"end rectHeight\n"
"\n"
"function rectWidth r\n"
"  return item 3 of r - item 1 of r\n"
"end rectWidth\n";

static char *lire(const char *chemin)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) { printf("   *** %s introuvable ***\n", chemin); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    if (!s) { fclose(f); return NULL; }
    size_t lu = fread(s, 1, (size_t)n, f);
    s[lu] = '\0'; fclose(f);
    return s;
}

/* Le noyau n'expose pas de recherche par nom : on parcourt, comme les autres
 * harnais. Le fond d'abord, puisque tous les champs de Graph Maker y vivent. */
static Object *trouve(Object *couche, ObjType type, const char *nom)
{
    for (int i = 0; couche && i < couche->nparts; i++) {
        Object *o = couche->parts[i];
        if (o->type == type && o->name && strcasecmp(o->name, nom) == 0) return o;
    }
    return NULL;
}
static Object *bgFond;
static const char *texte_de(const char *nom)
{
    Object *f = trouve(bgFond, OBJ_FIELD, nom);
    return f ? hc_field_text(f) : "(champ introuvable)";
}

static void champ(Object *ou, const char *nom, const char *val)
{ Object *f = hc_new_field(ou, nom); if (val) hc_set_field_text(f, val); }

static void bouton(Object *ou, const char *nom, int x, int y, int w, int hh)
{ Object *b = hc_new_button(ou, nom); b->x = x; b->y = y; b->w = w; b->h = hh; }

static void compte_rendu(const char *quand)
{
    printf("   %-22s traces=%-4ld clics=%-4ld frappes=%-3ld menus=%-2ld "
           "outil=%-10s motif=%-3s curseur=%s\n",
           quand, nDrag, nClick, nType, nMenu, dernierOutil,
           dernierMotif[0] ? dernierMotif : "-",
           dernierCurseur[0] ? dernierCurseur : "-");
}

int main(int argc, char **argv)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne; h.answer = ma_reponse;
    h.global_get = mon_get; h.global_set = mon_set;
    h.choose_tool = mon_outil; h.drag = mon_drag; h.click_at = mon_click;
    h.type_text = mon_type; h.do_menu = mon_menu; h.play_sound = mon_son;
    hc_set_host(&h);

    char *script = lire(argc > 1 ? argv[1] : "donnees/graphmaker.txt");
    if (!script) return 1;

    Object *st = hc_new_stack("Graph Maker"); hc_register_stack(st);
    st->w = 512; st->h = 342;
    hc_set_script(st, SCRIPT_PILE);

    Object *bg = hc_new_background(st, "Chart");
    bgFond = bg;
    champ(bg, "Data",          NULL);
    champ(bg, "Labels",        NULL);
    champ(bg, "Labels Title",  NULL);
    champ(bg, "Graph Title",   NULL);
    champ(bg, "Total",         NULL);
    champ(bg, "Percents",      NULL);
    champ(bg, "Percent Total", NULL);
    champ(bg, "Drew Data",     NULL);
    bouton(bg, "Frame",       20,  30, 240, 240);
    bouton(bg, "Legend",     280,  30, 120,  60);
    bouton(bg, "GrowLegend", 388,  78,  12,  12);
    bouton(bg, "Total:",      20, 290,  50,  16);
    bouton(bg, "Total Frame", 70, 290,  80,  16);

    Object *c = hc_new_card(st, bg, "Une");
    hc_set_script(c, script);
    free(script);
    hc_set_current_card(c);

    /* LE TEXTE APRES LA CARTE, ET C'EST TOUT L'INTERET DU MODELE.
     *
     * Un champ de FOND non partage porte un texte PAR CARTE : chaque carte de
     * Graph Maker est un graphique different, avec ses propres donnees. Les
     * remplir avant d'avoir une carte courante ne posait rien nulle part, et
     * « put bg field "Data" into brut » rendait vide — mon montage etait
     * fautif, pas l'interprete. */
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Data"),
                      "25\n40\n10\n5\n20");
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Labels"),
                      "Pommes\nPoires\nPrunes\nCerises\nAbricots");
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Labels Title"), "Recolte 1987");
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Graph Title"),  "Legende");

    hc_do("get 1");   /* la banniere v3 sort ici, pas au milieu d'un releve */

    puts("=== updateTotals : les pourcentages se calculent ===");
    hc_send(c, "updateTotals");
    printf("   Total         = [%s]\n", texte_de("Total"));
    printf("   Percents      = [%s]\n", texte_de("Percents"));
    printf("   Percent Total = [%s]\n", texte_de("Percent Total"));

    puts("\n=== drawChart : le camembert et sa legende ===");
    hc_v3_bilan_remise_a_zero();
    compte_rendu("avant");
    hc_send(c, "drawChart");
    compte_rendu("apres");
    printf("   Drew Data     = [%s]\n", texte_de("Drew Data"));
    {
        Object *lg = trouve(bg, OBJ_BUTTON, "Legend");
        Object *gl = trouve(bg, OBJ_BUTTON, "GrowLegend");
        /* La legende s'agrandit pour tenir les etiquettes, et GrowLegend suit
         * son coin. C'est « set rect of ... to legendRect » apres avoir ecrit
         * dans un ITEM de la variable, puis « set bottomRight of A to
         * bottomRight of B » — deux formes qu'aucun autre harnais n'exerce. */
        printf("   rect Legend   = %d,%d,%d,%d\n", lg->x, lg->y,
               lg->x + lg->w, lg->y + lg->h);
        printf("   coin GrowLegend suit : %s\n",
               (gl->x + gl->w == lg->x + lg->w && gl->y + gl->h == lg->y + lg->h)
               ? "oui" : "NON");
    }

    puts("\n=== openCard / closeCard : les objets se montrent et se cachent ===");
    hc_send(c, "openCard");
    Object *lg = trouve(bg, OBJ_BUTTON, "Legend");
    printf("   apres openCard  : Legend visible = %s\n", lg && lg->visible ? "oui" : "non");
    hc_send(c, "closeCard");
    printf("   apres closeCard : Legend visible = %s\n", lg && lg->visible ? "oui" : "non");

    puts("\n=== sans donnees, il le dit au lieu de dessiner ===");
    hc_set_field_text(trouve(bg, OBJ_FIELD, "Data"), "");
    nReponses = 0; premiereReponse[0] = '\0';
    hc_send(c, "drawChart");
    printf("   reponses demandees : %d\n", nReponses);
    printf("   message            : [%s]\n", premiereReponse);

    puts("\n=== ce que la v3 renvoie encore a l'ancien moteur ===");
    hc_v3_bilan();

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
