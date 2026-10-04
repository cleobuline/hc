#!/bin/sh
# Le fuzzing des deux portes de fichiers de HC — à passer avant une version.
#
#   tests/fuzz/lance.sh            20 000 essais par processus
#   tests/fuzz/lance.sh 100000     davantage
#
# QUATRE TEMPS, et le troisième n'est pas facultatif :
#
#   1. compiler le pilote sous ASan, UBSan (float-cast-overflow compris) et
#      LeakSanitizer ;
#   2. récolter les graines : la suite tourne, et chaque pile qu'un harnais
#      enregistre puis efface est gardée (garde.c). On y ajoute le pendu, une
#      pile enrichie des lignes que la suite n'écrit pas, et pour le format
#      d'Apple la pile synthétique du harnais origine, nue et en MacBinary ;
#   3. LE TÉMOIN : trois graines piégées — un débordement, une fuite, une
#      boucle sans fin — DOIVENT être signalées, et une graine saine ne doit
#      pas l'être. Sinon le script s'arrête sans verdict : un instrument sourd
#      rend des zéros, et le 4 octobre il en a rendu 400 000
#      (docs/mesures/fuzzing.txt) ;
#   4. la campagne : un processus par cœur sur notre format, un sur celui
#      d'Apple. Le script échoue s'il reste un seul cas signalé ; les fichiers
#      et les rapports sont dans tests/.travail/fuzz/campagne/.
#
# Pour rejouer un cas, sans fork, sous les mêmes sanitizers :
#
#   tests/.travail/fuzz/fuzz un hc   tests/.travail/fuzz/campagne/hc1/cas_…bin
#   tests/.travail/fuzz/fuzz un orig tests/.travail/fuzz/campagne/orig/cas_…bin
set -u
ICI=$(cd "$(dirname "$0")" && pwd)
RACINE=$(cd "$ICI/../.." && pwd)
HC="$RACINE/HC"
T="$RACINE/tests/.travail/fuzz"
ESSAIS="${1:-20000}"

export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export HC_HORLOGE=1757606400 TZ=UTC LC_ALL=C

mkdir -p "$T"
rm -rf -- "${T:?}/graines_hc" "${T:?}/graines_orig" "${T:?}/temoin" "${T:?}/campagne"
mkdir -p "$T/graines_hc" "$T/graines_orig" "$T/temoin/graines" "$T/temoin/sortie" "$T/campagne"

NOYAU="$HC/hc_core.c $HC/hc_file.c $HC/hc_icons.c $HC/hc_importe.c $HC/hc_origine.c
       $HC/hc_presse_papiers.c $HC/hc_script.c $(ls "$HC"/hct_*.c | tr '\n' ' ')"
BASE="-std=gnu99 -O1 -I$HC -Wall -Wextra"
SAN="-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -g"

# ── 1. compiler ─────────────────────────────────────────────────────────────
printf 'compilation................... '
# shellcheck disable=SC2086
cc $BASE $SAN -o "$T/fuzz"        "$ICI/fuzz.c" $NOYAU -lm -lz &&
cc $BASE $SAN -DCANARI -o "$T/fuzz_temoin" "$ICI/fuzz.c" $NOYAU -lm -lz &&
cc $BASE -o "$T/graine_mb" "$ICI/graine_mb.c" $NOYAU -lm -lz &&
cc -Wall -Wextra -O1 -shared -fPIC -o "$T/garde.so" "$ICI/garde.c" -ldl ||
  { echo "ÉCHEC"; exit 1; }
echo "fait"

# ── 2. récolter les graines ─────────────────────────────────────────────────
printf 'graines (la suite tourne)..... '
SUITE=$(GARDE="$T/graines_hc" LD_PRELOAD="$T/garde.so" HC_TESTS_TRAVAIL="$T/suite" \
        "$RACINE/tests/lance.sh" 2>&1 | tail -1)
case "$SUITE" in
  *"en échec : 0"*) ;;
  *) echo "la suite échoue : $SUITE"; exit 1 ;;
esac
"$T/suite/bin/pendu" "$RACINE/tests/donnees/pendu_pile.txt" "$T/graines_hc/pendu.stack" \
  > /dev/null 2>&1

# Les lignes que la suite n'enregistre jamais, posées dans une pile qui porte
# déjà un champ et un bouton.
for g in "$T"/graines_hc/*; do
  if grep -q '^end field' "$g" && grep -q '^end button' "$g"; then
    awk 'BEGIN { c = 0; b = 0; k = 0 }
         /^end card/   && !k { print "bghilite 1"; k = 1 }
         /^end field/  && !c { print "scroll 3\nselectedline 2\nwidemargins\nautoselect\nmarked\nforealpha 100"; c = 1 }
         /^end button/ && !b { print "unsharedhilite\nforealpha 40\nhilitealpha 7\nbackalpha 128\npoints 100,100 0,0 100,0 50,100"; b = 1 }
         { print }' "$g" > "$T/graines_hc/enrichie.stack"
    break
  fi
done

"$T/suite/bin/origine" "$T/graines_orig/synthetique.stak" > /dev/null 2>&1
"$T/graine_mb" "$T/graines_orig/synthetique.stak" "$T/graines_orig/synthetique" ||
  { echo "graines MacBinary : ÉCHEC"; exit 1; }
NHC=$(ls "$T/graines_hc" | wc -l | tr -d ' ')
NOR=$(ls "$T/graines_orig" | wc -l | tr -d ' ')
echo "$NHC dans notre format, $NOR dans celui d'Apple"
[ "$NHC" -ge 20 ] && [ -f "$T/graines_hc/enrichie.stack" ] && [ "$NOR" -ge 3 ] ||
  { echo "trop peu de graines : la récolte n'a pas marché"; exit 1; }

# ── 3. le témoin ────────────────────────────────────────────────────────────
printf 'témoin........................ '
SAINE="$T/graines_hc/enrichie.stack"
for piege in DEBORDE FUIT BOUCLE; do
  { echo "CANARI_$piege"; cat "$SAINE"; } > "$T/temoin/graines/canari_$piege.stack"
done
cp "$SAINE" "$T/temoin/graines/saine.stack"
VERDICT=$(FZ_ALARME=2 "$T/fuzz_temoin" temoin hc "$T/temoin/graines" "$T/temoin/sortie")
ATTRAPES=$(printf '%s\n' "$VERDICT" | grep -c '^canari_.*SIGNALÉ')
SAINE_OK=$(printf '%s\n' "$VERDICT" | grep -c '^saine.stack.*propre')
if [ "$ATTRAPES" -ne 3 ] || [ "$SAINE_OK" -ne 1 ]; then
  echo "L'INSTRUMENT EST SOURD — pas de campagne, pas de verdict :"
  printf '%s\n' "$VERDICT" | sed 's/^/   /'
  exit 1
fi
echo "les trois pièges attrapés, la graine saine laissée"

# ── 4. la campagne ──────────────────────────────────────────────────────────
CPU=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
NPROC=$((CPU > 1 ? CPU - 1 : 1))
echo "campagne : $NPROC × $ESSAIS dans notre format, $ESSAIS dans celui d'Apple"
i=1
while [ "$i" -le "$NPROC" ]; do
  mkdir -p "$T/campagne/hc$i"
  "$T/fuzz" hc "$T/graines_hc" "$ESSAIS" "$T/campagne/hc$i" "$i" \
    > "$T/campagne/hc$i.bilan" 2> "$T/campagne/hc$i.journal" &
  i=$((i + 1))
done
mkdir -p "$T/campagne/orig"
"$T/fuzz" orig "$T/graines_orig" "$ESSAIS" "$T/campagne/orig" 4242 \
  > "$T/campagne/orig.bilan" 2> "$T/campagne/orig.journal" &
wait

TOTAL=0
SIGNALES=0
for b in "$T"/campagne/*.bilan; do
  read -r porte n m < "$b" || { echo "un processus n'a pas fini : $b"; exit 1; }
  printf '   %-5s %8s essais   %s signalé(s)\n' "$porte" "$n" "$m"
  TOTAL=$((TOTAL + n))
  SIGNALES=$((SIGNALES + m))
done
echo
if [ "$SIGNALES" -ne 0 ]; then
  echo "$SIGNALES cas sur $TOTAL essais — fichiers et rapports :"
  ls "$T"/campagne/*/cas_*.bin | sed 's/^/   /'
  exit 1
fi
echo "aucun signalement sur $TOTAL essais"
