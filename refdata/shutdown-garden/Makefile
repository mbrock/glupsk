NIX ?= nix
I7_FLAKE ?= ../../tools/inform7-nix
SOURCE ?= shutdown-garden.ni
STORY ?= shutdown-garden.ulx

I7_CHECK := $(NIX) run $(I7_FLAKE)\#i7-check --
I7_BUILD := $(NIX) run $(I7_FLAKE)\#i7-build --

.PHONY: check glulx clean

check:
	$(I7_CHECK) $(SOURCE)

glulx:
	$(I7_BUILD) --glulx $(SOURCE) $(STORY)

clean:
	rm -rf build $(STORY) shutdown-garden.i6
