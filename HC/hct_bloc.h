/* hct_bloc.h — structures de contrôle et gestionnaires. Voir hct_bloc.c. */
#ifndef HCT_BLOC_H
#define HCT_BLOC_H

#include "hct_cmd.h"

/* Une instruction, structure de contrôle comprise. */
HctNoeud *hct_bloc_instruction(HctAnalyseur *a);

/* Un script entier : suite de gestionnaires et d'instructions. */
HctNoeud *hct_bloc_script(HctAnalyseur *a);

/* services supplémentaires de hct_expr.c */
void hct_expr_avance(HctAnalyseur *a);
int  hct_expr_op_ici(HctAnalyseur *a, const char *s);

#endif
