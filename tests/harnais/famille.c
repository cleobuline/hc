/* LA FAMILLE D'UN BOUTON : le mecanisme des boutons radio.
 *
 * « the family of <bouton> » ne rendait rien — ni valeur ni erreur. Le recours
 * reconstituait le texte, l'ancien moteur ne connaissait pas ce nom, et la
 * regle « mot inconnu = son propre nom » rendait la chaine « family of me ».
 * L'ecriture, elle, disait « propriete inconnue ».
 *
 * CE N'EST PAS UN NOMBRE A RANGER. Toute l'utilite de la famille est le
 * COMPORTEMENT : deux boutons de la meme couche et de la meme famille
 * s'excluent, en allumer un eteint l'autre. C'est ce que ce harnais verifie,
 * parce qu'un « family » qui se contenterait de retenir 3 aurait l'air de
 * marcher tout en ne servant a rien.
 *
 * Les points tenus :
 *
 *   1. allumer un bouton eteint ses freres de famille ;
 *   2. la famille 0 n'est pas un groupe : elle n'exclut personne ;
 *   3. la famille ne traverse PAS les couches — carte et fond sont deux
 *      groupes distincts, meme numero ;
 *   4. eteindre n'exclut rien : un groupe sans bouton allume est legitime ;
 *   5. entrer dans une famille en etant deja allume eteint les autres, sinon
 *      le groupe aurait deux allumes, etat qu'aucun clic ne peut produire ;
 *   6. hors de 0..15, on REFUSE au lieu d'ecreter ;
 *   7. la famille et titleWidth survivent a l'enregistrement ;
 *   8. LE CLIC suit exactement la meme regle que le script.
 *
 * Le point 8 est celui qui a failli manquer. Une exclusion par STYLE existait
 * deja au clic — un bouton de style radio eteignait tous les autres boutons
 * radio de la carte et du fond. La famille en ajoutait une seconde, par
 * numero. Deux mecanismes concurrents pour une meme question : le clic aurait
 * eteint ce que le script gardait. C'est le motif « un chemin corrige, son
 * jumeau oublie » qui a deja coute le catalogue d'icones et la conversion
 * octets/UTF-16.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *pilote;

static void fais(const char *corps)
{
    char s[900];
    snprintf(s, sizeof s, "on t\n%send t\n", corps);
    hc_set_script(pilote, s);
    hc_send(pilote, "t");
}

static void etat(const char *titre, Object **b, const char **noms, int n)
{
    printf("   %-34s", titre);
    for (int i = 0; i < n; i++)
        printf(" %s=%s", noms[i], hc_hilite_of(b[i], NULL) ? "on " : "off");
    puts("");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    /* hc_fin_de_clic refuse un objet qui n'est pas VIVANT, et « vivant » veut
     * dire joignable depuis le registre des piles. hc_new_stack n'y inscrit
     * pas : sans cet appel, le harnais aurait montre « aucun bouton ne
     * s'allume » et j'aurais cherche le defaut dans la famille. */
    hc_register_stack(st);

    Object *a1 = hc_new_button(c, "A1");
    Object *a2 = hc_new_button(c, "A2");
    Object *a3 = hc_new_button(c, "A3");
    Object *libre = hc_new_button(c, "Libre");
    Object *fond1 = hc_new_button(bg, "Fond1");
    pilote = hc_new_button(c, "Pilote");

    Object *trio[3] = { a1, a2, a3 };
    const char *noms3[3] = { "A1", "A2", "A3" };

    puts("== 1. allumer un bouton eteint ses freres ==");
    fais("  set the family of button \"A1\" to 1\n"
         "  set the family of button \"A2\" to 1\n"
         "  set the family of button \"A3\" to 1\n");
    printf("   familles posees                    A1=%d A2=%d A3=%d\n",
           a1->family, a2->family, a3->family);
    fais("  set the hilite of button \"A1\" to true\n");
    etat("apres avoir allume A1", trio, noms3, 3);
    fais("  set the hilite of button \"A2\" to true\n");
    etat("apres avoir allume A2", trio, noms3, 3);
    fais("  set the hilite of button \"A3\" to true\n");
    etat("apres avoir allume A3", trio, noms3, 3);

    puts("\n== 2. la famille 0 n'est pas un groupe ==");
    fais("  set the hilite of button \"Libre\" to true\n");
    printf("   Libre (famille %d) allume          %s\n", libre->family,
           hc_hilite_of(libre, NULL) ? "oui" : "non");
    etat("et A3 n'a pas bouge", trio, noms3, 3);
    Object *duo[2] = { libre, a3 };
    const char *noms2[2] = { "Libre", "A3" };
    fais("  set the family of button \"Libre\" to 0\n"
         "  set the hilite of button \"A3\" to true\n");
    etat("allumer A3 n'eteint pas Libre", duo, noms2, 2);

    puts("\n== 3. la famille ne traverse pas les couches ==");
    fais("  set the family of bg button \"Fond1\" to 1\n"
         "  set the hilite of bg button \"Fond1\" to true\n");
    printf("   Fond1 famille 1, allume            %s\n",
           hc_hilite_of(fond1, NULL) ? "oui" : "non");
    etat("A3 (carte, famille 1) intact", trio, noms3, 3);

    puts("\n== 4. eteindre n'exclut rien ==");
    fais("  set the hilite of button \"A3\" to false\n");
    etat("le groupe est vide", trio, noms3, 3);

    puts("\n== 5. entrer dans une famille en etant allume ==");
    fais("  set the family of button \"A1\" to 0\n"
         "  set the hilite of button \"A1\" to true\n"
         "  set the hilite of button \"A2\" to true\n");
    etat("A1 hors famille, A2 en famille 1", trio, noms3, 3);
    fais("  set the family of button \"A1\" to 1\n");
    etat("A1 rejoint la famille 1", trio, noms3, 3);

    puts("\n== 6. hors bornes : refus, pas ecretage ==");
    fais("  set the family of button \"A1\" to 20\n");
    printf("   A1 garde sa famille                %d\n", a1->family);
    fais("  set the family of button \"A1\" to -1\n");
    printf("   A1 garde sa famille                %d\n", a1->family);
    fais("  set the family of button \"A1\" to 15\n");
    printf("   15 est accepte                     %d\n", a1->family);

    puts("\n== 8. le clic suit la meme regle que le script ==");
    /* Deux boutons de STYLE radio et de familles DIFFERENTES : l'ancienne
     * regle les aurait fait s'exclure, la famille dit que non. */
    Object *r1 = hc_new_button(c, "R1");
    Object *r2 = hc_new_button(c, "R2");
    Object *r3 = hc_new_button(c, "R3");
    fais("  set the style of button \"R1\" to radioButton\n"
         "  set the style of button \"R2\" to radioButton\n"
         "  set the style of button \"R3\" to radioButton\n");
    Object *rad[3] = { r1, r2, r3 };
    const char *nomsr[3] = { "R1", "R2", "R3" };

    /* Sans famille : l'ancienne regle, inchangee. */
    hc_fin_de_clic(r1, c);
    etat("sans famille, clic sur R1", rad, nomsr, 3);
    hc_fin_de_clic(r2, c);
    etat("sans famille, clic sur R2", rad, nomsr, 3);

    /* Avec familles distinctes : R1 et R2 cessent de s'exclure. */
    fais("  set the family of button \"R1\" to 3\n"
         "  set the family of button \"R2\" to 4\n");
    hc_fin_de_clic(r1, c);
    hc_fin_de_clic(r2, c);
    etat("familles 3 et 4, les deux allumes", rad, nomsr, 3);
    /* R3 est reste sans famille : il ne doit ni les eteindre ni etre eteint
     * par eux. */
    hc_fin_de_clic(r3, c);
    etat("R3 sans famille, clic", rad, nomsr, 3);

    /* Meme famille : ils s'excluent, au clic comme au script. */
    fais("  set the family of button \"R2\" to 3\n");
    hc_fin_de_clic(r1, c);
    etat("R1 et R2 famille 3, clic sur R1", rad, nomsr, 3);
    hc_fin_de_clic(r2, c);
    etat("clic sur R2", rad, nomsr, 3);

    puts("\n== 9. hc_set_family : la porte unique ==");
    /* Le dialogue Infos bouton passe par elle, pas par o->family. Sa valeur
     * de retour est ce qui permet a l'appelant de dire non — le panneau ne
     * peut pas produire un numero hors bornes, mais un futur appelant le
     * pourrait, et un echec silencieux serait pire. */
    /* L'APPEL D'ABORD, LA LECTURE ENSUITE.
     *
     * La premiere version ecrivait printf("... %d, %d", hc_set_family(a1, 9),
     * a1->family) : l'ordre d'evaluation des arguments n'est pas defini, et
     * gcc lisait a1->family AVANT l'appel. La sortie annoncait « rend 1,
     * famille 15 » — un resultat qui aurait envoye chercher un defaut dans
     * hc_set_family, ou il n'y en a pas. */
    {
        int r;
        r = hc_set_family(a1, 9);
        printf("   hc_set_family(A1, 9)    rend %d, famille %d\n", r, a1->family);
        r = hc_set_family(a1, 16);
        printf("   hc_set_family(A1, 16)   rend %d, famille %d (inchangee)\n", r, a1->family);
        r = hc_set_family(a1, -1);
        printf("   hc_set_family(A1, -1)   rend %d, famille %d (inchangee)\n", r, a1->family);
        r = hc_set_family(a1, 0);
        printf("   hc_set_family(A1, 0)    rend %d, famille %d\n", r, a1->family);
        /* Sur autre chose qu'un bouton : refus, sans rien toucher. */
        r = hc_set_family(c, 3);
        printf("   hc_set_family(la carte, 3) rend %d\n", r);
    }

    puts("\n== 7. enregistrement et relecture ==");
    fais("  set the family of button \"A2\" to 7\n"
         "  set the titleWidth of button \"A2\" to 42\n");
    const char *chemin = "/tmp/hc_famille.stack";
    remove(chemin);
    if (hc_save(st, chemin) == 0) {
        Object *relu = hc_load(chemin);
        if (relu) {
            Object *rc = relu->nparts > 0 ? NULL : NULL;
            for (int i = 0; i < relu->nparts && !rc; i++)
                if (relu->parts[i]->type == OBJ_CARD) rc = relu->parts[i];
            Object *ra2 = NULL;
            for (int i = 0; rc && i < rc->nparts; i++)
                if (rc->parts[i]->name && !strcmp(rc->parts[i]->name, "A2"))
                    ra2 = rc->parts[i];
            printf("   A2 relu : famille %d, titleWidth %d\n",
                   ra2 ? ra2->family : -1, ra2 ? ra2->titlewidth : -1);
            /* Un bouton SANS famille ne doit rien avoir ecrit dans le
             * fichier : c'est ce qui garde les piles existantes identiques. */
            Object *rlibre = NULL;
            for (int i = 0; rc && i < rc->nparts; i++)
                if (rc->parts[i]->name && !strcmp(rc->parts[i]->name, "Libre"))
                    rlibre = rc->parts[i];
            printf("   Libre relu : famille %d (defaut)\n",
                   rlibre ? rlibre->family : -1);
            hc_free(relu);
        } else puts("   relecture impossible");
        remove(chemin);
    } else puts("   enregistrement impossible");

    hc_free(st);
    return 0;
}
