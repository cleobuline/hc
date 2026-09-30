/* « Script » ouvre le BON script, à la BONNE ligne — dans tous les cas.
 *
 * Demandé à l'usage : « le plus important, c'est que l'user voie exactement
 * l'endroit du script qui fâche ». Le noyau donnait déjà au dialogue un objet
 * et une ligne, mais par DEUX chemins : l'objet venait de la première ligne
 * d'erreur accumulée, quelle qu'elle soit ; la ligne, de la première erreur
 * d'exécution. Mesuré sur les seize cas ci-dessous, cinq désignaient un
 * mauvais endroit :
 *
 *   7   le bouton échoue ligne 3, mais le script de la CARTE, lu en chemin,
 *       porte une faute dans un AUTRE gestionnaire, jamais exécuté
 *       -> la carte, ligne 3, sous le titre de la faute de la carte :
 *          un script innocent accusé, à une ligne où il n'y a rien
 *   10  une ligne tapée dans la boîte de message -> le script de la carte
 *   14  un message que personne ne sert -> ligne 0, l'éditeur en haut
 *   15  un « end » manquant -> ligne 0, et le gestionnaire ne faisait RIEN,
 *       écarté sans un mot
 *   17-19 les fautes dites par une COMMANDE (hide, set…) -> ligne 0
 *
 * Chaque ligne imprime ce que le rappel « erreur » reçoit : l'objet, la ligne,
 * et la première ligne du message. La DERNIÈRE ligne du texte reçu est la
 * ligne source, telle qu'écrite : le dialogue la montre, pour qu'on la
 * reconnaisse au lieu de compter.
 *
 * NON MESURÉ, ET INSCRIT ICI POUR QU'ON LE VOIE BOUGER : après une faute dite
 * par une commande (17-19), le gestionnaire CONTINUE — « après » s'affiche.
 * HyperCard s'arrête-t-il sur « Can't understand » ? Probablement ; la
 * question se pose DANS HYPERCARD, pas ici. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("      [msg] %s\n", t);
}

static void erreur(const char *texte, Object *o, int l)
{
    char q[128] = "(aucun)";
    if (o) hc_describe(o, q, sizeof q);
    const char *e = strchr(texte, '\n');
    int n = e ? (int)(e - texte) : (int)strlen(texte);
    const char *src = strrchr(texte, '\n');
    printf("      -> objet %s, ligne %d |%.*s\n", q, l, n, texte);
    if (src && strncmp(src + 1, "ligne ", 6) == 0) printf("         %s\n", src + 1);
}

static Object *st, *c1, *b;

static void cas(const char *titre, const char *sb, const char *sc,
                const char *ss, const char *msg)
{
    printf("== %s\n", titre);
    hc_set_script(b, sb ? sb : "");
    hc_set_script(c1, sc ? sc : "");
    hc_set_script(st, ss ? ss : "");
    hc_set_current_card(c1);
    if (msg) hc_do(msg); else hc_send(b, "mouseUp");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ligne;
    h.erreur = erreur;
    hc_set_host(&h);
    st = hc_new_stack("P");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    c1 = hc_new_card(st, bg, "Un");
    hc_new_card(st, bg, "Deux");
    b = hc_new_button(c1, "B");

    cas("1 exécution, ligne 3 du bouton","on mouseUp\n  put 1 into x\n  put the zorglub of me\nend mouseUp\n",0,0,0);
    cas("2 syntaxe, ligne 4 du bouton","on mouseUp\n  put 1 into x\n  put 2 into y\n  put 3 into z zz\nend mouseUp\n",0,0,0);
    cas("3 gestionnaire de la carte fautif, ligne 4 de la carte","on mouseUp\n  foo\nend mouseUp\n","-- carte\non foo\n  put 1 into x\n  put the zorglub of me\nend foo\n",0,0);
    cas("4 fonction de la pile fautive, ligne 3 de la pile","on mouseUp\n  put f() into x\n  put \"apres\" into msg\nend mouseUp\n",0,"function f\n  put 1 into y\n  return the zorglub of me\nend f\n",0);
    cas("5 do, ligne 3 du bouton","on mouseUp\n  put 1 into x\n  do \"put the zorglub of me\"\nend mouseUp\n",0,0,0);
    cas("6 send à la carte, fautif ligne 3 de la carte","on mouseUp\n  send \"foo\" to card \"Un\"\nend mouseUp\n","on foo\n  put 1 into x\n  put the zorglub of me\nend foo\n",0,0);
    cas("7 autre gestionnaire fautif dans la carte, erreur ligne 3 du bouton","on mouseUp\n  bar\n  put the zorglub of me\nend mouseUp\n","on foo\n  put 1 into x zz\nend foo\non bar\n  put 1 into y\nend bar\n",0,0);
    cas("8 gestionnaire de carte à syntaxe fautive appelé, ligne 3 de la carte","on mouseUp\n  foo\nend mouseUp\n","on foo\n  put 1 into x\n  put 2 into y zz\nend foo\n",0,0);
    cas("9 continuation ¬, ligne 2 (début) ou 3","on mouseUp\n  put \"a\" & ¬\n    the zorglub of me into x\nend mouseUp\n",0,0,0);
    cas("10 boîte de message",0,0,0,"put the zorglub of this card");
    cas("11 imbriqué, ligne 5","on mouseUp\n  repeat 2\n    if true then\n      put 1 into x\n      put the zorglub of me\n    end if\n  end repeat\nend mouseUp\n",0,0,0);
    cas("12 value(), ligne 2","on mouseUp\n  put value(\"the zorglub of me\") into x\nend mouseUp\n",0,0,0);
    cas("13 objet introuvable, ligne 2","on mouseUp\n  put field \"Absent\" into x\nend mouseUp\n",0,0,0);
    cas("14 message inconnu, ligne 3","on mouseUp\n  put 1 into x\n  zorglub 3\nend mouseUp\n",0,0,0);
    cas("15 end manquant (cadre fautif)","on mouseUp\n  put 1 into x\n",0,0,0);
    cas("16 script de carte avec faute de LEXIQUE hors gestionnaire, erreur ligne 2 du bouton","on mouseUp\n  put the zorglub of me\nend mouseUp\n","∞∞∞ bannière\non foo\nend foo\n",0,0);
    cas("17 une commande fautive : hide, ligne 3",
        "on mouseUp\n  put 1 into x\n  hide button \"Absent\"\n  put \"après\"\nend mouseUp\n", 0, 0, 0);
    cas("18 une commande fautive : set, ligne 3",
        "on mouseUp\n  put 1 into x\n  set the zorglub of me to 3\n  put \"après\"\nend mouseUp\n", 0, 0, 0);
    cas("19 une commande fautive : delete, ligne 2",
        "on mouseUp\n  delete field \"Absent\"\nend mouseUp\n", 0, 0, 0);
    cas("20 end manquant, suivi d'un autre gestionnaire : l'en-tête, ligne 1",
        "on mouseUp\n  put 1 into x\non autre\n  beep\nend autre\n", 0, 0, 0);

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
