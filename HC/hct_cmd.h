/* hct_cmd.h — commandes et instructions HyperTalk. Voir hct_cmd.c pour
 * l'alphabet des motifs. */

#ifndef HCT_CMD_H
#define HCT_CMD_H

#include "hct_expr.h"

typedef struct { const char *verbe, *motif; } HctCommande;

const HctCommande *hct_commande_table(void);

/* Analyse une commande à la position courante. NULL si le verbe n'est pas
 * reconnu ou si son motif ne colle pas — l'appelant se rabat alors sur
 * l'envoi de message. */
HctNoeud *hct_commande(HctAnalyseur *a);

/* Une instruction : commande reconnue, ou envoi de message. */
HctNoeud *hct_instruction(HctAnalyseur *a);

/* ---- services fournis par hct_expr.c à hct_cmd.c ---- */
int             hct_expr_fini(HctAnalyseur *a);
const HctJeton *hct_expr_jeton(HctAnalyseur *a);
const HctJeton *hct_expr_jeton_apres(HctAnalyseur *a, int d);
int             hct_expr_virgule(HctAnalyseur *a);   /* avale une virgule */
HctNoeud       *hct_expr_faute(HctAnalyseur *a, const char *msg);
HctNoeud       *hct_expression_sans_ou(HctAnalyseur *a);
HctNoeud       *hct_expr_avale_mot(HctAnalyseur *a);
HctNoeud       *hct_expr_ouvre_commande(HctAnalyseur *a, const char *verbe,
                                        int nmots);
HctNoeud       *hct_expr_avale_motcle(HctAnalyseur *a, const char *el,
                                      int len, int alternative);

#endif
