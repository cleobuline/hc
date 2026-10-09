/* LE VÉRIFICATEUR DE SCRIPT, ET SON CONSEIL SUR LES NOMS DE GESTIONNAIRES.
 *
 * hct_verifie n'avait AUCUN harnais — c'est le bouton « Vérifier » de
 * l'éditeur de script, et rien ne tenait ce qu'il dit.
 *
 * L'OCCASION EST VENUE DE L'USAGE. Un script d'essai portait, côte à côte :
 *
 *     on commandKeyDown k …    -- se déclenche
 *     on commandKey k …        -- ne se déclenchera JAMAIS
 *
 * « commandKey » n'est pas un message : c'est une FONCTION, « the commandKey »
 * rend up ou down, avec « cmdKey » pour synonyme. Rien ne l'envoie, ni chez
 * nous ni chez HyperCard. Le gestionnaire est simplement ignoré, et rien à
 * l'exécution ne le dit — c'est exactement la faute que ce vérificateur
 * existe pour attraper.
 *
 * IL L'ATTRAPAIT DÉJÀ, ET IL N'APPRENAIT RIEN : « n'est pas un message
 * système connu » est vrai et ne dit pas ce qu'on voulait écrire. Il manquait
 * quatre lettres, et c'est cela qu'il fallait dire.
 *
 * DEUX SIGNAUX, ET LEURS LIMITES — c'est tout l'objet de ce harnais :
 *
 *   · le nom est un PRÉFIXE d'un seul message connu. « commandKey » ne peut
 *     être que « commandKeyDown ».
 *
 *   · une distance d'édition de 1 ou 2, à partir de cinq lettres.
 *     « mouseDwon », « closeCrad », « errorDialogue ».
 *
 * ET UN PRÉFIXE AMBIGU NE CONSEILLE RIEN. « on mouse » commence cinq messages ;
 * en proposer un serait tirer au sort. Un conseil faux est pire que pas de
 * conseil : il envoie corriger ce qui va bien. C'est le cas qui garde ce code
 * honnête, et c'est pour lui qu'il est ici.
 *
 * Les noms connus sont écrits dans LA CASSE DU GUIDE, parce qu'ils se
 * recopient maintenant dans la remarque : « vouliez-vous dire commandkeydown »
 * à côté d'un script qui écrit commandKeyDown donne un conseil qui a l'air
 * faux. La reconnaissance, elle, reste insensible à la casse — « on OPENCARD »
 * est toujours un message connu.
 */
#include "hct_verif.h"
#include <stdio.h>
#include <string.h>

static void nom(const char *n)
{
    char src[256];
    snprintf(src, sizeof src, "on %s\nend %s\n", n, n);
    HctRapport r;
    hct_verifie(src, &r, 1);
    printf("   on %-16s ", n);
    if (r.n == 0)                 printf("(rien)\n");
    else if (r.nerreurs)          printf("[ERREUR] %s\n", r.liste[0].message);
    else                          printf("%s\n", r.liste[r.n - 1].message);
    hct_rapport_libere(&r);
}

int main(void)
{
    puts("== 1. LES NOMS CONNUS ne disent rien ==");
    nom("mouseUp");
    nom("opencard");          /* la casse ne compte pas */
    nom("COMMANDKEYDOWN");
    nom("idle");
    nom("doMenu");

    puts("\n== 2. LE PRÉFIXE D'UN SEUL message : on nomme le message ==");
    nom("commandKey");        /* le cas venu de l'usage */
    nom("arrowke");
    nom("returnInFiel");

    puts("\n== 3. UNE OU DEUX LETTRES DE TRAVERS ==");
    nom("mouseDwon");
    nom("closeCrad");
    nom("newcrad");
    nom("errorDialogue");

    puts("\n== 4. UN PRÉFIXE AMBIGU NE CONSEILLE RIEN, et ne dit rien ==");
    nom("mouse");             /* mouseUp, mouseDown, mouseEnter, mouseLeave… */
    nom("close");             /* closeStack, closeCard, closeField… */
    nom("new");

    puts("\n== 5. CE QUI N'EST PAS UNE COQUILLE ne dit rien non plus ==");
    /* Un gestionnaire peut porter n'importe quel nom : il se déclenche quand
     * on l'appelle. La remarque « n'est pas un message système connu »
     * restait sans conseil sur chacun d'eux, et l'éditeur encadrait la
     * ligne : jon23, sur Reddit, l'a prise pour un refus de son « on
     * subroutine param ». Depuis le 9 octobre, rien. */
    nom("zorglub");
    nom("calculeTout");
    nom("markToday");
    nom("subroutine");        /* le cas de jon23 */

    puts("\n== 6. UN NOM PROCHE, MAIS APPELÉ PAR LE SCRIPT : un sous-programme ==");
    /* Idée de l'utilisatrice : on sait lister les gestionnaires d'un script,
     * autant voir qui les appelle. « closeCards » ressemble à « closeCard »,
     * mais le script l'appelle : c'est voulu. Sans appel, la remarque reste. */
    {
        static const char *SCRIPTS[][2] = {
            { "appelé par un message nu",
              "on mouseUp\n  closeCards 3\nend mouseUp\n"
              "on closeCards n\n  beep n\nend closeCards\n" },
            { "appelé par send",
              "on mouseUp\n  send \"closeCards 2\" to me\nend mouseUp\n"
              "on closeCards n\n  beep n\nend closeCards\n" },
            { "jamais appelé",
              "on mouseUp\n  beep\nend mouseUp\n"
              "on closeCards n\n  beep n\nend closeCards\n" },
            { "jon23 : on subroutine param",
              "on mouseUp\n  subroutine 5\nend mouseUp\n"
              "on subroutine param\n  put param * 2 into x\nend subroutine\n" },
        };
        for (int i = 0; i < 4; i++) {
            HctRapport r;
            hct_verifie(SCRIPTS[i][1], &r, 1);
            printf("   %-30s ", SCRIPTS[i][0]);
            if (r.n == 0) printf("(rien)\n");
            for (int k = 0; k < r.n; k++)
                printf("%sligne %d : %s\n", k ? "      " : "", r.liste[k].ligne,
                       r.liste[k].message);
            hct_rapport_libere(&r);
        }
    }

    puts("\n== 7. LES FAUTES DE SYNTAXE PASSENT AVANT ==");
    /* Le vérificateur sert d'abord à ça, et le harnais doit le montrer :
     * une remarque sur un nom ne doit pas masquer une vraie faute. */
    {
        HctRapport r;
        hct_verifie("on mouseUp\n  repeat with i = 1 up to 5\n  end repeat\n"
                    "end mouseUp\n", &r, 1);
        printf("   repeat sans « to »    erreurs=%d\n", r.nerreurs);
        for (int i = 0; i < r.n; i++)
            printf("      ligne %d : %s\n", r.liste[i].ligne, r.liste[i].message);
        hct_rapport_libere(&r);
    }

    return 0;
}
