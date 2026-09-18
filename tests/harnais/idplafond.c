/* LE COMPTEUR D'IDENTIFIANTS AU PLAFOND, ET CE QUI SE COLLE APRÈS.
 *
 * new_object a reçu sa garde : quand g_next_id atteint HC_ID_MAX, il cherche
 * un trou dans la pile au lieu de fabriquer un identifiant hors bornes.
 * Trois écritures « c->id = g_next_id++ » étaient restées sans garde —
 * hc_paste_part, et place_layer_clone pour la couche et chacune de ses parts.
 *
 * Le défaut n'est pas théorique : une pile qui porte « id 999999999 » suffit
 * à pousser le compteur au plafond, et l'objet collé ensuite reçoit alors un
 * identifiant que NOTRE PROPRE lecteur refuse — hc_id() rend 0 dessus. Au
 * rechargement l'objet change silencieusement de numéro, et toute référence
 * « card id N » écrite dans un script ne désigne plus rien.
 *
 * On vérifie donc la seule chose qui compte : tout identifiant posé doit
 * repasser par hc_id() sans être rejeté. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)k; (void)d; (void)t; }

static int fautes = 0;

/* hc_id() est le lecteur : il rend 0 sur ce qu'il refuse. C'est lui l'arbitre,
 * pas une comparaison à HC_ID_MAX écrite ici — le harnais doit interroger le
 * code, pas répéter sa constante. */
static void juge(const char *quoi, int id)
{
    char n[32];
    snprintf(n, sizeof n, "%d", id);
    int relu = hc_id(n);
    printf("  %-34s id %-11d relu %-11d %s\n",
           quoi, id, relu, relu == id ? "ok" : "REFUSÉ PAR NOTRE LECTEUR");
    if (relu != id) fautes++;
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c1 = hc_new_card(st, bg, "une");
    Object *b  = hc_new_button(c1, "bouton");
    hc_new_field(c1, "champ");
    hc_set_current_card(c1);

    /* On pousse le compteur au plafond comme le ferait un fichier : un objet
     * qui porte un identifiant légal mais énorme. 999999998 est accepté,
     * et g_next_id passe juste en dessous de la borne. */
    hc_set_id(b, 999999998);
    puts("── le compteur est au plafond (un objet porte id 999999998)");
    juge("le bouton lui-meme", b->id);

    puts("\n── coller une PART (hc_paste_part)");
    hc_copy_part(b);
    Object *colle = hc_paste_part(c1);
    if (!colle) { puts("  rien collé — le harnais ne mesure rien"); return 1; }
    juge("la part collée", colle->id);

    puts("\n── coller une CARTE (hc_paste_card : couche + ses parts)");
    hc_copy_card(c1);
    Object *cc = hc_paste_card(st);
    if (!cc) { puts("  rien collé — le harnais ne mesure rien"); return 1; }
    juge("la carte collée", cc->id);
    for (int i = 0; i < cc->nparts; i++) {
        char quoi[64];
        snprintf(quoi, sizeof quoi, "sa part %d", i + 1);
        juge(quoi, cc->parts[i]->id);
    }
    /* Le fond voyage avec la carte : c'est une couche de plus, posée par le
     * même place_layer_clone. Il a la même garde à tenir. */
    if (cc->bg && cc->bg != bg) juge("le fond venu avec elle", cc->bg->id);

    puts("\n── dupliquer une carte (hc_duplicate_card)");
    Object *dup = hc_duplicate_card(c1);
    if (dup) {
        juge("la carte dupliquée", dup->id);
        for (int i = 0; i < dup->nparts; i++) {
            char quoi[64];
            snprintf(quoi, sizeof quoi, "sa part %d", i + 1);
            juge(quoi, dup->parts[i]->id);
        }
    }

    /* Aucun doublon non plus : un identifiant neuf qui en répète un autre
     * rendrait « card id N » ambigu et ferait écrire deux fois la même clé. */
    puts("\n── et aucun doublon dans la pile");
    int doublons = 0;
    for (int i = 0; i < st->nparts; i++) {
        Object *ca = st->parts[i];
        for (int j = 0; j < st->nparts; j++) {
            if (i != j && ca->id == st->parts[j]->id) doublons++;
            for (int k = 0; k < st->parts[j]->nparts; k++)
                if (ca->id == st->parts[j]->parts[k]->id) doublons++;
        }
        for (int k = 0; k < ca->nparts; k++)
            for (int m = k + 1; m < ca->nparts; m++)
                if (ca->parts[k]->id == ca->parts[m]->id) doublons++;
    }
    printf("  paires en double : %d\n", doublons);
    if (doublons) fautes++;

    /* LE REPLI, ET CE QU'IL FAIT VRAIMENT.
     *
     * id_neuf rend « HC_ID_MAX - 1 » quand id_libre_dans ne trouve aucun trou.
     * On a longtemps porté ce cas au registre des défauts — « fabrique un
     * doublon ». La mesure dit autre chose, et il vaut mieux l'écrire ici que
     * de le corriger à l'aveugle.
     *
     * DANS une pile, le repli ne peut pas fabriquer de doublon : id_libre_dans
     * ne rend 0 que si les identifiants 1..HC_ID_MAX-1 sont TOUS pris, soit un
     * milliard d'objets. Et deux créations de suite ne se marchent pas dessus,
     * parce que chaque hc_new_* attache son objet avant qu'on rappelle.
     *
     * Le seul cas atteignable est la PILE elle-même : son propriétaire est
     * NULL, donc id_libre_dans n'a rien à fouiller et rend 0. Deux piles
     * créées pendant que le compteur est au plafond portent alors le même
     * identifiant. C'est SANS CONSÉQUENCE : une pile est seule dans son
     * fichier, les identifiants ne se rencontrent jamais. On le constate ici
     * plutôt que de le corriger, pour que la prochaine lecture du code n'ait
     * pas à refaire le raisonnement. */
    puts("\n── le repli, quand le compteur est au plafond");
    Object *p1 = hc_new_stack("P1");
    Object *p2 = hc_new_stack("P2");
    printf("  deux piles neuves            id %d et id %d  (%s)\n",
           p1->id, p2->id,
           p1->id == p2->id ? "identiques, et sans conséquence : fichiers séparés"
                            : "distincts");
    juge("l'identifiant de la pile neuve", p1->id);

    Object *n1 = hc_new_button(c1, "n1");
    Object *n2 = hc_new_button(c1, "n2");
    printf("  deux objets neufs DANS la pile id %d et id %d  (%s)\n",
           n1->id, n2->id, n1->id == n2->id ? "*** DOUBLON ***" : "distincts");
    if (n1->id == n2->id) fautes++;
    hc_free(p1); hc_free(p2);

    printf("\n  fautes : %d\n", fautes);
    hc_free(st);
    return 0;
}
