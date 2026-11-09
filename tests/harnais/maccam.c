/* maccam — la machine de MAC•CAM (xcmd/maccam/maccam_coeur.c), compilée ici
 * en natif, sous les sanitizers de la suite, avec le FORTH qui lui fait ses
 * tables.
 *
 * La machine est TRADUITE du code 68000 de MACCAM, lu et désassemblé
 * (docs/mesures/xcmd.txt) ; rien n'en a été EXÉCUTÉ dans HyperCard pour
 * comparaison. Ce harnais tient ce qu'elle fait sur des règles dont on sait
 * le résultat : le planeur de Conway avance d'une case en diagonale tous les
 * quatre pas et revient à sa place après un tour du tore ; « WEST » fait
 * glisser vers l'est ; « OPP », en Margolus, fait filer une particule en
 * diagonale. */
#include "../../xcmd/maccam/forth_coeur.c"
#include "../../xcmd/maccam/maccam_coeur.c"

#include <stdio.h>

static ForthSortie f;

/* Compile une règle et pose ses tables dans la machine, comme « load » :
 * pln0 → pln7, pln1 → pln6, pln2 → pln5, pln3 → pln4 ; et « major ». */
static void charge(Cam *c, const char *regle)
{
    int r = forth_compile(regle, &f);
    if (r < 0) printf("   FAUTE du FORTH : %s\n", f.rendu);
    for (int k = 0; k < 4; k++)
        if (f.ecrits & (1 << k)) cam_pose_pln(c, 7 - k, f.plans[k]);
    c->major = f.voisinage;
    /* « set the custom of window "CAM" to » le même texte */
    cam_voisins(c, f.voisins.n, f.voisins.dx, f.voisins.dy, f.voisins.plan);
}

/* L'intérieur de la grille, un caractère par cellule : le plan `bit`. */
static void montre(const Cam *c, int bit, const char *titre)
{
    printf("   %s\n", titre);
    for (int r = 1; r <= c->h - 2; r++) {
        printf("      ");
        for (int x = 1; x <= c->w - 2; x++)
            putchar((c->pix[r * c->w + x] >> bit) & 1 ? '#' : '.');
        putchar('\n');
    }
}

static int vivantes(const Cam *c, int bit)
{
    int n = 0;
    for (int r = 1; r <= c->h - 2; r++)
        for (int x = 1; x <= c->w - 2; x++)
            n += (c->pix[r * c->w + x] >> bit) & 1;
    return n;
}

static void allume(Cam *c, int r, int x, int bit)
{
    c->pix[r * c->w + x] |= (unsigned char)(1u << bit);
}

/* L'empreinte de l'INTÉRIEUR : la bordure, elle, porte après un pas les
 * copies du bord opposé. */
static unsigned empreinte(const Cam *c)
{
    unsigned h = 2166136261u;
    for (int r = 1; r <= c->h - 2; r++)
        for (int x = 1; x <= c->w - 2; x++)
            h = (h ^ c->pix[r * c->w + x]) * 16777619u;
    return h;
}

int main(void)
{
    Cam c;

    puts("== 1. les tables : « 0 » éteint, tout autre caractère allume ==");
    cam_ouvre(&c, 8, 8);
    char t[CAM_TABLE + 1];
    cam_pose_pln(&c, 7, "0101x1");
    cam_pose_pln(&c, 4, "1111");
    cam_pose_pln(&c, 3, "1111");                    /* pln3 : rien */
    cam_lit_pln(&c, 7, t);
    printf("   pln7 : %.8s… ; pln4 : ", t);
    cam_lit_pln(&c, 4, t);
    printf("%.8s… ; lutA[0..5] = %02x %02x %02x %02x %02x %02x ; lutB[0] = %02x\n",
           t, c.lutA[0], c.lutA[1], c.lutA[2], c.lutA[3], c.lutA[4], c.lutA[5],
           c.lutB[0]);
    cam_pose_pln(&c, 7, "00");                      /* un texte court */
    cam_lit_pln(&c, 7, t);
    printf("   après « 00 » : %.8s…\n", t);
    cam_ferme(&c);

    puts("\n== 2. Conway, en Moore : le planeur ==");
    cam_ouvre(&c, 12, 12);                          /* un tore de 10 × 10 */
    charge(&c, "0 CONSTANT DEATH 1 CONSTANT BORN : UNCHANGED CENTER ; "
               ": LIFE NORTH EAST SOUTH WEST N.WEST N.EAST S.WEST S.EAST "
               "+ + + + + + + { DEATH DEATH UNCHANGED BORN DEATH DEATH DEATH "
               "DEATH DEATH } >PLN0 ; MAKE-TABLE LIFE");
    allume(&c, 1, 2, 7); allume(&c, 2, 3, 7);
    allume(&c, 3, 1, 7); allume(&c, 3, 2, 7); allume(&c, 3, 3, 7);
    unsigned depart = empreinte(&c);
    montre(&c, 7, "au départ");
    for (int i = 0; i < 4; i++) cam_pas(&c, CAM_SELON_MAJOR);
    montre(&c, 7, "après quatre pas : une case vers le sud-est");
    for (int i = 0; i < 36; i++) cam_pas(&c, CAM_SELON_MAJOR);
    printf("   après quarante pas, un tour du tore : %d cellules, %s\n",
           vivantes(&c, 7), empreinte(&c) == depart ? "revenu à sa place"
                                                    : "AILLEURS");
    cam_ferme(&c);

    puts("\n== 3. von Neumann : WEST >PLN0 fait glisser vers l'est ==");
    cam_ouvre(&c, 10, 6);
    charge(&c, "N/VONN : R WEST >PLN0 ; MAKE-TABLE R");
    allume(&c, 2, 2, 7); allume(&c, 3, 3, 7);
    depart = empreinte(&c);
    cam_pas(&c, CAM_SELON_MAJOR);
    montre(&c, 7, "après un pas");
    for (int i = 0; i < 7; i++) cam_pas(&c, CAM_SELON_MAJOR);
    printf("   après huit pas, la largeur du tore : %s\n",
           empreinte(&c) == depart ? "revenu à sa place" : "AILLEURS");
    cam_ferme(&c);

    puts("\n== 4. Margolus, ALTERNATE : OPP fait filer en diagonale ==");
    cam_ouvre(&c, 10, 10);
    charge(&c, "N/MARG-PH ALTERNATE : R OPP >PLN0 ; MAKE-TABLE R");
    c.alternate = 1;
    allume(&c, 2, 2, 7);
    for (int i = 0; i < 3; i++) {
        cam_pas(&c, CAM_SELON_MAJOR);
        char titre[60];
        snprintf(titre, sizeof titre, "après %d pas (phase A : %d)", i + 1, c.phaseA);
        montre(&c, 7, titre);
    }
    cam_ferme(&c);

    puts("\n== 5. deux demi-machines : CAM-B lit CAM-A comme « & » ==");
    cam_ouvre(&c, 8, 8);
    charge(&c, ": R CENTER >PLN0 &CENTER >PLN2 ; MAKE-TABLE R");
    allume(&c, 3, 3, 7);
    cam_pas(&c, CAM_SELON_MAJOR);
    printf("   après un pas : plan 0 %d cellule(s), plan 2 %d — la copie du plan 0\n",
           vivantes(&c, 7), vivantes(&c, 5));
    cam_ferme(&c);

    puts("\n== 6. fill, erase, image ==");
    cam_ouvre(&c, 6, 6);
    cam_remplit(&c, 7);
    int n = 0;
    for (int i = 0; i < 36; i++) n += c.pix[i] >> 7;
    printf("   fill8 : %d pixels sur 36 — tout sauf la dernière ligne\n", n);
    cam_efface(&c);
    n = 0;
    for (int i = 0; i < 36; i++) n += c.pix[i] != 0;
    printf("   erasea : %d pixel(s) allumé(s)\n", n);
    static const unsigned char croix[9] = { 0, 1, 0, 1, 1, 1, 0, 1, 0 };
    cam_image(&c, 6, croix, 3, 3);
    montre(&c, 6, "image6 d'une croix 3 × 3, posée en haut à gauche — bords compris");
    cam_ferme(&c);

    puts("\n== 7. la palette du moteur moderne ==");
    static const unsigned char px[] = { 0x00, 0x80, 0x40, 0xC0, 0x20, 0x10, 0x30, 0x0F };
    for (size_t i = 0; i < sizeof px; i++) {
        unsigned char rgb[3];
        cam_couleur(px[i], rgb);
        printf("   %02x → %3d %3d %3d\n", px[i], rgb[0], rgb[1], rgb[2]);
    }

    /* L'AJOUT DE HC du 7 novembre, décidé par l'utilisatrice : un bit tiré
     * au sort, RAND, dans les voisinages 8, 9 et 10. La graine est la même
     * à chaque ouverture : les comptes ci-dessous ne bougent pas d'une
     * exécution à l'autre. */
    puts("\n== 8. le hasard : RAND >PLN0, sur 64 × 64 cellules ==");
    static const unsigned long probas[] = { 0, 1000, 250000, 500000, 1000000 };
    for (size_t k = 0; k < sizeof probas / sizeof *probas; k++) {
        cam_ouvre(&c, 66, 66);
        charge(&c, "N/MOORE-RAND : R RAND >PLN0 ; MAKE-TABLE R");
        cam_hasard(&c, probas[k]);
        cam_pas(&c, CAM_SELON_MAJOR);
        printf("   Moore, p = %7lu millionièmes : %4d cellules sur 4096\n",
               probas[k], vivantes(&c, 7));
        cam_ferme(&c);
    }
    cam_ouvre(&c, 66, 66);
    charge(&c, "N/VONN-RAND : R RAND >PLN0 ; MAKE-TABLE R");
    cam_pas(&c, CAM_SELON_MAJOR);
    printf("   von Neumann, p = 50 %% : %d cellules sur 4096\n", vivantes(&c, 7));
    cam_ferme(&c);

    puts("\n== 9. le même tirage pour les deux demi-machines ==");
    cam_ouvre(&c, 66, 66);
    charge(&c, "N/MOORE-RAND : R RAND >PLN0 RAND >PLN2 ; MAKE-TABLE R");
    cam_pas(&c, CAM_SELON_MAJOR);
    n = 0;
    for (int r = 1; r <= 64; r++)
        for (int x = 1; x <= 64; x++) {
            unsigned char p = c.pix[r * c.w + x];
            n += ((p >> 7) & 1) != ((p >> 5) & 1);
        }
    printf("   plan 0 : %d, plan 2 : %d, cellules où ils diffèrent : %d\n",
           vivantes(&c, 7), vivantes(&c, 5), n);
    cam_ferme(&c);

    puts("\n== 10. Margolus : un tirage par bloc ==");
    cam_ouvre(&c, 66, 66);
    charge(&c, "N/MARG-RAND : R RAND >PLN0 ; MAKE-TABLE R");
    cam_pas(&c, CAM_SELON_MAJOR);
    int panaches = 0, blocs = 0;
    for (int r = 1; r < 65; r += 2)
        for (int x = 1; x < 65; x += 2) {
            int s4 = (c.pix[r * c.w + x] >> 7) + (c.pix[r * c.w + x + 1] >> 7)
                   + (c.pix[(r + 1) * c.w + x] >> 7) + (c.pix[(r + 1) * c.w + x + 1] >> 7);
            blocs += s4 == 4;
            panaches += s4 != 0 && s4 != 4;
        }
    printf("   blocs allumés : %d sur 1024 ; blocs panachés : %d\n", blocs, panaches);
    cam_ferme(&c);

    puts("\n== 11. Margolus : un bloc tourné au hasard ne perd rien ==");
    cam_ouvre(&c, 66, 66);
    charge(&c, "N/MARG-RAND ALTERNATE : R RAND IF CW ELSE CCW THEN >PLN0 ; MAKE-TABLE R");
    c.alternate = 1;
    for (int r = 20; r < 40; r++)
        for (int x = 20; x < 40; x++) allume(&c, r, x, 7);
    int avant = vivantes(&c, 7);
    for (int i = 0; i < 200; i++) cam_pas(&c, CAM_SELON_MAJOR);
    printf("   %d particules au départ, %d après 200 pas : la diffusion\n",
           avant, vivantes(&c, 7));
    montre(&c, 7, "après 200 pas : le carré s'est dissous en nuage");
    cam_ferme(&c);

    puts("\n== 12. la même graine à chaque ouverture ==");
    Cam d;
    cam_ouvre(&c, 34, 34);
    cam_ouvre(&d, 34, 34);
    charge(&c, "N/MOORE-RAND : R RAND >PLN0 ; MAKE-TABLE R");
    charge(&d, "N/MOORE-RAND : R RAND >PLN0 ; MAKE-TABLE R");
    cam_pas(&c, CAM_SELON_MAJOR);
    cam_pas(&d, CAM_SELON_MAJOR);
    unsigned premier = empreinte(&c);
    printf("   deux fenêtres ouvertes : %s\n",
           premier == empreinte(&d) ? "le même tirage" : "DES TIRAGES DIFFÉRENTS");
    cam_pas(&c, CAM_SELON_MAJOR);
    printf("   deux pas dans la même : %s\n",
           empreinte(&c) != premier ? "deux tirages différents" : "LE MÊME TIRAGE");
    cam_ferme(&c);
    cam_ferme(&d);

    /* LE COMPTEUR, l'ajout de HC décidé par l'utilisatrice le 8 novembre :
     * les cellules allumées de chaque plan, sur le tore seul. Il doit dire,
     * plan par plan, ce que dit vivantes(), et ne jamais compter la
     * bordure, qui ne porte que des copies. */
    puts("\n== 13. le compteur ==");
    unsigned long nb[4];
    cam_ouvre(&c, 34, 34);
    cam_compte(&c, nb);
    printf("   une fenêtre neuve : %lu,%lu,%lu,%lu\n", nb[0], nb[1], nb[2], nb[3]);
    static const int planeur[5][2] = { { 2, 3 }, { 3, 4 }, { 4, 2 }, { 4, 3 }, { 4, 4 } };
    for (int k = 0; k < 5; k++) allume(&c, planeur[k][0], planeur[k][1], 7);
    for (int x = 5; x <= 11; x++) allume(&c, 10, x, 5);
    allume(&c, 1, 1, 4);                    /* le coin du tore, recopié au bord */
    allume(&c, 1, 1, 7);
    cam_compte(&c, nb);
    printf("   un planeur et un coin au plan 0, sept au plan 2, un coin au plan 3 :"
           " %lu,%lu,%lu,%lu\n", nb[0], nb[1], nb[2], nb[3]);
    charge(&c, "N/MOORE : A CENTER >PLN0 CENTER' >PLN1 ; "
               ": B CENTER >PLN2 CENTER' >PLN3 ; MAKE-TABLE A MAKE-TABLE B");
    cam_pas(&c, CAM_SELON_MAJOR);
    int grille = 0;
    for (int r = 0; r < c.h; r++)
        for (int x = 0; x < c.w; x++) grille += (c.pix[r * c.w + x] >> 7) & 1;
    cam_compte(&c, nb);
    printf("   après un pas qui ne change rien : %d au plan 0 dans toute la grille,"
           " bordure comprise ; le compteur : %lu,%lu,%lu,%lu\n",
           grille, nb[0], nb[1], nb[2], nb[3]);
    cam_ferme(&c);
    cam_ouvre(&c, 66, 66);
    charge(&c, "N/MOORE-RAND : R RAND >PLN0 RAND >PLN3 ; MAKE-TABLE R");
    cam_hasard(&c, 250000);
    cam_pas(&c, CAM_SELON_MAJOR);
    cam_compte(&c, nb);
    printf("   RAND à 25 %% : le compteur %lu,%lu,%lu,%lu ; vivantes() %d,%d,%d,%d\n",
           nb[0], nb[1], nb[2], nb[3], vivantes(&c, 7), vivantes(&c, 6),
           vivantes(&c, 5), vivantes(&c, 4));
    cam_ferme(&c);
    cam_compte(&c, nb);
    printf("   une fenêtre fermée : %lu,%lu,%lu,%lu\n", nb[0], nb[1], nb[2], nb[3]);

    /* LES VOISINS SUR MESURE, l'ajout de HC décidé par l'utilisatrice le 9
     * novembre : le voisinage 11 lit les cellules aux déplacements
     * déclarés par N/CUSTOM, sur le tore — dx > 0 à l'est, dy > 0 au nord,
     * comme son MacCam de bureau. */
    puts("\n== 14. les voisins sur mesure : N/CUSTOM ==");
    cam_ouvre(&c, 12, 8);                           /* un tore de 10 × 6 */
    charge(&c, "N/CUSTOM -3 0 0 END-CUSTOM : R NBR0 >PLN0 ; MAKE-TABLE R");
    printf("   major %d, %d voisin(s)\n", c.major, c.nvoisins);
    allume(&c, 2, 2, 7);
    allume(&c, 4, 9, 7);
    montre(&c, 7, "au départ");
    cam_pas(&c, CAM_SELON_MAJOR);
    montre(&c, 7, "NBR0 à (-3, 0), l'ouest à trois cases : tout file de trois vers l'est,"
                  " le tore compris");
    charge(&c, "N/CUSTOM 0 2 0 END-CUSTOM : R NBR0 >PLN0 ; MAKE-TABLE R");
    cam_pas(&c, CAM_SELON_MAJOR);
    montre(&c, 7, "NBR0 à (0, 2), le nord à deux cases : deux lignes vers le sud");
    charge(&c, "N/CUSTOM 0 -1 0 END-CUSTOM : R NBR0 >PLN0 ; MAKE-TABLE R");
    for (int i = 0; i < 4; i++) cam_pas(&c, CAM_SELON_MAJOR);
    montre(&c, 7, "NBR0 à (0, -1), quatre pas : quatre lignes vers le nord");
    cam_ferme(&c);

    /* Un déplacement plus long que le tore : le reste de la division. */
    cam_ouvre(&c, 12, 8);
    charge(&c, "N/CUSTOM 127 -100 0 END-CUSTOM : R NBR0 >PLN0 ; MAKE-TABLE R");
    allume(&c, 3, 5, 7);
    cam_pas(&c, CAM_SELON_MAJOR);
    montre(&c, 7, "NBR0 à (127, -100) sur 10 × 6 : comme (7, -4), soit (-3, 2)");
    cam_ferme(&c);

    /* Les deux demi-machines, aux mêmes déplacements : CAM-B lit ses plans
     * 2 et 3, et le plan d'un triplet est le second des siens. */
    cam_ouvre(&c, 12, 8);
    charge(&c, "N/CUSTOM 1 0 0 -1 0 1 END-CUSTOM "
               ": A NBR0 >PLN0 ; : B NBR1 >PLN2 ; MAKE-TABLE A MAKE-TABLE B");
    allume(&c, 3, 5, 7);                            /* plan 0 */
    allume(&c, 3, 5, 4);                            /* plan 3 */
    allume(&c, 3, 5, 6);                            /* plan 1 : nul n'y lit */
    cam_pas(&c, CAM_SELON_MAJOR);
    montre(&c, 7, "plan 0 : NBR0 à l'est, la cellule va vers l'ouest");
    montre(&c, 5, "plan 2 : NBR1, le plan 3 à l'ouest, va vers l'est");
    printf("   plan 1 : %d, plan 3 : %d — aucune table ne les écrit\n",
           vivantes(&c, 6), vivantes(&c, 4));
    cam_ferme(&c);

    /* RAND, au bit qui suit le dernier voisin */
    cam_ouvre(&c, 66, 66);
    charge(&c, "N/CUSTOM 0 0 0 5 5 0 0 9 1 END-CUSTOM : R RAND >PLN0 ; MAKE-TABLE R");
    cam_hasard(&c, 250000);
    cam_pas(&c, CAM_SELON_MAJOR);
    printf("   RAND après trois voisins, p = 25 %% : %d cellules sur 4096\n",
           vivantes(&c, 7));
    cam_ferme(&c);

    /* Douze voisins, une table de 8 192 entrées : la règle de Conway, à
     * des voisins nommés, refaite par N/CUSTOM, et le planeur revient. */
    cam_ouvre(&c, 12, 12);
    charge(&c, "N/CUSTOM 0 0 0 C 0 1 0 N 1 0 0 E 0 -1 0 S -1 0 0 W "
               "-1 1 0 NW 1 1 0 NE -1 -1 0 SW 1 -1 0 SE 0 0 1 0 0 1 9 9 0 "
               "END-CUSTOM 0 CONSTANT DEATH 1 CONSTANT BORN : UNCHANGED C ; "
               ": LIFE N E S W NW NE SW SE + + + + + + + "
               "{ DEATH DEATH UNCHANGED BORN DEATH DEATH DEATH DEATH DEATH } >PLN0 ; "
               "MAKE-TABLE LIFE");
    printf("   %d voisins, la table de pln0 : %zu entrées\n", c.nvoisins, strlen(f.plans[0]));
    allume(&c, 1, 2, 7); allume(&c, 2, 3, 7);
    allume(&c, 3, 1, 7); allume(&c, 3, 2, 7); allume(&c, 3, 3, 7);
    depart = empreinte(&c);
    for (int i = 0; i < 40; i++) cam_pas(&c, CAM_SELON_MAJOR);
    printf("   après quarante pas, un tour du tore : %d cellules, %s\n",
           vivantes(&c, 7), empreinte(&c) == depart ? "revenu à sa place" : "AILLEURS");
    cam_ferme(&c);

    /* Sans voisin, un pas du voisinage 11 ne fait rien ; au-delà de douze,
     * les douze premiers. */
    cam_ouvre(&c, 8, 8);
    charge(&c, "N/CUSTOM END-CUSTOM");
    allume(&c, 3, 3, 7);
    depart = empreinte(&c);
    cam_pas(&c, CAM_SELON_MAJOR);
    printf("   sans voisin : major %d, %s\n", c.major,
           empreinte(&c) == depart ? "rien ne change" : "LA GRILLE A CHANGÉ");
    {
        signed char dx[13] = { 0 }, dy[13] = { 0 };
        unsigned char pl[13] = { 0 };
        cam_voisins(&c, 13, dx, dy, pl);
        printf("   treize voisins donnés : %d retenus\n", c.nvoisins);
        cam_voisins(&c, -1, dx, dy, pl);
        printf("   -1 voisin : %d retenu\n", c.nvoisins);
    }
    cam_ferme(&c);
    return 0;
}
