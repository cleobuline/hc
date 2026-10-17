/* stdlib.h du kit des modules de HC : la mémoire, les entiers (hc_xcmd.c). */
#ifndef HC_KIT_STDLIB_H
#define HC_KIT_STDLIB_H
#include <stddef.h>
void *malloc(size_t n);
void *calloc(size_t a, size_t b);
void *realloc(void *p, size_t n);
void  free(void *p);
long  strtol(const char *s, char **fin, int base);
int   atoi(const char *s);
int   abs(int x);
#endif
