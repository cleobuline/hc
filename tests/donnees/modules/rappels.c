/* rappels.c — les rappels de HyperCard, un par un, pour le harnais xcmd.
 *
 * Chaque externe en appelle un et rend ce qu'il a reçu, « NULL » pour un
 * échec. Le .wasm est commis à côté, reconstruit par refais.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hc_xcmd.h"

/* Rend une chaîne reçue d'un rappel, ou « NULL ». La chaîne du rappel est
 * gardée jusqu'à l'appel suivant, puis rendue par free. */
static char *g_garde;

static void rends(XCmdPtr p, char *s)
{
    free(g_garde);
    g_garde = s;
    p->returnValue = s ? s : "NULL";
}

static const char *param(XCmdPtr p, int i)
{
    return i < p->paramCount ? p->params[i] : "";
}

HC_XFCN(evalue)     { rends(paramPtr, EvalExpr(param(paramPtr, 0))); }
HC_XFCN(globale)    { rends(paramPtr, GetGlobal(param(paramPtr, 0))); }
HC_XCMD(poseGlobale)
{
    SetGlobal(param(paramPtr, 0), param(paramPtr, 1));
}

HC_XFCN(champ)
{
    rends(paramPtr, GetFieldByName(atoi(param(paramPtr, 0)), param(paramPtr, 1)));
}
HC_XFCN(champNum)
{
    rends(paramPtr, GetFieldByNum(atoi(param(paramPtr, 0)), atoi(param(paramPtr, 1))));
}
HC_XFCN(champID)
{
    rends(paramPtr, GetFieldByID(atoi(param(paramPtr, 0)), atoi(param(paramPtr, 1))));
}

/* the result : 1 si le champ existait, 0 sinon. */
HC_XCMD(poseChamp)
{
    int r = SetFieldByName(atoi(param(paramPtr, 0)), param(paramPtr, 1),
                           param(paramPtr, 2));
    paramPtr->returnValue = r ? "1" : "0";
}
HC_XCMD(poseChampNum)
{
    int r = SetFieldByNum(atoi(param(paramPtr, 0)), atoi(param(paramPtr, 1)),
                          param(paramPtr, 2));
    paramPtr->returnValue = r ? "1" : "0";
}
HC_XCMD(poseChampID)
{
    int r = SetFieldByID(atoi(param(paramPtr, 0)), atoi(param(paramPtr, 1)),
                         param(paramPtr, 2));
    paramPtr->returnValue = r ? "1" : "0";
}

/* Comme le FORTH de MAC•CAM : ses tables partent dans les globales pln0 à
 * pln3, et la fonction rend l'état de sa pile. */
HC_XFCN(tables)
{
    static char t[64];
    for (int i = 0; i < 4; i++) {
        char nom[8];
        snprintf(nom, sizeof nom, "pln%d", i);
        snprintf(t, sizeof t, "table %d de %s", i, param(paramPtr, 0));
        SetGlobal(nom, t);
    }
    paramPtr->returnValue = "pile vide";
}

/* Dix mille EvalExpr, chacun rendu par free : la mémoire du module ne doit
 * pas grandir. Rend le nombre de pages gagnées. */
HC_XFCN(memoire)
{
    static char s[32];
    size_t avant = __builtin_wasm_memory_size(0);
    for (int i = 0; i < 10000; i++) free(EvalExpr("\"une chaîne de quelques octets\""));
    snprintf(s, sizeof s, "%d", (int)(__builtin_wasm_memory_size(0) - avant));
    paramPtr->returnValue = s;
}
