/* stdio.h du kit des modules de HC : seulement de quoi formater du texte. */
#ifndef HC_KIT_STDIO_H
#define HC_KIT_STDIO_H
#include <stddef.h>
#include <stdarg.h>
int snprintf(char *s, size_t n, const char *f, ...);
int vsnprintf(char *s, size_t n, const char *f, va_list ap);
#endif
