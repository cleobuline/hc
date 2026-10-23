/* epreuves.c — des externes qui se conduisent mal, pour le harnais xcmd.
 *
 * Chacun éprouve un garde-fou de hc_wasm.c. Le .wasm est commis à côté,
 * reconstruit par refais.sh : le harnais n'a pas besoin d'un clang qui
 * connaisse wasm32. */
#include <stdio.h>
#include "hc_xcmd.h"

/* Une boucle sans fin : Cmd-., ou le délai, doivent l'arrêter. */
HC_XCMD(boucle)
{
    (void)paramPtr;
    volatile int x = 0;
    for (;;) x++;
}

/* Fait attendre HC, puis boucle : un Cmd-. pendant l'attente doit arrêter
 * l'attente, puis le module. */
HC_XCMD(patiente)
{
    (void)paramPtr;
    SendCardMessage("wait 100 seconds");
    volatile int x = 0;
    for (;;) x++;
}

/* Envoie des lignes sans fin : HC doit respirer quand même, et entendre
 * Cmd-. entre deux lignes. */
HC_XCMD(bavarde)
{
    (void)paramPtr;
    for (;;) SendCardMessage("get 1");
}

/* Une faute : l'instruction « unreachable » de WebAssembly. */
HC_XCMD(faute)
{
    (void)paramPtr;
    __builtin_trap();
}

/* Un pointeur hors de sa mémoire, passé à HC, puis rendu à HC. */
HC_XCMD(hors)
{
    (void)paramPtr;
    SendCardMessage((const char *)0x7ffffff0);
}

HC_XFCN(hors)
{
    paramPtr->returnValue = (char *)0x7ffffff0;
}

/* S'appelle lui-même, par la boîte de messages. */
HC_XCMD(reentre)
{
    (void)paramPtr;
    SendCardMessage("reentre");
}

/* Supprime la carte courante par une ligne de HyperTalk : ce qui tenait
 * la carte pendant l'appel ne doit pas la relire ensuite (audits
 * d'octobre). */
HC_XCMD(supprime)
{
    (void)paramPtr;
    SendCardMessage("delete this card");
}

/* Un XCMD qui rend une valeur : elle doit devenir « the result ». */
HC_XCMD(resultat)
{
    paramPtr->returnValue = "fait";
}

/* Passe la main, comme « pass ». */
HC_XCMD(passe)
{
    paramPtr->passFlag = 1;
}

/* Une ligne de HyperTalk qui ne se lit pas. */
HC_XCMD(mauvaise)
{
    (void)paramPtr;
    SendCardMessage("put 1 into");
}

/* Rend ses paramètres : leur nombre, puis chacun. Pas « params », qui est
 * une fonction de HyperTalk, et qui répondrait à sa place. */
HC_XFCN(compte)
{
    static char s[2000];
    int k = snprintf(s, sizeof s, "%d", paramPtr->paramCount);
    for (int i = 0; i < paramPtr->paramCount && k < (int)sizeof s; i++)
        k += snprintf(s + k, sizeof s - (size_t)k, "|%s", paramPtr->params[i]);
    paramPtr->returnValue = s;
}
