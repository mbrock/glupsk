NIX ?= nix

AA_SOURCE := refdata/aa/aa.ni
AA_STORY := refdata/aa.ulx
I7_FLAKE := ./tools/inform7-nix

.PHONY: all aa clean-aa

all: aa

aa: $(AA_STORY)

$(AA_STORY): $(AA_SOURCE) tools/inform7-nix/flake.nix tools/inform7-nix/flake.lock
	$(NIX) run $(I7_FLAKE)#i7-build -- --glulx $(AA_SOURCE) $@

clean-aa:
	rm -f $(AA_STORY)
