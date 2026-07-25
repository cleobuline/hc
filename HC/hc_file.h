/* hc_file.h — Lecture et écriture des piles (format texte maison, v1). */
#ifndef HC_FILE_H
#define HC_FILE_H

#include "hc_core.h"

/* Écrit la pile dans un fichier. Renvoie 0 si tout va bien. */
int hc_save(Object *stack, const char *path);

/* Relit une pile. Renvoie NULL en cas d'échec. */
Object *hc_load(const char *path);

#endif
