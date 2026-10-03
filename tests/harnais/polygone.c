/* L'EXTENSION DE HC : les couleurs et le polygone d'un bouton.
 *
 * La seule extension annoncée (CLAUDE.md), décidée le 2 octobre pour le
 * flipper de l'utilisatrice. Le vocabulaire est celui de LiveCode : style
 * « polygon », « the points », backColor / foreColor / hiliteColor, et les
 * fonctions within() et intersect().
 *
 * Ce harnais tient trois promesses :
 *   1. un bouton qui ne porte rien rend VIDE — une pile d'HyperCard ne
 *      change pas ;
 *   2. ce qu'on pose se relit, se copie, se sauve et se relit du fichier ;
 *   3. la forme est juste : dedans, dehors, sur le bord, et le contact de
 *      deux formes. */
#include "hc_core.h"
#include "hc_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>

static void ligne(HcLineKind k, int d, const char *t)
{
    (void)d;
    if (k == HC_MSG)      printf("      %s\n", t);
    else if (k == HC_ERR) printf("      [ERR] %s\n", t);
}

static Object *B;

/* Un hôte qui tient la couleur de PEINTURE, pour vérifier que « the
 * backColor » sans « of » lui va toujours, et pas au bouton. */
static char g_peinture[64] = "0,0,0";
static const char *glob_lit(const char *nom)
{
    return strcasecmp(nom, "backColor") == 0 ? g_peinture : NULL;
}
static void glob_pose(const char *nom, const char *val)
{
    if (strcasecmp(nom, "backColor") == 0) {
        snprintf(g_peinture, sizeof g_peinture, "%s", val);
        printf("      [HÔTE] couleur de peinture <- %s\n", val);
    }
}

static void joue(const char *ligne_script)
{
    char s[2048];
    printf("   %s\n", ligne_script);
    snprintf(s, sizeof s, "on t\n%s\nend t\n", ligne_script);
    hc_set_script(B, s);
    hc_send(B, "t");
}

int main(void)
{
    static HcHost h;
    memset(&h, 0, sizeof h);
    h.line = ligne;
    h.global_get = glob_lit;
    h.global_set = glob_pose;
    hc_set_host(&h);

    Object *st = hc_new_stack("T");
    Object *bg = hc_new_background(st, "F");
    Object *c  = hc_new_card(st, bg, "C");
    Object *aile = hc_new_button(c, "aile");
    Object *bille = hc_new_button(c, "bille");
    B = hc_new_button(c, "script");
    hc_set_current_card(c);

    puts("== 1. un bouton qui ne porte rien : tout est vide ==");
    joue("put \"[\" & the points of button \"aile\" & \"]\"");
    joue("put \"[\" & the backColor of button \"aile\" & \"]\"");
    joue("put \"[\" & the foreColor of button \"aile\" & \"]\"");
    joue("put \"[\" & the hiliteColor of button \"aile\" & \"]\"");
    printf("   champs du noyau : backcolor %d forecolor %d hilitecolor %d npoints %d\n",
           aile->backcolor, aile->forecolor, aile->hilitecolor, aile->npoints);

    puts("\n== 2. le polygone : un triangle ==");
    joue("set the style of button \"aile\" to polygon");
    joue("set the points of button \"aile\" to \"100,200\" & return & \"160,190\" & return & \"150,215\"");
    joue("put the points of button \"aile\"");
    joue("put the rect of button \"aile\"");
    joue("set the loc of button \"aile\" to 230,302");
    joue("put the points of button \"aile\"");
    joue("set the width of button \"aile\" to 120");
    joue("put the points of button \"aile\"");
    joue("set the points of button \"aile\" to \"10,20abc\"");
    joue("set the points of button \"aile\" to \"10,20\"");
    joue("put the points of button \"aile\"");
    joue("set the points of button \"aile\" to \"100,200,160,190,150,215\"");
    joue("put the points of button \"aile\"");

    puts("\n== 3. les couleurs ==");
    joue("set the backColor of button \"aile\" to \"red\"");
    joue("set the foreColor of button \"aile\" to \"#0000FF\"");
    joue("set the hiliteColor of button \"aile\" to \"255,255,0\"");
    joue("put the backColor of button \"aile\" && the foreColor of button \"aile\" && the hiliteColor of button \"aile\"");
    joue("set the backColor of button \"aile\" to \"zorglub\"");
    joue("put the backColor of button \"aile\"");
    joue("set the backColor of button \"aile\" to \"noir\"");
    joue("put the backColor of button \"aile\"");
    joue("set the hiliteColor of button \"aile\" to empty");
    joue("put \"[\" & the hiliteColor of button \"aile\" & \"]\"");

    puts("\n== 3b. « the backColor » sans « of » reste la peinture ==");
    joue("set the backColor to \"green\"");
    joue("put the backColor");
    joue("put the backColor of button \"aile\"");

    puts("\n== 4. within : dedans, dehors, sur le bord ==");
    joue("put within(button \"aile\", \"130,200\")");
    joue("put within(button \"aile\", \"105,205\")");
    joue("put within(button \"aile\", \"100,200\")");
    joue("put within(button \"aile\", \"155,210\")");
    joue("put within(button \"aile\", \"99,200\")");
    joue("put within(button \"aile\", \"zut\")");

    puts("\n== 5. intersect : la bille contre l'aile ==");
    joue("set the style of button \"bille\" to oval");
    joue("set the rect of button \"bille\" to 120,170,136,186");
    joue("put intersect(button \"bille\", button \"aile\")");
    joue("set the rect of button \"bille\" to 125,190,141,206");
    joue("put intersect(button \"bille\", button \"aile\")");
    joue("set the rect of button \"bille\" to 300,300,316,316");
    joue("put intersect(button \"bille\", button \"aile\")");
    joue("put intersect(\"card button id \" & the id of button \"bille\", button \"aile\")");
    joue("put intersect(button \"bille\", \"rien\")");
    joue("-- un rectangle tout entier dans l'autre\n set the rect of button \"bille\" to 0,0,500,500\n put intersect(button \"bille\", button \"aile\")");

    puts("\n== 6. le copier-coller emporte tout ==");
    hc_copy_part(aile);
    Object *copie = hc_paste_part(c);
    if (copie) {
        printf("   copie : backcolor %06X forecolor %06X npoints %d, sommets %s\n",
               HC_COUL_RVB(copie->backcolor), HC_COUL_RVB(copie->forecolor),
               copie->npoints, copie->points != aile->points ? "distincts" : "PARTAGÉS");
    }

    puts("\n== 7. sauvé, puis relu ==");
    const char *chemin = "/tmp/hc_polygone_test.stack";
    hc_save(st, chemin);
    Object *relue = hc_load(chemin);
    if (relue) {
        Object *rc = NULL;
        for (int i = 0; i < relue->nparts; i++)
            if (relue->parts[i]->type == OBJ_CARD) { rc = relue->parts[i]; break; }
        Object *ra = rc ? rc->parts[0] : NULL;
        if (ra) {
            printf("   relu : style %s, backcolor %06X (%s), forecolor %06X, hilitecolor %d, %d sommets\n",
                   ra->style ? ra->style : "-", HC_COUL_RVB(ra->backcolor),
                   ra->backcolor ? "posée" : "aucune", HC_COUL_RVB(ra->forecolor),
                   ra->hilitecolor, ra->npoints);
            char pts[256];
            int xy[16];
            int n = hc_bouton_sommets(ra, xy, 8);
            pts[0] = '\0';
            for (int i = 0; i < n; i++) {
                char u[32];
                snprintf(u, sizeof u, "%s%d,%d", i ? " " : "", xy[2*i], xy[2*i+1]);
                strcat(pts, u);
            }
            printf("   relu, sommets : %s\n", pts);
            Object *rb = rc->parts[1];
            printf("   le bouton qui ne portait rien : backcolor %d, npoints %d, points %s\n",
                   rb->backcolor, rb->npoints, rb->points ? "NON NUL" : "nul");
        }
        hc_free(relue);
    }
    remove(chemin);

    puts("\n== 7b. l'opacité : « r,v,b,a », comme pour la peinture ==");
    joue("set the backColor of button \"bille\" to \"255,0,0,128\"");
    joue("put the backColor of button \"bille\"");
    joue("set the hiliteColor of button \"bille\" to \"0,255,0,0\"");
    joue("put the hiliteColor of button \"bille\"");
    joue("set the backColor of button \"bille\" to \"255,0,0,255\"");
    joue("put the backColor of button \"bille\" & \"  (255 : opaque, trois nombres)\"");
    joue("set the backColor of button \"bille\" to \"bleu\"");
    joue("put the backColor of button \"bille\" & \"  (un nom : opaque)\"");
    joue("set the foreColor of button \"bille\" to \"10,20,30,40\"");
    joue("set the foreColor of button \"bille\" to empty");
    joue("put \"[\" & the foreColor of button \"bille\" & \"]\"");
    printf("   champs : forealpha %d (effacé avec sa couleur)\n", bille->forealpha);
    joue("set the backColor of button \"bille\" to \"255,0,0,128\"");
    {
        /* sauvé, relu : l'opacité sur sa propre ligne */
        hc_save(st, chemin);
        FILE *f = fopen(chemin, "rb");
        char l[256];
        while (f && fgets(l, sizeof l, f))
            if (!strncmp(l, "backalpha", 9) || !strncmp(l, "hilitealpha", 11) ||
                !strncmp(l, "forealpha", 9))
                printf("   dans le fichier : %s", l);
        if (f) fclose(f);
        Object *re = hc_load(chemin);
        Object *rb = NULL;
        for (int i = 0; re && i < re->nparts; i++)
            if (re->parts[i]->type == OBJ_CARD) {
                for (int k = 0; k < re->parts[i]->nparts; k++)
                    if (re->parts[i]->parts[k]->name &&
                        !strcmp(re->parts[i]->parts[k]->name, "bille"))
                        rb = re->parts[i]->parts[k];
                break;
            }
        if (rb) printf("   relu : backalpha %d, hilitealpha %d\n",
                       HC_ALPHA(rb->backalpha), HC_ALPHA(rb->hilitealpha));
        hc_free(re);
        remove(chemin);
    }

    puts("\n== 8. un fichier abîmé : la ligne fautive est ignorée ==");
    /* « backcolor zz » se relisait en NOIR posé : hc_entier rendait son
     * défaut, 0, pris pour une couleur. Mesuré le 2 octobre. */
    {
        static const char *LIGNES[] = {
            "backcolor zz", "backcolor -5", "hilitecolor ",
            "points 50,40 0,0 1", "points 1,1", "points 50,40 0,0 1,1 x",
            "points -5,40 0,0 1,1", "backalpha 255", "backalpha zz",
            "backalpha -3", "hilitealpha 999", NULL
        };
        for (int i = 0; LIGNES[i]; i++) {
            Object *s2 = hc_new_stack("A");
            Object *b2 = hc_new_background(s2, "F");
            Object *c2 = hc_new_card(s2, b2, "C");
            Object *x  = hc_new_button(c2, "x");
            (void)x;
            hc_save(s2, chemin);
            hc_free(s2);
            /* La ligne abîmée, ajoutée juste avant « end button ». */
            FILE *f = fopen(chemin, "rb");
            static char tout[65536];
            size_t n = f ? fread(tout, 1, sizeof tout - 1, f) : 0;
            if (f) fclose(f);
            tout[n] = '\0';
            char *fin = strstr(tout, "end button");
            if (!fin) { puts("   (pas de bouton écrit)"); continue; }
            f = fopen(chemin, "wb");
            if (!f) continue;
            fwrite(tout, 1, (size_t)(fin - tout), f);
            fprintf(f, "%s\n%s", LIGNES[i], fin);
            fclose(f);
            Object *r = hc_load(chemin);
            Object *rb = NULL;
            for (int k = 0; r && k < r->nparts; k++)
                if (r->parts[k]->type == OBJ_CARD && r->parts[k]->nparts)
                    rb = r->parts[k]->parts[0];
            printf("   %-28s -> %s\n", LIGNES[i],
                   !rb ? "pile illisible" :
                   (rb->backcolor || rb->forecolor || rb->hilitecolor ||
                    rb->backalpha || rb->forealpha || rb->hilitealpha ||
                    rb->points || rb->npoints) ? "*** QUELQUE CHOSE A ÉTÉ POSÉ ***"
                                               : "ignorée, rien de posé");
            hc_free(r);
        }
        remove(chemin);
    }

    hc_free(st);
    return 0;
}
