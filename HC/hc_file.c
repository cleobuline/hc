/* hc_file.c — Format de pile texte, v1.
 *
 * Un objet s'ouvre par une ligne d'en-tête et se ferme par « end <type> ».
 * Les lignes de script et de contenu sont préfixées par « | » : ainsi un
 * script contenant « end mouseUp » ou « end script » ne casse pas l'analyse.
 *
 *     stack "Essai"
 *     script
 *     | on mouseUp
 *     |   beep
 *     | end mouseUp
 *     end script
 *     end stack
 *
 *     background "commun"
 *     button "suivant"
 *     script
 *     | on mouseUp
 *     |   go to card "zaza"
 *     | end mouseUp
 *     end script
 *     end button
 *     end background
 *
 *     card "accueil" background "commun"
 *     field "notes"
 *     contents
 *     | du texte
 *     end contents
 *     textstyle 1
 *     run 3,2,2
 *     end field
 *     end card
 *
 * Le style d'un champ tient sur deux lignes de nature différente :
 *   textstyle N   style du champ entier, valeur de repli
 *   run s,l,N     une plage : N s'applique aux `l` octets à partir de `s`
 * Un champ de fond non partagé a un texte ET un style par carte : la carte
 * les écrit ensemble, le bloc « bgtextdata » suivi de ses lignes « bgrun ».
 *
 *     bghilite 14
 *     bgtext 12
 *     bgtextdata
 *     | note propre a cette carte
 *     end bgtextdata
 *     bgrun 0,4,1
 *
 * Les icones appartiennent a la PILE, comme les ressources ICON de HyperCard :
 * une pile emporte ses icones, et un bouton n'en retient que le numero.
 *
 *     iconres 20554 "Terminator"
 *     | 00000000
 *     | 00018000
 *     | ...
 *     end iconres
 *
 * 32x32 en 1 bit, soit 128 octets, ecrits en hexadecimal a raison de quatre
 * octets par ligne : une ligne du fichier est une ligne de l'icone, et il y en
 * a trente-deux. L'hexadecimal plutot que le base64 des blocs « paint » :
 * c'est court, ca se lit, et ca se retouche a la main. Bit de poids fort a
 * gauche, bit a 1 = encre — la disposition de HCICONS, a l'octet pres.
 *
 * Le mot-cle est « iconres » et non « icon », deja pris par l'attribut de
 * bouton. Les distinguer au seul garde « && part » ne tiendrait que parce que
 * la pile s'ecrit avant les cartes : trop fragile pour qu'on s'y fie.
 *
 * Un binaire anterieur relisant une pile qui contient des icones ne les
 * comprend pas, mais ne s'y casse pas : « iconres ... » ne repond a aucun
 * prefixe connu et les lignes « | » hors bloc sont deja ignorees.
 */
#include "hc_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>    /* close, pour le fichier temporaire de hc_save */
#include <sys/stat.h>  /* stat, fchmod : les permissions du fichier remplacé */

/* LA DERNIÈRE VERSION DE FORMAT QUE CE BINAIRE COMPREND.
 *
 * Écrite par hc_save, exigée par hc_load : un seul endroit, sans quoi
 * l'écrivain et le lecteur finiraient par ne plus parler de la même chose. */
#define HC_FORMAT_MAX 2

/* POURQUOI LE DERNIER hc_load A REFUSÉ.
 *
 * hc_load rendait NULL pour tout — fichier absent, tronqué, sans ligne
 * « stack », d'un format inconnu — et l'interface n'avait que « Pile
 * illisible » à en dire. « Format trop récent » et « fichier abîmé »
 * appellent pourtant des gestes très différents de la part de l'utilisateur.
 *
 * Une phrase courte, en français, valable jusqu'au prochain hc_load. */
static char g_load_erreur[160] = "";

const char *hc_load_erreur(void)
{
    return g_load_erreur[0] ? g_load_erreur : NULL;
}

/* Écrit une chaîne entre guillemets, en protégeant les guillemets et les
 * contre-obliques qu'elle contient. Sans ça, un objet nommé
 *     go card "canard"
 * s'écrivait  button "go card "canard""  et le lecteur, qui s'arrête au
 * guillemet suivant, ne relisait que « go card ». Le nom était donc perdu
 * à l'écriture, pas à la lecture. */
/* UN RETOUR À LA LIGNE DANS UN NOM COUPAIT LE FICHIER EN DEUX.
 *
 * put_quoted n'échappait que le guillemet et la contre-oblique. Or le langage
 * pose n'importe quelle chaîne comme nom :
 *
 *     set the name of card button 1 to "Bonjour" & return & "Monde"
 *     save this stack
 *
 * L'en-tête sortait physiquement sur deux lignes —
 *
 *     button "Bonjour
 *     Monde"
 *
 * — et la relecture rendait un bouton nommé « Bonjour », la ligne « Monde" »
 * étant avalée comme une ligne inconnue. Sans un mot. Mesuré : le style et la
 * police se coupaient de la même façon.
 *
 * Le sérialiseur ne doit pas dépendre d'une restriction implicite de
 * l'interface : c'est le FORMAT qui sait encoder ce que le modèle accepte. */
static void put_quoted(FILE *f, const char *s)
{
    fputc('"', f);
    for (const char *p = s ? s : ""; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n",  f); break;
        case '\r': fputs("\\r",  f); break;
        default:   fputc(*p, f);
        }
    }
    fputc('"', f);
}

/* Une chaîne écrite SANS GUILLEMETS, seule sur sa ligne ou dans une liste
 * séparée par des virgules : une police de caractères.
 *
 * Le guillemet n'a rien à y faire, mais le retour à la ligne y couperait une
 * ligne structurelle comme ailleurs, et la virgule y découperait un champ de
 * trop. La virgule prend donc un échappement à elle, « \c », plutôt que
 * « \, » : ainsi la chaîne écrite ne contient PLUS AUCUNE virgule, et le
 * découpage des listes reste celui d'avant, à la virgule, sans rien savoir des
 * échappements.
 *
 * Les échappements inconnus sont rendus tels quels à la lecture : une pile
 * écrite avant ce changement, avec une police contenant une contre-oblique,
 * se relit exactement comme avant. */
static void put_echappe(FILE *f, const char *s)
{
    const char *d = s ? s : "";
    size_t n = strlen(d);
    for (size_t i = 0; i < n; i++) {
        /* Le lecteur applique ltrim et rtrim aux lignes structurelles : un
         * blanc AU BORD de la chaîne y serait mangé. Il suffit de protéger le
         * premier et le dernier caractère — ceux du milieu sont à l'abri
         * derrière eux. « Times New Roman » s'écrit donc toujours tel quel, et
         * une police à blancs de bord revient enfin intacte. */
        int bord = (i == 0 || i == n - 1);
        switch (d[i]) {
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n",  f); break;
        case '\r': fputs("\\r",  f); break;
        case ',':  fputs("\\c",  f); break;
        case '\t': fputs("\\t",  f); break;
        case ' ':  if (bord) fputs("\\s", f); else fputc(' ', f); break;
        default:   fputc(d[i], f);
        }
    }
}

/* Le pendant lecture, SUR PLACE : la chaîne décodée n'est jamais plus longue
 * que la chaîne encodée. */
static void desechappe(char *s)
{
    if (!s) return;
    char *e = s;
    for (const char *p = s; *p; ) {
        if (*p == '\\' && p[1]) {
            switch (p[1]) {
            case 'n':  *e++ = '\n'; p += 2; continue;
            case 'r':  *e++ = '\r'; p += 2; continue;
            case 'c':  *e++ = ',';  p += 2; continue;
            case 't':  *e++ = '\t'; p += 2; continue;
            case 's':  *e++ = ' ';  p += 2; continue;
            case '\\': *e++ = '\\'; p += 2; continue;
            case '"':  *e++ = '"';  p += 2; continue;
            default: break;               /* inconnu : tel quel */
            }
        }
        *e++ = *p++;
    }
    *e = '\0';
}

static char *dupstr_file(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p) hc_memoire_epuisee("copie d'une chaîne lue dans une pile");
    memcpy(p, s, n);
    return p;
}

/* ==================== écriture ==================== */

/* UN « | » PAR SEGMENT, Y COMPRIS LE SEGMENT VIDE FINAL.
 *
 * L'ancienne boucle s'arrêtait sur « *p », si bien que « abc » et « abc\n »
 * produisaient EXACTEMENT le même fichier : une seule ligne « | abc ». Le
 * lecteur remettait ensuite un saut de ligne après chaque « | », et les deux
 * revenaient en « abc\n ». Un champ enregistré puis relu n'était donc plus le
 * même — et pas seulement dans les cas tordus : « abc » suffisait.
 *
 * Un texte de N sauts de ligne a N+1 segments. On les écrit tous, et le
 * lecteur les joint ENTRE eux au lieu d'ajouter après chacun :
 *
 *     "abc"     -> ["abc"]          -> | abc
 *     "abc\n"   -> ["abc", ""]      -> | abc      puis  |
 *     "a\nb"    -> ["a", "b"]       -> | a        puis  | b
 *     ""        -> [""]             -> |
 *
 * L'aller-retour est alors exact, et le format se relit sans ambiguïté. */
static void put_block_wrap(FILE *f, const char *tag, const char *text, int wrap)
{
    if (!text || !*text) return;
    fprintf(f, "%s\n", tag);
    const char *p = text;
    for (;;) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (wrap) {
            /* découper en tronçons de 100 caractères (base64) : le lecteur
               les recolle et le décodeur base64 ignore les sauts de ligne. */
            int off = 0;
            while (off < len) {
                int chunk = (len - off > 100) ? 100 : (len - off);
                fprintf(f, "| %.*s\n", chunk, p + off);
                off += chunk;
            }
            if (len == 0) fprintf(f, "|\n");
        } else {
            fprintf(f, "| %.*s\n", len, p);
        }
        if (!nl) break;
        p = nl + 1;
    }
    fprintf(f, "end %s\n", tag);
}

static void put_block(FILE *f, const char *tag, const char *text)
{
    put_block_wrap(f, tag, text, 0);   /* scripts, contenus : pas de découpage */
}

static void put_paint(FILE *f, const char *b64)
{
    put_block_wrap(f, "paint", b64, 1);   /* base64 : découpé en lignes courtes */
}

/* Une icône de pile : en-tête, puis 128 octets en hexadécimal, huit par
 * ligne. Le nom passe par put_quoted, il peut donc contenir un guillemet. */
static void put_icon(FILE *f, const struct StackIcon *ic)
{
    fprintf(f, "iconres %d ", ic->id);
    put_quoted(f, ic->name ? ic->name : "");
    fputc('\n', f);
    /* Quatre octets par ligne : une ligne du fichier = une ligne de l'icone,
     * comme dans la source de HCICONS. Trente-deux lignes, et le dessin se
     * devine a l'oeil nu dans le fichier. */
    for (int i = 0; i < HC_ICON_BYTES; i += 4) {
        fputs("| ", f);
        for (int k = 0; k < 4; k++) fprintf(f, "%02X", ic->bits[i + k]);
        fputc('\n', f);
    }
    fprintf(f, "end iconres\n");
}

/* Plages de style d'un champ, une par ligne :
 *     run <start>,<len>,<style>
 * Les offsets sont en octets dans le texte du champ, comme dans le noyau
 * (hc_run_at les rend tels quels). La liste est déjà triée et fusionnée par
 * runs_tidy à l'écriture, on la recopie donc dans l'ordre.
 * Le tag change selon le porteur : « run » pour la liste du champ lui-même,
 * « bgrun » pour celle qu'une carte tient sur un champ de fond non partagé. */
static void put_runs(FILE *f, const char *tag, const struct RunList *rl)
{
    if (!rl) return;
    for (int i = 0; i < rl->n; i++) {
        const struct TextRun *r = &rl->v[i];
        if (r->len <= 0) continue;
        /* Une plage muette sur les trois attributs décrit le champ : inutile.
         * Attention, `style == 0` n'est pas muet — c'est « plain », qui doit
         * survivre à l'enregistrement dans un champ gras. L'ancien test le
         * jetait, et le mot reprenait le gras du champ au rechargement. */
        if (r->style == HC_STYLE_INHERIT && r->size == 0 && !r->font &&
            r->color == HC_COLOR_INHERIT) continue;

        /* Trois formes, de la plus courte à la plus longue, pour que les piles
         * déjà enregistrées gardent exactement la même allure et qu'un binaire
         * plus ancien continue de les lire :
         *     s,l,style
         *     s,l,style,corps,police
         *     s,l,style,corps,police,couleur
         * La police pouvant contenir des espaces mais jamais de virgule, la
         * couleur se lit sans ambiguïté après la dernière. */
        if (r->size == 0 && !r->font && r->color == HC_COLOR_INHERIT) {
            fprintf(f, "%s %d,%d,%d\n", tag, r->start, r->len, r->style);
        } else {
            /* La police passe par put_echappe : sa virgule devient « \c », si
             * bien que la ligne n'en contient plus une seule de trop et que le
             * découpage ci-dessous reste exactement celui d'avant. */
            fprintf(f, "%s %d,%d,%d,%d,", tag, r->start, r->len, r->style, r->size);
            put_echappe(f, r->font ? r->font : "");
            if (r->color == HC_COLOR_INHERIT) fputc('\n', f);
            else fprintf(f, ",%d\n", r->color);
        }
    }
}

static void put_part(FILE *f, Object *o)
{
    const char *kind = (o->type == OBJ_BUTTON) ? "button" : "field";
    fprintf(f, "%s ", kind); put_quoted(f, o->name); fputc('\n', f);
    fprintf(f, "rect %d,%d,%d,%d\n", o->x, o->y, o->x + o->w, o->y + o->h);
    /* Le style passait par un « %s » brut entre guillemets : un guillemet dans
     * le style refermait la chaîne, un retour à la ligne coupait le fichier. */
    if (o->style) { fprintf(f, "style "); put_quoted(f, o->style); fputc('\n', f); }
    put_block(f, "script", o->script);
    if (o->type == OBJ_FIELD || o->type == OBJ_BUTTON) put_block(f, "contents", o->contents);
    if (!o->visible) fprintf(f, "hidden\n");
    if (o->hilite) fprintf(f, "hilite\n");
    if (o->autohilite) fprintf(f, "autohilite\n");
    if (o->textsize) fprintf(f, "textsize %d\n", o->textsize);
    /* Écrit seulement s'il a été posé explicitement : zéro veut dire
     * « déduit du corps », et les piles enregistrées avant l'existence
     * de cette ligne se relisent donc sans rien perdre. */
    if (o->textheight) fprintf(f, "textheight %d\n", o->textheight);
    if (!o->showname) fprintf(f, "hidename\n");   /* nom masqué (défaut = affiché) */
    /* Comme « hidename » : on n'écrit que l'exception. Le défaut étant actif,
     * une pile où personne n'a désactivé de bouton reste identique à ce qu'une
     * version antérieure écrivait — et relisible par elle. */
    if (!o->enabled)  fprintf(f, "disabled\n");
    /* Même règle : on n'écrit que l'exception, le défaut étant partagé. */
    if (o->type == OBJ_BUTTON && !o->shared_hilite)
        fprintf(f, "unsharedhilite\n");
    if (o->icon) fprintf(f, "icon %d\n", o->icon);
    if (o->selectedline) fprintf(f, "selectedline %d\n", o->selectedline);
    if (o->locktext) fprintf(f, "locktext\n");
    if (o->wide_margins) fprintf(f, "widemargins\n");
    /* Écrites seulement si posées : une pile enregistrée avant l'existence de
     * ces lignes se relit donc sans rien perdre, et le fichier ne s'alourdit
     * pas de valeurs par défaut. */
    if (o->marked)         fprintf(f, "marked\n");
    if (o->auto_select)    fprintf(f, "autoselect\n");
    if (o->multiple_lines) fprintf(f, "multiplelines\n");
    if (o->dont_wrap)      fprintf(f, "dontwrap\n");
    if (o->text_align)     fprintf(f, "textalign %d\n", o->text_align);
    if (o->fixed_lh) fprintf(f, "fixedlineheight\n");
    if (o->show_lines) fprintf(f, "showlines\n");
    fprintf(f, "id %d\n", o->id);
    if (o->auto_tab) fprintf(f, "autotab\n");
    if (o->dont_search) fprintf(f, "dontsearch\n");
    if (o->cant_delete) fprintf(f, "cantdelete\n");
    if (o->shared_text) fprintf(f, "sharedtext\n");
    if (o->textfont && *o->textfont) {
        fprintf(f, "textfont "); put_echappe(f, o->textfont); fputc('\n', f);
    }
    if (o->textstyle) fprintf(f, "textstyle %d\n", o->textstyle);
    if (o->type == OBJ_FIELD) put_runs(f, "run", &o->runs);
    if (o->scroll) fprintf(f, "scroll %d\n", o->scroll);
    fprintf(f, "end %s\n", kind);
}

/* Sauvegarde ATOMIQUE : on écrit à côté, puis on renomme.
 *
 * fopen(path, "w") tronque le fichier AVANT d'écrire quoi que ce soit. Un
 * disque plein, un quota atteint, une coupure — et la pile d'origine est déjà
 * détruite, la nouvelle incomplète. Pour le document de l'utilisateur, c'est
 * la faute la plus coûteuse qu'un programme puisse commettre.
 *
 * On écrit donc dans « <chemin>.tmp », on vérifie que tout s'est bien passé
 * (ferror sur le flux ET le retour de fclose, car le dernier bloc peut
 * n'être écrit qu'à la fermeture), et seulement alors rename() prend la
 * place de l'ancien fichier. rename est atomique sur le même système de
 * fichiers : à aucun instant il n'existe de pile à moitié écrite sous le nom
 * attendu.
 *
 * En cas d'échec, le fichier temporaire est retiré et l'ancienne pile est
 * intacte — l'utilisateur perd sa sauvegarde, pas son travail. */
int hc_save(Object *stack, const char *path)
{
    if (!stack || stack->type != OBJ_STACK || !path) return -1;

    /* Nom temporaire IMPRÉVISIBLE, par mkstemp, et non « <chemin>.tmp ».
     *
     * Le nom fixe avait deux défauts dans un répertoire partagé : il écrasait
     * sans prévenir un fichier qui portait déjà ce nom, et il suffisait d'y
     * poser un lien symbolique pour faire écrire la sauvegarde ailleurs.
     * mkstemp crée le fichier lui-même, en exclusivité, avec un nom que
     * personne ne peut deviner.
     *
     * Il reste dans le MÊME répertoire que la destination : rename() n'est
     * atomique qu'à l'intérieur d'un système de fichiers, et c'est cette
     * atomicité qui garantit qu'une sauvegarde interrompue laisse l'original
     * intact. */
    size_t lp = strlen(path);
    char *tmp = malloc(lp + 12);
    if (!tmp) return -1;
    memcpy(tmp, path, lp);
    memcpy(tmp + lp, ".XXXXXX", 8);

    int fd = mkstemp(tmp);
    if (fd < 0) { free(tmp); return -1; }

    /* LES PERMISSIONS DU FICHIER REMPLACÉ SURVIVENT À LA SAUVEGARDE.
     *
     * mkstemp crée en 0600 — c'est bien ce qu'on veut d'un temporaire, dont
     * personne d'autre n'a à lire le contenu pendant qu'on l'écrit. Mais
     * rename() lui donne ensuite le nom du document, avec ses permissions à
     * lui : une pile en 0644, partagée par un groupe ou publiée par un serveur
     * de fichiers, repassait en 0600 à chaque enregistrement. Mesuré :
     *
     *     avant :  -rw-r--r--
     *     après :  -rw-------
     *
     * Ce n'est pas le contenu de la pile, mais c'est bien le DOCUMENT de
     * l'utilisateur qui change sous lui, sans qu'on le lui dise.
     *
     * On recopie donc le mode de l'original. Pour une pile neuve, il n'y a pas
     * d'original : on prend ce que tout programme prend, 0666 filtré par le
     * umask, plutôt que le 0600 d'un temporaire.
     *
     * Best-effort : un fchmod qui échoue ne doit pas faire échouer une
     * sauvegarde par ailleurs valide — l'utilisateur garderait ses données au
     * prix de ses permissions, jamais l'inverse.
     *
     * Ce que cela NE fait PAS : sous macOS, remplacer le fichier par un
     * nouvel inode perd aussi les ACL et les attributs étendus (étiquettes du
     * Finder comprises). copyfile(3) saurait les reporter ; ce code-là ne
     * peut pas être exercé par la suite, qui tourne sous Linux, et il n'est
     * donc pas écrit ici. */
    {
        struct stat sb;
        if (stat(path, &sb) == 0) {
            if (fchmod(fd, sb.st_mode & 07777) != 0) { /* tant pis */ }
        } else {
            mode_t m = umask(0); umask(m);
            if (fchmod(fd, (mode_t)(0666 & ~m)) != 0) { /* tant pis */ }
        }
    }

    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); remove(tmp); free(tmp); return -1; }

    /* LE NUMÉRO DE FORMAT, ET POURQUOI IL ARRIVE MAINTENANT.
     *
     * La version 2 écrit les blocs de texte exactement : un « | » par
     * segment, segment vide final compris, si bien qu'un champ relu est
     * identique octet pour octet à ce qu'il était. La version 1 ne le
     * pouvait pas — « abc » et « abc\n » y donnaient le même fichier.
     *
     * Les deux lectures ne sont donc pas interchangeables, et il faut savoir
     * laquelle appliquer. Sans ce numéro, relire un ancien fichier à la
     * nouvelle règle lui retirerait un saut de ligne par bloc : l'ancien
     * écrivain n'écrivait pas le segment vide final, mais l'ancien lecteur en
     * fabriquait un. Le numéro rend le choix explicite plutôt que deviné.
     *
     * Un fichier sans ligne « format » est de la version 1, par construction :
     * elle n'existait pas quand ils ont été écrits. */
    fprintf(f, "-- pile HyperCard (format maison)\n");
    fprintf(f, "format %d\n\n", HC_FORMAT_MAX);

    fprintf(f, "stack "); put_quoted(f, stack->name); fputc('\n', f);
    fprintf(f, "size %d,%d\n", stack->w, stack->h);
    put_block(f, "script", stack->script);
    /* Les icônes tiennent dans le bloc de la pile : elles lui appartiennent,
     * et se relisent donc avant la première carte susceptible de s'y référer. */
    for (int i = 0; i < stack->nicons; i++) put_icon(f, &stack->icons[i]);
    fprintf(f, "end stack\n\n");

    /* les fonds d'abord : les cartes s'y réfèrent par leur nom */
    /* les fonds d'abord : les cartes s'y réfèrent par leur nom */
    for (int i = 0; i < stack->nparts; i++) {
        Object *bg = stack->parts[i];
        if (bg->type != OBJ_BACKGROUND) continue;

        /* Un fond sans carte n'a pas d'existence dans HyperCard : il n'y est
         * jamais créé seul, et disparaît avec sa dernière carte. On ne le
         * réécrit donc pas — sans quoi les coquilles vides se transmettent
         * d'enregistrement en enregistrement, et deux fonds homonymes rendent
         * « go background "x" » imprévisible selon lequel resolve() trouve. */
        int utilise = 0;
        for (int j = 0; j < stack->nparts && !utilise; j++)
            if (stack->parts[j]->type == OBJ_CARD && stack->parts[j]->bg == bg)
                utilise = 1;
        if (!utilise) continue;

        fprintf(f, "background "); put_quoted(f, bg->name); fputc('\n', f);
        fprintf(f, "id %d\n", bg->id);
        if (bg->dont_search) fprintf(f, "dontsearch\n");
        if (bg->cant_delete) fprintf(f, "cantdelete\n");
        put_block(f, "script", bg->script);
        put_paint(f, bg->paint);
        for (int j = 0; j < bg->nparts; j++) put_part(f, bg->parts[j]);
        fprintf(f, "end background\n\n");
    }

    for (int i = 0; i < stack->nparts; i++) {
        Object *c = stack->parts[i];
        if (c->type != OBJ_CARD) continue;
        fprintf(f, "card "); put_quoted(f, c->name);
        if (c->bg && c->bg->name) { fprintf(f, " background "); put_quoted(f, c->bg->name); }
        /* L'ID DU FOND, ET POURQUOI LE NOM NE SUFFIT PAS.
         *
         * Rien n'interdit deux fonds homonymes, et la lecture prenait le
         * PREMIER de ce nom : deux cartes attachées à deux fonds différents
         * mais de même nom se retrouvaient toutes deux sur le premier après un
         * aller-retour. Mesuré : la seconde carte perdait son fond ET tout
         * son contenu de fond, sans un mot.
         *
         * Le nom RESTE écrit : un binaire plus ancien continue de lire ces
         * fichiers, et il retrouvera le bon fond dans le cas courant où les
         * noms sont distincts. L'id ne fait que lever l'ambiguïté. */
        if (c->bg) fprintf(f, " backgroundid %d", c->bg->id);
        fprintf(f, "\n");
        fprintf(f, "id %d\n", c->id);
        if (c->marked)      fprintf(f, "marked\n");
        /* Les deux verrous de l'Info carte. Écrits seulement s'ils sont posés,
         * comme « marked » : une pile enregistrée avant qu'ils existent se
         * relit sans rien perdre. */
        if (c->dont_search) fprintf(f, "dontsearch\n");
        if (c->cant_delete) fprintf(f, "cantdelete\n");
        put_block(f, "script", c->script);
        put_paint(f, c->paint);
        /* L'allumage des boutons de fond NON PARTAGÉS appartient à la carte.
         * Une ligne par bouton allumé : l'absence vaut éteint, ce qui évite
         * une ligne par bouton et par carte dans une pile ordinaire. */
        for (int j = 0; j < c->nbghilites; j++)
            if (c->bghilites[j].hilite)
                fprintf(f, "bghilite %d\n", c->bghilites[j].button_id);

        for (int j = 0; j < c->nbgtexts; j++) {
            fprintf(f, "bgtext %d\n", c->bgtexts[j].field_id);
            put_block(f, "bgtextdata", c->bgtexts[j].text);
            /* le style suit le texte : un champ de fond non partagé a un
               style par carte, exactement comme il a un texte par carte */
            put_runs(f, "bgrun", &c->bgtexts[j].runs);
        }
        for (int j = 0; j < c->nparts; j++) put_part(f, c->parts[j]);
        fprintf(f, "end card\n\n");
    }

    /* LA SIGNATURE DE FIN, ET CE QU'ELLE SEULE PEUT DIRE.
     *
     * Le format écrit l'en-tête, puis les fonds, puis les cartes — et rien ne
     * certifiait que la dernière carte écrite était RÉELLEMENT la dernière. Un
     * fichier coupé juste après un « end card » parfaitement valide se relisait
     * donc comme une pile complète, amputée de tout ce qui suivait, et
     * l'utilisateur pouvait la réenregistrer par-dessus l'original.
     *
     * Aucune vérification de structure ne peut attraper ce cas : le fichier
     * tronqué est syntaxiquement irréprochable. Il faut une marque, et elle
     * n'a de sens que si l'écrivain la pose toujours — d'où le numéro de
     * format, qui dit au lecteur s'il a le droit de l'exiger. */
    fprintf(f, "end hc-file\n");

    /* Les deux vérifications comptent : ferror voit ce qui a échoué en
     * cours de route, fclose ce qui a échoué en vidant le dernier bloc. */
    int mauvais = ferror(f);
    if (fclose(f) != 0) mauvais = 1;

    if (mauvais || rename(tmp, path) != 0) {
        remove(tmp);
        free(tmp);
        return -1;
    }
    free(tmp);
    /* La pile habite maintenant ICI — et c'est vrai aussi d'un « save as »,
     * qui est la seule façon pour une pile de changer d'adresse. */
    hc_set_stack_path(stack, path);
    return 0;
}

/* ==================== lecture ==================== */

/* Une ligne, quelle que soit sa longueur.
 *
 * Le lecteur employait « char line[4096] » avec fgets. L'écrivain, lui, ne
 * découpe PAS les lignes de script ni de contenu — seul le base64 de la
 * peinture est tronçonné. Une ligne HyperTalk ou une ligne de champ de plus
 * de 4 093 caractères était donc coupée en deux à la relecture : le premier
 * morceau gardait son « | » et le second, qui ne l'avait pas, était pris pour
 * une ligne parasite et jeté.
 *
 * Mesuré : un champ de 8 000 caractères revenait à 4 093, un script de 5 020
 * à 4 105. Perte de données silencieuse au cycle sauvegarde → relecture, sur
 * le document de l'utilisateur.
 *
 * getline existe sur macOS comme sur Linux, mais exige _POSIX_C_SOURCE 200809
 * ou _GNU_SOURCE selon le compilateur, et ce fichier est en C99 nu. Vingt
 * lignes suffisent, et elles ne dépendent de rien.
 *
 * Le tampon est réemployé d'une ligne à l'autre : il ne grandit que si une
 * ligne l'exige, et se libère à la fin de la lecture. */
typedef struct { char *p; size_t cap; } Ligne;

/* TROIS RÉPONSES, et non deux : une ligne, une fin PROPRE, ou un échec.
 *
 * Cette fonction rendait 0 aussi bien à la fin du fichier qu'en cas d'échec
 * de realloc, et ne regardait pas ferror après fgets. Une pénurie de mémoire
 * ou une erreur d'entrée-sortie au milieu du fichier se lisait donc comme une
 * fin de fichier : hc_load s'arrêtait là, rendait une pile AMPUTÉE, et
 * l'appelant n'avait aucun moyen de le savoir.
 *
 * Le pire enchaînement est celui-là : chargement partiel, l'utilisateur ne
 * voit pas tout de suite ce qui manque, il enregistre, et l'original est
 * remplacé par la version incomplète.
 *
 *    1  une ligne est disponible
 *    0  fin de fichier propre
 *   -1  échec : mémoire ou lecture */
#define LIGNE_ECHEC (-1)
static int ligne_lit(Ligne *l, FILE *f)
{
    if (!l->p) {
        l->cap = 512;
        l->p = malloc(l->cap);
        if (!l->p) { l->cap = 0; return LIGNE_ECHEC; }
    }
    size_t used = 0;
    for (;;) {
        if (used + 1 >= l->cap) {
            size_t nc = l->cap * 2;
            char *np = realloc(l->p, nc);
            if (!np) return LIGNE_ECHEC;   /* l->p reste valide et libérable */
            l->p = np; l->cap = nc;
        }
        if (!fgets(l->p + used, (int)(l->cap - used), f)) {
            /* fgets rend NULL pour la fin comme pour l'erreur : c'est ferror
             * qui les sépare, et lui seul. */
            if (ferror(f)) return LIGNE_ECHEC;
            return used > 0;            /* fin de fichier : ce qu'on tient */
        }
        used += strlen(l->p + used);
        if (used && l->p[used - 1] == '\n') return 1;   /* ligne complète */
        if (feof(f)) return used > 0;                   /* dernière ligne */
    }
}

static void ligne_libere(Ligne *l) { free(l->p); l->p = NULL; l->cap = 0; }


static void rtrim(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' ||
                     s[n-1] == ' '  || s[n-1] == '\t'))
        s[--n] = '\0';
}

/* La fin de ligne SEULE. Voir l'appel dans hc_load : les espaces de fin
 * appartiennent au texte d'un bloc, la fin de ligne non. */
static void strip_eol(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

static char *ltrim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* extrait le n-ième littéral entre guillemets de la ligne */
static int get_quoted(const char *line, int which, char *out, int outlen)
{
    int found = 0;
    const char *p = line;
    out[0] = '\0';
    while (*p) {
        if (*p == '"') {
            p++;
            int len = 0;
            int keep = (found == which);
            /* Un guillemet précédé d'une contre-oblique fait partie du nom :
               il ne referme pas la chaîne (voir put_quoted). */
            while (*p && *p != '"') {
                char c = *p;
                /* Les quatre échappements que put_quoted pose. Un échappement
                 * inconnu est rendu tel quel, contre-oblique comprise : une
                 * pile écrite avant ce changement se relit à l'identique. */
                if (c == '\\' && p[1]) {
                    switch (p[1]) {
                    case '"':  c = '"';  p++; break;
                    case '\\': c = '\\'; p++; break;
                    case 'n':  c = '\n'; p++; break;
                    case 'r':  c = '\r'; p++; break;
                    default: break;
                    }
                }
                if (keep && len < outlen - 1) out[len++] = c;
                p++;
            }
            if (keep) { out[len] = '\0'; return 1; }
            found++;
            if (*p == '"') p++;
        } else p++;
    }
    return 0;
}

/* accumulateur de texte pour les blocs « | » */
/* « manque » retient qu'une allocation a échoué pendant la lecture d'un bloc.
 * Le drapeau est COLLANT : une fois posé il ne se retire plus, parce qu'un
 * script ou un texte tronqué est pire qu'un chargement refusé — l'utilisateur
 * réenregistrerait par-dessus l'original sans savoir ce qu'il a perdu. */
typedef struct { char *buf; size_t len, cap; int manque; int nseg; } Acc;

static void acc_line(Acc *a, const char *s)
{
    size_t n = strlen(s);
    if (a->len + n + 2 > a->cap) {
        size_t cap = a->cap ? a->cap * 2 : 256;
        while (cap < a->len + n + 2) cap *= 2;
        char *p = realloc(a->buf, cap);
        if (!p) { a->manque = 1; return; }
        a->buf = p; a->cap = cap;
    }
    memcpy(a->buf + a->len, s, n);
    a->len += n;
    a->buf[a->len++] = '\n';
    a->buf[a->len]   = '\0';
}

/* Comme acc_line, mais sans ajouter de saut de ligne : le bloc paint est du
 * base64 découpé à l'écriture, il doit se recoller à l'identique. */
static void acc_join(Acc *a, const char *s)
{
    size_t n = strlen(s);
    if (a->len + n + 2 > a->cap) {
        size_t cap = a->cap ? a->cap * 2 : 256;
        while (cap < a->len + n + 2) cap *= 2;
        char *p = realloc(a->buf, cap);
        if (!p) { a->manque = 1; return; }
        a->buf = p; a->cap = cap;
    }
    memcpy(a->buf + a->len, s, n);
    a->len += n;
    a->buf[a->len] = '\0';
}

/* UN SEGMENT DE BLOC, JOINT AUX PRÉCÉDENTS.
 *
 * acc_line, juste au-dessus, ajoute un saut de ligne APRÈS chaque « | ». Un
 * texte relu finissait donc toujours par un saut de ligne, qu'il en eût un ou
 * non : « abc » revenait en « abc\n ». C'est le pendant lecture du défaut que
 * put_block_wrap vient de corriger côté écriture.
 *
 * Ici le saut se pose AVANT le segment, sauf pour le premier : N segments
 * donnent N-1 sauts, et le texte revient exactement tel qu'il est parti.
 *
 * acc_line reste, et sert aux fichiers d'AVANT ce changement : eux n'écrivent
 * pas le segment vide final, et les relire à la nouvelle règle leur retirerait
 * un saut de ligne qu'ils étaient censés avoir. Voir `format_fichier`. */
static void acc_seg(Acc *a, const char *s)
{
    size_t n = strlen(s);
    size_t besoin = a->len + n + 2;
    if (besoin > a->cap) {
        size_t cap = a->cap ? a->cap * 2 : 256;
        while (cap < besoin) cap *= 2;
        char *p = realloc(a->buf, cap);
        if (!p) { a->manque = 1; return; }
        a->buf = p; a->cap = cap;
    }
    if (a->nseg++ > 0) a->buf[a->len++] = '\n';
    memcpy(a->buf + a->len, s, n);
    a->len += n;
    a->buf[a->len] = '\0';
}

static char *acc_take(Acc *a)
{
    char *r = a->buf;
    a->buf = NULL; a->len = a->cap = 0; a->nseg = 0;
    return r;
}

/* Ajoute une plage de style à une liste. On écrit directement dans la
 * struct RunList plutôt que de passer par hc_run_add : celui-ci vise la liste
 * « active » (celle de la carte courante pour un champ de fond non partagé),
 * or au chargement il n'y a pas encore de carte courante et c'est une liste
 * précise que l'on veut remplir. Le fichier a été écrit trié et fusionné,
 * donc pas besoin de normaliser. */
/* « s,l,st » (ancienne forme) ou « s,l,st,taille,police ». Le nom de police
 * vient en dernier et court jusqu'au bout de la ligne : il peut donc contenir
 * des espaces (« Times New Roman ») sans qu'on ait à le citer. */
static int parse_run(const char *s, int *start, int *len, int *style,
                     int *size, char *font, int fontlen, int *color)
{
    *size = 0; font[0] = '\0'; *color = HC_COLOR_INHERIT;
    /* LES TROIS PREMIERS CHAMPS SONT LUS BORNÉS, ET LEUR ÉCHEC EST DISTINGUÉ.
     *
     * « %d » est indéfini sur ce qui dépasse un int, et ces deux-là sont des
     * DÉCALAGES dans le texte du champ : une valeur aberrante n'y reste pas
     * cosmétique. hc_entier_lu borne et dit si quelque chose a été lu, ce qui
     * garde la règle d'avant — trois nombres ou rien du tout. */
    {
        char q[3][32]; int lu = 0;
        if (sscanf(s, "%31[^,],%31[^,],%31[^,]", q[0], q[1], q[2]) != 3) return 0;
        *start = hc_entier_lu(q[0], 0, HC_COORD_MAX, 0, &lu);        if (!lu) return 0;
        *len   = hc_entier_lu(q[1], 0, HC_COORD_MAX, 0, &lu);        if (!lu) return 0;
        *style = hc_entier_lu(q[2], HC_STYLE_INHERIT, 0xFFFF, 0, &lu); if (!lu) return 0;
    }

    const char *p = s;
    for (int commas = 0; *p && commas < 3; p++)
        if (*p == ',') commas++;
    if (!*p) return 1;                       /* forme courte : rien de plus */

    /* Même raison qu'ailleurs : hc_entier veut TOUTE la chaîne, on lui donne
     * donc le champ seul et non la fin de la ligne. */
    {
        const char *fin = p;
        while (*fin && *fin != ',') fin++;
        char champ[32];
        size_t l = (size_t)(fin - p);
        if (l >= sizeof champ) l = sizeof champ - 1;
        memcpy(champ, p, l); champ[l] = '\0';
        *size = hc_entier(champ, 0, HC_TEXTE_MAX, 0);
    }
    const char *q = strchr(p, ',');
    if (!q) return 1;                        /* taille sans police */
    q++;

    /* La couleur, s'il y en a une, suit la DERNIÈRE virgule : un nom de police
     * peut contenir des espaces mais jamais de virgule, donc la découpe est
     * sans ambiguïté. Son absence laisse la sentinelle, et les piles écrites
     * avant l'existence de ce champ se relisent sans rien perdre. */
    const char *derniere = strrchr(q, ',');
    int n;
    if (derniere) {
        *color = hc_entier(derniere + 1, HC_COLOR_INHERIT, 0xFFFFFF, HC_COLOR_INHERIT);
        n = (int)(derniere - q);
    } else {
        n = (int)strlen(q);
    }
    while (n > 0 && (q[n-1] == '\n' || q[n-1] == '\r')) n--;
    if (n >= fontlen) n = fontlen - 1;
    if (n < 0) n = 0;
    memcpy(font, q, (size_t)n); font[n] = '\0';
    desechappe(font);
    return 1;
}

/* Rend 0 SI UNE PLAGE A ÉTÉ PERDUE, et l'appelant refuse alors le fichier.
 *
 * Elle rendait void, et une pénurie de mémoire faisait simplement disparaître
 * la plage : le champ revenait avec le bon texte et le mauvais style, sans un
 * mot. Le pire enchaînement est le même que partout ailleurs dans ce lecteur —
 * l'utilisateur ne voit pas tout de suite ce qui manque, il enregistre, et
 * l'original est remplacé par la version appauvrie.
 *
 * Le refus de la plage pour cause de contenu — longueur nulle, plage muette
 * sur tous les attributs — n'est PAS un échec : il n'y avait rien à garder. */
static int add_run(struct RunList *rl, int start, int len, int style,
                   int size, const char *font, int color)
{
    if (!rl || len <= 0 || start < 0) return 1;
    if (style == HC_STYLE_INHERIT && size == 0 && (!font || !*font) &&
        color == HC_COLOR_INHERIT) return 1;
    if (rl->n == rl->cap) {
        int cap = rl->cap ? rl->cap * 2 : 8;
        struct TextRun *v = (struct TextRun *)realloc(rl->v, (size_t)cap * sizeof *v);
        if (!v) return 0;
        rl->v = v; rl->cap = cap;
    }
    rl->v[rl->n].start = start;
    rl->v[rl->n].len   = len;
    rl->v[rl->n].style = style;
    rl->v[rl->n].size  = size;
    rl->v[rl->n].font  = (font && *font) ? dupstr_file(font) : NULL;
    rl->v[rl->n].color = color;
    rl->n++;
    return 1;
}

/* Un chiffre hexadécimal, ou -1. On ne se repose pas sur sscanf : une ligne
 * tronquée ou salie doit interrompre le remplissage sans écrire n'importe
 * quoi dans les 128 octets, et surtout sans déborder. */
static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Le fond d'identifiant `id`, ou NULL. L'id est unique par construction :
 * hc_set_id garde le compteur au-dessus de tout ce qui a été lu. */
static Object *find_bg_id(Object *stack, int id)
{
    if (id <= 0) return NULL;
    for (int i = 0; i < stack->nparts; i++) {
        Object *o = stack->parts[i];
        if (o->type == OBJ_BACKGROUND && o->id == id) return o;
    }
    return NULL;
}

/* Un mot-clé suivi d'un nombre, cherché HORS des guillemets.
 *
 * Une carte peut s'appeler « backgroundid 7 » : chercher le mot dans la ligne
 * entière trouverait celui-là. On repart donc du dernier guillemet, après quoi
 * il ne reste que les mots-clés. Rend -1 si le mot n'y est pas. */
static int mot_nombre_apres_guillemets(const char *ligne, const char *mot)
{
    const char *fin = strrchr(ligne, '"');
    const char *p = strstr(fin ? fin : ligne, mot);
    if (!p) return -1;
    p += strlen(mot);
    while (*p == ' ' || *p == '\t') p++;
    if (*p < '0' || *p > '9') return -1;
    /* Le mot-clé cherché n'est pas forcément le dernier de la ligne : on ne
     * lit que le champ, pas ce qui le suit. */
    return hc_entier_tete(p, 0, HC_ID_MAX, -1);
}

static Object *find_bg(Object *stack, const char *name)
{
    for (int i = 0; i < stack->nparts; i++) {
        Object *o = stack->parts[i];
        if (o->type == OBJ_BACKGROUND && o->name && strcmp(o->name, name) == 0)
            return o;
    }
    return NULL;
}

Object *hc_load(const char *path)
{
    g_load_erreur[0] = '\0';

    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    Object *stack = NULL;   /* la pile */
    Object *owner = NULL;   /* fond ou carte en cours */
    Object *part  = NULL;   /* bouton ou champ en cours */
    Object *target = NULL;  /* à qui appartient le bloc en cours */

    Ligne lg = { NULL, 0 };
    char nm[256], nm2[256]; (void)nm2;
    Acc acc = {0};
    int in_script = 0, in_contents = 0, in_paint = 0, in_bgtext = 0;
    int in_icon = 0;
    struct StackIcon *cur_icon = NULL;   /* icône en cours de remplissage */
    int icon_pos = 0;                    /* octets déjà lus, 0..HC_ICON_BYTES */
    int bgtext_id = 0;
    int last_bgtext = -1;   /* index de la dernière entrée bgtext créée : les
                               lignes « bgrun » qui suivent s'y rattachent */

    /* La version du format, lue sur la ligne « format N ». Absente : c'est
     * un fichier d'avant ce numéro, donc de la version 1. */
    int format_fichier = 1;
    int format_trop_recent = 0;
    int icon_abimee = 0;
    /* La signature de fin a-t-elle été vue ? Voir le verdict, tout en bas. */
    int fin_vue = 0;

    int lecture = 0;
    while ((lecture = ligne_lit(&lg, f)) == 1) {
        char *line = lg.p;
        /* NE RETIRER QUE LA FIN DE LIGNE.
         *
         * rtrim enlevait aussi les espaces et les tabulations, y compris sur
         * le CONTENU d'un « | » : un champ valant « abc   » revenait « abc ».
         * La fin de ligne, elle, n'appartient à personne — c'est le fichier
         * qui la met. Les lignes de structure passent par rtrim juste
         * au-dessous, où retirer des blancs de fin est sans conséquence. */
        strip_eol(line);
        char *s = ltrim(line);

        /* --- lignes d'un bloc --- */
        if (in_script || in_contents || in_paint || in_bgtext || in_icon) {
            if (s[0] == '|') {
                const char *piece = (s[1] == ' ') ? s + 2 : s + 1;
                if (in_icon) {
                    /* Paires de chiffres hexadécimaux. On s'arrête au premier
                     * caractère qui n'en est pas un, et de toute façon à
                     * HC_ICON_BYTES : une ligne trop longue ne déborde pas. */
                    for (const char *p = piece; p[0]; p += 2) {
                        int hi = p[1] ? hexval((unsigned char)p[0]) : -1;
                        int lo = p[1] ? hexval((unsigned char)p[1]) : -1;
                        /* UNE ICÔNE ABÎMÉE REFUSE LE FICHIER, elle ne se
                         * complète pas de zéros.
                         *
                         * On s'arrêtait au premier caractère qui n'était pas
                         * hexadécimal, et les octets manquants restaient nuls :
                         * l'icône revenait à moitié, en silence. C'était
                         * revendiqué, et ça ne l'est plus — le lecteur refuse
                         * une pile dont un texte a été amputé, il n'y a aucune
                         * raison d'accepter une image qui l'est.
                         *
                         * Un chiffre isolé en fin de ligne compte aussi : une
                         * paire coupée en deux n'est pas un octet. */
                        if (hi < 0 || lo < 0 || icon_pos >= HC_ICON_BYTES) {
                            icon_abimee = 1; break;
                        }
                        if (cur_icon) cur_icon->bits[icon_pos] = (unsigned char)(hi * 16 + lo);
                        icon_pos++;
                    }
                }
                else if (in_paint) acc_join(&acc, piece);   /* base64 : recoller */
                /* Version 2 : les segments se joignent entre eux, et le texte
                 * revient exact. Version 1 : un saut après chaque « | », comme
                 * l'ancien lecteur — ces fichiers-là n'ont pas de segment vide
                 * final à joindre. */
                else if (format_fichier >= 2) acc_seg(&acc, piece);
                else                          acc_line(&acc, piece);
                continue;
            }
            /* Hors « | », la ligne est structurelle : ses blancs de fin ne
             * veulent rien dire, et « end script » doit se reconnaître même
             * suivi d'une espace. */
            rtrim(s);
            if (strcmp(s, "end iconres") == 0) {
                /* Les 128 octets d'une icône, tous, ou le fichier est refusé.
                 * Un bloc qui s'arrête plus tôt donnait une image complétée de
                 * zéros — une corruption graphique silencieuse. */
                if (icon_pos != HC_ICON_BYTES) icon_abimee = 1;
                in_icon = 0; cur_icon = NULL; icon_pos = 0;
                continue;
            }
            if (strcmp(s, "end script") == 0) {
                char *t = acc_take(&acc);
                if (target && t) hc_set_script(target, t);
                free(t);
                in_script = 0;
                continue;
            }
            if (strcmp(s, "end contents") == 0) {
                char *t = acc_take(&acc);
                if (target && (target->type == OBJ_FIELD || target->type == OBJ_BUTTON)) {
                    free(target->contents);
                    target->contents = t ? t : NULL;
                } else free(t);
                in_contents = 0;
                continue;
            }
            if (strcmp(s, "end bgtextdata") == 0) {
                char *t = acc_take(&acc);
                if (owner && owner->type == OBJ_CARD && bgtext_id) {
                    if (owner->nbgtexts == owner->capbgtexts) {
                        int cap = owner->capbgtexts ? owner->capbgtexts * 2 : 4;
                        struct BgText *bp = realloc(owner->bgtexts, (size_t)cap * sizeof *bp);
                        if (bp) { owner->bgtexts = bp; owner->capbgtexts = cap; }
                        /* L'agrandissement échoué faisait disparaître TOUT le
                         * texte de fond de cette carte sur cette carte-là, et
                         * `free(t)` en dessous jetait les octets. Le texte d'un
                         * champ de fond est du contenu utilisateur, pas un
                         * réglage : on refuse le fichier. */
                        else acc.manque = 1;
                    }
                    if (owner->nbgtexts < owner->capbgtexts) {
                        struct BgText *e = &owner->bgtexts[owner->nbgtexts];
                        e->field_id = bgtext_id;
                        e->text = t ? t : dupstr_file("");
                        /* realloc rend de la mémoire non initialisée : sans ce
                           nettoyage, la liste de plages part sur un pointeur
                           bidon et hc_free y passe. */
                        memset(&e->runs, 0, sizeof e->runs);
                        last_bgtext = owner->nbgtexts;
                        owner->nbgtexts++;
                        t = NULL;
                    }
                }
                free(t);
                in_bgtext = 0;
                continue;
            }
            if (strcmp(s, "end paint") == 0) {
                char *t = acc_take(&acc);
                if (target && (target->type == OBJ_CARD || target->type == OBJ_BACKGROUND)) {
                    free(target->paint);
                    target->paint = t ? t : NULL;
                } else free(t);
                in_paint = 0;
                continue;
            }
            continue;   /* ligne parasite dans un bloc : ignorée */
        }

        rtrim(s);
        if (!*s || (s[0] == '-' && s[1] == '-')) continue;   /* vide / commentaire */

        if (strncmp(s, "format ", 7) == 0) {
            /* UN FORMAT PLUS RÉCENT SE REFUSE, IL NE S'IMPROVISE PAS.
             *
             * Tout numéro positif était accepté, et tout ce qui valait deux ou
             * plus empruntait les règles de la version 2. Le jour où une
             * version 3 existe, un binaire d'aujourd'hui l'ouvrirait donc
             * comme s'il la connaissait, au lieu de dire qu'il ne la connaît
             * pas — et l'enregistrement suivant écraserait l'original avec ce
             * qu'il en aura compris. C'est la seule incompatibilité vraiment
             * coûteuse : celle qui ne se voit pas.
             *
             * Le numéro de version n'a d'intérêt que si le lecteur s'en sert
             * pour REFUSER. */
            int v = hc_entier(s + 7, 0, HC_ID_MAX, 0);
            if (v > HC_FORMAT_MAX) { format_trop_recent = v; break; }
            if (v > 0) format_fichier = v;
            continue;
        }

        /* --- ouverture de blocs texte --- */
        if (strcmp(s, "script") == 0)   { in_script = 1;   continue; }
        if (strcmp(s, "contents") == 0) { in_contents = 1; continue; }
        if (strcmp(s, "paint") == 0)    { in_paint = 1;    continue; }
        /* Allumage d'un bouton de fond non partagé, sur CETTE carte.
         * `owner` désigne le fond ou la carte en cours : on vérifie donc que
         * c'est bien une carte, un fond n'ayant pas de table d'allumage. */
        if (strncmp(s, "bghilite ", 9) == 0 && owner && owner->type == OBJ_CARD) {
            int bid = hc_id(s + 9);
            if (bid) hc_set_hilite_raw(owner, bid, 1);
            continue;
        }
        if (strncmp(s, "bgtext ", 7) == 0) { bgtext_id = hc_id(s + 7); continue; }
        if (strcmp(s, "bgtextdata") == 0)  { in_bgtext = 1; continue; }

        /* --- icône de pile ---
         * L'en-tête porte le numéro puis le nom : iconres 20554 "Terminator".
         * L'entrée est créée vide, les lignes « | » la remplissent ; une icône
         * dont le bloc serait tronqué garde donc ses octets manquants à zéro
         * plutôt que de disparaître. */
        if (strncmp(s, "iconres ", 8) == 0 && stack) {
            /* Le nom SUIT le numéro sur cette ligne : hc_entier, qui veut
             * toute la chaîne, rendait donc 0 pour toutes les icônes. */
            int iid = hc_entier_tete(s + 8, -HC_ID_MAX, HC_ID_MAX, 0);
            if (!get_quoted(s, 0, nm, sizeof nm)) nm[0] = 0;
            cur_icon = hc_icon_add(stack, iid, nm);
            icon_pos = 0;
            in_icon  = 1;
            continue;
        }

        /* --- plages de style --- */
        if (strncmp(s, "run ", 4) == 0) {
            int a, b, c, sz, co; char fn[128];
            if (part && part->type == OBJ_FIELD &&
                parse_run(s + 4, &a, &b, &c, &sz, fn, sizeof fn, &co) &&
                !add_run(&part->runs, a, b, c, sz, fn, co))
                acc.manque = 1;   /* une plage perdue = fichier refusé */
            continue;
        }
        if (strncmp(s, "bgrun ", 6) == 0) {
            int a, b, c, sz, co; char fn[128];
            if (owner && owner->type == OBJ_CARD &&
                last_bgtext >= 0 && last_bgtext < owner->nbgtexts &&
                parse_run(s + 6, &a, &b, &c, &sz, fn, sizeof fn, &co) &&
                !add_run(&owner->bgtexts[last_bgtext].runs, a, b, c, sz, fn, co))
                acc.manque = 1;
            continue;
        }

        /* --- en-têtes d'objets --- */
        if (strncmp(s, "stack ", 6) == 0) {
            get_quoted(s, 0, nm, sizeof nm);
            stack = hc_new_stack(nm);
            target = stack;
            continue;
        }
        if (!stack) continue;   /* rien avant la pile */

        if (strncmp(s, "size ", 5) == 0) {
            /* Même raison qu'au « rect » plus bas : « %d » est indéfini sur ce
             * qui dépasse un int, et une pile de deux milliards de points de
             * large n'est de toute façon pas une taille. */
            char q[2][32];
            if (sscanf(s + 5, "%31[^,],%31s", q[0], q[1]) == 2) {
                stack->w = hc_coord(q[0], stack->w);
                stack->h = hc_coord(q[1], stack->h);
            }
            continue;
        }
        if (strncmp(s, "background ", 11) == 0) {
            get_quoted(s, 0, nm, sizeof nm);
            owner = hc_new_background(stack, nm);
            target = owner;
            continue;
        }
        if (strncmp(s, "card ", 5) == 0) {
            get_quoted(s, 0, nm, sizeof nm);
            Object *bg = NULL;
            /* L'ID D'ABORD : il est sans ambiguïté. Le nom ne sert plus que de
             * repli, pour les fichiers écrits avant que l'id soit noté — et
             * pour un fichier dont l'id désignerait un fond absent, où le nom
             * reste la meilleure indication disponible. */
            int bgid = mot_nombre_apres_guillemets(s, "backgroundid");
            if (bgid > 0) bg = find_bg_id(stack, bgid);
            if (!bg && get_quoted(s, 1, nm2, sizeof nm2)) bg = find_bg(stack, nm2);
            owner = hc_new_card(stack, bg, nm);
            target = owner;
            last_bgtext = -1;
            continue;
        }
        if (strncmp(s, "button ", 7) == 0 && owner) {
            get_quoted(s, 0, nm, sizeof nm);
            part = hc_new_button(owner, nm);
            target = part;
            continue;
        }
        if (strncmp(s, "field ", 6) == 0 && owner) {
            get_quoted(s, 0, nm, sizeof nm);
            part = hc_new_field(owner, nm);
            target = part;
            continue;
        }
        if (strcmp(s, "hidden") == 0) {
            if (target) target->visible = 0;
            continue;
        }
        if (strcmp(s, "hilite") == 0) {
            if (target) target->hilite = 1;
            continue;
        }
        if (strcmp(s, "marked") == 0) {
            if (target) target->marked = 1;
            continue;
        }
        if (strcmp(s, "autohilite") == 0) {
            if (target) target->autohilite = 1;
            continue;
        }
        if (strncmp(s, "textheight ", 11) == 0 && part) {
            part->textheight = hc_entier(s + 11, 0, HC_TEXTE_MAX, part->textheight);
            continue;
        }
        if (strncmp(s, "textsize ", 9) == 0 && part) {
            part->textsize = hc_entier(s + 9, 0, HC_TEXTE_MAX, part->textsize);
            continue;
        }
        if (strncmp(s, "icon ", 5) == 0 && part) {
            part->icon = hc_entier(s + 5, -HC_ID_MAX, HC_ID_MAX, part->icon);
            continue;
        }
        if (strncmp(s, "selectedline ", 13) == 0 && part) {
            part->selectedline = hc_entier(s + 13, 0, HC_COORD_MAX, part->selectedline);
            continue;
        }
        if (strcmp(s, "locktext") == 0 && part)       { part->locktext = 1; continue; }
        if (strcmp(s, "widemargins") == 0 && part)    { part->wide_margins = 1; continue; }
        if (strcmp(s, "marked") == 0 && target)        { target->marked = 1; continue; }
        if (strcmp(s, "autoselect") == 0 && part)     { part->auto_select = 1; continue; }
        if (strcmp(s, "multiplelines") == 0 && part)  { part->multiple_lines = 1; continue; }
        if (strcmp(s, "dontwrap") == 0 && part)       { part->dont_wrap = 1; continue; }
        if (strncmp(s, "textalign ", 10) == 0 && part) { part->text_align = hc_entier(s + 10, 0, 2, part->text_align); continue; }
        if (strcmp(s, "fixedlineheight") == 0 && part) { part->fixed_lh = 1; continue; }
        if (strcmp(s, "showlines") == 0 && part)      { part->show_lines = 1; continue; }
        if (strcmp(s, "autotab") == 0 && part)        { part->auto_tab = 1; continue; }
        /* Sur « target » et non « part » : ces deux-là valent pour un champ,
         * une carte ou un fond, et target est justement la cible du bloc en
         * cours, quelle qu'elle soit. */
        if (strcmp(s, "dontsearch") == 0 && target)   { target->dont_search = 1; continue; }
        if (strcmp(s, "cantdelete") == 0 && target)   { target->cant_delete = 1; continue; }
        if (strcmp(s, "sharedtext") == 0 && part)     { part->shared_text = 1; continue; }
        if (strncmp(s, "textfont ", 9) == 0 && part) {
            free(part->textfont);
            part->textfont = dupstr_file(s + 9);
            desechappe(part->textfont);
            continue;
        }
        if (strncmp(s, "textstyle ", 10) == 0 && part) {
            part->textstyle = hc_entier(s + 10, 0, 0xFFFF, part->textstyle);
            continue;
        }
        if (strncmp(s, "id ", 3) == 0 && target) {
            hc_set_id(target, hc_id(s + 3));
            continue;
        }
        if (strncmp(s, "scroll ", 7) == 0 && part) {
            part->scroll = hc_entier(s + 7, 0, HC_COORD_MAX, part->scroll);
            continue;
        }
        if (strcmp(s, "hidename") == 0 && part) {
            part->showname = 0;
            continue;
        }
        if (strcmp(s, "disabled") == 0 && part) {
            part->enabled = 0;
            continue;
        }
        if (strcmp(s, "unsharedhilite") == 0 && part) {
            part->shared_hilite = 0;
            continue;
        }
        if (strncmp(s, "rect ", 5) == 0 && part) {
            /* LES QUATRE NOMBRES SONT BORNÉS AVANT D'ÊTRE SOUSTRAITS.
             *
             * « %d » accepte tout ce qui tient dans un int, et la soustraction
             * qui suit débordait : « rect -2147483648,0,2147483647,10 » donnait
             * une largeur de -1. Un fichier n'a pas besoin d'être malveillant
             * pour en arriver là — un enregistrement fait après un calcul qui a
             * dérapé suffit.
             *
             * On lit chaque nombre par hc_coord, qui borne à un million de
             * points : mille fois la largeur d'une carte, et assez loin du bord
             * d'un int pour que la soustraction ne puisse plus déborder. Un
             * nombre hors bornes vaut zéro, et le rectangle reste lisible. */
            char q[4][32];
            if (sscanf(s + 5, "%31[^,],%31[^,],%31[^,],%31s",
                       q[0], q[1], q[2], q[3]) == 4) {
                int a = hc_coord(q[0], 0), b = hc_coord(q[1], 0);
                int c = hc_coord(q[2], 0), d = hc_coord(q[3], 0);
                part->x = a; part->y = b; part->w = c - a; part->h = d - b;
            }
            continue;
        }
        if (strncmp(s, "style ", 6) == 0 && part) {
            get_quoted(s, 0, nm, sizeof nm);
            free(part->style);
            part->style = nm[0] ? dupstr_file(nm) : NULL;
            continue;
        }

        /* --- fermetures --- */
        if (strcmp(s, "end button") == 0 || strcmp(s, "end field") == 0) {
            part = NULL; target = owner; continue;
        }
        if (strcmp(s, "end card") == 0 || strcmp(s, "end background") == 0) {
            /* Fermer la carte ferme aussi la part restée ouverte.
             *
             * Un fichier écrit à la main peut omettre « end field » : la carte
             * se referme quand même, et ce n'est PAS une troncature — l'objet
             * a bien une fin, elle est seulement implicite. Sans cette remise
             * à zéro, le contrôle de fin de fichier voyait une part ouverte et
             * refusait un fichier parfaitement lisible. Une coupure réelle au
             * milieu d'une part, elle, laisse part ET owner ouverts, puisque
             * ni l'un ni l'autre « end » n'a été rencontré. */
            part = NULL;
            owner = NULL; target = stack; last_bgtext = -1; continue;
        }
        if (strcmp(s, "end stack") == 0) { target = NULL; continue; }
        if (strcmp(s, "end hc-file") == 0) { fin_vue = 1; continue; }
    }

    free(acc.buf);
    ligne_libere(&lg);
    fclose(f);

    /* TROIS FAÇONS DE LIRE UNE PILE INCOMPLÈTE, UN SEUL VERDICT.
     *
     * Une seule allocation manquée pendant la lecture suffit à refuser toute
     * la pile. C'est brutal, et c'est voulu : rendre une pile où un script ou
     * le texte d'un champ a silencieusement perdu sa fin, c'est offrir à
     * l'utilisateur de l'enregistrer par-dessus l'original.
     *
     * Même verdict quand c'est le LECTEUR DE LIGNES qui a buté : il rend
     * maintenant -1 pour un échec, là où il rendait 0 comme pour une fin de
     * fichier ordinaire. La boucle s'arrêtait alors au même endroit dans les
     * deux cas, et la pile tronquée passait pour complète.
     *
     * Et même verdict, enfin, pour une coupure PROPRE au milieu d'un bloc.
     * « script » ouvre un bloc que « end script » ferme ; idem pour contents,
     * paint, bgtextdata et iconres. Un fichier tranché là se lit sans la
     * moindre erreur : la boucle s'arrête sur une fin de fichier ordinaire, et
     * le texte accumulé — un script amputé de sa moitié, le contenu d'un champ
     * sans sa fin — n'est même jamais posé sur l'objet, faute du « end » qui
     * l'y pose. La pile s'ouvrait donc, l'air complète, avec un script VIDE là
     * où il y en avait un. C'était le seul des trois cas qui restait. */
    int bloc_ouvert = in_script || in_contents || in_paint || in_bgtext || in_icon;

    /* UN OBJET RESTÉ OUVERT EST UNE TRONCATURE, LUI AUSSI.
     *
     * Le test ci-dessus ne regarde que les blocs de texte. Un fichier coupé
     * ENTRE deux objets — après « rect 10,10,100,40 », avant « end button » —
     * n'a aucun bloc ouvert et passait donc pour complet, avec un bouton sans
     * fin et une carte sans fin.
     *
     * À une fin propre, part et owner sont tous deux nuls : « end button »
     * remet part à NULL, « end card » remet owner à NULL. S'ils ne le sont
     * pas, le fichier s'est arrêté au milieu de quelque chose. */
    int objet_ouvert = (part != NULL) || (owner != NULL);

    /* ET LA SIGNATURE DE FIN, pour les fichiers qui savent la porter.
     *
     * C'est la seule chose qui attrape une coupure sur une frontière PROPRE —
     * juste après un « end card » —, où le fichier est syntaxiquement
     * irréprochable et où il manque simplement toutes les cartes suivantes.
     *
     * Exigée de la version 2 seulement : les fichiers d'avant n'en ont pas,
     * et les refuser rendrait illisible tout ce qui a été enregistré jusqu'ici.
     * Ce cas-là reste donc indétectable sur un fichier v1, et c'est une raison
     * de plus pour que les piles repassent par une sauvegarde. */
    int signature_manque = (format_fichier >= 2) && !fin_vue;

    if (format_trop_recent)
        snprintf(g_load_erreur, sizeof g_load_erreur,
                 "Cette pile est au format %d ; cette version de HC ne connaît "
                 "que le format %d.", format_trop_recent, HC_FORMAT_MAX);
    else if (acc.manque)
        snprintf(g_load_erreur, sizeof g_load_erreur,
                 "Mémoire insuffisante pour lire cette pile en entier.");
    else if (lecture == LIGNE_ECHEC)
        snprintf(g_load_erreur, sizeof g_load_erreur,
                 "Erreur de lecture au milieu du fichier.");
    else if (bloc_ouvert || objet_ouvert || signature_manque)
        snprintf(g_load_erreur, sizeof g_load_erreur,
                 "Fichier incomplet : il s'arrête au milieu de la pile.");
    else if (icon_abimee)
        snprintf(g_load_erreur, sizeof g_load_erreur,
                 "Une icône de cette pile est incomplète ou abîmée.");

    if (acc.manque || lecture == LIGNE_ECHEC || bloc_ouvert ||
        objet_ouvert || signature_manque || format_trop_recent || icon_abimee) {
        if (stack) hc_free(stack);
        return NULL;
    }
    if (!stack)
        snprintf(g_load_erreur, sizeof g_load_erreur,
                 "Ce fichier ne contient pas de pile.");
    /* Le noyau apprend ICI où la pile habite — « the long name of this stack »
     * en a besoin, et c'est le seul endroit qui le sache à la lecture. */
    else hc_set_stack_path(stack, path);
    return stack;
}
