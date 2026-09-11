#include "hc_core.h"
#include <stdio.h>
#include <string.h>
static void ligne(HcLineKind k,int d,const char *t){ (void)d; if(k==HC_ERR||k==HC_MSG) printf("   %s\n",t); }
static void plages(Object *f, const char *quand)
{
    int n = hc_run_count(f);
    printf("   %-28s : %d plage(s)\n", quand, n);
    for (int i = 0; i < n; i++) {
        int a=0,l=0,st=0,sz=0; const char *fn=NULL;
        if (hc_run_attrs(f, i, &a, &l, &st, &sz, &fn))
            printf("        [%d..%d[ style=%d corps=%d police=%s\n",
                   a, a+l, st, sz, fn?fn:"(champ)");
    }
}
int main(void)
{
    HcHost h; memset(&h,0,sizeof h); h.line=ligne; hc_set_host(&h);
    Object *stack = hc_new_stack("T");
    Object *bg = hc_new_background(stack,"F");
    Object *c  = hc_new_card(stack,bg,"U");
    Object *btn= hc_new_button(c,"B");
    Object *f  = hc_new_field(c,"test");
    hc_set_field_text(f, "abcdefghij");
    hc_set_current_card(c);

    plages(f, "au depart");

    hc_set_script(btn, "on mouseUp\n"
                       "  set the textStyle of char 1 to 5 of field \"test\" to bold\n"
                       "end mouseUp\n");
    hc_send(btn,"mouseUp");
    plages(f, "apres set char 1 to 5");

    hc_set_script(btn, "on mouseUp\n"
                       "  set the textStyle of field \"test\" to bold\n"
                       "end mouseUp\n");
    hc_send(btn,"mouseUp");
    printf("   textstyle du champ entier   : %d\n", f->textstyle);

    hc_free(stack); return 0;
}
