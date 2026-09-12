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

avertissements:
	@for f in $(SOURCES); do \
	   printf '%-16s ' $$(basename $$f); \
	   n=$$($(CC) $(CFLAGS) $(AVERTIR) -c -o /dev/null $$f 2>&1 | grep -c 'warning:'); \
	   echo "$$n"; \
	 done

propre:
	@rm -rf tests/.travail
	@echo "effacé"
