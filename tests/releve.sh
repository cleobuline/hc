#!/bin/sh
# Le relevé du CORPUS ENTIER : qu'est-ce qui passe encore par du TEXTE ?
#
# L'ancien moteur d'expressions a été retiré le 2 octobre — voir
# docs/mesures/sansv1.txt. Il ne reste à compter que ce qui RELIT du texte
# (« v3 relit », eval_expr appelée sur une chaîne) et les lignes REFUSÉES
# (« v1 refusée », là où l'on passait autrefois la main à l'ancien code).
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
# LIRE LES DEUX TABLEAUX. « recours » compte les fois où l'évaluateur a
# RENONCÉ et passé la main au recours ; « exec » compte ce qui a réellement
# relu du texte ou été refusé, d'où qu'on vienne. Les confondre a fait croire,
# du temps de l'ancien moteur, le chantier plus avancé qu'il n'était.
set -u
ICI=$(cd "$(dirname "$0")" && pwd)
TRAVAIL="${HC_TESTS_TRAVAIL:-$ICI/.travail}"
R="${HC_RELEVE_FICHIER:-$TRAVAIL/releve.tsv}"

"$ICI/lance.sh" >/dev/null 2>&1 || true
[ -d "$TRAVAIL/bin" ] || { echo "rien à mesurer : lancez ./lance.sh d'abord" >&2; exit 1; }

. "$ICI/arguments.sh"
rm -f "$R"
n=0
# N'EXÉCUTER QUE DES HARNAIS QUI EXISTENT ENCORE.
#
# .travail/bin garde les binaires des exécutions précédentes, y compris ceux
# des harnais renommés ou supprimés depuis. Le relevé les lançait, et comptait
# donc un corpus qui n'est plus celui du dépôt : mesuré, 193 programmes au lieu
# de 192, dont « banc_lignes », renommé « bench_lignes » il y a des jours.
#
# Un clone neuf ne voyait rien — c'est un arbre de travail qui dérive. Et un
# instrument qui mesure autre chose que ce qu'il annonce est précisément ce
# qu'on ne veut pas ici : on l'aligne sur les SOURCES.
for b in "$TRAVAIL"/bin/*; do
    [ -x "$b" ] || continue
    [ -f "$ICI/harnais/$(basename "$b").c" ] || continue
    # AVEC LEURS ARGUMENTS, comme lance.sh — voir arguments.sh.
    ARGS=$(arguments "$(basename "$b")")
    HC_V3_RELEVE="$R" HC_V3_RELEVE_QUI="$(basename "$b")" \
    HC_HORLOGE=1757606400 TZ=UTC LC_ALL=C \
        timeout 30 "$b" $ARGS >/dev/null 2>&1 </dev/null
    n=$((n + 1))
done
touch "$R"

if [ "${1:-}" = "--brut" ]; then
    echo "$R  ($n harnais)"
    exit 0
fi

echo "— ce qui relit encore du texte, et ce qui est refusé ($n harnais) —"
awk -F'\t' '$2=="exec"{a[$3]+=$4} END{for(k in a) printf "%7d  %s\n", a[k], k}' "$R" \
    | sort -rn | head -40
echo
echo "— les recours, par catégorie —"
awk -F'\t' '$2=="recours"{a[$3]+=$4} END{for(k in a) printf "%7d  %s\n", a[k], k}' "$R" \
    | sort -rn | head -25
echo
echo "— combien de harnais relisent du texte ou essuient un refus —"
printf "   relisent du texte : %s\n" "$(awk -F'\t' '$2=="exec" && $3 ~ /^v3 relit/{print $1}' "$R" | sort -u | wc -l)"
printf "   essuient un refus : %s\n" "$(awk -F'\t' '$2=="exec" && $3 ~ /^v1 refusée/{print $1}' "$R" | sort -u | wc -l)"
printf "   sur %s au total\n" "$n"
