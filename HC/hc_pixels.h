#ifndef hc_pixels_h
#define hc_pixels_h

/* ═══ LES TRANSFORMATIONS DU MENU PAINT, EN C PUR ═══════════════════════
 *
 * Invert, Darken, Lighten, Trace Edges, Flip, Rotate. Ce sont les articles
 * du menu Paint d'HyperCard qui ne font qu'une chose : relire des pixels et
 * en écrire d'autres.
 *
 * POURQUOI UN FICHIER À PART, ET INCLUS.
 *
 * graphics.m est de l'Objective-C : il importe AppKit, et rien de ce qui le
 * touche ne peut être compilé ni essayé ailleurs que sur un Mac. Or l'erreur
 * qui guette ici n'est pas une erreur d'interface — c'est un indice de
 * travers dans une rotation, un décalage d'une ligne dans un miroir. Ce sont
 * les bogues les plus faciles à écrire et les plus pénibles à voir.
 *
 * Le calcul est donc isolé dans du C99 qui ne connaît ni NSBitmapImageRep ni
 * NSPoint : rien qu'un tampon, son pas de ligne et son nombre de composantes.
 * graphics.m l'inclut et se contente de traduire. Les fonctions sont
 * `static` et le fichier n'a qu'un seul incluant réel ; un banc d'essai peut
 * l'inclure à son tour et vérifier chaque transformation sur des images de
 * quelques pixels, ce qu'aucun test à l'écran ne ferait aussi bien.
 *
 * LE MODÈLE DE PIXEL. Le calque est RVBA, transparent là où rien n'a été
 * peint. « Posé » veut donc dire alpha non nul, et c'est le seul test
 * d'encre : sur un calque en couleur, un pixel opaque est un pixel peint,
 * quelle que soit sa teinte.
 *
 * Le blanc du papier est ce qu'on voit à travers un pixel transparent. C'est
 * pourquoi Invert noircit ce qui est vide : inverser du blanc donne du noir,
 * et c'est bien ce que fait HyperCard sur une sélection vierge. */

#include <stdlib.h>
#include <string.h>

/* Un pixel du tampon. */
#define HCP_PX(data, bpr, spp, x, y)  ((data) + (long)(y) * (bpr) + (long)(x) * (spp))

/* Est-il posé ? Sans canal alpha, tout l'est. */
static int hcp_pose(const unsigned char *px, int spp)
{
    return spp < 4 || px[3] != 0;
}

static void hcp_encre(unsigned char *px, int spp)
{
    px[0] = 0; px[1] = 0; px[2] = 0;
    if (spp >= 4) px[3] = 255;
}

static void hcp_vide(unsigned char *px, int spp)
{
    px[0] = 0; px[1] = 0; px[2] = 0;
    if (spp >= 4) px[3] = 0;
}

/* ═══ Composition d'une encre semi-opaque ═══════════════════════════════
 *
 * « set the paintColor to "255,0,0,128" » : du rouge à moitié transparent.
 *
 * HyperCard ne savait pas faire cela — il n'avait ni couleur ni canal alpha.
 * Le calque de HC, lui, est RVBA depuis toujours : il ne manquait qu'un
 * mélange, et le fait que la couleur d'encre porte son opacité.
 *
 * C'est la composition « source par-dessus » habituelle. Ce qui compte, et ce
 * qui la distingue d'une simple pose d'alpha, c'est que les passages
 * S'ACCUMULENT : repasser deux fois avec une encre à moitié opaque donne
 * trois quarts d'opacité, exactement comme un lavis. Poser l'alpha tel quel
 * donnerait toujours la moitié, et un pinceau qui ne charge jamais.
 *
 * Tout en entiers : le calcul se fait des millions de fois par tracé, et l'on
 * ne va pas convertir en flottant pour arrondir aussitôt. */
static void hcp_melange(unsigned char *px, int spp,
                        unsigned char r, unsigned char g, unsigned char b,
                        int sa)
{
    if (sa <= 0) return;                    /* rien à poser */
    if (sa >= 255 || spp < 4) {             /* opaque, ou sans canal alpha */
        px[0] = r; px[1] = g; px[2] = b;
        if (spp >= 4) px[3] = 255;
        return;
    }

    int da = px[3];
    int reste = da * (255 - sa) / 255;      /* ce que la destination garde */
    int oa = sa + reste;
    if (oa <= 0) { hcp_vide(px, spp); return; }

    px[0] = (unsigned char)((r * sa + px[0] * reste) / oa);
    px[1] = (unsigned char)((g * sa + px[1] * reste) / oa);
    px[2] = (unsigned char)((b * sa + px[2] * reste) / oa);
    px[3] = (unsigned char)oa;
}

/* Le point (x,y) est-il dans le polygone ? Lancer de rayon horizontal, comme
 * fill_freeform — même méthode, pour que le lasso délimite exactement la même
 * chose quel que soit l'outil qui s'en sert.
 *
 * Les sommets sont donnés à plat : px[2*i], px[2*i+1]. NSPoint ne franchit
 * pas cette frontière, c'est tout l'intérêt. */
static int hcp_dans_poly(const double *poly, int n, int x, int y)
{
    if (!poly || n < 3) return 1;              /* pas de lasso : tout compte */
    int dedans = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = poly[2*i], yi = poly[2*i+1];
        double xj = poly[2*j], yj = poly[2*j+1];
        if (((yi > y) != (yj > y)) &&
            (x < (xj - xi) * (y - yi) / (yj - yi) + xi))
            dedans = !dedans;
    }
    return dedans;
}

/* Ramener la zone dans l'image. Rend 0 s'il n'en reste rien. */
static int hcp_borne(int W, int H, int *x0, int *y0, int *x1, int *y1)
{
    if (*x0 > *x1) { int t = *x0; *x0 = *x1; *x1 = t; }
    if (*y0 > *y1) { int t = *y0; *y0 = *y1; *y1 = t; }
    if (*x0 < 0) *x0 = 0;
    if (*y0 < 0) *y0 = 0;
    if (*x1 > W - 1) *x1 = W - 1;
    if (*y1 > H - 1) *y1 = H - 1;
    return (*x1 >= *x0 && *y1 >= *y0);
}

/* ---- Invert : le blanc devient noir, et le vide aussi ---- */
static void hcp_invert(unsigned char *data, long bpr, int spp, int W, int H,
                       int x0, int y0, int x1, int y1,
                       const double *poly, int npoly)
{
    if (!data || !hcp_borne(W, H, &x0, &y0, &x1, &y1)) return;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (!hcp_dans_poly(poly, npoly, x, y)) continue;
            unsigned char *px = HCP_PX(data, bpr, spp, x, y);
            if (!hcp_pose(px, spp)) { hcp_encre(px, spp); continue; }
            px[0] = 255 - px[0];
            px[1] = 255 - px[1];
            px[2] = 255 - px[2];
        }
}

/* ---- Darken / Lighten : le grain d'HyperCard ----
 *
 * Ce ne sont pas des filtres de luminosité : ils SÈMENT. Darken ajoute des
 * points d'encre au hasard, Lighten en retire. Repasser plusieurs fois
 * assombrit ou éclaircit progressivement, et c'est ce grain aléatoire qui
 * fait leur allure — un filtre régulier n'y ressemblerait pas du tout.
 *
 * Un pixel sur huit, ce qui demande huit passes pour couvrir une zone : la
 * même progression qu'à l'époque. */
static void hcp_darken(unsigned char *data, long bpr, int spp, int W, int H,
                       int x0, int y0, int x1, int y1,
                       const double *poly, int npoly)
{
    if (!data || !hcp_borne(W, H, &x0, &y0, &x1, &y1)) return;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (rand() % 8) continue;
            if (!hcp_dans_poly(poly, npoly, x, y)) continue;
            hcp_encre(HCP_PX(data, bpr, spp, x, y), spp);
        }
}

static void hcp_lighten(unsigned char *data, long bpr, int spp, int W, int H,
                        int x0, int y0, int x1, int y1,
                        const double *poly, int npoly)
{
    if (!data || !hcp_borne(W, H, &x0, &y0, &x1, &y1)) return;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (rand() % 8) continue;
            if (!hcp_dans_poly(poly, npoly, x, y)) continue;
            hcp_vide(HCP_PX(data, bpr, spp, x, y), spp);
        }
}

/* ---- Trace Edges : le contour, et rien d'autre ----
 *
 * Un pixel VIDE qui touche un pixel posé devient de l'encre ; tout ce qui
 * était posé s'efface. Il reste le liseré extérieur des formes — ce que fait
 * HyperCard, et la raison pour laquelle un trait fin passé à Trace Edges
 * devient deux traits.
 *
 * Deux passes obligatoires : décider d'après l'image d'ORIGINE. En un seul
 * parcours, les pixels déjà transformés serviraient de voisinage aux
 * suivants, et le contour s'épaissirait en avançant vers la droite. Le
 * voisinage est lu dans toute l'image, pas seulement dans la zone : une forme
 * qui déborde de la sélection garde un contour juste au bord. */
static void hcp_trace_edges(unsigned char *data, long bpr, int spp, int W, int H,
                            int x0, int y0, int x1, int y1,
                            const double *poly, int npoly)
{
    if (!data || !hcp_borne(W, H, &x0, &y0, &x1, &y1)) return;
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    unsigned char *neuf = calloc((size_t)w * h, 1);   /* 1 encre, 2 vide */
    if (!neuf) return;

    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (!hcp_dans_poly(poly, npoly, x, y)) continue;
            unsigned char *px = HCP_PX(data, bpr, spp, x, y);
            if (hcp_pose(px, spp)) { neuf[(y-y0)*w + (x-x0)] = 2; continue; }

            int touche = 0;
            static const int dx[4] = { -1, 1, 0, 0 };
            static const int dy[4] = { 0, 0, -1, 1 };
            for (int k = 0; k < 4 && !touche; k++) {
                int vx = x + dx[k], vy = y + dy[k];
                if (vx < 0 || vy < 0 || vx >= W || vy >= H) continue;
                if (hcp_pose(HCP_PX(data, bpr, spp, vx, vy), spp)) touche = 1;
            }
            if (touche) neuf[(y-y0)*w + (x-x0)] = 1;
        }

    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            unsigned char q = neuf[(y-y0)*w + (x-x0)];
            if (q == 1) hcp_encre(HCP_PX(data, bpr, spp, x, y), spp);
            else if (q == 2) hcp_vide(HCP_PX(data, bpr, spp, x, y), spp);
        }
    free(neuf);
}

/* ---- Flip : miroir dans le rectangle ----
 *
 * Sur la boîte englobante, pas sur le polygone du lasso. Retourner une forme
 * quelconque « sur elle-même » n'a pas de sens géométrique, et HyperCard ne
 * le prétend pas non plus : il retourne le rectangle de la sélection.
 *
 * Échange sur place, sans tampon : une colonne (ou une ligne) contre son
 * symétrique, en s'arrêtant au milieu. */
static void hcp_flip(unsigned char *data, long bpr, int spp, int W, int H,
                     int x0, int y0, int x1, int y1, int horizontal)
{
    if (!data || !hcp_borne(W, H, &x0, &y0, &x1, &y1)) return;

    if (horizontal) {
        for (int y = y0; y <= y1; y++)
            for (int xa = x0, xb = x1; xa < xb; xa++, xb--) {
                unsigned char *pa = HCP_PX(data, bpr, spp, xa, y);
                unsigned char *pb = HCP_PX(data, bpr, spp, xb, y);
                for (int k = 0; k < spp; k++) {
                    unsigned char t = pa[k]; pa[k] = pb[k]; pb[k] = t;
                }
            }
        return;
    }
    for (int x = x0; x <= x1; x++)
        for (int ya = y0, yb = y1; ya < yb; ya++, yb--) {
            unsigned char *pa = HCP_PX(data, bpr, spp, x, ya);
            unsigned char *pb = HCP_PX(data, bpr, spp, x, yb);
            for (int k = 0; k < spp; k++) {
                unsigned char t = pa[k]; pa[k] = pb[k]; pb[k] = t;
            }
        }
}

/* ---- Rotate : un quart de tour, autour du centre ----
 *
 * `sens` vaut +1 pour la droite (sens des aiguilles), -1 pour la gauche.
 *
 * Une sélection qui n'est pas carrée change de forme : ses côtés
 * s'échangent. Le nouveau rectangle est donc centré au même endroit, mais
 * large de ce qui était haut. `nx0..ny1` le rendent à l'appelant, qui doit
 * déplacer la sélection en conséquence — sans quoi les fourmis entoureraient
 * une zone qui n'a plus rien à voir avec l'image.
 *
 * Le bloc d'origine est copié AVANT que quoi que ce soit ne bouge : ancien et
 * nouveau rectangles se recouvrent, et écrire dans le second en lisant le
 * premier donnerait une bouillie. */
static void hcp_rotate(unsigned char *data, long bpr, int spp, int W, int H,
                       int x0, int y0, int x1, int y1, int sens,
                       int *nx0, int *ny0, int *nx1, int *ny1)
{
    if (nx0) { *nx0 = x0; *ny0 = y0; *nx1 = x1; *ny1 = y1; }
    if (!data || !hcp_borne(W, H, &x0, &y0, &x1, &y1)) return;

    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    unsigned char *src = malloc((size_t)w * h * spp);
    if (!src) return;
    for (int y = 0; y < h; y++)
        memcpy(src + (size_t)y * w * spp,
               HCP_PX(data, bpr, spp, x0, y0 + y), (size_t)w * spp);

    /* Le rectangle d'arrivée : côtés échangés, même centre. Les centres se
     * calculent en doubles de pixel pour que les tailles impaires ne se
     * décalent pas d'un demi-pixel à chaque rotation. */
    int cx2 = x0 + x1, cy2 = y0 + y1;          /* deux fois le centre */
    int nw = h, nh = w;
    int dx0 = (cx2 - (nw - 1)) / 2;
    int dy0 = (cy2 - (nh - 1)) / 2;

    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            hcp_vide(HCP_PX(data, bpr, spp, x, y), spp);

    for (int dy = 0; dy < nh; dy++)
        for (int dx = 0; dx < nw; dx++) {
            /* D(dx,dy) vient de S(sx,sy).
             * Droite  : dx = h-1-sy, dy = sx   →  sx = dy,      sy = h-1-dx
             * Gauche  : dx = sy,     dy = w-1-sx →  sx = w-1-dy, sy = dx   */
            int sx = (sens >= 0) ? dy : (w - 1 - dy);
            int sy = (sens >= 0) ? (h - 1 - dx) : dx;
            if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;

            int gx = dx0 + dx, gy = dy0 + dy;
            if (gx < 0 || gy < 0 || gx >= W || gy >= H) continue;

            memcpy(HCP_PX(data, bpr, spp, gx, gy),
                   src + ((size_t)sy * w + sx) * spp, (size_t)spp);
        }
    free(src);

    if (nx0) { *nx0 = dx0; *ny0 = dy0; *nx1 = dx0 + nw - 1; *ny1 = dy0 + nh - 1; }
}

/* ---- Fill : la zone, remplie de la trame courante ----
 *
 * Le septième article, et le seul qui PEINT au lieu de transformer ce qui
 * est déjà là. Il tient quand même ici, pour la même raison que les six
 * autres : c'est du calcul par pixel, et un décalage d'une colonne dans la
 * trame ne se voit pas à l'œil sur un écran.
 *
 * La trame arrive en huit octets — une ligne par octet, bit de poids fort à
 * gauche —, pas en numéro : hc_pixels.h ne connaît pas le catalogue des
 * trames, qui vit dans graphics.m avec le reste de l'habillage. Les huit
 * octets sont tout ce dont le calcul a besoin.
 *
 * L'encre porte son opacité, comme partout ailleurs : `ia` < 255 donne un
 * lavis qui charge si l'on remplit deux fois. Le fond, lui, est franc — un
 * fond à demi transparent n'est pas un fond.
 *
 * `fond_transparent` laisse intact ce que la trame ne couvre pas, au lieu
 * d'y poser le fond. C'est le même réglage que pour les formes pleines, et
 * il doit s'y comporter pareil : remplir en trame 25 % par-dessus un dessin
 * doit le voiler, pas l'effacer. */
static void hcp_remplit(unsigned char *data, long bpr, int spp, int W, int H,
                        int x0, int y0, int x1, int y1,
                        const double *poly, int npoly,
                        const unsigned char motif[8],
                        unsigned char ir, unsigned char ig, unsigned char ib,
                        int ia,
                        unsigned char br, unsigned char bg, unsigned char bb,
                        int fond_transparent)
{
    if (!data || !motif) return;
    if (!hcp_borne(W, H, &x0, &y0, &x1, &y1)) return;

    for (int y = y0; y <= y1; y++) {
        unsigned char ligne = motif[y & 7];
        for (int x = x0; x <= x1; x++) {
            if (!hcp_dans_poly(poly, npoly, x, y)) continue;
            unsigned char *px = HCP_PX(data, bpr, spp, x, y);

            if ((ligne >> (7 - (x & 7))) & 1) {
                if (ia >= 255) {
                    px[0] = ir; px[1] = ig; px[2] = ib;
                    if (spp >= 4) px[3] = 255;
                } else {
                    hcp_melange(px, spp, ir, ig, ib, ia);
                }
            } else if (!fond_transparent) {
                px[0] = br; px[1] = bg; px[2] = bb;
                if (spp >= 4) px[3] = 255;
            }
        }
    }
}

#endif /* hc_pixels_h */
