#!/bin/sh
# Isole UNE section de la pile de torture dans son propre bouton, avec des
# marqueurs qui disent où l'exécution s'arrête.
#
#   ./isole.sh donnees/torture2_bouton.txt sectionChemins > sept.txt
#
# POURQUOI CET OUTIL EXISTE. Le gros script se recopie mal dans Basilisk — 291
# lignes par le presse-papiers d'un émulateur, et un collage tronqué donne
# « Expected "end" after "on" » ou, pire, une section muette qui a l'air d'un
# défaut du code. Trois allers-retours ont été perdus à chercher dans HC un
# « egal » qu'HyperCard refusait, alors que la section n'avait jamais été
# collée en entier.
#
# Une section seule fait cinquante lignes : elle se colle d'un coup, et ses
# QUATRE MARQUEURS répondent à la question qu'on se pose vraiment —
#
#     aucun marqueur        le script de la PILE est en cause (journal)
#     « le bouton demarre » l'appel échoue, pas le contenu
#     « on entre »          c'est bien dans la section, et le dernier « ok »
#                           dit à quelle ligne
#     les quatre            la section est bonne : c'est la GROSSE copie qui
#                           est abîmée, pas le code
#
# C'est le même geste que denude.sh : rendre la recopie assez petite pour
# qu'elle cesse d'être une source d'erreurs qu'on prend pour des mesures.
set -u
[ $# -eq 2 ] || { echo "usage: $0 script.txt <nomDeSection>" >&2; exit 2; }
F=$1
S=$2
ICI=$(cd "$(dirname "$0")" && pwd)

grep -q "^on $S\$" "$F" || { echo "$0 : pas de « on $S » dans $F" >&2; exit 1; }

cat <<EOT
on mouseUp
  put empty into card field "R"
  razCompteurs
  set the lockErrorDialogs to true
  journal "*** le bouton demarre ***"
  seule
  journal "*** le bouton finit ***"
  bilan
end mouseUp

EOT
# Le corps de la section, renommé « seule » et encadré de deux marqueurs.
"$ICI/denude.sh" "$F" | sed -n "/^on $S\$/,/^end $S\$/p" \
  | sed -e "s/^on $S\$/on seule\n  journal \"*** on entre dans $S ***\"/" \
        -e "s/^end $S\$/  journal \"*** on sort de $S ***\"\nend seule/"
