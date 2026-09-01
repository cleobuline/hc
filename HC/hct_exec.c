/* hct_exec.c — exécution des instructions. Voir hct_exec.h pour la portée. */

#include "hct_exec.h"
#include "hct_chunk.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ------------------------------------------------------------- portées */

typedef struct { char *nom; char *val; } Var;

struct HctPortee {
    Var   *v;    int n, cap;
    char **gl;   int ngl, capgl;   /* noms déclarés `global` ici */
    struct HctPortee *dessous;
};
typedef struct HctPortee Portee;

static Portee *portee_neuve(Portee *dessous)
{
    Portee *p = calloc(1, sizeof *p);
    if (p) p->dessous = dessous;
    return p;
}

static void portee_libere(Portee *p)
{
    if (!p) return;
    for (int i = 0; i < p->n; i++) { free(p->v[i].nom); free(p->v[i].val); }
    free(p->v);
    for (int i = 0; i < p->ngl; i++) free(p->gl[i]);
    free(p->gl);
    free(p);
}

static Var *portee_trouve(Portee *p, const char *nom)
{
    for (int i = 0; i < p->n; i++)
        if (!strcasecmp(p->v[i].nom, nom)) return &p->v[i];
    return NULL;
}

static void portee_pose(Portee *p, const char *nom, const char *val)
{
    Var *v = portee_trouve(p, nom);
    if (v) {
        char *n = strdup(val ? val : "");
        if (!n) return;
        free(v->val);
        v->val = n;
        return;
    }
    if (p->n == p->cap) {
        int c = p->cap ? p->cap * 2 : 8;
        Var *t = realloc(p->v, (size_t)c * sizeof *t);
        if (!t) return;
        p->v = t; p->cap = c;
    }
    p->v[p->n].nom = strdup(nom);
    p->v[p->n].val = strdup(val ? val : "");
    if (p->v[p->n].nom && p->v[p->n].val) p->n++;
}

static int portee_est_globale(Portee *p, const char *nom)
{
    for (int i = 0; i < p->ngl; i++)
        if (!strcasecmp(p->gl[i], nom)) return 1;
    return 0;
}

static void portee_declare_globale(Portee *p, const char *nom)
{
    if (portee_est_globale(p, nom)) return;
    if (p->ngl == p->capgl) {
        int c = p->capgl ? p->capgl * 2 : 8;
        char **t = realloc(p->gl, (size_t)c * sizeof *t);
        if (!t) return;
        p->gl = t; p->capgl = c;
    }
    p->gl[p->ngl] = strdup(nom);
    if (p->gl[p->ngl]) p->ngl++;
}

/* ------------------------------------------------------- API variables */

int hct_var_lit(HctExec *x, const char *nom, HctValeur *out)
{
    Portee *loc = x->locales;
    if (loc && portee_est_globale(loc, nom)) {
        Var *v = portee_trouve(x->globales, nom);
        if (!v) return 0;
        *out = hct_val_texte(v->val);
        return 1;
    }
    if (loc) {
        Var *v = portee_trouve(loc, nom);
        if (v) { *out = hct_val_texte(v->val); return 1; }
    }
    /* Hors gestionnaire — la boîte de message — on travaille directement
     * dans l'espace global, comme HyperCard. */
    if (!loc) {
        Var *v = portee_trouve(x->globales, nom);
        if (v) { *out = hct_val_texte(v->val); return 1; }
    }
    return 0;
}

void hct_var_ecrit(HctExec *x, const char *nom, const char *val)
{
    Portee *loc = x->locales;
    if (!loc || portee_est_globale(loc, nom)) portee_pose(x->globales, nom, val);
    else portee_pose(loc, nom, val);
}

void hct_var_globale(HctExec *x, const char *nom)
{
    if (x->locales) portee_declare_globale(x->locales, nom);
}

/* Ponts entre l'évaluateur et l'exécuteur.
 *
 * L'évaluateur reçoit un hôte dont les données pointent sur l'EXÉCUTEUR, car
 * c'est lui qui tient les variables. Les autres rappels sont réexpédiés vers
 * l'hôte de l'appelant, avec SES données — sans quoi celui-ci perdrait son
 * contexte, et un hôte qui range ses champs dans une structure ne pourrait
 * plus les retrouver. */
/* L'hôte tient-il les variables ?
 *
 * Il faut les DEUX rappels pour cela. Un hôte qui n'en fournirait qu'un
 * laisserait la moitié des accès dans les portées internes et l'autre chez
 * lui : deux magasins qui divergent au premier « put », et une variable qui
 * vaut son propre nom parce qu'on la cherche là où elle n'est pas.
 *
 * C'est exactement ce qui se produisait ici : lit_var_pont ne consultait
 * jamais l'hôte, alors qu'ecrit_var_pont ne consultait que lui. Un hôte
 * comme HC, qui a déjà ses variables, ne pouvait donc pas fonctionner. */
static int hote_tient_les_vars(const HctExec *x)
{
    return x->hote.lit_var && x->hote.ecrit_var;
}

/* Les DEUX seuls accès aux variables dans tout ce fichier. Rien d'autre ne
 * doit appeler hct_var_lit ni hct_var_ecrit directement, sous peine de
 * réintroduire la dissymétrie. */
static int var_lit(HctExec *x, const char *nom, HctValeur *out)
{
    if (hote_tient_les_vars(x))
        return x->hote.lit_var(x->hote.donnees, nom, out);
    return hct_var_lit(x, nom, out);
}

static void var_ecrit(HctExec *x, const char *nom, const char *val)
{
    if (hote_tient_les_vars(x))
        x->hote.ecrit_var(x->hote.donnees, nom, val ? val : "");
    else
        hct_var_ecrit(x, nom, val);
}

static int lit_var_pont(void *d, const char *nom, HctValeur *out)
{
    return var_lit((HctExec *)d, nom, out);
}

static int ecrit_var_pont(void *d, const char *nom, const char *val)
{
    var_ecrit((HctExec *)d, nom, val);
    return 1;
}

static int fonction_pont(void *d, const char *nom, HctValeur *a, int n,
                         HctValeur *out)
{
    HctExec *x = (HctExec *)d;

    /* Un gestionnaire du script l'emporte sur une fonction de l'hôte : c'est
     * ce qui rend « return n * fact(n - 1) » possible, et c'est aussi l'ordre
     * de HyperCard, où un gestionnaire masque la fonction intégrée de même
     * nom.
     *
     * SAUF quand l'hôte tient les variables. hct_appelle ouvre alors sa propre
     * portée et y lie les paramètres par portee_pose, alors que toute lecture
     * passe par l'hôte : les paramètres se retrouvent posés là où personne ne
     * les cherche, et valent leur propre nom. C'est ce qui donnait
     * « theDateItems » au lieu d'une date.
     *
     * Qui tient les variables tient les portées : on laisse donc l'hôte
     * appeler le gestionnaire, ouvrir son cadre et lier ses paramètres. Il
     * nous rappellera pour en exécuter le corps. */
    if (x->script && !hote_tient_les_vars(x) &&
        hct_appelle(x, x->script, nom, a, n, out)) return 1;

    if (!x->hote.fonction) return 0;
    return x->hote.fonction(x->hote.donnees, nom, a, n, out);
}

/* ------------------------------------------------------------- services */

static char *texte(const HctNoeud *n)
{
    char *s = malloc((size_t)n->jeton.len + 1);
    if (!s) return NULL;
    memcpy(s, n->jeton.deb, (size_t)n->jeton.len);
    s[n->jeton.len] = '\0';
    return s;
}

static int est_motcle(const HctNoeud *n, const char *m)
{
    return n && n->genre == HCTN_MOTCLE && n->op && !strcasecmp(n->op, m);
}


/* Fin de tour de boucle : l'hôte redessine, traite ses événements, et dit
 * s'il faut s'arrêter. Rend 0 pour interrompre la boucle.
 *
 * Sans cela, « repeat while the mouse is down » ne rendait jamais la main :
 * la file d'événements n'était pas vidée, l'état de la souris ne changeait
 * plus, et rien ne se redessinait. L'ancien exécuteur appelait host_idle()
 * à chaque tour ; il fallait la même porte ici. */
static int respire(HctExec *x)
{
    if (!x->hote.respire) return 1;
    return x->hote.respire(x->hote.donnees);
}

static char delim_de(HctExec *x)
{
    HctValeur v;
    if (x->ctx.hote.fonction &&
        x->ctx.hote.fonction(x->ctx.hote.donnees, "itemDelimiter", NULL, 0, &v)) {
        char d = v.txt[0] ? v.txt[0] : ',';
        hct_val_libere(&v);
        return d;
    }
    return ',';
}

/* ------------------------------------------------------------ écriture
 *
 * `put X into <cible>` : la cible est soit une variable, soit un morceau
 * d'une variable. Dans le second cas il faut relire la variable, remplacer le
 * morceau, et réécrire l'ensemble — d'où la récursion sur la cible.
 *
 * `mode` : 0 remplace, 1 avant, 2 après.
 */
/* Écrit `val` dans une cible. Rend 1 si elle a su, 0 sinon.
 *
 * Le verdict compte : une cible inconnue — un champ, une propriété, un objet —
 * n'est pas une faute, c'est simplement quelque chose que l'exécuteur ne sait
 * pas faire seul. L'appelant repasse alors la commande ENTIÈRE à l'hôte, qui
 * la traitera avec son propre interpréteur. Poser une faute ici arrêtait le
 * gestionnaire sur « put x into card field "data" », ce qui condamnait tout
 * script touchant à un champ. */
static int ecrit_dans(HctExec *x, const HctNoeud *cible, const char *val,
                      int mode)
{
    if (!cible) return 0;

    if (cible->genre == HCTN_IDENT) {
        char *nom = texte(cible);
        if (!nom) return 0 ;
        if (mode == 0) {
            var_ecrit(x, nom, val);
        } else {
            HctValeur ancien;
            if (!var_lit(x, nom, &ancien)) ancien = hct_val_vide();
            int la = ancien.len, lv = (int)strlen(val);
            char *neuf = malloc((size_t)la + lv + 1);
            if (neuf) {
                if (mode == 1) { memcpy(neuf, val, (size_t)lv);
                                 memcpy(neuf + lv, ancien.txt, (size_t)la); }
                else           { memcpy(neuf, ancien.txt, (size_t)la);
                                 memcpy(neuf + la, val, (size_t)lv); }
                neuf[la + lv] = '\0';
                var_ecrit(x, nom, neuf);
                free(neuf);
            }
            hct_val_libere(&ancien);
        }
        free(nom);
        return 1;
    }

    if (cible->genre == HCTN_CHUNK) {
        /* Le morceau porte sa cible en dernier enfant : on la lit, on y
         * remplace le morceau, puis on réécrit la cible entière. */
        const HctNoeud *sous = cible->fils[cible->nfils - 1];
        HctValeur base = hct_evalue(&x->ctx, sous);
        if (x->ctx.erreur) { hct_val_libere(&base); return 1; }

        char d = delim_de(x);
        int n1 = 0, n2 = 0;
        if (cible->ordinal) {
            int total = hct_chunk_compte(base.txt, cible->sorte, d);
            switch (cible->ordinal) {
                case HCT_ORD_DERNIER: n1 = total; break;
                case HCT_ORD_MILIEU:  n1 = total > 0 ? total / 2 + 1 : 0; break;
                default:              n1 = (int)cible->ordinal; break;
            }
        } else {
            if (cible->nfils >= 2) {
                HctValeur a = hct_evalue(&x->ctx, cible->fils[0]);
                n1 = (int)hct_vers_nombre(a.txt);
                hct_val_libere(&a);
            }
            if (cible->nfils >= 3) {
                HctValeur b = hct_evalue(&x->ctx, cible->fils[1]);
                n2 = (int)hct_vers_nombre(b.txt);
                hct_val_libere(&b);
            }
        }

        const char *aecrire = val;
        /* Pas de hct_val_vide() ici : on écrase compose.txt par un malloc
         * juste après, ce qui perdrait l'octet alloué par la valeur vide. */
        HctValeur compose = { NULL, 0 };
        if (mode != 0) {
            HctValeur ancien = hct_chunk_lit(base.txt, cible->sorte, n1, n2, d);
            int la = ancien.len, lv = (int)strlen(val);
            compose.txt = malloc((size_t)la + lv + 1);
            if (compose.txt) {
                if (mode == 1) { memcpy(compose.txt, val, (size_t)lv);
                                 memcpy(compose.txt + lv, ancien.txt, (size_t)la); }
                else           { memcpy(compose.txt, ancien.txt, (size_t)la);
                                 memcpy(compose.txt + la, val, (size_t)lv); }
                compose.txt[la + lv] = '\0';
                compose.len = la + lv;
                aecrire = compose.txt;
            }
            hct_val_libere(&ancien);
        }

        HctValeur neuf = hct_chunk_ecrit(base.txt, cible->sorte, n1, n2, d,
                                         aecrire);
        /* Le morceau porte sur une cible que l'on ne sait peut-être pas
         * écrire non plus : le verdict se propage. */
        int ok = ecrit_dans(x, sous, neuf.txt, 0);
        hct_val_libere(&neuf);
        hct_val_libere(&compose);
        hct_val_libere(&base);
        return ok;
    }

    if (cible->genre == HCTN_OBJET) {
        /* Un objet : l'hôte le résout, puis y écrit. C'est le chemin court —
         * sans lui, la ligne entière repartait à l'interpréteur de l'hôte,
         * qui la réanalysait et réévaluait l'expression. */
        if (!x->ctx.hote.resout || !x->ctx.hote.ecrit_objet) return 0;
        void *o = x->ctx.hote.resout(x->ctx.hote.donnees, cible, &x->ctx);
        if (!o) return 0;
        return x->ctx.hote.ecrit_objet(x->ctx.hote.donnees, o, val, mode);
    }

    return 0;      /* propriété, ou objet que l'hôte ne sait pas écrire */
}

/* La cible est-elle de celles qu'ecrit_dans sait écrire ?
 *
 * Il faut le savoir AVANT d'évaluer l'opérande. Découvrir après coup que la
 * cible est un champ obligeait à rendre la ligne entière à l'hôte, qui la
 * réévaluait : « put random(10) into fld 1 » tirait deux nombres et écrivait
 * le second, « put nextID() into fld 1 » consommait deux identifiants. */
static int cible_connue(HctExec *x, const HctNoeud *c)
{
    if (!c) return 0;
    if (c->genre == HCTN_IDENT) return 1;
    if (c->genre == HCTN_CHUNK)
        return c->nfils >= 1 && cible_connue(x, c->fils[c->nfils - 1]);
    /* Un objet ne compte que si l'hôte sait le résoudre ET y écrire. */
    if (c->genre == HCTN_OBJET)
        return x->ctx.hote.resout != NULL && x->ctx.hote.ecrit_objet != NULL;
    return 0;
}

/* Le vide vaut zéro, comme dans hct_eval.c : « put empty into total » suivi de
 * « add 1 to total » doit donner 1, pas une erreur. Un texte non vide et non
 * numérique reste une faute. */
static int nombre_ou_vide(const char *s, double *x)
{
    if (!s) { *x = 0; return 1; }
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (!*s) { *x = 0; return 1; }
    if (!hct_est_nombre(s)) return 0;
    *x = hct_vers_nombre(s);
    return 1;
}

/* ------------------------------------------------------------ commandes */

static void commande(HctExec *x, const HctNoeud *n)
{
    const char *v = n->op ? n->op : "";

    if (!strcasecmp(v, "put")) {
        if (n->nfils < 1) return;

        /* Ce que l'exécuteur ne saura pas écrire — un champ, une propriété,
         * ou la boîte de message — repart à l'hôte SANS avoir été évalué. */
        int a_cible = n->nfils >= 3;
        if ((!a_cible || !cible_connue(x, n->fils[2])) && x->ctx.hote.commande) {
            x->ctx.hote.commande(x->ctx.hote.donnees, n, &x->ctx);
            return;
        }

        HctValeur val = hct_evalue(&x->ctx, n->fils[0]);
        if (x->ctx.erreur) { hct_val_libere(&val); return; }

        if (n->nfils >= 3) {
            int mode = est_motcle(n->fils[1], "before") ? 1
                     : est_motcle(n->fils[1], "after")  ? 2 : 0;
            if (!ecrit_dans(x, n->fils[2], val.txt, mode) &&
                x->ctx.hote.commande)
                x->ctx.hote.commande(x->ctx.hote.donnees, n, &x->ctx);
        } else {
            /* « put X » sans cible écrit dans la BOÎTE DE MESSAGE, qui n'est
             * pas une variable : lui poser var("msg") créait une variable de
             * ce nom et n'affichait rien. Seul l'hôte sait montrer la boîte,
             * on lui rend donc la commande entière — c'est aussi ce que fait
             * l'ancien exécuteur, qui émet HC_MSG.
             *
             * Le repli sur ecrit_var ne sert qu'aux hôtes sans `commande`. */
            if (x->ctx.hote.commande)
                x->ctx.hote.commande(x->ctx.hote.donnees, n, &x->ctx);
            else if (x->ctx.hote.ecrit_var)
                x->ctx.hote.ecrit_var(x->ctx.hote.donnees, "msg", val.txt);
        }
        hct_val_libere(&val);
        return;
    }

    if (!strcasecmp(v, "get")) {
        if (n->nfils < 1) return;
        HctValeur val = hct_evalue(&x->ctx, n->fils[0]);
        if (!x->ctx.erreur) var_ecrit(x, "it", val.txt);
        hct_val_libere(&val);
        return;
    }

    if (!strcasecmp(v, "global")) {
        /* Quand l'hôte tient les variables, il tient aussi la distinction
         * entre locale et globale : déclarer ici, dans des portées qui ne
         * servent pas, ne ferait rien du tout. On lui repasse la commande
         * entière plus bas, par le recours. */
        if (hote_tient_les_vars(x)) {
            if (x->ctx.hote.commande)
                x->ctx.hote.commande(x->ctx.hote.donnees, n, &x->ctx);
            return;
        }
        for (int i = 0; i < n->nfils; i++) {
            const HctNoeud *f = n->fils[i];
            if (f->genre != HCTN_IDENT) continue;
            char *nom = texte(f);
            if (nom) { hct_var_globale(x, nom); free(nom); }
        }
        return;
    }

    /* add/subtract/multiply/divide : lisent la cible, calculent, réécrivent.
     *
     * Attention à l'ordre des opérandes, qui n'est PAS le même :
     *
     *   add e to c          la cible est en dernier
     *   subtract e from c   idem
     *   multiply c by e     la cible est en PREMIER
     *   divide c by e       idem
     *
     * Les traiter uniformément donnait « cible d'écriture non gérée » sur
     * multiply et divide, dont le premier opérande est la variable. */
    if (!strcasecmp(v, "add") || !strcasecmp(v, "subtract") ||
        !strcasecmp(v, "multiply") || !strcasecmp(v, "divide")) {
        if (n->nfils < 3) return;

        int cible_devant = !strcasecmp(v, "multiply") || !strcasecmp(v, "divide");
        const HctNoeud *ncible  = cible_devant ? n->fils[0] : n->fils[2];
        const HctNoeud *nvaleur = cible_devant ? n->fils[2] : n->fils[0];

        /* Cible inconnue : à l'hôte, avant toute évaluation. */
        if (!cible_connue(x, ncible) && x->ctx.hote.commande) {
            x->ctx.hote.commande(x->ctx.hote.donnees, n, &x->ctx);
            return;
        }

        HctValeur val = hct_evalue(&x->ctx, nvaleur);
        if (x->ctx.erreur) { hct_val_libere(&val); return; }
        HctValeur act = hct_evalue(&x->ctx, ncible);
        if (x->ctx.erreur) { hct_val_libere(&val); hct_val_libere(&act); return; }

        double xv, xc, r = 0;
        if (!nombre_ou_vide(val.txt, &xv) || !nombre_ou_vide(act.txt, &xc)) {
            hct_ctx_faute(&x->ctx, n, "un nombre est attendu ici");
            hct_val_libere(&val); hct_val_libere(&act);
            return;
        }
        if      (!strcasecmp(v, "add"))      r = xc + xv;
        else if (!strcasecmp(v, "subtract")) r = xc - xv;
        else if (!strcasecmp(v, "multiply")) r = xc * xv;
        else {
            if (xv == 0) { hct_ctx_faute(&x->ctx, n, "division par zéro");
                           hct_val_libere(&val); hct_val_libere(&act); return; }
            r = xc / xv;
        }
        HctValeur res = hct_val_nombre(r);
        if (!ecrit_dans(x, ncible, res.txt, 0) && x->ctx.hote.commande)
            x->ctx.hote.commande(x->ctx.hote.donnees, n, &x->ctx);
        hct_val_libere(&res);
        hct_val_libere(&val); hct_val_libere(&act);
        return;
    }

    if (!strcasecmp(v, "return")) {
        /* Évaluer AVANT de libérer l'ancienne valeur de retour.
         *
         * L'évaluation peut déclencher un appel récursif — « return n *
         * fact(n-1) » — qui repose lui-même une valeur dans x->retour. En
         * libérant d'abord, on écrasait ensuite celle du niveau imbriqué sans
         * la rendre : une fuite par niveau de récursion, invisible tant qu'on
         * ne récursait pas. */
        HctValeur r = n->nfils ? hct_evalue(&x->ctx, n->fils[0])
                               : hct_val_vide();
        hct_val_libere(&x->retour);
        x->retour = r;
        x->a_rendu = 1;
        x->signal = HCT_SIG_EXIT_HANDLER;
        return;
    }

    if (!strcasecmp(v, "exit")) {
        /* « exit repeat », « exit <gestionnaire> », « exit to HyperCard ». */
        if (n->nfils && n->fils[0]->genre == HCTN_IDENT) {
            char *quoi = texte(n->fils[0]);
            if (quoi && !strcasecmp(quoi, "repeat")) x->signal = HCT_SIG_EXIT_REPEAT;
            else x->signal = HCT_SIG_EXIT_HANDLER;
            free(quoi);
        } else if (n->nfils >= 2 && est_motcle(n->fils[0], "to")) {
            x->signal = HCT_SIG_EXIT_TOUT;
        } else x->signal = HCT_SIG_EXIT_HANDLER;
        return;
    }

    if (!strcasecmp(v, "next")) {          /* next repeat */
        x->signal = HCT_SIG_NEXT_REPEAT;
        return;
    }

    if (!strcasecmp(v, "pass")) {
        x->signal = HCT_SIG_PASS;
        return;
    }

    /* Tout le reste — go, set, show, beep, play… — appartient au monde et
     * revient à l'hôte.
     *
     * D'ABORD sous forme de NŒUD, s'il sait le prendre ainsi : « go to card 3 »
     * n'a de sens que si la référence d'objet lui parvient intacte, alors que
     * l'appel par `fonction` ci-dessous évalue les opérandes au préalable et la
     * réduirait à du texte. C'est par là que passe la transition, l'hôte
     * pouvant reconstituer la ligne d'origine et la confier à son ancien
     * interpréteur. */
    if (x->ctx.hote.commande &&
        x->ctx.hote.commande(x->ctx.hote.donnees, n, &x->ctx))
        return;

    if (x->ctx.hote.fonction) {
        int nargs = 0;
        for (int i = 0; i < n->nfils; i++)
            if (n->fils[i]->genre != HCTN_MOTCLE) nargs++;
        HctValeur *args = nargs ? calloc((size_t)nargs, sizeof *args) : NULL;
        int k = 0;
        for (int i = 0; i < n->nfils && !x->ctx.erreur; i++) {
            if (n->fils[i]->genre == HCTN_MOTCLE) continue;
            args[k++] = hct_evalue(&x->ctx, n->fils[i]);
        }
        HctValeur out;
        if (x->ctx.hote.fonction(x->ctx.hote.donnees, v, args, k, &out))
            hct_val_libere(&out);
        for (int i = 0; i < k; i++) hct_val_libere(&args[i]);
        free(args);
    }
}

/* -------------------------------------------------- structures de contrôle */

static void execute_si(HctExec *x, const HctNoeud *n)
{
    if (n->nfils < 2) return;
    HctValeur c = hct_evalue(&x->ctx, n->fils[0]);
    if (x->ctx.erreur) { hct_val_libere(&c); return; }

    int valide, vrai = hct_vers_bool(c.txt, &valide);
    hct_val_libere(&c);
    if (!valide) {
        hct_ctx_faute(&x->ctx, n->fils[0], "true ou false attendu ici");
        return;
    }
    if (vrai)                    hct_exec(x, n->fils[1]);
    else if (n->nfils >= 3)      hct_exec(x, n->fils[2]);
}

static void execute_repete(HctExec *x, const HctNoeud *n)
{
    const char *forme = n->op ? n->op : "forever";
    const HctNoeud *corps = n->nfils ? n->fils[n->nfils - 1] : NULL;
    if (!corps) return;

    /* Garde-fou : une boucle sans fin bloquerait l'essai en ligne de commande.
     * HyperCard, lui, laisse la main à l'utilisateur — c'est à l'hôte de
     * décider quand interrompre, via la fonction « doit_interrompre ». */
    const long PLAFOND = 10000000L;
    long tours = 0;

    if (!strncasecmp(forme, "with", 4)) {
        if (n->nfils < 4) return;
        char *nom = texte(n->fils[0]);
        if (!nom) return;

        HctValeur d = hct_evalue(&x->ctx, n->fils[1]);
        HctValeur f = hct_evalue(&x->ctx, n->fils[2]);
        if (x->ctx.erreur) { free(nom); hct_val_libere(&d); hct_val_libere(&f); return; }

        double deb = hct_vers_nombre(d.txt), fin = hct_vers_nombre(f.txt);
        hct_val_libere(&d); hct_val_libere(&f);
        int descend = !strncasecmp(forme, "with down", 9);

        /* Le pas de « by P », quand l'analyseur en a posé un : il est alors le
         * fils qui précède le corps. Toujours positif — c'est « down » qui
         * donne le sens, comme dans HyperCard. Un pas nul boucleraient sans
         * fin, on le ramène à 1. */
        double pas = 1;
        if (strstr(forme, "pas") && n->nfils >= 5) {
            HctValeur p = hct_evalue(&x->ctx, n->fils[n->nfils - 2]);
            if (!x->ctx.erreur) pas = hct_vers_nombre(p.txt);
            hct_val_libere(&p);
            if (pas < 0) pas = -pas;
            if (pas == 0) pas = 1;
        }

        for (double i = deb; descend ? i >= fin : i <= fin;
             i += descend ? -pas : pas) {
            if (++tours > PLAFOND) break;
            HctValeur vi = hct_val_nombre(i);
            var_ecrit(x, nom, vi.txt);
            hct_val_libere(&vi);

            hct_exec(x, corps);
            if (x->ctx.erreur) break;
            if (!respire(x)) break;
            if (x->signal == HCT_SIG_NEXT_REPEAT) { x->signal = HCT_SIG_AUCUN; continue; }
            if (x->signal == HCT_SIG_EXIT_REPEAT) { x->signal = HCT_SIG_AUCUN; break; }
            if (x->signal) break;
        }
        free(nom);
        return;
    }

    long limite = -1;
    if (!strcasecmp(forme, "times")) {
        if (n->nfils < 2) return;
        HctValeur v = hct_evalue(&x->ctx, n->fils[0]);
        if (x->ctx.erreur) { hct_val_libere(&v); return; }
        limite = (long)hct_vers_nombre(v.txt);
        hct_val_libere(&v);
    }

    for (;;) {
        if (++tours > PLAFOND) break;
        if (limite >= 0 && tours > limite) break;

        if (!strcasecmp(forme, "while") || !strcasecmp(forme, "until")) {
            HctValeur c = hct_evalue(&x->ctx, n->fils[0]);
            if (x->ctx.erreur) { hct_val_libere(&c); break; }
            int valide, vrai = hct_vers_bool(c.txt, &valide);
            hct_val_libere(&c);
            if (!valide) {
                hct_ctx_faute(&x->ctx, n->fils[0], "true ou false attendu ici");
                break;
            }
            if (!strcasecmp(forme, "while") && !vrai) break;
            if (!strcasecmp(forme, "until") &&  vrai) break;
        }

        hct_exec(x, corps);
        if (x->ctx.erreur) break;
        if (!respire(x)) break;
        if (x->signal == HCT_SIG_NEXT_REPEAT) { x->signal = HCT_SIG_AUCUN; continue; }
        if (x->signal == HCT_SIG_EXIT_REPEAT) { x->signal = HCT_SIG_AUCUN; break; }
        if (x->signal) break;
    }
}

/* ------------------------------------------------------------ l'entrée */

void hct_exec(HctExec *x, const HctNoeud *n)
{
    if (!n || x->ctx.erreur || x->signal) return;

    switch (n->genre) {
        case HCTN_BLOC:
            if (!x->script) x->script = n;   /* le bloc racine */
            for (int i = 0; i < n->nfils; i++) {
                hct_exec(x, n->fils[i]);
                if (x->ctx.erreur || x->signal) return;
            }
            return;

        case HCTN_COMMANDE:      commande(x, n);        return;
        case HCTN_SI:            execute_si(x, n);      return;
        case HCTN_REPETE:        execute_repete(x, n);  return;

        case HCTN_GESTIONNAIRE:
            /* Un gestionnaire ne s'exécute pas là où il est écrit : il attend
             * d'être appelé. Le rencontrer dans un script est normal. */
            return;

        case HCTN_MESSAGE: {
            /* Un envoi de message : l'hôte le transmet à la hiérarchie
             * d'objets.
             *
             * Par le NŒUD d'abord, comme les commandes. C'est indispensable :
             * « drawCalendar theDateItems » doit parvenir à l'hôte sous sa
             * forme écrite, pour qu'il ouvre un cadre, y lie les paramètres,
             * et fasse remonter le message dans la hiérarchie. Ne l'envoyer
             * que par `fonction`, avec les arguments déjà évalués, le
             * réduisait à un appel plat — et quand le nom lui était inconnu,
             * le message se perdait SANS un mot, le retour n'étant même pas
             * testé. C'est ce qui empêchait le calendrier de se dessiner. */
            if (x->ctx.hote.commande &&
                x->ctx.hote.commande(x->ctx.hote.donnees, n, &x->ctx))
                return;

            /* Sans hôte, on l'ignore silencieusement — ce n'est pas une
             * erreur de syntaxe. */
            if (!x->ctx.hote.fonction) return;
            char *nom = texte(n);
            if (!nom) return;
            int nargs = n->nfils;
            HctValeur *args = nargs ? calloc((size_t)nargs, sizeof *args) : NULL;
            for (int i = 0; i < nargs && !x->ctx.erreur; i++)
                args[i] = hct_evalue(&x->ctx, n->fils[i]);
            HctValeur out;
            if (x->ctx.hote.fonction(x->ctx.hote.donnees, nom, args, nargs, &out))
                hct_val_libere(&out);
            else
                hct_ctx_faute(&x->ctx, n, "message inconnu");
            for (int i = 0; i < nargs; i++) hct_val_libere(&args[i]);
            free(args);
            free(nom);
            return;
        }

        case HCTN_ERREUR:
            hct_ctx_faute(&x->ctx, n, n->msg ? n->msg : "instruction invalide");
            return;

        default: {
            /* Une expression seule : on l'évalue pour ses effets. */
            HctValeur v = hct_evalue(&x->ctx, n);
            hct_val_libere(&v);
            return;
        }
    }
}

/* ------------------------------------------------------ gestionnaires */

int hct_appelle(HctExec *x, const HctNoeud *script, const char *nom,
                HctValeur *args, int nargs, HctValeur *out)
{
    if (!script) return 0;

    const HctNoeud *g = NULL;
    for (int i = 0; i < script->nfils; i++) {
        const HctNoeud *f = script->fils[i];
        if (f->genre != HCTN_GESTIONNAIRE || f->nfils < 1) continue;
        char *n2 = texte(f->fils[0]);
        if (!n2) continue;
        int trouve = !strcasecmp(n2, nom);
        free(n2);
        if (trouve) { g = f; break; }
    }
    if (!g) return 0;

    const HctNoeud *garde_script = x->script;
    x->script = script;

    if (x->profondeur > 200) {
        x->script = garde_script;
        hct_ctx_faute(&x->ctx, g, "trop d'appels imbriqués");
        return 1;
    }

    Portee *loc = portee_neuve(x->locales);
    if (!loc) return 1;
    x->locales = loc;
    x->profondeur++;

    /* Les paramètres reçoivent les arguments, dans l'ordre. Un paramètre non
     * fourni vaut la chaîne vide, comme en HyperTalk. */
    if (g->nfils >= 2) {
        const HctNoeud *params = g->fils[1];
        for (int i = 0; i < params->nfils; i++) {
            char *p = texte(params->fils[i]);
            if (!p) continue;
            portee_pose(loc, p, i < nargs ? args[i].txt : "");
            free(p);
        }
    }
    portee_pose(loc, "it", "");

    hct_val_libere(&x->retour);
    x->retour = hct_val_vide();
    int garde_rendu = x->a_rendu;
    x->a_rendu = 0;

    if (g->nfils >= 3) hct_exec(x, g->fils[2]);

    if (x->signal == HCT_SIG_EXIT_HANDLER || x->signal == HCT_SIG_PASS)
        x->signal = HCT_SIG_AUCUN;

    if (out) *out = hct_val_copie(x->retour);

    x->locales = loc->dessous;
    portee_libere(loc);
    x->profondeur--;
    x->script = garde_script;
    x->a_rendu = garde_rendu;
    return 1;
}

/* --------------------------------------------------------- construction */

void hct_exec_init(HctExec *x, HctHote hote)
{
    memset(x, 0, sizeof *x);
    x->hote = hote;                 /* on garde l'hôte et ses données     */

    HctHote pont;
    memset(&pont, 0, sizeof pont);
    pont.donnees   = x;
    pont.lit_var   = lit_var_pont;
    pont.ecrit_var = ecrit_var_pont;
    pont.fonction  = fonction_pont;
    pont.resout    = hote.resout;
    pont.lit_objet = hote.lit_objet;
    pont.lit_prop  = hote.lit_prop;
    /* Ces deux-là manquaient, et rien ne le disait.
     *
     * `recours` est la porte de sortie de l'évaluateur pour tout ce qu'il ne
     * sait pas calculer — les références d'objets, les propriétés. `commande`
     * est la même porte pour les instructions. Ne pas les transmettre au
     * contexte revenait à les couper : l'exécuteur les recevait de son
     * appelant et ne les passait jamais à l'évaluateur qu'il pilote. */
    pont.recours   = hote.recours;
    pont.commande  = hote.commande;
    pont.ecrit_objet = hote.ecrit_objet;

    hct_ctx_init(&x->ctx, pont);
    x->globales = portee_neuve(NULL);
    x->retour = hct_val_vide();
}

void hct_exec_libere(HctExec *x)
{
    while (x->locales) {
        Portee *p = x->locales;
        x->locales = p->dessous;
        portee_libere(p);
    }
    portee_libere(x->globales);
    x->globales = NULL;
    hct_val_libere(&x->retour);
}
