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

        "  put \"== ce qui ne doit PAS changer ==\"\n"
        "  put the short name of this stack\n"
        "  put the short name of card id 102\n"
        "end mouseUp\n");
    hc_send(decl, "mouseUp");

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
