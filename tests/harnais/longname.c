/* « the long name » etait lu, reconnu, puis JETE.
 *
 * Les deux analyseurs d'adjectifs reduisaient « short | long | abbreviated »
 * a un seul booleen « court ou non ». « long » traversait donc tout le chemin
 * pour finir en forme abregee : « the long name of me » rendait exactement la
 * meme chose que « the name of me ».
 *
 * Personne ne s'en etait apercu parce que le seul harnais qui l'ecrivait —
 * manque.c — s'arretait en erreur une ligne AVANT d'y arriver. Un test qui ne
 * teste rien, de la meme famille que ceux qu'on a deja nettoyes.
 *
 * MAIS UN LONG NAME N'A D'INTERET QUE S'IL SE RE-RESOUT. C'est toute sa
 * raison d'etre : le passer a une fonction, puis s'en servir comme reference
 * depuis une AUTRE carte, et retrouver le meme objet. Or resolve travaillait
 * relativement a la carte courante et ignorait purement et simplement la queue
 * « of … ». Mesure : depuis une autre carte, « card button "Bouton" of card
 * id 101 » ne resolvait rien — la portee etait lue puis jetee, elle aussi.
 *
 * Les deux vont donc ensemble, et ce harnais tient les deux :
 *   - les trois formes sont distinctes et correctes ;
 *   - la forme longue se re-resout depuis ailleurs.
 *
 * Une part de FOND s'ancre sur son fond et non sur une carte : elle existe
 * independamment de celle qu'on regarde. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);

    Object *st = hc_new_stack("Pile"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    Object *c1 = hc_new_card(st, bg, "Une");   hc_set_id(c1, 101);
    Object *c2 = hc_new_card(st, bg, NULL);    hc_set_id(c2, 102);   /* sans nom */
    Object *c3 = hc_new_card(st, bg, "Table of contents"); hc_set_id(c3, 103);
    Object *b  = hc_new_button(c1, "Bouton");  hc_set_id(b, 2915);
    Object *bf = hc_new_button(bg, "DuFond");  hc_set_id(bf, 7);
    Object *f  = hc_new_field(c1, "Champ");    hc_set_id(f, 9);
    hc_set_current_card(c1);
    Object *decl = hc_new_button(c1, "decl");

    hc_set_script(decl,
        "on mouseUp\n"
        "  put \"== les trois formes ==\"\n"
        "  put the short name of card button \"Bouton\"\n"
        "  put the name of card button \"Bouton\"\n"
        "  put the abbreviated name of card button \"Bouton\"\n"
        "  put the long name of card button \"Bouton\"\n"
        "  put \"== la forme longue, objet par objet ==\"\n"
        "  put the long name of bg button \"DuFond\"\n"
        "  put the long name of card field \"Champ\"\n"
        "  put the long name of this card\n"
        "  put the long name of card id 102\n"
        "  put the long name of this background\n"
        "  put the long name of this stack\n"
        "  put the long name of me\n"

        "  put \"== et ca se RE-RESOUT, depuis une autre carte ==\"\n"
        "  put the long name of card button \"Bouton\" into lb\n"
        "  put the long name of bg button \"DuFond\" into lf\n"
        "  put the long name of card field \"Champ\" into lc\n"
        "  put the long name of card id 101 into lcarte\n"
        "  go card id 102\n"
        "  put \"on est sur : \" & the id of this card\n"
        "  put the short name of lb & \" / \" & the id of lb\n"
        "  put the short name of lf & \" / \" & the id of lf\n"
        "  put the short name of lc & \" / \" & the id of lc\n"
        "  put the short name of lcarte & \" / \" & the id of lcarte\n"
        "  put the width of lb\n"

        "  put \"== un nom qui contient « of » n'est pas une portee ==\"\n"
        "  go card id 103\n"
        "  put the short name of this card\n"
        "  put the long name of this card\n"
        "  put the long name of this card into lt\n"
        "  go card id 101\n"
        "  put the short name of lt & \" / \" & the id of lt\n"
        "  put \"Table of contents\" into txt\n"
        "  put the number of chars of txt\n"

        "  put \"== bkgnd est un synonyme de background ==\"\n"
        "  put \"bkgnd \" & quote & \"Fond\" & quote into bk\n"
        "  put the short name of bk\n"
        "  put \"bkgnd button \" & quote & \"DuFond\" & quote into bkb\n"
        "  put the short name of bkb\n"

        "  put \"== la forme ABREGEE porte la couche, elle aussi ==\"\n"
        /* Elle rendait « button "ok" », ce qui ne designe PAS un objet : une
         * carte et son fond peuvent porter chacun un bouton de ce nom. « the
         * name of me » etait donc inutilisable pour designer l'objet dont il
         * venait — or c'est tout ce qu'on lui demande. */
        "  put the name of card button \"Bouton\"\n"
        "  put the name of bg button \"DuFond\"\n"
        "  put the name of card field \"Champ\"\n"
        "  put the name of this card\n"
        "  put the name of this background\n"
        "  put the name of this stack\n"
        "  put the name of bg button \"DuFond\" into ab\n"
        "  put \"   et elle se re-resout : \" & the short name of ab\n"

        "  put \"== une CIBLE CALCULEE, entre parentheses ==\"\n"
        /* La variable intermediaire marchait deja ; la parenthese, non. Elle
         * rendait le texte de la demande — « short name of (the name of card
         * button "Bouton") » — sans la moindre erreur. */
        "  put the short name of (the name of card button \"Bouton\")\n"
        "  put the id of (the long name of card field \"Champ\")\n"
        "  put the short name of (\"card button \" & quote & \"Bouton\" & quote)\n"
        "  put the short name of (line 1 of lb)\n"

        "  put \"== ce qui ne doit PAS changer ==\"\n"
        "  put the short name of this stack\n"
        "  put the short name of card id 102\n"
        "end mouseUp\n");
    hc_send(decl, "mouseUp");

    /* Une cible calculee qui ne designe rien doit le DIRE — et nommer l'OBJET,
     * pas la propriete. A part, parce qu'une erreur arrete le gestionnaire :
     * la mettre au milieu de celui d'au-dessus aurait fait disparaitre tout ce
     * qui suit, et un harnais qui se coupe ne mesure plus rien.
     *
     * Le second bloc porte DEUX cas qu'il ne faut pas confondre, et c'est tout
     * son interet :
     *
     *   the number of chars of ("abc" & "d")   -> 4
     *   the short name of ("Bouton")           -> objet introuvable
     *
     * Le premier est le garde-fou : un texte calcule qui ne s'ecrit pas comme
     * un descripteur reste du texte, et « the number of chars » a le droit de
     * le compter. Sans lui, la correction inventerait une erreur la ou il n'y
     * en a pas.
     *
     * Le second rendait autrefois « short name of ("Bouton" » — l'echo. Cette
     * reference l'enregistrait, si bien qu'un echo passait pour un resultat
     * voulu. Il ne l'etait pas : « short name » n'a de sens que sur un objet,
     * et un nom nu n'en designe aucun chez nous — il ne dit ni la couche ni
     * la sorte (voir v3_ressemble_a_un_objet). L'erreur est donc la bonne
     * reponse, et c'est la PROPRIETE qui la decide, pas la tete de la cible.
     *
     * Les deux lignes tiennent ensemble la frontiere : meme forme de cible,
     * meme parenthese, meme texte calcule — seule la propriete differe, et
     * c'est elle qui doit trancher. */
    puts("\n== une cible calculee qui ne designe rien ==");
    hc_set_script(decl,
        "on mouseUp\n"
        "  put the short name of (\"card button \" & quote & \"Absent\" & quote)\n"
        "end mouseUp\n");
    hc_send(decl, "mouseUp");
    hc_set_script(decl,
        "on mouseUp\n"
        "  put the number of chars of (\"abc\" & \"d\")\n"
        "  put the short name of (\"Bouton\")\n"
        "end mouseUp\n");
    hc_send(decl, "mouseUp");

    /* ─── UN DESCRIPTEUR DOIT POUVOIR SE RELIRE, QUEL QUE SOIT LE NOM ───
     *
     * « the long name » ne promet qu'une chose : ce qu'il rend doit designer
     * l'objet d'ou il vient. Le fabricant ecrivait « %s "%s" » et le lecteur
     * s'arretait au premier guillemet — un nom qui en contient un cassait donc
     * le descripteur en deux, SANS ERREUR :
     *
     *     card button "a"b"   puis  the short name of  ->  le litteral
     *
     * Le fichier .stack savait deja ecrire ces noms ; c'est le LANGAGE qui
     * n'avait pas recu la meme regle. Une syntaxe d'echappement d'un cote,
     * aucune de l'autre.
     *
     * Trois formes de noms, et l'aller-retour complet pour chacune. */
    puts("\n== un nom qui casse le descripteur ==");
    hc_set_current_card(c1);
    hc_set_script(decl,
        "on mouseUp\n"
        "  set the name of card button id 2915 to \"a\" & quote & \"b\"\n"
        "  put the name of card button id 2915 into r\n"
        "  put r\n"
        "  put \"   short name : \" & the short name of r\n"
        "  put \"   id         : \" & the id of r\n"

        "  set the name of card button id 2915 to \"bonjour\" & return & \"canard\"\n"
        "  put the long name of card button id 2915 into r\n"
        "  put r\n"
        "  put \"   id         : \" & the id of r\n"

        /* Une barre oblique dans le nom : l'echappement ne doit pas la manger,
         * et un echappement INCONNU se relit tel quel — sans quoi les
         * descripteurs deja ecrits dans les scripts d'une pile changeraient de
         * sens du jour au lendemain. */
        "  set the name of card button id 2915 to \"a\\b\"\n"
        "  put the long name of card button id 2915 into r\n"
        "  put r\n"
        "  put \"   short name : \" & the short name of r\n"
        "  put \"   id         : \" & the id of r\n"
        "end mouseUp\n");
    hc_send(decl, "mouseUp");

    /* ─── UN NOM TROP LONG NE SE TRONQUE PAS : IL CHANGE DE FORME ────────
     *
     * Mesure avant correction : un nom de 300 caracteres donnait un long name
     * de 187 — tronque — et « the id of » ce resultat rendait son propre
     * texte. Deux plafonds se cachaient derriere : les tampons du fabricant,
     * puis un « char tete[256] » dans resolve, qui repartait en silence sur la
     * reference entiere, donc sans sa portee.
     *
     * Le seuil mesure etait entre 279 et 319 caracteres de descripteur. On
     * balaie donc de part et d'autre. */
    puts("\n== des noms de plus en plus longs ==");
    for (int n = 200; n <= 440; n += 80) {
        char nom[600];
        memset(nom, 'x', (size_t)n); nom[n] = '\0';
        free(b->name); b->name = strdup(nom);
        char d[2048];
        hc_nom_de(b, HC_NOM_LONG, d, sizeof d);
        hc_set_script(decl,
            "on mouseUp\n"
            "  put the long name of card button id 2915 into r\n"
            "  put \"   nom \" & the number of chars of the short name of r"
            " & \" -> descripteur \" & the number of chars of r"
            " & \" -> id \" & the id of r\n"
            "end mouseUp\n");
        hc_send(decl, "mouseUp");
        (void)d;
    }
    free(b->name); b->name = strdup("Bouton");

    puts("\n== le CHEMIN du fichier, une fois la pile enregistree ==");
    /* HyperCard met le chemin complet dans la forme longue d'une pile. Le
     * noyau ne le connaissait pas — c'etait le document qui le tenait —, si
     * bien que « the long name of this stack » ne rendait que le NOM. Deux
     * piles ouvertes peuvent porter le meme nom ; leur chemin, non. */
    {
        const char *fic = "/tmp/hc_longname.stack";
        remove(fic);
        printf("   avant enregistrement : chemin = [%s]\n",
               hc_stack_path(st) ? hc_stack_path(st) : "(aucun)");
        printf("   sauvegarde : %s\n", hc_save(st, fic) == 0 ? "faite" : "ECHEC");
        printf("   apres               : chemin = [%s]\n",
               hc_stack_path(st) ? hc_stack_path(st) : "(aucun)");
        hc_set_current_card(c1);
        hc_set_script(decl,
            "on mouseUp\n"
            "  put the long name of this stack\n"
            "  put the long name of me\n"
            "  put the long name of this stack into lp\n"
            "  put \"   et la pile se retrouve par son chemin : \""
            " & the short name of lp\n"
            "end mouseUp\n");
        hc_send(decl, "mouseUp");
        remove(fic);
    }

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
