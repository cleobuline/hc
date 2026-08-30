/* hc_icons.c — Table des icônes d'une pile.
 *
 * HyperCard rangeait les icônes de boutons en ressources ICON dans le fichier
 * de pile : une pile emportait ses icônes, et un bouton n'en retenait que le
 * numéro. La table appartient donc à l'objet PILE, et se sauve avec elle
 * (blocs « iconres » de hc_file.c).
 *
 * Le noyau ne sait pas dessiner : les 128 octets lui sont opaques, exactement
 * comme l'est déjà le base64 de `paint`.
 */
#include "hc_core.h"

#include <stdlib.h>
#include <string.h>

static char *icon_dupstr(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static int is_stack(Object *o)
{
    return o && o->type == OBJ_STACK;
}

struct StackIcon *hc_icon_get(Object *stack, int id)
{
    if (!is_stack(stack)) return NULL;
    for (int i = 0; i < stack->nicons; i++)
        if (stack->icons[i].id == id) return &stack->icons[i];
    return NULL;
}

int hc_icon_count(Object *stack)
{
    return is_stack(stack) ? stack->nicons : 0;
}

struct StackIcon *hc_icon_at(Object *stack, int i)
{
    if (!is_stack(stack) || i < 0 || i >= stack->nicons) return NULL;
    return &stack->icons[i];
}

struct StackIcon *hc_icon_add(Object *stack, int id, const char *name)
{
    if (!is_stack(stack)) return NULL;

    /* Le numéro est unique. Reposer la même icône la remplace : sans ça, un
     * fichier contenant deux fois le même numéro ferait grossir la table à
     * chaque relecture. */
    struct StackIcon *e = hc_icon_get(stack, id);

    if (!e) {
        if (stack->nicons == stack->capicons) {
            int cap = stack->capicons ? stack->capicons * 2 : 8;
            struct StackIcon *t = realloc(stack->icons, (size_t)cap * sizeof *t);
            if (!t) return NULL;
            stack->icons    = t;
            stack->capicons = cap;
        }
        e = &stack->icons[stack->nicons];
        /* realloc rend de la mémoire non initialisée : sans ce nettoyage,
         * `name` part sur un pointeur bidon et le free d'après y passe. Les
         * 128 octets valent zéro du même coup, ce qu'on veut pour une icône
         * neuve. */
        memset(e, 0, sizeof *e);
        e->id = id;
        stack->nicons++;
    }

    free(e->name);
    e->name = icon_dupstr(name);
    return e;
}

void hc_icon_remove(Object *stack, int id)
{
    struct StackIcon *e = hc_icon_get(stack, id);
    if (!e) return;

    free(e->name);
    int i = (int)(e - stack->icons);
    memmove(e, e + 1, (size_t)(stack->nicons - i - 1) * sizeof *e);
    stack->nicons--;
}

/* Pas de « numéro libre » ici : le noyau ne voit que la pile, alors qu'un
 * numéro libre doit l'être aussi dans le catalogue compilé dans l'application.
 * C'est hcicon_edit_free_id, côté Cocoa, qui tranche. */

/* À appeler depuis hc_free, sur la branche OBJ_STACK. */
void hc_icons_free(Object *stack)
{
    if (!is_stack(stack)) return;
    for (int i = 0; i < stack->nicons; i++) free(stack->icons[i].name);
    free(stack->icons);
    stack->icons    = NULL;
    stack->nicons   = 0;
    stack->capicons = 0;
}
