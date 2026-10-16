/* wamr_hc.h — les réglages de WAMR pour HC, inclus en tête de chacun de
 * ses fichiers (-include). Le pourquoi : docs/mesures/xcmd.txt, marche 1.
 *
 * L'interprète « fast » seul : ni AOT, ni JIT, ni bibliothèque C du côté
 * du module — un module ne voit que les rappels de HC. */
#ifndef HC_WAMR_HC_H
#define HC_WAMR_HC_H

#define WASM_ENABLE_INTERP            1
#define WASM_ENABLE_FAST_INTERP       1
#define WASM_ENABLE_AOT               0
#define WASM_ENABLE_JIT               0
#define WASM_ENABLE_FAST_JIT          0
#define WASM_ENABLE_LIBC_BUILTIN      0
#define WASM_ENABLE_LIBC_WASI         0
#define WASM_ENABLE_MULTI_MODULE      0
#define WASM_ENABLE_MINI_LOADER       0
#define WASM_ENABLE_SHARED_MEMORY     0
#define WASM_ENABLE_SIMD              0

/* Ce que pose d'office le clang 24 d'emsdk (mesuré sur le Mac de
 * l'utilisatrice) : un module compilé là doit se charger ici. */
#define WASM_ENABLE_BULK_MEMORY       1
#define WASM_ENABLE_BULK_MEMORY_OPT   1
#define WASM_ENABLE_REF_TYPES         1
#define WASM_ENABLE_CALL_INDIRECT_OVERLONG 1

/* Le gestionnaire de fils : sans lui, wasm_runtime_terminate n'arrête pas
 * un module qui boucle (mesuré : tué au bout de 5 s ; avec lui, 200 ms). */
#define WASM_ENABLE_THREAD_MGR        1

/* PAS DE GARDE MATÉRIELLE. Par défaut, sur 64 bits, WAMR réserve de grandes
 * zones de mémoire et attrape les débordements par un gestionnaire de
 * signal (SIGSEGV). Dans une application, ce gestionnaire se battrait avec
 * celui du système et avec AddressSanitizer : on garde les vérifications
 * logicielles, à chaque accès. */
#define WASM_DISABLE_HW_BOUND_CHECK        1
#define WASM_DISABLE_STACK_HW_BOUND_CHECK  1

/* Agrandir la mémoire d'un module : par copie, partout. Linux saurait le
 * faire en place (mremap), macOS non ; une seule liste de fichiers pour les
 * deux vaut mieux qu'un agrandissement un peu plus rapide, qui est rare. */
#define WASM_HAVE_MREMAP 0

#define BH_MALLOC wasm_runtime_malloc
#define BH_FREE   wasm_runtime_free

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#endif
