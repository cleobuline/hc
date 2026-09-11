#!/bin/sh
# Suite de non-régression de HC — sans interface, donc exécutable partout.
#
# Chaque harnais est un programme C autonome qui construit une pile en
# mémoire, y fait tourner du HyperTalk, et écrit ce qu'il observe. Sa sortie
# est comparée octet pour octet à un fichier de référence rangé dans attendu/.
#
#   ./lance.sh              compare à attendu/
#   ./lance.sh --enregistre remplit attendu/ depuis l'état courant
#   ./lance.sh --asan       recompile sous les sanitizers et signale
#   ./lance.sh <motif>      ne traite que les harnais dont le nom contient <motif>
#
# L'HORLOGE EST GELÉE (HC_HORLOGE) et le fuseau fixé (TZ=UTC) : sans cela un
# test qui affiche une date passerait aujourd'hui et échouerait demain, ce qui
# revient à ne pas pouvoir le versionner.
set -u
ICI=$(cd "$(dirname "$0")" && pwd)
HC="$ICI/../HC"
TRAVAIL="${HC_TESTS_TRAVAIL:-$ICI/.travail}"

export HC_HORLOGE=1757606400      # 11 septembre 2025, 16:00 UTC
export TZ=UTC
export LC_ALL=C
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=0
export UBSAN_OPTIONS=print_stacktrace=1

CFLAGS="-std=gnu99 -w -O1 -I$HC"
ENREGISTRE=0
MOTIF=""
for a in "$@"; do
  case "$a" in
    --enregistre) ENREGISTRE=1 ;;
    --asan)       CFLAGS="$CFLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -g" ;;
    --*)          echo "option inconnue : $a" >&2; exit 2 ;;
    *)            MOTIF="$a" ;;
  esac
done

UNITES="hc_core hc_file hc_icons hct_arbre hct_bloc hct_chunk hct_cmd hct_eval hct_exec hct_expr hct_lex hct_val hct_verif"

mkdir -p "$TRAVAIL/obj" "$TRAVAIL/bin" "$TRAVAIL/sortie" "$ICI/attendu"

printf 'compilation du noyau'
for u in $UNITES; do
  cc $CFLAGS -c "$HC/$u.c" -o "$TRAVAIL/obj/$u.o" || { echo " — ÉCHEC sur $u"; exit 1; }
  printf '.'
done
echo ' fait'

# Les harnais qui attendent un fichier en argument, et lequel.
arguments() {
  case "$1" in
    calreel)            echo "donnees/calendrier.txt" ;;
    navharn|navharn2|navharn3) echo "donnees/navtest.txt" ;;
    tortureh)           echo "donnees/torture_bouton.txt donnees/torture_pile.txt" ;;
    quelgest|analyse)   echo "donnees/rawchart.txt" ;;
    profond)            echo "donnees/endmanquant.txt" ;;
    *)                  echo "" ;;
  esac
}

ok=0; rate=0; neuf=0; saute=0; chrono=0
cd "$ICI"
for src in harnais/*.c; do
  n=$(basename "$src" .c)
  [ -n "$MOTIF" ] && case "$n" in *"$MOTIF"*) ;; *) continue ;; esac

  if ! cc $CFLAGS -o "$TRAVAIL/bin/$n" "$src" "$TRAVAIL"/obj/*.o -lm 2>/dev/null; then
    echo "  NE COMPILE PAS  $n"; rate=$((rate+1)); continue
  fi

  ARGS=$(arguments "$n")
  # Un harnais qui exige un argument qu'on ne sait pas lui donner est écarté,
  # plutôt que compté en échec : il n'a pas de sens hors de son contexte.
  if grep -q 'argc *< *[0-9]' "$src" && [ -z "$ARGS" ]; then
    saute=$((saute+1)); continue
  fi

  timeout 120 "$TRAVAIL/bin/$n" $ARGS > "$TRAVAIL/sortie/$n" 2>&1
  code=$?
  if [ $code -ge 124 ]; then
    echo "  DÉLAI DÉPASSÉ  $n"; rate=$((rate+1)); continue
  fi

  if grep -qE 'AddressSanitizer|runtime error:|LeakSanitizer' "$TRAVAIL/sortie/$n"; then
    echo "  SANITIZER      $n"
    grep -hE 'ERROR:|runtime error:' "$TRAVAIL/sortie/$n" | sed 's/^/                 /' | head -2
    rate=$((rate+1)); continue
  fi

  # Un CHRONOMÈTRE n'est pas un test de non-régression : sa sortie porte des
  # millisecondes, qui ne sont jamais deux fois les mêmes. On le fait tourner
  # — il doit au moins finir sans planter — mais on ne compare pas.
  case "$n" in
    bench*|banc*) chrono=$((chrono+1)); continue ;;
  esac

  ref="attendu/$n.txt"
  if [ "$ENREGISTRE" = 1 ]; then
    cp "$TRAVAIL/sortie/$n" "$ref"; neuf=$((neuf+1)); continue
  fi
  if [ ! -f "$ref" ]; then
    echo "  SANS RÉFÉRENCE $n   (lancez ./lance.sh --enregistre)"; saute=$((saute+1)); continue
  fi
  if cmp -s "$TRAVAIL/sortie/$n" "$ref"; then
    ok=$((ok+1))
  else
    echo "  DIFFÉRENT      $n"
    diff "$ref" "$TRAVAIL/sortie/$n" | head -8 | sed 's/^/                 /'
    rate=$((rate+1))
  fi
done

echo
if [ "$ENREGISTRE" = 1 ]; then
  echo "références enregistrées : $neuf"
  exit 0
fi
echo "conformes : $ok    en échec : $rate    écartés : $saute    chronomètres : $chrono"
[ "$rate" -eq 0 ]
