/* forth — le FORTH de MAC•CAM (xcmd/maccam/forth_coeur.c), compilé ici en
 * natif, sous les sanitizers de la suite ; le module, lui, est éprouvé dans
 * HC par le harnais xcmd.
 *
 * TROIS SOURCES, aucune commise (CLAUDE.md) ; ce qu'on y a lu ou mesuré l'est
 * (docs/mesures/xcmd.txt), et ce harnais le tient :
 *   · LE CODE 68000 de l'XFCN d'origine, désassemblé : la disposition des
 *     bits, les cellules de 16 bits, NOT bit à bit, le dictionnaire où un mot
 *     entre dès « : », la recherche qui rend la première entrée, les mots de
 *     base sans effet au premier niveau, ALTERNATE à l'adresse -4, la pile
 *     que MAKE-TABLE ne remet pas, les fautes envoyées et leur rang, la pile
 *     rendue sommet en tête ;
 *   · MAC•CAM WORKSHOP, la pile de l'utilisatrice : 71 cartes gardent leur
 *     programme et les tables que l'original en a tirées — 163 sur 175
 *     refaites à l'identique, les douze autres plus vieilles que lui —, et
 *     le champ Pile, « -4 » sur seize cartes ; VRAI vaut -1 (« Pair ») ;
 *     { … } hors de ses bornes donne 0 (« Grow flow ») ; une faute garde les
 *     tables déjà faites (« Fourmis ») ;
 *   · MAC•CAM FORTH, sa pile de démonstration de 1990 : la récursion, la
 *     pile retour à quatre octets par adresse, le code machine déposé par
 *     « , » ; et ses champs « result », que l'original avait remplis :
 *     Fermat (225 nombres), 11^1024 = -20479, LEAVE, les boucles. */
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
    printf("      rend « %s »", s.rendu);
    if (r) printf(" ; envoie « %s »", s.faute);
    if (s.bips) printf(" ; %d bip(s)", s.bips);
    for (int k = 0; k < FORTH_PLANS; k++)
        if (s.ecrits & (1 << k))
        {
            printf(" ; pln%d : %d uns, bit %d", k, uns(s.plans[k]), bit_de(s.plans[k]));
            if (strlen(s.plans[k]) != FORTH_TABLE)
                printf(" (%zu caractères)", strlen(s.plans[k]));
        }
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
    compile(": T 0 0 = ; T");                           /* VRAI = -1 */
    compile(": PAIR = -1 * ; : R NORTH SOUTH PAIR >PLN0 ; MAKE-TABLE R");
    compile(": R 1 0 = NOT >PLN0 ; MAKE-TABLE R");
    compile(": R 1 { 0 1 } >PLN0 ; MAKE-TABLE R");
    compile(": R 5 { 1 1 } >PLN0 ; MAKE-TABLE R");      /* hors de la liste */
    compile(": R CENTER NORTH + { 0 1 0 } >PLN0 ; MAKE-TABLE R");
    compile(": R CENTER NORTH + >PLN0 ; MAKE-TABLE R"); /* 2 : non nul, donc 1 */

    puts("\n== 3. les mots de l'utilisatrice passent avant les mots de base ==");
    compile(": CENTER NORTH ; : R CENTER >PLN0 ; MAKE-TABLE R");
    compile(": RAND &CENTER' ; : R RAND >PLN0 ; MAKE-TABLE R");
    compile("( un commentaire\n  sur deux lignes ) : R CENTER >PLN0 ; MAKE-TABLE R");
    compile("( un ) ( deux ) : R CENTER >PLN0 ; MAKE-TABLE R");
    compile(": DUP 7 ; : T 1 DUP ; T");

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
    compile("N/MARG-PH ALTERNATE : R CENTER >PLN0 ; MAKE-TABLE R");   /* « -4 » */

    puts("\n== 5. 16 bits, NOT, la pile, les boucles ==");
    compile(": T 300 300 * 32767 1 + 5 NOT ; T");
    compile("70000 -70000");
    compile(": T 7 2 / -7 2 / 7 0 / -32768 -1 / ; T");
    compile(": T 1 2 3 ROT 4 5 SWAP 6 7 OVER ; T");
    compile(": T 1 2 >R 3 R> ; T");
    compile(": T 0 3 0 DO 1 + LOOP 0 0 DO 9 LOOP ; T");      /* 3 tours, puis 0 */
    compile(": T 0 BEGIN 1 + DUP 3 = UNTIL ; T");
    compile(": T 0 BEGIN DUP 3 < WHILE 1 + REPEAT ; T");
    compile("VARIABLE V : S 1 V ! ; S : T V @ V BASE BASE @ ALTERNATE ; T");
    compile("VARIABLE V 1 V ! V @");           /* ! et @ : sans effet ici */
    compile("-1 CONSTANT M : R M >PLN0 ; MAKE-TABLE R");
    compile(": R CENTER IF NORTH IF 1 ELSE 0 THEN ELSE 0 THEN >PLN0 ; MAKE-TABLE R");
    compile("1 2 3 DUP + SWAP");               /* les mots de base : sans effet ici */
    compile(": T 1 2 \xe2\x89\xa0 3 \xe2\x89\xa5 ; T");      /* ≠ et ≥ : rien */

    puts("\n== 6. le dictionnaire : récursion directe, première entrée, casse ==");
    compile(": FACT DUP 2 < IF DROP 1 ELSE DUP 1 - FACT * THEN ; 7 FACT");
    compile(": R FOO >PLN0 ; : FOO 1 ; MAKE-TABLE R");       /* pas d'indirecte */
    compile(": A 1 ; : B A ; : A 0 ; B A");                  /* la première A */
    compile("1000 CONSTANT mille : MILLE mille 2 * ; MILLE mille");
    compile("dup");

    puts("\n== 7. la pile retour, et « , » ==");
    compile(": EXIT 22671 , ; : K BEEP ; : T 1 EXIT K 2 ; T 3");
    compile(": HOP R> R> DROP DROP ; : K BEEP ; : T 1 HOP K 2 ; T 3");
    compile(": index 12315 , -7360 , 14135 , 0 , ; : i 4 index ; : j 6 index ; "
            ": T 3 1 DO 2 0 DO j 10 * i + LOOP LOOP ; T");
    compile(": PICK 12315 , -7360 , 14131 , 0 , ; : T 10 20 30 2 PICK 0 PICK ; T");
    compile(": LEAVE 16239 , 6 , 4 , ; : T 0 10 0 DO 1 + DUP 3 = IF LEAVE THEN LOOP ; T");
    compile(": D -22017 , 5 ; D");                           /* _Debugger : rien */
    compile(": X 17999 , ; X");                              /* hors du jeu */
    compile(": X DUP , ; X");

    puts("\n== 8. MAKE-TABLE ne remet pas la pile ==");
    compile(": R CENTER >PLN0 ; 5 MAKE-TABLE R");
    compile(": R 1 CENTER >PLN0 ; MAKE-TABLE R");
    compile(": R DROP CENTER >PLN0 ; MAKE-TABLE R");

    puts("\n== 9. les fautes : envoyées, et leur rang compte les commentaires ==");
    compile("ZORGLUB");
    compile("( un deux trois ) 1 ZORGLUB");
    compile(": R IF ; MAKE-TABLE R");
    compile(": R : S ;");
    compile(": R CENTER ZORGLUB >PLN0 ;");
    compile("1 ; 2");
    compile(": R");
    compile("MAKE-TABLE ABSENT");
    compile(": R CENTER >PLN0 ; MAKE-TABLE R avec la propriété que");
    compile(": B BEEP ; B B 2 B");
    compile("");
    return 0;
}
