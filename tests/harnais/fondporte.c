/* COLLER DEUX CARTES QUI PARTAGENT UN FOND EN CREAIT DEUX.
 *
 * SIGNALE A L'USAGE. Sur une pile B dont les cartes Une et Deux s'appuient
 * sur le meme fond « Commun » :
 *
 *     copier Une,  coller dans A   -> A gagne un fond « Commun »
 *     copier Deux, coller dans A   -> A gagne un SECOND fond « Commun »
 *
 * Les deux cartes se retrouvaient sur des fonds DISTINCTS alors qu'elles en
 * partageaient un. Modifier le fond n'en changeait plus qu'une, et le texte
 * des champs de fond partages cessait d'etre commun. Sur une pile ou l'on
 * rapatrie des cartes une par une, on finit avec autant de fonds que de
 * cartes.
 *
 * POURQUOI LA MEMOIRE EXISTANTE NE SUFFISAIT PAS, et c'est ce qui rend le
 * defaut interessant : elle existait, et elle marchait — pour un autre cas.
 * g_clip_bg_live retenait « le fond, et la pile ou il est », et
 * hc_paste_card l'ECRASAIT apres avoir recree un fond, si bien que coller
 * DEUX FOIS LA MEME CARTE reutilisait correctement. Mais hc_copy_card la
 * repose a chaque copie : la correspondance etait attachee au CONTENU DU
 * PRESSE-PAPIERS, et mourait avec lui.
 *
 * Or ce qu'il faut retenir n'a rien a voir avec le presse-papiers. C'est une
 * propriete des PILES : « le fond F de B a deja ete porte dans A, et c'est
 * celui-ci ». D'ou une table qui survit aux copies.
 *
 * CE QUE CE HARNAIS TIENT SURTOUT, ce n'est pas le comptage des fonds — un
 * « reutiliser toujours » le passerait — mais les quatre cas ou il ne FAUT
 * PAS reutiliser, et le texte par carte des champs de fond, qui est ce que
 * la correction pouvait casser sans qu'on le voie.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (t && k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static int nfonds(Object *st)
{
    int n = 0;
    for (int i = 0; i < st->nparts; i++)
        if (st->parts[i]->type == OBJ_BACKGROUND) n++;
    return n;
}

static void etat(const char *quoi, Object *st)
{
    printf("   %-44s %d fond(s)\n", quoi, nfonds(st));
}

static Object *derniere(Object *st)
{
    Object *d = NULL;
    for (int i = 0; i < st->nparts; i++)
        if (st->parts[i]->type == OBJ_CARD) d = st->parts[i];
    return d;
}

/* Le champ de fond de la carte, et le texte qu'elle en voit. */
static const char *note_de(Object *carte)
{
    if (!carte || !carte->bg) return "(pas de fond)";
    hc_set_current_card(carte);
    for (int k = 0; k < carte->bg->nparts; k++)
        if (carte->bg->parts[k]->type == OBJ_FIELD)
            return hc_field_text(carte->bg->parts[k]);
    return "(pas de champ)";
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    /* La pile SOURCE : deux fonds, trois cartes. Une et Deux partagent
     * « Commun » ; Trois est sur « Autre » — c'est elle qui prouvera qu'on
     * ne reutilise pas n'importe quoi. */
    Object *B  = hc_new_stack("B");
    Object *f1 = hc_new_background(B, "Commun");
    Object *f2 = hc_new_background(B, "Autre");
    /* Un champ de fond NON partage : son texte appartient a chaque carte.
     * C'est lui qui dira si le rattachement au fond reutilise a bien
     * reporte les bgtexts. */
    Object *note = hc_new_field(f1, "Note");
    note->shared_text = 0;
    Object *b1 = hc_new_card(B, f1, "Une");
    Object *b2 = hc_new_card(B, f1, "Deux");
    Object *b3 = hc_new_card(B, f2, "Trois");
    hc_register_stack(B);
    hc_set_current_card(b1); hc_set_field_text(note, "note de Une");
    hc_set_current_card(b2); hc_set_field_text(note, "note de Deux");

    /* La pile D'ACCUEIL, avec son fond a elle. */
    Object *A  = hc_new_stack("A");
    Object *fA = hc_new_background(A, "Sien");
    Object *a1 = hc_new_card(A, fA, "Origine");
    hc_register_stack(A);

    puts("== 1. LE CAS SIGNALE ==");
    etat("A au depart", A);
    hc_set_current_card(b1); hc_copy_card(b1);
    hc_set_current_card(a1); hc_paste_card(A);
    etat("colle Une", A);
    hc_set_current_card(b2); hc_copy_card(b2);
    hc_set_current_card(derniere(A)); hc_paste_card(A);
    etat("colle Deux, qui PARTAGE le fond de Une", A);

    puts("\n== 2. et les deux cartes sont sur LE MEME fond ==");
    {
        Object *une = NULL, *deux = NULL;
        for (int i = 0; i < A->nparts; i++) {
            Object *c = A->parts[i];
            if (c->type != OBJ_CARD || !c->name) continue;
            if (!strcmp(c->name, "Une"))  une  = c;
            if (!strcmp(c->name, "Deux")) deux = c;
        }
        printf("   meme fond : %s\n",
               (une && deux && une->bg == deux->bg) ? "oui" : "NON");
    }

    puts("\n== 3. LE TEXTE PAR CARTE A SUIVI ==");
    /* Ce que la correction pouvait casser sans qu'on le voie : les bgtexts
     * du clone designent les champs de la COPIE du fond, et doivent etre
     * reportes sur ceux du fond REELLEMENT utilise. */
    for (int i = 0; i < A->nparts; i++)
        if (A->parts[i]->type == OBJ_CARD)
            printf("   %-10s -> [%s]\n", A->parts[i]->name,
                   note_de(A->parts[i]));

    puts("\n== 4. LES CAS OU IL NE FAUT PAS REUTILISER ==");
    hc_set_current_card(b3); hc_copy_card(b3);
    hc_set_current_card(derniere(A)); hc_paste_card(A);
    etat("colle Trois, qui est sur un AUTRE fond", A);

    /* CE QU'ON NE PEUT PAS EPROUVER ICI, et il vaut mieux le dire.
     *
     * bg_deja_porte verifie que le fond retenu est TOUJOURS dans les parts[]
     * de sa pile avant de le rendre — un fond supprime entre-temps doit etre
     * recree, pas rendu mort. Ce garde-fou n'est pas exerce par ce harnais :
     * le noyau n'expose aucune porte pour supprimer un FOND. hc_delete_part
     * ne prend que les boutons et les champs, hc_delete_card que les cartes.
     *
     * Le bricoler en detachant le fond a la main mesurerait mon bricolage et
     * non le code. On le note donc comme non couvert, ce qui est une
     * information, plutot que d'ecrire un test qui rassure sans rien tenir.
     * Le jour ou « Delete Background » existera, ce cas viendra ici. */
    printf("   (le fond supprime entre-temps : non couvert, "
           "faute de porte publique)\n");

    /* Et la correspondance TIENT dans le temps : on recolle Une apres avoir
     * copie une carte d'un autre fond entre-temps. C'est exactement ce que
     * l'ancienne version perdait — la memoire mourait avec le
     * presse-papiers. */
    hc_set_current_card(b1); hc_copy_card(b1);
    hc_set_current_card(derniere(A)); hc_paste_card(A);
    etat("recolle Une APRES avoir copie Trois", A);

    puts("\n== 5. coller dans SA PROPRE pile ne cree rien ==");
    {
        int avant = nfonds(B);
        hc_set_current_card(b1); hc_copy_card(b1);
        hc_set_current_card(b1); hc_paste_card(B);
        printf("   B : %d -> %d fond(s)\n", avant, nfonds(B));
    }

    puts("\n== 6. la pile source fermee, la correspondance part avec ==");
    /* La table retient quatre pointeurs par ligne et n'en possede aucun. Une
     * pile fermee les rend caducs : il ne faut meme plus les COMPARER, une
     * adresse liberee pouvant etre rendue a quelqu'un d'autre. On purge donc
     * la table, et c'est ce que ce cas verifie.
     *
     * LE COMPTE NE MONTE PLUS POUR AUTANT, et il faut dire pourquoi : le
     * presse-papiers garde encore la COPIE du fond — clip_bg_clear n'est pas
     * appelee ici —, si bien que la reconnaissance par l'apparence retrouve
     * dans A le fond qu'on y avait deja porte. La table a bien ete purgee ;
     * c'est l'autre voie qui repond. Avant qu'elle existe, ce collage creait
     * un quatrieme fond. */
    {
        hc_set_current_card(b1); hc_copy_card(b1);
        hc_clipboard_stack_closing(B);
        hc_set_current_card(derniere(A)); hc_paste_card(A);
        etat("colle apres la fermeture de B", A);
    }

    hc_free(A);
    hc_free(B);
    return 0;
}
