#!/bin/sh
# refais.sh — reconstruit les modules d'épreuve du harnais xcmd avec le kit
# (xcmd/). Les .wasm sont commis : la suite de tests n'a pas besoin d'un
# clang qui connaisse wasm32. À relancer après avoir touché un source ou le
# kit, puis relire le diff des témoins.
#
#     tests/donnees/modules/refais.sh
#     CLANG=~/emsdk/upstream/bin/clang tests/donnees/modules/refais.sh
set -e
ICI=$(cd "$(dirname "$0")" && pwd)
KIT="$ICI/../../../xcmd"
CLANG=${CLANG:-clang}
C="--target=wasm32 -O2 -ffreestanding -nostdlib -isystem $KIT/inc -I $KIT -Wall -Wextra"
L="-Wl,--no-entry -Wl,--strip-debug"
make -s -C "$KIT" CLANG="$CLANG"
cp "$KIT/exemples/bonjour.wasm" "$ICI/bonjour.wasm"
# shellcheck disable=SC2086
$CLANG $C $L -o "$ICI/epreuves.wasm" "$ICI/epreuves.c" "$KIT/hc_xcmd.c"
# shellcheck disable=SC2086
$CLANG $C $L -o "$ICI/inconnu.wasm" "$ICI/inconnu.c" "$KIT/hc_xcmd.c"
$CLANG --target=wasm32 -O2 -nostdlib -Wall -Wextra $L -o "$ICI/sansboite.wasm" "$ICI/sansboite.c"
ls -l "$ICI"/*.wasm
