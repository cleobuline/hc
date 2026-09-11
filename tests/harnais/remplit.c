#include <stdio.h>
#include "hc_pixels.h"

#define W 12
#define H 8
static unsigned char img[H][W][4];
static const long BPR = W*4;

static void vide(void){ memset(img,0,sizeof img); }
static void montre(const char *t){
  printf("%s\n", t);
  for (int y=0;y<H;y++){
    printf("  ");
    for (int x=0;x<W;x++){
      unsigned char *p=img[y][x];
      if (p[3]==0)                          putchar('.');   /* rien */
      else if (p[0]>200&&p[1]>200&&p[2]>200) putchar(' ');   /* fond blanc */
      else if (p[0]>200&&p[1]<80&&p[2]<80)   putchar('R');   /* encre rouge */
      else if (p[0]<80&&p[1]<80&&p[2]<80)    putchar('#');   /* encre noire */
      else printf("%c", 'a'+(p[0]/32));                      /* mélange */
    }
    printf("   |");
    for (int x=0;x<W;x++) printf("%3d", img[y][x][3]);
    printf("\n");
  }
}

int main(void){
  /* damier 2x2 : 0xCC = 1100 1100 */
  unsigned char damier[8]={0xCC,0xCC,0x33,0x33,0xCC,0xCC,0x33,0x33};
  unsigned char plein [8]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

  puts("=== 1. trame damier, encre rouge opaque, fond blanc, zone (2,1)-(9,6) ===");
  vide();
  hcp_remplit(&img[0][0][0],BPR,4,W,H, 2,1,9,6, NULL,0, damier,
              255,0,0,255,  255,255,255, 0);
  montre("(R = encre, espace = fond, . = jamais touche)");

  puts("\n=== 2. la meme, fond transparent : la trame seule se pose ===");
  vide();
  hcp_remplit(&img[0][0][0],BPR,4,W,H, 2,1,9,6, NULL,0, damier,
              255,0,0,255,  255,255,255, 1);
  montre("(les creux de la trame restent vides)");

  puts("\n=== 3. lasso triangulaire, trame pleine, fond transparent ===");
  vide();
  double tri[6] = { 1,1,  10,1,  5.5,7 };
  hcp_remplit(&img[0][0][0],BPR,4,W,H, 0,0,W-1,H-1, tri,3, plein,
              0,0,0,255,  255,255,255, 1);
  montre("(rien hors du triangle)");

  puts("\n=== 4. encre a 50% sur du vide, puis un 2e remplissage : ca charge ===");
  vide();
  for (int n=1;n<=3;n++){
    hcp_remplit(&img[0][0][0],BPR,4,W,H, 0,0,3,0, NULL,0, plein,
                255,0,0,128,  255,255,255, 1);
    printf("  apres %d passe(s) : alpha=%3d  rvb=%d,%d,%d\n",
           n, img[0][0][3], img[0][0][0], img[0][0][1], img[0][0][2]);
  }

  puts("\n=== 5. zone hors image et zone inversee : rien, et pas de plantage ===");
  vide();
  hcp_remplit(&img[0][0][0],BPR,4,W,H, 50,50,60,60, NULL,0, plein, 0,0,0,255, 255,255,255, 1);
  hcp_remplit(&img[0][0][0],BPR,4,W,H, 9,6,2,1,     NULL,0, plein, 0,0,0,255, 255,255,255, 1);
  int poses=0; for(int y=0;y<H;y++)for(int x=0;x<W;x++) if(img[y][x][3]) poses++;
  printf("  zone hors image : rien pose. Zone inversee (9,6)->(2,1) : %d pixels\n", poses);
  printf("  (hcp_borne remet les bornes dans l'ordre : 6*8 = 48 attendus)\n");
  return 0;
}
