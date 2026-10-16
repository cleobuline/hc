/* bonjour.c — le premier module de HC : une commande et une fonction.
 *
 *     bonjour "Patricia"        la boîte de messages dit « Bonjour, Patricia ! »
 *     bonjour                   « Bonjour, le monde ! »
 *     put bonjour("Patricia")   la même phrase, rendue par la fonction
 *
 * La commande passe par SendCardMessage, comme un XCMD de 1990 : elle fait
 * jouer à HC une ligne de HyperTalk. La fonction rend sa valeur dans
 * returnValue. Les deux chemins de la chaîne sont ainsi éprouvés. */

#include <stdio.h>
#include "hc_xcmd.h"

static const char *qui(XCmdPtr paramPtr)
{
    return paramPtr->paramCount > 0 ? paramPtr->params[0] : "le monde";
}

HC_XCMD(bonjour)
{
    char ligne[300];
    snprintf(ligne, sizeof ligne, "put \"Bonjour, %s !\"", qui(paramPtr));
    SendCardMessage(ligne);
}

HC_XFCN(bonjour)
{
    static char phrase[300];
    snprintf(phrase, sizeof phrase, "Bonjour, %s !", qui(paramPtr));
    paramPtr->returnValue = phrase;
}
