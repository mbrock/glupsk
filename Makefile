NIX ?= nix
MESON ?= meson
WASM_CXX ?= clang++

AA_SOURCE := refdata/aa/aa.ni
AA_STORY := refdata/aa.ulx
AA_RELEASE := 14
AA_WEB_STORY := www/aa-$(AA_RELEASE).ulx
TINY_I7_SOURCE := refdata/tiny-i7/apple.ni
TINY_I7_STORY := refdata/tiny-i7/apple.ulx
SHUTDOWN_GARDEN_SOURCE := refdata/shutdown-garden/shutdown-garden.ni
SHUTDOWN_GARDEN_STORY := refdata/shutdown-garden/shutdown-garden.ulx
SHUTDOWN_GARDEN_RELEASE := 18
SHUTDOWN_GARDEN_WEB_STORY := www/shutdown-garden-$(SHUTDOWN_GARDEN_RELEASE).ulx
I7_FLAKE := ./tools/inform7-nix

BUILD_DIR := build
MESON_BUILD_DIR := $(BUILD_DIR)/meson
WASM_BUILD_DIR := $(BUILD_DIR)/wasm
MESON_BUILD_FILE := $(MESON_BUILD_DIR)/build.ninja
MESON_COMPILE_COMMANDS := $(MESON_BUILD_DIR)/compile_commands.json

GLUPSK_INFO := $(MESON_BUILD_DIR)/glupsk-info
GLUPSK_PLAY := $(MESON_BUILD_DIR)/glupsk-play
GLUPSK_PROFILE := $(MESON_BUILD_DIR)/glupsk-profile
GLUPSK_TRACE := $(MESON_BUILD_DIR)/glupsk-trace
GLUPSK_WASM := $(WASM_BUILD_DIR)/glupsk.wasm

CORE_SOURCES := \
	core/decode.cpp \
	core/execute.cpp \
	core/glk_events.cpp \
	core/glk_names.cpp \
	core/glk_text.cpp \
	core/machine.cpp \
	core/opcode_meta.cpp \
	core/story.cpp \
	core/vm_accel.cpp \
	core/vm_frames.cpp \
	core/vm_operands.cpp \
	core/vm_search.cpp \
	core/vm_strings.cpp

WASM_SOURCES := \
	hosts/wasm/glupsk_wasm.cpp \
	hosts/wasm/no_exceptions.cpp \
	$(CORE_SOURCES)
WASM_EXPORTS := \
	-Wl,--export-memory \
	-Wl,--export=vm_alloc \
	-Wl,--export=vm_free \
	-Wl,--export=vm_create \
	-Wl,--export=vm_load_story \
	-Wl,--export=vm_step \
	-Wl,--export=vm_run_until_blocked \
	-Wl,--export=vm_resume \
	-Wl,--export=vm_status \
	-Wl,--export=vm_pc \
	-Wl,--export=vm_sp \
	-Wl,--export=vm_last_error \
	-Wl,--export=vm_snapshot_size \
	-Wl,--export=vm_snapshot_write \
	-Wl,--export=vm_snapshot_read \
	-Wl,--export=vm_destroy

.PHONY: all aa clean clean-aa clean-shutdown-garden clean-tiny-i7 compile-commands deno-play info meson meson-setup meson-test play profile shutdown-garden shutdown-garden-check test tiny-i7 trace wasm web-assets

all: meson

aa: $(AA_STORY)

tiny-i7: $(TINY_I7_STORY)

shutdown-garden: $(SHUTDOWN_GARDEN_STORY)

shutdown-garden-check: meson
	$(MAKE) -C refdata/shutdown-garden full-check GLUPSK_PLAY=../../$(GLUPSK_PLAY)

$(AA_STORY): $(AA_SOURCE) tools/inform7-nix/flake.nix tools/inform7-nix/flake.lock
	$(NIX) run $(I7_FLAKE)#i7-build -- --glulx $(AA_SOURCE) $@

$(TINY_I7_STORY): $(TINY_I7_SOURCE) tools/inform7-nix/flake.nix tools/inform7-nix/flake.lock
	$(NIX) run $(I7_FLAKE)#i7-build -- --glulx $(TINY_I7_SOURCE) $@

$(SHUTDOWN_GARDEN_STORY): $(SHUTDOWN_GARDEN_SOURCE) tools/inform7-nix/flake.nix tools/inform7-nix/flake.lock
	$(NIX) run $(I7_FLAKE)#i7-build -- --glulx $(SHUTDOWN_GARDEN_SOURCE) $@

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

$(GLUPSK_WASM): $(WASM_SOURCES) hosts/wasm/allowed-undefined.txt Makefile
	mkdir -p "$(WASM_BUILD_DIR)"
	$(WASM_CXX) --target=wasm32-wasi -std=c++23 -Oz -flto -g0 -I. \
		-DGLUPSK_ENABLE_FILESYSTEM=0 -DGLUPSK_NO_EXCEPTIONS \
		-fno-exceptions -fno-use-cxa-atexit -mexec-model=reactor \
		$(WASM_SOURCES) \
		-Wl,--allow-undefined-file=hosts/wasm/allowed-undefined.txt \
		$(WASM_EXPORTS) \
		-Wl,--gc-sections -Wl,--strip-all \
		-lc++abi -lc++ -lc \
		-o "$@"

wasm: $(GLUPSK_WASM)

deno-play: wasm
	/home/mbrock/.deno/bin/deno run --allow-read tools/glupsk-play-deno.ts $(TINY_I7_STORY)

web-assets: wasm $(AA_STORY) $(SHUTDOWN_GARDEN_STORY)
	cp "$(GLUPSK_WASM)" www/glupsk.wasm
	cp "$(AA_STORY)" "$(AA_WEB_STORY)"
	cp "$(AA_STORY)" www/aa.ulx
	cp "$(SHUTDOWN_GARDEN_STORY)" "$(SHUTDOWN_GARDEN_WEB_STORY)"
	cp "$(SHUTDOWN_GARDEN_STORY)" www/shutdown-garden.ulx
	chmod 644 www/glupsk.wasm "$(AA_WEB_STORY)" www/aa.ulx "$(SHUTDOWN_GARDEN_WEB_STORY)" www/shutdown-garden.ulx
	zstd -q -f -19 www/glupsk.wasm -o www/glupsk.wasm.zst
	zstd -q -f -19 "$(AA_WEB_STORY)" -o "$(AA_WEB_STORY).zst"
	zstd -q -f -19 www/aa.ulx -o www/aa.ulx.zst
	zstd -q -f -19 "$(SHUTDOWN_GARDEN_WEB_STORY)" -o "$(SHUTDOWN_GARDEN_WEB_STORY).zst"
	zstd -q -f -19 www/shutdown-garden.ulx -o www/shutdown-garden.ulx.zst
	gzip -9 -k -f www/glupsk.wasm
	gzip -9 -k -f "$(AA_WEB_STORY)"
	gzip -9 -k -f www/aa.ulx
	gzip -9 -k -f "$(SHUTDOWN_GARDEN_WEB_STORY)"
	gzip -9 -k -f www/shutdown-garden.ulx
	chmod 644 www/glupsk.wasm.zst "$(AA_WEB_STORY).zst" www/aa.ulx.zst "$(SHUTDOWN_GARDEN_WEB_STORY).zst" www/shutdown-garden.ulx.zst www/glupsk.wasm.gz "$(AA_WEB_STORY).gz" www/aa.ulx.gz "$(SHUTDOWN_GARDEN_WEB_STORY).gz" www/shutdown-garden.ulx.gz

meson-test: meson
	$(MESON) test -C "$(MESON_BUILD_DIR)"

test: meson-test

compile-commands: meson-setup

trace: meson

info: meson
	$(GLUPSK_INFO) $(AA_STORY)

play: meson
	$(GLUPSK_PLAY) $(AA_STORY)

profile: meson
	$(GLUPSK_PROFILE) $(AA_STORY)

clean:
	rm -rf $(BUILD_DIR) compile_commands.json

clean-aa:
	rm -f $(AA_STORY)

clean-tiny-i7:
	rm -f $(TINY_I7_STORY)

clean-shutdown-garden:
	rm -f $(SHUTDOWN_GARDEN_STORY)
