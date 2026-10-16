#!/bin/sh
# wamr_compile.sh <dossier> [drapeaux…] — compile WAMR, l'interprète
# WebAssembly des modules, en un objet par fichier dans <dossier>.
#
# Appelé par tests/lance.sh et tests/fuzz/lance.sh, qui lient ensuite ces
# objets avec le noyau. Les drapeaux supplémentaires (les sanitizers de
# --asan) s'ajoutent aux nôtres. Un objet plus récent que son source et que
# les réglages n'est pas recompilé : WAMR compte trente-six fichiers.
#
# C'est du code tiers : il se compile SANS nos avertissements (-w), qui sont
# une porte pour NOTRE code. Il ne passe donc ni par « make avertissements »
# ni par « make analyse ». La liste des fichiers est wamr_sources.txt, et le
# projet Xcode en porte une copie : un site jumeau.

set -e
ICI=$(cd "$(dirname "$0")" && pwd)
HC=$(cd "$ICI/.." && pwd)
SORTIE=$1
shift
mkdir -p "$SORTIE"

W="$ICI/wamr/core"
INC="-I$ICI/wamr_plateforme -I$W -I$W/iwasm/include -I$W/iwasm/common"
INC="$INC -I$W/iwasm/interpreter -I$W/iwasm/libraries/thread-mgr"
INC="$INC -I$W/shared/utils -I$W/shared/mem-alloc -I$W/shared/platform/include"

# Les drapeaux font partie de l'objet : changés, tout se recompile.
SIGNATURE="$SORTIE/.drapeaux"
VOULUS="$* $(cat "$ICI/wamr_hc.h" | cksum)"
if [ ! -f "$SIGNATURE" ] || [ "$(cat "$SIGNATURE")" != "$VOULUS" ]; then
  rm -f "$SORTIE"/*.o
  printf '%s' "$VOULUS" > "$SIGNATURE"
fi

while read -r f; do
  o="$SORTIE/$(basename "$f" .c).o"
  if [ ! -f "$o" ] || [ "$HC/$f" -nt "$o" ]; then
    # shellcheck disable=SC2086
    cc -std=gnu99 -O2 -w -include "$ICI/wamr_hc.h" $INC "$@" -c "$HC/$f" -o "$o"
  fi
done < "$ICI/wamr_sources.txt"
