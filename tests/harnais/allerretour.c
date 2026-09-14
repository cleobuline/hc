/* Un texte enregistré puis relu doit être LE MÊME, octet pour octet.
 *
 * Il ne l'était pas, et pas seulement dans les cas tordus : « abc » suffisait.
 *
 * L'écrivain découpait le texte sur les sauts de ligne et s'arrêtait sur la
 * fin de chaîne, si bien que « abc » et « abc\n » produisaient EXACTEMENT le
 * même fichier — une seule ligne « | abc ». Le lecteur remettait ensuite un
 * saut après chaque « | », et les deux revenaient en « abc\n ».
 *
 * Et rtrim, appliqué à toutes les lignes avant l'analyse, retirait aussi les
 * espaces et les tabulations DU CONTENU : « abc   » revenait « abc ».
 *
 * C'est un défaut d'intégrité des données, pas de format : l'utilisateur
 * enregistre, rouvre, et son champ a changé sans qu'on le lui dise. Le pire
 * enchaînement est qu'il réenregistre ensuite par-dessus l'original.
 *
 * La propriété que ce harnais vérifie tient en une ligne :
 *
 *     texte avant == texte après
 *
 * Elle porte sur les CHAMPS et sur les SCRIPTS, qui passent par le même
 * mécanisme de bloc. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>

#define FIC "/tmp/hc_allerretour.stack"

static void ma_ligne(HcLineKind k, int d, const char *t)
{ (void)k; (void)d; (void)t; }

/* Les blancs invisibles se voient : sans cela, « abc » et « abc   » auraient
 * la même allure dans le relevé, et un échec passerait pour un succès. */
static void montre(const char *s)
{
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if      (*p == '\n') printf("\\n");
        else if (*p == '\t') printf("\\t");
        else if (*p == ' ')  printf("_");
        else if (*p < 32)    printf("\\x%02x", *p);
        else                 putchar(*p);
    }
}

/* Le champ et le script du même objet font l'aller-retour ensemble : ils
 * empruntent le même mécanisme de bloc, et un défaut de l'un serait un défaut
 * de l'autre. */
static void essai(const char *titre, const char *texte)
{
    Object *st = hc_new_stack("P");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "U");
    Object *f  = hc_new_field(c, "f");
    Object *b  = hc_new_button(c, "b");
    hc_set_current_card(c);
    hc_set_field_text(f, texte);
    hc_set_script(b, texte);

    remove(FIC);
    int code = hc_save(st, FIC);
    hc_free(st);
    if (code != 0) { printf("  %-22s ÉCHEC de la sauvegarde\n", titre); return; }

    Object *rl = hc_load(FIC);
    if (!rl) { printf("  %-22s ÉCHEC de la relecture\n", titre); return; }

    const char *champ = "", *script = "";
    for (int i = 0; i < rl->nparts; i++) {
        Object *ca = rl->parts[i];
        if (ca->type != OBJ_CARD) continue;
        for (int j = 0; j < ca->nparts; j++) {
            if (ca->parts[j]->type == OBJ_FIELD)  champ  = hc_field_text(ca->parts[j]);
            if (ca->parts[j]->type == OBJ_BUTTON) script = hc_script_of(ca->parts[j]);
        }
    }
    if (!script) script = "";

    printf("  %-22s [", titre); montre(texte); printf("]  champ %s  script %s\n",
           strcmp(texte, champ)  == 0 ? "=" : "*** ≠ ***",
           strcmp(texte, script) == 0 ? "=" : "*** ≠ ***");
    if (strcmp(texte, champ) != 0)  { printf("        champ  reçu [");  montre(champ);  printf("]\n"); }
    if (strcmp(texte, script) != 0) { printf("        script reçu ["); montre(script); printf("]\n"); }
    hc_free(rl);
}

/* Un fichier de l'ANCIEN format, écrit à la main : pas de ligne « format ».
 * Il doit se relire exactement comme avant ce changement — un saut de ligne
 * après chaque « | » —, sans quoi toutes les piles déjà enregistrées
 * changeraient de contenu en s'ouvrant. */
static const char *V1 =
    "-- pile HyperCard (format maison v1)\n\n"
    "stack \"P\"\nsize 512,342\nend stack\n\n"
    "background \"F\"\nid 2\nend background\n\n"
    "card \"U\" background \"F\"\nid 3\n"
    "field \"f\"\nrect 20,60,220,160\nid 4\n"
    "contents\n| abc\n| def\nend contents\n"
    "end card\n";

int main(void)
{
    setbuf(stdout, NULL);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);

    puts("=== aller-retour : mémoire -> hc_save -> hc_load -> comparaison ===");
    essai("abc",                 "abc");
    essai("abc + retour",        "abc\n");
    essai("abc + 2 espaces",     "abc  ");
    essai("abc + tabulation",    "abc\t");
    essai("a retour b",          "a\nb");
    essai("a retour b retour",   "a\nb\n");
    essai("ligne vide au milieu","a\n\nb");
    essai("espaces devant",      "  abc");
    essai("deux retours",        "a\n\n");
    essai("juste un retour",     "\n");
    essai("texte vide",          "");
    essai("accents",             "été\nà demain  ");
    essai("ligne de barre",      "| pas un separateur");

    puts("\n=== un fichier de l'ancien format se relit comme avant ===");
    {
        FILE *g = fopen(FIC, "w");
        if (g) { fputs(V1, g); fclose(g); }
        Object *rl = hc_load(FIC);
        if (!rl) puts("   *** l'ancien fichier est REFUSÉ ***");
        else {
            const char *t = "";
            for (int i = 0; i < rl->nparts; i++) {
                Object *c = rl->parts[i];
                if (c->type != OBJ_CARD) continue;
                for (int j = 0; j < c->nparts; j++)
                    if (c->parts[j]->type == OBJ_FIELD) t = hc_field_text(c->parts[j]);
            }
            printf("   champ relu : ["); montre(t); printf("]\n");
            printf("   (l'ancien lecteur rendait abc\\ndef\\n — même résultat attendu)\n");
            hc_free(rl);
        }
    }

    remove(FIC);
    return 0;
}
