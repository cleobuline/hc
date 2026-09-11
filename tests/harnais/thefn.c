/* Le point délicat : « the maFonction » pour une fonction définie par la
 * pile. Elle est cherchée dans la hiérarchie d'objets AVANT le refus ; si le
 * refus passait devant, cette forme cesserait de marcher. */
#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ma_ligne(HcLineKind k,int d,const char *t){(void)d;
  if(k==HC_MSG)printf("   [msg] %s\n",t); else if(k==HC_ERR)printf("   [ERR] %s\n",t);}
int main(void){
  static HcHost h;memset(&h,0,sizeof h);h.line=ma_ligne;hc_set_host(&h);
  Object *st=hc_new_stack("T");Object *bg=hc_new_background(st,"F");
  Object *c=hc_new_card(st,bg,"Une");Object *b=hc_new_button(c,"B");
  hc_set_current_card(c);
  hc_set_script(st,
    "function maTemperature\n"
    "  return 37\n"
    "end maTemperature\n");
  hc_set_script(b,
    "on mouseUp\n"
    "  put \"1. the maTemperature -> \" & the maTemperature\n"
    "  put \"2. maTemperature()   -> \" & maTemperature()\n"
    "  put \"3. sans the          -> \" & maTemperature\n"
    "  put \"4. inconnue avec the :\"\n"
    "  put the maTemperatur\n"
    "end mouseUp\n");
  hc_send(b,"mouseUp"); hc_free(st); return 0;}
