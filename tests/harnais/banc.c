#include "hc_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static long g_clicks = 0, g_drags = 0;
static const char *mon_answer(const char *p, const char *b1, const char *b2, const char *b3)
{ (void)p;(void)b1;(void)b2;(void)b3; return "No"; }   /* pas d'effacement */
static const char *mon_global(const char *n)
{
    if (!strcasecmp(n,"optionKey")||!strcasecmp(n,"shiftKey")||!strcasecmp(n,"commandKey")) return "up";
    if (!strcasecmp(n,"tool")) return "browse tool";
    return NULL;
}
static void mon_choose(const char *n){ (void)n; }
static void mon_click(int x,int y,const char *m){ (void)x;(void)y;(void)m; g_clicks++; }
static void mon_drag(int a,int b,int c,int d,const char *m){ (void)a;(void)b;(void)c;(void)d;(void)m; g_drags++; }
static void mon_menu(const char *i){ (void)i; }
static void ligne(HcLineKind k,int d,const char *t){ (void)d; if (k==HC_ERR||k==HC_MSG) printf("   %s\n", t); }

static char *lire(const char *c){FILE*f=fopen(c,"rb");if(!f){perror(c);exit(1);}
 fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);char*b=malloc((size_t)n+1);
 fread(b,1,(size_t)n,f);b[n]=0;fclose(f);return b;}

static void champ(Object *o, const char *nom, const char *val)
{ Object *f = hc_new_field(o, nom); hc_set_field_text(f, val); }

int main(int argc, char **argv)
{
    HcHost h; memset(&h,0,sizeof h);
    h.line=ligne; h.answer=mon_answer; h.global_get=mon_global;
    h.choose_tool=mon_choose; h.click_at=mon_click; h.drag=mon_drag; h.do_menu=mon_menu;
    hc_set_host(&h);

    Object *stack = hc_new_stack("surfaces");
    Object *bg    = hc_new_background(stack, "Fond");
    Object *card  = hc_new_card(stack, bg, "Une");
    Object *btn   = hc_new_button(card, "Draw graph");
    hc_set_current_card(card);

    /* les champs du tore, tels qu'ils sont dans la copie d'ecran */
    champ(card, "xEq", "2*cos(u)+cos(v)*cos(u)");
    champ(card, "yEq", "2*sin(u)+cos(v)*sin(u)");
    champ(card, "zEq", "sin(v)");
    champ(card, "StartU", "0");    champ(card, "EndU", "2*pi+.1");
    champ(card, "StartV", "0");    champ(card, "EndV", "2*pi+.1");
    champ(card, "IncremU", "pi/16"); champ(card, "IncremV", "pi/16");
    champ(card, "MaxX", "4"); champ(card, "MaxY", "4"); champ(card, "MaxZ", "4");
    champ(card, "XMax", ""); champ(card, "YMax", ""); champ(card, "ZMax", "");
    champ(card, "StpdU", ""); champ(card, "StpdV", "");
    Object *b1 = hc_new_button(bg, "Connect u");  b1->hilite = 1;
    Object *b2 = hc_new_button(bg, "Connect v");  b2->hilite = 1;
    Object *b3 = hc_new_button(bg, "\xe2\x80\x9c" "Depth" "\xe2\x80\x9d"); b3->hilite = 0;

    char *s = lire(argc>1?argv[1]:"draw.txt");
    hc_set_script(btn, s);
    hc_send(btn, "mouseUp");
    printf("   -> %ld clics, %ld traces\n", g_clicks, g_drags);
    free(s); hc_free(stack);
    return 0;
}
