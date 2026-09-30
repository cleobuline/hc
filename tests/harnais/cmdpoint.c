/* Cmd-. ARRÊTE ce qui tourne — tout ce qui tourne, et rien d'autre.
 *
 * Il n'y avait pas d'arrêt : un « repeat forever » tournait jusqu'au plafond
 * de dix millions de tours, ou jusqu'à ce qu'on quitte l'application.
 *
 * L'hôte d'ici joue l'utilisatrice : son rappel `idle` — le seul moment où
 * une vraie interface a la main pendant un script — appelle hc_interrompre()
 * au Nième passage. On imprime ce que le script a FAIT : jusqu'où il est
 * allé, ce qu'il a écrit, ce que le dialogue d'erreur a reçu. Un arrêt qui
 * laisserait l'appelant finir sa ligne se verrait au texte d'un champ, pas à
 * un message.
 *
 * Ce que fait HyperCard n'est PAS mesuré ici : qu'il s'arrête sans dialogue,
 * qu'il arrête aussi les appelants. C'est l'hypothèse ; voir la note. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static int n_idle = 0, arreter_a = 0;

static void mon_idle(void)
{
    n_idle++;
    if (arreter_a && n_idle == arreter_a) {
        printf("      [Cmd-. au passage %d]\n", n_idle);
        hc_interrompre();
    }
}

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("      msg : %s\n", t);
}

static void mon_erreur(const char *texte, Object *o, int ligne)
{
    (void)o;
    /* Le texte complet porte l'extrait de script : la première ligne dit la
     * faute, c'est elle qu'on compare. */
    char t[200];
    snprintf(t, sizeof t, "%s", texte);
    char *nl = strchr(t, '\n');
    if (nl) *nl = '\0';
    printf("      DIALOGUE : %s (ligne %d)\n", t, ligne);
}

static void mon_global(const char *nom, const char *val)
{
    if (!strcmp(nom, "lockScreen")) printf("      lockScreen %s\n", val);
}

static Object *st, *b, *champ;

static void essai(const char *titre, int passage, const char *script)
{
    printf("── %s\n", titre);
    n_idle = 0;
    arreter_a = passage;
    hc_set_script(b, script);
    hc_send(b, "essai");
    printf("      champ « %s », en cours %d, arrêt tenu %d\n",
           hc_field_text(champ), hc_is_running(), hc_interrompu());
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ma_ligne;
    h.idle = mon_idle;
    h.erreur = mon_erreur;
    h.global_set = mon_global;
    hc_set_host(&h);

    st = hc_new_stack("P");
    hc_register_stack(st);
    Object *bg = hc_new_background(st, "F");
    Object *u = hc_new_card(st, bg, "U");
    b = hc_new_button(bg, "B");
    champ = hc_new_field(u, "S");
    hc_set_current_card(u);

    puts("== 1. la boucle s'arrête, et la suite du gestionnaire ne tourne pas ==");
    essai("repeat forever, avec un corps", 3,
          "on essai\n  put \"avant\" into field \"S\"\n"
          "  repeat forever\n    add 1 to n\n  end repeat\n"
          "  put \"après\" into field \"S\"\nend essai\n");
    essai("repeat forever, SANS corps", 3,
          "on essai\n  repeat forever\n  end repeat\n"
          "  put \"après\" into field \"S\"\nend essai\n");
    essai("repeat with, et repeat while", 2,
          "on essai\n  repeat with i = 1 to 100000000\n  end repeat\n"
          "  if i > 1 and i < 100000000 then put \"coupé en route\" into field \"S\"\n"
          "  repeat while true\n  end repeat\n"
          "  put \"après\" into field \"S\"\nend essai\n");

    puts("\n== 2. les APPELANTS s'arrêtent aussi ==");
    essai("un message : la ligne d'après l'appel ne tourne pas", 3,
          "on essai\n  put \"avant\" into field \"S\"\n  tourne\n"
          "  put \"après l'appel\" into field \"S\"\nend essai\n"
          "on tourne\n  repeat forever\n  end repeat\nend tourne\n");
    essai("une FONCTION : la ligne qui l'appelle n'écrit pas son vide", 3,
          "on essai\n  put \"intact\" into field \"S\"\n"
          "  put total() into field \"S\"\nend essai\n"
          "function total\n  repeat forever\n  end repeat\n  return 42\nend total\n");

    puts("\n== 3. les attentes ==");
    {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        essai("wait 10 seconds", 5,
              "on essai\n  wait 10 seconds\n  put \"après\" into field \"S\"\nend essai\n");
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double s = (double)(t1.tv_sec - t0.tv_sec)
                 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
        printf("      rendu en moins d'une seconde : %s\n", s < 1.0 ? "oui" : "NON");
    }
    essai("wait until false", 5,
          "on essai\n  wait until false\n  put \"après\" into field \"S\"\nend essai\n");

    puts("\n== 4. pas de dialogue pour l'arrêt ; une faute d'AVANT reste dite ==");
    essai("faute d'une commande, puis boucle arrêtée", 3,
          "on essai\n  hide button \"Absent\"\n  repeat forever\n  end repeat\nend essai\n");

    puts("\n== 5. au repos, tout est remis ==");
    essai("lock screen, puis arrêt : l'écran se déverrouille", 3,
          "on essai\n  lock screen\n  repeat forever\n  end repeat\nend essai\n");
    essai("le script suivant tourne normalement", 0,
          "on essai\n  put \"suivant\" into field \"S\"\nend essai\n");
    printf("── Cmd-. hors script, puis un script\n");
    hc_interrompre();
    printf("      arrêt tenu %d\n", hc_interrompu());
    essai("  …qui tourne jusqu'au bout", 0,
          "on essai\n  repeat 3\n    put \"tour\" && the number of chars of field \"S\" into x\n"
          "  end repeat\n  put \"fini\" into field \"S\"\nend essai\n");

    puts("\n== 6. la boîte de messages ==");
    printf("── repeat forever tapé dans la boîte\n");
    n_idle = 0;
    arreter_a = 3;
    hc_do("repeat forever\nend repeat");
    printf("      rendu, arrêt tenu %d\n", hc_interrompu());

    hc_unregister_stack(st);
    hc_free(st);
    return 0;
}
