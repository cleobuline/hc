# Le code tiers de HC

Un seul pour l'instant : **WAMR**, l'interprète WebAssembly qui fait tourner
les modules d'une pile (les XCMD et XFCN, `HC/hc_wasm.c`). Choisi le
16 octobre parmi trois candidats mesurés ; les mesures et le pourquoi sont
dans `docs/mesures/xcmd.txt`.

## WAMR

| | |
| --- | --- |
| projet | WebAssembly Micro Runtime, de la Bytecode Alliance |
| version | 2.4.3 |
| commit | `ea5dd88e9ca40a44b6a5d9ffeb87a20833b13b8a`, du 9 octobre 2026 |
| licence | Apache 2.0 avec l'exception LLVM : `wamr/LICENSE` |
| pris | 36 fichiers C et leurs en-têtes, sur des milliers |

### Ce qui est pris, et ce qui ne l'est pas

Seulement de quoi faire tourner l'interprète **« fast »** : le chargeur, le
validateur, l'interprète, le gestionnaire de fils (pour l'arrêt d'un module
qui boucle), la mémoire, et les plateformes Linux et macOS. Ni AOT, ni JIT,
ni WASI, ni bibliothèque C côté module, ni SIMD, ni mémoire partagée.

Les en-têtes AOT (`core/iwasm/aot`, `core/iwasm/compilation/aot.h`) sont là
parce que `wasm_memory.c` les inclut même quand l'AOT est éteint. Aucun de
leurs fichiers C n'est compilé.

La liste exacte est `wamr_sources.txt`. **Le projet Xcode en porte une
copie** : un fichier ajouté ou retiré ici doit l'être aussi dans
`HC.xcodeproj`. C'est un site jumeau.

### Les réglages : `wamr_hc.h`

Inclus en tête de chaque fichier de WAMR (`-include`). Chaque choix y est
expliqué ; les deux qui surprennent :

- **pas de garde matérielle** (`WASM_DISABLE_HW_BOUND_CHECK`) : WAMR, par
  défaut, attrape les débordements par un gestionnaire de signal, qui se
  battrait dans une application avec celui du système et avec
  AddressSanitizer. Les vérifications sont logicielles, à chaque accès
  (mesuré : environ 6 % plus lent sur le banc MAC•CAM, 399 pas par seconde
  au lieu de 425) ;
- **`invokeNative_general.c`, mais jamais appelé** : cette version en C des
  appels natifs suppose des cases de 32 bits, et sur nos machines 64 bits
  elle TRONQUAIT les pointeurs (mesuré : `SendCardMessage` recevait une
  chaîne vide). HC n'enregistre donc que des rappels « bruts »
  (`wasm_runtime_register_natives_raw`), qui ne passent pas par elle. Elle
  reste compilée parce que WAMR y fait référence. **N'enregistrer aucun
  rappel ordinaire.**

`wamr_plateforme/platform_internal.h` choisit entre Linux et macOS à la
compilation, pour qu'une seule liste de fichiers serve aux deux.

### Comment il se compile

- **les tests et le fuzzing** : `wamr_compile.sh`, appelé par
  `tests/lance.sh` et `tests/fuzz/lance.sh`. Sans nos avertissements ni nos
  sanitizers : ils sont une porte pour NOTRE code ;
- **l'application** : le projet Xcode, avec les mêmes réglages ;
- **`make verifie`, `make avertissements`, `make analyse`** ne le touchent
  pas : ils ne regardent que `HC/hc_*.c` et `HC/hct_*.c`.

### La licence, à la livraison

Apache 2.0 demande que la licence accompagne ce qu'on distribue. Le DMG de
HC devra donc porter `wamr/LICENSE` (et `wamr/ATTRIBUTIONS.md`) : À FAIRE
avant la première version qui contient les modules.

### Mettre à jour WAMR

Reprendre les mêmes fichiers d'une version plus récente, relire le
`git diff`, puis rejouer : `tests/lance.sh`, `tests/lance.sh --asan`, le
banc MAC•CAM et l'arrêt d'une boucle (`docs/mesures/xcmd.txt`), et le job
« interface Cocoa ».
