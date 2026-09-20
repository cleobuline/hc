/* LE FOND PORTE D'UNE PILE A L'AUTRE SURVIT-IL A UNE RELANCE ?
 *
 * La table des fonds portes vit EN MEMOIRE. Elle repond parfaitement tant
 * que l'application tourne, et ne repond plus rien apres un redemarrage :
 *
 *     session 1 : copier Une dans B, coller dans A  -> A gagne « Commun »
 *     quitter, relancer
 *     session 2 : copier Deux dans B, coller dans A -> A gagne un SECOND
 *                                                      « Commun »
 *
 * C'est le defaut de fondporte.c, decale d'une relance. Or rapatrier des
 * cartes une par une est justement ce qu'on fait sur plusieurs jours.
 *
 * LA SEULE MEMOIRE QUI NE MENTE PAS est celle que le fond porte SUR LUI,
 * dans la pile d'accueil : son nom, sa peinture, ses parts. Un fichier
 * annexe devrait designer la pile source par son chemin ou son nom, et
 * l'utilisateur a le droit de renommer ses piles.
 *
 * CE QUE CE HARNAIS TIENT, ce n'est pas le comptage des fonds — un
 * « reutiliser toujours » le passerait — mais les cas ou il ne faut SURTOUT
 * PAS reconnaitre. Rattacher une carte au mauvais fond lui ferait perdre sa
 * mise en page sans un mot ; en creer un de trop n'est qu'agacant. Les deux
 * erreurs ne se valent pas, et le harnais pese le bon cote.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

#define FIC_A "/tmp/hc_fondrelance_A.stack"
#define FIC_B "/tmp/hc_fondrelance_B.stack"

/* Une « peinture » : le noyau ne l'interprete pas, il la rend telle quelle.
 * C'est donc une chaine quelconque, et c'est bien ce qu'on veut comparer —
 * y compris a travers un aller-retour par le fichier. */
#define PEINTURE  "iVBORw0KGgoAAAANSUhEUg=="
#define AUTRE_PEINTURE "R0lGODlhAQABAIAAAAAAAP8="

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (t && k == HC_ERR) printf("      [ERR] %s\n", t); }

static int nfonds(Object *st)
{
    int n = 0;
    for (int i = 0; i < st->nparts; i++)
        if (st->parts[i]->type == OBJ_BACKGROUND) n++;
    return n;
}

static Object *carte(Object *st, const char *nom)
{
    for (int i = 0; i < st->nparts; i++) {
        Object *c = st->parts[i];
        if (c->type == OBJ_CARD && c->name && !strcmp(c->name, nom)) return c;
    }
    return NULL;
}

static Object *derniere(Object *st)
{
    Object *d = NULL;
    for (int i = 0; i < st->nparts; i++)
        if (st->parts[i]->type == OBJ_CARD) d = st->parts[i];
    return d;
}

static Object *premier_fond(Object *st)
{
    for (int i = 0; i < st->nparts; i++)
        if (st->parts[i]->type == OBJ_BACKGROUND) return st->parts[i];
    return NULL;
}

/* Le texte du champ de fond, vu depuis cette carte. Ce que la reconnaissance
 * pouvait casser sans qu'on le voie. */
static const char *note_de(Object *c)
{
    if (!c || !c->bg) return "(pas de fond)";
    hc_set_current_card(c);
    for (int k = 0; k < c->bg->nparts; k++)
        if (c->bg->parts[k]->type == OBJ_FIELD)
            return hc_field_text(c->bg->parts[k]);
    return "(pas de champ)";
}

/* Copier la carte nommee de `src`, la coller a la fin de `dst`. */
static void porte(Object *src, const char *nom, Object *dst)
{
    Object *c = carte(src, nom);
    if (!c) { printf("   !! carte %s introuvable\n", nom); return; }
    hc_set_current_card(c);
    hc_copy_card(c);
    Object *d = derniere(dst);
    if (d) hc_set_current_card(d);
    hc_paste_card(dst);
}

/* La pile source du scenario principal : un fond « Commun » avec un champ
 * non partage et une peinture, deux cartes dessus. */
static Object *fabrique_B(void)
{
    Object *B  = hc_new_stack("B");
    Object *f  = hc_new_background(B, "Commun");
    hc_set_paint(f, PEINTURE);
    Object *nt = hc_new_field(f, "Note");
    nt->shared_text = 0;
    Object *b1 = hc_new_card(B, f, "Une");
    Object *b2 = hc_new_card(B, f, "Deux");
    hc_register_stack(B);
    hc_set_current_card(b1); hc_set_field_text(nt, "note de Une");
    hc_set_current_card(b2); hc_set_field_text(nt, "note de Deux");
    return B;
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    /* ═══ 1. LE CAS SIGNALE, A TRAVERS UNE RELANCE ═══════════════════════ */
    puts("== 1. session 1 : on porte UNE carte, puis on enregistre ==");
    {
        Object *B  = fabrique_B();
        Object *A  = hc_new_stack("A");
        Object *fA = hc_new_background(A, "Sien");
        hc_new_card(A, fA, "Origine");
        hc_register_stack(A);

        int id_source = premier_fond(B)->id;
        porte(B, "Une", A);
        printf("   A apres avoir colle Une              %d fond(s)\n", nfonds(A));
        /* La place etait libre : le fond porte a repris son numero. Ce n'est
         * pas ce qui le fera reconnaitre — l'apparence s'en charge — mais
         * cela rend leur sens aux scripts qui disent « bg field id 12 ». */
        printf("   le fond porte a repris son id        %s\n",
               (carte(A, "Une") && carte(A, "Une")->bg->id == id_source)
               ? "oui" : "non");

        if (hc_save(A, FIC_A) != 0 || hc_save(B, FIC_B) != 0)
            { puts("   !! echec de l'enregistrement"); return 1; }
        hc_free(A);
        hc_free(B);
    }

    puts("\n== 2. on relance : les piles reviennent du fichier ==");
    /* La table des correspondances est vide — les deux piles ont ete
     * liberees. Tout repose donc sur ce que le fond d'accueil porte sur lui,
     * PEINTURE COMPRISE : si l'aller-retour par le fichier changeait la
     * chaine base64 d'un seul caractere, la reconnaissance echouerait ici. */
    {
        Object *A = hc_load(FIC_A);
        Object *B = hc_load(FIC_B);
        if (!A || !B) { puts("   !! echec du chargement"); return 1; }
        hc_register_stack(A);
        hc_register_stack(B);

        printf("   A au chargement                      %d fond(s)\n", nfonds(A));
        porte(B, "Deux", A);
        printf("   A apres avoir colle Deux             %d fond(s)\n", nfonds(A));

        Object *une  = carte(A, "Une");
        Object *deux = carte(A, "Deux");
        printf("   Une et Deux sur le MEME fond         %s\n",
               (une && deux && une->bg == deux->bg) ? "oui" : "NON");
        printf("   Une  -> [%s]\n", note_de(une));
        printf("   Deux -> [%s]\n", note_de(deux));

        hc_free(A);
        hc_free(B);
    }
    remove(FIC_A);
    remove(FIC_B);

    /* ═══ 3. CE QU'IL NE FAUT PAS RECONNAITRE ════════════════════════════ */

    puts("\n== 3. meme nom et meme id, mais PAS LA MEME STRUCTURE ==");
    /* Deux piles ecrites dans deux sessions distinctes portent les memes
     * petits identifiants : le compteur repart a 1 dans chacune. Un fond
     * « id 2 » de l'une n'a donc rien a voir avec le « id 2 » de l'autre. */
    {
        Object *B  = fabrique_B();
        Object *A  = hc_new_stack("A");
        Object *fA = hc_new_background(A, "Commun");   /* meme NOM, expres  */
        hc_new_card(A, fA, "Origine");
        hc_register_stack(A);
        hc_set_id(fA, premier_fond(B)->id);            /* meme ID, expres   */

        printf("   A au depart                          %d fond(s)\n", nfonds(A));
        porte(B, "Une", A);
        printf("   A apres le collage                   %d fond(s)\n", nfonds(A));
        printf("   Une rattachee au fond de A           %s\n",
               carte(A, "Une") && carte(A, "Une")->bg == fA ? "OUI (defaut)" : "non");
        hc_free(A);
        hc_free(B);
    }

    puts("\n== 4. meme nom, memes parts, mais PAS LA MEME PEINTURE ==");
    /* Le cas dangereux : tout se ressemble sauf ce qu'on VOIT. */
    {
        Object *B  = fabrique_B();
        Object *A  = hc_new_stack("A");
        Object *fA = hc_new_background(A, "Commun");
        hc_set_paint(fA, AUTRE_PEINTURE);
        Object *cfA = hc_new_field(fA, "Note");
        cfA->shared_text = 0;
        hc_new_card(A, fA, "Origine");
        hc_register_stack(A);

        printf("   A au depart                          %d fond(s)\n", nfonds(A));
        porte(B, "Une", A);
        printf("   A apres le collage                   %d fond(s)\n", nfonds(A));
        printf("   Une rattachee au fond de A           %s\n",
               carte(A, "Une") && carte(A, "Une")->bg == fA ? "OUI (defaut)" : "non");
        hc_free(A);
        hc_free(B);
    }

    puts("\n== 5. un fond SANS SIGNE PARTICULIER n'est pas reconnu ==");
    /* Ni nom, ni peinture, ni parts : deux fonds vides ne se distinguent par
     * rien, et c'est justement pour cela qu'il ne faut pas les confondre. La
     * ressemblance ne prouve plus rien, alors qu'un fond de plus ne coute
     * rien. */
    {
        Object *B  = hc_new_stack("B");
        Object *f  = hc_new_background(B, NULL);
        Object *b1 = hc_new_card(B, f, "Une");
        hc_register_stack(B);

        Object *A  = hc_new_stack("A");
        Object *fA = hc_new_background(A, NULL);
        hc_new_card(A, fA, "Origine");
        hc_register_stack(A);

        printf("   A au depart                          %d fond(s)\n", nfonds(A));
        hc_set_current_card(b1); hc_copy_card(b1);
        hc_set_current_card(derniere(A)); hc_paste_card(A);
        printf("   A apres le collage                   %d fond(s)\n", nfonds(A));
        hc_free(A);
        hc_free(B);
    }

    /* ═══ 6. L'IDENTIFIANT AIDE, MAIS N'EST PAS EXIGE ════════════════════ */
    puts("\n== 6. l'id d'origine DEJA PRIS dans la pile d'accueil ==");
    /* C'est le cas ORDINAIRE, pas le cas rare : deux piles ecrites
     * separement se disputent les petits numeros. Le fond porte garde alors
     * l'identifiant neuf — et doit quand meme etre reconnu apres la relance,
     * sur sa seule apparence. Exiger l'egalite des identifiants ferait
     * echouer la reconnaissance ici, c'est-a-dire presque partout. */
    {
        Object *B  = fabrique_B();
        Object *A  = hc_new_stack("A");
        Object *fA = hc_new_background(A, "Sien");
        hc_new_card(A, fA, "Origine");
        hc_register_stack(A);
        /* La place est prise : le fond porte ne pourra pas reprendre son
         * numero. hc_set_id est ce que fait le lecteur de fichier. */
        hc_set_id(fA, premier_fond(B)->id);

        porte(B, "Une", A);
        Object *porte1 = carte(A, "Une") ? carte(A, "Une")->bg : NULL;
        printf("   le fond porte a repris son id        %s\n",
               (porte1 && porte1->id == premier_fond(B)->id) ? "oui" : "non");

        if (hc_save(A, FIC_A) != 0 || hc_save(B, FIC_B) != 0)
            { puts("   !! echec de l'enregistrement"); return 1; }
        hc_free(A);
        hc_free(B);

        Object *A2 = hc_load(FIC_A);
        Object *B2 = hc_load(FIC_B);
        if (!A2 || !B2) { puts("   !! echec du chargement"); return 1; }
        hc_register_stack(A2);
        hc_register_stack(B2);
        printf("   A au chargement                      %d fond(s)\n", nfonds(A2));
        porte(B2, "Deux", A2);
        printf("   A apres avoir colle Deux             %d fond(s)\n", nfonds(A2));
        printf("   Une et Deux sur le MEME fond         %s\n",
               (carte(A2, "Une") && carte(A2, "Deux") &&
                carte(A2, "Une")->bg == carte(A2, "Deux")->bg) ? "oui" : "NON");
        hc_free(A2);
        hc_free(B2);
        remove(FIC_A);
        remove(FIC_B);
    }

    /* ═══ 7. CE QUE LA TABLE TIENT ENCORE, ET QUE L'APPARENCE PERDRAIT ═══ */
    puts("\n== 7. retoucher le fond porte ne defait pas la correspondance ==");
    /* L'apparence ne reconnaitrait plus rien une fois le fond d'accueil
     * retouche. La table, elle, retient l'IDENTITE : c'est pourquoi un fond
     * reconnu y est aussitot inscrit, et pourquoi on ne l'a pas remplacee. */
    {
        Object *B  = fabrique_B();
        Object *A  = hc_new_stack("A");
        Object *fA = hc_new_background(A, "Sien");
        hc_new_card(A, fA, "Origine");
        hc_register_stack(A);

        porte(B, "Une", A);
        Object *porte1 = carte(A, "Une") ? carte(A, "Une")->bg : NULL;
        if (porte1) hc_set_paint(porte1, AUTRE_PEINTURE);  /* on repeint */
        printf("   A apres avoir colle Une              %d fond(s)\n", nfonds(A));
        porte(B, "Deux", A);
        printf("   A apres avoir repeint puis colle     %d fond(s)\n", nfonds(A));
        hc_free(A);
        hc_free(B);
    }

    return 0;
}
