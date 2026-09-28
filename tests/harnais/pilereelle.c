/* pilereelle.c — Ce qu'une VRAIE pile de 1993 a appris au vérificateur et au
 * lexer.
 *
 * Les deux premiers défauts corrigés ici viennent de « Découvrir HyperCard », la
 * pile d'initiation d'Apple localisée en français, lue avec HC/hc_origine.c. La
 * section 4 vient de « Stack Templates », d'Apple aussi, et bien plus grosse :
 * 198 gestionnaires, 2164 lignes, des palettes, des menus et de l'impression.
 * Aucune pile de torture écrite par nous ne les aurait trouvés : on invente les
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

    /* ------------------------------------------------------------------ */
    /* 3. La porte de version                                             */
    /* ------------------------------------------------------------------ */

    puts("");
    puts("=== 3. « the version » rend celle d'HYPERCARD, pas la notre ===");
    puts("(le script de fond de la pile d'Apple porte « if the version < 2.2 »");
    puts(" et refusait de tourner : « Vous utilisez la version 0.6.9.4 ». La");
    puts(" porte de version etait une pratique standard en 1993, jusque chez");
    puts(" Apple : toute pile qui en porte une restait fermee, sans recours.)");

    joue("the version", "put the version");
    joue("la porte d'Apple, mot pour mot",
         "if the version < 2.2 then\n"
         "    put \"REFUSE : trop vieux\"\n"
         "  else\n"
         "    put \"la pile accepte de tourner\"\n"
         "  end if");
    /* ET LA NÔTRE RESTE LISIBLE, ce qui est la moitié de la décision : un script
     * peut s'adapter à HC en connaissance de cause, ce qu'on ne pouvait pas
     * faire quand les deux valeurs étaient confondues. */
    joue("the hcVersion, sous son propre nom", "put the hcVersion");
    joue("les deux ne sont pas la meme chose",
         "put the version & \" / \" & the hcVersion");

    /* ------------------------------------------------------------------ */
    /* 4. Ce que « Stack Templates » a appris                             */
    /* ------------------------------------------------------------------ */

    puts("");
    puts("=== 4. « Stack Templates » : de 94,4 % a 99,5 % de ses 198 scripts ===");
    puts("(la premiere pile de PRODUCTION d'Apple du corpus. Onze gestionnaires");
    puts(" refuses, en quatre defauts — et chacun se mesure a cote de la forme");
    puts(" VOISINE qui passait deja, sans quoi on chercherait la cause au");
    puts(" mauvais etage.)");

    puts("");
    puts("--- 4a. le ¬ de continuation suivi d'un COMMENTAIRE ---");
    puts("(bouton « Find... » d'Apple. Le ¬ etait avale, la ligne N'ETAIT PAS");
    puts(" continuee, et rien ne le disait : la faute tombait trois lignes plus");
    puts(" loin. MESURE DANS HYPERCARD (Basilisk II) avant d'y toucher : le");
    puts(" dialogue affiche « un deux », sans un mot.)");
    verif("¬ puis un commentaire, puis la suite de la ligne",
          "on t\n  answer \"un\" && \xc2\xac -- commentaire\n  \"deux\"\nend t\n", "rien");
    verif("temoin : ¬ sans commentaire (passait deja)",
          "on t\n  answer \"un\" && \xc2\xac\n  \"deux\"\nend t\n", "rien");
    /* La ligne d'Apple elle-même, mot pour mot, guillemets courbes compris. */
    verif("la ligne d'Apple, mot pour mot",
          "on t\n  find \"x\"\n  if the result = \"Not Found\" then\n"
          "    answer \"unable to find any cards containing\" && \xc2\xac -- \xe2\x88\x86\n"
          "    \"\xe2\x80\x9c\" & it & \"\xe2\x80\x9d.\"\n  end if\nend t\n", "rien");

    puts("");
    puts("--- 4b. « else » n'appartient a aucune expression ---");
    puts("(bouton « Log In » d'Apple : « if bg field \"Log Name\" then logIn else");
    puts(" logOut ». Le si sur une ligne etait ecrit et teste depuis longtemps ;");
    puts(" ce qui manquait est un etage plus bas. Les deux mesures COTE A COTE");
    puts(" nomment la cause — l'argument facultatif d'une commande.)");
    verif("if C then <commande a argument facultatif> else <cmd>",
          "on t\n  if x then beep else beep\nend t\n", "rien");
    verif("temoin : « into y » sature le motif, donc passait deja",
          "on t\n  if x then put 1 into y else beep\nend t\n", "rien");
    verif("la ligne d'Apple, mot pour mot",
          "on t\n  if bg field \"Log Name\" then logIn else logOut\nend t\n", "rien");
    verif("« return » a aussi un argument facultatif",
          "on t\n  if x then return 1 else return 2\nend t\n", "rien");
    /* Et la forme où le « then » ouvre la ligne SUIVANTE : trois fonds de la
     * pile l'écrivent, et c'était le même défaut, pas un second. */
    verif("« then ... else ... » en tete de la ligne suivante",
          "on returnKey\n  if x is not empty and the selectedField is empty\n"
          "  then findText else pass returnKey\nend returnKey\n", "rien");

    puts("");
    puts("--- 4c. « print card from x,y to x,y » : une PARTIE de la carte ---");
    puts("(trois fonds l'ecrivent, avec le commentaire de l'auteur : « prints");
    puts(" only the invoice part of the card ». Et « print card 1 to 600 », que");
    puts(" tout un harnais tient, doit continuer de passer : c'est le butoir du");
    puts(" motif qui decide, et c'est lui qu'il a fallu corriger.)");
    verif("print card from 0,0 to 512,304",
          "on t\n  print card from 0,0 to 512,304\nend t\n", "rien");
    verif("temoin : print card 1 to 600 (le « to » sans « from »)",
          "on t\n  print card 1 to 600\nend t\n", "rien");
    verif("temoin : print all cards",  "on t\n  print all cards\nend t\n", "rien");
    verif("temoin : print marked cards","on t\n  print marked cards\nend t\n", "rien");
    verif("temoin : print this card",  "on t\n  print this card\nend t\n", "rien");

    puts("");
    puts("--- 4d. un ORDINAL devant « menuItem » designe deja ---");
    puts("(sept sites dans la pile. Le garde de menu exige qu'un designateur");
    puts(" SUIVE le mot de type, pour que « put menu into x » ne devienne pas un");
    puts(" menu nomme « into ». « last menuItem of menu \"T\" » met le sien");
    puts(" DEVANT, et ce qui suit est « of ». Deux sites jumeaux a corriger : la");
    puts(" lecture, et le guichet qui decide si l'on y entre.)");
    verif("disable last menuItem of menu",
          "on t\n  disable last menuItem of menu \"Templates\"\nend t\n", "rien");
    verif("enable last menuItem of menu",
          "on t\n  enable last menuItem of menu \"Templates\"\nend t\n", "rien");
    verif("set name of last menuItem of menu ... to ...",
          "on t\n  set name of last menuItem of menu \"T\" to \"Show Palette\"\nend t\n",
          "rien");
    verif("the name of first menu",
          "on t\n  put the name of first menu into x\nend t\n", "rien");
    verif("temoin : menuItem CITE passait deja",
          "on t\n  set checkMark of menuItem \"A\" of menu \"T\" to true\nend t\n", "rien");
    /* LE GARDE DOIT TENIR : c'est lui qu'on vient d'assouplir, et ces deux
     * témoins sont la seule raison de croire qu'on ne l'a pas ouvert trop
     * grand. Sans ordinal devant, rien ne change. */
    verif("garde : « put menu into x » n'est pas un menu nomme « into »",
          "on t\n  put menu into x\nend t\n", "rien");
    verif("garde : « the family of button » n'est pas une famille nommee « of »",
          "on t\n  put the family of button \"X\" into x\nend t\n", "rien");

    puts("");
    puts("--- 4e. CE QUI RESTE REFUSE, ET POURQUOI ---");
    puts("(« get visible of window \"StackTemplatePal\" » : une fenetre NOMMEE.");
    puts(" Ce n'est pas un trou de syntaxe, c'est un type d'objet que l'arbre");
    puts(" n'a pas — hc_core.c le dit en toutes lettres pour « card window », et");
    puts(" ne sert que ses quatre proprietes de geometrie. Notre application n'a");
    puts(" ni palette, ni fenetre Outils, ni fenetre Motifs : repondre quoi que");
    puts(" ce soit a « the visible of window \"X\" » serait DECIDER SEUL de ce");
    puts(" qu'HyperCard aurait repondu. Refus assume, ecrit ici pour qu'il ne");
    puts(" passe pas pour un oubli.)");
    verif("window NOMMEE : refus assume, pas un oubli",
          "on t\n  get visible of window \"StackTemplatePal\"\nend t\n", "erreur");
    verif("temoin : « card window », que hc_core.c sert deja",
          "on t\n  put the width of card window into x\nend t\n", "rien");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
