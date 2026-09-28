#!/bin/sh
# Ôte les commentaires et les lignes vides d'un script HyperTalk.
#
# POURQUOI CET OUTIL EXISTE. Les scripts de la pile de torture font près de
# mille lignes, dont les deux tiers sont des commentaires : ils expliquent
# quelle mesure a produit quelle ligne, et ils ont leur place dans le dépôt.
#
# Mais pour REJOUER le banc dans HyperCard, il faut recopier ces scripts à la
# main dans Basilisk, et là chaque ligne coûte. Les commentaires n'y servent à
# rien : la pile d'HyperCard est un instrument jetable, on la monte, on relève,
# on la jette. Les recopier, c'est payer deux tiers du travail pour zéro mesure.
#
#   ./denude.sh donnees/torture2_bouton.txt > bouton_nu.txt
#
# ON NE TOUCHE QUE LES LIGNES DONT LE PREMIER CARACTÈRE NON BLANC EST « -- ».
# Un « -- » au milieu d'une ligne peut vivre dans une chaîne — « put "a--b" » —
# et le couper changerait le sens du script. Le gain est déjà de deux tiers
# sans prendre ce risque.
set -u
[ $# -eq 1 ] || { echo "usage: $0 script.txt" >&2; exit 2; }
sed -e '/^[[:space:]]*--/d' -e '/^[[:space:]]*$/d' "$1"
