/* WAMR choisit son système par le chemin d'inclusion : chaque dossier
 * core/shared/platform/<système> a son platform_internal.h. HC se construit
 * sous Linux (le noyau, ses tests) ET sous macOS (l'application), avec la
 * même liste de fichiers : ce relais choisit à la compilation. */
#if defined(__APPLE__)
#include "../wamr/core/shared/platform/darwin/platform_internal.h"
#else
#include "../wamr/core/shared/platform/linux/platform_internal.h"
#endif
