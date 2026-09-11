#include <stdio.h>
#include "hc_pixels.h"
int main(void){
  unsigned char px[4];
  printf("  Rouge a moitie opaque (128), pose sur du VIDE puis repasse :\n");
  px[0]=px[1]=px[2]=px[3]=0;
  for(int k=1;k<=4;k++){
    hcp_melange(px,4, 255,0,0, 128);
    printf("    passe %d : rvb=%3d,%3d,%3d  alpha=%3d  (%.0f%% d'opacite)\n",
           k,px[0],px[1],px[2],px[3], px[3]*100.0/255);
  }
  printf("\n  Bleu a moitie opaque pose sur du ROUGE opaque :\n");
  px[0]=255;px[1]=0;px[2]=0;px[3]=255;
  hcp_melange(px,4, 0,0,255, 128);
  printf("    rvb=%3d,%3d,%3d  alpha=%3d   (violet attendu)\n",px[0],px[1],px[2],px[3]);
  printf("\n  Alpha 255 : pose franche, sans melange :\n");
  px[0]=255;px[1]=0;px[2]=0;px[3]=255;
  hcp_melange(px,4, 0,255,0, 255);
  printf("    rvb=%3d,%3d,%3d  alpha=%3d   (vert pur attendu)\n",px[0],px[1],px[2],px[3]);
  printf("\n  Alpha 0 : ne touche a rien :\n");
  px[0]=12;px[1]=34;px[2]=56;px[3]=200;
  hcp_melange(px,4, 0,255,0, 0);
  printf("    rvb=%3d,%3d,%3d  alpha=%3d   (inchange attendu)\n",px[0],px[1],px[2],px[3]);
  return 0;}
