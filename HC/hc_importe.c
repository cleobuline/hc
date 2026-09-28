/* hc_importe.c — Voir hc_importe.h pour la séparation des rôles et pour ce qui
 * n'est pas traduit.
 *
 * L'ORDRE DES GESTES COMPTE, ET PAS SEULEMENT POUR LA FORME.
 *
 * Trois endroits où faire les choses dans le mauvais ordre donne un résultat
 * plausible et faux — la pire sorte :
 *
 *   1. LES FONDS AVANT LES CARTES. Une carte se crée en désignant son fond ; il
 *      faut donc que les fonds existent, et qu'on sache retrouver le nôtre par
 *      l'identifiant que le fichier d'origine lui donnait.
 *
 *   2. `shared_text` AVANT d'écrire le texte d'un champ de fond. hc_set_field_text
 *      route vers les bgtexts de la CARTE quand le champ est un champ de fond non
 *      partagé, et vers le champ LUI-MÊME sinon. Poser le drapeau après
 *      écrirait le texte d'une seule carte comme texte par défaut du fond, et
 *      toutes les cartes l'afficheraient.
 *
 *   3. LA CARTE COURANTE AVANT CE MÊME TEXTE, pour la même raison : sans elle,
 *      hc_set_field_text ne sait pas dans quelle carte ranger le texte.
 */
#include "hc_importe.h"
#include "hc_interne.h"   /* id_adopte : reprendre les identifiants d'origine */

#include <stdlib.h>
#include <string.h>

/* L'ALIGNEMENT NE SE RECOPIE PAS, IL SE TRADUIT.
 *
 * Les deux conventions se ressemblent assez pour qu'on ne regarde pas :
 *
 *     Apple   0 gauche   1 centre   -1 droite
 *     nous    0 gauche   1 centre    2 droite
 *
 * Recopier la valeur brute donnait -1, que hc_file.c refuse à bon droit — notre
 * intervalle valide est 0..2 — et qui retombait donc sur zéro : TOUS les champs
 * alignés à droite de TOUTES les piles importées se seraient retrouvés alignés à
 * gauche, sans une erreur, sans un message.
 *
 * Trouvé par le tour complet à son PREMIER passage, et c'est exactement ce pour
 * quoi il a été écrit. La spec ajoute que d'autres valeurs se rencontrent, et
 * qu'elles s'affichent comme « à gauche » : d'où le défaut de cette fonction. */
static int alignement(int apple)
{
    if (apple ==  1) return 1;      /* centre */
    if (apple == -1) return 2;      /* droite */
    return 0;                       /* gauche, et tout le reste */
}

static char *copie(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* Une part, avec toutes ses propriétés. Le rectangle du format d'origine est en
 * haut/gauche/bas/droite ; le nôtre en x/y/largeur/hauteur. */
static Object *pose_part(Object *proprio, const HcOrigPart *q)
{
    Object *o = (q->genre == HC_ORIG_BOUTON)
              ? hc_new_button(proprio, q->nom ? q->nom : "")
              : hc_new_field(proprio, q->nom ? q->nom : "");
    if (!o) return NULL;

    o->x = q->gauche;
    o->y = q->haut;
    o->w = q->droite - q->gauche;
    o->h = q->bas - q->haut;
    if (o->w < 0) o->w = 0;
    if (o->h < 0) o->h = 0;

    free(o->style);
    o->style = copie(q->style);

    o->visible     = q->visible;
    o->dont_wrap   = q->dont_wrap;
    o->dont_search = q->dont_search;
    o->shared_text = q->shared_text;
    o->fixed_lh    = q->fixed_lh;
    o->auto_tab    = q->auto_tab;
    o->family      = q->family;

    o->text_align  = alignement(q->text_align);
    o->textsize    = q->textsize;
    o->textstyle   = q->textstyle;
    o->textheight  = q->textheight;
    if (q->police) { free(o->textfont); o->textfont = copie(q->police); }

    if (q->genre == HC_ORIG_BOUTON) {
        o->enabled       = q->enabled;
        o->showname      = q->showname;
        o->hilite        = q->hilite;
        o->autohilite    = q->autohilite;
        o->shared_hilite = q->shared_hilite;
        o->titlewidth    = q->titlewidth;
        o->icon          = q->icon;
    } else {
        o->locktext       = q->locktext;
        o->show_lines     = q->show_lines;
        o->wide_margins   = q->wide_margins;
        o->multiple_lines = q->multiple_lines;
        o->auto_select    = q->auto_select;
    }

    if (q->script && *q->script) hc_set_script(o, q->script);

    /* L'IDENTIFIANT D'ORIGINE, ET C'EST UNE PROPRIÉTÉ COMME LES AUTRES.
     *
     * Sans lui, « card field id 5 » écrit dans un script de 1993 ne désigne
     * plus rien : la part existe, elle est à sa place, elle a son texte, et le
     * script qui la vise regarde ailleurs. C'est la pire sorte de perte — elle
     * ne se voit pas sur l'écran, elle se voit à l'exécution.
     *
     * id_adopte compare aux parts de LA COUCHE, ce qui est l'espace de noms
     * d'HyperCard : mesuré sur nos trois piles, les 140 parts le gardent toutes.
     * Un conflit — deux parts de même numéro dans une même couche, qu'aucune
     * pile saine ne porte — laisse simplement le numéro neuf, et la part reste
     * atteignable autrement. On le compte plutôt que de le taire : c'est le
     * genre de chiffre qui doit rester à zéro.
     *
     * id_adopte plutôt que hc_set_id, qui est la porte du LECTEUR DE FICHIER :
     * celui-là crie sur un conflit, et il a raison de le faire pour un .stack
     * édité à la main. Ici le conflit n'a pas de remède à proposer à
     * l'utilisatrice au moment de l'import, et une boîte de dialogue par part
     * ferait fuir. Ce qui compte est qu'il reste à zéro, et c'est le TOUR COMPLET
     * du harnais qui le mesure — il compare désormais les identifiants un par un.
     *
     * Le retour est ignoré à dessein : échouer à reprendre un numéro n'est pas
     * une raison de perdre la part, qui garde alors celui de sa création et
     * reste atteignable par son nom et par son rang. */
    id_adopte(o, q->id);
    return o;
}

/* Le texte des parts d'une couche.
 *
 * `du_fond` DIT « CETTE PART EST UNE PART DE FOND », DANS L'ABSOLU, et non
 * « elle appartient à une autre couche ». La nuance a l'air scolaire et elle
 * change tout : dans un bloc BKGD, les parts du fond sont SES PROPRES parts, donc
 * marquées `du_fond`. Dans un bloc CARD, elles appartiennent au fond et le texte
 * ne vaut que pour cette carte-là.
 *
 * Je l'avais d'abord écrit comme « propre contre autre », et le texte par défaut
 * des champs de fond aurait été purement et simplement ignoré — sans erreur,
 * sans message, des champs vides dans une pile qui en a. Les vraies piles le
 * disent : « Découvrir HyperCard » porte des contenus marqués fond SOUS son
 * fond.
 *
 * D'où `couche_est_fond`. Et pour une carte, le texte se pose en la rendant
 * courante, après quoi hc_set_field_text sait où le ranger — à condition que
 * `shared_text` du champ soit déjà à zéro, ce que pose_part a fait puisque les
 * fonds sont bâtis avant les cartes. */
/* LA PART VISÉE SE TROUVE PAR POSITION, PAS PAR IDENTIFIANT, et c'est une
 * correction qui vaut d'être racontée.
 *
 * J'appariais un contenu à sa part en comparant l'identifiant d'origine à
 * `p->id`. Or `p->id` est le NÔTRE — hc_new_field l'attribue — et il ne
 * correspond à rien du fichier d'origine. Aucune part n'était donc jamais
 * trouvée, et le fichier converti ne portait PAS UNE SEULE ligne « contents » :
 * tous les champs de toutes les piles importées arrivaient vides.
 *
 * Et je l'avais écrit noir sur blanc dans la doc — « les identifiants de part ne
 * survivent pas, un script qui dit card field id 5 ne retrouvera pas son
 * champ » — sans voir que MON PROPRE CODE en dépendait. Savoir nommer un défaut
 * ne suffit pas à ne pas le commettre.
 *
 * Trouvé par l'autrice dans l'application, pas par le tour complet : celui-ci
 * comparait les propriétés et les scripts, et jamais LE TEXTE. Le trou était
 * dans l'instrument, et il est comblé.
 *
 * La position, elle, correspond : les parts sont créées dans l'ordre de
 * `k->parts[]`, donc la j-ième part de la couche bâtie est la j-ième du
 * fichier. */
static Object *part_visee(Object *couche, const HcOrigCouche *k, int id_origine)
{
    if (!couche || !k) return NULL;
    for (int j = 0; j < k->nparts && j < couche->nparts; j++)
        if (k->parts[j].id == id_origine) return couche->parts[j];
    return NULL;
}

static void pose_les_textes(Object *couche, const HcOrigCouche *k,
                            Object *fond, const HcOrigCouche *kfond,
                            int couche_est_fond)
{
    for (int i = 0; i < k->ncontenus; i++) {
        const HcOrigContenu *ct = &k->contenus[i];
        Object *p = (couche_est_fond || !ct->du_fond)
                  ? part_visee(couche, k, ct->id_part)
                  : part_visee(fond, kfond, ct->id_part);
        if (!p) continue;
        if (p->type != OBJ_FIELD && p->type != OBJ_BUTTON) continue;
        hc_set_field_text(p, ct->texte ? ct->texte : "");
    }
}

Object *hc_importe_pile(const HcOrigPile *orig, const char *nom)
{
    if (!orig) return NULL;
    Object *st = hc_new_stack(nom ? nom : "sans nom");
    if (!st) return NULL;

    st->w = orig->largeur;
    st->h = orig->hauteur;
    if (orig->script && *orig->script) hc_set_script(st, orig->script);

    /* 1. LES FONDS D'ABORD : une carte les désigne. On garde la correspondance
     * entre l'identifiant du fichier d'origine et l'objet bâti — les
     * identifiants que hc_new_background attribue sont les nôtres, et rien ne
     * dit qu'ils coïncident. */
    Object **fonds = NULL;
    int     *ids   = NULL;
    if (orig->nfonds_lus > 0) {
        fonds = calloc((size_t)orig->nfonds_lus, sizeof *fonds);
        ids   = calloc((size_t)orig->nfonds_lus, sizeof *ids);
        if (!fonds || !ids) { free(fonds); free(ids); hc_free(st); return NULL; }
    }
    int rang_du_fond = -1;      /* l'index, pour retrouver AUSSI sa couche d'origine */
    for (int i = 0; i < orig->nfonds_lus; i++) {
        const HcOrigCouche *k = &orig->fonds[i];
        Object *bg = hc_new_background(st, k->nom ? k->nom : "");
        if (!bg) { free(fonds); free(ids); hc_free(st); return NULL; }
        fonds[i] = bg;
        ids[i]   = k->id;
        /* L'identifiant du fond, repris quand la place est libre : « go to bg
         * id 2619 » et « the id of » en dépendent. Mesuré sur nos trois piles :
         * les 32 couches gardent le leur, les identifiants de couche étant
         * uniques dans la pile chez HyperCard comme chez nous. */
        id_adopte(bg, k->id);
        bg->dont_search = k->dont_search;
        bg->cant_delete = k->cant_delete;
        if (k->script && *k->script) hc_set_script(bg, k->script);
        for (int j = 0; j < k->nparts; j++)
            if (!pose_part(bg, &k->parts[j])) { free(fonds); free(ids); hc_free(st); return NULL; }
    }

    /* 2. LES CARTES, DANS L'ORDRE OÙ hc_origine LES A RANGÉES. Si l'ordre a été
     * lu et vérifié, c'est celui de la pile ; sinon c'est celui du fichier, et
     * `ordre_lu` le dit à qui veut le savoir. Ici on ne fait que suivre. */
    Object *precedente = hc_current_card();
    for (int i = 0; i < orig->ncartes_lues; i++) {
        const HcOrigCouche *k = &orig->cartes[i];
        Object *bg = NULL;
        for (int j = 0; j < orig->nfonds_lus; j++)
            if (ids[j] == k->fond) { bg = fonds[j]; break; }
        /* Une carte dont le fond est introuvable prend le premier : elle existe,
         * et la perdre coûterait plus que la mal ranger. Sans fond du tout, il
         * n'y a rien à faire — hc_new_card en exige un. */
        rang_du_fond = -1;
        for (int j = 0; j < orig->nfonds_lus; j++)
            if (ids[j] == k->fond) { rang_du_fond = j; break; }
        if (!bg && orig->nfonds_lus > 0) { bg = fonds[0]; rang_du_fond = 0; }
        if (!bg || rang_du_fond < 0) continue;

        Object *cd = hc_new_card(st, bg, k->nom ? k->nom : "");
        if (!cd) { free(fonds); free(ids); hc_free(st); return NULL; }
        id_adopte(cd, k->id);            /* même raison que pour les fonds */
        cd->marked      = k->marque;
        cd->dont_search = k->dont_search;
        cd->cant_delete = k->cant_delete;
        if (k->script && *k->script) hc_set_script(cd, k->script);
        for (int j = 0; j < k->nparts; j++)
            if (!pose_part(cd, &k->parts[j])) { free(fonds); free(ids); hc_free(st); return NULL; }

        /* La carte courante, pour que le texte d'un champ de fond aille dans SES
         * bgtexts et non dans le champ du fond. */
        hc_set_current_card(cd);
        pose_les_textes(cd, k, bg, &orig->fonds[rang_du_fond], 0);
    }

    /* Le texte par défaut des champs de fond se pose APRÈS les cartes : posé
     * avant, chaque carte l'aurait recopié dans ses propres bgtexts au moment où
     * elle devenait courante.
     *
     * Et la carte courante est retirée d'abord, sans quoi hc_set_field_text
     * rangerait ce texte-là dans les bgtexts de la dernière carte au lieu du
     * champ du fond : c'est exactement le routage qu'on exploite plus haut, et
     * ici il nuirait. */
    hc_set_current_card(NULL);
    for (int i = 0; i < orig->nfonds_lus; i++)
        pose_les_textes(fonds[i], &orig->fonds[i], NULL, NULL, 1);

    hc_set_current_card(precedente);
    free(fonds);
    free(ids);
    return st;
}
