/* macbinaire — LES ICÔNES D'UNE PILE, PAR UN FICHIER MACBINARY (.bin).
 *
 * Les icônes d'une pile HyperCard vivent dans sa RESSOURCE, que presque tout
 * transfert hors d'un Mac classique perd : c'est pourquoi elles manquaient à
 * l'import. Une archive MacBinary, faite dans l'émulateur, porte les deux
 * parties du fichier. Voir hc_macbinaire et hc_origine_ressources
 * (hc_origine.c), hc_importe_icones (hc_importe.c).
 *
 * LE VRAI FICHIER A ÉTÉ MESURÉ, MAIS IL N'EST PAS ICI : « Stack Templates »
 * est une pile d'Apple, et le dépôt ne porte aucun fichier d'Apple — voir
 * docs/mesures/pile_origine.txt pour la licence, et pour le relevé fait
 * dessus le 2 octobre (six ICON, une PICT, deux « vers », un XCMD).
 *
 * Ce harnais MONTE donc lui-même une ressource et un MacBinary, avec ses
 * propres dessins. Ce qui le garde d'être d'accord avec lui-même :
 *
 *   · la somme de contrôle de l'en-tête MacBinary II, règle du dehors, que
 *     le lecteur vérifie et que l'écrivain doit satisfaire ;
 *   · les décalages de la carte des ressources, écrits selon Inside
 *     Macintosh et relus par le même lecteur qui a lu le vrai fichier ;
 *   · un dessin, imprimé point par point : un carré et une croix doivent
 *     sortir comme un carré et une croix.
 *
 * Il tient :
 *
 *   1. un MacBinary II reconnu et découpé ; un MacBinary I aussi ;
 *   2. ce qui n'en est pas un, refusé : somme fausse, fichier tronqué, notre
 *      format texte, une pile binaire nue ;
 *   3. les ICON lues, avec leur nom ; les autres types comptés ; une ICON de
 *      mauvaise taille comptée en anomalie et laissée ;
 *   4. une carte des ressources qui sort du fichier, refusée en clair ;
 *   5. les icônes posées dans une pile, et qui survivent à hc_save/hc_load ;
 *   6. chaque troncature et chaque octet de la carte retourné, sans faute
 *      de mémoire — à lire sous ASan.
 */
#include "hc_origine.h"
#include "hc_importe.h"
#include "hc_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned char *o; size_t n, cap; } Tampon;

static void met(Tampon *t, const void *p, size_t n)
{
    if (t->n + n > t->cap) {
        size_t c = t->cap ? t->cap : 256;
        while (c < t->n + n) c *= 2;
        t->o = realloc(t->o, c);
        if (!t->o) { puts("mémoire"); exit(1); }
        t->cap = c;
    }
    memcpy(t->o + t->n, p, n);
    t->n += n;
}
static void met16(Tampon *t, unsigned v) { unsigned char b[2] = { (unsigned char)(v >> 8), (unsigned char)v }; met(t, b, 2); }
static void met32(Tampon *t, unsigned long v)
{ unsigned char b[4] = { (unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v }; met(t, b, 4); }
static void pose16(unsigned char *o, unsigned v) { o[0] = (unsigned char)(v >> 8); o[1] = (unsigned char)v; }
static void pose32(unsigned char *o, unsigned long v)
{ o[0] = (unsigned char)(v >> 24); o[1] = (unsigned char)(v >> 16); o[2] = (unsigned char)(v >> 8); o[3] = (unsigned char)v; }

/* Une ressource à monter : type, numéro, nom (MacRoman), octets. */
typedef struct { const char *type; int id; const char *nom; const unsigned char *o; size_t n; } Res;

/* Monte une ressource selon Inside Macintosh : en-tête, données, carte. Les
 * types sont regroupés dans l'ordre de première apparition. */
static Tampon monte_ressource(const Res *rs, int nr)
{
    Tampon d = {0}, noms = {0};
    unsigned long *dec = calloc((size_t)nr, sizeof *dec);
    long *dnom = calloc((size_t)nr, sizeof *dnom);
    for (int i = 0; i < nr; i++) {
        dec[i] = d.n;
        met32(&d, rs[i].n); met(&d, rs[i].o, rs[i].n);
        if (rs[i].nom) {
            dnom[i] = (long)noms.n;
            unsigned char l = (unsigned char)strlen(rs[i].nom);
            met(&noms, &l, 1); met(&noms, rs[i].nom, l);
        } else dnom[i] = -1;
    }
    /* les types distincts */
    const char *types[16]; int nt = 0;
    for (int i = 0; i < nr; i++) {
        int vu = 0;
        for (int k = 0; k < nt; k++) if (!memcmp(types[k], rs[i].type, 4)) vu = 1;
        if (!vu) types[nt++] = rs[i].type;
    }
    Tampon m = {0};
    unsigned char zero[24] = {0};
    met(&m, zero, 24);                      /* copie de l'en-tête, handle, attributs */
    met16(&m, 28);                          /* liste des types, depuis la carte */
    met16(&m, 0);                           /* liste des noms : posée plus bas */
    size_t debut_types = m.n;
    met16(&m, (unsigned)(nt - 1));
    size_t refs = 2 + (size_t)nt * 8;       /* depuis le début de la liste */
    for (int k = 0; k < nt; k++) {
        int c = 0; for (int i = 0; i < nr; i++) if (!memcmp(rs[i].type, types[k], 4)) c++;
        met(&m, types[k], 4); met16(&m, (unsigned)(c - 1)); met16(&m, (unsigned)refs);
        refs += (size_t)c * 12;
    }
    for (int k = 0; k < nt; k++)
        for (int i = 0; i < nr; i++) {
            if (memcmp(rs[i].type, types[k], 4)) continue;
            met16(&m, (unsigned)(rs[i].id & 0xffff));
            met16(&m, dnom[i] < 0 ? 0xffff : (unsigned)dnom[i]);
            unsigned char a[4] = { 0x20, (unsigned char)(dec[i] >> 16), (unsigned char)(dec[i] >> 8), (unsigned char)dec[i] };
            met(&m, a, 4); met32(&m, 0);
        }
    pose16(m.o + 26, (unsigned)m.n);
    met(&m, noms.o, noms.n);
    (void)debut_types;

    Tampon r = {0};
    unsigned char ent[256] = {0};
    pose32(ent, 256); pose32(ent + 4, 256 + d.n); pose32(ent + 8, d.n); pose32(ent + 12, m.n);
    met(&r, ent, 256);
    met(&r, d.o, d.n);
    met(&r, m.o, m.n);
    free(d.o); free(noms.o); free(m.o); free(dec); free(dnom);
    return r;
}

static unsigned crc16(const unsigned char *o, size_t n)
{
    unsigned c = 0;
    for (size_t i = 0; i < n; i++) {
        c ^= (unsigned)o[i] << 8;
        for (int b = 0; b < 8; b++) c = (c & 0x8000) ? ((c << 1) ^ 0x1021) & 0xffff : (c << 1) & 0xffff;
    }
    return c;
}

/* Un MacBinary : version 2 avec somme, ou 1 sans. */
static Tampon monte_macbinaire(const char *nom, const unsigned char *d, size_t nd,
                               const unsigned char *r, size_t nr, int version)
{
    unsigned char h[128] = {0};
    h[1] = (unsigned char)strlen(nom);
    memcpy(h + 2, nom, strlen(nom));
    memcpy(h + 65, "STAK", 4); memcpy(h + 69, "WILD", 4);
    pose32(h + 83, nd); pose32(h + 87, nr);
    if (version >= 2) { h[122] = 129; h[123] = 129; pose16(h + 124, crc16(h, 124)); }
    Tampon t = {0};
    unsigned char zero[128] = {0};
    met(&t, h, 128);
    met(&t, d, nd); met(&t, zero, (128 - nd % 128) % 128);
    met(&t, r, nr); met(&t, zero, (128 - nr % 128) % 128);
    return t;
}

static void dessine(const unsigned char *b)
{
    for (int y = 0; y < 32; y += 2) {
        printf("      ");
        for (int x = 0; x < 32; x++) {
            int h = (b[y * 4 + x / 8] >> (7 - x % 8)) & 1, l = (b[(y + 1) * 4 + x / 8] >> (7 - x % 8)) & 1;
            fputs(h && l ? "█" : h ? "▀" : l ? "▄" : "·", stdout);
        }
        putchar('\n');
    }
}

static void montre_mb(const char *quoi, const unsigned char *o, size_t n)
{
    HcMacBinaire mb;
    if (!hc_macbinaire(o, n, &mb)) { printf("   %-34s refusé\n", quoi); return; }
    printf("   %-34s MacBinary %d, « %s », %s/%s, %zu + %zu octets\n", quoi,
           mb.version, mb.nom, mb.type, mb.createur, mb.ndonnees, mb.nressources);
}

int main(void)
{
    /* Deux dessins à nous : un carré creux et une croix. */
    unsigned char carre[128] = {0}, croix[128] = {0};
    for (int y = 4; y < 28; y++)
        for (int x = 4; x < 28; x++)
            if (y == 4 || y == 27 || x == 4 || x == 27)
                carre[y * 4 + x / 8] |= (unsigned char)(0x80 >> (x % 8));
    for (int i = 2; i < 30; i++) {
        croix[i * 4 + i / 8] |= (unsigned char)(0x80 >> (i % 8));
        croix[i * 4 + (31 - i) / 8] |= (unsigned char)(0x80 >> ((31 - i) % 8));
    }
    unsigned char vers[6] = { 1, 0, 0x80, 0, 0, 0 };
    unsigned char court[100] = {0};
    /* « Cr\x8fme » : un é en MacRoman, qui doit sortir en UTF-8. */
    Res rs[] = {
        { "ICON", 2001, "Carr\x8e", carre, 128 },
        { "vers", 1,    NULL,       vers,  6   },
        { "ICON", -16000, NULL,     croix, 128 },
        { "ICON", 2002, "trop court", court, 100 },
        { "PICT", 128,  "image",    court, 100 },
    };
    Tampon r = monte_ressource(rs, 5);
    const unsigned char donnees[] = "\0\0\0\x20STAK ... une pile, ici pour la forme";
    Tampon mb2 = monte_macbinaire("Ma pile", donnees, sizeof donnees, r.o, r.n, 2);
    Tampon mb1 = monte_macbinaire("Ma pile", donnees, sizeof donnees, r.o, r.n, 1);

    puts("== 1. reconnu et découpé ==");
    montre_mb("MacBinary II", mb2.o, mb2.n);
    montre_mb("MacBinary I", mb1.o, mb1.n);
    HcMacBinaire mb;
    if (hc_macbinaire(mb2.o, mb2.n, &mb))
        printf("   les données commencent par « %.4s », la ressource fait %zu octets\n",
               (const char *)mb.donnees + 4, mb.nressources);

    puts("\n== 2. ce qui n'en est pas un ==");
    Tampon faux = mb2; faux.o = malloc(mb2.n); memcpy(faux.o, mb2.o, mb2.n);
    faux.o[124] ^= 1;
    montre_mb("somme de contrôle fausse", faux.o, faux.n);
    montre_mb("tronqué dans la ressource", mb2.o, mb2.n - 200);
    const char *texte = "-- pile HyperCard (format maison)\nformat 2\n"
                        "...........................................................................................................";
    montre_mb("notre format texte", (const unsigned char *)texte, strlen(texte));
    unsigned char nue[160] = { 0, 0, 0, 160, 'S', 'T', 'A', 'K' };
    montre_mb("une pile binaire nue", nue, sizeof nue);
    free(faux.o);

    puts("\n== 3. les ICON de la ressource ==");
    HcOrigRessources res; char pq[160] = "";
    int ok = hc_origine_ressources(mb.ressources, mb.nressources, &res, pq, sizeof pq);
    printf("   lecture : %s%s ; %d icônes, %d autres ressources, %d anomalie(s)\n",
           ok == 0 ? "ok" : "refusée — ", ok == 0 ? "" : pq,
           res.nicones, res.autres, res.anomalies);
    for (int i = 0; i < res.nicones; i++) {
        printf("   ICON %d « %s »\n", res.icones[i].id, res.icones[i].nom);
        dessine(res.icones[i].bits);
    }

    puts("\n== 4. une carte qui sort du fichier ==");
    {
        unsigned char *abime = malloc(r.n); memcpy(abime, r.o, r.n);
        pose32(abime + 4, (unsigned long)r.n + 10);
        HcOrigRessources x; char m[160] = "";
        int rr = hc_origine_ressources(abime, r.n, &x, m, sizeof m);
        printf("   %s : %s\n", rr == 0 ? "lue" : "refusée", m);
        hc_origine_ressources_libere(&x);
        free(abime);
    }

    puts("\n== 5. dans une pile, et après enregistrement ==");
    Object *st = hc_new_stack("Pile");
    hc_new_card(st, hc_new_background(st, "Fond"), "Une");
    printf("   icônes posées : %d\n", hc_importe_icones(&res, st));
    const char *chemin = "/tmp/hc_macbinaire_essai.stack";
    if (hc_save(st, chemin) != 0) puts("   enregistrement impossible");
    Object *relue = hc_load(chemin);
    remove(chemin);
    int ids[] = { 2001, -16000, 2002 };
    for (int k = 0; k < 3; k++) {
        struct StackIcon *a = hc_icon_get(st, ids[k]);
        struct StackIcon *b = relue ? hc_icon_get(relue, ids[k]) : NULL;
        printf("   icon %6d : %s, %s, nom « %s »\n", ids[k],
               a ? "posée" : "absente",
               !a ? "-" : !b ? "PERDUE à la relecture" :
               memcmp(a->bits, b->bits, 128) ? "DESSIN CHANGÉ à la relecture" : "intacte à la relecture",
               b && b->name ? b->name : "");
    }
    hc_free(st);
    if (relue) hc_free(relue);
    hc_origine_ressources_libere(&res);

    puts("\n== 6. troncatures et octets retournés ==");
    long lus = 0, refus = 0;
    for (size_t n = 0; n <= mb2.n; n++) {
        HcMacBinaire m2;
        if (!hc_macbinaire(mb2.o, n, &m2)) continue;
        HcOrigRessources x; char m[80];
        if (hc_origine_ressources(m2.ressources, m2.nressources, &x, m, sizeof m) == 0) lus++; else refus++;
        hc_origine_ressources_libere(&x);
    }
    printf("   troncatures du fichier : %ld lues, %ld refusées, aucune faute\n", lus, refus);
    lus = refus = 0;
    unsigned char *v = malloc(r.n);
    for (size_t i = 0; i < r.n; i++)
        for (int b = 0; b < 8; b++) {
            memcpy(v, r.o, r.n);
            v[i] ^= (unsigned char)(1u << b);
            HcOrigRessources x; char m[80];
            if (hc_origine_ressources(v, r.n, &x, m, sizeof m) == 0) lus++; else refus++;
            hc_origine_ressources_libere(&x);
        }
    printf("   bits retournés dans la ressource : %ld lus, %ld refusés, aucune faute\n", lus, refus);
    free(v);

    free(r.o); free(mb2.o); free(mb1.o);
    return 0;
}
