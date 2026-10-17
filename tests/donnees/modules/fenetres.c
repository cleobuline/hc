/* fenetres.c — les fenêtres d'un module, pour le harnais xcmd.
 *
 * L'XFCN « fenetre » ouvre une fenêtre et en reçoit ensuite les événements,
 * comme MACCAM : chacun se dit dans la boîte de messages, et chaque message
 * qu'on lui envoie éprouve un rappel de fenêtre, ou un garde-fou. Compilé
 * avec -DAUTRE, il ne porte qu'un XCMD qui dessine dans la fenêtre d'un
 * AUTRE module — ce que HC doit refuser. Le .wasm est commis à côté,
 * reconstruit par refais.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hc_xcmd.h"

static const unsigned char PIXELS[6] = { 0, 1, 2, 2, 1, 0 };

static unsigned char *palette(void)
{
    static unsigned char p[768];
    p[3] = 255;                         /* 1 : rouge */
    p[7] = 200; p[8] = 255;             /* 2 : cyan */
    return p;
}

#ifndef AUTRE

static int  g_ouvertures;
static char g_compte[64] = "0";

static void dis(const char *quoi, int n)
{
    char l[200];
    snprintf(l, sizeof l, "put \"%s %d\"", quoi, n);
    SendCardMessage(l);
}

static void message(XCmdPtr p, XWEventInfoPtr e)
{
    const char *m = e->eventParams[0];
    WindowPtr w = e->eventWindow;
    char l[200];
    snprintf(l, sizeof l, "put \"xSendEvt « %s »\"", m);
    SendCardMessage(l);
    if (!strcmp(m, "dessine")) {
        DrawXWindow(w, PIXELS, 3, 2, palette(), 4);
    } else if (!strcmp(m, "copie")) {
        dis("CopyXWindow :", CopyXWindow(w));
    } else if (!strcmp(m, "guette")) {         /* jusqu'au clic, ou Cmd-. */
        int n = 0;
        while (!Button()) n++;
        dis("Button a dit oui ; tours avant :", n > 0);
    } else if (!strcmp(m, "boucle")) {         /* sans guetter : arrêtée */
        volatile int x = 0;
        for (;;) x++;
    } else if (!strcmp(m, "sourd")) {          /* guette, sans écouter */
        Button();
        volatile int x = 0;
        for (;;) x++;
    } else if (!strcmp(m, "faute")) {
        __builtin_trap();
    } else if (!strcmp(m, "ferme")) {
        CloseXWindow(w);
    } else if (!strcmp(m, "fille")) {          /* xOpenEvt après l'événement */
        Rect r = { 0, 0, 10, 10 };
        dis("NewXWindow pendant un événement :",
            NewXWindow(&r, "Fille", 1, 0, 1, 0) > 0);
    } else if (!strcmp(m, "largeur")) {        /* pendant qu'il tourne */
        SendCardMessage("set the width of window \"Essai\" to 200");
    } else if (!strcmp(m, "reentre")) {
        SendCardMessage("send \"dessine\" to window \"Essai\"");
    } else if (!strcmp(m, "peinture")) {
        Rect r = { 1, 2, 3, 6 };               /* 4 × 2, depuis 2,1 */
        unsigned char noir[8];
        memset(noir, 9, sizeof noir);
        int ok = GetCardPicture(&r, noir);
        snprintf(l, sizeof l, "put \"GetCardPicture : %d — %d%d%d%d %d%d%d%d\"",
                 ok, noir[0], noir[1], noir[2], noir[3],
                 noir[4], noir[5], noir[6], noir[7]);
        SendCardMessage(l);
        Rect vide = { 5, 5, 5, 9 };
        dis("GetCardPicture d'un rectangle vide :", GetCardPicture(&vide, noir));
    } else {
        p->passFlag = 1;                       /* le module ne sait pas */
    }
}

static void evenement(XCmdPtr p)
{
    XWEventInfoPtr e = (XWEventInfoPtr)p->params[0];
    switch (e->what) {
    case xOpenEvt:
        g_ouvertures++;
        dis("xOpenEvt, fenêtre", e->eventWindow);
        return;
    case xCloseEvt:
        dis("xCloseEvt, fenêtre", e->eventWindow);
        return;
    case xSendEvt:
        message(p, e);
        return;
    case xSetPropEvt:
        if (!strcmp(e->eventParams[0], "compte")) {
            snprintf(g_compte, sizeof g_compte, "%s", e->eventParams[1]);
            return;
        }
        p->passFlag = 1;
        return;
    case xGetPropEvt:
        if (!strcmp(e->eventParams[0], "compte")) {
            e->eventResult = g_compte;
            return;
        }
        if (!strcmp(e->eventParams[0], "ouvertures")) {
            static char s[16];
            snprintf(s, sizeof s, "%d", g_ouvertures);
            e->eventResult = s;
            return;
        }
        p->passFlag = 1;
        return;
    }
    p->passFlag = 1;
}

/* put fenetre("Essai") : le numéro de la fenêtre, ou 0. */
HC_XFCN(fenetre)
{
    if (paramPtr->paramCount == -1) {
        evenement(paramPtr);
        return;
    }
    Rect r = { 20, 10, 60, 90 };
    WindowPtr w = NewXWindow(&r, paramPtr->paramCount ? paramPtr->params[0]
                                                      : "Essai", 1, 17, 1, 1);
    static char s[16];
    snprintf(s, sizeof s, "%d", w);
    paramPtr->returnValue = s;
    /* fenetre("Cinq", "ligne") : une ligne jouée APRÈS l'ouverture, pendant
     * l'appel — un autre module, par exemple. */
    if (paramPtr->paramCount >= 2) SendCardMessage(paramPtr->params[1]);
}

#else

/* dessineAutre 1 : dessiner dans la fenêtre 1, qui n'est pas à ce module. */
HC_XCMD(dessineAutre)
{
    if (paramPtr->paramCount < 1) return;
    DrawXWindow(atoi(paramPtr->params[0]), PIXELS, 3, 2, palette(), 1);
    CloseXWindow(atoi(paramPtr->params[0]));
}

#endif
