NIX ?= nix
MESON ?= meson

AA_SOURCE := refdata/aa/aa.ni
AA_STORY := refdata/aa.ulx
TINY_I7_SOURCE := refdata/tiny-i7/apple.ni
TINY_I7_STORY := refdata/tiny-i7/apple.ulx
I7_FLAKE := ./tools/inform7-nix

BUILD_DIR := build
MESON_BUILD_DIR := $(BUILD_DIR)/meson
MESON_BUILD_FILE := $(MESON_BUILD_DIR)/build.ninja
MESON_COMPILE_COMMANDS := $(MESON_BUILD_DIR)/compile_commands.json

GLUPSK_INFO := $(MESON_BUILD_DIR)/glupsk-info
GLUPSK_TRACE := $(MESON_BUILD_DIR)/glupsk-trace

.PHONY: all aa clean clean-aa clean-tiny-i7 compile-commands info meson meson-setup meson-test test tiny-i7 trace

all: meson

aa: $(AA_STORY)

tiny-i7: $(TINY_I7_STORY)

$(AA_STORY): $(AA_SOURCE) tools/inform7-nix/flake.nix tools/inform7-nix/flake.lock
	$(NIX) run $(I7_FLAKE)#i7-build -- --glulx $(AA_SOURCE) $@

$(TINY_I7_STORY): $(TINY_I7_SOURCE) tools/inform7-nix/flake.nix tools/inform7-nix/flake.lock
	$(NIX) run $(I7_FLAKE)#i7-build -- --glulx $(TINY_I7_SOURCE) $@

$(MESON_BUILD_FILE): meson.build
	@if [ -f "$(MESON_BUILD_FILE)" ]; then \
		$(MESON) setup --reconfigure "$(MESON_BUILD_DIR)"; \
	else \
		$(MESON) setup "$(MESON_BUILD_DIR)"; \
	fi

meson-setup: $(MESON_BUILD_FILE)
	ln -sf "$(MESON_COMPILE_COMMANDS)" compile_commands.json

meson: meson-setup
	$(MESON) compile -C "$(MESON_BUILD_DIR)"

meson-test: meson
	$(MESON) test -C "$(MESON_BUILD_DIR)"

test: meson-test

compile-commands: meson-setup

trace: meson

info: meson
	$(GLUPSK_INFO) $(AA_STORY)

clean:
	rm -rf $(BUILD_DIR) compile_commands.json

clean-aa:
	rm -f $(AA_STORY)

clean-tiny-i7:
	rm -f $(TINY_I7_STORY)
