/* pilereelle.c — Ce qu'une VRAIE pile de 1993 a appris au vérificateur et au
 * lexer.
 *
 * Les deux défauts corrigés ici viennent de « Découvrir HyperCard », la pile
 * d'initiation d'Apple localisée en français, lue avec HC/hc_origine.c. Aucune
 * pile de torture écrite par nous ne les aurait trouvés : on invente les
 * tournures qu'on connaît déjà.
 *
 * La pile elle-même n'est pas versionnable — « ©Copyright 1993-1995 by Apple
 * Computer,Inc. / Tous droits réservés. » — d'où ce harnais, qui rejoue ses deux
 * lignes décisives sans l'emporter. Voir docs/mesures/pile_origine.txt.
 */
#include "hc_core.h"
#include "hct_verif.h"

#include <stdio.h>
#include <string.h>

#define INF "\xe2\x88\x9e"          /* ∞ : MacRoman 0xB0, le trait du bandeau */
#define COP "\xc2\xa9"              /* © : MacRoman 0xA9 */

/* ------------------------------------------------------------------ */
/* 1. Le vérificateur : la place décide du niveau                      */
/* ------------------------------------------------------------------ */

static void verif(const char *quoi, const char *src, const char *attendu)
{
    HctRapport r;
    memset(&r, 0, sizeof r);
    hct_verifie(src, &r, 0);
    const char *obtenu = r.nerreurs ? "erreur" : (r.n ? "remarque" : "rien");
    printf("%-4s %-52s -> %-8s", strcmp(obtenu, attendu) ? "RATE" : "ok", quoi, obtenu);
    if (r.n) printf("  (%s)", r.liste[0].message);
    printf("\n");
    hct_rapport_libere(&r);
}

static void le_verificateur(void)
{
    puts("=== 1. le niveau d'une faute depend de sa PLACE dans l'arbre ===");
    puts("(ERREUR = « ne peut pas s'executer, HyperCard refuserait aussi »,");
    puts(" c'est le contrat ecrit dans hct_verif.h)");

    /* LE CAS D'APPLE. Son script de pile commence par un bandeau de copyright
     * SANS « -- » : un trait de ∞, le titre, la version, le copyright, les
     * auteurs, un second trait, et les commentaires ne commencent qu'après. En
     * HyperCard un gestionnaire est compilé À SON APPEL, et seul son bloc l'est :
     * ce bandeau n'est jamais analysé, donc ne refuse rien. */
    verif("le bandeau d'Apple devant un gestionnaire",
          INF INF INF "\n"
          "Decouvrir HyperCard\n"
          "Version 2.3\n"
          "\n"
          COP "Copyright 1993-1995 by Apple Computer,Inc.\n"
          "Tous droits reserves.\n"
          INF INF INF "\n"
          "\n"
          "-- S T A C K  S C R I P T\n"
          "on ouvre\n  beep\nend ouvre\n", "remarque");

    /* Un script de pile qui n'a QUE son bandeau et des commentaires : c'est le
     * cas exact d'Apple, dont un commentaire dit « THE SCRIPTS FOR THIS STACK
     * ARE IN THE BACKGROUND SCRIPT ». Rien à compiler, donc rien qui échoue. */
    verif("le bandeau SEUL, aucun gestionnaire du tout",
          INF INF INF "\n" COP "Copyright 1993\n-- les scripts sont au fond\n",
          "remarque");

    verif("du charabia APRES le dernier gestionnaire",
          "on ouvre\n  beep\nend ouvre\n" INF INF " ((( ]]] $$$\n", "remarque");

    /* ET LA CONTREPARTIE, qui est ce qui donne un sens au reste. Ma première
     * version de la règle était POSITIONNELLE — la faute tombe-t-elle dans
     * l'étendue d'un gestionnaire ? — et ce témoin-ci l'a démolie aussitôt : un
     * « end » manquant est signalé APRÈS le dernier jeton du gestionnaire, donc
     * hors de son étendue, donc il passait. Or c'est une faute DU gestionnaire.
     * La règle est structurelle : descendant d'un gestionnaire, ou frère. */
    verif("« end » manquant (faute DU gestionnaire)",
          "on ouvre\n  beep\n", "erreur");
    verif("« end » qui ne reprend pas le nom",
          "on ouvre\n  beep\nend ferme\n", "erreur");
    verif("du charabia DANS un gestionnaire",
          "on ouvre\n  " INF " ((( ]]]\nend ouvre\n", "erreur");
    verif("parenthese non fermee dans un gestionnaire",
          "on ouvre\n  put (1 + 2 into x\nend ouvre\n", "erreur");
    verif("temoin : un gestionnaire sain",
          "on ouvre\n  beep\nend ouvre\n", "rien");

    /* LE CARACTÈRE N'A JAMAIS ÉTÉ EN CAUSE, seulement la place. Mesuré : ∞, ©,
     * # et les accents passent dans un commentaire et dans une chaîne, et
     * restent refusés nus à l'intérieur d'un gestionnaire. */
    puts("--- le caractere lui-meme : dans un commentaire, dans une chaine ---");
    verif("∞ © # et accents dans un commentaire",
          "on ouvre\n  -- " INF " " COP " # \xc3\xa9\xc3\xa0\xc3\xa7\n  beep\nend ouvre\n", "rien");
    verif("∞ © # dans une chaine",
          "on ouvre\n  put \"" INF " " COP " #3\"\nend ouvre\n", "rien");
    verif("# nu dans un gestionnaire : toujours une faute",
          "on ouvre\n  #\nend ouvre\n", "erreur");
}

/* ------------------------------------------------------------------ */
/* 2. Le lexer : le diese d'un musicien                                */
/* ------------------------------------------------------------------ */

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("     %s\n", t);
    else if (k == HC_ERR) printf("     [ERREUR] %s\n", t);
}

static Object *bouton;

static void joue(const char *quoi, const char *corps)
{
    char s[512];
    snprintf(s, sizeof s,
             "on t\n  %s\n  put \"le gestionnaire est alle au bout\"\nend t\n", corps);
    printf("-- %s\n", quoi);
    hc_set_script(bouton, s);
    hc_send(bouton, "t");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    le_verificateur();

    puts("");
    puts("=== 2. le diese d'une note : « play harpsichord tempo 300 a#2q » ===");
    puts("(la ligne exacte d'un bouton « Hilite » de la pile d'Apple. Le lexer");
    puts(" s'arretait dessus AVANT que v3_cmd_play voie quoi que ce soit : le son");
    puts(" ne jouait pas, et l'utilisateur voyait un dialogue d'erreur.)");

    Object *st = hc_new_stack("T");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    bouton = hc_new_button(c, "B");

    joue("la vraie ligne d'Apple, avec deux dieses",
         "play harpsichord tempo 300 a#2q c3w");
    joue("la meme sans diese (temoin : elle passait deja)",
         "play harpsichord tempo 300 a2q c3w");
    joue("un diese seul reste une faute",
         "put #");
    joue("temoin : rien de particulier",
         "beep");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
