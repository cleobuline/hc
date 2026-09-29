/* showPict — la peinture d'une couche se cache sans s'effacer.
 *
 * « set the showPict of this card to false » est l'idiome du bouton « Hide Card
 * Picture », et « Readymade Buttons » en fait son sujet :
 *
 *     on mouseUp
 *       if "hide" is in short name of me then
 *         set name of me to "Show Card Picture"
 *         set showPict of this card to false
 *       else
 *         set name of me to "Hide Card Picture"
 *         set showPict of this card to true
 *       end if
 *     end mouseUp
 *
 * La propriété n'existait pas du tout : « propriété inconnue : showPict ».
 *
 * SON JUMEAU EST cantDelete — même mot de drapeaux à 0x14 dans le fichier
 * d'origine, même chemin à travers les étages — MAIS AVEC DEUX DIFFÉRENCES, et
 * ce harnais existe surtout pour elles :
 *
 *   la valeur par DÉFAUT est VRAIE, donc un objet neuf doit la poser et non
 *     compter sur le calloc ;
 *   le bit du fichier est INVERSÉ — la spec le nomme « not show pict ».
 *
 * L'une oubliée cache la peinture de toutes les piles ; l'autre la montre
 * toujours. Les deux sont des fautes SILENCIEUSES : rien ne refuse, l'écran
 * ment. D'où des témoins à chaque étage, et pas seulement sur la lecture.
 */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d; printf("   %s%s\n", k == HC_ERR ? "[ERREUR] " : "", t); }

static Object *b;
static void joue(const char *quoi, const char *corps)
{
    char s[600];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", corps);
    printf("-- %s\n", quoi);
    hc_set_script(b, s);
    hc_send(b, "t");
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne; hc_set_host(&h);
    Object *st = hc_new_stack("T"); hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    b = hc_new_button(c, "B");

    puts("=== 1. LA VALEUR PAR DEFAUT EST VRAIE ===");
    puts("(une couche neuve MONTRE sa peinture. Si ce temoin disait false, toute");
    puts(" pile ouverte apparaitrait sans son dessin, et rien ne le signalerait.)");
    joue("showPict d'une carte neuve",  "put the showPict of this card");
    joue("showPict d'un fond neuf",     "put the showPict of this background");

    puts("");
    puts("=== 2. ELLE SE POSE ET SE RELIT ===");
    joue("poser a false, puis relire",
         "set the showPict of this card to false\n  put the showPict of this card");
    joue("et la remettre a true",
         "set the showPict of this card to true\n  put the showPict of this card");
    joue("le fond a sa propre valeur",
         "set the showPict of this background to false\n"
         "  put \"carte \" & the showPict of this card"
         " & \"   fond \" & the showPict of this background");

    puts("");
    puts("=== 3. LA LIGNE DE LA PILE, MOT POUR MOT ===");
    hc_set_script(b,
        "on mouseUp\n"
        "  if \"hide\" is in short name of me then\n"
        "    set name of me to \"Show Card Picture\"\n"
        "    set showPict of this card to false\n"
        "  else\n"
        "    set name of me to \"Hide Card Picture\"\n"
        "    set showPict of this card to true\n"
        "  end if\n"
        "  put the short name of me & \"   showPict \" & the showPict of this card\n"
        "end mouseUp\n");
    { free(b->name); b->name = strdup("Hide Card Picture"); }
    printf("-- premier clic (le bouton dit « Hide »)\n");
    hc_send(b, "mouseUp");
    printf("-- second clic (il dit maintenant « Show »)\n");
    hc_send(b, "mouseUp");

    puts("");
    puts("=== 4. LA PEINTURE N'EST PAS EFFACEE, SEULEMENT CACHEE ===");
    puts("(c'est toute la difference avec « choose select tool / delete » : le");
    puts(" calque doit survivre au masquage, sans quoi le bouton « Show » de la");
    puts(" pile ne ramenerait rien.)");
    b = hc_new_button(c, "B2");
    hc_set_paint(c, "PEINTURE-DE-LA-CARTE");
    joue("cacher, puis regarder le calque",
         "set the showPict of this card to false\n"
         "  put \"showPict \" & the showPict of this card");
    printf("   le calque contient : « %s »\n", hc_paint_of(c) ? hc_paint_of(c) : "(rien)");

    puts("");
    puts("=== 5. ELLE TRAVERSE hc_save ET hc_load ===");
    puts("(c'est « hidepict » qui s'ecrit dans NOTRE format — pas un mot de");
    puts(" HyperTalk, un marqueur de fichier comme « cantdelete » — et seulement");
    puts(" quand la peinture est cachee : une pile enregistree AVANT que cette");
    puts(" propriete existe se relit donc en montrant son dessin.)");
    {
        /* REMETTRE CE QUE LES SECTIONS D'AVANT ONT CACHÉ. Sans cela les quatre
         * couches sortaient cachées et le témoin comptait QUATRE marqueurs là
         * où il en annonçait deux : mon propre harnais mesurait autre chose que
         * ce qu'il disait — la huitième fois cette semaine. Un état mélangé vaut
         * mieux qu'un état uniforme : il prouve que le marqueur est PAR COUCHE
         * et non global. */
        c->show_pict = 1;
        bg->show_pict = 1;
        Object *bg2 = hc_new_background(st, "Deux");
        Object *c2  = hc_new_card(st, bg2, "Cachee");
        c2->show_pict = 0;
        bg2->show_pict = 0;
        const char *chemin = "/tmp/hc_showpict.stack";
        int r = hc_save(st, chemin);
        printf("   hc_save : %s\n", r == 0 ? "ecrit" : "ECHEC");
        /* le marqueur est-il dans le fichier, et UNE SEULE FOIS par couche ? */
        FILE *fp = fopen(chemin, "rb");
        int n = 0;
        if (fp) { char l[512];
            while (fgets(l, sizeof l, fp)) if (strncmp(l, "hidepict", 8) == 0) n++;
            fclose(fp); }
        printf("   « hidepict » ecrit %d fois (2 attendues : « Deux » et « Cachee »)\n", n);

        hc_unregister_stack(st); hc_free(st);
        Object *st2 = hc_load(chemin);
        if (!st2) { printf("   hc_load a REFUSE\n"); return 1; }
        printf("   apres relecture :\n");
        for (int i = 0; i < st2->nparts; i++) {
            Object *o = st2->parts[i];
            if (o->type != OBJ_CARD && o->type != OBJ_BACKGROUND) continue;
            printf("     %-11s « %-7s » showPict %s\n",
                   o->type == OBJ_CARD ? "carte" : "fond",
                   o->name ? o->name : "", o->show_pict ? "true" : "false");
        }
        remove(chemin);
        hc_free(st2);
    }
    return 0;
}
