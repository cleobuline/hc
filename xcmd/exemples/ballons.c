/* ballons.c — l'XCMD Ballons de MAC•CAM Workshop, en module de HC : une
 * bulle d'aide de System 7 sur ce que la souris survole.
 *
 *     on mouseEnter
 *       ballons the rect of me, "Ouvre la fenêtre de la machine CAM"
 *     end mouseEnter
 *
 * Comme l'original, lu dans son code 68000 : si les bulles sont allumées —
 * « Show Balloons », au menu Help —, le texte s'affiche dans une bulle
 * pointée sur la souris, qui s'efface quand la souris sort du rectangle ;
 * sinon, rien. Sans texte, la bulle dit « Sans titre ». Toute pile peut
 * s'en servir : ajouter ballons.wasm à ses modules suffit. */
#include "hc_xcmd.h"

HC_XCMD(ballons)
{
    if (!HMGetBalloons()) return;
    Rect zone = { 0, 0, 0, 0 };
    if (paramPtr->paramCount > 0) StrToRect(paramPtr->params[0], &zone);
    const char *texte = paramPtr->paramCount > 1 && paramPtr->params[1][0]
                      ? paramPtr->params[1] : "Sans titre";
    HMShowBalloon(texte, &zone);
}
