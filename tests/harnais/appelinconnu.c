/* UN APPEL DE FONCTION INCONNUE RENDAIT SON PROPRE TEXTE.
 *
 *     put maFonction("ok")        -- la fonction n'existe nulle part
 *     -> maFonction ("ok")
 *
 * Aucune erreur, aucun arret : le script continuait, et cette chaine — avec
 * l'espace que la reconstitution insere avant la parenthese — partait dans un
 * champ, dans une comparaison, dans un calcul. Une faute de frappe dans un
 * nom de fonction ne disait donc RIEN, et le resultat ressemblait assez a du
 * texte pour passer inapercu longtemps. HyperCard, lui, repondait « No such
 * function ».
 *
 * C'est la vieille regle « nom inconnu = son propre nom », deja fermee pour
 * les references d'objet (« field "menu" ») et pour les proprietes (« the
 * zorglub »). Les appels de fonction en etaient le dernier refuge.
 *
 * LE RAISONNEMENT : un mot nu peut legitimement valoir lui-meme — une piste
 * de peinture s'appelle « pattern », et « put pattern » a un sens. Une
 * PARENTHESE D'APPEL jamais : celui qui ecrit « maFonction("ok") » demande un
 * calcul, pas une citation. Si les deux moteurs echouent, il faut le dire.
 *
 * OU LA CORRECTION EST POSEE, et pourquoi c'est un point etroit : dans
 * v3_recours, a l'endroit meme ou l'ECHO est deja detecte pour les objets.
 * Le cas ou la fonction EXISTE ne passe pas par la — term_value la trouve et
 * rend autre chose que la demande. Seul l'echec des deux moteurs change.
 *
 * Ce harnais tient :
 *
 *   1. une fonction utilisateur qui EXISTE repond toujours, avec et sans
 *      parentheses, et son resultat n'est pas touche — c'est l'invariant qui
 *      rend la correction sure ;
 *   2. une fonction inconnue leve une erreur NOMMEE : le nom, pas seulement
 *      « fonction inconnue », parce qu'une ligne peut porter deux appels et
 *      que ce refus naît neuf fois sur dix d'une faute de frappe ;
 *   3. le gestionnaire s'ARRETE — la ligne suivante ne s'execute pas. C'est
 *      le point qui distingue une vraie erreur d'un message decoratif ;
 *   4. une fonction qui ne rend RIEN reste legitime : vide n'est pas echec,
 *      et confondre les deux etait le defaut d'a cote (« the result » valant
 *      vide quand tout va bien) ;
 *   5. une fonction du noyau homonyme d'aucune pile marche toujours ;
 *   6. « the result » porte toujours la valeur d'un appel qui a MARCHE. Le
 *      cas du refus n'est pas observable de l'interieur — le gestionnaire
 *      s'arrete, c'est le point 3 —, et pretendre le tenir ici serait un
 *      test qui ne regarde pas.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

static Object *b;

static void execute(const char *titre, const char *corps)
{
    char s[2048];
    printf("   %s\n", titre);
    snprintf(s, sizeof s,
             "function maFonction x\n"
             "  return x & \"!\"\n"
             "end maFonction\n"
             "function rienDuTout x\n"
             "  return empty\n"
             "end rienDuTout\n"
             "on essaie\n%s\nend essaie\n", corps);
    hc_set_script(b, s);
    hc_send(b, "essaie");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);
    b = hc_new_button(c, "B");

    puts("== 1. la fonction EXISTE : rien ne change ==");
    execute("maFonction(\"ok\")",        "  put maFonction(\"ok\")");
    execute("the maFonction of \"ok\"",  "  put the maFonction of \"ok\"");
    execute("imbriquee",                 "  put maFonction(maFonction(\"ok\"))");
    execute("dans un calcul",            "  put the length of maFonction(\"ok\")");

    puts("\n== 2. la fonction est INCONNUE : un refus qui la nomme ==");
    execute("inconnue, un argument",  "  put pasLa(\"ok\")");
    execute("inconnue, deux arguments", "  put pasLaNonPlus(1,2)");
    execute("inconnue, sans argument",  "  put pasLaDuTout()");

    puts("\n== 3. le gestionnaire s'ARRETE : la suite ne tourne pas ==");
    execute("la ligne d'apres",
            "  put pasLa(\"ok\")\n"
            "  put \"CETTE LIGNE NE DOIT PAS PARAITRE\"");

    puts("\n== 4. une fonction qui rend VIDE reste legitime ==");
    execute("rienDuTout(1)",
            "  put rienDuTout(1) into v\n"
            "  put \"vide, et sans erreur : [\" & v & \"]\"");

    puts("\n== 5. les fonctions du noyau ne sont pas touchees ==");
    execute("length",   "  put the length of \"abcde\"");
    execute("offset",   "  put offset(\"c\",\"abc\")");
    execute("sqrt",     "  put sqrt(16)");

    puts("\n== 6. « the result » porte la valeur d'un appel qui a marche ==");
    execute("get maFonction(\"ok\"), puis the result",
            "  get maFonction(\"ok\")\n"
            "  put \"result : [\" & the result & \"]\"");

    hc_free(st);
    return 0;
}
