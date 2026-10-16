/* sansboite.c — un module écrit sans le kit : il exporte un externe, mais
 * pas de boîte aux paramètres. HC doit le refuser en disant pourquoi. */
__attribute__((export_name("XCMD seul")))
void seul(int p);
void seul(int p)
{
    (void)p;
}
