/* « put » dans un menu : les six formes, et ce que la préposition change.
 *
 * SIGNALÉ : « put "a" into menuItem 1 of menu "X" » disait « ne sait pas
 * faire ». En l'instrumentant on a trouvé pire à côté : la préposition était
 * ignorée, si bien que « put X after menu "Y" » REMPLAÇAIT le menu entier au
 * lieu d'y ajouter un article — une commande d'ajout qui efface, en silence
 * et en rendant un résultat vide.
 *
 * La forme `into menu`, seule écrite dans les harnais existants, donne le bon
 * résultat par accident : c'est exactement pour cela que le défaut a vécu.
 * Les six formes sont donc mesurées ensemble, et séparément. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)d; if (k == HC_MSG) printf("   [msg] %s\n", t);
           else if (k == HC_ERR) printf("   [ERR] %s\n", t); }

static int g_notifs = 0;
static void ma_barre(void) { g_notifs++; }

static Object *g_pile, *g_carte;

static void montre(void)
{
    for (int i = 0; i < hc_menu_nombre(); i++) {
        printf("      menu « %s » :", hc_menu_nom(i));
        for (int j = 0; j < hc_menu_nb_articles(i); j++)
            printf(" [%d]%s%s%s", j + 1, hc_menu_article(i, j),
                   hc_menu_article_coche(i, j) ? "\xe2\x9c\x93" : "",
                   hc_menu_article_actif(i, j) ? "" : "(off)");
        printf("\n");
    }
}

/* Une ligne de HyperTalk, jouée seule, suivie de l'état de la barre. */
static void fais(const char *ligne)
{
    char script[2048];
    snprintf(script, sizeof script, "on essai\n  %s\nend essai\n", ligne);
    hc_set_script(g_pile, script);
    printf("   %s\n", ligne);
    hc_send(g_carte, "essai");
    montre();
}

/* Remettre le menu dans un état connu avant chaque forme : sans cela la
 * mesure d'une forme dépend de celle d'avant, et un défaut se cache. */
static void remet(void)
{
    hc_set_script(g_pile,
        "on remet\n"
        "  if there is a menu \"Essai\" then delete menu \"Essai\"\n"
        "  create menu \"Essai\"\n"
        "  put \"un,deux,trois\" into menu \"Essai\"\n"
        "end remet\n");
    hc_send(g_carte, "remet");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.menus_changed = ma_barre;
    hc_set_host(&h);

    g_pile = hc_new_stack("menus");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);

    printf("=== 1. les trois prépositions sur le MENU ===\n");
    printf("   (« into » remplace tout, « before » pose en tête, « after » au bout)\n");
    remet(); fais("put \"NEUF\" into menu \"Essai\"");
    remet(); fais("put \"NEUF\" before menu \"Essai\"");
    remet(); fais("put \"NEUF\" after menu \"Essai\"");

    printf("=== 2. les trois prépositions sur un ARTICLE ===\n");
    remet(); fais("put \"DEUX\" into menuItem 2 of menu \"Essai\"");
    remet(); fais("put \"DEUX\" before menuItem 2 of menu \"Essai\"");
    remet(); fais("put \"DEUX\" after menuItem 2 of menu \"Essai\"");

    printf("=== 3. aux deux bouts, là où un décalage se voit ===\n");
    remet(); fais("put \"AV\" before menuItem 1 of menu \"Essai\"");
    remet(); fais("put \"AP\" after menuItem 3 of menu \"Essai\"");

    printf("=== 4. l'article désigné par son NOM, pas par son rang ===\n");
    remet(); fais("put \"DEUX\" into menuItem \"deux\" of menu \"Essai\"");
    remet(); fais("put \"X\" after menuItem \"trois\" of menu \"Essai\"");

    printf("=== 5. une LISTE dans un seul article ===\n");
    printf("   (l'article visé s'en va, les trois nouveaux prennent sa place)\n");
    remet(); fais("put \"a,b,c\" into menuItem 2 of menu \"Essai\"");

    printf("=== 6. ce que les survivants gardent ===\n");
    printf("   (insérer en tête ne doit pas relever ce qui était désactivé)\n");
    remet();
    fais("disable menuItem 2 of menu \"Essai\"");
    fais("set the checkMark of menuItem 3 of menu \"Essai\" to true");
    fais("put \"AV\" before menuItem 1 of menu \"Essai\"");

    printf("=== 7. le MESSAGE suit l'article, pas son rang ===\n");
    printf("   (on choisit un article et on regarde ce qui part)\n");
    hc_set_script(g_pile,
        "on prepare\n"
        "  if there is a menu \"Essai\" then delete menu \"Essai\"\n"
        "  create menu \"Essai\"\n"
        "  put \"un,deux\" into menu \"Essai\" with menuMsg \"disUn,disDeux\"\n"
        "end prepare\n"
        "on disUn\n  put \"-> disUn\"\nend disUn\n"
        "on disDeux\n  put \"-> disDeux\"\nend disDeux\n"
        "on disZero\n  put \"-> disZero\"\nend disZero\n");
    hc_send(g_carte, "prepare");
    montre();
    printf("   on choisit l'article 2 (« deux ») :\n");
    hc_menu_choisi(0, 1);
    hc_set_script(g_pile,
        "on ajoute\n"
        "  put \"zero\" before menu \"Essai\" with menuMsg \"disZero\"\n"
        "end ajoute\n"
        "on disUn\n  put \"-> disUn\"\nend disUn\n"
        "on disDeux\n  put \"-> disDeux\"\nend disDeux\n"
        "on disZero\n  put \"-> disZero\"\nend disZero\n");
    hc_send(g_carte, "ajoute");
    montre();
    printf("   « deux » est maintenant l'article 3 ; on le choisit :\n");
    hc_menu_choisi(0, 2);
    printf("   et l'article 1, le nouveau :\n");
    hc_menu_choisi(0, 0);
    printf("   on REMPLACE l'article 2 et son message :\n");
    hc_set_script(g_pile,
        "on remplace\n"
        "  put \"UN\" into menuItem 2 of menu \"Essai\" with menuMsg \"disDeux\"\n"
        "end remplace\n"
        "on disUn\n  put \"-> disUn\"\nend disUn\n"
        "on disDeux\n  put \"-> disDeux\"\nend disDeux\n"
        "on disZero\n  put \"-> disZero\"\nend disZero\n");
    hc_send(g_carte, "remplace");
    montre();
    hc_menu_choisi(0, 1);

    printf("=== 8. ce qui n'existe pas, et la raison exacte ===\n");
    remet();
    fais("put \"x\" into menuItem 2 of menu \"Absent\"");
    fais("put \"x\" into menuItem 9 of menu \"Essai\"");
    fais("put \"x\" after menuItem \"jamais\" of menu \"Essai\"");
    fais("put \"x\" into menu \"Absent\"");

    printf("=== 9. le plafond : on refuse EN ENTIER, on n'ampute pas ===\n");
    remet();
    hc_set_script(g_pile,
        "on trop\n"
        "  put \"a\" into liste\n"
        "  repeat 80 times\n"
        "    put \",a\" after liste\n"
        "  end repeat\n"
        "  put liste after menu \"Essai\"\n"
        "  put the result & \" / \" & the number of menuItems of menu \"Essai\"\n"
        "end trop\n");
    hc_send(g_carte, "trop");
    montre();

    printf("=== 10. les notifications de l'hôte ===\n");
    printf("      %d notification(s) en tout\n", g_notifs);
    return 0;
}
