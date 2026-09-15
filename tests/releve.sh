#!/bin/sh
# Le relevé du CORPUS ENTIER : qu'est-ce qui appelle encore l'ancien moteur ?
#
#   ./releve.sh            agrège les 192 harnais et imprime les tableaux
#   ./releve.sh --brut     laisse le fichier tabulé, pour trier autrement
#
# Pourquoi pas « debug bilan » : il écrit sur la sortie du harnais, donc dans
# sa référence. Le mettre partout voudrait dire réenregistrer 189 références,
# et une référence réenregistrée en masse ne prouve plus rien. Mesuré : 71
# harnais sur 192 impriment un bilan, et chacun remet ses compteurs à zéro.
# Les 121 autres étaient un angle mort — et c'est exactement là que les
# surprises se logent.
#
# On passe donc par HC_V3_RELEVE, qui fait écrire chaque processus dans un
# FICHIER à sa sortie. Rien sur la sortie standard, aucune référence touchée,
# un enregistrement par harnais.
#
# LIRE LES DEUX TABLEAUX. « recours » compte les fois où la v3 a RENONCÉ ;
# « exec » compte ce que l'ancien moteur a RÉELLEMENT exécuté, d'où qu'on
# vienne. Les confondre fait croire le chantier plus avancé qu'il n'est :
# « (aucun) recours » ne veut pas dire que l'ancien code ne tourne plus.
set -u
ICI=$(cd "$(dirname "$0")" && pwd)
TRAVAIL="${HC_TESTS_TRAVAIL:-$ICI/.travail}"
R="${HC_RELEVE_FICHIER:-$TRAVAIL/releve.tsv}"

"$ICI/lance.sh" >/dev/null 2>&1 || true
[ -d "$TRAVAIL/bin" ] || { echo "rien à mesurer : lancez ./lance.sh d'abord" >&2; exit 1; }

rm -f "$R"
n=0
for b in "$TRAVAIL"/bin/*; do
    [ -x "$b" ] || continue
    HC_V3_RELEVE="$R" HC_V3_RELEVE_QUI="$(basename "$b")" \
    HC_HORLOGE=1757606400 TZ=UTC LC_ALL=C \
        timeout 30 "$b" >/dev/null 2>&1 </dev/null
    n=$((n + 1))
done
touch "$R"

if [ "${1:-}" = "--brut" ]; then
    echo "$R  ($n harnais)"
    exit 0
fi

echo "— ce que l'ancien moteur exécute encore ($n harnais) —"
awk -F'\t' '$2=="exec"{a[$3]+=$4} END{for(k in a) printf "%7d  %s\n", a[k], k}' "$R" \
    | sort -rn | head -40
echo
echo "— les recours, par catégorie —"
awk -F'\t' '$2=="recours"{a[$3]+=$4} END{for(k in a) printf "%7d  %s\n", a[k], k}' "$R" \
    | sort -rn | head -25
echo
echo "— la part des SONDES DE NOM, qui ne coûtent qu'une fois par nom —"
awk -F'\t' '
  $2=="exec" && $3 ~ /^v1 (terme|fonction)/ { tout += $4; if ($3 !~ /recours/) sonde += $4 }
  END { printf "   entrées dans term_value / call_function : %d\n", tout;
        printf "   dont sondes de nom                      : %d (%.0f%%)\n",
               sonde, tout ? 100.0 * sonde / tout : 0 }' "$R"
echo
echo "— combien de harnais touchent encore l'ancien moteur —"
printf "   au total         : %s\n" "$(awk -F'\t' '$2=="exec"{print $1}' "$R" | sort -u | wc -l)"
printf "   par le recours   : %s\n" "$(awk -F'\t' '$2=="exec" && $3 ~ /recours expr/{print $1}' "$R" | sort -u | wc -l)"
printf "   sur %s au total\n" "$n"
