/* COPIER-COLLER UN BOUTON OU UN CHAMP : rien ne doit rester derriere.
 *
 * clone_part() recopie l'objet CHAMP PAR CHAMP. Une liste ecrite a la main
 * derive : chaque propriete ajoutee a struct Object depuis des mois devait
 * etre ajoutee la aussi, et ne l'a pas ete. Mesure, en posant les proprietes
 * puis en copiant-collant :
 *
 *   text_align       source 2     copie 0     PERDU
 *   auto_select      source 1     copie 0     PERDU
 *   multiple_lines   source 1     copie 0     PERDU
 *   dont_wrap        source 1     copie 0     PERDU
 *   textheight       source 27    copie 0     PERDU
 *   family           source 5     copie 0     PERDU
 *   titlewidth       source 48    copie 0     PERDU
 *
 * Cinq de ces sept etaient la bien avant la famille. Un champ « ne pas couper
 * les mots », copie-colle, se remettait a couper — sans message, sur un objet
 * qui a l'air identique. C'est le defaut le plus desagreable qui soit : la
 * copie ment sur sa fidelite.
 *
 * ── SI VOUS AJOUTEZ UN CHAMP A struct Object ────────────────────────────
 *
 * Il y a QUATRE endroits a servir, et ce harnais n'en garde qu'un :
 *
 *   1. clone_part()        dans hc_core.c  — la copie (garde ici)
 *   2. l'ecriture          dans hc_file.c  — sinon perdu a l'enregistrement
 *   3. la lecture          dans hc_file.c  — sinon perdu au rechargement
 *   4. ce harnais          — ajoutez la ligne COMPARE, sinon plus de garde
 *
 * Les points 2 et 3 sont tenus par le harnais « famille », qui enregistre et
 * relit. Le 4 ne peut etre tenu par personne : c'est une discipline, pas un
 * test. D'ou cette liste en toutes lettres.
 */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)k; (void)d; (void)t; }

static int perdus = 0;

#define COMPARE(champ)                                                        \
    do {                                                                      \
        if (src->champ != cp->champ) {                                        \
            printf("   %-16s source %-6d copie %-6d *** PERDU ***\n",         \
                   #champ, (int)src->champ, (int)cp->champ);                  \
            perdus++;                                                         \
        } else                                                                \
            printf("   %-16s %-6d transmis\n", #champ, (int)src->champ);      \
    } while (0)

#define COMPARE_TXT(champ)                                                    \
    do {                                                                      \
        const char *a = src->champ ? src->champ : "";                         \
        const char *b = cp->champ  ? cp->champ  : "";                         \
        if (strcmp(a, b)) {                                                   \
            printf("   %-16s [%s] -> [%s] *** PERDU ***\n", #champ, a, b);    \
            perdus++;                                                         \
        } else                                                                \
            printf("   %-16s [%s] transmis\n", #champ, a);                    \
    } while (0)

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    Object *st = hc_new_stack("Pile");
    Object *bg = hc_new_background(st, "Fond");
    Object *c  = hc_new_card(st, bg, "Une");
    hc_set_current_card(c);
    hc_register_stack(st);

    /* ── un CHAMP, toutes proprietes posees a une valeur NON PAR DEFAUT ──
     *
     * Le point delicat : une propriete laissee a son defaut passerait le test
     * meme si clone_part l'oubliait, les deux valant zero. On pose donc tout
     * a une valeur qui n'est pas celle d'un objet neuf. */
    Object *f = hc_new_field(c, "source");
    f->x = 11; f->y = 22; f->w = 133; f->h = 44;
    f->visible = 0;
    f->locktext = 1; f->wide_margins = 1; f->fixed_lh = 1;
    f->show_lines = 1; f->auto_tab = 1; f->dont_search = 1;
    f->cant_delete = 1; f->shared_text = 1;
    f->text_align = 2; f->auto_select = 1; f->multiple_lines = 1;
    f->dont_wrap = 1; f->textheight = 27; f->textsize = 18;
    f->textstyle = 3; f->scroll = 7; f->showname = 0;
    hc_set_field_text(f, "du texte");
    hc_set_script(f, "on mouseUp\n  put 1\nend mouseUp\n");

    hc_copy_part(f);
    Object *cp = hc_paste_part(c);
    if (!cp) { puts("rien colle : le harnais ne mesure rien"); return 1; }
    Object *src = f;

    puts("== un CHAMP ==");
    /* x et y NE SONT PAS compares a l'identique : hc_paste_part decale le
     * colle quand la place est deja prise, pour qu'il ne se cache pas
     * exactement derriere l'original — comme HyperCard. Ma premiere version
     * de ce harnais l'accusait de perte. On verifie donc que la position a
     * ete DECALEE, pas remise a zero : un decalage garde l'ecart entre les
     * deux coins, un oubli les mettrait tous les deux a zero. */
    printf("   %-16s %d,%d -> %d,%d (decale, voulu)\n", "position",
           src->x, src->y, cp->x, cp->y);
    if (cp->x == 0 && cp->y == 0 && (src->x || src->y)) {
        puts("      *** REMIS A ZERO, ce n'est pas un decalage ***");
        perdus++;
    }
    COMPARE(w); COMPARE(h);
    COMPARE(visible); COMPARE(showname);
    COMPARE(locktext); COMPARE(wide_margins); COMPARE(fixed_lh);
    COMPARE(show_lines); COMPARE(auto_tab); COMPARE(dont_search);
    COMPARE(cant_delete);
    COMPARE(text_align); COMPARE(auto_select); COMPARE(multiple_lines);
    COMPARE(dont_wrap);
    COMPARE(textsize); COMPARE(textheight); COMPARE(textstyle); COMPARE(scroll);
    COMPARE_TXT(name); COMPARE_TXT(script); COMPARE_TXT(contents);

    /* shared_text n'est PAS compare : coller sur une carte remet le partage a
     * faux, deliberement — un champ ne peut etre partage que sur un fond, et
     * c'est le comportement d'HyperCard. Le verifier serait verifier qu'une
     * correction n'a pas eu lieu. */
    printf("   %-16s %d sur la carte (remis a faux : voulu)\n",
           "shared_text", cp->shared_text);

    /* ── un BOUTON ── */
    Object *b = hc_new_button(c, "bouton");
    b->x = 5; b->y = 6; b->w = 77; b->h = 28;
    b->autohilite = 1; b->enabled = 0; b->icon = 20554;
    b->selectedline = 3; b->textsize = 14; b->textheight = 19;
    b->titlewidth = 48; b->showname = 0; b->shared_hilite = 0;
    hc_set_family(b, 5);
    hc_set_script(b, "on mouseUp\n  beep\nend mouseUp\n");

    hc_copy_part(b);
    Object *cb = hc_paste_part(c);
    if (!cb) { puts("rien colle : le harnais ne mesure rien"); return 1; }
    src = b; cp = cb;

    puts("\n== un BOUTON ==");
    printf("   %-16s %d,%d -> %d,%d (decale, voulu)\n", "position",
           src->x, src->y, cp->x, cp->y);
    if (cp->x == 0 && cp->y == 0 && (src->x || src->y)) {
        puts("      *** REMIS A ZERO, ce n'est pas un decalage ***");
        perdus++;
    }
    COMPARE(w); COMPARE(h);
    COMPARE(visible); COMPARE(showname); COMPARE(enabled);
    COMPARE(autohilite); COMPARE(shared_hilite);
    COMPARE(icon); COMPARE(selectedline);
    COMPARE(family); COMPARE(titlewidth);
    COMPARE(textsize); COMPARE(textheight);
    COMPARE_TXT(name); COMPARE_TXT(script); COMPARE_TXT(style);

    /* L'identifiant, lui, DOIT changer : deux objets de meme id rendraient
     * « field id 42 » ambigu. C'est la seule difference voulue. */
    puts("\n== ce qui doit DIFFERER ==");
    printf("   id           source %d, copie %d : %s\n",
           src->id, cp->id, src->id != cp->id ? "different, voulu" : "*** IDENTIQUE ***");
    if (src->id == cp->id) perdus++;

    printf("\n   proprietes perdues : %d\n", perdus);
    hc_free(st);
    return 0;
}
