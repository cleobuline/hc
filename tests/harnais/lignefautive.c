/* Une ligne qui porte une faute ne s'exécute pas, PAS MÊME EN PARTIE.
 *
 * C'est ce que promet v3_cadre_fautif (hc_core.c) : « une faute dans son CORPS
 * ne condamne qu'une INSTRUCTION ; l'exécuteur la signale quand il l'atteint,
 * et le gestionnaire s'arrête là ». L'analyseur posait pourtant l'instruction
 * qu'il avait su lire, PUIS la faute à côté — on exécutait donc le début de la
 * ligne avant de se plaindre du reste. Trouvé par le fuzzing :
 *
 *     delete card "D          la carte courante était SUPPRIMÉE, puis
 *                             « texte inattendu en fin de ligne »
 *
 * Signaler une erreur ET avoir agi est la pire des deux issues : l'auteur lit
 * que sa ligne n'est pas comprise, et la pile a déjà changé.
 *
 * Chaque cas imprime ce qu'il a CHANGÉ — une globale, le nombre de cartes, la
 * carte courante —, pas seulement le message : c'est le changement qui était
 * le défaut, le message était déjà là.
 *
 * Ce que fait HyperCard d'une ligne fautive — la refuse-t-il en compilant le
 * gestionnaire entier, ou en l'atteignant ? — n'est PAS mesuré. Les deux
 * réponses excluent d'en exécuter la moitié ; la section 3 fixe notre choix
 * actuel (les lignes d'AVANT tournent) pour qu'il ne bouge pas sans qu'on le
 * voie. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    /* Le message d'EXÉCUTION seulement : celui de l'analyse répète la ligne,
     * et l'on veut voir si le gestionnaire s'est arrêté, pas relire le texte. */
    if (k == HC_ERR && strstr(t, "(v3,")) printf("      [ERR] %s\n", t);
    else if (k == HC_MSG)                 printf("      %s\n", t);
}

static Object *st, *b;

static const char *nom_courant(void)
{
    Object *c = hc_current_card();
    return (c && c->name) ? c->name : "?";
}

static int nb_cartes(void)
{
    int n = 0;
    for (int i = 0; i < st->nparts; i++)
        if (st->parts[i]->type == OBJ_CARD) n++;
    return n;
}

static void essai(const char *titre, const char *corps)
{
    char s[2000];
    snprintf(s, sizeof s,
             "on essai\n  global g\n  put \"avant\" into g\n%s\nend essai\n", corps);
    printf("── %s\n", titre);
    int n0 = nb_cartes();
    char c0[64];
    snprintf(c0, sizeof c0, "%s", nom_courant());
    hc_set_script(b, s);
    hc_send(b, "essai");
    hc_set_script(b, "on lis\n  global g\n  put \"g = \" & g\nend lis\n");
    hc_send(b, "lis");
    printf("      cartes %d -> %d, carte %s -> %s\n", n0, nb_cartes(), c0, nom_courant());
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    st = hc_new_stack("P");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *u = hc_new_card(st, bg, "U");
    hc_new_card(st, bg, "V");
    hc_new_card(st, bg, "W");
    b = hc_new_button(bg, "B");
    hc_set_current_card(u);

    puts("== 1. la ligne fautive ne fait RIEN ==");
    essai("texte en trop après un put",
          "  put \"posé\" into g zz");
    essai("guillemet non fermé après delete card : la carte reste",
          "  delete card \"D");
    essai("la même, dans un si d'une ligne",
          "  if true then delete card \"D");
    essai("guillemet non fermé dans l'argument de go : on ne bouge pas",
          "  go next card \"zz");
    essai("texte en trop sur l'en-tête d'un repeat : aucun tour",
          "  repeat with i = 1 to 2 zz\n    put i into g\n  end repeat");
    essai("un « ¬ » ne cache pas la fin de la ligne",
          "  put \"posé\" into g ¬\n  zz");

    puts("\n== 2. ce qui suit la ligne fautive ne tourne pas non plus ==");
    essai("le gestionnaire s'arrête sur la faute",
          "  put 1 into g zz\n  put \"après\" into g");

    puts("\n== 3. ce qui la PRÉCÈDE tourne (choix actuel, non mesuré chez HyperCard) ==");
    essai("la ligne d'avant a posé g",
          "  put \"d'abord\" into g\n  delete card zz\"");

    puts("\n== 4. une faute sur la ligne du « end » n'arrête qu'APRÈS le bloc ==");
    essai("end repeat suivi d'un mot : les tours ont lieu",
          "  put 0 into g\n  repeat 2\n    add 1 to g\n  end repeat zz");
    essai("end repeat au lieu de end if : la branche a eu lieu",
          "  if true then\n    put \"dans\" into g\n  end repeat");

    puts("\n== 5. le témoin : une ligne saine s'exécute ==");
    essai("put ordinaire", "  put \"posé\" into g");
    essai("delete card ordinaire", "  delete card \"W\"");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
