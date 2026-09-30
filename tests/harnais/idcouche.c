/* L'ESPACE DE NOMS D'UN IDENTIFIANT DE PART EST SA COUCHE, ET NON LA PILE.
 *
 * hc_core.c ne connaissait qu'un espace, la pile : deux parts de meme numero y
 * etaient un doublon, meme dans deux couches differentes. C'est faux, et la
 * mesure vient des piles d'Apple. En simulant l'adoption des identifiants dans
 * l'ordre ou l'importateur batit :
 *
 *     3D Parametric Equations   105 parts,  47 en conflit  (45 %)
 *     Decouvrir HyperCard        28 parts,   5 en conflit
 *     TEST3                       7 parts,   1 en conflit
 *     les 32 couches des trois piles :  AUCUN conflit
 *
 * Tous les conflits sont des parts contre des parts d'une AUTRE couche — la
 * part 4 d'une carte contre la part 4 de son fond, que « Decouvrir HyperCard »
 * porte vraiment. Jamais une part contre une couche, jamais deux fois le meme
 * numero dans une couche.
 *
 * CE QUE COUTAIT L'ERREUR : 45 % des parts d'une pile importee recevaient un
 * numero neuf, et tout « card button id N » de ses scripts de 1993 designait le
 * vide. En silence, et sur les piles les plus riches d'abord — celles qui ont
 * le plus de parts sont celles qui en perdent le plus.
 *
 * Le doublon qui compte reste celui qui rend une designation AMBIGUE : deux
 * parts de meme numero dans une MEME couche. Celui-la est toujours refuse, et
 * la derniere section le verifie.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if (k == HC_MSG || k == HC_ERR) printf("      %s\n", t ? t : ""); }

static Object *pilote = NULL;

static void joue(const char *corps)
{
    char s[400];
    snprintf(s, sizeof s, "on t\n%send t\n", corps);
    hc_set_script(pilote, s);
    hc_send(pilote, "t");
}

static Object *couche_nommee(Object *st, ObjType type, const char *nom)
{
    for (int i = 0; i < st->nparts; i++)
        if (st->parts[i]->type == type && st->parts[i]->name &&
            strcmp(st->parts[i]->name, nom) == 0) return st->parts[i];
    return NULL;
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c1 = hc_new_card(st, bg, "une");
    Object *c2 = hc_new_card(st, bg, "deux");

    /* LE MEME NUMERO DANS TROIS COUCHES, pose par hc_set_id — c'est-a-dire par
     * la porte qu'emprunte le lecteur de fichier, la seule qui refusait. */
    Object *fbg = hc_new_field(bg, "du fond");    hc_set_id(fbg, 1);
    Object *fc1 = hc_new_field(c1, "de la une");  hc_set_id(fc1, 1);
    Object *bc2 = hc_new_button(c2, "de la deux");hc_set_id(bc2, 1);
    /* Et un numero qui est aussi celui d'une COUCHE : les deux espaces ne se
     * touchent pas, donc une part a le droit de porter le numero de sa carte. */
    Object *fc2 = hc_new_field(c2, "comme sa carte"); hc_set_id(fc2, c2->id);

    printf("== les numeros poses ==\n");
    printf("   fond %d, carte une %d, carte deux %d\n", bg->id, c1->id, c2->id);
    printf("   champ du fond id %d, champ de la une id %d, bouton de la deux id %d\n",
           fbg->id, fc1->id, bc2->id);
    printf("   champ de la deux id %d (le numero de sa carte, %d)\n", fc2->id, c2->id);

    hc_register_stack(st);
    hc_set_current_card(c1);
    hc_set_field_text(fbg, "texte du fond, sur la une");
    hc_set_field_text(fc1, "texte de la une");
    hc_set_current_card(c2);
    /* LE TEXTE D'UN CHAMP DE FOND EST PAR CARTE, et ce harnais a failli me
     * faire corriger un defaut qui n'existe pas. En n'ecrivant le texte du champ
     * du fond que depuis la carte une, « put field id 1 » rendait vide sur la
     * carte deux : j'ai cru que le repli sur le fond etait casse par le bouton
     * de meme numero, et j'ai ecrit un banc pour l'accuser. Le banc disait la
     * meme chose, pour la meme raison — le champ de fond non partage rangeait
     * son texte dans les bgtexts de la carte UNE, et sur la deux il est bel et
     * bien vide. On ecrit donc les deux, et la ligne reste ici pour la prochaine
     * fois. */
    hc_set_field_text(fbg, "texte du fond, sur la deux");
    hc_set_field_text(fc2, "texte de la deux");

    pilote = hc_new_button(c1, "pilote");

    /* ── chacun se resout dans SA couche ── */
    puts("\n== chacun se resout dans sa couche ==");
    hc_set_current_card(c1);
    puts("   sur la carte une :");
    joue("  put card field id 1\n");
    joue("  put bg field id 1\n");
    joue("  put the name of card field id 1\n");
    joue("  put the name of bg field id 1\n");

    hc_set_current_card(c2);
    puts("   sur la carte deux :");
    joue("  put the name of card button id 1\n");
    joue("  put the name of bg field id 1\n");
    joue("  put card field id 1\n");   /* il n'y en a pas : un champ, pas un bouton */

    /* ── sans prefixe : un CHAMP se cherche au FOND d'abord, puis sur la
     * carte (voir couche_implicite, hc_core.c). Mesure dans HyperCard par
     * le rang et par le nom ; par l'IDENTIFIANT, non mesure — la meme regle
     * est appliquee par coherence. ── */
    puts("\n== sans prefixe, le fond passe avant la carte ==");
    hc_set_current_card(c1);
    joue("  put field id 1\n");        /* celui du FOND : les deux couches ont un 1 */
    hc_set_current_card(c2);
    /* La carte deux n'a pas de CHAMP 1 — elle a un bouton 1 — donc le repli
     * mene au champ du fond, et le type est bien filtre en chemin. */
    joue("  put the name of field id 1\n");
    joue("  put field id 1\n");

    /* ── LA PORTEE EXPLICITE NE SE REPLIE PAS, et c'est un defaut trouve ici ──
     *
     * « card field id 1 » sur la carte deux, qui n'a pas de champ 1 alors que le
     * fond en a un : la reponse doit etre VIDE. Le resolveur v1 cherchait sur la
     * carte puis se repliait sur le fond QUOI QU'ON AIT ECRIT, et rendait donc le
     * champ du fond — « the name of card field id 1 » repondait bkgnd field
     * « du fond ». Ses deux branches voisines, le rang et le nom, tenaient deja
     * la portee ; celle de l'identifiant l'avait oubliee.
     *
     * L'executeur v3 repondait juste : il ne trouvait rien. C'est le repli du
     * pont vers le resolveur v1 qui changeait « introuvable » en « le mauvais ».
     * Une bonne reponse ne rattrape pas une mauvaise.
     *
     * Le defaut est ancien, et presque inatteignable tant que les identifiants
     * etaient uniques dans la pile : une carte et son fond ne pouvaient pas se
     * disputer un numero. Il devient ordinaire des que l'espace de noms est la
     * couche, donc dans toute pile importee. */
    puts("\n== une portee explicite ne se replie pas ==");
    hc_set_current_card(c2);
    joue("  put card field id 1\n");
    joue("  put the name of card field id 1\n");
    joue("  if there is a card field id 1 then put \"il y en a\" else put \"aucun\"\n");
    /* Et dans l'autre sens : le fond n'a pas de BOUTON 1, la carte deux si. */
    joue("  put the name of bg button id 1\n");
    joue("  if there is a bg button id 1 then put \"il y en a\" else put \"aucun\"\n");

    /* ── et tout cela survit a NOTRE format ── */
    puts("\n== enregistre puis relu ==");
    const char *chemin = "/tmp/hc_idcouche.stack";
    remove(chemin);
    if (hc_save(st, chemin) != 0) { puts("   enregistrement impossible"); return 1; }
    Object *r = hc_load(chemin);
    if (!r) { puts("   rechargement impossible"); return 1; }

    Object *rbg = couche_nommee(r, OBJ_BACKGROUND, "Fond");
    Object *r1  = couche_nommee(r, OBJ_CARD, "une");
    Object *r2  = couche_nommee(r, OBJ_CARD, "deux");
    printf("   fond %d, carte une %d, carte deux %d\n",
           rbg ? rbg->id : -1, r1 ? r1->id : -1, r2 ? r2->id : -1);
    for (int i = 0; rbg && i < rbg->nparts; i++)
        printf("   fond      : %-16s id %d\n", rbg->parts[i]->name, rbg->parts[i]->id);
    for (int i = 0; r1 && i < r1->nparts; i++)
        printf("   carte une : %-16s id %d\n", r1->parts[i]->name, r1->parts[i]->id);
    for (int i = 0; r2 && i < r2->nparts; i++)
        printf("   carte deux: %-16s id %d\n", r2->parts[i]->name, r2->parts[i]->id);

    /* LE CONTROLE POSITIF : sans la regle, ce harnais passerait-il quand meme ?
     * Non — et voici par quoi ca se verrait. On repose sur le champ du fond le
     * numero du champ de la carte une : si l'espace etait la pile, hc_set_id
     * refuserait et le numero changerait. Le test ci-dessus mesure donc bien ce
     * qu'il annonce. */
    puts("\n== controle positif : deux parts de meme numero dans une MEME couche ==");
    Object *autre = hc_new_field(c1, "second de la une");
    int avant = autre->id;
    hc_set_id(autre, 1);            /* 1 est deja pris DANS la carte une */
    printf("   %s : %d -> %d  %s\n", autre->name, avant, autre->id,
           autre->id == avant ? "refuse, et il garde le sien"
                              : "*** ACCEPTE, et la couche est ambigue ***");

    puts("\n== et le meme numero dans une autre couche reste accepte ==");
    /* Une carte NEUVE, qui ne porte encore aucun 1 : sur la carte deux le
     * numero 1 est deja pris par son bouton, et ce serait le cas ambigu. */
    Object *c3 = hc_new_card(st, bg, "trois");
    Object *encore = hc_new_field(c3, "de la trois");
    avant = encore->id;
    hc_set_id(encore, 1);           /* deja porte par le fond ET par les deux cartes */
    printf("   %s : %d -> %d  %s\n", encore->name, avant, encore->id,
           encore->id == 1 ? "accepte : ce n'est pas le meme espace"
                           : "*** REFUSE ***");

    hc_free(r);
    hc_free(st);
    remove(chemin);
    return 0;
}
