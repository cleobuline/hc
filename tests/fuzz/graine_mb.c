/* graine_mb.c — des graines MACBINARY pour le fuzzer : une pile au format
 * d'Apple ET une ressource à icônes, dans un seul fichier.
 *
 *   graine_mb <pile.stak> <préfixe>     écrit <préfixe>.mb1 et <préfixe>.mb2
 *
 * Les deux monteurs sont ceux du harnais macbinaire, repris tels quels par
 * inclusion : une seconde copie du format dériverait de la première, et le
 * fuzzer abîmerait alors des fichiers que la suite ne reconnaît plus. */
#define main macbinaire_main
#include "../harnais/macbinaire.c"
#undef main

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "graine_mb <pile.stak> <prefixe>\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    if (n <= 0) { fclose(f); return 1; }
    unsigned char *d = malloc((size_t)n);
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return 1; }
    fclose(f);

    unsigned char motif[128];
    for (int i = 0; i < 128; i++) motif[i] = (unsigned char)(i * 37);
    Res rs[] = {
        { "ICON", 2001,   "Carr\x8e", motif, 128 },
        { "ICON", -16000, NULL,       motif, 128 },
        { "PICT", 128,    "image",    motif, 100 },
        { "ICON", 7,      "x",        motif, 128 },
    };
    Tampon r = monte_ressource(rs, 4);
    for (int v = 1; v <= 2; v++) {
        Tampon mb = monte_macbinaire("Ma pile.stack", d, (size_t)n, r.o, r.n, v);
        char p[4096];
        snprintf(p, sizeof p, "%s.mb%d", argv[2], v);
        FILE *o = fopen(p, "wb");
        if (!o) { perror(p); return 1; }
        fwrite(mb.o, 1, mb.n, o);
        fclose(o);
        free(mb.o);
    }
    free(r.o);
    free(d);
    return 0;
}
