#!/bin/sh
# Le délai d'un harnais — partagé par lance.sh et releve.sh.
#
# `timeout` N'EST PAS PARTOUT : il vient des coreutils de GNU, et macOS ne le
# fournit pas. Sans ce fichier, chaque harnais rendait 127 — « commande
# introuvable » — et la suite annonçait « 333 en échec » sur un arbre où rien
# n'était cassé. C'est le pire échec pour une porte : elle répond fort, et à
# côté de la question.
#
# Partagé plutôt que recopié, pour la même raison qu'arguments.sh : les deux
# scripts doivent poser le MÊME délai par le même moyen, sinon un harnais qui
# se bloque sous l'un passe inaperçu sous l'autre.
#
# Le sourcer pose DELAI, qui s'appelle « $DELAI <secondes> cmd… ». Par ordre
# de préférence :
#
#   timeout      coreutils de GNU, présent sous Linux et en CI
#   gtimeout     le même, tel que le coreutils de Homebrew l'installe sur macOS
#   delai_perl   le repli ci-dessous : rien à installer
#
# Le repli garde les seuls codes de sortie que les appelants distinguent : 124
# quand C'EST LUI qui tue l'enfant, 128+N quand c'est un signal (139 faute de
# segmentation, 134 abort), et le code de l'enfant sinon. Se tromper ici ferait
# d'un plantage un « DÉLAI DÉPASSÉ » — la confusion que lance.sh a déjà payée
# une fois.
delai_perl() {
  duree=$1; shift
  perl -e '
    my $d = shift;
    my $pid = fork(); die "fork: $!\n" unless defined $pid;
    if (!$pid) { exec @ARGV; exit 127 }
    $SIG{ALRM} = sub {
      kill "TERM", $pid; select undef, undef, undef, 0.5;
      kill "KILL", $pid; waitpid $pid, 0; exit 124;
    };
    alarm $d; waitpid $pid, 0; my $s = $?; alarm 0;
    exit(($s & 127) ? 128 + ($s & 127) : ($s >> 8));
  ' "$duree" "$@"
}

if command -v timeout >/dev/null 2>&1;    then DELAI=timeout
elif command -v gtimeout >/dev/null 2>&1; then DELAI=gtimeout
elif command -v perl >/dev/null 2>&1;     then DELAI=delai_perl
else
  echo "ni timeout, ni gtimeout, ni perl : aucun délai ne peut être posé" >&2
  echo "une porte qui ne pose pas sa question n'est pas une porte" >&2
  exit 1
fi
