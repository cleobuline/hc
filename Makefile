# HC — un clone d'HyperCard.
#
# L'application elle-même se construit avec Xcode (HC.xcodeproj) : elle a
# besoin de Cocoa. Ce Makefile ne sert qu'au NOYAU et à sa suite de tests,
# qui sont du C99 pur et se compilent partout — c'est ce qui permet de les
# faire tourner en intégration continue, sur une machine sans AppKit.

CC      ?= cc
CFLAGS  ?= -std=gnu99 -O2 -I HC
AVERTIR  = -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-qual -Wwrite-strings \
           -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition

# LES AVERTISSEMENTS SE COMPTENT À PLUSIEURS NIVEAUX D'OPTIMISATION.
#
# Certains ne se voient qu'à un seul, parce qu'ils dépendent de ce que le
# compilateur a su propager : -Wformat-truncation a besoin des bornes, et les
# bornes viennent de l'analyse de flot, qui change avec -O.
#
# Mesuré, et ce n'est pas théorique : « delete menu "<nom de plus de 63
# caractères>" » recopiait le nom dans un tampon de 64 octets et ne retrouvait
# plus le menu — il survivait, « the result » annonçant la réussite. gcc le
# disait, « output between 1 and 256 bytes into a destination of size 64 », À
# -O0 SEULEMENT. Cette cible compilait en -O2, donc personne ne l'a jamais lu.
#
# Trois niveaux suffisent : -O3 n'a rien montré que -O2 ne montrait déjà, et
# chaque niveau coûte une compilation complète du noyau.
NIVEAUX  = -O0 -O1 -O2
BASE     = -std=gnu99 -I HC

SOURCES  = $(wildcard HC/hc_*.c HC/hct_*.c)

.PHONY: test test-asan test-enregistre verifie avertissements propre aide

aide:
	@echo "make test              la suite de non-régression"
	@echo "make test-asan         la même sous AddressSanitizer et UBSan"
	@echo "make test-enregistre   remet à jour les sorties de référence"
	@echo "make verifie           compile le noyau, sans rien produire"
	@echo "make avertissements    compile sous huit familles d'avertissements"
	@echo "make propre            efface les objets de test"

test:
	@tests/lance.sh

test-asan:
	@tests/lance.sh --asan

test-enregistre:
	@tests/lance.sh --enregistre

verifie:
	@for f in $(SOURCES); do $(CC) $(CFLAGS) -fsyntax-only $$f || exit 1; done
	@echo "le noyau compile"

# LA CIBLE ÉCHOUE S'IL EN RESTE UN.
#
# Elle se contentait de COMPTER et rendait toujours zéro : l'intégration
# continue affichait fidèlement « hc_core.c 3 » et passait au vert. Un
# compteur que personne ne regarde ne compte rien ; c'est exactement ainsi
# qu'on se réhabitue à des avertissements, et la suite a déjà payé ce
# prix-là — le -w de lance.sh cachait un déréférencement de virgule.
#
# Le détail des avertissements est réaffiché pour le fichier fautif : un
# nombre sans le message n'aide personne à corriger.
avertissements:
	@echec=0; \
	 for f in $(SOURCES); do \
	   printf '%-20s ' $$(basename $$f); \
	   detail=''; \
	   for o in $(NIVEAUX); do \
	     msg=$$($(CC) $(BASE) $$o $(AVERTIR) -c -o /dev/null $$f 2>&1); \
	     n=$$(printf '%s\n' "$$msg" | grep -c 'warning:'); \
	     printf '%s:%s ' "$$o" "$$n"; \
	     if [ "$$n" -ne 0 ]; then \
	       echec=1; \
	       detail="$$detail\n--- $$f $$o ---\n$$msg"; \
	     fi; \
	   done; \
	   echo ""; \
	   if [ -n "$$detail" ]; then printf '%b\n' "$$detail"; fi; \
	 done; \
	 if [ $$echec -ne 0 ]; then \
	   echo "des avertissements : la cible echoue"; exit 1; \
	 fi; \
	 echo "aucun avertissement, aux trois niveaux"

propre:
	@rm -rf tests/.travail
	@echo "effacé"
