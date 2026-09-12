#include <stdio.h>
#include "hc_pixels.h"

#define W 6
#define H 4
#define SPP 4
static unsigned char img[H][W][SPP];
static long bpr = W*SPP;

static void vide_tout(void){ memset(img,0,sizeof img); }
static void pose(int x,int y){ img[y][x][0]=img[y][x][1]=img[y][x][2]=0; img[y][x][3]=255; }
static void blanc(int x,int y){ img[y][x][0]=img[y][x][1]=img[y][x][2]=255; img[y][x][3]=255; }
static void montre(const char *titre){
  printf("%s\n", titre);
  for(int y=0;y<H;y++){ printf("      ");
    for(int x=0;x<W;x++){
      unsigned char *p=img[y][x];
      putchar(p[3]==0 ? '.' : (p[0]<128 ? '#' : 'o'));
    }
    putchar('\n');
  }
}
/* « depuis » : dessine une figure repérable, un L asymétrique */
static void figure(void){
  vide_tout();
  pose(1,1); pose(1,2); pose(2,2); pose(3,2);
}
int main(void){
  printf("  . = vide   # = encre   o = blanc opaque\n\n");

  figure(); montre("figure de depart (un L)");

  figure(); hcp_invert((unsigned char*)img,bpr,SPP,W,H, 1,1,3,2, NULL,0);
  montre("\ninvert sur x1..3 y1..2  (le vide devient encre, l'encre devient blanc)");

  figure(); hcp_flip((unsigned char*)img,bpr,SPP,W,H, 1,1,3,2, 1);
  montre("\nflip horizontal sur x1..3 y1..2");

  figure(); hcp_flip((unsigned char*)img,bpr,SPP,W,H, 1,1,3,2, 0);
  montre("\nflip vertical sur x1..3 y1..2");

  {int a,b,c,d; figure();
   hcp_rotate((unsigned char*)img,bpr,SPP,W,H, 1,1,3,2, +1, &a,&b,&c,&d);
   char t[80]; snprintf(t,sizeof t,"\nrotate DROITE de x1..3 y1..2  ->  nouveau rect x%d..%d y%d..%d",a,c,b,d);
   montre(t);}

  {int a,b,c,d; figure();
   hcp_rotate((unsigned char*)img,bpr,SPP,W,H, 1,1,3,2, -1, &a,&b,&c,&d);
   char t[80]; snprintf(t,sizeof t,"\nrotate GAUCHE de x1..3 y1..2  ->  nouveau rect x%d..%d y%d..%d",a,c,b,d);
   montre(t);}

  figure(); hcp_trace_edges((unsigned char*)img,bpr,SPP,W,H, 0,0,W-1,H-1, NULL,0);
  montre("\ntrace edges sur toute l'image (le L s'efface, son contour reste)");

  vide_tout(); blanc(2,1); blanc(3,1);
  hcp_invert((unsigned char*)img,bpr,SPP,W,H, 2,1,3,1, NULL,0);
  montre("\ninvert sur deux pixels BLANCS opaques (doivent devenir encre)");

  /* Darken et Lighten : on ne peut pas prédire QUELS pixels, mais on peut
     vérifier que la proportion est la bonne et qu'ils restent dans la zone. */
  {
    int poses=0, hors=0;
    vide_tout();
    for(int k=0;k<200;k++) hcp_darken((unsigned char*)img,bpr,SPP,W,H, 1,1,3,2, NULL,0);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
      int dedans = (x>=1&&x<=3&&y>=1&&y<=2);
      if(img[y][x][3]) { poses++; if(!dedans) hors++; }
    }
    printf("\ndarken x200 sur x1..3 y1..2 : %d pixels poses sur 6 possibles, %d hors zone\n", poses, hors);
    for(int k=0;k<200;k++) hcp_lighten((unsigned char*)img,bpr,SPP,W,H, 1,1,3,2, NULL,0);
    poses=0; for(int y=0;y<H;y++) for(int x=0;x<W;x++) if(img[y][x][3]) poses++;
    printf("lighten x200 ensuite            : %d pixels restants (0 attendu)\n", poses);
  }
  return 0;
}
