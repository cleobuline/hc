/* « the top of card window » : LA FENÊTRE SE PLACE PAR RAPPORT À L'ÉCRAN.
 *
 * Rapporté le 2 octobre, dans une vraie pile :
 *
 *     put (bottom of target + top of card window + 1) into tp
 *
 * levait « un nombre est attendu ici ». « top », « left », « bottom »,
 * « right » de la card window n'étaient pas servis — ni par la v3, ni avant
 * par l'ancien moteur, vérifié sur le noyau d'avant sa suppression. Seuls
 * width, height, rect et loc l'étaient, et rect valait toujours
 * 0,0,largeur,hauteur, comme si la fenêtre était au coin de l'écran.
 *
 * La référence (hypercard.center, fiches top et rectangle) dit que HyperCard
 * mesure la card window depuis le coin haut-gauche de l'écran qui porte la
 * barre de menus. Le script ci-dessus ne veut rien dire autrement : il passe
 * d'un point de la carte à un point de l'écran.
 *
 * L'hôte donne le coin de la carte sur l'écran. Le harnais joue deux fois :
 *
 *   1. sans hôte qui réponde : la fenêtre au coin de l'écran, rect comme
 *      avant ;
 *   2. avec un hôte qui place la carte en 100,60.
 *
 * NON MESURÉ DANS HYPERCARD : les valeurs elles-mêmes, et « the loc of card
 * window », que la référence dit être le coin haut-gauche de la fenêtre et
 * que HC rend encore comme le centre de la carte. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static const char *place(const char *nom)
{
    if (nom && strcmp(nom, "card window topLeft") == 0) return "100,60";
    return NULL;
}

static const char *CAS[] = {
    "put the width of card window",
    "put the height of card window",
    "put the rect of card window",
    "put the left of card window",
    "put the top of card window",
    "put the right of card window",
    "put the bottom of card window",
    "put the topLeft of card window",
    "put the bottomRight of card window",
    "put the loc of card window",
    "put top of card window",
    "put the top of the card window",
    "put the top of cd window",
    "-- « card window » s'arrete au mot window : le reste est au calcul",
    "put the top of card window + 1",
    "put (bottom of target + top of card window + 1) into tp\n  put tp",
    "-- le rang d'une carte garde son droit a l'arithmetique",
    "put the short name of card 1 + 1",
    "-- une propriete qui n'est pas servie reste un refus",
    "put the zorglub of card window",
    NULL
};

static void joue(Object *b)
{
    for (int i = 0; CAS[i]; i++) {
        if (CAS[i][0] == '-') { printf("   %s\n", CAS[i]); continue; }
        char s[512];
        printf("   %s\n", CAS[i]);
        snprintf(s, sizeof s, "on mouseUp\n  %s\nend mouseUp\n", CAS[i]);
        hc_set_script(b, s);
        hc_send(b, "mouseUp");
    }
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("Pile");
    st->w = 512; st->h = 342;
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_new_card(st, bg, "Deux");
    hc_register_stack(st);
    hc_set_current_card(c);
    Object *b = hc_new_button(c, "B");
    b->x = 10; b->y = 20; b->w = 100; b->h = 30;

    puts("== 1. sans hote qui reponde : la fenetre au coin de l'ecran ==");
    joue(b);

    puts("\n== 2. un hote qui place la carte en 100,60 ==");
    h.global_get = place;
    hc_set_host(&h);
    joue(b);

    hc_free(st);
    return 0;
}
