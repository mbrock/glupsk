NIX ?= nix
I7_FLAKE ?= ../../tools/inform7-nix
SOURCE ?= shutdown-garden.ni
STORY ?= shutdown-garden.ulx
GLUPSK_PLAY ?= ../../build/meson/glupsk-play
EXPERIMENT_SOURCES := $(wildcard experiments/*.ni)

I7_CHECK := $(NIX) run $(I7_FLAKE)\#i7-check --
I7_BUILD := $(NIX) run $(I7_FLAKE)\#i7-build --

.PHONY: check experiments glulx routes full-check clean

check:
	$(I7_CHECK) $(SOURCE)

experiments:
	@set -e; \
	for source in $(EXPERIMENT_SOURCES); do \
		echo "Checking $$source"; \
		$(I7_CHECK) "$$source"; \
	done

glulx:
	$(I7_BUILD) --glulx $(SOURCE) $(STORY)

routes: glulx
	./check-routes.sh "$(GLUPSK_PLAY)" "$(STORY)"

full-check: check experiments routes

clean:
	rm -rf build $(STORY) shutdown-garden.i6
