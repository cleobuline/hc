/* passusage — UN « pass » ATTEINT LA PILE MISE EN USAGE PENDANT LE GESTIONNAIRE.
 *
 * La chaîne d'un message était figée à l'envoi. « HyperTalk Reference »
 * d'Apple fait, dans son openStack :
 *
 *     start using stack "HyperCard Help"
 *     pass openStack
 *
 * et c'est l'openStack de HyperCard Help, atteint par ce pass, qui crée le
 * menu « Reference ». Le pass n'atteignait personne : pas de menu, et au
 * premier suspendStack ou closeStack, l'aide renvoyait « setCheckMark » à la
 * pile, dont la ligne
 *
 *     set the checkMark of menuItem 4 of menu gHMnu to TRUEorFALSE
 *
 * répondait « menu introuvable ». Signalé le 10 octobre DANS HC
 * (l'application) ; reproduit dans le noyau avec les deux piles chargées et
 * la suite de messages qu'envoie Hcdocument.m.
 *
 * Les gestionnaires sont recopiés ici dans leur FORME — pas dans leur texte,
 * les piles d'Apple n'entrent pas dans le dépôt.
 *
 * DÉDUIT DU CODE D'APPLE, pas mesuré : sans ce comportement, ouvrir
 * « HyperTalk Reference » seule n'aurait pas de menu dans HyperCard non plus.
 * NON MESURÉ dans HyperCard : les sections 3 à 5 (une pile mise en usage
 * AVANT celle qui passe, une pile qui se retire, deux qui se redéclarent).
 * Elles disent ce que fait HC. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("   %s\n", t ? t : "");
  else if (k == HC_ERR) printf("   [ERR] %s\n", t ? t : ""); }

static Object *pile(const char *nom, const char *script)
{
    Object *st = hc_new_stack(nom);
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    hc_new_card(st, bg, "Une");
    hc_set_script(st, script);
    return st;
}

static Object *carte_de(Object *st)
{
    for (int i = 0; i < st->nparts; i++)
        if (st->parts[i]->type == OBJ_CARD) return st->parts[i];
    return NULL;
}

/* Comme Hcdocument.m, envoiePile : à la première carte de la pile. */
static void envoie(Object *st, const char *msg)
{
    printf("-> %s à « %s »\n", msg, st->name);
    hc_set_current_card(carte_de(st));
    hc_send(carte_de(st), msg);
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ligne;
    hc_set_host(&h);

    puts("== 1. la forme d'Apple : start using, puis pass openStack ==");
    Object *aide = pile("Aide",
        "on openStack\n"
        "  global gMenu\n"
        "  put \"Reference\" into gMenu\n"
        "  if there is not a menu gMenu then\n"
        "    create menu gMenu\n"
        "    put \"Un,Deux,Trois,Quatre\" into menu gMenu\n"
        "  end if\n"
        "  send \"coche true\" to this stack\n"
        "  pass openStack\n"
        "end openStack\n"
        "on closeStack\n"
        "  send \"coche false\" to this stack\n"
        "  pass closeStack\n"
        "end closeStack\n");
    Object *ref = pile("Ref",
        "on openStack\n"
        "  start using stack \"Aide\"\n"
        "  pass openStack\n"
        "end openStack\n"
        "on coche vraiOuFaux\n"
        "  global gMenu\n"
        "  set the checkMark of menuItem 4 of menu gMenu to vraiOuFaux\n"
        "  put \"coche \" & vraiOuFaux & \" -> \" & the checkMark of menuItem 4 of menu gMenu\n"
        "end coche\n");
    envoie(ref, "openStack");
    hc_do("put \"menus : \" & the menus");
    envoie(ref, "closeStack");
    hc_do("stop using stack \"Aide\"");
    hc_do("delete menu \"Reference\"");

    puts("\n== 2. une fonction aussi ==");
    Object *biblio = pile("Biblio",
        "function dernier delim, t\n"
        "  set the itemDelimiter to delim\n"
        "  return last item of t\n"
        "end dernier\n");
    Object *appel = pile("Appel",
        "function dernier\n"
        "  start using stack \"Biblio\"\n"
        "  pass dernier\n"
        "end dernier\n"
        "on essai\n"
        "  put \"dernier : [\" & dernier(\":\", \"a:b:c\") & \"]\"\n"
        "end essai\n");
    envoie(appel, "essai");
    hc_do("stop using stack \"Biblio\"");

    /* Trois bibliothèques qui disent leur nom et passent. */
    Object *l1 = pile("L1", "on qui\n  put \"L1\"\n  pass qui\nend qui\n");
    Object *l2 = pile("L2",
        "on qui\n  put \"L2\"\n  start using stack \"L3\"\n  pass qui\nend qui\n");
    Object *l3 = pile("L3", "on qui\n  put \"L3\"\n  pass qui\nend qui\n");
    Object *ici = pile("Ici", "on qui\n  put \"Ici\"\n  pass qui\nend qui\n");

    puts("\n== 3. une pile mise en usage AVANT celle qui passe (non mesuré) ==");
    hc_set_current_card(carte_de(ici));
    hc_do("start using stack \"L1\"");
    hc_do("start using stack \"L2\"");
    envoie(ici, "qui");
    hc_do("put \"en usage : \" & the stacksInUse");
    hc_do("stop using stack \"L1\"");
    hc_do("stop using stack \"L2\"");
    hc_do("stop using stack \"L3\"");

    puts("\n== 4. une pile qui se retire pendant son gestionnaire (non mesuré) ==");
    hc_set_script(l2, "on qui\n  put \"L2\"\n  stop using stack \"L2\"\n  pass qui\nend qui\n");
    hc_do("start using stack \"L1\"");
    hc_do("start using stack \"L3\"");
    hc_do("start using stack \"L2\"");
    envoie(ici, "qui");
    hc_do("stop using stack \"L1\"");
    hc_do("stop using stack \"L3\"");

    puts("\n== 5. deux piles qui se redéclarent : chacune une fois (non mesuré) ==");
    hc_set_script(l1, "on qui\n  put \"L1\"\n  start using stack \"L1\"\n  pass qui\nend qui\n");
    hc_set_script(l2, "on qui\n  put \"L2\"\n  start using stack \"L2\"\n  pass qui\nend qui\n");
    hc_do("start using stack \"L1\"");
    hc_do("start using stack \"L2\"");
    envoie(ici, "qui");

    Object *toutes[] = { aide, ref, biblio, appel, l1, l2, l3, ici };
    for (size_t i = 0; i < sizeof toutes / sizeof *toutes; i++) {
        hc_unregister_stack(toutes[i]);
        hc_free(toutes[i]);
    }
    return 0;
}
