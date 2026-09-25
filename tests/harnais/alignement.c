/* « the textAlign » d'un champ : le noyau, bout à bout.
 *
 * CE QUE CE HARNAIS PEUT PROUVER, ET CE QU'IL NE PEUT PAS.
 *
 * Le dialogue lui-même — les trois boutons radio « Left / Center / Right » du
 * panneau Text Style — vit dans HCdialogs.m et demande AppKit : il ne se
 * vérifie qu'à la compilation Xcode, et rien ici ne le mesure. Le dire est
 * plus utile que de faire semblant.
 *
 * Mais le dialogue ne fait qu'OUVRIR UNE PORTE, et c'est la pièce derrière
 * qu'on mesure : la propriété se pose, se relit, et surtout SURVIT À
 * L'ENREGISTREMENT. Un alignement choisi à la souris qui repartirait à la
 * réouverture de la pile serait pire que pas de dialogue du tout — il aurait
 * l'air d'avoir marché.
 *
 * Signalé en traduisant HypoGraph 0.91 : le noyau connaissait textAlign
 * depuis toujours, le rendu l'honorait (field_attr_string), le fichier le
 * gardait — et aucun contrôle ne permettait de le poser autrement que par un
 * script. Une propriété qu'un script pouvait écrire et que la souris ne
 * pouvait pas : le défaut de famille, pris à l'envers.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIC "/tmp/hc_alignement.stack"

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *g_pile, *g_carte;
static void fais(const char *ligne)
{
    char script[512];
    snprintf(script, sizeof script, "on essai\n  %s\nend essai\n", ligne);
    hc_set_script(g_pile, script);
    hc_send(g_carte, "essai");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    hc_set_host(&h);

    g_pile  = hc_new_stack("Alignement");
    Object *fond = hc_new_background(g_pile, "Fond");
    g_carte = hc_new_card(g_pile, fond, "Une");
    hc_set_current_card(g_carte);
    hc_new_field(g_carte, "texte");

    printf("=== 1. le défaut, et les trois valeurs que le panneau propose ===\n");
    printf("   (l'indice du bouton radio EST le codage du noyau : 0 1 2)\n");
    fais("put the textAlign of cd fld \"texte\"");
    fais("set the textAlign of cd fld \"texte\" to \"left\"\n"
         "  put the textAlign of cd fld \"texte\"");
    fais("set the textAlign of cd fld \"texte\" to \"center\"\n"
         "  put the textAlign of cd fld \"texte\"");
    fais("set the textAlign of cd fld \"texte\" to \"right\"\n"
         "  put the textAlign of cd fld \"texte\"");

    printf("=== 2. les formes indulgentes qu'on rencontre dans les scripts ===\n");
    fais("set the textAlign of cd fld \"texte\" to \"centre\"\n"
         "  put the textAlign of cd fld \"texte\"");
    fais("set the textAlign of cd fld \"texte\" to \"centered\"\n"
         "  put the textAlign of cd fld \"texte\"");

    printf("=== 3. un mot inconnu retombe à gauche, sans échouer ===\n");
    printf("   (HyperCard est indulgent sur celle-ci ; refuser casserait des\n");
    printf("    piles qui marchaient)\n");
    fais("set the textAlign of cd fld \"texte\" to \"patate\"\n"
         "  put the textAlign of cd fld \"texte\"");

    printf("=== 4. L'ENREGISTREMENT : le choix survit-il à la relecture ? ===\n");
    printf("   (c'est LA question que pose l'ajout du dialogue. Un alignement\n");
    printf("    posé à la souris qui repartirait à la réouverture aurait l'air\n");
    printf("    d'avoir marché, ce qui est pire que de ne pas exister)\n");
    for (int i = 0; i < 3; i++) {
        const char *mots[3] = { "left", "center", "right" };
        char ligne[160];
        snprintf(ligne, sizeof ligne,
                 "set the textAlign of cd fld \"texte\" to \"%s\"", mots[i]);
        fais(ligne);

        if (hc_save(g_pile, FIC) != 0) { printf("   [ERR] enregistrement\n"); continue; }
        Object *rl = hc_load(FIC);
        if (!rl) { printf("   [ERR] relecture\n"); continue; }

        Object *c2 = NULL;
        for (int k = 0; k < rl->nparts; k++)
            if (rl->parts[k]->type == OBJ_CARD) { c2 = rl->parts[k]; break; }
        if (!c2) { printf("   [ERR] pas de carte relue\n"); hc_free(rl); continue; }

        Object *sauve_pile = g_pile, *sauve_carte = g_carte;
        g_pile = rl; g_carte = c2;
        hc_set_current_card(c2);
        char v[160];
        snprintf(v, sizeof v,
                 "put \"%s enregistré, relu : \" & the textAlign of cd fld \"texte\"",
                 mots[i]);
        fais(v);
        g_pile = sauve_pile; g_carte = sauve_carte;
        hc_set_current_card(g_carte);
        hc_free(rl);
    }

    printf("=== 5. le témoin : une coquille voisine reste refusée ===\n");
    fais("set the textAline of cd fld \"texte\" to \"right\"");
    remove(FIC);
    return 0;
}
