/* LA REMISE À ZÉRO DU BILAN, DANS SES TROIS FORMES.
 *
 * Ce harnais naît d'un relevé sur une vraie pile. « debug bilan raz » avait
 * été tapé dans HC, suivi de « debug bilan », et le rapport était vide — ce
 * qui pouvait aussi bien dire « rien ne retombe sur le vieux moteur » que
 * « l'instrument est muet ». On ne pouvait pas trancher de l'extérieur.
 *
 * LE TÉMOIN POSITIF a tranché : « the width of card field "nExistePas" »
 * produit des retours à coup sûr, et il les a bien montrés. L'instrument
 * n'était donc pas muet, et le zéro était vrai.
 *
 * MAIS LE TÉMOIN A RÉVÉLÉ AUTRE CHOSE : « debug bilan raz » ne remettait
 * rien à zéro. v3_cmd_debug ne lisait que son PREMIER mot, comparait
 * « bilan » à « raz », n'y voyait pas de remise à zéro, et imprimait un
 * rapport. Les compteurs restaient intacts.
 *
 * POURQUOI LA SUITE NE L'AVAIT JAMAIS VU : tous les harnais et toutes les
 * piles de tests/donnees écrivent « debug raz », la forme qui marchait. Seul
 * un commentaire de hc_core.c documentait « debug bilan raz ». Le défaut
 * vivait dans la seule forme que personne n'exerçait — et c'est exactement la
 * forme qu'on tape spontanément.
 *
 * C'est la deuxième fois de la journée qu'une porte est annoncée et jamais
 * percée : « the version » interrogeait un hôte qui ne répondait pas. Les
 * deux ont la même signature, un commentaire juste et un chemin absent.
 *
 * CE QUE CE HARNAIS GARDE : les trois formes, ET le témoin positif. Sans le
 * témoin, un jour où le compteur cesserait de compter, toutes les sections
 * passeraient au vert en annonçant zéro retour. Un instrument qu'on ne
 * vérifie pas ment dans le sens qui fait plaisir. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

/* Le bilan sort par emit(HC_INFO), comme la trace. On ne garde que les lignes
 * du bilan : le reste du monitorage noierait la mesure. */
static int g_dans_bilan;
static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k != HC_INFO) return;
    if (strstr(t, "retours de la v3")) { g_dans_bilan = 1; printf("   %s\n", t); return; }
    if (strstr(t, "ancien interpr"))   { printf("   %s\n", t); return; }
    if (!g_dans_bilan) return;
    printf("   %s\n", t);
}
static Object *b;

static void releve(const char *titre)
{
    printf("── %s\n", titre);
    g_dans_bilan = 0;
    hc_do("debug bilan");
    g_dans_bilan = 0;
}

static void temoin(void)
{
    hc_set_script(b, "on t\n"
                     "  put the width of card field \"nExistePas\" into z\n"
                     "end t\n");
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    b = hc_new_button(c, "B");
    hc_set_current_card(c);

    releve("1. AU DEPART : rien n'a tourne, le bilan doit etre vide");

    printf("── 2. LE TEMOIN POSITIF tourne\n");
    temoin();
    releve("2b. et le bilan doit maintenant compter");

    printf("── 3. « debug raz »\n");
    hc_do("debug raz");
    releve("3b. le bilan doit etre revenu a vide");

    printf("── 4. LE TEMOIN tourne de nouveau\n");
    temoin();
    releve("4b. et le bilan compte de nouveau");

    printf("── 5. « debug bilan raz » — LA FORME QUI NE MARCHAIT PAS\n");
    hc_do("debug bilan raz");
    releve("5b. le bilan doit etre revenu a vide, comme en 3b");

    printf("── 6. « debug raz bilan », l'ordre inverse\n");
    temoin();
    hc_do("debug raz bilan");
    releve("6b. vide aussi : « raz » est cherche parmi TOUS les mots");

    printf("── 7. du HyperTalk ordinaire ne doit RIEN compter\n");
    printf("   (la chaine de trace de la rosace, vingt tours)\n");
    hc_set_script(b,
      "on t\n"
      "  put 0 into t\n"
      "  repeat 20 times\n"
      "    put 8*cos(3*t) into r\n"
      "    put round(r*cos(t)*20+256) into x\n"
      "    add pi/144 to t\n"
      "  end repeat\n"
      "end t\n");
    hc_send(b, "t");
    releve("7b. et c'est bien le cas : la v3 fait tout");

    hc_free(st);
    return 0;
}
