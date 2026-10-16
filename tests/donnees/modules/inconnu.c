/* inconnu.c — un module qui demande un rappel que HC ne fournit pas : il
 * doit être refusé À L'AJOUT, pas au premier appel. */
#include "hc_xcmd.h"

HC_RAPPEL(Zorglub) void Zorglub(void);

HC_XCMD(inconnu)
{
    (void)paramPtr;
    Zorglub();
}
