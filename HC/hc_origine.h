/* hc_origine.h — Lecture d'une pile HyperCard D'ORIGINE (le format d'Apple).
 *
 * À ne pas confondre avec hc_file.c, qui lit et écrit NOTRE format, un texte
 * qui s'ouvre dans un éditeur (« -- pile HyperCard (format maison) »). Ici il
 * s'agit du fichier binaire qu'HyperCard écrivait lui-même, entre 1987 et
 * 1998 : une suite de blocs de quatre lettres, STAK, MAST, CARD, BKGD, TAIL,
 * en gros-boutiste.
 *
 * CE QUE CE MODULE FAIT, ET CE QU'IL NE FAIT PAS.
 *
 * Il extrait les SCRIPTS et les NOMS — de la pile, des fonds, des cartes, des
 * boutons et des champs — et rien d'autre. Pas les dessins (blocs BMAP,
 * compressés), pas le texte des champs, pas les polices ni les styles. C'est
 * délibéré et ce n'est pas un premier jet honteux : un extracteur de scripts
 * n'a besoin d'aucune de ces choses, et il rapporte tout de suite ce qu'aucune
 * pile de torture écrite par nous ne peut donner — du vrai HyperTalk, écrit
 * par des gens qui ne cherchaient pas à nous faire plaisir.
 *
 * L'IMPORTATEUR, LUI, N'EST PAS ÉCRIT, et c'est voulu. Un importateur doit
 * rendre une pile qui MARCHE : il est couplé à tout le noyau, et chaque trou
 * de HC devient un de ses bugs, si bien qu'on ne sait plus si l'on a mal lu le
 * bloc ou si HC ne sait pas faire. Un extracteur n'a qu'à dire la vérité sur
 * ce qu'il voit. Il est donc juste, ou refuse.
 *
 * D'OÙ VIENNENT LES OFFSETS. De deux descriptions publiques et indépendantes
 * du format, citées dans docs/mesures/pile_origine.txt, qui CONCORDENT sur
 * tout ce qui est utilisé ici. Aucune ligne de code n'a été reprise d'un autre
 * projet : ni HyperCardPreview ni mystextract ne portent de fichier LICENSE,
 * donc aucune licence n'est accordée, donc on n'emprunte rien — les offsets
 * sont des faits sur un format, le code est le nôtre.
 */
#ifndef HC_ORIGINE_H
#define HC_ORIGINE_H

#include <stddef.h>

/* Un bouton ou un champ. */
enum { HC_ORIG_CHAMP = 0, HC_ORIG_BOUTON = 1 };

typedef struct {
    int   genre;                /* HC_ORIG_CHAMP ou HC_ORIG_BOUTON */
    int   id;
    int   haut, gauche, bas, droite;
    char *nom;                  /* UTF-8, jamais NULL (« » si sans nom) */
    char *script;               /* UTF-8, NULL s'il n'y en a pas */

    /* --- les propriétés communes ---
     *
     * QUATRE DRAPEAUX DU FORMAT SONT INVERSÉS, et c'est le piège de cette
     * structure : le bit allumé signifie FAUX. La spec les écrit entre
     * parenthèses — « (not visible) », « (not enabled) », « (not fixed line
     * height) », « (not shared highlight) ». Les champs ci-dessous portent tous
     * le sens POSITIF, celui de notre modèle, la conversion étant faite une
     * fois pour toutes à la lecture. */
    int   visible;
    int   enabled;              /* bouton : actif ; champ : voir locktext */
    int   locktext;             /* champ : le même bit que enabled */
    int   dont_wrap;
    int   dont_search;
    int   shared_text;
    int   fixed_lh;
    int   auto_tab;
    int   family;               /* 0-15 */

    /* --- LES MÊMES BITS, DEUX SENS ---
     *
     * L'octet de 0xE porte quatre bits dont la signification DÉPEND DU GENRE
     * de la part. Un bouton y lit « montre le nom », « allumé », « allumage
     * automatique », « allumage partagé » ; un champ y lit « montre les
     * lignes », « marges larges », « plusieurs lignes », « sélection
     * automatique ». Les huit champs sont donc séparés ici, et seuls ceux du
     * bon genre sont remplis — mélanger les deux familles donnerait des
     * propriétés plausibles et fausses, ce qui est le pire résultat. */
    int   showname, hilite, autohilite, shared_hilite;        /* bouton */
    int   show_lines, wide_margins, multiple_lines, auto_select; /* champ */

    const char *style;          /* « transparent », « scrolling »… jamais NULL */
    int   titlewidth;           /* bouton seulement */
    int   icon;                 /* bouton seulement ; 0 = aucune */
    int   premiere_ligne;       /* champ seulement */
    int   derniere_ligne;       /* champ seulement */

    int   text_align;           /* -1 droite, 0 gauche, 1 centre */
    char *police;               /* nom résolu par FTBL, ou NULL si inconnu */
    int   textsize;
    int   textstyle;            /* MÊMES BITS que HC_BOLD..HC_GROUP */
    int   textheight;
} HcOrigPart;

/* Le texte d'une part.
 *
 * IL NE VIT PAS TOUJOURS AVEC SA PART, et c'est la subtilité de ce format. Dans
 * un bloc CARD, un identifiant NÉGATIF désigne une part de la carte ; un
 * identifiant POSITIF désigne un champ du FOND dont cette carte-là porte son
 * propre texte. C'est exactement ce que notre modèle appelle un BgText, et le
 * confondre avec le texte par défaut du fond ferait que toutes les cartes
 * afficheraient la même chose.
 *
 * Les contenus appartiennent à la COUCHE et non à la part : une seule
 * propriété de chaque pointeur, donc aucune double libération possible. */
typedef struct {
    int   id_part;
    int   du_fond;              /* 1 : part du fond, texte propre à cette carte */
    char *texte;                /* UTF-8, jamais NULL */
    int   decore;               /* portait des styles par plage, NON LUS */
} HcOrigContenu;

/* ═══ LE DESSIN D'UNE COUCHE ══════════════════════════════════
 *
 * Deux plans d'un bit par pixel, l'image et le masque, tels que le bloc BMAP les
 * porte. C'est tout ce que ce module en dit : la règle des couleurs — image à 1
 * donne du noir, sinon masque à 1 donne du blanc, sinon transparent — est une
 * INTERPRÉTATION, et elle appartient au bâtisseur. Ici on ne fait que
 * décompresser et placer.
 *
 * Les deux plans font la taille du rectangle de la CARTE, même quand les
 * données ne couvrent qu'une partie : le bloc porte trois rectangles — la
 * carte, le masque, l'image — et les deux derniers se posent dans le premier.
 * Un appelant qui devrait les recoller lui-même refarait ce calcul, et les
 * arrondis à 32 bits qui vont avec.
 *
 * Bit 7 du premier octet = pixel de GAUCHE, comme QuickDraw. `octets_par_ligne`
 * est calé sur 32 bits, comme dans le fichier. */
typedef struct {
    int present;                /* 0 : pas de bloc BMAP, ou il a été refusé */
    int largeur, hauteur;       /* le rectangle de la carte, en pixels */
    int octets_par_ligne;
    unsigned char *image;       /* NULL si `present` est nul */
    unsigned char *masque;
    /* CE QUI A ÉTÉ LU, pour la mesure. Les tailles annoncées sont calées sur
     * quatre octets ; `reste_*` dit combien d'octets le flot a laissés derrière
     * lui, ce qui doit rester entre 0 et 3. */
    unsigned long taille_masque, taille_image;
    int reste_masque, reste_image;
} HcOrigDessin;

/* Une carte ou un fond : les deux blocs ont la même queue (parts, contenus,
 * nom, script) et ne diffèrent que par leur en-tête. */
typedef struct {
    int   id;
    int   fond;                 /* carte : l'id de son fond ; fond : 0 */
    int   marque;               /* carte marquée (« marked ») ; vient de la liste */
    int   debut_de_fond;        /* première carte de son fond, selon la liste */
    /* Les drapeaux de la COUCHE, qui ne sont pas ceux de ses parts : ils vivent
     * dans l'en-tête du bloc, à 0x14. */
    int   dont_search;
    int   cant_delete;
    int   bloc_image;           /* l'id du bloc BMAP, ou 0 si la couche est
                                 * transparente. */
    HcOrigDessin dessin;        /* le dessin lu, quand il y en a un */
    char *nom;                  /* UTF-8, jamais NULL */
    char *script;               /* UTF-8, NULL s'il n'y en a pas */
    HcOrigPart *parts;
    int   nparts;
    HcOrigContenu *contenus;
    int   ncontenus;
} HcOrigCouche;

/* Le recensement des blocs, avant toute interprétation. C'est la seule partie
 * qui ne dépend que de l'en-tête de bloc — la brique dont on est le plus sûr —
 * et elle se valide SEULE : une chaîne qui part de 0 et tombe pile sur la fin
 * du fichier, avec des types de quatre lettres imprimables, confirme la
 * disposition sur des octets qu'on n'a pas écrits. */
typedef struct {
    char type[5];
    int  id;
    unsigned long taille;
    unsigned long offset;
} HcOrigBloc;

typedef struct {
    /* --- le recensement --- */
    HcOrigBloc   *blocs;
    int           nblocs;
    int           chaine_atteint_la_fin;  /* la somme des tailles == n */
    int           tail_vu;                /* un bloc TAIL a terminé la chaîne */

    /* L'ORDRE DES CARTES N'EST PAS LU, et il faut le dire fort.
     *
     * Il ne vient pas de l'ordre des blocs CARD dans le fichier : il vient des
     * blocs LIST et PAGE, que ce module recense sans les interpréter. Les
     * cartes sont donc rendues dans l'ordre où le FICHIER les porte, qui n'est
     * pas forcément celui de la pile.
     *
     * Pour extraire des scripts, ça suffit — chaque script arrive avec l'id de
     * sa carte, qui l'identifie sans ambiguïté. Pour un importateur, non : il
     * lui faudra ces deux blocs. Trouvé par le lecteur tiers, qui a refusé la
     * pile de démonstration du harnais avec « no LIST block » ; sans lui je
     * l'aurais écrite sans m'en apercevoir. D'où ce drapeau, affiché à chaque
     * lecture : un manque visible vaut mieux qu'un manque qu'on oublie. */
    int           liste_vue;              /* un bloc LIST est présent */

    /* L'ORDRE, QUAND IL A PU ÊTRE LU ET VÉRIFIÉ.
     *
     * `ordre_lu` ne dit pas « il y avait un bloc LIST » — ça, c'est
     * `liste_vue`. Il dit que la chaîne LIST -> PAGE -> références de cartes a
     * été parcourue ET que les sommes de contrôle de la liste et de chacune de
     * ses pages tombent juste. C'est une garantie d'un autre ordre de grandeur :
     * les identifiants rendus sont ceux qu'HyperCard a écrits, dans son ordre,
     * et pas une suite d'entiers vraisemblables.
     *
     * À zéro, `cartes` reste dans l'ordre du FICHIER, qui n'est pas celui de la
     * pile. Le drapeau est donc à regarder avant de croire l'ordre. */
    int           ordre_lu;
    int           npages;                 /* pages de la liste des cartes */

    /* --- STAK --- */
    unsigned long format;       /* 8 : HyperCard 1.x ; 10 : 2.x */
    unsigned long ncartes;      /* ce que STAK ANNONCE */
    unsigned long nfonds;       /* idem */
    int           largeur, hauteur;
    int           somme_juste;  /* la somme de contrôle de STAK totalise zéro */
    int           protegee;     /* accès privé : les blocs sont chiffrés */
    char         *script;       /* UTF-8, NULL s'il n'y en a pas */

    /* --- les couches --- */
    HcOrigCouche *fonds;   int nfonds_lus;
    HcOrigCouche *cartes;  int ncartes_lues;

    /* DEUX SORTES DE FAUTE, ET ELLES NE SE TRAITENT PAS PAREIL.
     *
     * Une faute STRUCTURELLE — une taille de bloc nulle, une chaîne qui sort
     * du fichier, une liste de parts qui ne tombe pas où sa propre taille
     * annoncée le dit — arrête la lecture : on ne peut pas continuer sans
     * inventer, et inventer est exactement ce qu'on refuse.
     *
     * Une faute LOCALE — l'octet marqueur d'un script de part qui n'est pas le
     * zéro que les deux sources annoncent — ne doit pas faire perdre les cent
     * autres scripts du fichier. Elle est COMPTÉE ici, et la part concernée
     * rend un script nul. Refuser le fichier entier pour un octet serait aussi
     * malhonnête que de deviner : dans les deux cas on perdrait ce qu'on
     * savait lire. */
    int           anomalies;

    /* ET PARMI LES FAUTES LOCALES, CELLES QUI ONT VRAIMENT PERDU QUELQUE CHOSE.
     *
     * LE DÉFAUT QUE CECI CORRIGE, relevé À L'USAGE : la boîte d'import annonçait
     * « 1 anomalie relevée en chemin : quelque chose n'a pas pu être lu et a été
     * laissé de côté ». Sur « Stack Templates », cette anomalie unique est une
     * TAILLE DE BLOC RÉPARÉE — l'octet de poids fort du bloc MAST. Rien n'a été
     * laissé de côté : le bloc a été lu, et la chaîne des 65 blocs retombe
     * exactement sur la fin du fichier, ce qui est la preuve qu'elle a été bien
     * lue. Le message énonçait donc une PERTE là où il n'y en avait pas.
     *
     * C'est la quatrième fois dans ce projet qu'un diagnostic faux coûte plus
     * cher qu'un diagnostic vague : le vague fait chercher partout, le faux fait
     * chercher au mauvais endroit et donne confiance en le faisant. Ici il aurait
     * fait douter d'une pile entière.
     *
     * `anomalies` compte donc la MÉFIANCE — un recoupement qui ne tombe pas
     * juste, une taille réparée, un style qui ne va pas au genre, une carte que
     * la liste ne nomme pas — et `perdus` compte ce qui MANQUE dans le résultat :
     * un dessin abandonné, un script abandonné, un nom de police abandonné.
     * `perdus` est toujours inférieur ou égal à `anomalies`. Zéro perdu avec des
     * anomalies veut dire « tout est là, et quelque chose demande à être vérifié
     * » — ce qui est exactement l'état de « Stack Templates ». */
    int           perdus;

    /* LA TABLE DES POLICES, et elle n'est pas un luxe : les identifiants de
     * police n'étaient PAS les mêmes d'un Macintosh à l'autre, si bien
     * qu'HyperCard rangeait les NOMS dans la pile. Sans ce bloc, « police 3 »
     * ne veut rien dire. */
    struct { int id; char *nom; } *polices;
    int           npolices;

    /* Combien de contenus portaient des styles par plage. Ils ne sont pas lus —
     * il faudrait le bloc STBL — et leur TEXTE l'est. Compté pour que le manque
     * soit visible plutôt qu'oublié. */
    int           contenus_decores;
} HcOrigPile;

/* Rend 0 en cas de succès, et remplit `pourquoi` sinon.
 *
 * UN REFUS NE LAISSE RIEN À LIBÉRER : la pile rendue est vide, et seul
 * `pourquoi` porte quelque chose. Après un succès, hc_origine_libere est
 * obligatoire. (Le contrat inverse — au calleur de nettoyer un refus — a tenu
 * une heure : il n'était écrit nulle part, et le premier appelant l'a oublié.
 * Voir hc_origine.c pour ce que le fuzzing a vraiment mesuré, et pour la part
 * qui revenait au module plutôt qu'à l'appelant.)
 *
 * REFUSE plutôt que de deviner : une taille de bloc nulle, une chaîne qui sort du fichier, une
 * liste de parts qui ne tombe pas où sa taille annoncée le dit, une chaîne de
 * caractères sans son zéro — tout cela arrête la lecture avec un motif écrit
 * en clair. Un lecteur qui rend des scripts vraisemblables à partir d'octets
 * qu'il a mal compris est bien pire qu'un lecteur qui dit non. */
int  hc_origine_lit(const unsigned char *octets, size_t n,
                    HcOrigPile *pile, char *pourquoi, size_t npourquoi);
void hc_origine_libere(HcOrigPile *pile);

/* CE FICHIER EST-IL UNE PILE AU FORMAT D'ORIGINE ?
 *
 * La question se pose au moment d'OUVRIR : notre format est du texte, le sien
 * est du binaire gros-boutiste, et l'application doit choisir son lecteur sans
 * rien demander à personne. Les quatre lettres du premier bloc suffisent —
 * elles sont à l'offset 4, et une pile HyperCard commence TOUJOURS par son bloc
 * STAK.
 *
 * Aucune confusion possible avec le nôtre : un .stack de HC commence par
 * « stack », donc ses octets 4 à 8 sont « k » et ce qui suit le nom. Et se
 * tromper ne coûte rien : hc_origine_lit refuse avec un motif écrit en clair,
 * exactement comme il le fait déjà pour un fichier abîmé.
 *
 * On ne va pas plus loin ICI — ni somme de contrôle, ni chaîne de blocs. Ce
 * n'est pas une validation, c'est un AIGUILLAGE : valider est le travail de
 * hc_origine_lit, qui le fait mieux et qui sait dire pourquoi. */
int  hc_origine_reconnait(const unsigned char *octets, size_t n);

/* MacRoman -> UTF-8, et les « \r » du Mac classique -> « \n ».
 *
 * Exporté parce qu'il se teste SEUL, et exhaustivement : 256 octets, 256
 * réponses, et la référence de la suite les porte toutes. Le noyau n'avait
 * aucune table MacRoman complète — dup_script (hc_script.c) ne convertit que
 * les quatre caractères dont la SYNTAXE a besoin (¬ ≠ ≤ ≥) — si bien qu'un
 * « é » d'une pile d'origine, 0x8E, arrivait tel quel dans un monde UTF-8.
 *
 * Et il n'y a pas de doublon avec dup_script : celui-ci accepte déjà les
 * formes UTF-8 de ≠ ≤ ≥ ¬ aussi bien que les octets MacRoman. On transcode
 * donc fidèlement ici, et la syntaxe reste son affaire à lui.
 *
 * Rend une chaîne à libérer, ou NULL si la mémoire manque. */
char *hc_origine_utf8(const unsigned char *octets, size_t n);

#endif
