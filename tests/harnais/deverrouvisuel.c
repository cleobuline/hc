/* « unlock screen with visual effect <effet> » JOUE l'effet.
 *
 * Signalé à l'usage sur le calendrier de Stack Templates : « il n'y a aucun
 * effet de scroll ». updateCalendar reçoit son effet en PARAMÈTRE —
 * « updateCalendar bg field "Year" + 1, "scroll left" » — et finit par un
 * « unlock screen with visual effect » sur ce paramètre. Le noyau
 * déverrouillait et JETAIT la queue de la commande : l'hôte n'entendait
 * jamais parler de l'effet.
 *
 * On imprime, DANS L'ORDRE, ce que l'hôte reçoit : l'effet doit arriver
 * AVANT « lockScreen false », car l'hôte a besoin de l'écran gelé comme image
 * de départ. On imprime aussi ce que devient un « visual » armé pour le
 * prochain « go » : le déverrouillage ne doit pas le consommer.
 *
 * Qu'HyperCard lise la VARIABLE dans « visual effect theEffect » n'est pas
 * mesuré dans Basilisk ; c'est ce que suppose le calendrier d'Apple, qui ne
 * marcherait pas autrement. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_ERR)      printf("      [ERR] %s\n", t);
    else if (k == HC_MSG) printf("      %s\n", t);
}

static void mon_effet(const char *e, const char *v, const char *i)
{
    printf("      effet « %s » vitesse « %s » vers « %s »\n", e, v, i);
}

static void ma_globale(const char *nom, const char *val)
{
    if (strcmp(nom, "lockScreen") == 0) printf("      lockScreen %s\n", val);
}

static Object *b;

static void essai(const char *titre, const char *corps)
{
    char s[1000];
    snprintf(s, sizeof s,
             "on lance\n  essai \"scroll left\"\nend lance\n"
             "on essai p\n%s\nend essai\n", corps);
    printf("── %s\n", titre);
    hc_set_script(b, s);
    hc_send(b, "lance");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    h.visual_effect = mon_effet;
    h.global_set = ma_globale;
    hc_set_host(&h);

    Object *st = hc_new_stack("P");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *u = hc_new_card(st, bg, "U");
    hc_new_card(st, bg, "V");
    b = hc_new_button(bg, "B");
    hc_set_current_card(u);

    puts("== 1. l'effet part à l'hôte, AVANT le déverrouillage ==");
    essai("un mot",
          "  lock screen\n  unlock screen with visual dissolve");
    essai("« effect », deux mots, une vitesse",
          "  lock screen\n  unlock screen with visual effect barn door open fast");
    essai("vitesse en deux mots, vers le noir",
          "  lock screen\n  unlock screen with visual effect iris close very slow to black");
    essai("une chaîne",
          "  lock screen\n  unlock screen with visual effect \"scroll right\"");

    puts("\n== 2. le calendrier : l'effet dans une variable ==");
    essai("le paramètre p vaut « scroll left »",
          "  lock screen\n  unlock screen with visual effect p");
    essai("une locale",
          "  put \"wipe up\" into e\n  lock screen\n  unlock screen with visual e slow");
    essai("« visual effect e » hors déverrouillage, puis go",
          "  put \"zoom open\" into e\n  visual effect e\n  go next card");

    puts("\n== 3. sans « with », rien ne part ==");
    essai("unlock screen tout court",
          "  lock screen\n  unlock screen");

    puts("\n== 4. un « visual » armé pour go n'est pas consommé ==");
    essai("visual iris open ; lock ; unlock with wipe left ; go",
          "  visual iris open\n  lock screen\n  unlock screen with visual wipe left\n"
          "  go next card");

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
