/* hc_file.h — Lecture et écriture des piles (format texte maison, v1). */
#ifndef HC_FILE_H
#define HC_FILE_H

#include "hc_core.h"

/* Écrit la pile dans un fichier. Renvoie 0 si tout va bien. */
int hc_save(Object *stack, const char *path);

/* Relit une pile. Renvoie NULL en cas d'échec. */
Object *hc_load(const char *path);

/* Pourquoi le DERNIER hc_load a refusé, en une phrase, ou NULL s'il a réussi.
 *
 * hc_load rendait NULL pour tout — fichier absent, tronqué, sans ligne
 * « stack », d'un format inconnu — et l'interface n'avait que « Pile
 * illisible » à en dire. « Format trop récent » et « fichier abîmé » appellent
 * pourtant des gestes très différents. Valable jusqu'au prochain hc_load. */
const char *hc_load_erreur(void);

#endif
