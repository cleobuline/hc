/* fenetrepile — UN « go » VERS UNE AUTRE PILE, CHEZ UN HÔTE QUI DONNE UNE
 * FENÊTRE À CHAQUE PILE.
 *
 * Signalé le 10 octobre DANS HC (l'application) : un clic sur un mot du
 * glossaire de HyperTalk Reference ouvrait bien Help Extras, puis
 *
 *     set the checkMark of menuItem 4 of menu gHMnu to TRUEorFALSE
 *
 * répondait « menu introuvable ». Le noyau envoyait closeStack à la pile
 * quittée, et l'hôte, dont la fenêtre passait derrière, suspendStack : deux
 * départs pour un seul. HyperCard Help retire son menu au premier ; le
 * setCheckMark du second ne le trouvait plus.
 *
 * La référence d'Apple, carte « closeStack » : « If you have more than one
 * stack open at a time, HyperCard sends suspendStack, not closeStack, when
 * the stack becomes inactive. » Chez HC, l'ancienne pile reste ouverte dans
 * sa fenêtre : seuls les messages de l'hôte valent.
 *
 * LE FAUX HÔTE imite Hcdocument.m, et c'est une IMITATION : envoiePile,
 * setCurrent qui rend à la fenêtre sa carte retenue, suspendStack au départ,
 * resumeStack à l'arrivée. Il suppose que les avis de fenêtre arrivent
 * PENDANT makeKeyAndOrderFront, ce que ce harnais ne peut pas vérifier : il
 * faut macOS. Les piles d'Apple n'entrent pas dans le dépôt ; leurs
 * gestionnaires sont recopiés dans leur FORME. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

/* ---- le faux hôte à fenêtres ---- */
typedef struct { Object *pile; Object *retenue; } Fenetre;
static Fenetre g_fen[2];
static int     g_devant = 0;

static Object *premiere(Object *pile)
{
    for (int i = 0; i < pile->nparts; i++)
        if (pile->parts[i]->type == OBJ_CARD) return pile->parts[i];
    return NULL;
}

/* Hcdocument.m, envoiePile, tel quel. */
static void envoie_pile(Fenetre *f, const char *msg)
{
    Object *p = premiere(f->pile);
    Object *cur = hc_current_card();
    if (!cur || cur->owner != f->pile) hc_set_current_card(p);
    hc_send(p, msg);
}

/* makeKeyAndOrderFront : windowDidResignMain de l'une, puis
 * windowDidBecomeMain de l'autre, qui passe par setCurrent. */
static void devant(int k)
{
    if (k == g_devant) return;
    Fenetre *avant = &g_fen[g_devant], *apres = &g_fen[k];
    Object *cur = hc_current_card();
    if (cur && cur->owner == avant->pile) avant->retenue = cur;   /* son dernier dessin */
    envoie_pile(avant, "suspendStack");
    g_devant = k;
    if (apres->retenue && apres->retenue->owner == apres->pile)
        hc_set_current_card(apres->retenue);
    envoie_pile(apres, "resumeStack");
}

static void pile_changee(Object *pile)
{
    for (int k = 0; k < 2; k++) if (g_fen[k].pile == pile) devant(k);
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);

    /* La pile quittée : la forme de HyperTalk Reference et de HyperCard Help
     * réunies. Le menu naît à l'arrivée et meurt au départ, et le départ
     * décoche d'abord l'article. */
    Object *ref = hc_new_stack("Ref"); hc_register_stack(ref);
    Object *rbg = hc_new_background(ref, "Double List");
    Object *sujet = hc_new_card(ref, rbg, "Sujet");
    Object *b = hc_new_button(sujet, "Mot");
    hc_set_script(ref,
        "on resumeStack\n  put \"Ref       resumeStack\"\n  arrive\nend resumeStack\n"
        "on suspendStack\n  put \"Ref       suspendStack\"\n  part\nend suspendStack\n"
        "on closeStack\n  put \"Ref       closeStack\"\n  part\nend closeStack\n"
        "on closeCard\n  put \"Ref       closeCard\"\nend closeCard\n"
        "on closeBackground\n  put \"Ref       closeBackground\"\nend closeBackground\n"
        "on arrive\n"
        "  if there is not a menu \"Reference\" then\n"
        "    create menu \"Reference\"\n"
        "    put \"Un,Deux,Trois,Quatre\" into menu \"Reference\"\n"
        "  end if\n"
        "  setCheckMark true\n"
        "end arrive\n"
        "on part\n"
        "  setCheckMark false\n"
        "  delete menu \"Reference\"\n"
        "end part\n"
        "on setCheckMark vraiOuFaux\n"
        "  set the checkMark of menuItem 4 of menu \"Reference\" to vraiOuFaux\n"
        "end setCheckMark\n");
    hc_set_script(b,
        "on mouseUp\n"
        "  go card \"Glossary:stack\" of stack \"Extras\" in a new window\n"
        "end mouseUp\n");

    Object *ext = hc_new_stack("Extras"); hc_register_stack(ext);
    Object *ebg = hc_new_background(ext, "Glossaire");
    Object *index = hc_new_card(ext, ebg, "Index");
    hc_new_card(ext, ebg, "Glossary:stack");
    hc_set_script(ext,
        "on openStack\n  put \"Extras    openStack\"\nend openStack\n"
        "on resumeStack\n  put \"Extras    resumeStack\"\nend resumeStack\n"
        "on openBackground\n  put \"Extras    openBackground\"\nend openBackground\n"
        "on openCard\n  put \"Extras    openCard \" & the short name of this card\nend openCard\n");

    puts("== 1. hôte à fenêtres : Extras déjà ouverte, derrière ==");
    g_fen[0].pile = ref; g_fen[0].retenue = sujet;
    g_fen[1].pile = ext; g_fen[1].retenue = index;   /* elle montrait l'Index */
    g_devant = 0;
    h.stack_changed = pile_changee;
    hc_set_host(&h);
    hc_set_current_card(sujet);
    hc_do("create menu \"Reference\"");
    hc_do("put \"Un,Deux,Trois,Quatre\" into menu \"Reference\"");
    hc_send(b, "mouseUp");
    char d[96]; hc_describe(hc_current_card(), d, sizeof d);
    printf("   carte d'arrivée : %s\n", d);
    printf("   menus : [%s]\n", hc_menu_nombre() ? hc_menu_nom(0) : "");

    puts("\n== 2. sans fenêtres : la pile est remplacée, closeStack et openStack ==");
    h.stack_changed = NULL;
    hc_set_host(&h);
    hc_set_current_card(sujet);
    hc_do("create menu \"Reference\"");
    hc_do("put \"Un,Deux,Trois,Quatre\" into menu \"Reference\"");
    hc_set_script(ext,
        "on openStack\n  put \"Extras    openStack\"\nend openStack\n"
        "on openCard\n  put \"Extras    openCard \" & the short name of this card\nend openCard\n");
    hc_send(b, "mouseUp");
    hc_describe(hc_current_card(), d, sizeof d);
    printf("   carte d'arrivée : %s\n", d);

    hc_unregister_stack(ref); hc_free(ref);
    hc_unregister_stack(ext); hc_free(ext);
    return 0;
}
