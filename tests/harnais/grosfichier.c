/* Les entrées-sorties fichier au-delà du tampon de 64 Ko, et les positions.
 *
 * Trois défauts d'un même genre, tous silencieux :
 *
 *  — « write <champ> to file » recopiait la valeur dans un tampon HC_VAL
 *    AVANT d'écrire. Un champ de 200 000 octets en posait 65 535 sur le
 *    disque, et le contrôle d'erreur d'écriture — qui regarde fwrite et
 *    fflush — annonçait un succès parfait, puisque l'écriture s'était bien
 *    passée. La perte avait eu lieu avant de toucher le disque.
 *
 *  — « read » écrivait dans le même tampon et comparait la longueur lue à
 *    son bord pour décider de la troncature. Un fichier de très exactement
 *    65 535 octets, lu en entier sans rien perdre, se voyait annoncer
 *    « Value too large ».
 *
 *  — « at <pos> » passait par atol, qui ne dit jamais si son texte était un
 *    nombre, et le fseek qui suivait n'était pas relu. « read … at canard »
 *    lisait donc depuis le début, et une position refusée laissait écrire à
 *    l'endroit courant du fichier. */
#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define F1 "/tmp/hc_gros1.txt"
#define F2 "/tmp/hc_gros2.txt"

static void ma_ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG) printf("   %s\n", t);
    else if (k == HC_ERR) printf("   [ERR] %s\n", t);
}

static Object *b, *champ;

static void essai(const char *titre, const char *corps)
{
    char s[2048];
    snprintf(s, sizeof s, "on t\n  %s\nend t\n", corps);
    printf("── %s\n", titre);
    hc_set_script(b, s);
    hc_send(b, "t");
    printf("\n");
}

/* La taille réelle du fichier, vue du dehors — c'est elle qui tranche, pas
 * ce que le script croit avoir écrit. */
static long taille(const char *nom)
{
    FILE *f = fopen(nom, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

/* Un fichier de n octets exactement, rempli d'un motif reconnaissable. */
static void fabrique(const char *nom, size_t n)
{
    FILE *f = fopen(nom, "wb");
    if (!f) { printf("   [ERR] impossible de créer %s\n", nom); return; }
    for (size_t i = 0; i < n; i++) fputc('a' + (int)(i % 26), f);
    fclose(f);
}

int main(void)
{
    remove(F1); remove(F2);
    static HcHost h; memset(&h, 0, sizeof h); h.line = ma_ligne; hc_set_host(&h);
    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "Une");
    champ = hc_new_field(c, "gros");
    b     = hc_new_button(c, "B");
    hc_set_current_card(c);

    /* 200 000 octets : trois fois l'ancien tampon, pour que la coupure se
     * voie sans ambiguïté. Le champ est rempli depuis C, pas par un script :
     * ce qu'on mesure ici est l'écriture, pas la construction. */
    size_t n = 200000;
    char *gros = malloc(n + 1);
    for (size_t i = 0; i < n; i++) gros[i] = 'a' + (int)(i % 26);
    gros[n] = '\0';
    hc_set_field_text(champ, gros);
    printf("── le champ porte %zu octets\n\n", strlen(hc_field_text(champ)));

    essai("write d'un champ de 200 000 octets",
        "open file \"" F1 "\"\n"
        "  write card field \"gros\" to file \"" F1 "\"\n"
        "  put \"the result = [\" & the result & \"]\"\n"
        "  close file \"" F1 "\"");
    printf("   le fichier fait %ld octets\n\n", taille(F1));

    essai("relecture du même fichier en entier",
        "open file \"" F1 "\"\n"
        "  read from file \"" F1 "\"\n"
        "  put \"the result = [\" & the result & \"]\"\n"
        "  put \"lu : \" & the length of it\n"
        "  put \"début : \" & char 1 to 6 of it\n"
        "  close file \"" F1 "\"");

    /* Le cas de bord exact : 65 535 octets, l'ancien plafond moins l'octet
     * nul. Rien n'est perdu, donc rien ne doit être annoncé. */
    fabrique(F2, 65535);
    essai("read for 65535 sur un fichier de 65535 octets",
        "open file \"" F2 "\"\n"
        "  read from file \"" F2 "\" for 65535\n"
        "  put \"the result = [\" & the result & \"]\"\n"
        "  put \"lu : \" & the length of it\n"
        "  close file \"" F2 "\"");

    essai("read sans « for » sur le même fichier",
        "open file \"" F2 "\"\n"
        "  read from file \"" F2 "\"\n"
        "  put \"the result = [\" & the result & \"]\"\n"
        "  put \"lu : \" & the length of it\n"
        "  close file \"" F2 "\"");

    essai("read ... at <mot qui n'est pas un nombre>",
        "open file \"" F2 "\"\n"
        "  read from file \"" F2 "\" at canard for 3\n"
        "  put \"the result = [\" & the result & \"]\"\n"
        "  put \"it = [\" & it & \"]\"\n"
        "  close file \"" F2 "\"");

    essai("read ... for <mot qui n'est pas un nombre>",
        "open file \"" F2 "\"\n"
        "  read from file \"" F2 "\" for canard\n"
        "  put \"the result = [\" & the result & \"]\"\n"
        "  close file \"" F2 "\"");

    essai("read ... at <position démesurée>",
        "open file \"" F2 "\"\n"
        "  read from file \"" F2 "\" at 1e300 for 3\n"
        "  put \"the result = [\" & the result & \"]\"\n"
        "  close file \"" F2 "\"");

    essai("write ... at <mot qui n'est pas un nombre> n'écrit nulle part",
        "open file \"" F2 "\"\n"
        "  write \"XXX\" to file \"" F2 "\" at canard\n"
        "  put \"the result = [\" & the result & \"]\"\n"
        "  close file \"" F2 "\"");
    printf("   le fichier fait toujours %ld octets\n\n", taille(F2));

    essai("read ... at 4 for 3  (la forme normale marche toujours)",
        "open file \"" F2 "\"\n"
        "  read from file \"" F2 "\" at 4 for 3\n"
        "  put \"[\" & it & \"]\"\n"
        "  close file \"" F2 "\"");

    free(gros);
    remove(F1); remove(F2);
    hc_free(st);
    return 0;
}
