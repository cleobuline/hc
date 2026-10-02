/* UN ARGUMENT ABSENT EST UNE CHAÎNE VIDE.
 *
 *     put PopUpMenu(list,,tp,lp) into it      -- Minkowski Stack 1, 1992
 *
 * levait « expression attendue », rapporté le 2 octobre : l'analyseur
 * exigeait une expression à chaque place d'une liste d'arguments. Même chose
 * pour un message, « g 1,,3 ». Seul « send », qui découpe son TEXTE à part,
 * rendait déjà trois paramètres, le deuxième vide : c'est sur lui que les
 * deux autres chemins s'alignent — voir arg_vide dans hct_expr.c.
 *
 * Ce harnais joue chaque forme par les trois chemins, et imprime
 * paramCount suivi des trois premiers paramètres.
 *
 * NON MESURÉ DANS HYPERCARD : la virgule FINALE — « f(1,) », « g 1, » —
 * comptée ici comme une place vide, à l'image de « send ». La place vide
 * entre deux virgules, elle, est dans des piles d'époque qui tournaient. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static const char *CAS[] = {
    "== 1. un appel de fonction ==",
    "put f(1,,3)",
    "put f(,2)",
    "put f(1,)",
    "put f(,)",
    "put f()",
    "put f(1, \"\" ,3)",
    "== 2. un message ==",
    "g 1,,3",
    "g ,2",
    "g 1,",
    "g",
    "== 3. send, qui le faisait deja ==",
    "send \"g 1,,3\" to me",
    "send \"g ,2\" to me",
    "== 4. ce qui reste une faute ==",
    "put f(1,,3",
    "put f(1 + ,3)",
    NULL
};

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);
    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_register_stack(st);
    hc_set_current_card(c);
    Object *b = hc_new_button(c, "B");

    for (int i = 0; CAS[i]; i++) {
        if (CAS[i][0] == '=') { printf("\n%s\n", CAS[i]); continue; }
        char s[1024];
        printf("   %s\n", CAS[i]);
        snprintf(s, sizeof s,
                 "function f\n"
                 "  return paramCount() & \":\" & param(1) & \"|\" & param(2)"
                 " & \"|\" & param(3)\n"
                 "end f\n"
                 "on g\n"
                 "  put paramCount() & \":\" & param(1) & \"|\" & param(2)"
                 " & \"|\" & param(3)\n"
                 "end g\n"
                 "on mouseUp\n  %s\nend mouseUp\n", CAS[i]);
        hc_set_script(b, s);
        hc_send(b, "mouseUp");
    }
    hc_free(st);
    return 0;
}
