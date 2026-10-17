/* forth — le FORTH de MAC•CAM (xcmd/maccam/forth_coeur.c), compilé ici en
 * natif, sous les sanitizers de la suite ; le module, lui, est éprouvé dans
 * HC par le harnais xcmd.
 *
 * L'ORACLE est la pile de l'utilisatrice : 71 cartes gardent leur programme
 * Forth ET les tables que le FORTH d'origine en a tirées. Elle ne se commet
 * pas (CLAUDE.md) ; ce qu'on y a mesuré l'est (docs/mesures/xcmd.txt), et ce
 * harnais le tient :
 *   · la disposition des bits de l'index, pour chaque voisinage — LUE dans
 *     le code du FORTH d'origine, qui en porte les huit tables ; Moore, von
 *     Neumann et Margolus-PH aussi mesurées sur les tables de la pile ;
 *   · VRAI vaut -1 (la carte « Pair ») ;
 *   · { … } hors de ses bornes donne 0 (« Grow flow ») ;
 *   · une faute garde les tables déjà faites (« Fourmis »).
 * Les fautes ont la FORME de l'original, lue dans son code ; le nombre
 * qu'elles portent n'est pas mesuré. Le reste — les boucles, CONSTANT et
 * VARIABLE — est le vocabulaire relevé dans l'XFCN d'origine, dont le
 * COMPORTEMENT n'est pas mesuré : ce harnais tient ce que fait notre FORTH,
 * pas ce que faisait l'autre. */
#include "../../xcmd/maccam/forth_coeur.c"

static ForthSortie s;

/* Le bit de l'index qu'une table recopie, ou -1. */
static int bit_de(const char *t)
{
    for (int b = 0; b < FORTH_BITS; b++) {
        int i = 0;
        while (i < FORTH_TABLE && t[i] - '0' == ((i >> b) & 1)) i++;
        if (i == FORTH_TABLE) return b;
    }
    return -1;
}

static int uns(const char *t)
{
    int n = 0;
    for (int i = 0; i < FORTH_TABLE; i++) n += t[i] == '1';
    return n;
}

/* Une empreinte de la table, pour la tenir sans l'imprimer. */
static unsigned empreinte(const char *t)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < FORTH_TABLE; i++) h = (h ^ (unsigned char)t[i]) * 16777619u;
    return h;
}

static void compile(const char *src)
{
    int r = forth_compile(src, &s);
    printf("   %s\n", src);
    printf("      %s« %s »", r ? "FAUTE " : "rend ", s.rendu);
    for (int k = 0; k < FORTH_PLANS; k++)
        if (s.ecrits & (1 << k))
            printf(" ; pln%d : %d uns, bit %d", k, uns(s.plans[k]), bit_de(s.plans[k]));
    printf("\n");
}

static void disposition(const char *decl)
{
    char nom[FORTH_BITS][16];
    for (int b = 0; b < FORTH_BITS; b++) snprintf(nom[b], sizeof nom[b], "-");
    int voisinage = FORTH_MOORE;
    for (int v = 0; v < V_NOMBRE; v++) {
        char src[200];
        snprintf(src, sizeof src, "%s : R %s >PLN0 ; MAKE-TABLE R", decl,
                 forth_nom_voisin(v));
        if (forth_compile(src, &s) < 0) continue;
        voisinage = s.voisinage;
        int b = bit_de(s.plans[0]);
        if (b >= 0) snprintf(nom[b], sizeof nom[b], "%s", forth_nom_voisin(v));
    }
    printf("   %-11s (major %d) :", *decl ? decl : "(rien)", voisinage);
    for (int b = 0; b < FORTH_BITS; b++) printf(" %d:%s", b, nom[b]);
    printf("\n");
}

int main(void)
{
    puts("== 1. la disposition des bits, LUE dans le FORTH d'origine ==");
    disposition("");
    disposition("N/MOORE");
    disposition("N/VONN");
    disposition("N/MARG-PH");
    disposition("N/MARG-HV");
    disposition("N/MOORE-PH");
    disposition("N/MOORE-HV");
    disposition("N/VONN-PH");
    disposition("N/VONN-HV");

    puts("\n== 2. vrai, faux, et { … } ==");
    compile("0 0 =");                                   /* VRAI = -1 */
    compile(": PAIR = -1 * ; : R NORTH SOUTH PAIR >PLN0 ; MAKE-TABLE R");
    compile(": R 1 0 = NOT >PLN0 ; MAKE-TABLE R");
    compile(": R 1 { 0 1 } >PLN0 ; MAKE-TABLE R");
    compile(": R 5 { 1 1 } >PLN0 ; MAKE-TABLE R");      /* hors de la liste */
    compile(": R CENTER NORTH + { 0 1 0 } >PLN0 ; MAKE-TABLE R");

    puts("\n== 3. les mots de l'utilisatrice passent avant les mots de base ==");
    compile(": CENTER NORTH ; : R CENTER >PLN0 ; MAKE-TABLE R");
    compile(": RAND &CENTER' ; : R RAND >PLN0 ; MAKE-TABLE R");
    compile("( un commentaire\n  sur deux lignes ) : R CENTER >PLN0 ; MAKE-TABLE R");

    puts("\n== 4. la règle de Conway, et deux tables ==");
    compile("0 CONSTANT DEATH 1 CONSTANT BORN : UNCHANGED CENTER ; "
            ": LIFE NORTH EAST SOUTH WEST N.WEST N.EAST S.WEST S.EAST + + + + + + + "
            "{ DEATH DEATH UNCHANGED BORN DEATH DEATH DEATH DEATH DEATH } >PLN0 ; "
            ": ECHO CENTER >PLN1 ; MAKE-TABLE LIFE MAKE-TABLE ECHO");
    printf("      empreinte de pln0 : %08x\n", empreinte(s.plans[0]));
    compile("N/MARG-PH : R CENTER CW CCW OPP + + + { 0 OPP CW CCW 1 } >PLN0 "
            "CENTER' >PLN1 ; MAKE-TABLE R");
    printf("      empreinte de pln0 : %08x\n", empreinte(s.plans[0]));
    compile(": A CENTER >PLN0 NORTH >PLN1 ; : B SOUTH >PLN1 ; MAKE-TABLE A MAKE-TABLE B");

    puts("\n== 5. boucles, constantes, variables ==");
    compile(": R 0 3 0 DO I + LOOP >PLN0 ; MAKE-TABLE R");          /* 0+1+2 = 3 */
    compile(": R 0 BEGIN 1 + DUP 3 = UNTIL >PLN0 ; MAKE-TABLE R");
    compile(": R 0 BEGIN DUP 3 < WHILE 1 + REPEAT >PLN0 ; MAKE-TABLE R");
    compile("VARIABLE V 1 V ! : R V @ >PLN0 ; MAKE-TABLE R");
    compile("-1 CONSTANT M : R M >PLN0 ; MAKE-TABLE R");
    compile(": R CENTER IF NORTH IF 1 ELSE 0 THEN ELSE 0 THEN >PLN0 ; MAKE-TABLE R");
    compile("1 2 3");

    puts("\n== 6. compilé : un mot est cherché quand la définition se fait ==");
    compile(": R FOO >PLN0 ; : FOO 1 ; MAKE-TABLE R");
    compile(": A 1 ; : B A >PLN0 ; : A 0 ; MAKE-TABLE B");      /* B garde l'ancien A */

    puts("\n== 7. les fautes, et une faute garde les tables faites ==");
    compile("ZORGLUB");
    compile(": R IF ; MAKE-TABLE R");
    compile(": R : S ;");
    compile(": R DROP >PLN0 ; MAKE-TABLE R");
    compile("IF");
    compile(": R");
    compile("; MAKE-TABLE R");
    compile("MAKE-TABLE ABSENT");
    compile(": R CENTER >PLN0 ; MAKE-TABLE R avec la propriété que");
    compile("");
    return 0;
}
