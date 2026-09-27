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
    if (r.n == 0)                 printf("(connu)\n");
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

    puts("\n== 4. UN PRÉFIXE AMBIGU NE CONSEILLE RIEN ==");
    nom("mouse");             /* mouseUp, mouseDown, mouseEnter, mouseLeave… */
    nom("close");             /* closeStack, closeCard, closeField… */
    nom("new");

    puts("\n== 5. ET CE QUI N'EST PAS UNE COQUILLE non plus ==");
    /* Un gestionnaire peut porter n'importe quel nom : il se déclenche quand
     * on l'appelle. La remarque reste — c'est un AVERTISSEMENT, pas une
     * faute — mais sans conseil, puisqu'il n'y a rien à conseiller. */
    nom("zorglub");
    nom("calculeTout");
    nom("markToday");

    puts("\n== 6. LES FAUTES DE SYNTAXE PASSENT AVANT ==");
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
