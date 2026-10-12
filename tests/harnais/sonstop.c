/* sonstop — « play stop » COUPE LE SON ; « play "stop" » ne le coupe pas.
 *
 * HC cherchait un son nommé « stop », et l'application bipait : c'était dans
 * le « connu, non corrigé » des notes de version depuis 0.7.9.3.
 *
 * MESURÉ DANS HYPERCARD 2.4.1 (Basilisk II) par l'utilisatrice, le 12
 * octobre, sur une mélodie de clavecin (« play "harpsichord" tempo 100 … ») :
 *
 *   · « play stop » l'arrête net ;
 *   · « play "stop" », entre guillemets, la laisse jouer jusqu'au bout, et
 *     « the sound » répond ensuite « harpsichord ».
 *
 * C'est donc le MOT NU qui coupe, pas la valeur. Le harnais tient la place de
 * l'hôte et dit lequel de ses deux crochets est appelé.
 *
 * NON MESURÉ : une variable nommée « stop » (section 3) — HC prend le mot,
 * comme un mot-clé ; ce que répond « the sound » juste après « play stop »
 * — c'est l'hôte qui le dit, et sans hôte rien ne joue (section 4). */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static char g_vu[256];
static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if      (k == HC_ERR) printf("   [ERR] %s\n", t);
    else if (k == HC_MSG) printf("%s\n", t ? t : "");
}
static void joue(const char *nom) { snprintf(g_vu, sizeof g_vu, "joue « %s »", nom ? nom : ""); }
static void coupe(void)           { snprintf(g_vu, sizeof g_vu, "COUPE"); }

static Object *g_carte;
static void cas(const char *quoi, const char *msg)
{
    strcpy(g_vu, "(rien)");
    hc_send(g_carte, msg);
    printf("   %-40s -> %s\n", quoi, g_vu);
}
static void ligne(const char *l)
{
    strcpy(g_vu, "(rien)");
    hc_do(l);
    printf("   %-40s -> %s\n", l, g_vu);
}

int main(void)
{
    static HcHost h; memset(&h, 0, sizeof h);
    h.line = ma_ligne; h.play_sound = joue; h.stop_sound = coupe;
    hc_set_host(&h);
    Object *st = hc_new_stack("Sons");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "Fond");
    g_carte = hc_new_card(st, bg, "Carte");
    hc_set_current_card(g_carte);
    hc_set_script(st,
        "on motNu\n  play stop\nend motNu\n"
        "on majuscules\n  play STOP\nend majuscules\n"
        "on guillemets\n  play \"stop\"\nend guillemets\n"
        "on variable\n  put \"stop\" into s\n  play s\nend variable\n"
        "on masque\n  put \"boing\" into stop\n  play stop\nend masque\n"
        "on leResultat\n  play stop\n  put the result into r\n"
        "  put \"[\" & r & \"]\" into card field 1\nend leResultat\n");
    hc_new_field(g_carte, "R");

    puts("== 1. le mot nu coupe, quelle que soit sa casse ==");
    cas("play stop", "motNu");
    cas("play STOP", "majuscules");
    ligne("play stop");

    puts("\n== 2. la valeur « stop » ne coupe pas : elle se joue (mesuré) ==");
    cas("play \"stop\"", "guillemets");
    cas("put \"stop\" into s / play s", "variable");

    puts("\n== 3. une variable nommée stop : le mot l'emporte (NON MESURÉ) ==");
    cas("put \"boing\" into stop / play stop", "masque");

    puts("\n== 4. sans hôte qui coupe : pas d'erreur, et the result vide ==");
    h.stop_sound = NULL;
    cas("play stop", "motNu");
    hc_send(g_carte, "leResultat");
    hc_do("put \"   the result : \" & card field 1");
    hc_do("put \"   the sound : \" & the sound");

    puts("\n== 5. les autres sons ne bougent pas ==");
    h.stop_sound = coupe;
    ligne("play \"boing\"");
    ligne("play harpsichord tempo 100 \"c d e\"");

    hc_free(st);
    return 0;
}
