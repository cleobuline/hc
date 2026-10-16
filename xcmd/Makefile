# Le kit des modules de HC : un fichier C devient un module .wasm.
#
#     make                          tous les exemples
#     make CLANG=~/emsdk/upstream/bin/clang
#                                   avec le clang d'emsdk (macOS)
#
# Il faut un clang qui connaisse la cible wasm32, et son éditeur de liens
# wasm-ld. Ni emcc ni JavaScript : le module ne dépend que de HC.

CLANG   ?= clang
CFLAGS   = --target=wasm32 -O2 -ffreestanding -nostdlib -isystem inc -I . \
           -Wall -Wextra
LIENS    = -Wl,--no-entry -Wl,--strip-debug

MODULES  = exemples/bonjour.wasm

.PHONY: tout propre
tout: $(MODULES)

exemples/%.wasm: exemples/%.c hc_xcmd.c hc_xcmd.h
	$(CLANG) $(CFLAGS) $(LIENS) -o $@ $< hc_xcmd.c

propre:
	rm -f $(MODULES)
