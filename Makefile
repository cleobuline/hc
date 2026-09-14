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

SOURCES  = $(wildcard HC/hc_core.c HC/hc_file.c HC/hc_icons.c HC/hct_*.c)

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
	   printf '%-16s ' $$(basename $$f); \
	   msg=$$($(CC) $(CFLAGS) $(AVERTIR) -c -o /dev/null $$f 2>&1); \
	   n=$$(printf '%s\n' "$$msg" | grep -c 'warning:'); \
	   echo "$$n"; \
	   if [ "$$n" -ne 0 ]; then echec=1; printf '%s\n' "$$msg"; fi; \
	 done; \
	 if [ $$echec -ne 0 ]; then \
	   echo "des avertissements : la cible echoue"; exit 1; \
	 fi; \
	 echo "aucun avertissement"

propre:
	@rm -rf tests/.travail
	@echo "effacé"
