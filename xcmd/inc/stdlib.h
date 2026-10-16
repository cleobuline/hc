/* stdlib.h du kit des modules de HC : la mémoire. free ne rend rien. */
#ifndef HC_KIT_STDLIB_H
#define HC_KIT_STDLIB_H
#include <stddef.h>
void *malloc(size_t n);
void *calloc(size_t a, size_t b);
void  free(void *p);
#endif
