/* hc_script.c — le SCRIPT d'un objet : le poser, le normaliser, l'analyser.
 *
 * DEUXIÈME MORCEAU DÉTACHÉ DE hc_core.c, et le plus isolé mesuré jusqu'ici :
 * trois cent quarante-deux lignes pour DEUX symboles partagés, et aucune
 * variable globale ne traverse la frontière. Le presse-papiers, hier, en
 * demandait sept.
 *
 * CE QU'IL CONTIENT, et pourquoi ça va ensemble — un texte de script
 * parcourt toujours les quatre mêmes étapes, dans cet ordre :
 *
 *   1. dup_script       le recopie en normalisant : fins de ligne « \r » des
 *                       Macintosh d'époque, et caractères spéciaux MacRoman
 *                       (≠ ≤ ≥ ¬) traduits en leurs équivalents ASCII ;
 *   2. garde_texte      en garde une copie sûre pour les jetons, qui pointent
 *                       DANS le texte et ne survivraient pas à sa libération ;
 *   3. script_arbre     le lexe et l'analyse, une fois, et range l'arbre dans
 *                       l'objet — c'est le cache qui évite de réanalyser à
 *                       chaque envoi de message ;
 *   4. v3_dis_les_fautes signale ce que l'analyseur a refusé, en nommant la
 *                       ligne et la colonne.
 *
 * hc_set_script les enchaîne, hc_arbre_oublie défait le tout.
 *
 * CE QU'IL NE CONTIENT PAS : l'EXÉCUTION. L'arbre fabriqué ici part dans le
 * pont v3 de hc_core.c, qui le parcourt. La frontière est donc celle du
 * TEXTE : ici on fabrique un arbre à partir de caractères, ailleurs on s'en
 * sert. C'est ce qui explique qu'elle soit si nette.
 *
 * LE DÉPLACEMENT EST MÉCANIQUE. Un seul changement de forme, et il RÉDUIT la
 * surface au lieu de l'élargir : les cinq « emit(HC_ERR, …) » du bloc sont
 * devenus « hc_emet_erreur(…) ». Exporter `emit` lui-même aurait mis un nom
 * de trois lettres, parmi les plus courants qui soient, dans l'espace des
 * symboles partagés avec AppKit et tout ce que l'éditeur de liens rassemble.
 * Un nom préfixé ne coûte rien et ne peut entrer en collision avec personne.
 */
#include "hc_interne.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hct_arbre.h"
#include "hct_lex.h"
#include "hct_bloc.h"
#include "hct_expr.h"

/* Normalise les fins de ligne : « \r\n » (Windows) et « \r » seul (Mac
 * classique) deviennent « \n ». Sans ça, un script en « \r » n'est qu'une
 * seule ligne géante : find_handler reconnaît bien « on mouseUp », mais le
 * corps — cherché après le premier « \n » — est vide. Le gestionnaire est
 * alors annoncé comme traité et ne fait rien, ce qui est très déroutant.
 * Les scripts d'HyperCard d'origine sont tous en « \r ». */
/* Caractères spéciaux du Macintosh. Les scripts d'origine sont encodés en
 * MacRoman ; recopiés depuis un navigateur ils arrivent en UTF-8. On accepte
 * les deux, car on ne peut pas savoir d'où vient la pile :
 *
 *     ¬   0xC2   /  0xC2 0xAC        continuation : la ligne suivante suit
 *     ≠   0xAD   /  0xE2 0x89 0xA0   devient « <> »
 *     ≤   0xB2   /  0xE2 0x89 0xA4   devient « <= »
 *     ≥   0xB3   /  0xE2 0x89 0xA5   devient « >= »
 *
 * L'ambiguïté du 0xC2 se lève seule : en UTF-8 il est toujours suivi d'un
 * octet de continuation (0x80-0xBF), et l'unique séquence qui nous intéresse
 * est 0xC2 0xAC. Un 0xC2 suivi d'autre chose est un ¬ MacRoman ; un 0xC2
 * suivi d'un octet de continuation autre que 0xAC est un caractère UTF-8
 * quelconque (« ² », « ° »…) qu'on recopie intact.
 *
 * La sortie peut être plus longue que l'entrée (un octet « ≠ » devient deux),
 * d'où l'allocation au double. */
static int is_utf8_cont(unsigned char c) { return c >= 0x80 && c <= 0xBF; }

static char *dup_script(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = (char *)malloc(2 * n + 2);
    if (!d) return NULL;
    char *w = d;

    for (const unsigned char *p = (const unsigned char *)s; *p; ) {
        /* --- continuation de ligne --- */
        int cont = 0;
        if (p[0] == 0xC2 && p[1] == 0xAC)              { cont = 1; p += 2; }
        else if (p[0] == 0xC2 && !is_utf8_cont(p[1]))  { cont = 1; p += 1; }
        if (cont) {
            /* avaler les blancs puis la fin de ligne : les deux lignes n'en
             * font plus qu'une, séparées par une espace. */
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\r') { p++; if (*p == '\n') p++; }
            else if (*p == '\n') p++;
            *w++ = ' ';
            continue;
        }

        /* --- opérateurs de comparaison --- */
        if (p[0] == 0xE2 && p[1] == 0x89) {
            if (p[2] == 0xA0) { *w++ = '<'; *w++ = '>'; p += 3; continue; }
            if (p[2] == 0xA4) { *w++ = '<'; *w++ = '='; p += 3; continue; }
            if (p[2] == 0xA5) { *w++ = '>'; *w++ = '='; p += 3; continue; }
        }
        if (p[0] == 0xAD) { *w++ = '<'; *w++ = '>'; p++; continue; }
        if (p[0] == 0xB2) { *w++ = '<'; *w++ = '='; p++; continue; }
        if (p[0] == 0xB3) { *w++ = '>'; *w++ = '='; p++; continue; }

        /* --- caractère UTF-8 multi-octets : recopie intégrale --- */
        if (p[0] >= 0xC2 && p[0] <= 0xF4) {
            int len = p[0] >= 0xF0 ? 4 : p[0] >= 0xE0 ? 3 : 2;
            for (int i = 0; i < len && p[i]; i++) *w++ = (char)p[i];
            while (len-- && *p) p++;
            continue;
        }

        /* --- fins de ligne --- */
        if (p[0] == '\r') {
            p++;
            if (*p == '\n') p++;          /* \r\n : une seule fin de ligne */
            *w++ = '\n';
            continue;
        }

        *w++ = (char)*p++;
    }
    *w = '\0';
    return d;
}

/* Jette l'arbre d'un objet. Appelé dès que son script change, et par hc_free.
 *
 * Les jetons pointent DANS le texte du script : libérer l'un sans l'autre
 * laisserait des pointeurs dans de la mémoire rendue. Les trois morceaux
 * naissent et meurent donc ensemble. */
void hc_arbre_oublie(Object *o)
{
    if (!o) return;

    /* L'arbre tourne : on ne peut rien libérer sous ses pieds. On marque, et
     * v3_execute nettoiera en sortant. */
    if (o->arbre_usage > 0) { o->arbre_perime = 1; return; }

    if (o->reserve) { hct_reserve_libere((HctReserve *)o->reserve); free(o->reserve); }
    if (o->lot)     { hct_lot_libere((HctLot *)o->lot);             free(o->lot); }
    o->arbre = NULL;
    o->reserve = NULL;
    o->lot = NULL;
    o->arbre_sain = 0;
    o->arbre_signale = 0;
    o->arbre_perime = 0;

    /* Les anciens textes mis de côté ne servent plus : plus aucun jeton n'y
     * pointe, puisque le lot vient d'être libéré. */
    for (int i = 0; i < o->ntextes_gardes; i++) free(o->textes_gardes[i]);
    free(o->textes_gardes);
    o->textes_gardes = NULL;
    o->ntextes_gardes = 0;
}

/* Met un texte de script de côté au lieu de le libérer.
 *
 * Les jetons de l'arbre en cours d'exécution pointent dedans : le libérer
 * maintenant les ferait viser de la mémoire rendue. On le garde jusqu'à ce
 * que l'arbre lui-même parte. */
static void garde_texte(Object *o, char *texte)
{
    if (!texte) return;
    char **t = realloc(o->textes_gardes,
                       (size_t)(o->ntextes_gardes + 1) * sizeof *t);
    /* FAUTE DE PLACE, ON FUIT — délibérément.
     *
     * Ce texte est celui d'un script EN COURS D'EXÉCUTION : les jetons de
     * l'arbre pointent dedans, c'est toute la raison d'être de cette liste.
     * Le libérer parce qu'on ne peut pas l'inscrire — ce que faisait la ligne
     * « faute de mieux » — le fait viser de la mémoire rendue par le
     * gestionnaire qui tourne : un use-after-free, exactement ce que la liste
     * existe pour empêcher.
     *
     * Entre perdre quelques centaines d'octets jusqu'à la fermeture de la
     * pile et lire de la mémoire libérée, le choix n'est pas difficile. Et
     * ce n'est pas un cas où l'on peut s'arrêter par hc_memoire_epuisee :
     * l'application marche encore très bien, elle a seulement un texte de
     * plus qu'elle ne rendra pas. */
    if (!t) return;
    o->textes_gardes = t;
    o->textes_gardes[o->ntextes_gardes++] = texte;
}

/* Parcourt l'arbre et signale chaque nœud d'erreur, avec la ligne, la colonne
 * et le texte de la ligne fautive.
 *
 * L'analyseur ne s'arrête pas à la première faute : il en pose une et
 * continue, pour pouvoir toutes les montrer d'un coup. On les montre donc
 * toutes — mais pas plus de vingt, un script vraiment cassé en produirait
 * autant que de lignes. */
static void v3_dis_les_fautes_r(Object *o, const HctNoeud *n, int *reste)
{
    if (!n || *reste <= 0) return;

    if (n->genre == HCTN_ERREUR) {
        if (!o->arbre_faute_ligne) o->arbre_faute_ligne = n->jeton.ligne;

        /* La ligne du script telle qu'elle est écrite, pour n'avoir pas à
         * compter les lignes dans l'éditeur. */
        char ligne[160] = "";
        if (o->script) {
            const char *p = o->script;
            for (int l = 1; l < n->jeton.ligne && *p; p++)
                if (*p == '\n') l++;
            int k = 0;
            while (*p && *p != '\n' && *p != '\r' && k < (int)sizeof ligne - 1)
                ligne[k++] = *p++;
            ligne[k] = '\0';
        }

        hc_emet_erreur( "   !! script, ligne %d colonne %d : %s",
             n->jeton.ligne, n->jeton.col, n->msg ? n->msg : "forme non comprise");
        if (*ligne) hc_emet_erreur( "      %s", ligne);
        (*reste)--;
    }

    for (int i = 0; i < n->nfils; i++)
        v3_dis_les_fautes_r(o, n->fils[i], reste);
}

static void v3_dis_les_fautes(Object *o, const HctNoeud *racine)
{
    int reste = 20;
    v3_dis_les_fautes_r(o, racine, &reste);
}

/* Les fautes qui COÛTENT quelque chose : celles qui sont dans un
 * gestionnaire, et qui le privent donc de la v3.
 *
 * Depuis qu'une bannière hors gestionnaire n'empêche plus rien, la signaler
 * serait du bruit : le cadre de « ∞ » des piles d'époque produit des dizaines
 * de « caractère inattendu » qui ne changent rien à l'exécution. Mais se
 * taire sur TOUT serait pire — une coquille dans un gestionnaire lui coûte
 * l'exécuteur v3, et l'auteur doit l'apprendre.
 *
 * On descend donc par gestionnaire, et on laisse le décor tranquille. */
static void v3_dis_les_fautes_utiles(Object *o, const HctNoeud *racine)
{
    if (!racine) return;
    int reste = 20;
    for (int i = 0; i < racine->nfils; i++)
        if (racine->fils[i]->genre == HCTN_GESTIONNAIRE)
            v3_dis_les_fautes_r(o, racine->fils[i], &reste);
}

/* L'arbre du script, analysé à la première demande et gardé ensuite.
 *
 * Rend NULL si le script est vide, ou si l'analyse a signalé la moindre
 * faute. Ce dernier point est délibéré : mieux vaut confier tout le script à
 * l'ancien interpréteur que d'en exécuter la moitié avec le nouveau et de
 * s'arrêter au milieu sur une forme mal comprise. La frontière se déplacera
 * quand la v3 saura tout lire, pas avant. */
/* Plus `static` : hc_core.c l'appelle pour exécuter. Voir hc_interne.h. */
const HctNoeud *script_arbre(Object *o)
{
    if (!o || !o->script || !*o->script) return NULL;
    if (o->arbre) return o->arbre_sain ? (const HctNoeud *)o->arbre : NULL;
    if (o->lot) return NULL;          /* déjà tenté, et rejeté */

    HctLot *lot = calloc(1, sizeof *lot);
    HctReserve *res = calloc(1, sizeof *res);
    if (!lot || !res) { free(lot); free(res); return NULL; }

    o->lot = lot;
    o->reserve = res;

    int sain = hct_lex(o->script, lot);

    HctAnalyseur a;
    hct_analyseur_init(&a, lot, res);
    HctNoeud *racine = hct_bloc_script(&a);

    /* Une faute ne condamne plus TOUT le script, seulement le gestionnaire qui
     * la porte — trouve_gestionnaire l'écartera, et ce message-là repartira à
     * l'ancien interpréteur.
     *
     * CE QU'ON EXIGE : qu'il reste au moins un gestionnaire. Rien de plus.
     * Ni que le lexeur soit propre, ni que tout enfant de la racine soit un
     * gestionnaire — deux conditions posées ici et qui condamnaient des
     * scripts parfaitement utilisables.
     *
     * POURQUOI ELLES SAUTENT. Les piles d'époque s'ouvrent presque toutes sur
     * une bannière — un cadre de « ∞ », le nom du programme, la liste de ses
     * gestionnaires — écrite hors de tout « on … end ». HyperCard l'ignorait :
     * seuls les blocs on/function comptaient, le reste était du décor. Nous,
     * on refusait le script ENTIER. Mesuré sur Graph Maker 2.2 : onze lignes
     * de bannière coûtaient 374 lignes exécutées par la v1 et près de 500
     * réanalyses, pour un script que la v3 savait parfaitement lire dès qu'on
     * commentait l'en-tête.
     *
     * CE QUI PROTÈGE ENCORE, et qui suffit : trouve_gestionnaire écarte, un
     * par un, les gestionnaires qui portent une faute. Contrôle plus fin que
     * celui qu'on retire, puisqu'il examine le gestionnaire qu'on s'apprête à
     * exécuter plutôt que son voisinage.
     *
     * Le cas qu'on redoutait — un « end » manquant qui fait avaler le
     * gestionnaire suivant — ne laisse d'ailleurs PAS de nœuds nus à la
     * racine : il produit un gestionnaire fautif, que le contrôle par
     * gestionnaire écarte, et l'avalé n'est simplement pas trouvé. Il repart
     * à l'ancien interpréteur, ce qui est exactement ce qu'on veut. Le veto
     * global ne rattrapait donc rien que l'autre ne rattrape déjà — vérifié
     * en construisant le cas.
     *
     * Une faute de LEXIQUE suit la même règle : dans un gestionnaire elle le
     * rend fautif et il est écarté ; dans la bannière elle ne regarde
     * personne. */
    int gestionnaires = 0;
    for (int i = 0; racine && i < racine->nfils; i++)
        if (racine->fils[i]->genre == HCTN_GESTIONNAIRE) gestionnaires++;

    if (racine && gestionnaires > 0) {
        o->arbre = racine;
        o->arbre_sain = 1;
        /* Sur a.nerreurs OU sur une faute de lexique : le lexeur pose des
         * jetons d'erreur que l'analyseur ne compte pas toujours, et se taire
         * sur eux ferait disparaître « caractère inattendu » d'un script qui
         * en contient un — le diagnostic était rendu par le refus, et le
         * refus n'a plus lieu. */
        if (a.nerreurs || !sain) {
            o->arbre_faute_ligne = 0;
            v3_dis_les_fautes_utiles(o, racine);
        }
        return racine;
    }

    /* Analyse douteuse : on garde le lot et la réserve pour ne pas
     * recommencer à chaque message, mais on ne rendra jamais l'arbre.
     *
     * On dit AUSSI pourquoi, et où. « analyse non propre » tout court
     * n'apprenait rien : un script de trois cents lignes refusé pour une
     * virgule se cherchait à la main. Une faute suffit à écarter le script
     * entier, donc la première ligne signalée est celle à corriger. */
    o->arbre_sain = 0;
    o->arbre_faute_ligne = 0;

    for (int i = 0; i < lot->n; i++)
        if (lot->jetons[i].genre == HCT_ERREUR) {
            if (!o->arbre_faute_ligne) o->arbre_faute_ligne = lot->jetons[i].ligne;
            hc_emet_erreur( "   !! script, ligne %d colonne %d : %s",
                 lot->jetons[i].ligne, lot->jetons[i].col,
                 lot->jetons[i].msg ? lot->jetons[i].msg : "jeton mal formé");
        }
    v3_dis_les_fautes(o, racine);

    return NULL;
}

void hc_set_script(Object *o, const char *script)
{
    if (o->arbre_usage > 0) {
        /* Le script se réécrit pendant qu'il s'exécute — le calendrier
         * d'Apple range ses données dans le sien. On ne libère donc ni
         * l'arbre ni son texte : le premier est marqué périmé, le second mis
         * de côté, et tout partira quand l'exécution sera finie.
         *
         * Le gestionnaire en cours continue sur l'ANCIEN texte, ce qui est le
         * comportement de HyperCard : la réécriture ne prend effet qu'au
         * prochain appel. */
        /* Le NOUVEAU texte d'abord, l'ancien mis de côté ensuite.
         *
         * Dans l'ordre inverse, un dup_script qui échoue laissait o->script à
         * NULL alors que l'ancien venait d'être confié à la liste : l'objet
         * se retrouvait sans script, et la réécriture perdue. En le
         * construisant d'abord, un échec ne change rien du tout — le
         * gestionnaire continue sur son texte, et le script reste celui
         * d'avant. */
        char *neuf = dup_script(script);
        if (script && !neuf) {
            hc_emet_erreur( "   !! mémoire insuffisante : script inchangé");
            return;
        }
        o->arbre_perime = 1;
        garde_texte(o, o->script);
        o->script = neuf;
        return;
    }

    /* Même ordre ici, et pour la même raison. */
    char *neuf = dup_script(script);
    if (script && !neuf) {
        hc_emet_erreur( "   !! mémoire insuffisante : script inchangé");
        return;
    }
    hc_arbre_oublie(o);          /* AVANT de libérer le texte : les jetons y pointent */
    free(o->script);
    o->script = neuf;
}