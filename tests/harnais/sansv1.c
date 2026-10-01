/* CE QUE L'ANCIEN MOTEUR SERVAIT ENCORE, ET QUE LA V3 SERT DÉSORMAIS SEULE.
 *
 * La v3 rappelait l'ancien moteur d'expressions — term_value, call_function,
 * parse_expr — dès qu'elle ne savait pas faire. Pour le retirer, il fallait
 * savoir ce qu'il servait encore, et la seule façon de le savoir était de le
 * couper : HC_SANS_V1=1. La suite entière, rejouée ainsi, perdait :
 *
 *   · « the number of cards of bg 1 » : 1 au lieu de 2, SANS ERREUR. Et en
 *     cherchant les formes voisines, que la suite n'exerçait pas : tout
 *     comptage qui porte une cible — « cards of this stack », « buttons of
 *     card 2 », « bgs of this stack ».
 *   · la forme « the » de charToNum, numToChar et random ;
 *   · « the maFonction of "ok" », une fonction de la pile sous cette forme ;
 *   · toute fonction du noyau appelée avec un argument de moins ou de trop —
 *     « length() », « sqrt() », « offset("a") », « sqrt(4,9) » —, que la v3
 *     refusait d'un « fonction inconnue : length », faux puisqu'elle existe.
 *     L'ancien moteur, lui, complétait par le vide et ignorait le surplus ;
 *     c'est ce qu'on avait d'abord repris. MESURÉ ENSUITE DANS HYPERCARD
 *     (Basilisk II), le 1er octobre : « length() » et « sqrt(4, 9) » y sont
 *     des erreurs. Les sections 4 et 5 lèvent donc « mauvais nombre
 *     d'arguments », avec et sans l'ancien moteur.
 *
 * Mesurées aussi, et ce sont des défauts de l'ancien moteur lui-même :
 *
 *   · « the number of cards of bg 9 », sur une pile de deux fonds, et « the
 *     number of cards of card 1 » rendaient le TOTAL de la pile ;
 *   · « sqrt("abc") » valait 0, « exp2("x") » valait 1 — là où « "abc" + 1 »
 *     est une faute ;
 *   · « the zorglub of 3 » rendait « zorglub of 3 » en clair ;
 *   · « the charToNum of ("a") » levait une erreur d'analyse, PUIS rendait 0.
 *
 * LE HARNAIS JOUE CHAQUE CAS DEUX FOIS, dans deux processus : l'un avec
 * l'ancien moteur, l'autre sans — le réglage se lit une fois par processus.
 * Il imprime la réponse du premier, et signale toute ligne où le second
 * diffère. Une ligne « SANS V1 : » dans les six premières sections est donc
 * une réponse que la v3 ne sait pas encore donner seule. Dans la dernière,
 * l'écart est VOULU — c'est l'ancien moteur qui se trompe —, et il prouve au
 * passage que le réglage coupe bien quelque chose.
 *
 * Ce que fait HyperCard d'un argument manquant ou en trop, d'une fonction de
 * la pile sous la forme « the », et d'un comptage dont la cible manque, n'est
 * PAS mesuré. */
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static void ligne(HcLineKind k, int d, const char *t)
{ (void)d;
  if      (k == HC_MSG) printf("      %s\n", t ? t : "");
  else if (k == HC_ERR) printf("      [ERR] %s\n", t ? t : ""); }

/* Écrit après chaque cas : le parent compare les cas UN À UN, et non les
 * lignes, pour qu'un cas qui en écrit une de moins ne décale pas tous les
 * suivants. */
#define FIN_DE_CAS '\x01'

static const char *CAS[] = {
    "== 1. un comptage qui porte une cible ==",
    "put the number of cards of bg 1",
    "put the number of cards of bg 2",
    "put the number of cards of background \"Deux\"",
    "put the number of cards in bg 2",
    "put the number of cds of bg 2",
    "put the number of cards of this bg",
    "put the number of marked cards of bg 2",
    "put the number of cards of this stack",
    "put the number of cards of stack \"Pile\"",
    "put the number of cards of bg 2 of this stack",
    "put the number of bgs of this stack",
    "put the number of backgrounds of stack \"Pile\"",
    "put the number of buttons of card 2",
    "put the number of card buttons of card \"C\"",
    "put the number of fields of card 2",
    "put the number of card fields of card 2",
    "put the number of bg fields of card 2",
    "put the number of parts of card 2",
    "put the number of fields of bg 2",
    "put the number of buttons of bg 1",
    "put the number of bg buttons of bg 1",
    "-- sans cible, rien n'a change",
    "put the number of cards",
    "put the number of buttons",
    "put the number of fields",
    "put the number of card 2",
    "-- une cible qui manque, ou qui ne contient pas ce type",
    "put the number of cards of bg 9",
    "put the number of cards of card 1",
    "put the number of card buttons of bg 1",
    "put the number of bgs of card 1",

    "== 2. les fonctions du noyau sous la forme « the » ==",
    "put the charToNum of \"a\"",
    "put the numToChar of 65",
    "put the random of 1",
    "put the sqrt of 16",
    "put the sqrt of \"\"",
    "put the sqrt of \"abc\"",
    "put the charToNum of (\"a\")",
    "put the sqrt of 4 + 1",
    "put the offset of \"a\"",

    "== 3. une fonction de la pile sous la forme « the » ==",
    "put the maF of \"ok\"",
    "put the maF of 3",
    "put the length of maF(\"x\")",
    "put the zorglub of 3",
    "put the zorglub of \"x\"",
    "-- les refus d'a cote gardent leur message",
    "put the zorglub of me",
    "put the textStyle of word 2 of card field \"Absent\"",

    "== 4. un argument de moins ==",
    "put \"[\" & length() & \"]\"",
    "put \"[\" & numToChar() & \"]\"",
    "put \"[\" & charToNum() & \"]\"",
    "put \"[\" & offset() & \"]\"",
    "put \"[\" & offset(\"a\") & \"]\"",
    "put \"[\" & random() & \"]\"",
    "put \"[\" & sqrt() & \"]\"",
    "put \"[\" & value() & \"]\"",
    "put \"[\" & annuity(1) & \"]\"",

    "== 5. un argument de trop ==",
    "put \"[\" & length(1,2) & \"]\"",
    "put \"[\" & charToNum(\"a\",\"b\") & \"]\"",
    "put \"[\" & sqrt(4,9) & \"]\"",

    "== 6. un argument vide, ou qui n'est pas un nombre ==",
    "put \"[\" & sqrt(\"\") & \"]\"",
    "put \"[\" & abs(empty) & \"]\"",
    "put \"[\" & annuity(\"\",\"\") & \"]\"",
    "put \"[\" & sqrt(\"abc\") & \"]\"",
    "put \"[\" & exp2(\"x\") & \"]\"",
    "put \"[\" & numToChar(\"x\") & \"]\"",
    "-- la regle qu'elles suivent, celle de l'arithmetique",
    "put \"[\" & (empty + 1) & \"]\"",
    "put \"[\" & (\"abc\" + 1) & \"]\"",

    "== 7. LES ECARTS VOULUS : la ou l'ancien moteur se trompe ==",
    "-- Si cette section ne montrait aucun ecart, c'est que le reglage ne",
    "-- coupe rien, et les six autres ne prouveraient rien non plus.",
    "-- il repete l'erreur de la v3 :",
    "put the name of card 1 of stack \"PileAbsente\"",
    "-- le texte qu'on lui reconstitue perd sa parenthese fermante :",
    "put the maF of (\"o\" & \"k\")",
    NULL
};

/* Joue tous les cas, dans ce processus, et écrit leurs sorties sur `fd`. */
static void joue(int fd, int sans_v1)
{
    if (sans_v1) setenv("HC_SANS_V1", "1", 1);
    else         unsetenv("HC_SANS_V1");
    if (dup2(fd, STDOUT_FILENO) < 0) _exit(2);
    setvbuf(stdout, NULL, _IOLBF, 0);

    static HcHost h; memset(&h, 0, sizeof h); h.line = ligne;
    hc_set_host(&h);

    /* Deux fonds ENTRELACÉS — A C B D —, pour que le rang dans le fond et
     * le rang dans la pile ne coïncident pour aucune carte au-delà de la
     * première. Voir cartedufond. */
    Object *st = hc_new_stack("Pile");
    Object *f1 = hc_new_background(st, "Un");
    Object *f2 = hc_new_background(st, "Deux");
    Object *A  = hc_new_card(st, f1, "A");
    Object *C  = hc_new_card(st, f2, "C");
    Object *B  = hc_new_card(st, f1, "B");
    Object *D  = hc_new_card(st, f2, "D");
    hc_register_stack(st);
    hc_set_current_card(A);
    Object *b = hc_new_button(A, "B1");
    hc_new_button(A, "B2");
    hc_new_button(C, "C1");
    hc_new_field(C, "CF");
    hc_new_field(f2, "F2a");
    hc_new_field(f2, "F2b");
    hc_new_button(f1, "FB");
    D->marked = 1;
    (void)B;

    for (int i = 0; CAS[i]; i++) {
        if (CAS[i][0] == '=' || CAS[i][0] == '-') {
            printf("%s%s\n%c\n", CAS[i][0] == '=' ? "\n" : "   ", CAS[i],
                   FIN_DE_CAS);
            continue;
        }
        char s[1024];
        printf("   %s\n", CAS[i]);
        snprintf(s, sizeof s,
                 "function maF x\n  return x & \"!\"\nend maF\n"
                 "on t\n  %s\nend t\n", CAS[i]);
        hc_set_script(b, s);
        hc_send(b, "t");
        printf("%c\n", FIN_DE_CAS);
    }
    fflush(stdout);
    hc_free(st);
}

/* Lance joue() dans un processus fils et rend tout ce qu'il a écrit. */
static char *capture(int sans_v1)
{
    int p[2];
    if (pipe(p) < 0) return NULL;
    fflush(stdout);
    pid_t f = fork();
    if (f < 0) return NULL;
    if (f == 0) { close(p[0]); joue(p[1], sans_v1); _exit(0); }
    close(p[1]);
    size_t n = 0, cap = 1 << 16;
    char *t = malloc(cap);
    if (!t) return NULL;
    for (;;) {
        if (n + 4096 > cap) {
            char *u = realloc(t, cap *= 2);
            if (!u) { free(t); return NULL; }
            t = u;
        }
        ssize_t r = read(p[0], t + n, cap - n - 1);
        if (r <= 0) break;
        n += (size_t)r;
    }
    t[n] = '\0';
    close(p[0]);
    int etat;
    waitpid(f, &etat, 0);
    return t;
}

int main(void)
{
    char *avec = capture(0);
    char *sans = capture(1);
    if (!avec || !sans) { puts("capture impossible"); return 1; }

    /* Cas par cas. Les deux processus jouent les mêmes cas dans le même
     * ordre ; on imprime la réponse avec l'ancien moteur, et, si elle diffère,
     * celle sans lui, tout entière, juste en dessous. */
    int ecarts = 0;
    char *sa = avec, *ss = sans;
    while (*sa || *ss) {
        char *fa = strchr(sa, FIN_DE_CAS), *fs = strchr(ss, FIN_DE_CAS);
        size_t la = fa ? (size_t)(fa - sa) : strlen(sa);
        size_t ls = fs ? (size_t)(fs - ss) : strlen(ss);
        fwrite(sa, 1, la, stdout);
        if (la != ls || memcmp(sa, ss, la) != 0) {
            /* La première ligne est l'énoncé du cas, identique des deux
             * côtés : on ne répète que les réponses. */
            const char *r = memchr(ss, '\n', ls);
            r = r ? r + 1 : ss + ls;
            const char *bout = ss + ls;
            while (r < bout) {
                const char *e = memchr(r, '\n', (size_t)(bout - r));
                if (!e) e = bout;
                printf("  SANS V1 :%.*s\n", (int)(e - r), r);
                r = e + 1;
            }
            ecarts++;
        }
        sa += la + (fa ? 2 : 0);          /* le marqueur et son retour */
        ss += ls + (fs ? 2 : 0);
    }
    printf("\n%d cas où la v3 seule ne répond pas comme la v3 secourue par "
           "l'ancien moteur\n", ecarts);
    free(avec);
    free(sans);
    return 0;
}
