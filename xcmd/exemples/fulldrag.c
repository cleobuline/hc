/* fulldrag.c — l'XFCN FullDrag de Frédéric Rinaldi (version 1.3, 1990-1992),
 * que MAC•CAM Workshop emploie, en module de HC.
 *
 *     put fulldrag(the rect of the target, the rect of this card) into Move
 *
 *     <DeltaX,DeltaY> FullDrag(<rect(s)> [,<limit rect> [,<test>,<if true>
 *                              [,<if false>]]])
 *
 * Comme l'original, lu dans son code 68000 : tant que le bouton de la souris
 * est enfoncé, le contour gris des rectangles — un par ligne — suit la
 * souris, qui ne sort pas du rectangle limite (la carte, s'il manque).
 * Majuscule tenue au départ contraint le geste à l'horizontale ou à la
 * verticale, selon le premier mouvement, lu un tiers de seconde plus tard.
 * Elle rend le déplacement, « dh,dv » ; « 0,0 » si l'on a lâché hors de la
 * limite, ou si le bouton était déjà relâché ; une faute, « Error : … »,
 * avec les messages de l'original.
 *
 * NE FAIT PAS ENCORE <test>, <if true> et <if false> : une expression
 * évaluée pendant le glisser, et les messages envoyés selon elle. MAC•CAM
 * ne s'en sert pas ; ici, ils rendent une erreur plutôt que d'être ignorés
 * en silence. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hc_xcmd.h"

enum { RECTS = 64 };

static char g_rendu[128];

static void erreur(XCmdPtr p, const char *quoi)
{
    snprintf(g_rendu, sizeof g_rendu, "Error : %s", quoi);
    p->returnValue = g_rendu;
}

/* « h,v » — the mouseLoc — en Point. */
static Point souris(void)
{
    Point pt = { 0, 0 };
    char *l = EvalExpr("the mouseLoc");
    if (l) {
        char *fin;
        pt.h = (short)strtol(l, &fin, 10);
        if (*fin == ',') pt.v = (short)strtol(fin + 1, NULL, 10);
        free(l);
    }
    return pt;
}

static int touche_majuscule(void)
{
    char *s = EvalExpr("the shiftKey");
    int bas = s && !strcmp(s, "down");
    free(s);
    return bas;
}

HC_XFCN(fulldrag)
{
    Point depart = souris();

    /* Les rectangles, un par ligne ; une ligne vide termine la liste. */
    static Rect rects[RECTS];
    int n = 0;
    const char *s = paramPtr->paramCount > 0 ? paramPtr->params[0] : "";
    if (!*s) {
        erreur(paramPtr, "Missing or empty rect parameter");
        return;
    }
    while (*s && *s != '\r' && *s != '\n' && n < RECTS) {
        char ligne[64];
        size_t k = strcspn(s, "\r\n");
        memcpy(ligne, s, k < 63 ? k : 63);  /* le snprintf du kit n'a pas %.*s */
        ligne[k < 63 ? k : 63] = '\0';
        if (!StrToRect(ligne, &rects[n++])) {
            erreur(paramPtr, "Incorrect rect in param #1");
            return;
        }
        s += k;
        if (*s) s++;
    }

    Rect limite = { 0, 0, 0, 0 };
    if (paramPtr->paramCount > 1) {
        if (!StrToRect(paramPtr->params[1], &limite)) {
            erreur(paramPtr, "Incorrect limit rect");
            return;
        }
    } else {
        char *carte = EvalExpr("the rect of this card");
        StrToRect(carte ? carte : "", &limite);
        free(carte);
    }
    if (paramPtr->paramCount > 2) {
        erreur(paramPtr, "<test>, <if true> and <if false> are not supported "
                         "by HC yet");
        return;
    }

    paramPtr->returnValue = "0,0";
    if (!Button()) return;

    int axe = 0;
    if (touche_majuscule()) {               /* Delay(20), puis l'axe */
        SendCardMessage("wait 20 ticks");
        Point p = souris();
        int dh = abs(p.h - depart.h), dv = abs(p.v - depart.v);
        axe = dv >= dh ? 2 : 1;
    }

    long r = DragGrayRgn(rects, n, depart, &limite, &limite, axe);
    if (HiWord(r) == (short)0x8000 || LoWord(r) == (short)0x8000) return;
    snprintf(g_rendu, sizeof g_rendu, "%d,%d", LoWord(r), HiWord(r));
    paramPtr->returnValue = g_rendu;
}
