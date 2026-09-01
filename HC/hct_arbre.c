/* hct_arbre.c — nœuds, réserve mémoire, affichage. Voir hct_arbre.h. */

#include "hct_arbre.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Un bloc de la réserve. Les nœuds y sont posés à la suite ; quand un bloc
 * est plein on en enchaîne un autre. Les tableaux de fils sont alloués à
 * part, puisqu'ils grandissent, mais leur pointeur est retenu pour être
 * libéré avec la réserve. */
#define PAR_BLOC 128

/* Deux listes chaînées : les blocs de nœuds, et les tableaux de fils.
 * Les seconds sont enregistrés un par un, sans limite de nombre. */
struct HctBloc {
    HctNoeud  noeuds[PAR_BLOC];
    int       n;
    HctBloc  *suivant;
};

struct HctAlloc {
    void     *p;
    HctAlloc *suivant;
};

static HctBloc *bloc_neuf(HctBloc *suivant)
{
    HctBloc *b = calloc(1, sizeof *b);
    if (b) b->suivant = suivant;
    return b;
}

/* Retient une allocation pour la libérer avec la réserve. */
static int retiens(HctReserve *r, void *p)
{
    HctAlloc *a = malloc(sizeof *a);
    if (!a) return 0;
    a->p = p;
    a->suivant = r->allocs;
    r->allocs = a;
    return 1;
}

HctNoeud *hct_noeud(HctReserve *r, HctGenreNoeud genre, HctJeton jeton)
{
    if (!r->tete || r->tete->n == PAR_BLOC) {
        HctBloc *b = bloc_neuf(r->tete);
        if (!b) return NULL;
        r->tete = b;
    }
    HctNoeud *n = &r->tete->noeuds[r->tete->n++];
    memset(n, 0, sizeof *n);
    n->genre = genre;
    n->jeton = jeton;
    r->nnoeuds++;
    return n;
}

/* Ajoute un fils. Pas de realloc : on alloue un tableau neuf et on recopie.
 * L'ancien reste enregistré et sera libéré avec la réserve.
 *
 * C'est volontairement naïf. La version avec realloc devait retrouver
 * l'ancien pointeur pour mettre à jour sa trace — donc lire un pointeur que
 * realloc venait de libérer, et fuir en silence si la recherche échouait.
 * Ici le gaspillage est borné (la somme des puissances de deux vaut moins de
 * deux fois la taille finale) et il n'y a rien à comprendre. */
int hct_ajoute_fils(HctReserve *r, HctNoeud *pere, HctNoeud *fils)
{
    if (!pere || !fils) return 0;

    int cap = 1;
    while (cap < pere->nfils + 1) cap *= 2;

    int ancienne_cap = 1;
    while (ancienne_cap < pere->nfils) ancienne_cap *= 2;

    if (pere->nfils == 0 || cap > ancienne_cap) {
        HctNoeud **neuf = malloc((size_t)cap * sizeof *neuf);
        if (!neuf) return 0;
        if (!retiens(r, neuf)) { free(neuf); return 0; }
        for (int i = 0; i < pere->nfils; i++) neuf[i] = pere->fils[i];
        pere->fils = neuf;
    }
    pere->fils[pere->nfils++] = fils;
    return 1;
}

const char *hct_reserve_texte(HctReserve *r, const char *s, int len)
{
    char *p = malloc((size_t)len + 1);
    if (!p) return NULL;
    memcpy(p, s, (size_t)len);
    p[len] = 0;
    if (!retiens(r, p)) { free(p); return NULL; }
    return p;
}

void hct_reserve_libere(HctReserve *r)
{
    for (HctBloc *b = r->tete; b; ) { HctBloc *s = b->suivant; free(b); b = s; }
    for (HctAlloc *a = r->allocs; a; ) { HctAlloc *s = a->suivant; free(a->p); free(a); a = s; }
    r->tete = NULL;
    r->allocs = NULL;
    r->nnoeuds = 0;
}

/* --------------------------------------------------- étendue dans le source */

/* Parcours : on retient l'octet le plus à gauche et le plus à droite vus dans
 * le sous-arbre. Les nœuds n'étant pas forcément dans l'ordre du texte — un
 * mot-clé consommé après coup peut précéder son opérande dans l'arbre —, on
 * ne peut pas se contenter du premier et du dernier fils. */
static void etendue_parcours(const HctNoeud *n, const char **min, const char **max)
{
    if (!n) return;

    const HctJeton *j = &n->jeton;
    if (j->deb && j->len > 0 && j->genre != HCT_EOL && j->genre != HCT_FIN) {
        if (!*min || j->deb < *min) *min = j->deb;
        if (!*max || j->deb + j->len > *max) *max = j->deb + j->len;
    }
    for (int i = 0; i < n->nfils; i++)
        etendue_parcours(n->fils[i], min, max);
}

int hct_noeud_etendue(const HctNoeud *n, const char **deb, int *len)
{
    const char *min = NULL, *max = NULL;
    etendue_parcours(n, &min, &max);
    if (!min || !max || max <= min) return 0;

    /* Étendre jusqu'à la fin de la LIGNE, et non jusqu'au dernier jeton retenu.
     *
     * Certains caractères ne deviennent le jeton d'aucun nœud : l'analyseur les
     * consomme et les oublie. Les parenthèses d'un appel en font partie, si
     * bien que « get item it of monthNameData() » se reconstituait sans elles.
     * L'ancien interpréteur y lisait alors une variable jamais affectée, qui
     * vaut son propre nom en HyperTalk — le calendrier recevait la chaîne
     * « monthNameData » au lieu de la liste des mois, et échouait sans un mot.
     *
     * Une instruction occupant sa ligne entière, aller jusqu'au saut de ligne
     * rend tout ce que l'auteur a écrit, ponctuation comprise. On s'arrête
     * aussi au point-virgule, qui sépare deux instructions sur une même ligne,
     * et l'on retire les blancs de fin. */
    const char *fin = max;
    while (*fin && *fin != '\n' && *fin != '\r' && *fin != ';') {
        /* Arrêt devant un commentaire. Le lexer les avale sans en faire de
         * jetons, si bien qu'aller jusqu'au bout de la ligne les ramassait :
         * « saveRect -- saves current frame size » repartait tel quel à
         * l'ancien interpréteur, qui lisait « -- saves… » comme les arguments
         * du message et se plaignait d'une expression attendue.
         *
         * Le scan ne commence qu'APRÈS le dernier jeton du nœud, donc un
         * « -- » rencontré ici est forcément un commentaire, jamais deux moins
         * consécutifs d'une expression. */
        if (fin[0] == '-' && fin[1] == '-') break;

        /* Arrêt devant « else » : dans « if C then go home else beep », la
         * branche « then » ne va que jusque-là. Sans cette borne, elle avalait
         * le reste et l'ancien interpréteur cherchait une carte nommée
         * « home else beep ».
         *
         * « else » n'entre dans aucun motif de commande : le rencontrer hors
         * d'une chaîne veut donc toujours dire qu'on est sorti de l'instruction.
         * Le test des bords évite d'arrêter sur un nom qui le contient, comme
         * « elsewhere ». */
        if ((fin[0] == 'e' || fin[0] == 'E') &&
            (fin == min || fin[-1] == ' ' || fin[-1] == '\t') &&
            (fin[1] == 'l' || fin[1] == 'L') &&
            (fin[2] == 's' || fin[2] == 'S') &&
            (fin[3] == 'e' || fin[3] == 'E') &&
            (fin[4] == '\0' || fin[4] == ' ' || fin[4] == '\t' ||
             fin[4] == '\n' || fin[4] == '\r'))
            break;
        fin++;
    }
    while (fin > max && (fin[-1] == ' ' || fin[-1] == '\t')) fin--;
    if (fin > max) max = fin;

    *deb = min;
    *len = (int)(max - min);
    return 1;
}

/* ------------------------------------------------------------------ noms */

const char *hct_genre_noeud_nom(HctGenreNoeud g)
{
    switch (g) {
        case HCTN_NOMBRE:  return "nombre";
        case HCTN_CHAINE:  return "chaine";
        case HCTN_IDENT:   return "ident";
        case HCTN_BINAIRE: return "binaire";
        case HCTN_UNAIRE:  return "unaire";
        case HCTN_CHUNK:   return "chunk";
        case HCTN_OF:      return "of";
        case HCTN_APPEL:   return "appel";
        case HCTN_LISTE:   return "liste";
        case HCTN_OBJET:   return "objet";
        case HCTN_COMMANDE:return "commande";
        case HCTN_MESSAGE: return "message";
        case HCTN_MOTCLE:  return "motcle";
        case HCTN_BLOC:    return "bloc";
        case HCTN_SI:      return "si";
        case HCTN_REPETE:  return "repete";
        case HCTN_GESTIONNAIRE: return "gestionnaire";
        case HCTN_ERREUR:  return "ERREUR";
    }
    return "?";
}

const char *hct_type_objet_nom(HctTypeObjet t)
{
    static const char *n[] = { "stack", "background", "card", "button",
                               "field", "part", "me", "target", "message" };
    return (t >= 0 && t <= HCT_OBJ_MESSAGE) ? n[t] : "?";
}

const char *hct_portee_nom(HctPortee p)
{
    static const char *n[] = { "", "card", "bg" };
    return (p >= 0 && p <= HCT_PORTEE_FOND) ? n[p] : "?";
}

const char *hct_sorte_chunk_nom(HctSorteChunk s)
{
    static const char *n[] = { "char", "word", "item", "line" };
    return (s >= 0 && s <= HCT_CH_LINE) ? n[s] : "?";
}

const char *hct_ordinal_nom(HctOrdinal o)
{
    static const char *n[] = {
        "", "first", "second", "third", "fourth", "fifth", "sixth",
        "seventh", "eighth", "ninth", "tenth", "middle", "last", "any"
    };
    return (o >= 0 && o <= HCT_ORD_QUELCONQUE) ? n[o] : "?";
}

/* ------------------------------------------------------------- affichage */

void hct_arbre_montre(const HctNoeud *n, int profondeur, void *sortie)
{
    FILE *f = sortie ? sortie : stdout;
    if (!n) { fprintf(f, "%*s(vide)\n", profondeur * 2, ""); return; }

    char buf[128];
    fprintf(f, "%*s%s", profondeur * 2, "", hct_genre_noeud_nom(n->genre));

    switch (n->genre) {
        case HCTN_NOMBRE:
        case HCTN_CHAINE:
        case HCTN_IDENT:
            fprintf(f, " [%s]", hct_texte(&n->jeton, buf, sizeof buf));
            break;
        case HCTN_BINAIRE:
        case HCTN_UNAIRE:
        case HCTN_COMMANDE:
        case HCTN_MESSAGE:
        case HCTN_MOTCLE:
        case HCTN_SI:
        case HCTN_REPETE:
        case HCTN_GESTIONNAIRE:
            fprintf(f, " %s", n->op ? n->op : "?");
            break;
        case HCTN_CHUNK:
            fprintf(f, " %s", hct_sorte_chunk_nom(n->sorte));
            if (n->ordinal) fprintf(f, " %s", hct_ordinal_nom(n->ordinal));
            break;
        case HCTN_OBJET:
            if (n->portee) fprintf(f, " %s.", hct_portee_nom(n->portee));
            else fprintf(f, " ");
            fprintf(f, "%s", hct_type_objet_nom(n->typeobj));
            if (n->designateur == HCT_DES_ID)      fprintf(f, " par-id");
            else if (n->designateur == HCT_DES_NOM)  fprintf(f, " par-nom");
            else if (n->designateur == HCT_DES_RANG) fprintf(f, " par-rang");
            else if (n->designateur == HCT_DES_ORDINAL)
                fprintf(f, " %s", hct_ordinal_nom(n->ordinal));
            else if (n->designateur == HCT_DES_RELATIF)
                fprintf(f, " %s", n->relatif == HCT_REL_CE ? "this"
                                : n->relatif == HCT_REL_SUIVANT ? "next" : "prev");
            break;
        case HCTN_ERREUR:
            fprintf(f, " << %s", n->msg ? n->msg : "?");
            break;
        default:
            break;
    }
    fprintf(f, "   (%d:%d)\n", n->jeton.ligne, n->jeton.col);

    for (int i = 0; i < n->nfils; i++)
        hct_arbre_montre(n->fils[i], profondeur + 1, f);
}
