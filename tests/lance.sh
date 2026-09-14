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
# LeakSanitizer ALLUMÉ. Il était éteint depuis que la suite est entrée dans le
# dépôt — un réglage par défaut, pas une décision — et cet angle mort a coûté :
# une correction posée il y a deux jours fuyait 87 octets par appel refusé, et
# le harnais qui exerçait précisément ce cas ne pouvait pas le voir.
#
# Mesuré avant de l'allumer : les 159 harnais ne fuient pas une seule fois. Ça
# ne coûte donc rien, et ça ferme la seule famille de défauts mémoire que les
# sanitizers laissaient passer — ils voyaient déjà les débordements et les
# lectures après libération.
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0
export UBSAN_OPTIONS=print_stacktrace=1

# -w A DISPARU, ET LES ERREURS DE TYPE SONT FATALES.
#
# La suite compilait avec -w, qui éteint TOUS les avertissements. C'était
# confortable et ça coûtait cher : le passage du délimiteur d'items de char à
# const char* a laissé des appels qui passaient encore un caractère. gcc l'a
# signalé, -w l'a tu, et le premier « delete item 2 of … » a déréférencé la
# virgule comme une adresse. Segfault, découvert à l'exécution.
#
# -Werror=… seul n'aurait rien changé : -w supprime l'avertissement AVANT que
# -Werror puisse le promouvoir, et l'un annule l'autre selon l'ordre. Il
# fallait retirer -w.
#
# Mesuré avant de le faire : sans -w, les 176 harnais et le noyau n'émettaient
# que sept avertissements, dont quatre étaient de vrais défauts —
# « \xe2\x80\x9cDepth » où le D de Depth est un chiffre hexadécimal, si bien
# que l'échappement dévorait la lettre et que le harnais ne testait pas les
# guillemets qu'il croyait. Ils sont corrigés ; il ne reste rien à taire, sauf
# format-truncation, deux cas connus et bénins de hc_core.c.
#
# Les trois -Werror ne sont jamais du style : ce sont des bugs, toujours.
CFLAGS="-std=gnu99 -O1 -I$HC -Wno-format-truncation"
CFLAGS="$CFLAGS -Werror=int-conversion -Werror=incompatible-pointer-types"
CFLAGS="$CFLAGS -Werror=implicit-function-declaration"
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
    # Trois harnais cherchaient leur donnée dans le répertoire courant : elle
    # est dans donnees/. Ils ne testaient donc plus rien — ils affichaient
    # « fichier introuvable », et leur fichier de référence enregistrait ce
    # message, si bien qu'ils passaient pour conformes.
    test_exercice|test_exercice2) echo "donnees/exercice.txt" ;;
    rendu)              echo "donnees/arcenciel.txt" ;;
    # Les deux bancs attendaient « draw.txt » depuis toujours, et il n'était
    # nulle part dans le dépôt : ils ne tournaient pas du tout.
    banc|banc_rom)      echo "donnees/draw.txt" ;;
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

  # L'ENTRÉE STANDARD VIENT DE /dev/null, ET CE N'EST PAS UN DÉTAIL.
  #
  # « answer » et « ask » retombent sur la console quand l'hôte ne sait pas
  # ouvrir de dialogue, et la console lit stdin par fgets. Un harnais lancé
  # avec une entrée standard encore OUVERTE — un terminal, un tuyau que
  # personne ne ferme — s'y bloque donc pour de bon : test_v3 et test_v3c,
  # qui tournent en huit millisecondes, se faisaient tuer au bout de deux
  # minutes, selon la façon dont la suite avait été lancée. Le même dépôt
  # rendait « 167 conformes » ou « 2 en échec » sans qu'une ligne ait changé.
  #
  # Ces deux harnais annoncent d'ailleurs « stdin vide -> defaut » dans leur
  # propre titre. Cette ligne rend cette phrase VRAIE, au lieu de l'espérer
  # de l'environnement.
  timeout 120 "$TRAVAIL/bin/$n" $ARGS > "$TRAVAIL/sortie/$n" 2>&1 < /dev/null
  code=$?
  # UN PLANTAGE N'EST PAS UN DÉPASSEMENT DE DÉLAI.
  #
  # Tout code >= 124 était annoncé « DÉLAI DÉPASSÉ ». Or 124 est celui que
  # `timeout` rend quand IL tue le programme ; 128+N est celui d'un programme
  # tué par le signal N — 139 pour un segfault, 134 pour un abort. Les trois
  # se ressemblent en nombre et n'ont rien à voir en cause.
  #
  # Mesuré à mes dépens : un segfault que je venais d'introduire s'est annoncé
  # « DÉLAI DÉPASSÉ », et j'ai cherché une boucle infinie pendant cinq minutes
  # avant de regarder le vrai code de sortie.
  #
  # Et un code entre 1 et 123 — un exit(1) du noyau, par exemple — ne disait
  # rien du tout : le harnais était simplement comparé à sa référence, et si sa
  # sortie partielle correspondait, il passait pour conforme.
  if [ $code -eq 124 ]; then
    echo "  DÉLAI DÉPASSÉ  $n"; rate=$((rate+1)); continue
  fi
  if [ $code -gt 128 ]; then
    sig=$((code - 128))
    nom_sig=$(kill -l $sig 2>/dev/null || echo "signal $sig")
    echo "  TUÉ PAR $nom_sig  $n"; rate=$((rate+1)); continue
  fi
  if [ $code -ne 0 ]; then
    # Un chronomètre dont la donnée manque ne peut pas tourner, et ce n'est pas
    # une régression : sa sortie n'est de toute façon pas comparée. On le DIT —
    # un chronomètre muet depuis des mois est un chronomètre inutile — sans
    # rougir la suite pour autant.
    case "$n" in
      bench*) echo "  NE TOURNE PAS  $n (code $code)"; chrono=$((chrono+1)); continue ;;
    esac
    echo "  CODE $code        $n"; rate=$((rate+1)); continue
  fi

  if grep -qE 'AddressSanitizer|runtime error:|LeakSanitizer' "$TRAVAIL/sortie/$n"; then
    echo "  SANITIZER      $n"
    grep -hE 'ERROR:|runtime error:' "$TRAVAIL/sortie/$n" | sed 's/^/                 /' | head -2
    rate=$((rate+1)); continue
  fi

  # Un CHRONOMÈTRE n'est pas un test de non-régression : sa sortie porte des
  # millisecondes, qui ne sont jamais deux fois les mêmes. On le fait tourner
  # — il doit au moins finir sans planter — mais on ne compare pas.
  #
  # LA RÈGLE EST LE PRÉFIXE, ET ELLE EST VRAIE : « bench » mesure du TEMPS,
  # « banc » mesure un COMPORTEMENT. banc_lignes a donc été renommé
  # bench_lignes — il affichait des millisecondes sous un nom qui promettait
  # l'inverse, et une règle par préfixe qui traîne une liste d'exceptions
  # finit toujours par être fausse quelque part.
  #
  # « banc » et « banc_rom » N'EN SONT PLUS. Ils portaient ce nom, mais leur
  # sortie ne compte que des clics et des tracés : avec l'horloge gelée, elle
  # est identique d'un passage à l'autre — vérifié. Les exempter revenait à
  # jeter le test le plus exigeant de la suite : deux mille tracés produits par
  # un vrai script de 1987, dont le moindre écart d'évaluation se verrait. Le
  # relevé v3/v1 y figure aussi, si bien qu'un retour vers l'ancien moteur qui
  # réapparaîtrait se signalerait tout seul.
  case "$n" in
    bench*) chrono=$((chrono+1)); continue ;;
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
